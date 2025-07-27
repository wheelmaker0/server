#include "log_test.h"
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/hourly_file_sink.h"

void LogTest::run() {
    auto logger = spdlog::hourly_logger_mt<spdlog::async_factory>("total_recall", "/log/total_recall.log", false, 72);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e %t %l %s:%#] %v");
    logger->set_level(spdlog::level::info);
    spdlog::set_default_logger(logger);
    
    std::thread t([]() {
        SPDLOG_INFO("hello spdlog");
    });
    t.join();
    SPDLOG_TRACE("hello spdlog");
    SPDLOG_DEBUG("hello spdlog");
    SPDLOG_ERROR("hello spdlog");
    SPDLOG_CRITICAL("hello spdlog");
}
