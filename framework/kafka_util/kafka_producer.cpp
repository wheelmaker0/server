#include <glog/logging.h>
#include "kafka_producer.h"

namespace kafka_util{

bool KafkaProducer::init(const KafkaParam &param){
    std::string                    errstr;
    std::unique_ptr<RdKafka::Conf> conf {RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL)};
    std::unique_ptr<RdKafka::Conf> tconf {RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC)};
    topic_ = param.topics;
    queue_.set_capacity(64);

    #define SET_CONF_KV_BY_NAME(conf, key, value)                                                   \
    do {                                                                                            \
        if (conf->set(key, value, errstr) != RdKafka::Conf::CONF_OK) {                              \
        LOG(ERROR) << "kafka topic(" << topic_ << ") conf set " << key << " failed: " << errstr;    \
        return false;                                                                               \
        }                                                                                           \
    } while (0)

    #define SET_CONF_KV(key, value)  SET_CONF_KV_BY_NAME(conf, key, value)
    #define SET_TCONF_KV(key, value) SET_CONF_KV_BY_NAME(tconf, key, value)

    SET_CONF_KV("bootstrap.servers", param.brokers);
    SET_CONF_KV("socket.timeout.ms", "10000");
    SET_CONF_KV("socket.keepalive.enable", "true");
    SET_CONF_KV("socket.nagle.disable", "true");
    SET_CONF_KV("reconnect.backoff.ms", "10");
    SET_CONF_KV("reconnect.backoff.max.ms", "50");
    SET_CONF_KV("queue.buffering.max.messages", "10000");
    SET_CONF_KV("linger.ms", "1000");
    SET_CONF_KV("message.send.max.retries", "0");
    callback_.name = topic_;
    SET_CONF_KV("dr_cb", &callback_);

    if (!param.username.empty() && !param.password.empty()) {
        SET_CONF_KV("security.protocol", "sasl_plaintext");
        SET_CONF_KV("sasl.mechanisms", "PLAIN");
        SET_CONF_KV("sasl.username", param.username);
        SET_CONF_KV("sasl.password", param.password);
    }
    if (!param.compress.empty()) {
        SET_CONF_KV("compression.type", param.compress);
    }
    SET_TCONF_KV("request.required.acks", "1");
    #undef SET_TCONF_KV
    #undef SET_CONF_KV
    #undef SET_CONF_KV_BY_NAME

    auto producer = RdKafka::Producer::create(conf.get(), errstr);
    if (producer == nullptr) {
        LOG(ERROR) << "kafka topic(" << topic_ << ") create producer failed: " << errstr;
        return false;
    }
    producer_.reset(producer);

    auto topic = RdKafka::Topic::create(producer, topic_, tconf.get(), errstr);
    if (topic == nullptr) {
        LOG(ERROR) << "kafka topic(" << topic_ << ") create topic failed: " << errstr;
        return false;
    }
    rd_topic_.reset(topic);
    finished_ = false;
    thread_ = std::thread(&KafkaProducer::loop, this);
    return true;
 }

 void KafkaProducer::destory(){
    finished_ = true;
    queue_.clear();
    queue_.push("");
 }

 void KafkaProducer::join(){
    thread_.join();
 }

bool KafkaProducer::write(const std::string &msg){
    if(finished_){
        return false;
    }
    queue_.push(msg);
    return true;
}

void KafkaProducer::loop(){
    while(!finished_){
        std::string msg;
        queue_.pop(msg);
        if(msg.empty()){
            continue;
        }
        auto err = producer_->produce(rd_topic_.get(), RdKafka::Topic::PARTITION_UA, 
                                    RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(msg.data()),
                                    msg.length(), nullptr, nullptr);
        if(err == RdKafka::ERR__QUEUE_FULL){
            //阻塞等待消息发送，貌似只有加了回调，这里poll(-1)才不会一直阻塞
            producer_->poll(-1);
            err = producer_->produce(rd_topic_.get(), RdKafka::Topic::PARTITION_UA, 
                                    RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(msg.data()),
                                    msg.length(), nullptr, nullptr);
        }
        if(err != RdKafka::ERR_NO_ERROR) {
            common::PrometheusClient::get_instance()->gauge_inc({{"kafka", name_}, {"kafka_qps", "error"}}, 1);
            LOG(ERROR) << "kafka topic " << topic_ << " produce failed: " << RdKafka::err2str(err);
        }
    }
 }

}; //namespace kafka_util