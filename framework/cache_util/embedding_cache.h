#pragma once
#include "memory_pool.h"
#include "common/time_util.h"
#include "concurrent_hash_map.h"
#include "concurrent_queue.h"
#include <thread>
#include <utility>

namespace cache_util{

class EmbeddingCache{

    typedef tbb::concurrent_hash_map<int64_t, float *> EBD_CACHE_MAP_TYPE;
    typedef tbb::concurrent_queue<std::pair<int64_t, float *>> INSERT_QUEUE_TYPE;
    typedef tbb::concurrent_queue<int64_t> DELETE_QUEUE_TYPE;

public:
    struct CacheItem{
        int64_t key;
        int64_t ts;
        float *value;
    };

    EmbeddingCache(size_t cache_size_exp, size_t conflict_tolerance, int64_t expire_time, size_t embedding_size):conflict_tolerance_(conflict_tolerance),expire_time_(expire_time),
        embedding_size_(embedding_size),cache_size_(1 << cache_size_exp),memory_pool_(embedding_size_*sizeof(float),cache_size_){
        key_hash_mask_ = cache_size_ - 1;
        array_.resize(cache_size_ + conflict_tolerance_);
        finished_ = false;
        thread_ = std::thread(&EmbeddingCache::loop, this);
    }

    ~EmbeddingCache(){
        finished_ = true;
        thread_.join();
    }

    bool get(int64_t key, float * value){
        int64_t h = (key & key_hash_mask_);
        for(size_t i = 0; i < conflict_tolerance_; i++){
            auto index = (h + i);
            if(array_[h+i].key == key){
                if(common::now_in_s() - array_[h+i].ts > expire_time_){
                    erase(h+i);
                    return false;
                }
                else{
                    memcpy(value, array_[h+i].value, embedding_size_ * sizeof(float));
                    return true;
                }
            }
        }
        EBD_CACHE_MAP_TYPE::const_accessor key_accessor;
        if(fullback_.find(key_accessor, key)) {
            memcpy(value, key_accessor->second, embedding_size_ * sizeof(float));
            return true;
        }
        return false;
    }

    bool insert(int64_t key, float *value){
        auto dst = memory_pool_.alloc();
        if(dst == nullptr){
            return false;
        }
        memcpy(dst, value, embedding_size_ * sizeof(float));
        insert_queue_.push(std::make_pair(key, dst));
        return true;
    }

    bool erase(int64_t index){
        delete_queue_.push(index);
        return true;
    }

protected:

    size_t find_index(int64_t key){
        int64_t h = (key & key_hash_mask_);
        for(size_t i = 0; i < conflict_tolerance_; i++){
            if(array_[h+i].key == 0){
                return h+i;
            }
        }
        return array_.size();
    }

    // bool erase_by_key(int64_t key){
    //     auto index = find_index(key);
    //     if(index == array_.size()){
    //         EBD_CACHE_MAP_TYPE::const_accessor key_accessor;
    //         if(fullback_.find(key_accessor, key)) {
    //             auto t = key_accessor->second;
    //             memory_pool_.erase(key_accessor);
    //             memory_pool_.free(t);
    //             return true;
    //         }
    //         return false;
    //     }
    //     return erase_by_index(index);
    // }

    bool erase_by_index(size_t index){
        array_[index].key = 0;
        memory_pool_.free(array_[index].value);
        return true;
    }

    bool insert_impl(int64_t key, float * value){
        int64_t h = (key & key_hash_mask_);
        for(size_t i = 0; i < conflict_tolerance_; i++){
            auto index = (h + i);
            //因为是单线程写，多线程读，当写线程发现key == 0的时候，key不可能变成别的值，也不会有人读它，所以可以安全写入
            if(array_[h+i].key == 0){
                //先写value，再写key，防止get时候发现key != 0，但是value还没有写好
                array_[h+i].value = value;
                array_[h+i].ts = common::now_in_s();
                array_[h+i].key = key;
                return true;
            }

            //发现了过期的key，直接往里面写，防止临界时间点刚好有读者在读，过期时间给延长2s
            if(common::now_in_s() - array_[h+i].ts > expire_time_ + 2){
                erase_by_index(h+i);
                array_[h+i].value = value;
                array_[h+i].ts = common::now_in_s();
                array_[h+i].key = key;
                return true;
            }
        }

        EBD_CACHE_MAP_TYPE::accessor key_accessor;
        if(fullback_.find(key_accessor, key)) {
            return false;
        }
        // EBD_CACHE_MAP_TYPE::value_type kv(key, value);
        fullback_.emplace(key_accessor, key, value);
        return true;
    }

    void loop(){
        while(!finished_){
            int64_t index;
            while(delete_queue_.try_pop(index)){
                erase_by_index(index);
            }
            std::pair<int64_t, float *> insert_data;
            while(insert_queue_.try_pop(insert_data)){
                insert_impl(insert_data.first, insert_data.second);
            }
        }
    }

private:
    size_t key_hash_mask_;
    size_t conflict_tolerance_;
    size_t expire_time_;
    size_t embedding_size_;
    size_t cache_size_;
    FixSizeMemoryPool<float> memory_pool_;
    std::thread thread_;
    std::atomic<bool> finished_;
    std::vector<CacheItem> array_;
    EBD_CACHE_MAP_TYPE fullback_;
    INSERT_QUEUE_TYPE insert_queue_;
    DELETE_QUEUE_TYPE delete_queue_;
};

}; // namespace cache_util