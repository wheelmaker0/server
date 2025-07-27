#pragma once

#include "test.h"
#include "redis_util/redis_client.h"
#include "redis_util/redis_helper.h"
#include "brpc/redis.h"
#include "brpc/channel.h"
#include "brpc/policy/redis_authenticator.h"
#include "brpc/policy/gzip_compress.h"
#include "shared/user_behavior_fetcher.h"

class RedisTest : public Test{
protected:
    std::string mid_file_path;
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("mid_file_path,m", po::value<std::string>(&mid_file_path)->default_value(""), "mid_file_path");
    }
private:
    virtual void run() override;
};

class ReadRedis : public Test{
protected:
    std::string config_path;
    std::string redis_prefix;

    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("config_path,c", po::value<std::string>(&config_path)->default_value(""), "config_path")
        ("redis_prefix,r", po::value<std::string>(&redis_prefix)->default_value(""), "redis_prefix");
    }

    virtual void run() override{
        nlohmann::json config;
        if(!common::load_config(config_path, config)){
            return ;
        }
        auto redis_client = redis_util::make_redis_client(config);
        int n_channel = redis_client->get_channel().size();
        uint64_t id;
        while(std::cin >> id){
            NewRedisBehaviorKey key(id, n_channel, redis_prefix);
            LOG(INFO) << key.str() << ": " << key.hash();
            RedisBehaviorValue value;
            auto ret = redis_client->get(key, value);
            LOG(INFO) << ret << ": " << value.behaviors.size();
        }
    }
};

class ReadUidRedis : public Test{
protected:
    std::string config_path;
    std::string uid_path;
    std::string redis_prefix;
   
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("config_path,c", po::value<std::string>(&config_path)->default_value(""), "config_path")
        ("uid_path,u", po::value<std::string>(&uid_path)->default_value(""), "uid_path")
        ("redis_prefix,r", po::value<std::string>(&redis_prefix)->default_value(""), "redis_prefix");
    }

    virtual void run() override{
        nlohmann::json config;
        if(!common::load_config(config_path, config)){
            return ;
        }
        std::vector<int64_t> uids;
        if(!common::load_container(uid_path, uids)){
            LOG(ERROR) << "load uid failed " << uid_path;
            return ;
        }

        auto redis_client = redis_util::make_redis_client(config);
        int n_channel = redis_client->get_channel().size();
        std::vector<NewRedisBehaviorKey> new_keys;
        new_keys.reserve(uids.size());
        for(auto uid : uids){
            new_keys.emplace_back(uid, n_channel, "ucf_");
        }

        std::vector<RedisBehaviorKey<RedisOptionUserBehavior>> old_keys;
        old_keys.reserve(uids.size());
        for(auto uid : uids){
            old_keys.emplace_back(uid);
        }
        
        std::vector<RedisBehaviorValue> new_values(new_keys.size());
        std::vector<RedisBehaviorValue> old_values(new_keys.size());
        auto r1 = redis_client->mget(new_keys, new_values);
        auto r2 = redis_util::RedisClientSingleton<RedisOptionUserBehavior>::get_instance()->mget(old_keys, old_values);
        int s1 = 0;
        for(auto &v : new_values){
            s1 += v.behaviors.size() != 0;
        }
        int s2 = 0;
        for(auto &v : old_values){
            s2 += v.behaviors.size() != 0;
        }
        LOG(INFO) << uids.size() << ": " << r1 << "-" << r2 << ", " << s1 << "-" << s2;
    }
};

class RedisCopy : public Test{
protected:
    std::string src_config;
    std::string dst_config;
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("src_config,s", po::value<std::string>(&src_config)->default_value(""), "src_config")
        ("dst_config,d", po::value<std::string>(&dst_config)->default_value(""), "dst_config");
    }

