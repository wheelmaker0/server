#include <glog/logging.h>
#include "kafka_consumer.h"

namespace kafka_util{

 bool KafkaConsumer::init(const KafkaParam &param){
    std::string                    errstr;
    std::unique_ptr<RdKafka::Conf> conf {RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL)};
    std::unique_ptr<RdKafka::Conf> tconf {RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC)};
    topic_ = param.topics;

    #define SET_CONF_KV_BY_NAME(conf, key, value)                                                    \
    do {                                                                                           \
        if ((conf)->set(key, value, errstr) != RdKafka::Conf::CONF_OK) {                             \
        LOG(ERROR) << "kafka topic(" << topic_ << ") conf set " << (key) << " failed: " << errstr; \
        return false;                                                                              \
        }                                                                                            \
    } while (0)

    #define SET_CONF_KV(key, value)  SET_CONF_KV_BY_NAME(conf, key, value)
    #define SET_TCONF_KV(key, value) SET_CONF_KV_BY_NAME(tconf, key, value)

    SET_CONF_KV("bootstrap.servers", param.brokers);
    SET_CONF_KV("socket.keepalive.enable", "true");
    SET_CONF_KV("socket.nagle.disable", "true");
    SET_CONF_KV("reconnect.backoff.ms", "10");
    SET_CONF_KV("reconnect.backoff.max.ms", "50");
    SET_CONF_KV("max.partition.fetch.bytes", "10240000");
    if (!param.group_id.empty()) {
        SET_CONF_KV("group.id", param.group_id);
    }
    if (!param.username.empty() && !param.password.empty()) {
        SET_CONF_KV("security.protocol", "sasl_plaintext");
        SET_CONF_KV("sasl.mechanisms", "PLAIN");
        SET_CONF_KV("sasl.username", param.username);
        SET_CONF_KV("sasl.password", param.password);
    }
    SET_TCONF_KV("auto.offset.reset", "smallest");

    #undef SET_TCONF_KV
    #undef SET_CONF_KV
    #undef SET_CONF_KV_BY_NAME

    auto consumer = RdKafka::Consumer::create(conf.get(), errstr);
    if (consumer == nullptr) {
        LOG(ERROR) << "kafka topic(" << topic_ << ") create consumer failed: " << errstr;
        return false;
    }
    auto consume_queue = RdKafka::Queue::create(consumer);
    if (consume_queue == nullptr) {
        LOG(ERROR) << "kafka topic(" << topic_ << ") create queue failed: " << errstr;
        return false;
    }
    auto topic = RdKafka::Topic::create(consumer, topic_, tconf.get(), errstr);
    if (topic == nullptr) {
        LOG(ERROR) << "kafka topic(" << topic_ << ") create topic failed: " << errstr;
        return false;
    }
    rd_topic_.reset(topic);
    rd_consumer_.reset(consumer);
    rd_queue_.reset(consume_queue);

    RdKafka::Metadata *metadata;
    auto r = rd_consumer_->metadata(false, topic, &metadata, 1000);
    if( r != RdKafka::ERR_NO_ERROR ){
        LOG(ERROR) << "kafka topic(" << topic_ << ") get meta failed: " << RdKafka::err2str(r);
        return false;
    }

    auto topics = metadata->topics();
    if(topics == nullptr){
        LOG(ERROR) << "get topics meta null: " << topic_;
        return false;
    }

    if(topics->size() != 1){
        LOG(ERROR) << "get topics meta size not 1: " << topics->size();
        return false;
    }

    auto &topic_meta = (*topics)[0];
    if(topic_meta == nullptr){
        LOG(ERROR) << "get topic 0 null: " << topic_;
        return false;
    }

    auto partitions = topic_meta->partitions();

    if(partitions == nullptr){
        LOG(ERROR) << "get partitions meta null: " << topic_;
        return false;
    }

    LOG(INFO) << "topic: " << topic_meta->topic() << " ,partitions: " << partitions->size();  
    partition_num_ = partitions->size();

    if(param.partitions_offset.size() == 0){
        for(size_t i = 0; i < partitions->size(); i++){
            RdKafka::ErrorCode resp = rd_consumer_->start(rd_topic_.get(), i, RdKafka::Topic::OFFSET_END, rd_queue_.get());
            if (resp != RdKafka::ERR_NO_ERROR) {
                LOG(ERROR) << "RdKafka failed to start consumer : " << RdKafka::err2str(resp);
                return false;
            }
            LOG(INFO) << "Part " << i << " INIT DONE WithOffset: " << RdKafka::Topic::OFFSET_END;
        }
    }
    else{
        if(param.partitions_offset.size() != partition_num_){
            LOG(ERROR) << "partition offset missmatch " << param.partitions_offset.size() << " != " << partition_num_;
            return false;
        }
        
        for (size_t i = 0; i < param.partitions_offset.size(); i++) {
            if (auto offset = param.partitions_offset[i]; offset != RdKafka::Topic::OFFSET_INVALID) {
                RdKafka::ErrorCode resp = rd_consumer_->start(rd_topic_.get(), i, offset, rd_queue_.get());
                if (resp != RdKafka::ERR_NO_ERROR) {
                    LOG(ERROR) << "RdKafka failed to start consumer : " << RdKafka::err2str(resp);
                    return false;
                }
                LOG(INFO) << "Part " << i << " INIT DONE WithOffset: " << offset;
            }
        }
    }
    finished_ = false;
    threads_ = std::thread(&KafkaConsumer::loop, this);
    return true;
 }

 bool KafkaConsumer::check_message(const RdKafka::Message &msg){
    switch(msg.err()){
        case RdKafka::ERR_NO_ERROR:{
            return true;
        }
        case RdKafka::ERR__TIMED_OUT: {
            LOG(WARNING) << "Kafka timout last offset: " << msg.offset() << " part: " << msg.partition();
            break;
        }
        case RdKafka::ERR__PARTITION_EOF: {
            break;
        }
        case RdKafka::ERR__UNKNOWN_TOPIC:
        case RdKafka::ERR__UNKNOWN_PARTITION:
        default:
            LOG(ERROR) << "Consume failed: " << msg.errstr();
            break;
    }
    return false;
 }

void KafkaConsumer::destory(){
    finished_ = true;
}

void KafkaConsumer::join(){
    threads_.join();
}

void KafkaConsumer::loop(){
    while(!finished_){
        auto msg   = std::shared_ptr<RdKafka::Message>(rd_queue_->consume(timeout_ms_));
        if(check_message(*msg)){
            on_message(*msg);
        }
    }
 }

 }; //namespace kafka_util{