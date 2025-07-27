#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <librdkafka/rdkafkacpp.h>
#include "kafka_param.h"

namespace kafka_util{

class KafkaConsumer{
public:
    KafkaConsumer(){}
    virtual ~KafkaConsumer(){}
    virtual bool init(const KafkaParam &param);
    virtual void destory();
    virtual void join();
    virtual bool on_message(const RdKafka::Message &msg) = 0;

protected:
    virtual bool check_message(const RdKafka::Message &msg);
    virtual void loop();
    static bool set_conf(RdKafka::Conf *pConf, const std::string &key, const std::string &value);
    std::string topic_;
    std::unique_ptr<RdKafka::Topic>          rd_topic_;
    std::unique_ptr<RdKafka::Consumer>       rd_consumer_;
    std::unique_ptr<RdKafka::Queue>          rd_queue_;
    std::atomic<bool> finished_;
    std::thread threads_;
    std::size_t thread_num_ = 1;
    std::size_t timeout_ms_ = -1;         //-1代表阻塞
    std::size_t partition_num_ = 0;
};

}; //namespace kafka_util{