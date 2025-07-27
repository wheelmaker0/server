#pragma once
#include <dirent.h>
#include <vector>
#include <string>

namespace file_util{
    std::vector<std::string> list_files(const std::string &path, const std::string &pattern);
    uint64_t get_file_mtime(const std::string &file_name);
    bool read_file(const std::string &file_path, std::string &data);
    bool write_file(const std::string &file_path, char *data, size_t data_len);
} //namespace file_util