#pragma once

#include <memory>
#include <utility>
#include "kafka_consumer.h"
#include "kafka_producer.h"
#include "nlohmann.hpp"
#include "common/json_util.h"

namespace kafka_util{

template<class T>
bool init_kafka(T *kafka, const nlohmann::json &config){
    KafkaParam kp;
    kp.brokers = common::get_value_from_json<std::string>(config, "brokers", "");
    kp.topics = common::get_value_from_json<std::string>(config, "topics", "");
    kp.group_id = common::get_value_from_json<std::string>(config, "group_id", "");
    kp.username = common::get_value_from_json<std::string>(config, "username", "");
    kp.password = common::get_value_from_json<std::string>(config, "password", "");
    return kafka->init(kp);
}

template<class T>
T *make_kafka(const nlohmann::json &config){
    T *ret = new T();
    if(!init_kafka(std::forward<T*>(ret), config)){
        delete ret;
        return nullptr;
    }
    return ret;
}

}; //namespace kafka_util{