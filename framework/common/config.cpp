#include "config.h"
#include <absl/strings/numbers.h>
#include <glog/logging.h>

namespace common{
    
int64_t Config::get(const std::string &key, int64_t def_val) const{
    auto iter = config_map_.find(key);
    if(iter == config_map_.end()){
        return def_val;
    }
    int64_t ret;
    if(!absl::SimpleAtoi(iter->second, &ret)){
        return def_val;
    }
    return ret;
}

int Config::get(const std::string &key, int def_val) const{
    auto iter = config_map_.find(key);
    if(iter == config_map_.end()){
        return def_val;
    }
    int ret;
    if(!absl::SimpleAtoi(iter->second, &ret)){
        return def_val;
    }
    return ret;
}

double Config::get(const std::string &key, double def_val) const{
    auto iter = config_map_.find(key);
    if(iter == config_map_.end()){
        return def_val;
    }
    double ret;
    if(!absl::SimpleAtod(iter->second, &ret)){
        return def_val;
    }
    return def_val;
}

std::string Config::get(const std::string &key, const std::string &def_val) const{
    auto iter = config_map_.find(key);
    if(iter == config_map_.end()){
        return def_val;
    }
    return iter->second;
}

void Config::set(const std::string &key, const std::string &value){
    config_map_[key] = value;
}

void Config::dump_to_log() const{
    for(auto &item : config_map_){
        LOG(INFO) << "config " << item.first << ": " << item.second;
    }
}

bool Config::parser_ini(const std::string &config_path){
    boost::property_tree::ptree configs;
    try {
        boost::property_tree::ini_parser::read_ini(config_path, configs);
    } catch (boost::property_tree::ini_parser::ini_parser_error& e) {
        LOG(ERROR) << "parse config " << config_path << " error: " << e.what();
        return false;
    }
    parser_ini(configs, "");
    return true;
} 

void Config::parser_ini(const boost::property_tree::ptree &tree, std::string path){
    if(tree.empty()){
        if(!path.empty() && path[path.size()-1] == '.'){
            path.pop_back();
        }
        config_map_[path] = tree.data();
        return ;
    }
    for(auto &it : tree){
        parser_ini(it.second, path + it.first + ".");
    }
    return ;
}

}; // namespace common