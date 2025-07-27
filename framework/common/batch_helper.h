#pragma once

#include "folly/executors/CPUThreadPoolExecutor.h"
#include "folly/executors/FutureExecutor.h"


namespace common{
namespace batch_helper{

    int reduce_by_sum(std::vector<folly::Future<int>> &future_results);
    
    template<typename INPUT_TYPE, typename OUTPUT_TYPE, typename FUNC>
    std::vector<folly::Future<int>> async_run(INPUT_TYPE *input,  OUTPUT_TYPE *output, size_t n, size_t batch_size, folly::FutureExecutor<folly::CPUThreadPoolExecutor> *pool, FUNC func, size_t input_step = 1, size_t output_step = 1){
        assert(n != 0);
        assert(pool);
        auto batch_num = (n + batch_size - 1) / batch_size;
        std::vector<folly::Future<int>> future_results;
        future_results.reserve(batch_num);
        for(size_t i = 0; i < batch_num; i++){
            size_t begin_index = i * batch_size;
            size_t size = std::min(batch_size, (n - begin_index));
            future_results.push_back(pool->addFuture(
                [src = input + begin_index * input_step, dst = output + begin_index * output_step, size, f = std::move(func)] () {
                    return f(src , dst , size);
                }));
        }
        return future_results;
    }

    template<typename INPUT_TYPE, typename OUTPUT_TYPE, typename FUNC>
    int run(INPUT_TYPE *input,  OUTPUT_TYPE *output, size_t n, size_t batch_size, folly::FutureExecutor<folly::CPUThreadPoolExecutor> *pool, const FUNC &func, size_t input_step = 1, size_t output_step = 1){
        auto futures = async_run(input, output, n, batch_size, pool, func, input_step, output_step);
        return reduce_by_sum(futures);
    }

};

};
