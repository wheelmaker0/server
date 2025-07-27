#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <type_traits>
#include "nlohmann.hpp"

namespace common{

template <typename T>
inline void AddDocument(rapidjson::Document &doc, const std::string &str_key, const T &value) {
    auto &allocator = doc.GetAllocator();
    rapidjson::Value key(str_key.c_str(), allocator);

    if constexpr (std::is_same_v<T, int64_t>) {
        doc.AddMember(key, rapidjson::Value().SetInt64(value), allocator);
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        doc.AddMember(key, rapidjson::Value().SetUint64(value), allocator);
    } else if constexpr (std::is_same_v<T, int>) {
        doc.AddMember(key, rapidjson::Value().SetInt(value), allocator);
    } else if constexpr (std::is_same_v<T, bool>) {
        doc.AddMember(key, rapidjson::Value().SetBool(value), allocator);
    } else if constexpr (std::is_same_v<T, double>) {
        doc.AddMember(key, rapidjson::Value().SetDouble(value), allocator);
    } else if constexpr (std::is_same_v<T, float>) {
        doc.AddMember(key, rapidjson::Value().SetFloat(value), allocator);
    } else if constexpr (std::is_same_v<T, std::string>) {
        doc.AddMember(key, rapidjson::Value().SetString(value.c_str(), value.size(), allocator), allocator);
    } else if constexpr (std::is_same_v<T, std::unordered_set<uint64_t>>) {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const auto &v : value) {
            array.PushBack(rapidjson::Value().SetUint64(v), allocator);
        }
        doc.AddMember(key, array, allocator);
    } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const auto &v : value) {
            array.PushBack(rapidjson::Value().SetInt64(v), allocator);
        }
        doc.AddMember(key, array, allocator);
    } else if constexpr (std::is_same_v<T, std::vector<uint64_t>>) {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const auto &v : value) {
            array.PushBack(rapidjson::Value().SetUint64(v), allocator);
        }
        doc.AddMember(key, array, allocator);
    } else if constexpr (std::is_same_v<T, std::vector<int>>) {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const auto &v : value) {
            array.PushBack(rapidjson::Value().SetInt(v), allocator);
        }
        doc.AddMember(key, array, allocator);
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const auto &v : value) {
            array.PushBack(rapidjson::Value().SetString(v.c_str(), v.size(), allocator), allocator);
        }
        doc.AddMember(key, array, allocator);
    } else if constexpr (std::is_same_v<T, std::unordered_map<std::string, int64_t>>) {
        rapidjson::Value map(rapidjson::kObjectType);
        for (const auto &kv : value) {
            map.AddMember(rapidjson::Value(kv.first.c_str(), kv.first.size(), allocator),
                          rapidjson::Value().SetInt64(kv.second), allocator);
        }
        doc.AddMember(key, map, allocator);
    } else {
        return;
    }
}

template<class T>
T get_value_from_json(const nlohmann::json &data, const std::string &key, const T &def){
    try{
        return data[key].get<T>();
    }
    catch(std::exception &e){
        //LOG(ERROR) << e.what() << " ,key: " << key << " ,data: " << data.dump();
    }
    return def;
}

};