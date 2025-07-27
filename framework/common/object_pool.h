#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include "concurrent_queue.h"

namespace common{

template<class T>
class ObjectPool{
public:

    ~ObjectPool(){
        destory();
    }

    bool init(size_t pool_size, std::function<T*()> creator, std::function<void(T*)> deleter = nullptr){
        deleter_ = deleter;
        queue_.set_capacity(pool_size);
        for(size_t i = 0; i < pool_size; i++){
            auto ptr = creator();
            if(!ptr){
                return false;
            }
            queue_.push(ptr);
        }
        return true;
    }

    void destory(){
        T *ptr;
        while(!queue_.empty()){
            queue_.pop(ptr);
            delete ptr;
        }
    }

    std::shared_ptr<T> borrow(){
        T *ptr;
        queue_.pop(ptr);
        auto ret = std::shared_ptr<T>(ptr, [this](auto p){
                queue_.push(p);
            });
        return ret;
    }

    std::shared_ptr<T> try_borrow(){
        T *ptr;
        if(!queue_.try_pop(ptr)){
            return nullptr;
        }
        auto ret = std::shared_ptr<T>(ptr, [this](auto p){
                if(deleter_){
                    deleter_(p);
                }
                queue_.push(p);
            });
        return ret;
    }
    
private:
    tbb::concurrent_bounded_queue<T *> queue_;
    std::function<void(T*)> deleter_;
};

}; // namespace common