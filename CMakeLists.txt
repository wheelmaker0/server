cmake_minimum_required(VERSION 3.15.4)

project(TR)

# 检查GCC版本
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8.0.0")
  message(FATAL_ERROR "gcc版本过低，执行'rm -rf build.tmp && scl enable devtoolset-8 bash'")
endif()

# 默认不显示编译命令，需要调查编译出错时，打开以下开关 set(CMAKE_VERBOSE_MAKEFILE true)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake-module)
include(GetAllTargets)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

set(CMAKE_SKIP_BUILD_RPATH true)

# 获取git相关信息，TAG/分支名称，COMMIT信息
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
if("${GIT_REFSPEC}" STREQUAL "")
  git_describe(GIT_REFSPEC)
endif()
if(${GIT_REFSPEC} MATCHES ".*(v[0-9]+\\.[0-9]+\\.[0-9]+).*")
  string(REGEX REPLACE ".*(v[0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" GIT_BRANCH ${GIT_REFSPEC})
else()
  string(REGEX REPLACE ".*/(.*)" "\\1" GIT_BRANCH ${GIT_REFSPEC})
endif()

if(DEFINED ENV{BUILD_TAG})
  set(GIT_BRANCH $ENV{BUILD_TAG})
endif()
configure_file(${CMAKE_SOURCE_DIR}/framework/miscs/config.h.in ${CMAKE_SOURCE_DIR}/framework/miscs/config.h @ONLY)

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)
add_compile_options(-O2 -fno-omit-frame-pointer -Wextra -Wno-sign-compare -ftls-model=local-exec)
add_compile_options(-DSUPPORT_BREEZE) #use motan breeze

execute_process(COMMAND python3 -c "from __future__ import print_function;import tensorflow as tf; print(tf.sysconfig.get_include(),end='')" OUTPUT_VARIABLE TF_INCLUDE)
execute_process(COMMAND python3 -c "from __future__ import print_function;import tensorflow as tf; print(tf.sysconfig.get_lib(),end='')" OUTPUT_VARIABLE TF_LIB_DIR)
set(TF_LIB "-Wl,--no-as-needed ${TF_LIB_DIR}/libtensorflow_framework.so.1 ${TF_LIB_DIR}/python/_pywrap_tensorflow_internal.so /home/pai/lib/libpython3.6m.so -Wl,--as-needed")

# option(TCMALLOC_DEBUG "use tcmalloc instead of default jemalloc, in order to profile performance" OFF)
# if(TCMALLOC_DEBUG)
#   find_package(libtcmalloc REQUIRED)
#   # this options is required by brpc in cpu profiling
#   add_compile_options(-DBRPC_ENABLE_CPU_PROFILER)
# else()
#   find_package(jemalloc REQUIRED)
# endif()
# if(ENABLE_TESTING)
#   find_package(benchmark REQUIRED)
#   find_package(mpark_variant REQUIRED)
# endif()

if(ENABLE_GDB_DEBUG)
  # debug模式允许gdb调试
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb")
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -ggdb")
  add_definitions(-DNOT_OPEN_REQUEST_CANCEL)  # 禁止请求提前取消
endif()

# find_package(tbb REQUIRED)

# find_package(libmotan-client REQUIRED)
# find_package(libbrpc REQUIRED)
# find_package(libleveldb REQUIRED)
find_package(absl REQUIRED)
find_package(FBThrift REQUIRED)
find_package(Folly REQUIRED)
find_package(Wangle REQUIRED)
find_package(fizz REQUIRED)
find_package(Threads REQUIRED)
find_package(yarpl REQUIRED)
find_package(fmt REQUIRED)
find_package(rsocket REQUIRED)
find_package(glog REQUIRED)
find_package(Gflags REQUIRED)
find_package(libsvm_api REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/third-party/include /usr/local/include /usr/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/framework ${BOOST_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/third-party/include/tbb ${fmt_INCLUDE_DIR} ${LIBSVM_API_INCLUDE_DIR} /usr/local/hadoop-2.7.3/include)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY} ${CMAKE_SOURCE_DIR}/third-party/lib
                 ${absl_LIBRARY_DIR} ${grpc_LIBRARY_DIR} /home/pai/lib /usr/local/lib /usr/local/hadoop-2.7.3/lib/native)
                 
