#pragma once
#include "lock_free_stack.h"

namespace cache_util{

template<typename T>
class FixSizeMemoryPool{
public:
    FixSizeMemoryPool(size_t block_size, size_t pool_size):block_size_(block_size), pool_size_(pool_size),
     pool_(block_size_*pool_size_),free_ptr_(pool_size_) {
        for(size_t i = 0; i < pool_.size(); i += block_size){
            free_ptr_.push(pool_.data() + i);
        }
    }

    float *alloc(){
        float *ret;
        if(!free_ptr_.pop(ret)){
            return nullptr;
        }
        return ret;
    }

    void free(float *ptr){
        free_ptr_.push(ptr);
    }

private:
    size_t block_size_;
    size_t pool_size_;
    std::vector<T> pool_;
    LockFreeStack<T *> free_ptr_;
};

}; //cache_util