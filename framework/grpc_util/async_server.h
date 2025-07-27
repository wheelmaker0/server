#pragma once

#include <memory>
#include <iostream>
#include <string>
#include <thread>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include <grpcpp/support/async_unary_call.h>

namespace grpc_util{

template<class REQUEST_TYPE, class RESPONSE_TYPE, class SERVICE_TYPE>
class AsyncServer{
public:
    virtual ~AsyncServer(){}
    virtual void async_call(::grpc::ServerContext* context, REQUEST_TYPE* request, ::grpc::ServerAsyncResponseWriter< RESPONSE_TYPE>* response, grpc::CompletionQueue* new_call_cq, grpc::ServerCompletionQueue* notification_cq, void *tag) = 0;
    virtual void do_call(const REQUEST_TYPE* request, RESPONSE_TYPE *response) = 0;
    SERVICE_TYPE service;
};

template<class REQUEST_TYPE, class RESPONSE_TYPE, class SERVICE_TYPE>
class ServerImpl final {
public:
    using ASYNC_SERVER_TYPE = std::shared_ptr<AsyncServer<REQUEST_TYPE, RESPONSE_TYPE, SERVICE_TYPE>>;

    ServerImpl(const std::string &address, const ASYNC_SERVER_TYPE &async_server, size_t thread_num = 1): address_(address), service_(async_server), thread_num_(thread_num) {
    }
    ~ServerImpl() {}

  void Run() {
    std::string server_address(address_);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_->service);
    cq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();
    for(size_t i = 0; i < thread_num_; i++){
      threads_.emplace_back(&ServerImpl::HandleRpcs, this);
    }
  }

  void Shutdown() {
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    //关闭后就不会有请求进来了
    server_->Shutdown(deadline);
    //等2秒把正在处理的请求处理完，这里就不加同步机制了，避免影响服务性能
    sleep(2);
    //会使next返回false退出循环
    cq_->Shutdown();
  }

  void join(){
      for(auto &t : threads_){
        t.join();
      }
  }

 private:
  class CallData {
   public:
    CallData(ASYNC_SERVER_TYPE service, grpc::ServerCompletionQueue* cq)
        : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
      Proceed();
    }

    void Proceed() {
      if (status_ == CREATE) {
        status_ = PROCESS;
        service_->async_call(&ctx_, &request_, &responder_, cq_, cq_,
                                  this);
      } else if (status_ == PROCESS) {
        new CallData(service_, cq_);
        service_->do_call(&request_, &reply_);
        status_ = FINISH;
        responder_.Finish(reply_, grpc::Status::OK, this);
      } else {
        GPR_ASSERT(status_ == FINISH);
        delete this;
      }
    }

   private:
    ASYNC_SERVER_TYPE service_;

    REQUEST_TYPE request_;
    RESPONSE_TYPE reply_;
    grpc::ServerAsyncResponseWriter<RESPONSE_TYPE> responder_;
    grpc::ServerCompletionQueue* cq_;
    grpc::ServerContext ctx_;
    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status_;  // The current serving state.
  };

  void HandleRpcs() {
    new CallData(service_, cq_.get());
    void* tag;  // uniquely identifies a request.
    bool ok;

    while (cq_->Next(&tag, &ok)) {
      //client cancel 将导致ok为false
      if(!ok){
        delete static_cast<CallData*>(tag);
        continue;
      }
      static_cast<CallData*>(tag)->Proceed();
    }
  }

  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  std::unique_ptr<grpc::Server> server_;
  std::vector<std::thread> threads_;
  std::string address_;
  ASYNC_SERVER_TYPE service_;
  size_t thread_num_;
};

}; //namespace grpc_util