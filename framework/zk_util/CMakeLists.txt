
file(GLOB srcs *.cpp)
add_library(zk_util ${srcs})
target_compile_definitions(zk_util PUBLIC -DTHREADED)
