#include <boost/stacktrace.hpp>
#include "signal_handler.h"

namespace common{

void signal_backtrace(int signo) {
  // auto t = boost::stacktrace::stacktrace();
  // auto coredump = boost::stacktrace::to_string(t);
  // printf("coredump: %s\n", coredump.c_str());
  // fflush(stdout);
  // //LOG(ERROR) << "coredump: \n" << coredump;
  // google::FlushLogFiles(google::GLOG_ERROR);
  // google::FlushLogFiles(google::GLOG_INFO);
  ::raise(signo); // 再次触发信号，生成 coredump文件，保留事件现场
}

int setup_signals() {
  sigset_t sigmask;
  sigfillset(&sigmask);
  sigdelset(&sigmask, SIGSEGV);
  sigdelset(&sigmask, SIGBUS);
  sigdelset(&sigmask, SIGFPE);
  sigdelset(&sigmask, SIGILL);
  sigdelset(&sigmask, SIGPROF);
  sigdelset(&sigmask, SIGABRT);
  if (pthread_sigmask(SIG_SETMASK, &sigmask, nullptr) != 0) {
    LOG(ERROR) << "failed call pthread_sigmask(SIG_SETMASK)";
    return -1;
  }

  struct sigaction action;
  sigemptyset(&action.sa_mask);
  action.sa_flags   = SA_NODEFER | SA_RESETHAND;
  action.sa_handler = signal_backtrace;
  sigaction(SIGSEGV, &action, nullptr);
  sigaction(SIGBUS, &action, nullptr);
  sigaction(SIGFPE, &action, nullptr);
  sigaction(SIGILL, &action, nullptr);
  sigaction(SIGABRT, &action, nullptr);

  return 0;
}

};//namespace common