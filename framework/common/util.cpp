#include "util.h"

namespace common{
    
float variance(const float *a, const float *b, size_t n){
    if(n==0){
        return 0;
    }
    float sum = 0;
    for(size_t i = 0; i < n; i++){
        sum += (a[i]-b[i])*(a[i]-b[i]);
    }
    return sum/n;
}

bool equal(const std::vector<float> &a, const std::vector<float> &b){
    for(size_t i = 0; i < a.size() && i < b.size(); i++){
        if(std::fabs(a[i] - b[i]) > 0.0001){
            return false;
        }
    }
    return true;
    // float s = variance(a.data(), b.data(), std::min(a.size(), b.size()));
    // return s <= 0.0001;
}

};