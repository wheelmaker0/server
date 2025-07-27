#pragma once
#include <stdint.h>
#include <string>

namespace common {

    uint32_t hash(const char* data, size_t n, uint32_t seed);
    uint32_t bloomHash(const std::string &key);
    bool KeyMayMatchInternal(uint32_t h, uint32_t delta, const std::string &filter);
    bool keyMayMatch(const std::string &key, const std::vector<const std::string *> &filter_vec);
    bool keyMayMatch(const std::string &key, const std::string &filter);

}; //namespace common

