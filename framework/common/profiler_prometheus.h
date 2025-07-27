#pragma once

#include "time_util.h"
#include "prometheus_client.h"

namespace common{

template<class T>
class PrometheusProfiler{
public:
    PrometheusProfiler(){
        reset();
    }

    void reset(){
        watch_.reset();
    }

    //距离上一次调用lap的时间
    int64_t lap(const std::map<std::string, std::string> &labels) {
        auto ret = watch_.lap();
        common::PrometheusClient::get_instance()->gauge_set(labels, ret);
        return ret;
    }

    //距离开始时的时间
    int64_t total(const std::map<std::string, std::string> &labels) {
        auto ret = watch_.total();
        common::PrometheusClient::get_instance()->gauge_set(labels, ret);
        return ret;
    }

private:
    StopWatch<T> watch_;
};

template<class T>
class PrometheusScopedProfiler{
public:
    PrometheusScopedProfiler(const std::map<std::string, std::string> &labels):labels_(labels) {}
    ~PrometheusScopedProfiler(){
        common::PrometheusClient::get_instance()->gauge_set(labels_, watch_.total());
    }

private:
    StopWatch<T> watch_;
    std::map<std::string, std::string> labels_;
};

using ProfilerUs=PrometheusProfiler<std::chrono::microseconds>;
using ScopedProfilerUs=PrometheusScopedProfiler<std::chrono::microseconds>;

}; //namespace common