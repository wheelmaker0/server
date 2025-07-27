
#include <pthread.h>
#include <glog/logging.h>
#include <folly/system/ThreadName.h>

#include "thread_pool_manager.h"
#include "thread_util.h"

namespace thread_util {

class NamedThreadFactory : public folly::ThreadFactory {
 public:
  explicit NamedThreadFactory(const std::string &name) : name_(name) { }

  std::thread newThread(folly::Func &&func) override {
    return std::thread([func = std::move(func), this]() mutable {
      setThreadName(name_);
      func();
    });
  }

 private:
  std::string name_;
};

class NamedIdleThreadFactory : public folly::ThreadFactory {
 public:
    explicit NamedIdleThreadFactory(const std::string &name) : name_(name) { }

    std::thread newThread(folly::Func &&func) override {
        return std::thread([func = std::move(func), this]() mutable {
        setThreadName(name_);
        setThreadScheduleLowPriority();
        func();
        });
    }

 private:
    std::string name_;
};

ThreadPoolManager::ThreadPoolManager(){
    unsigned int num = std::thread::hardware_concurrency();
    core_num_ = num > 0 ? num : 16;
    cpu_thread_pool_ = build_thread_pool<NamedThreadFactory>(core_num_, core_num_, "cpu_pool");
    io_thread_pool_ = build_thread_pool<NamedThreadFactory>(core_num_ * 2, core_num_ * 6, "io_pool");
    redis_thread_pool_ = build_thread_pool<NamedThreadFactory>(core_num_ * 2, core_num_ * 6, "redis_pool");
}

std::shared_ptr<folly::FutureExecutor<folly::CPUThreadPoolExecutor>> ThreadPoolManager::create_thread_pool(const std::string &name, size_t min_size, size_t max_size){
    return std::shared_ptr<folly::FutureExecutor<folly::CPUThreadPoolExecutor>>(build_thread_pool<NamedThreadFactory>(min_size, max_size, name));
}

}; // namespace thread_util