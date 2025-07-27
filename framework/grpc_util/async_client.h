#pragma once
#include "zk_util/zk_client.h"
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <cstdint>
#include <future>
//std::future 不知道和哪个库冲突了，在此项目下调用set_value会crash，因此改用boost
#include <boost/thread/future.hpp>
#include <grpcpp/grpcpp.h>

namespace grpc_util{

template<class RESPONSE_TYPE>
struct CallData{
    grpc::ClientContext context{};
    std::unique_ptr<grpc::ClientAsyncResponseReader<RESPONSE_TYPE>> response_reader{};
    boost::promise<int> promise{};
};

template<class REQUEST_TYPE, class RESPONSE_TYPE, class SERVICE_TYPE>
class AsyncClient{
public:
    AsyncClient(){}
    AsyncClient(const std::string &address):address_(address),client_(SERVICE_TYPE::NewStub(grpc::CreateChannel(address, grpc::InsecureChannelCredentials()))) {}
    virtual ~AsyncClient(){}

    boost::unique_future<int> async_call(const REQUEST_TYPE &request, RESPONSE_TYPE *response, grpc::Status *status, grpc::CompletionQueue *cq, int timeout_ms){
        CallData<RESPONSE_TYPE> *call = new CallData<RESPONSE_TYPE>();
        call->context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(timeout_ms));
        // LOG(INFO) << "add CallData: " << call << " ,p: " << &(call->promise) << " ,c: " << &(call->context);
        // call->response_reader = client_->PrepareAsyncTotalRecall(&(call->context), request, cq);
        //先get_future,不然有可能call先到完成队列，然后被销毁了
        auto ret = (call->promise).get_future();
        call->response_reader = prepare_async_call(&(call->context), request, cq);
        call->response_reader->StartCall();
        call->response_reader->Finish(response, status, (void*)call);
        return ret;
    }

    std::string get_address(){
        return address_;
    }

protected:
    virtual std::unique_ptr<grpc::ClientAsyncResponseReader<RESPONSE_TYPE>> prepare_async_call(grpc::ClientContext *context, const REQUEST_TYPE &request, grpc::CompletionQueue *cq) = 0;
    std::string address_;
    std::unique_ptr<typename SERVICE_TYPE::Stub> client_;
};

template<class REQUEST_TYPE, class RESPONSE_TYPE, class ClientType>
class AsyncCluster{
public:
    class Cluster : public zk_util::ZKClient::ChildrenWatcher{
    public:
        Cluster(){}
        //该函数由zk线程回调
        virtual void operator()(absl::flat_hash_set<std::string> &&children){
            std::unique_lock lock(mutex);
            int pos = 0;
            for(int i = 0; i < cluster_.size(); i++){
                if(children.count(cluster_[i]->get_address()) == 0){
                    LOG(INFO) << "erase node << " << cluster_[i]->get_address();
                    continue;
                }
                children.erase(cluster_[i]->get_address());
                cluster_[pos++] = std::move(cluster_[i]);
            }
            cluster_.resize(pos);
            for(auto &c : children){
                LOG(INFO) << "add node << " << c;
                cluster_.emplace_back(new ClientType(c));
            }
        }

        std::shared_ptr<ClientType> get_client(uint32_t index){
            std::shared_lock lock(mutex);
            if(cluster_.empty()){
                return nullptr;
            } 
            return cluster_[index%cluster_.size()];
        }

    private:
        std::shared_mutex mutex;
        std::vector<std::shared_ptr<ClientType>> cluster_;
    };

    AsyncCluster():cnt_(0),cluster_(new Cluster()){}

    ~AsyncCluster(){
        if(thread_.joinable()){
            cq_.Shutdown();
            thread_.join();
        }
    }

    bool init(const std::string &zk_path, const std::shared_ptr<zk_util::ZKClient> &zk_client){
        zk_path_ = zk_path;
        zk_client_ = zk_client;
        if(!zk_client_->exist(zk_path_)){
            LOG(ERROR) << "zk path " << zk_path_ << " not exist";
            return false;
        }
        zk_client_->watch_children(zk_path_, cluster_);
        thread_ = std::thread(&AsyncCluster::loop, this);
        return true;
    }
 
    void destory(){
        cq_.Shutdown();
        thread_.join();
    }

    boost::unique_future<int> async_call(const REQUEST_TYPE &request, RESPONSE_TYPE &response, grpc::Status &status, int timeout_ms){
        auto client = cluster_->get_client(cnt_.fetch_add(1));
        if(client == nullptr){
            boost::promise<int> promise;
            promise.set_value(-1);
            return promise.get_future();
        }
        // LOG(INFO) << "use client : " << client->get_address() << " : " << client.get();
        return client->async_call(request, &response, &status, &cq_, timeout_ms);
    }

private:
    void loop() {
        void* got_tag = nullptr;
        bool ok = false;
        while (cq_.Next(&got_tag, &ok)) {
            CallData<RESPONSE_TYPE>* call = static_cast<CallData<RESPONSE_TYPE>*>(got_tag);
            // LOG(INFO) << "tag: " << got_tag << " ,ok: " << ok << " ,p: " << &(call->promise) << " ,peer: " << call->context.peer();
            //这里有偶现的core加个日志确认一下
            if(call == nullptr){
                LOG(ERROR) << "call nullptr " << ok;
                continue;
            }
            (call->promise).set_value((int)ok);
            delete call;
           
        }
    }

    std::atomic<uint32_t> cnt_{};
    std::string zk_path_{};
    std::shared_ptr<zk_util::ZKClient> zk_client_{};
    std::shared_ptr<Cluster> cluster_{};
    grpc::CompletionQueue cq_{};
    std::thread thread_;
};

}; //grpc_util