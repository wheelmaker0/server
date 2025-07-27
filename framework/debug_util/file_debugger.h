#pragma once
#include <string>
#include <fstream>
#include "nlohmann.hpp"

// #define DEBUG_FILE_DUMP_PATH "/nfs/bin/new/dump/"
// #define DEBUG_FILE_HACK_PATH "/nfs/bin/new/hack/"

#ifdef DEBUG_FILE_DUMP_PATH
#define DEBUG_DUMP_TO_FILE(...) debug_util::debug_dump_to_file(__VA_ARGS__)
#else
#define DEBUG_DUMP_TO_FILE(...)
#endif

#ifdef DEBUG_FILE_HACK_PATH
#define DEBUG_HACK_FROM_FILE(...) debug_util::debug_hack_from_file(__VA_ARGS__)
#else
#define DEBUG_HACK_FROM_FILE(...)
#endif

#if defined(DEBUG_FILE_DUMP_PATH) || defined(DEBUG_FILE_HACK_PATH)

#define FILE_DEBUG(...) debug_util::file_debug(__VA_ARGS__)

namespace debug_util{

inline bool debug_dump_to_file(const std::string &name, const std::string &data){
    std::string file_path = DEBUG_FILE_DUMP_PATH + name;
    std::ofstream ofs(file_path);
    if(!ofs){
        LOG(ERROR) << "open file failed " << file_path;
        return false;
    }
    ofs << data;
    return true;
}

inline bool debug_hack_from_file(const std::string &name, std::string &data){
    std::string file_path = DEBUG_FILE_HACK_PATH + name;
    std::ifstream ifs(file_path);
    if(!ifs){
        LOG(ERROR) << "open file failed " << file_path;
        return false;
    }
    ifs >> data;
    return true;
}

template<class T>
bool debug_dump_to_file(const std::string &name, const T &container){
    std::string file_path = DEBUG_FILE_DUMP_PATH + name;
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

template<class T>
bool debug_hack_from_file(const std::string &name, T &container){
    std::string file_path = DEBUG_FILE_HACK_PATH + name;
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
void file_debug(const std::string &name, T &container){
    DEBUG_HACK_FROM_FILE(name, container);
    DEBUG_DUMP_TO_FILE(name, container);
}

template<class T>
void file_debug(const std::string &name, const T &container){
    DEBUG_DUMP_TO_FILE(name, container);
}

} //  namespace debug_util

#else
#define FILE_DEBUG(...)
#endif

