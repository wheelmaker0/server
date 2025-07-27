#pragma once
#include <string>
#include <vector>

namespace hdfs_util{

class HdfsClient{
public:

    //connect 
    bool init(const std::string &host, int port, const std::string user_name);

    bool destory();

    bool reconnect();
    
    //file system api
    bool exist(const std::string &path);

    int mkdir(const std::string &path);
    
    //deep > 0， 只显示文件，deep代表扫描目录深度数。 deep < 0，代表显示文件和目录
    int list_dir(const std::string &path, std::vector<std::string> &file, int deep);

    int remove(const std::string &path, int recursive);

    int move(const std::string &str, const std::string &dst);

    int64_t get_file_size(const std::string &path);

    //file api
    void *open_file(const std::string &path, int flags);

    int close_file(void * fp);

    int read_file(void * fp, char *buffer, size_t size);

    bool read_whole_file(const std::string &file_path, std::string &file_data);

    int write_file(void * fp, const char *buffer, size_t size);

    int seek_file(void * fp, size_t pos);

    int flush_file(void * fp);

protected:
    std::string host_;
    int port_;
    std::string user_name_;
    void *fs_;
};


}; //namespace hdfs_util