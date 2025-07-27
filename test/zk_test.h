#pragma once
#include <grpcpp/grpcpp.h>
#include "test.h"
#include "common/util.h"
#include "zk_util/zk_client.h"
#include "proto/recommend_collaborate.grpc.pb.h"
#include <string_view>
#include <string>
#include <memory>
#include <utility>
#include <common/util.h>
#include <stdio.h>
#include <future>
#include <boost/thread/future.hpp>
#include <folly/TokenBucket.h>
#include <boost/filesystem.hpp>
#include "common/file_util.h"
#include <absl/strings/str_split.h>
#include <cmath>
#include "server/rpc/grpc_cluster.h"
#include "zk_util/service_discovery.h"
#include "grpc_util/async_client.h"

class ZKTest : public Test{
protected:
    std::string zk_adress;
    class MyWatcher : public zk_util::ZKClient::ChildrenWatcher{
    public:
        MyWatcher(const std::string &path):path_(path){}
        virtual void operator()(absl::flat_hash_set<std::string> &&children) override{
            LOG(INFO) << "MyWatcher " << path_ << ": " << common::container_2_json(children);
        }
    private:
        std::string path_;
    };

public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("zk_adress,a", po::value<std::string>(&zk_adress)->default_value(""), "zk_adress");
    }

    virtual void run() override{
        zk_util::ZKClient zk_client(zk_adress);
        if(!zk_client.connect()){
            LOG(ERROR) << "connect zk fail " << zk_adress;
            return ;
        }

        std::string cmd;
        std::vector<std::shared_ptr<MyWatcher>> watchers;
        while(std::getline(std::cin, cmd)){
            std::vector<std::string> cmd_args = absl::StrSplit(cmd, ' ');
            int argn = cmd_args.size();
            std::string command = cmd_args[0];
            if(argn == 3 && command == "set"){
                if(!zk_client.set(cmd_args[1], cmd_args[2])){
                    LOG(ERROR) << "set " << cmd_args[1] << " " << cmd_args[2] << " fail";
                }
            }else if(argn == 3 && command == "get"){
                std::string output;
                bool watch = cmd_args[2] == "true";
                if(!zk_client.get(cmd_args[1], output, watch)){
                    LOG(ERROR) << "get " << cmd_args[1] << " fail";
                }else{
                    LOG(INFO) << "get " << cmd_args[1] << ": " << output;
                }
            }else if(argn == 3 && command == "get_children"){
                bool watch = cmd_args[2] == "true";
                std::vector<std::string> children;
                if(!zk_client.get_children(cmd_args[1], children, watch)){
                    LOG(ERROR) << "get_children " << cmd_args[1] << " fail";
                }else{
                    LOG(INFO) << "get_children " << cmd_args[1] << ": " << common::container_2_str(children);
                }
            }else if(argn == 3 && command == "get_children_value"){
                bool watch = cmd_args[2] == "true";
                absl::flat_hash_set<std::string> children;
                if(!zk_client.get_children_value(cmd_args[1], children, watch)){
                    LOG(ERROR) << "get_children_value " << cmd_args[1] << " fail";
                }else{
                    LOG(INFO) << "get_children_value " << cmd_args[1] << ": " << common::container_2_str(children);
                }
            }else if(argn == 2 && command == "watch_children"){
                watchers.emplace_back(new MyWatcher(cmd_args[1]));
                if(!zk_client.watch_children(cmd_args[1], watchers.back())){
                    LOG(ERROR) << "watch_children " << cmd_args[1] << " fail";
                }
            }else if(argn == 3 && command == "create"){
                std::string real_path;
                if(!zk_client.create(cmd_args[1], real_path, cmd_args[2])){
                    LOG(ERROR) << "get " << cmd_args[1] << " fail";
                }else{
                    LOG(ERROR) << "create " << cmd_args[1] << ": " << real_path ;
                }
            }else if(argn == 2 && command == "delete"){
                if(!zk_client.del(cmd_args[1])){
                    LOG(ERROR) << "delete " << cmd_args[1] << " fail";
                }
            }else if(argn == 3 && command == "exist"){
                bool watch = cmd_args[2] == "true";
                LOG(INFO) << "exist " << zk_client.exist(cmd_args[1], watch);
            }else if(command == "exit" || command == "quit"){
                return ;
            }else{
                LOG(ERROR) << "unknown command: " << cmd;
            }
        }
    }
};
class TotalRecallTest : public Test{
protected:
    std::string zk_adress;
    std::string service_path;
    int qps;
public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("zk_adress,a", po::value<std::string>(&zk_adress)->default_value(""), "zk_adress")
        ("service_path,p", po::value<std::string>(&service_path)->default_value(""), "service_path")
        ("qps,q", po::value<int>(&qps)->default_value(2000), "qps");
    }   
    
    void dummy_async_call(const std::string &address){
        RecommendRequest request;
        CollaRecommendResponse response;
        grpc::Status status;
        grpc::ClientContext context;
        grpc::CompletionQueue cq;

        std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
        std::unique_ptr< CollaRecommendServer::Stub> client = CollaRecommendServer::NewStub(channel);
        auto rpc = client->PrepareAsyncTotalRecall(&context, request, &cq);
        rpc->StartCall();
        rpc->Finish(&response, &status, (void*)1);

        void* got_tag;
        bool ok = false;
        LOG(INFO) << "Next: " << cq.Next(&got_tag, &ok);
        LOG(INFO) << "got_tag: " << got_tag << " ,ok: " << ok;
    }

    void dummy_sync_call(const std::string &address){
        RecommendRequest request;
        CollaRecommendResponse response;
        grpc::Status status;
        grpc::ClientContext context;
        std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
        std::unique_ptr< CollaRecommendServer::Stub> client = CollaRecommendServer::NewStub(channel);
        status = client->TotalRecall(&context, request, &response);
        LOG(INFO) << response.midinfos().size();
    }

    virtual void run() override{
        // dummy_async_call("10.30.76.49:9092");

        std::shared_ptr<zk_util::ZKClient> zk_client = std::make_shared<zk_util::ZKClient>(zk_adress);
        if(!zk_client->connect()){
            LOG(ERROR) << "connect zk fail " << zk_adress;
            return ;
        }
        
        TotalRecallCluster cluster;
        if(!cluster.init(service_path, zk_client)){
            LOG(ERROR) << "init cluster failed: " << service_path;
            return ;
        }

        folly::BasicDynamicTokenBucket<> token_bucket;
        std::vector<std::thread> threads;
        int nthreads = 32;
        LOG(INFO) << "qps: " << qps;
        for(int i = 0; i < nthreads; i++){
            threads.emplace_back([&](){
                while(true){
                    if(token_bucket.consume(1, qps, 32)){
                        RecommendRequest request;
                        CollaRecommendResponse response;
                        grpc::Status status;
                        auto f = cluster.async_call(request, response, status, 10);
                        auto ret = f.get();
                        if(ret != 1 ||!status.ok()){
                            LOG(INFO) << "ret: " << ret << " ,ok: " << status.ok() << " ,error_code: " << status.error_code() << " ,error_message: " << status.error_message();
                        }
                    }
                }
            });
        }
        for(int i = 0; i < nthreads; i++){
            threads[i].join();
        }
        // int n = 1024;
        // std::vector<boost::unique_future<int>> futures(n);
        // std::vector<CollaRecommendResponse> response(n);
        // std::vector<grpc::Status> status(n);
        // RecommendRequest request;
        // for(int i = 0; i < n; i++){
        //     futures[i] = cluster.async_call(request, response[i], status[i]);
        // }
        // for(int i = 0; i < n; i++){
        //     LOG(INFO) << i << " " << futures[i].get() << " ,midinfos: " << response[i].midinfos().size();
        // }
        // cluster.destory();
    }
};

