#pragma once
#include <numeric>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <map>
#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <type_traits>
#include <fstream>
#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <boost/algorithm/string.hpp>
#include <boost/core/demangle.hpp>
#include <boost/any.hpp>
#include <glog/logging.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include "nlohmann.hpp"

namespace common{

template <typename T, typename T_INDEX>
void apply_permutation(std::vector<T>& vec, const std::vector<T_INDEX>& p){
    std::vector<T> ret(p.size());
    std::transform(p.begin(), p.end(), ret.begin(), [&](T_INDEX i){ return vec[i]; });
    vec = std::move(ret);
}

template <typename T, typename T_INDEX, typename CMP=std::less<T> >
void get_sorted_permutation(const std::vector<T>& vec, std::vector<T_INDEX>& p){
    p.resize(vec.size());
    for(T_INDEX i = 0; i < vec.size(); i++){
        p[i] = i;
    }
    std::sort(p.begin(), p.end(), [&](T_INDEX a, T_INDEX b){
        return CMP()(vec[a], vec[b]);
    });
}

///////////////////////// embedding相关 /////////////////////////

inline bool is_zero(const float *a, size_t n){
    for(size_t i = 0; i < n; i++){
        if(a[i] != 0.0f){
            return false;
        }
    }
    return true;
}

///////////////////////// 各种split /////////////////////////////

inline bool str_2_any(const std::string_view &src, float &dst){
    return absl::SimpleAtof(src, &dst);
}

inline bool str_2_any(const std::string_view &src, double &dst){
    return absl::SimpleAtod(src, &dst);
}

inline bool str_2_any(const std::string_view &src, bool &dst){
    return absl::SimpleAtob(src, &dst);
}

inline bool str_2_any(const std::string_view &src, std::string_view &dst){
    dst = src;
    return true;
}

inline bool str_2_any(const std::string_view &src, std::string &dst){
    dst = src;
    return true;
}

template <typename INT_TYPE>
bool str_2_any(const std::string_view &src, INT_TYPE &dst){
    return absl::SimpleAtoi(src, &dst);
}

template<typename Delimiter, typename T1, typename T2>
size_t split_2_pair_vec(const std::string_view &input, const Delimiter &d, std::vector<std::pair<T1, T2>> &output){
    output.clear();
    std::vector<std::string_view> str_pair_vec = absl::StrSplit(input, d);
    for(size_t i = 0; i + 1 < str_pair_vec.size(); i += 2){
        T1 t1;
        T2 t2;
        if(str_2_any(str_pair_vec[i] ,t1) && str_2_any(str_pair_vec[i+1] ,t2)){
            output.emplace_back(t1, t2);
        }
    }
    return output.size();
}

template<typename Delimiter, typename T>
size_t split_2_vec(const std::string_view &input, const Delimiter &d, std::vector<T> &output, size_t step = 1, size_t offset = 0){
    output.clear();
    std::vector<std::string_view> str_pair_vec = absl::StrSplit(input, d);
    for(size_t i = offset; i < str_pair_vec.size(); i += step){
        T t;
        if(str_2_any(str_pair_vec[i] ,t)){
            output.emplace_back(t);
        }
    }
    return output.size();
}
template<class T, class P>
bool split_2_struct_vec(const std::string_view &input, char d1, char d2, std::vector<T> &output, P &cvt){
    std::vector<std::string_view> rows = absl::StrSplit(input, d1);
    output.reserve(output.size() + rows.size());
    for(auto &r : rows){
        std::vector<std::string_view> cols = absl::StrSplit(r, d2);
        output.emplace_back();
        if(!cvt(cols, output.back())){
            output.pop_back();
            continue;
        }
    }
    return true;
}

template<class T>
bool serialize_container(const T &container, std::string &data){
    try{
        nlohmann::json json_obj(container);
        data = json_obj.dump();
    }catch(const std::exception &e){
        LOG(ERROR) << e.what();
        return false;
    }
    return true;
}

template<class T>
bool deserialize_container(const std::string &data, T &container){
    try{
        nlohmann::json json_obj = nlohmann::json::parse(data);
        container = json_obj.get<T>();
    }
    catch(const std::exception &e){
        LOG(ERROR) << e.what();
        return false;
    }
    return true;
}

template<class T>
bool load_container(const std::string &file_path, T &container){
    std::ifstream ifs(file_path);
    if(!ifs){
        LOG(ERROR) << "open file failed " << file_path;
        return false;
    }
    try{
        nlohmann::json json_obj = nlohmann::json::parse(ifs);
        container = json_obj.get<T>();
    }
    catch(const std::exception &e){
        LOG(ERROR) << e.what();
        return false;
    }
    return true;
}

template<class T>
bool save_container(const std::string &file_path, const T &container){
    std::ofstream ofs(file_path);
    if(!ofs){
        LOG(ERROR) << "open file failed " << file_path;
        return false;
    }
    try{
        nlohmann::json json_obj(container);
        ofs << json_obj;
    }catch(const std::exception &e){
        LOG(ERROR) << e.what();
        return false;
    }
    return true;
}

inline bool load_config(const std::string &path, nlohmann::json &config){
    std::ifstream ifs(path);
    if(!ifs){
        LOG(ERROR) << "open file failed " << path;
        return false;
    }
    try{
        config = nlohmann::json::parse(ifs);
    }catch(const std::exception &e){
        LOG(ERROR) << e.what();
        return false;
    }
    return true;
}

//////////////////////// 各种split ///////////////////////////////

/////////////// 各种嵌套容器to string ////////////////////////

inline std::string binary_to_string(char *data, size_t len){
    std::stringstream ss;
    ss << std::hex;
    for(size_t i = 0; i < len; i++){
        ss << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return ss.str();
}

inline std::string to_string(const rapidjson::Document &doc){
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

template<typename T>
void container_2_str(const T &v, std::stringstream &ss){
    ss << v;
    return ;
}

inline void container_2_str(const std::string &v, std::stringstream &ss){
    if(v.size() == 0){
        ss << "\"\"";
        return ;
    }
    //已经是json格式了
    if(v[0] == '"' || v[0] == '{' || v[0] == '['){
        ss << v;
    }
    else{
        ss << '"' << v << '"';
    }
    return ;
}

inline void container_2_str(const std::string *v, std::stringstream &ss){
    container_2_str(*v, ss);
}

//这些模板会相互调用，所以先声明一下

template<typename K, typename V>
void container_2_str(const std::pair<K,V> &c, std::stringstream &ss);

template<typename T>
void container_2_str(const std::shared_ptr<T> &c, std::stringstream &ss);

template<typename T>
void container_2_str(const std::unique_ptr<T> &c, std::stringstream &ss);

template<typename T>
void container_2_str(const std::vector<T> &c, std::stringstream &ss);

template<typename T>
void container_2_str(const std::set<T> &c, std::stringstream &ss);

template<typename T>
void container_2_str(const std::unordered_set<T> &c, std::stringstream &ss);

template<typename K, typename V>
void container_2_str(const std::map<K,V> &c, std::stringstream &ss);

template<typename K, typename V>
void container_2_str(const std::unordered_map<K,V> &c, std::stringstream &ss);

inline void container_2_str(const boost::any &v, std::stringstream &ss){
    // LOG(INFO) << boost::core::demangle(v.type().name());
    if(v.empty() ){
        ss << "\"\"";
        return ;
    }
    if(v.type() == typeid(nullptr)){
        ss << "\"null\"";
        return ;
    }
    if(v.type() == typeid(std::string)){
        container_2_str(boost::any_cast<std::string>(v), ss);
    }else if(v.type() == typeid(std::vector<boost::any>)){
        container_2_str(boost::any_cast<std::vector<boost::any>>(v), ss);
    }else if(v.type() == typeid(std::map<std::string, boost::any>)){
        container_2_str(boost::any_cast<std::map<std::string, boost::any>>(v), ss);
    }else if(v.type() == typeid(std::unordered_map<std::string, boost::any>)){
        container_2_str(boost::any_cast<std::unordered_map<std::string, boost::any>>(v), ss);
    }else if(v.type() == typeid(int8_t)){
        ss << boost::any_cast<int8_t>(v);
    }else if(v.type() == typeid(int8_t)){
        ss << boost::any_cast<int8_t>(v);
    }else if(v.type() == typeid(uint8_t)){
        ss << boost::any_cast<uint8_t>(v);
    }else if(v.type() == typeid(int16_t)){
        ss << boost::any_cast<int16_t>(v);
    }else if(v.type() == typeid(uint16_t)){
        ss << boost::any_cast<uint16_t>(v);
    }else if(v.type() == typeid(int32_t)){
        ss << boost::any_cast<int32_t>(v);
    }else if(v.type() == typeid(uint32_t)){
        ss << boost::any_cast<uint32_t>(v);
    }else if(v.type() == typeid(int64_t)){
        ss << boost::any_cast<int64_t>(v);
    }else if(v.type() == typeid(uint64_t)){
        ss << boost::any_cast<uint64_t>(v);
    }else if(v.type() == typeid(float)){
        ss << boost::any_cast<float>(v);
    }else if(v.type() == typeid(double)){
        ss << boost::any_cast<double>(v);
    }else if(v.type() == typeid(bool)){
        ss << boost::any_cast<bool>(v);
    }else{
        LOG(ERROR) << "unknown type: " << boost::core::demangle(v.type().name());
    }
    return ;
}

//key 必须是基础类型
template<typename K, typename V>
void container_2_str(const std::pair<K,V> &c, std::stringstream &ss){
    ss << "{\"" << c.first << "\"";
    ss << ":";
    container_2_str(c.second, ss);
    ss << "}";
    return ;
}

//key 必须是基础类型
template<typename K, typename V>
void map_pair_2_str(const std::pair<K,V> &c, std::stringstream &ss){
    ss << "\"" << c.first << "\"";
    ss << ":";
    container_2_str(c.second, ss);
    return ;
}

template<typename T>
void container_2_str(const std::shared_ptr<T> &c, std::stringstream &ss){
    return container_2_str(*c, ss);
}

template<typename T>
void container_2_str(const std::unique_ptr<T> &c, std::stringstream &ss){
    return container_2_str(*c, ss);
}

template<typename T>
void container_2_str(const std::vector<T> &c, std::stringstream &ss){
    if(c.size() == 0){
        ss << "[]";
        return ;
    }
    ss << "[";
    auto iter = c.begin();
    container_2_str(*iter, ss);
    for(++iter; iter != c.end(); ++iter){
        ss << ",";
        container_2_str(*iter, ss);
    }
    ss << "]";
    return ;
}

template<typename T>
void container_2_str(const std::set<T> &c, std::stringstream &ss){
    if(c.size() == 0){
        ss << "[]";
        return ;
    }
    ss << "[";
    auto iter = c.begin();
    container_2_str(*iter, ss);
    for(++iter; iter != c.end(); ++iter){
        ss << ",";
        container_2_str(*iter, ss);
    }
    ss << "]";
    return ;
}

template<typename T>
void container_2_str(const std::unordered_set<T> &c, std::stringstream &ss){
    if(c.size() == 0){
        ss << "[]";
        return ;
    }
    ss << "[";
    auto iter = c.begin();
    container_2_str(*iter, ss);
    for(++iter; iter != c.end(); ++iter){
        ss << ",";
        container_2_str(*iter, ss);
    }
    ss << "]";
    return ;
}

template<typename T>
void container_2_str(const absl::flat_hash_set<T> &c, std::stringstream &ss){
    if(c.size() == 0){
        ss << "[]";
        return ;
    }
    ss << "[";
    auto iter = c.begin();
    container_2_str(*iter, ss);
    for(++iter; iter != c.end(); ++iter){
        ss << ",";
        container_2_str(*iter, ss);
    }
    ss << "]";
    return ;
}

template<typename K, typename V>
void container_2_str(const std::map<K,V> &c, std::stringstream &ss){
    if(c.size() == 0){
        ss << "{}";
        return ;
    }
    ss << "{";
    auto iter = c.begin();
    map_pair_2_str(*iter, ss);
    for(++iter; iter != c.end(); ++iter){
        ss << ","; 
        map_pair_2_str(*iter, ss);
    }
    ss << "}";
    return ;
}

template<typename K, typename V>
void container_2_str(const std::unordered_map<K,V> &c, std::stringstream &ss){
    if(c.size() == 0){
        ss << "{}";
        return ;
    }
    ss << "{";
    auto iter = c.begin();
    map_pair_2_str(*iter, ss);
    for(++iter; iter != c.end(); ++iter){
        ss << ",";
        map_pair_2_str(*iter, ss);
    }
    ss << "}";
    return ;
}

template<typename K, typename V>
void container_2_str(const absl::flat_hash_map<K,V> &c, std::stringstream &ss){
    if(c.size() == 0){
        ss << "{}";
        return ;
    }
    ss << "{";
    auto iter = c.begin();
    map_pair_2_str(*iter, ss);
    for(++iter; iter != c.end(); ++iter){
        ss << ",";
        map_pair_2_str(*iter, ss);
    }
    ss << "}";
    return ;
}

template<typename T>
void array_2_str(const T *vec, size_t n, std::stringstream &ss){
    if(n == 0){
        ss << "[]";
        return ;
    }
    ss << "[";
    container_2_str(vec[0], ss);
    for(size_t i = 1; i < n; i++){
        ss << ",";
        container_2_str(vec[i], ss);
    }
    ss << "]";
    return ;
}

template<typename T>
std::string array_2_str(const T *vec, size_t n, size_t precision = 0){
    std::stringstream ss;
    if(precision != 0){
        ss.precision(precision);
    }
    array_2_str(vec, n, ss);
    return ss.str();
}

template<typename T>
std::string container_2_str(const T &c, size_t precision = 0){
    std::stringstream ss;
    if(precision != 0){
        ss.precision(precision);
    }
    container_2_str(c, ss);
    return ss.str();
}

template<typename T>
std::string container_2_json(const T &c){
    std::stringstream ss;
    try{
        nlohmann::json json_obj(c);
        ss << json_obj;
    }
    catch(const std::exception &e){
        LOG(ERROR) << e.what();
        return "";
    }
    return ss.str();
}

/////////////// 各种嵌套容器to string ////////////////////////

/////////////////  各种容器 diff ////////////////////////////

float variance(const float *a, const float *b, size_t n);

template<class T>
bool equal(const T &a, const T &b){
    return a == b;
}

bool equal(const std::vector<float> &a, const std::vector<float> &b);

template<class KEY_T, class VALUE_T>
static std::string container_diff(const std::unordered_map<KEY_T, VALUE_T> &src, const std::unordered_map<KEY_T, VALUE_T> &dst, bool show_details = false){
    std::stringstream ss;
    std::vector<std::pair<KEY_T, VALUE_T>> extra;
    std::vector<std::pair<KEY_T, VALUE_T>> miss;
    std::unordered_map<KEY_T, std::pair<VALUE_T, VALUE_T>> diff;
    size_t same_count = 0;
    for(const auto &[k, v]: src){
        auto iter = dst.find(k);
        if(iter == dst.end()){
            extra.emplace_back(k,v);
            continue;
        }
        if(equal(iter->second, v)){
            same_count++;
            continue;
        }
        auto &d = diff[k];
        d.first = v;
        d.second = iter->second;
    }
    for(const auto &[k, v]: dst){
        auto iter = src.find(k);
        if(iter == src.end()){
            miss.emplace_back(k, v);
        }
    }
    ss << "src_size: " << src.size() << std::endl;
    ss << "dst_size: " << dst.size() << std::endl;
    ss << "same_count: " << same_count << std::endl;
    ss << "extra_count: " << extra.size() << std::endl;
    ss << "miss_count: " << miss.size() << std::endl;
    ss << "diff_count: " << diff.size() << std::endl;
    if(show_details){
        ss << "extra: " << common::container_2_json(extra) << std::endl;
        ss << "miss: " << common::container_2_json(miss) << std::endl;
        ss << "diff: " << common::container_2_json(std::forward<std::unordered_map<KEY_T, std::pair<VALUE_T, VALUE_T>>>(diff)) << std::endl;
    }
    return ss.str();
}

template<class T>
static std::string container_diff(const std::unordered_set<T> &src, const std::unordered_set<T> &dst, bool show_details = false){
    std::stringstream ss;
    std::vector<T> extra;
    std::vector<T> miss;
    size_t same_count = 0;
    for(const auto &v: src){
        if(!dst.count(v)){
            extra.emplace_back(v);
        }
        else{
            same_count++;
        }
    }
    for(const auto &v: dst){
        if(!src.count(v)){
            miss.emplace_back(v);
        }
    }
    ss << "src_size: " << src.size() << std::endl;
    ss << "dst_size: " << dst.size() << std::endl;
    ss << "same_count: " << same_count << std::endl;
    ss << "extra_count: " << extra.size() << std::endl;
    ss << "miss_count: " << miss.size() << std::endl;
    if(show_details){
        ss << "extra: " << common::container_2_json(extra) << std::endl;
        ss << "miss: " << common::container_2_json(miss) << std::endl;
    }
    return ss.str();
}

template<typename T>
inline bool is_zero_vec(const T *v , size_t n){
    for(size_t i = 0; i < n; i++){
        if(v[i] != 0){
            return false;
        }
    }
    return true;
}

/////////////////////  各种容器 diff ////////////////////////////


};