#pragma once
#include "common/time_util.h"
#include "grpc_util/async_client.h"
#include "glog/logging.h"
#include "zk_util/zk_client.h"

#include <memory>
#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>
#include "proto/recommend_collaborate.grpc.pb.h"
#include "proto/recommend_server.grpc.pb.h"
#include "boost/format.hpp"

class TotalRecallClient : public grpc_util::AsyncClient<RecommendRequest, CollaRecommendResponse, CollaRecommendServer> {
public:
    TotalRecallClient(const std::string &address) : grpc_util::AsyncClient<RecommendRequest, CollaRecommendResponse, CollaRecommendServer>(address) {}

protected:
    virtual std::unique_ptr<grpc::ClientAsyncResponseReader<CollaRecommendResponse>> prepare_async_call(grpc::ClientContext *context, const RecommendRequest &request, grpc::CompletionQueue *cq) override{
        return client_->PrepareAsyncTotalRecall(context, request, cq);
    }
};

class GCRRecallClient: public grpc_util::AsyncClient<RecommendRequest, RecommendResponse, RecommendService> {
public:
    GCRRecallClient(const std::string &address) : grpc_util::AsyncClient<RecommendRequest, RecommendResponse, RecommendService>(address) {}

protected:
    virtual std::unique_ptr<grpc::ClientAsyncResponseReader<RecommendResponse>> prepare_async_call(grpc::ClientContext *context, const RecommendRequest &request, grpc::CompletionQueue *cq) override{
        return client_->PrepareAsyncRecommendRecall(context, request, cq);
    }
};

using TotalRecallCluster = grpc_util::AsyncCluster<RecommendRequest, CollaRecommendResponse, TotalRecallClient>;
using GCRRecallCluster = grpc_util::AsyncCluster<RecommendRequest, RecommendResponse, GCRRecallClient>;

//管理同一个服务的不同domain
template<typename ClusterType>
class ClusterManager {
public:
    ClusterManager(){}

    bool init(const std::string &zk_address, const std::string &zk_path_format){
        zk_path_format_ = zk_path_format;
        zk_client_ = std::make_shared<zk_util::ZKClient>(zk_address);
        if(!zk_client_->connect()){
            LOG(ERROR) << "connect zk fail " << zk_address;
            return false;
        }
        return true;
    }
    
    std::shared_ptr<ClusterType> get_or_create_cluster(const std::string &domain) {
        std::lock_guard<std::mutex> lock(mutex_);
        //一分钟内，连不上的domain就不重复连了
        auto iter = failed_domain_ts_.find(domain);
        if(iter != failed_domain_ts_.end()){
            int64_t ts = iter->second;
            if(common::now_in_s() - ts < 60){
                return nullptr;
            }
            else{
                failed_domain_ts_.erase(iter);
            }
        }

        auto ret = cluster_map_.insert({domain, nullptr});
        //插入成功，说明是新domain需要创建
        if(ret.second){
            auto &cluster = ret.first->second;
            cluster = std::make_shared<ClusterType>();
            std::string zk_path = boost::str(boost::format(zk_path_format_) % domain);
            if(!cluster->init(zk_path, zk_client_)){
                LOG(ERROR) << "init cluster " << domain << ": " << zk_path << " failed";
                failed_domain_ts_[domain] = common::now_in_s();
                cluster_map_.erase(ret.first);
                return nullptr;
            }
            LOG(INFO) << "init cluster " << domain << ": " << zk_path << " success";
            return cluster;
        }
        return ret.first->second;
    }

private:
    std::string zk_path_format_;
    std::mutex mutex_;
    std::shared_ptr<zk_util::ZKClient> zk_client_;
    absl::flat_hash_map<std::string, int64_t> failed_domain_ts_;
    absl::flat_hash_map<std::string, std::shared_ptr<ClusterType>> cluster_map_;
};