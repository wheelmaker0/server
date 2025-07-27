#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <prometheus/counter.h>
#include <prometheus/summary.h>
#include <prometheus/gauge.h>
#include <prometheus/gateway.h>
#include <prometheus/registry.h>
#include "prometheus_client.h"

using namespace prometheus;

namespace common {

class PrometheusObjHolder{
public:
  std::string project_;
  std::unique_ptr<Gateway> gateway_;
  std::shared_ptr<Registry> registry_;
  Family<Gauge> *family_gauage_;
  Family<Summary> *family_summary_;
  std::map<std::string, Gauge *> tag_to_gauge_;
  std::atomic<bool> finished;
  std::thread thread_;
};

PrometheusClient::PrometheusClient(){
    holder_ = std::unique_ptr<PrometheusObjHolder>(new PrometheusObjHolder());
}

PrometheusClient::~PrometheusClient(){
    holder_->finished = true;
    holder_->thread_.join();
}

PrometheusClient *PrometheusClient::get_instance(){
    static PrometheusClient client;
    return &client;
}

bool PrometheusClient::init(const std::map<std::string, std::string> &labels, const std::string &project, const std::string &ip, const std::string &port){
    holder_->project_ = project;
    holder_->gateway_ = std::unique_ptr<Gateway>(new Gateway(ip, port, project, labels));
    holder_->registry_ = std::make_shared<Registry>();
    holder_->family_gauage_ = &BuildGauge().Name(holder_->project_).Register(*(holder_->registry_));
    holder_->gateway_->RegisterCollectable(holder_->registry_);
    holder_->finished = false;
    holder_->thread_ = std::thread(&PrometheusClient::loop, this);
    return true;
}

bool PrometheusClient::gauge_inc(const std::map<std::string, std::string> &labels, double value){
    holder_->family_gauage_->Add(labels).Increment(value);
    return true;
}

bool PrometheusClient::gauge_set(const std::map<std::string, std::string> &labels, double value){
    holder_->family_gauage_->Add(labels).Set(value);
    return true;
}

void PrometheusClient::push(){
    holder_->gateway_->Push();
}

void PrometheusClient::loop(){
    while(!holder_->finished){
        holder_->gateway_->Push();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

} //namespace common