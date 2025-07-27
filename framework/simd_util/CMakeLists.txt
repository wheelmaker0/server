
file(GLOB srcs *.cpp)
add_library(simd_util ${srcs})
target_compile_options(simd_util PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-mavx2 -mfma -mf16c -mpopcnt>)