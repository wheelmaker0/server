#pragma once
#include <signal.h>
#include <glog/logging.h>

namespace common{

void signal_backtrace(int signo);
int setup_signals();

};//namespace common