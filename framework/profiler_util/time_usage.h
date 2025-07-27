
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <future>
#include <chrono>
#include <glog/logging.h>

namespace profiler_util{

class TimeUsage{
public:
    TimeUsage(const std::string &name, int64_t duration_is_s = 100):name_(name){
        block_index_ = 0;
        block_num_ = 10;
        block_duration_in_us_ =  duration_is_s * 1000 * 1000 / block_num_;
        work_duration_block_.resize(block_num_, 0);
        block_start_ts_.resize(block_num_, 0);
        block_start_ts_[block_index_] = now_in_us();
    }

    void start(){
        work_start_ = now_in_us();
    }

    void end(){
        auto now = now_in_us();
        auto work_time = now - work_start_;
        if(block_start_ts_[block_index_] + block_duration_in_us_ < now){
            block_index_ = (block_index_ + 1) % block_num_;
            output();
            block_start_ts_[++block_index_] = now;
            work_duration_block_[block_index_] = 0;
        }
        work_duration_block_[block_index_] += work_time;
    }

private:
    int64_t now_in_us() {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void output(){
        double sum = 0.0;
        size_t num = 0;
        for(size_t i = 0; i < block_num_; i++){
            if(block_start_ts_[i] != 0){
                sum += work_duration_block_[i];
                num++;
            }
            
        }
        LOG(INFO) << name_ << ", usage: " << sum / num / block_duration_in_us_ << std::endl;
    }
    std::string name_;
    int64_t work_start_;
    size_t block_num_;
    int64_t block_duration_in_us_;
    std::vector<int64_t> work_duration_block_;
    std::vector<int64_t> block_start_ts_;
    int64_t block_index_;
};

} //namespace profiler_util