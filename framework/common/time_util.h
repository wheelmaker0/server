#pragma once
#include <chrono>
#include <string>
#include <time.h>

namespace common{

int64_t now_in_s();

int64_t now_in_ms();

int64_t now_in_us();

std::string date_format(time_t ts_ms, const char *format = "%d%02d%02d");

std::string date_time_format(time_t ts_ms, const char *format = "%d%02d%02d %02d:%02d:%02d");

std::string date_format_from_now(time_t time_offset_ms = 0, const char *format = "%d%02d%02d");

std::string date_time_format_from_now(time_t time_offset_ms = 0, const char *format = "%d%02d%02d %02d:%02d:%02d");

template<class T>
class StopWatch {
  public:
    StopWatch(){
        reset();
    }

    void reset(){
        start_ = now();
        last_ = start_;
    }

    int64_t lap(){
        auto n = now();
        auto r = n - last_;
        last_ = n;
        return r;
    }

    int64_t total(){
        return now() - start_;
    }

    static int64_t now() {
        return std::chrono::duration_cast<T>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
private:
    int64_t start_;
    int64_t last_;
};

using StopWatchNs=StopWatch<std::chrono::nanoseconds>;
using StopWatchUs=StopWatch<std::chrono::microseconds>;
using StopWatchMs=StopWatch<std::chrono::milliseconds>;
using StopWatchS=StopWatch<std::chrono::seconds>;

};