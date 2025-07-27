#pragma once
#define BOOST_STACKTRACE_USE_BACKTRACE
#include <pthread.h>
#include <thread>
#include <utility>
#include <type_traits>
#include <string_view>

namespace thread_util {

  void setThreadScheduleLowPriority(pthread_t pid);
  void setThreadScheduleLowPriority();
  void setThreadName(std::string_view name);

} // namespace common
