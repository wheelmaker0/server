#include "hdfs_test.h"
#include "hdfs_util/hdfs_util.h"
#include "common/file_util.h"
#include <glog/logging.h>

void GetHdfsFile::run() {

    hdfs_util::HdfsClient client;
    if (!client.init("default", 0, "hot_weibo")) {
        LOG(ERROR) << "init hdfs client fail";
        return ;
    }

    if(!client.exist(remote_path)){
        LOG(ERROR) << "file " << remote_path << " not exist";
        return ;
    }

    std::string data;
    if(!client.read_whole_file(remote_path, data)){
        LOG(ERROR) << "read file " << remote_path << " error";
        return ;
    }

    if(!file_util::write_file(local_path, data.data(), data.size())){
        LOG(ERROR) << "write file " << local_path << " error";
        return ;
    }
}