#include "hdfs_util.h"
#include "hdfs.h"
#include <fcntl.h> /* for O_RDONLY, O_WRONLY */
#include <folly/ScopeGuard.h>

namespace hdfs_util{

bool HdfsClient::init(const std::string &host, int port, const std::string user_name){
    host_ = host;
    port_ = port;
    user_name_ = user_name;
    return reconnect();
}

bool HdfsClient::destory(){
    return hdfsDisconnect((hdfsFS) fs_) != -1;
}

bool HdfsClient::reconnect(){
    struct hdfsBuilder *builder = hdfsNewBuilder();
    if(builder == nullptr){
        return false;
    }
    hdfsBuilderSetNameNode(builder, host_.c_str());
    hdfsBuilderSetNameNodePort(builder, port_);
    if (!user_name_.empty()) {
      hdfsBuilderSetUserName(builder, user_name_.c_str());
    }
    hdfsBuilderSetForceNewInstance(builder);
    fs_ = (void *)hdfsBuilderConnect(builder);
    return fs_ != nullptr;
}

//file system api
bool HdfsClient::exist(const std::string &path){
    return 0 == hdfsExists((hdfsFS) fs_, path.c_str());
}

int HdfsClient::mkdir(const std::string &path){
    return hdfsCreateDirectory((hdfsFS) fs_, path.c_str());
}

int HdfsClient::list_dir(const std::string &path, std::vector<std::string> &output_list, int deep){
    int n = 0;
    hdfsFileInfo *fileInfo = hdfsListDirectory((hdfsFS) fs_, path.c_str(), &n);
    if (nullptr == fileInfo) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (fileInfo[i].mKind == kObjectKindDirectory) {
            if(deep < 0){
                output_list.emplace_back(fileInfo[i].mName);
            }
            else if(deep > 0){
                list_dir(fileInfo[i].mName, output_list, deep-1);
            }
        }
        else{
            output_list.emplace_back(fileInfo[i].mName);
        }
    }
    hdfsFreeFileInfo(fileInfo, n);
    return 0;
}

int HdfsClient::remove(const std::string &path, int recursive){
    return hdfsDelete((hdfsFS) fs_, path.c_str(), recursive);
}

int HdfsClient::move(const std::string &src, const std::string &dst){
    return hdfsRename((hdfsFS) fs_, src.c_str(), dst.c_str());
}

int64_t HdfsClient::get_file_size(const std::string &path){
    auto file_info = hdfsGetPathInfo((hdfsFS) fs_, path.c_str());
    if(file_info == nullptr){
        return -1;
    }
    int64_t ret = file_info->mSize;
    hdfsFreeFileInfo(file_info, 1);
    return ret;
}

//file api
void * HdfsClient::open_file(const std::string &path, int flags){
     return hdfsOpenFile((hdfsFS) fs_, path.c_str(), flags, 0, 0, 0);
}

int HdfsClient::close_file(void * fp){
    return hdfsCloseFile((hdfsFS) fs_, (hdfsFile)fp);
}

int HdfsClient::read_file(void * fp, char *buffer, size_t buf_size){
    return hdfsRead((hdfsFS) fs_, (hdfsFile)fp, buffer, buf_size);
}

bool HdfsClient::read_whole_file(const std::string &file_path, std::string &file_data){
    int64_t ret;
    ret = get_file_size(file_path);
    if(ret == -1){
        return false;
    }

    auto fp = hdfsOpenFile((hdfsFS) fs_, file_path.c_str(), O_RDONLY, 0, 0, 0);
    if(fp == nullptr){
        return false;
    }

    SCOPE_EXIT{ hdfsCloseFile((hdfsFS) fs_, fp); };

    file_data.resize(ret);
    int64_t readed = 0;
    while(readed != file_data.size()){
        ret = hdfsRead((hdfsFS) fs_, fp, (void *)(file_data.data() + readed), file_data.size() - readed);
        if(ret < 0){
            return false;
        }
        readed += ret;
    }
    return true;
}

int HdfsClient::write_file(void * fp, const char *buffer, size_t size){
    return hdfsWrite((hdfsFS) fs_, (hdfsFile)fp, buffer, size);
}

int HdfsClient::seek_file(void * fp, size_t pos){
    return hdfsSeek((hdfsFS) fs_, (hdfsFile)fp, pos);
}

int HdfsClient::flush_file(void * fp){
    return hdfsFlush((hdfsFS) fs_, (hdfsFile)fp);
}

}; //namespace hdfs_util