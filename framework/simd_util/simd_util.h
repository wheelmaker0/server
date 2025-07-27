#pragma once
#include <vector>

namespace simd_util{

float fvec_inner_product(const float* x, const float* y, size_t d);

float fvec_norm_L2sqr(const float* x, size_t d);

void fvec_renorm_L2(size_t d, size_t nx, float* x);

}; //simd_util
