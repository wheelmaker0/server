#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <unordered_set>
#include <tbb/concurrent_queue.h>
#include <glog/logging.h>

namespace common {

class BackgroundTask{
public:
    virtual bool run() = 0;
};

class BackgroundTaskManager{
public:
    static BackgroundTaskManager *get_instance(){
        static BackgroundTaskManager instance;
        return &instance;
    }

    void regist(const std::weak_ptr<BackgroundTask> &task){
        regist_queue_.push(task);
    }

private:
    BackgroundTaskManager(){
        finished_ = false;
        thread_ = std::thread(&BackgroundTaskManager::loop, this);
    }
    ~BackgroundTaskManager(){
        finished_ = true;
        thread_.join();
    }

    void loop();
    std::thread thread_;
    std::atomic<bool> finished_;
    tbb::concurrent_queue<std::weak_ptr<BackgroundTask>> regist_queue_;
    std::vector<std::weak_ptr<BackgroundTask>> task_queue_;
};

}; //