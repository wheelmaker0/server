#pragma once
#include "redis_client.h"
#include "thread_util/thread_pool_manager.h"
#include "nlohmann.hpp"
#include "common/json_util.h"
#include "common/util.h"

namespace redis_util{

inline RedisClient *make_redis_client(const nlohmann::json &config){
    brpc::ChannelOptions options;
    options.protocol        = brpc::PROTOCOL_REDIS;
    options.connection_type = "pooled";
    options.connect_timeout_ms = common::get_value_from_json<int>(config, "connect_timeout_ms", 100);
    options.timeout_ms      = common::get_value_from_json<int>(config, "timeout_ms", 30);;
    options.max_retry       = common::get_value_from_json<int>(config, "max_retry", 0);
    std::string auth = common::get_value_from_json<std::string>(config, "namespace", "");
    if(auth != ""){
        options.auth = new brpc::policy::RedisAuthenticator(auth);
    }
    std::vector<std::string> address = common::get_value_from_json<std::vector<std::string>>(config, "address", std::vector<std::string>());
    std::string metrics_name = common::get_value_from_json<std::string>(config, "metrics_name", "");
    size_t thread_num = common::get_value_from_json<size_t>(config, "thread_num", 0);
    std::shared_ptr<folly::FutureExecutor<folly::CPUThreadPoolExecutor>> pool = nullptr;
    if(thread_num != 0){
        pool = thread_util::ThreadPoolManager::get_instance()->create_thread_pool(metrics_name, thread_num/2, thread_num);
    }
    RedisClient *ret = new RedisClient();
    if(!ret->init(address, metrics_name, &options, pool)){
        LOG(ERROR) <<  "redis " << metrics_name << "init failed: " << config.dump();
        delete ret;
        return nullptr;
    }
    LOG(INFO) << "redis " << metrics_name << " init success!";
    LOG(INFO) << "redis " << metrics_name << " address: " << common::container_2_str(address);
    LOG(INFO) << "redis " << metrics_name << " connect_timeout_ms: " << options.connect_timeout_ms;
    LOG(INFO) << "redis " << metrics_name << " timeout_ms: " << options.timeout_ms;
    LOG(INFO) << "redis " << metrics_name << " namespace: " << auth;
    LOG(INFO) << "redis " << metrics_name << " thread_num: " << thread_num;
    return ret;
}

}; //namespace redis_util
