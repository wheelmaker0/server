#include "batch_helper.h"

namespace common{
namespace batch_helper{
    int reduce_by_sum(std::vector<folly::Future<int>> &future_results){
        int success_cnt = 0;
        for (const auto &ret : folly::collectAll(future_results).get()) {
            if(*ret > 0){
                success_cnt += *ret;
            }
        }
        return success_cnt;
    }

};//batch_helper
};//common

