#include <pthread.h>
#include <string_view>
#include <glog/logging.h>
#include <folly/system/ThreadName.h>

namespace thread_util {

void setThreadName(std::string_view name) {
  folly::setThreadName(folly::StringPiece {name.data(), name.length()});
}

// 根据 https://lwn.net/Articles/805317/ kernel 5.3以后，SCHED_IDLE 改善有更好的效果
void setThreadScheduleLowPriority(pthread_t pid) {
  struct sched_param param = {};
  param.sched_priority     = 0;
  if (auto retval = pthread_setschedparam(pid, SCHED_IDLE, &param)) {
    LOG(ERROR) << "pthread_setschedparam() failed, code:" << retval << " tid:" << pid
               << " error:" << ::strerror(errno);
  }
}

void setThreadScheduleLowPriority() { setThreadScheduleLowPriority(pthread_self()); }

} // namespace common
