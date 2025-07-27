#include "redis_test.h"
#include "common/util.h"
#include "redis_util/redis_client.h"
#include "brpc/redis.h"
#include "brpc/channel.h"
#include "brpc/policy/redis_authenticator.h"
#include "brpc/policy/gzip_compress.h"

struct RedisMidLibsvm {
    static const std::vector<std::string> address_vec() {
        return std::vector<std::string> {
            "10.185.29.211:6666",
            "10.185.29.212:6666",
            "10.185.29.214:6666"};
    }

    static void init_options(brpc::ChannelOptions *options) {
        options->protocol           = brpc::PROTOCOL_REDIS;
        options->connection_type    = "pooled";
        options->connect_timeout_ms = 100;
        options->timeout_ms         = 7;
        options->max_retry          = 0;
        options->auth = new brpc::policy::RedisAuthenticator("mid_libsvm");
    }

    //给一个独立线程池，防止和读取redis的线程池抢资源
    static std::shared_ptr<folly::FutureExecutor<folly::CPUThreadPoolExecutor>> make_thread_pool(){
        return thread_util::ThreadPoolManager::get_instance()->create_thread_pool("RedisMidLibsvm", 32, 32);
    }
};


void raw_test(){
        brpc::ChannelOptions options;
        options.protocol           = brpc::PROTOCOL_REDIS;
        options.connection_type    = "pooled";
        options.connect_timeout_ms = 100;
        options.timeout_ms         = 30;
        options.max_retry          = 0;
        options.auth = new brpc::policy::RedisAuthenticator("mid_libsvm");
        std::shared_ptr<redis_util::RedisChannel> channel = std::make_shared<redis_util::RedisChannel>();
        if(channel->Init("redis://10.185.29.209:6666", "rr", &options) != 0){
            fprintf(stderr, "init failed\n");
            return ;
        }

        brpc::RedisRequest  req;
        brpc::RedisResponse resp;
        brpc::Controller    cntl;
    
        std::vector<butil::StringPiece> component_vec;
        component_vec.emplace_back("SETEX");
        component_vec.emplace_back("0");
        component_vec.emplace_back("300");
        component_vec.emplace_back("test");

        for(auto &c : component_vec){
            LOG(INFO) << c;
        }

        if (!req.AddCommandByComponents(component_vec.data(), component_vec.size())) {
            fprintf(stderr, "AddCommandByComponents failed\n");
            return ;
        }

        channel->CallMethod(nullptr, &cntl, &req, &resp, nullptr /*done*/);
        if (cntl.Failed()) {
            fprintf(stderr, "CallMethod failed\n");
            if(cntl.ErrorCode() == brpc::ERPCTIMEDOUT){
                return ;
            }
            return ;
        }

        if (resp.reply_size() != 1) {
            fprintf(stderr, "resp.reply_size() != 1\n");
            return ;
        }

        auto &reply = resp.reply(0);
        if (reply.is_error()) {
            std::cerr << "reply has error: " << " " << reply.error_message() << std::endl;
        }
        return ;
}

std::atomic<int> null_cnt(0);
std::atomic<int> error_cnt(0);
std::atomic<int> other_cnt(0);
std::atomic<int> total_cnt(0);
std::atomic<int> str_cnt(0);

class LibsvmValue : public redis_util::ComplexValue{
public:
    bool parser(const brpc::RedisReply &reply){
        total_cnt++;

        if(reply.is_nil()){
            null_cnt++;
            return false;
        }

        if(reply.is_error()){ 
            error_cnt++;
            return false;
        }

        if(!reply.is_string()){
            other_cnt++;
            LOG(INFO) << "reply type: " << reply.type() << "\n";
            return false;
        }

        LOG(INFO) << "reply.size() " << str_cnt++ << ": " << reply.data().size() << std::endl;
        return true;
    }
    std::string data;
};
   

void RedisTest::run(){
    // raw_test();

    std::ifstream ifs(mid_file_path);
    if(!ifs){  
        std::cerr << "open mid file failed:" << mid_file_path << std::endl;
        return ;
    }

    std::vector<LibsvmValue> v(1);
    redis_util::RedisClientSingleton<RedisMidLibsvm>::get_instance()->mget(std::vector<uint64_t>(1, 5134808997173084), v);
    LOG(INFO) << "index: " << 5134808997173084  % 3 << std::endl;
    null_cnt = 0;
    error_cnt = 0;
    other_cnt = 0;
    total_cnt = 0;
    str_cnt = 0;

    std::vector<uint64_t> keys;
    std::string line;
    while(getline(ifs, line)){
        keys.push_back(std::stoull(line));
        // std::cout << line << " " << keys.back() << std::endl;
    }

    std::vector<LibsvmValue> values(keys.size());
    int ret = redis_util::RedisClientSingleton<RedisMidLibsvm>::get_instance()->mget(keys, values);
    LOG(INFO) << "ret: " << ret;
    LOG(INFO) << "null_cnt: " << null_cnt << " error_cnt: " << error_cnt << " other_cnt: " << other_cnt << " str_cnt: " << str_cnt  << " total_cnt: " << total_cnt;
    return ;
}