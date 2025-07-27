#pragma once

#include <brpc/naming_service.h>
#include <brpc/periodic_naming_service.h>
#include <absl/strings/str_split.h>

#include <grpcpp/grpcpp.h>
#include <butil/endpoint.h>
#include "zk_client.h"
#include <glog/logging.h>
#include <shared_mutex>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <string>
#include <memory>

namespace zk_util {

class ZKNamingService : public brpc::PeriodicNamingService {
public:
    ZKNamingService(ZKClient *zk_client):zk_client_(zk_client) {} 

    std::vector<std::string> get_servers(const std::string &service_name){
        std::vector<std::string> ret;
        std::vector<std::string> children;
        if(!zk_client_->get_children(service_name, children)){
            return ret;
        }
        ret.reserve(children.size());
        for(auto &child : children){
            std::string ip_port;
            LOG(INFO) << "get child" << child;
            if(zk_client_->get(service_name + "/"+ child, ip_port)){
                ret.push_back(ip_port);
                LOG(INFO) << "node " << child << ": " << ip_port;
            }
        }
        return ret;
    }

    virtual int GetServers(const char *service_name, std::vector<brpc::ServerNode>* servers) override{
        LOG(INFO) << "GetServers: " << service_name;
        auto children = get_servers(service_name);
        servers->reserve(children.size());
        for(auto &child : children){
            butil::EndPoint pt;
            if(str2endpoint(child.c_str(), &pt) == -1){
                continue;
            }
            servers->emplace_back(pt);
            LOG(INFO) << child;
        }
        return 0; 
    }
    
    virtual NamingService* New() const{
        LOG(INFO) << "New";
        return new ZKNamingService(zk_client_);
    }

    virtual void Destroy(){
       delete this;
    }
private:
    ZKClient *zk_client_;
};

};//zk_util