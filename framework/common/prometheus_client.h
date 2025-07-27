#pragma once
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <glog/logging.h>
#include "time_util.h"

namespace common {

class PrometheusObjHolder;

class PrometheusClient{
public:
  static PrometheusClient *get_instance();

  bool init(const std::map<std::string, std::string> &labels, const std::string &project, const std::string &ip, const std::string &port);

  bool gauge_inc(const std::map<std::string, std::string> &labels, double value);

  bool gauge_set(const std::map<std::string, std::string> &labels, double value);

  void push();

private:
  PrometheusClient();
  ~PrometheusClient();

  void loop();

  std::unique_ptr<PrometheusObjHolder> holder_;
};

class QpsRecorder{
public:
  class Recorder{
  public:
    friend class QpsRecorder;
    Recorder(): last_cnt_(0),cnt_(0), qps_(0){}
    void inc(){
        cnt_++;
    }

    float get(){
        return qps_;
    }

    void dump(){
      LOG(INFO) << this  << " qps: " << qps_ << ", cnt: " << cnt_ << ", last_cnt: " << last_cnt_;
    }

  private:
    int64_t last_cnt_;
    std::atomic<int64_t> cnt_;
    std::atomic<float> qps_;
  };

  static QpsRecorder *get_instance(){
    static QpsRecorder instance;
    return &instance;
  }

  ~QpsRecorder(){
    finished_ = true;
    thread_.join();
    for(auto &r : recorders_){
      delete r;
    }
  }

  void start(){
    last_ts_ = now_in_ms();
    finished_ = false;
    thread_ = std::thread(&QpsRecorder::loop, this);
  }

  Recorder *allocate_recorder(){
    //这里需要用指针，因为vecotr realloc之后返回的迭代器会失效
    recorders_.emplace_back(new Recorder());
    return recorders_.back();
  }

  void loop(){
    while(!finished_){
      auto ts = last_ts_;
      last_ts_ = now_in_ms();
      auto duration = last_ts_ - ts;
      if(duration > 0){
        for(auto &r : recorders_){
          auto cnt = r->last_cnt_;
          r->last_cnt_ = r->cnt_;
          r->qps_ = (r->last_cnt_ - cnt) * 1000.0f / duration;
        }
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

private:
  QpsRecorder(){}

  std::vector<Recorder *> recorders_;
  std::atomic<bool> finished_;
  int64_t last_ts_;
  std::thread thread_;
};


} // namespace common
