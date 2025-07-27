#pragma once
#include "test.h"
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/util.h>

class Timer{
public:
    class TimerCallback{
    public:
        virtual void run() = 0;
    };

    bool init(){
        eb_ = event_base_new();
        if (!eb_) {
            LOG(INFO) << "event_base_new failed";
            return false;
        }
        return true;
    }

    bool sched(std::shared_ptr<TimerCallback> &callback, int64_t time_us){

        struct event* timer_event = event_new(eb_, -1, 0, Timer::timer_callback, eb_);
        if (!timer_event) {
            LOG(ERROR) << "event_new failed";
            return false;
        }

        struct timeval tv;
        tv.tv_sec = 3 / 1000000;
        tv.tv_usec = time_us % 1000000;
        
        timer_event

    }


    static void timer_callback(evutil_socket_t fd, short events, void* arg) {



    }


    void destory(){
        if(eb_){
            delete eb_;
        }
    } 

private:    
    struct event_base* eb_ = nullptr;
};








class LibeventTest : public Test{
protected:
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    std::string table;
public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("host,a", po::value<std::string>(&host)->default_value(""), "host")
        ("user,u", po::value<std::string>(&user)->default_value(""), "user")
        ("password,p", po::value<std::string>(&password)->default_value(""), "password")
        ("database,d", po::value<std::string>(&database)->default_value(""), "database")
        ("table,t", po::value<std::string>(&table)->default_value(""), "table");
    }

    void timer_callback(evutil_socket_t fd, short events, void* arg) {
        std::cout << "一次性定时器触发！" << std::endl;
        
        // 获取event_base以便退出事件循环
        struct event_base* base = static_cast<struct event_base*>(arg);
        event_base_loopexit(base, NULL);
    }


    virtual void run(){
        
        struct event_base* base = event_base_new();
        if (!base) {
            std::cerr << "创建event_base失败" << std::endl;
            return ;
        }
        
        // 创建一次性定时器事件 (注意：不使用EV_PERSIST标志)
        struct event* timer_event = event_new(base, -1, 0, timer_callback, base);
        if (!timer_event) {
            std::cerr << "创建定时器事件失败" << std::endl;
            event_base_free(base);
            return ;
        }
        
        // 设置定时器参数 (3秒后触发)
        struct timeval tv;
        tv.tv_sec = 3;   // 3秒
        tv.tv_usec = 0;  // 0微秒
        
        // 添加定时器事件
        if (event_add(timer_event, &tv) < 0) {
            std::cerr << "添加定时器事件失败" << std::endl;
            event_free(timer_event);
            event_base_free(base);
            return ;
        }
        
        std::cout << "定时器已启动，将在3秒后触发..." << std::endl;
        
        // 运行事件循环
        event_base_dispatch(base);
        
        // 清理资源
        event_free(timer_event);
        event_base_free(base);
        
        std::cout << "程序正常退出" << std::endl;


    }

};