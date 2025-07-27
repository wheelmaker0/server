#pragma once
#include <map>
#include <string>

#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/ini_parser.hpp"
#include "boost/property_tree/exceptions.hpp"

namespace common{

class Config{
public:
    int64_t get(const std::string &key, int64_t def_val) const;
    int get(const std::string &key, int def_val) const;
    double get(const std::string &key, double def_val) const;
    std::string get(const std::string &key, const std::string &def_val) const;
    void set(const std::string &key, const std::string &value);
    void dump_to_log() const;
    bool parser_ini(const std::string &config_path);
    void parser_ini(const boost::property_tree::ptree &tree, std::string path);
private:
    std::map<std::string, std::string> config_map_;
};

}; // namespace common