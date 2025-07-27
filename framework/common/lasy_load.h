#pragma once
#include <mutex>

namespace common{

template<class T>
class LasyLoad{
public:
    void add_creator(const std::function<T()> &func){
        init_func_ = func;
    }
    const T *get(){
        if(is_init_){
            return &data_;
        }
        std::unique_lock lock(m_);
        if(is_init_){
            return &data_;
        }
        if(init_func_ == nullptr){
            return nullptr;
        }
        data_ = init_func_();
        is_init_ = true;
        return &data_;
    }
private:
    std::function<T()> init_func_;
    T data_;
    std::mutex m_;
    volatile bool is_init_ = false;
};

} //common