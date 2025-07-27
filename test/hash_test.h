#pragma once

#include "test.h"

class HashCollisionTest : public Test{
private:
    std::string ids_path;
    bool need_detail;
public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("path,p", po::value<std::string>(&ids_path)->required(), "id file path")
        ("need_detail,d", po::value<bool>(&need_detail)->default_value(false), "need detail info");
    }
    virtual void run() override{
        test_id_file(ids_path, need_detail);
    }
protected:
    void test_id_file(const std::string &ids_path, bool need_detail){
        std::vector<std::string> ids;
        common::StopWatchMs s;
        if(!read_id(ids_path, ids)){
            return ;
        }
        std::cerr << "read_id time cost: " << s.lap() << std::endl;
        std::cerr << "ids size: " << ids.size() << std::endl;
        int dup = 0;
        if(need_detail){
            tbb::concurrent_hash_map<uint64_t, std::vector<std::string>> counter;
            dup = count_detail(ids, counter);
            for(auto &item : counter){
                if(item.second.size() > 1){
                    std::cout << item.first << "(" << item.second.size() << "):" << common::container_2_str(item.second) << std::endl;
                }
            }
        }
        else{
            dup = count_dup(ids);
        }
        std::cerr << "count_id time cost: " << s.lap() << std::endl;
        std::cerr << "dup: " << dup << std::endl;
    }

    static int64_t make_fid(uint64_t slot, const std::string &value){
        uint64_t hashCode = std::hash<std::string>()(value);
        if (slot >= 500) {
            return (slot << 44 | ((hashCode << 20) >> 20));
        }
        return (slot << 54 | ((hashCode << 10) >> 10));
    }

    uint64_t make_mid(uint64_t ts, uint64_t suffix){
        return ((ts - 515483463) << 22) + (suffix & 0x3FFFFF);
    }

    bool read_id(const std::string &path, std::vector<std::string> &ids){
        std::ifstream ifs(path);
        if(!ifs){
            std::cerr << "open file failed: " << path;
            return false;
        }
        std::string id;
        while(ifs >> id){
            ids.emplace_back(id);
        }
        return true;
    }

    size_t count_dup(const std::vector<std::string> &ids){
        tbb::concurrent_hash_map<uint64_t, int> counter;
        size_t ret = 0;
        tbb::parallel_for((size_t)0, ids.size(), [&](size_t i){
            auto &id = ids[i];
            auto h = make_fid(49, id);
            // std::cout << id << "," << h << std::endl;
            tbb::concurrent_hash_map<uint64_t, int>::accessor key_accessor;
            if(counter.find(key_accessor, h)) {
                key_accessor->second++;
                ret++;
            }
            else{
                counter.emplace(key_accessor, h, 1);
            }
        });
        return ret;
    }

    size_t count_detail(const std::vector<std::string> &ids, tbb::concurrent_hash_map<uint64_t, std::vector<std::string>> &counter){
        size_t ret = 0;
        for(auto &id : ids){
            auto h = make_fid(49, id);
            // std::cout << id << "," << h << std::endl;
            tbb::concurrent_hash_map<uint64_t, std::vector<std::string>>::accessor key_accessor;
            if(counter.find(key_accessor, h)) {
                key_accessor->second.emplace_back(id);
                ret++;
            }
            else{
                std::vector<std::string> t(1, id);
                counter.emplace(key_accessor, h, t);
            }
        }
        return ret;
    }
};

class RandMidHashCollisionTest : public HashCollisionTest{
private:
    size_t num_total;
    size_t num_per_second;
    bool need_detail;
public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("num_total,t", po::value<size_t>(&num_total)->required(), "num_total")
        ("num_per_second,s", po::value<size_t>(&num_per_second)->required(), "num_per_second")
        ("need_detail,d", po::value<bool>(&need_detail)->default_value(false), "need detail info");
    }
    virtual void run() override{
        test_rand_mid(num_total, num_per_second, need_detail);
    }
protected:
    void test_rand_mid(size_t num_total, size_t num_per_second, bool need_detail){
        std::vector<std::string> ids;
        common::StopWatchMs s;
        if(!gen_mid(num_total, num_per_second, ids)){
            return ;
        }
        std::cerr << "gen_mid time cost: " << s.lap() << std::endl;
        std::cerr << "ids size: " << ids.size() << std::endl;
        int dup = 0;
        if(need_detail){
            tbb::concurrent_hash_map<uint64_t, std::vector<std::string>> counter;
            dup = count_detail(ids, counter);
            for(auto &item : counter){
                if(item.second.size() > 1){
                    std::cout << item.first << "(" << item.second.size() << "):" << common::container_2_str(item.second) << std::endl;
                }
            }
        }
        else{
            dup = count_dup(ids);
        }
        std::cerr << "count_id time cost: " << s.lap() << std::endl;
        std::cerr << "dup: " << dup << std::endl;
    }

    bool gen_mid(size_t num_total, size_t num_per_secend, std::vector<std::string> &ids){
        uint64_t end_ts = 1712573766; //2024-04-08 18:56:06
        uint64_t start_ts = 1712573766 - (num_total / num_per_secend);
        ids.resize((end_ts-start_ts)*num_per_secend);
        tbb::parallel_for((size_t)start_ts, end_ts, [&](size_t ts){
            auto i = ts - start_ts;
            for(size_t j = 0; j < num_per_secend; j++){
                ids[i*num_per_secend + j] = std::to_string(make_mid(ts, j * 1000));
            }
        });
        return true;
    }
};
