#include "file_util.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>

namespace file_util{

std::vector<std::string> list_files(const std::string &path, const std::string &pattern){
    DIR *dir;
    std::vector<std::string> ret;
    if ((dir = opendir(path.c_str())) == NULL) {
        return ret;  
    }
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL) {
        if(strstr(ent->d_name, pattern.c_str())){
            std::string full = path + "/" + ent->d_name;
            ret.emplace_back(full);
        }
    }
    closedir(dir);
    return ret;
}

uint64_t get_file_mtime(const std::string &file_name){
    struct stat result;
    if(stat(file_name.c_str(), &result) == 0){
        return result.st_mtime;
    }
    return 0;
}

bool read_file(const std::string &file_path, std::string &data){
    std::ifstream ifs(file_path, std::ios::binary);
    if(!ifs){
        return false;
    }
    data = std::move(std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>()));
    return true;
}

bool write_file(const std::string &file_path, char *data, size_t data_len){
    std::ofstream ofs(file_path, std::ios::binary);
    if(!ofs){
        return false;
    }
    ofs.write(data, data_len);
    return true;
}

} //namespace file_util