class GrpcZKTest : public Test{
protected:
    std::string zk_adress;
    std::string service_path;
public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("zk_address,a", po::value<std::string>(&zk_adress)->default_value(""), "zk_address")
        ("service_path,p", po::value<std::string>(&service_path)->default_value(""), "service_path");
    }

    // void run_brpc(){
    //     zk_util::ZKClient zk_client(zk_adress);
    //     zk_util::ZKNamingService ns(&zk_client);
    //     brpc::Extension<const brpc::NamingService>::instance()->Register("zookeeper", &ns);
    //     brpc::ChannelOptions options;
    //     options.protocol = "h2:grpc";
    //     options.connect_timeout_ms = 100;
    //     options.timeout_ms         = 100;

    //     std::string zk = "zookeeper://" + service_path;
    //     brpc::Channel channel;
    //     auto ret = channel.Init("list://10.30.76.49:9092", "rr", &options);
    //     if(ret != 0){
    //         LOG(ERROR) << "init failed";
    //         return ;
    //     }

    //     brpc::CollaRecommendServer_Stub client(&channel);
    //     for(int i = 0; i < 10; i++){
    //         RecommendRequest request;
    //         CollaRecommendResponse response;
    //         brpc::Controller cntl;
    //         client.TotalRecall(&cntl, &request, &response, nullptr);
    //         if (!cntl.Failed()) {
    //             LOG(INFO) << "Received response from " << cntl.remote_side()
    //                 << " to " << cntl.local_side()
    //                 << ": " << response.midinfos().size()
    //                 << " latency=" << cntl.latency_us() << "us";
    //         } else {
    //             LOG(ERROR) << cntl.ErrorText();
    //         }
    //     }
    // }

    void run_grpc(){
        grpc::ChannelArguments args;
        args.SetLoadBalancingPolicyName("round_robin");
        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel("ipv4:///1.2.3.4:9092,1.2.2.3:9092", 
                                                                grpc::InsecureChannelCredentials(), args);
        
        std::vector<std::unique_ptr<CollaRecommendServer::Stub>> stub;
        int n_channel = 1;
        for(int s = 0; s < n_channel; s++){
            stub.emplace_back(CollaRecommendServer::NewStub(channel));
        }
        
        while(true){
            for(int s = 0; s < n_channel; s++){
                RecommendRequest request;
                CollaRecommendResponse response;
                grpc::ClientContext context;
                grpc::Status status;
                status = stub[s]->TotalRecall(&context, request, &response);
                if(!status.ok()) {
                    LOG(ERROR) << status.error_code() << ": " << status.error_message();
                }else{
                    LOG(INFO) << s << " " << context.peer() << " ,midinfos: " << response.midinfos().size();
                }
            }
        }
    }

    virtual void run() override{
        run_grpc();
    }
};