private:
    bool copy(redis_util::RedisClient *src_client, redis_util::RedisClient *dst_client, uint64_t limit_pre_ch = 10000000000L, int batch_size = 64){
        const int n_src_ch = src_client->get_channel().size();
        const int n_dst_ch = dst_client->get_channel().size();
        auto channels = src_client->get_channel();
        std::vector<folly::Future<int>> future_results;
        auto pool = thread_util::ThreadPoolManager::get_instance()->get_cpu_thread_pool();
        for(int ch_index = 0; ch_index < channels.size(); ch_index++){
            auto job = [&, ch_index](){
                auto &ch = channels[ch_index];
                int64_t cursor = 0;
                uint64_t cnt = 0;
                int error_cnt = 0;
                while(cnt < limit_pre_ch && cursor >= 0 && error_cnt < 100){
                    brpc::RedisRequest  req;
                    brpc::RedisResponse resp;
                    brpc::Controller    cntl;
                    req.AddCommand("scan %d COUNT %d" , cursor, batch_size);
                    ch->CallMethod(nullptr, &cntl, &req, &resp, nullptr /*done*/);
                    if (cntl.Failed()) {
                        if(cntl.ErrorCode() == brpc::ERPCTIMEDOUT){
                            LOG(ERROR)<< cnt << ": timeout ";
                        }
                        LOG(ERROR) << cnt << ": error " << cntl.ErrorText();
                        error_cnt++;
                        continue;
                    }

                    if(resp.reply_size() == 0 || !resp.reply(0).is_array() || resp.reply(0).size() < 2){
                        break;
                    }

                    if(!resp.reply(0)[0].is_string()  || !resp.reply(0)[1].is_array()){
                        break;
                    }

                    if(!absl::SimpleAtoi(resp.reply(0)[0].c_str(), &cursor)){
                        break;
                    }
                    
                    if(cursor == 0){
                        break;
                    }

                    LOG(INFO) << "ch-" << ch_index << ": cursor: " << cursor << ", " << resp.reply(0)[1].size() << " ,cnt: " << cnt;
                    std::vector<NewRedisBehaviorKey> src_keys;
                    std::vector<NewRedisBehaviorKey> icf_keys;
                    std::vector<NewRedisBehaviorKey> ucf_keys;
                    for(int i = 0; i < resp.reply(0)[1].size(); i++){
                        int64_t uid;
                        if(!absl::SimpleAtoi(resp.reply(0)[1][i].c_str(), &uid)){
                            continue;
                        }
                        src_keys.emplace_back(uid, n_src_ch);
                        icf_keys.emplace_back(uid, n_dst_ch, "icf_");
                        ucf_keys.emplace_back(uid, n_dst_ch, "ucf_");
                        // LOG(INFO) << "ch-" << ch_index << ": " << i << ": " << cnt << ": " << uid << " : " << src_keys.back().hash() << ": " << icf_keys.back().hash() << " : " << ucf_keys.back().hash();
                        // LOG(INFO) << "ch-" << ch_index << ", uid: " << uid;
                        cnt++;
                    }
                    error_cnt = 0;
                    std::vector<std::string> values(src_keys.size());
                    src_client->mget(src_keys, values, src_keys.size());
                    dst_client->batch_set(icf_keys, values, src_keys.size(), 3600*24*7);
                    dst_client->batch_set(ucf_keys, values, src_keys.size(), 3600*24*7);
                }
                return cnt;
            };
            future_results.push_back(pool->addFuture(job));
        }

        int success_cnt = 0;
        for (const auto &ret : folly::collectAll(future_results).get()) {
            if(*ret > 0){
                success_cnt += *ret;
            }
        }
        LOG(INFO) << "success_cnt: " << success_cnt;
        return true;
    }

    virtual void run() override{
        nlohmann::json src_cfg,dst_cfg;
        if(!common::load_config(src_config, src_cfg) || !common::load_config(dst_config, dst_cfg)){
            return ;
        }

        auto src_redis = redis_util::make_redis_client(src_cfg);
        auto dst_redis = redis_util::make_redis_client(dst_cfg);
        if(!src_redis || !dst_redis){
            return ;
        }
        copy(src_redis, dst_redis);
    }
};