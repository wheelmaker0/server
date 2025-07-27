#pragma once
#include <vector>
#include <atomic>


namespace cache_util{

template<typename T>
class LockFreeStack{
public:
    LockFreeStack(size_t max_size):queue_(max_size){
        tail_ = 0;
    }

    bool push(const T &v){
        size_t index = tail_.load();
        while(index < queue_.size() && !tail_.compare_exchange_strong(index, index+1)){
            index = tail_.load();
        }
        if(index > queue_.size()){
            return false;
        }
        queue_[index] = v;
        return true;
    }

    bool pop(T &v){
        size_t index = tail_.load();
        while(index > 0 && !tail_.compare_exchange_strong(index, index-1)){
            index = tail_.load();
            v = queue_[index];
        }
        if(index == 0){
            return false;
        }
        return true;
    }

private:
    std::vector<T> queue_;
    std::atomic<size_t> tail_; 
};

};  //namespace cache_util
