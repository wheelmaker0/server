#pragma once

#include <atomic>
#include <map>
#include <string>
#include <thread>
#include "zk_client.h"

namespace zk_util{

class ServicePublisher{
public:
    ServicePublisher(const std::string &host, std::map<std::string, std::string> path_2_hosts):zk_client_(host),path_2_hosts_(std::move(path_2_hosts)){
        zk_client_.connect();
        finished_ = false;
        for(auto &item : path_2_hosts_){
            bool ret = zk_client_.create(item.first, path_2_real_path_[item.first], item.second);
            if(!ret){
                LOG(ERROR) << "register zk failed on " << item.first << ", ip: " << item.second;
            }
            else{
                LOG(INFO) << "register zk success on " << path_2_real_path_[item.first] << ", ip: " <<  item.second;
            }
        }
        thread_ = std::thread(&ServicePublisher::loop, this);
    }

    ~ServicePublisher(){
        finished_ = true;
        thread_.join();
        for(auto &item : path_2_real_path_){
            LOG(INFO) << "del zk: " << item.second;
            zk_client_.del(item.second);
        }
        zk_client_.disconnect();
        // while(zk_client_.is_connected());
        // LOG(INFO) << "after while: " << zk_client_.is_connected();
        // google::FlushLogFiles(google::GLOG_INFO);
    }

private:
    ZKClient zk_client_;
    void loop(){
        while(!finished_){
            for(auto &item : path_2_real_path_){
                if(!zk_client_.exist(item.second)){
                    std::string output_path = "";
                    auto ret = zk_client_.create(item.second, output_path, path_2_hosts_[item.first]);
                    if(!ret){
                        LOG(ERROR) << "register zk failed on " << item.second << ", ip: " << path_2_hosts_[item.first];
                    }
                    else{
                        LOG(INFO) << "register zk success on " << item.second << ", ip: " << path_2_hosts_[item.first];
                        item.second = output_path;
                    }
                }
                else{
                    // LOG(INFO) << "zk exist on " << item.second << ", ip: " << path_2_hosts_[item.first];
                }
            }
            sleep(5);
        }
    }

    std::atomic<bool> finished_;
    std::thread thread_;
    std::map<std::string, std::string> path_2_hosts_;
    std::map<std::string, std::string> path_2_real_path_;
};

} //namespace zk_util