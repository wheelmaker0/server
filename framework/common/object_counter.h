#pragma once
#include <iostream>
#include <vector>
#include <chrono>
#include <atomic>
#include <boost/core/demangle.hpp>
#include <glog/logging.h>

namespace common{

template <typename T>
class Counter{
public:
    static Counter<T> *get(){
        static Counter<T> instance;
        return &instance;
    }

    void dump(){
        static std::string name = boost::core::demangle(typeid(T).name());
        auto n = now<std::chrono::seconds>();
        if(ts_ + 5 < n){
            LOG(INFO) << "obj_counter " << name << " : " << count_;
            ts_ = n;
        }
    }

    std::atomic<int> count_;
    std::atomic<int64_t> ts_;

private:
    Counter(){
        count_ = 0;
        ts_ = now<std::chrono::seconds>();
    }

    template <typename TIME_TYPE>
    static int64_t now() {
        return std::chrono::duration_cast<TIME_TYPE>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

// #define OBJECT_COUNTER
#ifdef OBJECT_COUNTER

template <typename T>
class ObjectCounter{
public:
    ObjectCounter(){
        Counter<T>::get()->count_++;
        Counter<T>::get()->dump();
    }

    ObjectCounter(const ObjectCounter<T> &o){
        Counter<T>::get()->count_++;
        Counter<T>::get()->dump();
    }

    virtual ~ObjectCounter(){
        Counter<T>::get()->count_--;
    }
};

template <typename T>
class ObjectWrapperCounter : public T{
public:
    ObjectWrapperCounter(){
        Counter<T>::get()->count_++;
        Counter<T>::get()->dump();
    }

    virtual ~ObjectWrapperCounter(){
        Counter<T>::get()->count_--;
    }
};

#else

template <typename T>
class ObjectCounter{
public:
};

template <typename T>
class ObjectWrapperCounter : public T{
public:
};

#endif

}; //namespace common
