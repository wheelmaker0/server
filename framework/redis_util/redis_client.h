#pragma once
#include <vector>
#include <string>
#include <memory>
#include <iterator>
#include <boost/format.hpp>
#include <absl/meta/type_traits.h>
#include <folly/ScopeGuard.h>
#include <folly/Traits.h>
#include <absl/strings/match.h>
#include "brpc/redis.h"
#include "brpc/channel.h"
#include "brpc/policy/redis_authenticator.h"
#include "brpc/policy/gzip_compress.h"
#include "thread_util/thread_pool_manager.h"
#include "common/profiler_prometheus.h"
#include "common/prometheus_client.h"
#include <boost/core/demangle.hpp>

namespace redis_util{

//FOLLY_CREATE_HAS_MEMBER_FN_TRAITS(has_init_option_impl, init_option);

template <typename T>
using has_init_options_impl = std::enable_if_t<std::is_same_v<void, decltype(T::init_options(static_cast<brpc::ChannelOptions *>(nullptr)))>>;

template <typename T>
struct has_init_options : absl::type_traits_internal::is_detected<has_init_options_impl, T> { };

template <typename T>
using has_make_thread_pool_impl = std::enable_if_t<std::is_same_v<std::shared_ptr<folly::FutureExecutor<folly::CPUThreadPoolExecutor>>, decltype(T::make_thread_pool())>>;

template <typename T>
struct has_make_thread_pool : absl::type_traits_internal::is_detected<has_make_thread_pool_impl, T> { };

// template <typename T>
// using has_hash_impl = std::enable_if_t<std::is_same_v<void, decltype(T::hash(const std::string &)>>;

// template <typename T>
// struct has_hash : absl::type_traits_internal::is_detected<has_hash_impl, T> { };


class RedisChannel : public brpc::Channel {
  using Base = brpc::Channel;

 public:
  using Base::Base;

  const std::string &address() const { 
    static std::string address = std::string(butil::ip2str(_server_address.ip).c_str()) + ":" + std::to_string(_server_address.port);
    return address; 
  }

  void CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *cntl, const google::protobuf::Message *request,
                  google::protobuf::Message *response, google::protobuf::Closure *done) override {
    Base::CallMethod(method, cntl, request, response, done);
  }
};

enum ErrorCode{
    REDIS_NULL = 0,
    REDIS_TIMEOUT = -1,
    REDIS_ERROR = -2
};

class ComplexKey{
public:
    virtual ~ComplexKey() {}
    virtual uint64_t hash() const = 0;
    virtual std::string str() const = 0;
};

class ComplexValue{
public:
    virtual ~ComplexValue() {}
    virtual bool parser(const brpc::RedisReply &reply) = 0;
};

class RedisClient{
public:

    RedisClient() {}
    template <typename AddressTraits>
    bool init() {
        auto address_vec = AddressTraits::address_vec();
        if(address_vec.size() == 0){
            LOG(ERROR) << "no address";
            return false;
        }
        brpc::ChannelOptions options;
        options.protocol = brpc::PROTOCOL_REDIS;
        if constexpr (has_init_options<AddressTraits>::value) {
            AddressTraits::init_options(&options);
        } else {
            options.connection_type    = "pooled";
            options.connect_timeout_ms = 100;
            options.timeout_ms         = 30;
        }
        if constexpr (has_make_thread_pool<AddressTraits>::value){
            pool_ = AddressTraits::make_thread_pool();
        }
        else{
            auto t = thread_util::ThreadPoolManager::get_instance()->get_redis_thread_pool();
            pool_ = std::shared_ptr<folly::FutureExecutor<folly::CPUThreadPoolExecutor>>(t, [](const folly::FutureExecutor<folly::CPUThreadPoolExecutor> *){
            });
        }
        ch_vec_.resize(address_vec.size());
        name_ = boost::core::demangle(typeid(AddressTraits).name());
        for (size_t i = 0; i < address_vec.size(); i++) {
            std::string address = address_vec[i];
            if (!absl::StartsWith(address, "redis://")) {
                address = "redis://" + address_vec[i];
            }
            ch_vec_[i] = std::make_shared<RedisChannel>();
            if (ch_vec_[i]->Init(address.data(), "rr", &options) != 0) {
                LOG(FATAL) << "fail to init channel to redis: " << address;
                return false;
            }
        }
        return true;
    }

