#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace framework {
namespace common {

class AsyncLogger {
public:
    static AsyncLogger& Instance();

    // 初始化日志系统
    bool Init(const std::string& log_path);  // 日志文件路径

    // 设置日志级别
    void SetLevel(spdlog::level::level_enum level);

    // 获取logger实例
    std::shared_ptr<spdlog::logger> GetLogger() const { return logger_; }

    // 刷新日志
    void Flush();

private:
    AsyncLogger() = default;
    ~AsyncLogger();
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    // 生成当前小时的日志文件名
    std::string GenerateHourlyFilename(const std::string& log_path) const;

    // 清理过期的日志文件（保留最近3天的）
    void CleanOldLogs(const std::string& log_path) const;

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::string base_path_;
};

// 使用spdlog的原生宏，需要先定义SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

// 定义获取logger的宏
#define LOGGER framework::common::AsyncLogger::Instance().GetLogger()

}  // namespace common
}  // namespace framework

// 包含spdlog的宏定义头文件
#include <spdlog/spdlog.h>
