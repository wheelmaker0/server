#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <tbb/concurrent_queue.h>
#include <librdkafka/rdkafkacpp.h>
#include <boost/core/demangle.hpp>
#include "common/prometheus_client.h"
#include "kafka_param.h"

namespace kafka_util{

struct DeliveryReportCb : public RdKafka::DeliveryReportCb {
    std::string name;
    void dr_cb(RdKafka::Message &msg) override {
        if (msg.status() != RdKafka::Message::MSG_STATUS_PERSISTED) {
            common::PrometheusClient::get_instance()->gauge_inc({{"kafka", name}, {"kafka_qps", "fail"}}, 1);
        }
        else{
            common::PrometheusClient::get_instance()->gauge_inc({{"kafka", name}, {"kafka_qps", "success"}}, 1);
        }
    }
};

class KafkaProducer{
public:
    KafkaProducer():finished_(true){}
    virtual ~KafkaProducer(){}
    // virtual std::string name(){
    //     static std::string n = boost::core::demangle(typeid(*this).name());
    //     return n;
    // }
    virtual bool init(const KafkaParam &param);
    virtual void destory();
    virtual void join();
    virtual bool write(const std::string &msg);

protected:
    virtual void loop();
    std::string topic_;
    std::atomic<bool> finished_;
    std::unique_ptr<RdKafka::Topic>    rd_topic_;
    std::unique_ptr<RdKafka::Producer> producer_;
    tbb::concurrent_bounded_queue<std::string> queue_;
    std::thread thread_;
    DeliveryReportCb callback_;
    std::string name_;
};

}; //kafka_util