    bool init(const std::vector<std::string> &address_vec, const std::string &name, brpc::ChannelOptions *options = nullptr, std::shared_ptr<folly::FutureExecutor<folly::CPUThreadPoolExecutor>> pool = nullptr) {
        if(address_vec.size() == 0){
            LOG(ERROR) << "no address";
            return false;
        }
        brpc::ChannelOptions local_options;
        local_options.protocol = brpc::PROTOCOL_REDIS;
        local_options.connection_type    = "pooled";
        local_options.connect_timeout_ms = 100;
        local_options.timeout_ms         = 30;
        name_ = name;
        if(options == nullptr){
            options = &local_options;
        }
        if(pool != nullptr){
            pool_ = pool;
        }
        else{
            auto t = thread_util::ThreadPoolManager::get_instance()->get_redis_thread_pool();
            pool_ = std::shared_ptr<folly::FutureExecutor<folly::CPUThreadPoolExecutor>>(t, [](const folly::FutureExecutor<folly::CPUThreadPoolExecutor> *){});
        }
        ch_vec_.resize(address_vec.size());
        for (size_t i = 0; i < address_vec.size(); i++) {
            std::string address = address_vec[i];
            if (!absl::StartsWith(address, "redis://")) {
                address = "redis://" + address_vec[i];
            }
            ch_vec_[i] = std::make_shared<RedisChannel>();
            if (ch_vec_[i]->Init(address.data(), "rr", options) != 0) {
                LOG(FATAL) << "fail to init channel to redis: " << address;
                return false;
            }
        }
        return true;
    }

    std::vector<std::shared_ptr<RedisChannel>> get_channel(){
        return ch_vec_;
    }

    template<typename KEY_TYPE, typename VALUE_TYPE>
    int batch_query(const char *query_format, const std::vector<KEY_TYPE> &keys, std::vector<VALUE_TYPE> &values, size_t batch_size = 64){
        values.resize(keys.size());
        return split_query_by_channel(query_format, keys, values, batch_size);
    }

    template<typename KEY_TYPE, typename VALUE_TYPE>
    int query(const char *query_format, const KEY_TYPE &key, VALUE_TYPE &value, size_t batch_size = 64){
        std::vector<KEY_TYPE> keys = {key};
        std::vector<VALUE_TYPE> values(1);
        auto ret = split_query_by_channel(query_format, keys, values, batch_size);
        value = std::move(values[0]);
        return ret;
    }

    template<typename KEY_TYPE, typename VALUE_TYPE>
    int get(const KEY_TYPE &key, VALUE_TYPE &value){
        return query("GET %s", key, value, 1);
    }

    template<typename KEY_TYPE, typename VALUE_TYPE>
    int mget(const std::vector<KEY_TYPE> &keys, std::vector<VALUE_TYPE> &results, size_t batch_size = 64){
        return split_query_by_channel("MGET", keys, results, batch_size);
    }

    template<typename VALUE_TYPE>
    int keys(std::vector<VALUE_TYPE> &output_keys){
        std::vector<std::string> keys;
        return split_query_by_channel("KEYS *", keys, output_keys, 1);
    }

    // SET SETEX
    template <typename KEY_TYPE, typename VALUE_TYPE>
    int batch_set(const std::vector<KEY_TYPE> &keys, std::vector<VALUE_TYPE> &values, size_t batch_size = 32, int ttl = 86400, const std::string &mode = "") {
        if (keys.size() != values.size()) return 0;
        return split_set_by_channel("SET", keys, values, batch_size, ttl, mode);
    }

    template<typename KEY_TYPE, typename VALUE_TYPE>
    int set(const KEY_TYPE &key, const VALUE_TYPE &value){
        return single_set(key, value);
    }

protected:
    std::vector<std::shared_ptr<RedisChannel>> ch_vec_;

    int64_t hash(const int64_t &key){
        return key;
    }
    
    uint64_t hash(const uint64_t &key){
        return key;
    }

    uint64_t hash(const std::string &key){
        return std::hash<std::string>()(key);
    }

    uint64_t hash(const ComplexKey &key){
        return key.hash();
    }

    std::string to_str(const uint64_t &key){
        return std::to_string(key);
    }

    std::string to_str(const std::string &key){
        return key;
    }

    std::string to_str(const ComplexKey &key){
        return key.str();
    }

    int parser(const brpc::RedisReply &reply, uint64_t &value){
        if(!reply.is_integer()){
            return 0;
        }
        value = reply.integer();
        return 1;
    }

    int parser(const brpc::RedisReply &reply, std::string &value){
        if(!reply.is_string()){
            return 0;
        }
        value = reply.data().as_string();
        return 1;
    }