add_definitions(-DRAPIDJSON_HAS_STDSTRING -DRAPIDJSON_WRITE_DEFAULT_FLAGS=kWriteNanAndInfFlag)

set(ABSL_LIB_USING
    absl_city
    absl_base
    absl_raw_logging_internal
    absl_raw_hash_set
    absl_throw_delegate
    absl_int128
    absl_bad_optional_access
    absl_bad_variant_access
    absl_civil_time
    absl_cord
    absl_cord_internal
    absl_cordz_info
    absl_cordz_functions
    absl_cordz_handle
    absl_crc32c
    absl_crc_cord_state
    absl_crc_cpu_detect
    absl_crc_internal
    absl_debugging_internal
    absl_demangle_internal
    absl_exponential_biased
    absl_graphcycles_internal
    absl_hash
    absl_hashtablez_sampler
    absl_low_level_hash
    absl_log_severity
    absl_malloc_internal
    absl_spinlock_wait
    absl_stacktrace
    absl_status
    absl_statusor
    absl_str_format_internal
    absl_strerror
    absl_strings
    absl_strings_internal
    absl_symbolize
    absl_synchronization
    absl_time
    absl_time_zone)

set(MODULE_STATIC_LIB
    weidl.static 
    weips.static 
    thrift.static 
    weidl_func.static 
    weithrift
    FBThrift::thriftcpp2
    service_discovery
    jsoncpp
    motan-client
    grpc
    grpc++
    gpr
    brpc
    address_sorting
    grpc++_reflection
    cares
    protobuf
    leveldb
    snappy
    tbb
    zookeeper_mt 
    ${ABSL_LIB_USING})

set(MODULE_SHARED_LIB
  mysqlcppconn
  opencc
  libsvm_api
  faiss_avx512
  hdfs
  rdkafka++
  -Wl,--no-as-needed
  rdkafka
  -Wl,--as-needed
  glog
  prometheus-cpp-push
  prometheus-cpp-pull
  prometheus-cpp-core
  mkl_rt
  /usr/local/lib/libfmt.so.5
  boost_program_options
  boost_system
  boost_filesystem
  boost_regex
  boost_context
  boost_thread
  boost_iostreams
  event
  event_pthreads
  double-conversion
  gflags
  pthread
  crypto
  ssl
  zstd
  z
  dl
  ${TF_LIB}
  folly
  jemalloc)

set(MODULE_EXT_LDFLAGS "-Wl,-rpath=/usr/local/lib -Wl,-rpath=${TF_LIB_DIR} -Wl,-rpath=${TF_LIB_DIR}/python -Wl,-rpath=/home/pai/lib -Wl,-rpath=/total_recall/plugs -Wl,-rpath-link=${CMAKE_SOURCE_DIR}/third-party/lib -Wl,-rpath=/usr/local/hadoop-2.7.3/lib/native")

message(${MODULE_EXT_LDFLAGS})
add_subdirectory(src)
add_subdirectory(framework)

SET(CPACK_GENERATOR "TGZ")
include(CPack)

set(TR_TGZ_DIR ${CMAKE_SOURCE_DIR}/deploy/total_recall)
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  set(TR_TGZ_FILE "total_recall-${GIT_BRANCH}-debug.tar.gz")
else()
  set(TR_TGZ_FILE "total_recall-${GIT_BRANCH}-release.tar.gz")
endif()
file(GLOB DYNAMIC_LIB ${LIBSVM_API_LIB})

add_custom_target(
  release-tgz
  COMMAND [ -z ${DYNAMIC_LIB} ] || cp -r ${DYNAMIC_LIB} ${TR_TGZ_DIR}/plugs
  COMMAND cp ${CMAKE_BINARY_DIR}/src/server/total_recall ${TR_TGZ_DIR}/bin
  #COMMAND tar czf ${TR_TGZ_FILE} ${TR_TGZ_DIR}
  DEPENDS total_recall
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "build release package: ${HA_TGZ_FILE}")