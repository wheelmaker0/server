#pragma once
#include <iostream>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <experimental/type_traits>
#include <tbb/concurrent_queue.h>
#include <boost/core/demangle.hpp>
#include <absl/meta/type_traits.h>
#include <boost/core/demangle.hpp>
#include <glog/logging.h>

namespace pipeline{

template <typename T>
using has_parallelism_impl = std::enable_if_t<std::is_same_v<size_t, decltype(T::parallelism())>>;

template <typename T>
struct has_parallelism : absl::type_traits_internal::is_detected<has_parallelism_impl, T> { };

template<typename T> 
struct function_traits;  

template<typename R, typename ...Args> 
struct function_traits<R (Args...)>{
    static const size_t nargs = sizeof...(Args);

    typedef R result_type;

    template <size_t i>
    struct arg{
        typedef typename std::remove_reference<typename std::tuple_element<i, std::tuple<Args...>>::type>::type type;
    };
};

template<typename ...TYPES>
class SerialPipeline{
public:
    void start(size_t paralleism = 1){
        finished_ = false;
        for(size_t i = 0; i < paralleism; i++){
            threads_.emplace_back([this](){
                while(!finished_){
                    do_source_work<TYPES...>();
                }
            });
        }
    }

    void stop(){
        finished_ = true;
    }

    void join(){
        for(auto &t : threads_){
            t.join();
        }
    }

protected:
    template<typename F, typename F2, typename ...Fs, 
             typename T= typename function_traits<decltype(F::do_work)>::template arg<0>::type>
    bool do_source_work(){
        T soure_out;
        F::do_work(soure_out);
        return do_step_work<F2, Fs...>((F2 *)nullptr, soure_out);
    }

    template<typename F, typename F2, typename...Fs,
            typename IT= typename function_traits<decltype(F::do_work)>::template arg<0>::type,
            typename OT= typename function_traits<decltype(F::do_work)>::template arg<1>::type >
    bool do_step_work(const IT &input){
        OT output;
        F::do_work(input, output);
        return do_step_work<F2, Fs...>((F2 *)nullptr, output);
    }

    template<typename F, 
            typename T= typename function_traits<decltype(F::do_work)>::template arg<0>::type >
    bool do_step_work(T &input){
        return F::do_work(input);
    }

    std::vector<std::thread> threads_;
    std::atomic<bool> finished_;
};

template<typename ...TYPES>
class ParallelPipeline{
public:
    void start(){
        finished_ = false;
        init_source_work<TYPES...>();
    }

    void stop(){
        finished_ = true;
    }

    void join(){
        for(auto &t : threads_){
            t.join();
        }
    }

protected:
    template<typename F, typename F2, typename ...Fs, 
             typename T= typename function_traits<decltype(F::do_work)>::template arg<0>::type >
    bool init_source_work(){
        std::shared_ptr<tbb::concurrent_bounded_queue<T>> source_queue = std::make_shared<tbb::concurrent_bounded_queue<T>>();
        source_queue->set_capacity(queue_size_);

        size_t parallelism = 1;
        if constexpr (has_parallelism<F>::value){
            parallelism = F::parallelism();
        }
        LOG(INFO) << "init " << boost::core::demangle(typeid(F).name()) << " parallelism: " << parallelism;
        for(size_t i = 0; i < parallelism; i++){
            threads_.emplace_back([source_queue, this](){
                while(!finished_){
                    T soure_out;
                    F::do_work(soure_out);
                    source_queue->push(soure_out);
                }
                //结束时候放一个空对象，防止下一级loop一直阻塞
                T soure_out;
                source_queue->push(soure_out);
            });
        }
        return init_step_work<F2, Fs...>((F2 *)nullptr, source_queue);
    }

    template<typename F, typename F2, typename...Fs, 
             typename IT= typename function_traits<decltype(F::do_work)>::template arg<0>::type,
             typename OT= typename function_traits<decltype(F::do_work)>::template arg<1>::type>

    bool init_step_work(F *, std::shared_ptr<tbb::concurrent_bounded_queue<IT>> &input_queue){
        std::shared_ptr<tbb::concurrent_bounded_queue<OT>> output_queue = std::make_shared<tbb::concurrent_bounded_queue<OT>>();
        output_queue->set_capacity(queue_size_);
        
        size_t parallelism = 1;
        if constexpr (has_parallelism<F>::value){
            parallelism = F::parallelism();
        }
        LOG(INFO) << "init " << boost::core::demangle(typeid(F).name()) << " parallelism: " << parallelism;

        for(size_t i = 0; i < parallelism; i++){
            threads_.emplace_back([input_queue, output_queue, this](){
                while(!finished_){
                    IT input;
                    input_queue->pop(input);
                    OT output;
                    F::do_work(input, output);
                    output_queue->push(output);
                }
                //结束时候放一个空对象，防止下一级loop一直阻塞
                OT output;
                output_queue->push(output);
            });
        }
        return init_step_work<F2, Fs...>((F2 *)nullptr, output_queue);
    }

    template<typename F, 
             typename T = typename function_traits<decltype(F::do_work)>::template arg<0>::type >
    bool init_step_work(F *, std::shared_ptr<tbb::concurrent_bounded_queue<T>> &input_queue){

        size_t parallelism = 1;
        if constexpr (has_parallelism<T>::value){
            parallelism = F::parallelism();
        }
        LOG(INFO) << "init " << boost::core::demangle(typeid(F).name()) << " parallelism: " << parallelism;

        for(size_t i = 0; i < parallelism; i++){
            threads_.emplace_back([input_queue, this](){
                while(!finished_){
                    T input;
                    input_queue->pop(input);
                    F::do_work(input);
                }
            });
        }
        return true;
    }

    size_t queue_size_ = 64;
    std::vector<std::thread> threads_;
    std::atomic<bool> finished_;
};

};  //namepsace