    int parser(const brpc::RedisReply &reply, std::vector<std::string> &value){
        if(!reply.is_array()){
            return 0;
        }
        value.resize(reply.size());
        int success_cnt = 0;
        for(size_t i = 0; i < value.size(); i++){
            success_cnt += parser(reply[i], value[i]);
        }
        return success_cnt;
    }

    int parser(const brpc::RedisReply &reply, std::vector<uint64_t> &value){
        if(!reply.is_array()){
            return 0;
        }
        value.resize(reply.size());
        int success_cnt = 0;
        for(size_t i = 0; i < value.size(); i++){
            success_cnt += parser(reply[i], value[i]);
        }
        return success_cnt;
    }

    int parser(const brpc::RedisReply &reply, ComplexValue &value){
        return value.parser(reply);
    }

    //预先知道大小，reply的size必须等于已知值
    template<typename VALUE_TYPE>
    int parser(const brpc::RedisReply &reply, VALUE_TYPE *value, size_t num){
        if(!reply.is_array()){
            return 0;
        }

        if(reply.size() != num){
            return 0;
        }

        int success_cnt = 0;
        for(size_t i = 0; i < num; i++){
            success_cnt += (parser(reply[i], value[i]) > 0);
        }
        return success_cnt;
    }

    //预先不知道，result大小的情况，根据reply大小分配空间
    template<typename VALUE_TYPE>
    int parser(const brpc::RedisReply &reply, std::vector<VALUE_TYPE> &value){
        value.clear();
        if(!reply.is_array()){
            return 0;
        }

        value.resize(reply.size());
        int success_cnt = 0;
        for(size_t i = 0; i < value.size(); i++){
            success_cnt += parser(reply[i], value[i]);
        }
        return success_cnt;
    }

    template<typename KEY_TYPE, typename VALUE_TYPE>
    int split_query_by_channel(const std::string &query_format, const std::vector<KEY_TYPE> &keys, std::vector<VALUE_TYPE> &results, size_t batch_size){

        auto ch_size = ch_vec_.size();
        std::vector<std::vector<std::string>> key_by_ch(ch_size);
        std::vector<std::vector<VALUE_TYPE>> value_by_ch(ch_size);
        std::vector<std::vector<size_t>> index_by_ch(ch_size);
        
        size_t num = results.size();
        if(!keys.empty()){
            for(size_t i = 0; i < num; i++){
                auto &key = keys[i];
                auto ch_index = hash(key) % ch_size;
                key_by_ch[ch_index].push_back(to_str(key));
                index_by_ch[ch_index].push_back(i);
            }
        }

        std::vector<folly::Future<int>> future_results;

        if(keys.empty()){
            future_results.reserve(ch_size);
            for (size_t i = 0; i < ch_size; i++) {
                future_results.push_back(pool_->addFuture(
                    [&, i] () {
                        return query_without_keys(query_format, ch_vec_[i], value_by_ch[i]);
                    }));
            }
        }
        else{
            future_results.reserve(num/batch_size);
            for (size_t i = 0; i < ch_size; i++) {
                value_by_ch[i].resize(key_by_ch[i].size());
                auto f = split_query_by_batch(query_format, ch_vec_[i], key_by_ch[i].data(), value_by_ch[i].data(), key_by_ch[i].size(), batch_size);
                std::move(std::begin(f), std::end(f), std::back_inserter(future_results));
            }
        }

        int success_cnt = 0;
        for (const auto &ret : folly::collectAll(future_results).get()) {
            if(*ret > 0){
                success_cnt += *ret;
            }
        }

        if(!keys.empty()){
            for(size_t i = 0; i < ch_size; i++){
                auto &values = value_by_ch[i];
                auto &index = index_by_ch[i];
                assert(index.size() == values.size());
                for(size_t j = 0; j < index.size(); j++){
                    results[index[j]] = std::move(values[j]);
                }
            }
        }
        else{
            for(auto & v : value_by_ch){
                results.insert(results.end(), v.begin(), v.end());
            }
        }
        return success_cnt;
    }

