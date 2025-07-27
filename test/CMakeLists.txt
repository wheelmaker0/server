
file(GLOB srcs *.cpp)
include_directories(${CMAKE_SOURCE_DIR}/src/server)
add_executable(test ${srcs})
target_link_libraries(
  test
  -Wl,--eh-frame-hdr
  -Wl,--start-group
  -Wl,-Bstatic
  common
  zk_util
  thread_util
  hdfs_util
  simd_util
  kafka_util
  sql_util
  ${MODULE_STATIC_LIB}
  -Wl,-Bdynamic
  ${MODULE_SHARED_LIB}
  -Wl,--end-group
  ${MODULE_EXT_LDFLAGS})
