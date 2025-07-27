#pragma once
#include<string>
#include<vector>
#include<inttypes.h>

namespace kafka_util{

struct KafkaParam{
public:
    std::string brokers{};
    std::string topics{};
    std::string group_id{};
    std::string username{};
    std::string password{};
    std::string compress{};
    std::vector<uint64_t> partitions_offset{};
    uint64_t timeout_ms = 1000;
};

}; //namespace kafka_util{