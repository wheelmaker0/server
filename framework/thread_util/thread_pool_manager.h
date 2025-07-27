#pragma once

#include "folly/executors/CPUThreadPoolExecutor.h"
#include "folly/executors/FutureExecutor.h"

namespace thread_util {

class ThreadPoolManager{
public:
    static ThreadPoolManager *get_instance(){
        static ThreadPoolManager instance;
        return &instance;
    }

    ~ThreadPoolManager(){
        delete cpu_thread_pool_;
        delete io_thread_pool_;
        delete redis_thread_pool_;
        cpu_thread_pool_ = nullptr;
        io_thread_pool_ = nullptr;
        redis_thread_pool_ = nullptr;
    }

    uint32_t get_core_num(){
        return core_num_;
    }

    folly::FutureExecutor<folly::CPUThreadPoolExecutor> *get_cpu_thread_pool(){
        return cpu_thread_pool_;
    }

    folly::FutureExecutor<folly::CPUThreadPoolExecutor> *get_io_thread_pool(){
        return io_thread_pool_;
    }

    folly::FutureExecutor<folly::CPUThreadPoolExecutor> *get_redis_thread_pool(){
        return redis_thread_pool_;
    }

    std::shared_ptr<folly::FutureExecutor<folly::CPUThreadPoolExecutor>> create_thread_pool(const std::string &name,  size_t min_size = 16, size_t max_size = 16);
private:
    template <typename factory_type>
    static folly::FutureExecutor<folly::CPUThreadPoolExecutor> *build_thread_pool(size_t min, size_t max, const std::string &name) {
        auto thread_factory = std::make_shared<factory_type>(name);
        auto range          = std::make_pair(max, min);
        return new folly::FutureExecutor<folly::CPUThreadPoolExecutor>(range, thread_factory);
    }

    ThreadPoolManager();
    uint32_t core_num_ = 16;
    folly::FutureExecutor<folly::CPUThreadPoolExecutor> * cpu_thread_pool_;
    folly::FutureExecutor<folly::CPUThreadPoolExecutor> * io_thread_pool_;
    folly::FutureExecutor<folly::CPUThreadPoolExecutor> * redis_thread_pool_;
};

}; // namespace thread_util