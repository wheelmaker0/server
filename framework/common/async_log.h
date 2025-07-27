#pragma once
#include <memory>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <future>
#include <tbb/concurrent_queue.h>
#include <glog/logging.h>

namespace common{

class AsyncLogger{
public:

    AsyncLogger(){
        finished_ = false;
        thread_ = std::thread(&AsyncLogger::loop, this);
    }

    ~AsyncLogger(){
        finished_ = true;
        queue_.push("shutting down");
        thread_.join();
    }

    void loop(){
        std::string s;
        while(!finished_){
            queue_.pop(s);
            LOG(INFO) << s;
        }
    }

    bool log(const std::string &s){
        return queue_.try_push(s);
    }

private:
    std::thread thread_;
    std::atomic<bool> finished_;
    tbb::concurrent_bounded_queue<std::string> queue_;
};


};