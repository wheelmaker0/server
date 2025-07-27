#pragma once
#include <memory>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <future>
#include "background_task.h"
#include "common/time_util.h"
#include "thread_util/thread_util.h"

namespace common {

enum DOUBLE_BUFFER_CODE{
    SUCCESS = 0,
    NO_NEED_UPDATE = 1,
    FAIL = 2
};

template<typename T>
class DoubleBuffer{
    class DoubleBufferTask : public BackgroundTask{
        public: 
            DoubleBufferTask(DoubleBuffer<T> *db): db_(db){}
            virtual bool run(){
                return db_->run();
            }
        private:
            DoubleBuffer<T> *db_;
    };

public:
    DoubleBuffer(size_t update_interval):update_interval_(update_interval) {
        ready_ = false;
        finished_ = true;
        current_index_ = 0;
        version_ = 0;
        for(size_t i = 0; i < 2; i++){
            buffer_[i] = std::make_shared<T>();
        }
    }

    virtual ~DoubleBuffer(){
        task_ = nullptr;
    }
    //不在构造函数启动后台线程，防止和子类构造函数产生竞争，将启动时机交给子类控制
    virtual void start_update(){
        thread_ = std::thread(&DoubleBuffer<T>::loop_start, this);
        thread_.detach();
        last_update_time_ = common::now_in_s();
    }

    virtual uint64_t version(){
        return version_;
    }

    bool ready(){
        return ready_;
    }

    std::shared_ptr<T> get_buffer(){
        return buffer_[current_index_];
    }

protected:
    virtual void watch(int64_t update_duration_in_s, int64_t fill_data_cost_in_ms, size_t buffer_in_use_count_, size_t fill_data_fail_count_){}
    virtual int fill_data(std::shared_ptr<T> &buffer) = 0;
    bool run(){
        int back_index_ = 1 - current_index_;
        if(buffer_[back_index_].use_count() > 1){
            buffer_in_use_count_++;
            return false;
        }
        int64_t start = now_in_ms();
        int rc = fill_data(buffer_[back_index_]);
        if(rc != SUCCESS){
            if(rc == FAIL){
                fill_data_fail_count_++;
            }
            return false;
        }
        current_index_ = back_index_;
        ready_ = true;
        int64_t end = now_in_ms();
        int64_t update_duration = end/1000 - last_update_time_;
        int64_t fill_data_cost = end - start;
        last_update_time_ = end/1000;
        version_++;
        watch(update_duration, fill_data_cost, buffer_in_use_count_, fill_data_fail_count_);
        buffer_in_use_count_ = 0;
        fill_data_fail_count_ = 0;
        return true;
    }

    void loop_start(){
        //首次初始化为了速度还是并发的
        while(!run()){
            sleep(1);
        }
        //to do 这里有问题，DoubleBufferTask持有了本对象，在本对象析构之前，weak_ptr有机会获取到该对象，并在析构后继续使用
        //之所以包装一层是为了控制task的生命周期，task必须先于本类销毁
        task_ = std::shared_ptr<BackgroundTask>(new DoubleBufferTask(this));
        //之后都由BackgroundTaskManager执行
        common::BackgroundTaskManager::get_instance()->regist(task_);
    }

    size_t update_interval_;
    std::shared_ptr<T> buffer_[2];
    std::thread thread_;
    std::atomic<int> current_index_;
    std::atomic<bool> finished_;
    std::atomic<bool> ready_;
    int64_t last_update_time_;
    std::atomic<uint64_t> version_;
    size_t buffer_in_use_count_ = 0;
    size_t fill_data_fail_count_ = 0;
    std::shared_ptr<BackgroundTask> task_;
};

} //namespace common