    template<typename VALUE_TYPE>
    std::vector<folly::Future<int>> split_query_by_batch(const std::string &query_format, const std::shared_ptr<RedisChannel> &channel, const std::string *keys, VALUE_TYPE *results, size_t num, size_t batch_size){
        std::vector<folly::Future<int>> future_results;

        if(num == 0){
            return future_results;
        }

        size_t batch_num = (num + batch_size - 1) / batch_size;
        future_results.reserve(batch_num);

        for (size_t i = 0; i < batch_num; i++) {
            size_t begin_index = i * batch_size;
            size_t size = std::min(batch_size, (num - begin_index));
            future_results.push_back(pool_->addFuture(
                [channel, query_format, src = keys + begin_index, dst = results + begin_index, size, this] () {
                    if(query_format == "MGET"){
                        return mget_by_batch(channel, src, dst, size);
                    }
                    return query_by_batch(query_format, channel, src, dst, size);
                }));
        }

        return future_results;
    }

    template<typename VALUE_TYPE>
    int query_without_keys(const std::string &query_format, const std::shared_ptr<RedisChannel> &channel, VALUE_TYPE &values){
        common::ProfilerUs profiler;
        brpc::RedisRequest  req;
        brpc::RedisResponse resp;
        brpc::Controller    cntl;
        const std::string   &address     = channel->address();
        req.AddCommand(query_format.c_str());
        channel->CallMethod(nullptr, &cntl, &req, &resp, nullptr /*done*/);
        profiler.lap({{"redis", name_}, {"redis_tc", "query_nk"}});
        if (cntl.Failed()) {
            if(cntl.ErrorCode() == brpc::ERPCTIMEDOUT){
                //LOG(ERROR) << "REDIS_TIMEOUT";
                common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "timeout"}, {"addr", address}}, 1);
                return REDIS_TIMEOUT;
            }
            common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "fail"}, {"addr", address}}, 1);
            LOG(ERROR) << "address " << address <<  ", error " << cntl.ErrorText() << ", query: " << query_format;
            return REDIS_ERROR;
        }

        if (resp.reply_size() != 1) {
            common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "fail"}, {"addr", address}}, 1);
            LOG(ERROR) << "reply_size except: " << 1 << " ,got: " << resp.reply_size();
            return REDIS_ERROR;
        }
        int ret = parser(resp.reply(0), values);
        profiler.lap({{"redis", name_}, {"redis_tc", "query_nk_parser"}});
        if(ret > 0){
            common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "success"}, {"addr", address}}, 1);
        }
        else{
            common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "null"}, {"addr", address}}, 1);
        }
        return ret;
    }

    //ret < 0出错 , >= 成功个数
    template<typename VALUE_TYPE>
    int query_by_batch(const std::string &query_format, const std::shared_ptr<RedisChannel> &channel, const std::string *keys, VALUE_TYPE *results, size_t size) {
        common::ProfilerUs profiler;
        brpc::RedisRequest  req;
        brpc::RedisResponse resp;
        brpc::Controller    cntl;
        const std::string   &address     = channel->address();
        for (size_t i = 0; i < size; i++) {
            req.AddCommand(query_format.c_str() , to_str(keys[i]).c_str());
        }

        channel->CallMethod(nullptr, &cntl, &req, &resp, nullptr /*done*/);
        profiler.lap({{"redis", name_}, {"redis_tc", "query"}});
        if (cntl.Failed()) {
            if(cntl.ErrorCode() == brpc::ERPCTIMEDOUT){
                // LOG(ERROR) << "REDIS_TIMEOUT";
                common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "timeout"}, {"addr", address}}, size);
                return REDIS_TIMEOUT;
            }
            common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "fail"}, {"addr", address}}, size);
            std::string query = (boost::format(query_format) % to_str(keys[0])).str();
            LOG(ERROR) << "address " << address <<  ", error " << cntl.ErrorText() << ", query: " << query;
            return REDIS_ERROR;
        }

        if (resp.reply_size() != size) {
            common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "fail"}, {"addr", address}}, size);
            LOG(ERROR) << "reply_size except: " << size << " ,got: " << resp.reply_size();
            return REDIS_ERROR;
        }

        int success_cnt = 0;
        for(size_t i = 0; i < size; i++){
            auto &reply = resp.reply(i);
            if (reply.is_error()) {
                std::string query = (boost::format(query_format) % to_str(keys[0])).str();
                LOG(ERROR) << "reply has error continue: " << i << " " << reply.error_message() << ", address: " << address << ", query: " << query;
                continue;
            }
            //成功个数应该是跟key对应的，如果一个key下是个容器，不应该记容器内部个数
            success_cnt += (parser(reply, results[i]) > 0);
        }
        profiler.lap({{"redis", name_}, {"redis_tc", "query_parser"}});
        common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "success"}, {"addr", address}}, success_cnt);
        common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "null"}, {"addr", address}}, size - success_cnt);
        return success_cnt;
    }

    //ret < 0出错 , >= 成功个数
    template<typename VALUE_TYPE>
    int mget_by_batch(const std::shared_ptr<RedisChannel> &channel, const std::string *keys, VALUE_TYPE *results, size_t size) {
        common::ProfilerUs profiler;
        brpc::RedisRequest  req;
        brpc::RedisResponse resp;
        brpc::Controller    cntl;

        const std::string              &address     = channel->address();
        std::vector<butil::StringPiece> component_vec;
        component_vec.resize(size + 1);
        component_vec[0] = "MGET";
        for (size_t i = 0; i < size; i++) {
            component_vec[i + 1] = keys[i];
        }

        // LOG(INFO) << "component_vec" << dump(component_vec);
        if (!req.AddCommandByComponents(component_vec.data(), component_vec.size())) {
            return REDIS_ERROR;
        }

        channel->CallMethod(nullptr, &cntl, &req, &resp, nullptr /*done*/);
        profiler.lap({{"redis", name_}, {"redis_tc", "mget"}});
        if (cntl.Failed()) {
            if(cntl.ErrorCode() == brpc::ERPCTIMEDOUT){
                // LOG(ERROR) << "REDIS_TIMEOUT";
                common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "timeout"}, {"addr", address}}, size);
                return REDIS_TIMEOUT;
            }
            common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "fail"}, {"addr", address}}, size);
            LOG(ERROR) << "address " << address <<  ", error " << cntl.ErrorText();
            return REDIS_ERROR;
        }

        if (resp.reply_size() != 1) {
            common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "fail"}, {"addr", address}}, size);
            LOG(ERROR) << "reply_size except: " << 1 << " ,got: " << resp.reply_size();
            return REDIS_ERROR;
        }
        int success_cnt = parser(resp.reply(0), results, size);
        profiler.lap({{"redis", name_}, {"redis_tc", "mget_parser"}});
        common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "success"}, {"addr", address}}, success_cnt);
        common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "null"}, {"addr", address}}, size - success_cnt);
        return success_cnt;
    }

        template<typename KEY_TYPE, typename VALUE_TYPE>
    int split_set_by_channel(const std::string &query_format, const std::vector<KEY_TYPE> &keys, std::vector<VALUE_TYPE> &values, size_t batch_size, int ttl, const std::string &mode){
        auto                                  ch_size = ch_vec_.size();
        std::vector<std::vector<std::string>> key_by_ch(ch_size);
        std::vector<std::vector<std::string>> value_by_ch(ch_size);

        size_t num = keys.size();
        for (size_t i = 0; i < num; i++) {
            auto &key      = keys[i];
            auto &value    = values[i];
            auto  ch_index = hash(key) % ch_size;
            key_by_ch[ch_index].push_back(to_str(key));
            value_by_ch[ch_index].push_back(to_str(value));
        }

        std::vector<folly::Future<int>> future_results;
        future_results.reserve(num / batch_size);
        for (size_t i = 0; i < ch_size; i++) {
            auto f = split_set_by_batch(query_format, ch_vec_[i], key_by_ch[i].data(),
                                        value_by_ch[i].data(), key_by_ch[i].size(), batch_size, ttl, mode);
            std::move(std::begin(f), std::end(f), std::back_inserter(future_results));
        }

        int success_cnt = 0;
        for (const auto &ret : folly::collectAll(future_results).get()) {
            if(*ret > 0){
                success_cnt += *ret;
            }
        }

        return success_cnt;
    }

    std::vector<folly::Future<int>>
        split_set_by_batch(const std::string                   &query_format,
                           const std::shared_ptr<RedisChannel> &channel, const std::string *keys,
                           const std::string *values, size_t num, size_t batch_size, int ttl, const std::string &mode) {
        std::vector<folly::Future<int>> future_results;

        if (num == 0) {
            return future_results;
        }

        size_t batch_num = (num + batch_size - 1) / batch_size;
        future_results.reserve(batch_num);

        for (size_t i = 0; i < batch_num; i++) {
            size_t begin_index = i * batch_size;
            size_t size        = std::min(batch_size, (num - begin_index));
            future_results.push_back(pool_->addFuture(
                    [channel, query_format, k = keys + begin_index, v = values + begin_index, size, ttl, mode,
                     this]() { return set_by_batch(query_format, channel, k, v, size, ttl, mode); }));
        }

        return future_results;
    }

    int set_by_batch(const std::string &query_format, const std::shared_ptr<RedisChannel> &channel,
                     const std::string *keys, const std::string *values, size_t size, int ttl, const std::string &mode) {
        brpc::RedisRequest  req;
        brpc::RedisResponse resp;
        brpc::Controller    cntl;
        const std::string  &address = channel->address();
        std::string str_ttl = std::to_string(ttl);
        for (size_t i = 0; i < size; i++) {
            std::vector<butil::StringPiece> component_vec;
            component_vec.emplace_back(query_format);
            component_vec.emplace_back(keys[i]);
            component_vec.emplace_back(values[i]);
            if(ttl != 0){
                component_vec.emplace_back("EX");
                component_vec.emplace_back(str_ttl);
            }
            
            if(mode != ""){
                component_vec.emplace_back(mode);
            }

            if (!req.AddCommandByComponents(component_vec.data(), component_vec.size())) {
                common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "fail"}, {"addr", address}}, size);
                return REDIS_ERROR;
            }
        }

        channel->CallMethod(nullptr, &cntl, &req, &resp, nullptr /*done*/);
        if (cntl.Failed()) {
            if(cntl.ErrorCode() == brpc::ERPCTIMEDOUT){
                common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "timeout"}, {"addr", address}}, size);
                return REDIS_TIMEOUT;
            }
            std::string query = query_format + to_str(keys[0]);
            LOG(ERROR) << "address " << address <<  ", error " << cntl.ErrorText() << ", query: " << query;
            common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "fail"}, {"addr", address}}, size);
            return REDIS_ERROR;
        }

        if (resp.reply_size() != size) {
            common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "fail"}, {"addr", address}}, size);
            LOG(ERROR) << query_format << " reply_size except: " << size << " ,got: " << resp.reply_size();
            return REDIS_ERROR;
        }

        int success_cnt = 0;
        for(size_t i = 0; i < size; i++){
            auto &reply = resp.reply(i);
            if (reply.is_error()) {
                std::string query = query_format + to_str(keys[0]);
                LOG(ERROR) << "reply has error continue: " << i << " " << reply.error_message() << ", address: " << address << ", query: " << query;
                continue;
            }
            ++success_cnt;
        }
        common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "fail"}, {"addr", address}}, size - success_cnt);
        common::PrometheusClient::get_instance()->gauge_inc({{"redis", name_}, {"redis_qps", "success"}, {"addr", address}}, success_cnt);
        return success_cnt;
    }

    template<typename KEY_TYPE, typename VALUE_TYPE>
    int single_set(const KEY_TYPE &key, const VALUE_TYPE &value) {
        brpc::RedisRequest  req;
        brpc::RedisResponse resp;
        brpc::Controller    cntl;
        int channel_index = hash(key) % ch_vec_.size();
        const std::string &address = ch_vec_[channel_index]->address();
        req.AddCommand("set %s %b", to_str(key).data(), to_str(value).data(), to_str(value).size());
        ch_vec_[channel_index]->CallMethod(nullptr, &cntl, &req, &resp, nullptr /*done*/);
        if (cntl.Failed()) {
            if(cntl.ErrorCode() == 1008){
                return REDIS_TIMEOUT;
            }
            LOG(ERROR) << "single_set failed " << cntl.ErrorText();
            return REDIS_ERROR;
        }

        if (resp.reply_size() != 1) {
            LOG(ERROR) << "single_set reply_size except: " << 1 << " ,got: " << resp.reply_size();
            return REDIS_ERROR;
        }
        return 1;
    }

    std::string dump(const std::vector<butil::StringPiece> &component_vec){
        std::stringstream ss;
        for(auto &c : component_vec){
            ss << c << " ";
        }
        return ss.str();
    }

private:
    std::shared_ptr<folly::FutureExecutor<folly::CPUThreadPoolExecutor>> pool_;
    std::string name_; //地址太长，太多，比较太抽象，起个名字便于监控观察
};

template <typename AddressTraits>
class RedisClientSingleton{
public:
    static RedisClient *get_instance(){
        static RedisClient *instance = create();
        return instance;
    }

    RedisClientSingleton(const RedisClientSingleton<AddressTraits> &) = delete;
    RedisClientSingleton<AddressTraits> &operator=(const RedisClientSingleton<AddressTraits> &) = delete;
private:
    static RedisClient *create(){
        RedisClient *rc = new RedisClient();
        rc->init<AddressTraits>();
        return rc;
    }
};

}; //redis_util