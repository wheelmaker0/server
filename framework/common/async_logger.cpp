#include "async_logger.h"
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace framework {
namespace common {

AsyncLogger& AsyncLogger::Instance() {
    static AsyncLogger instance;
    return instance;
}

std::string AsyncLogger::GenerateHourlyFilename(const std::string& log_path) const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << log_path << "/log_"
       << std::put_time(std::localtime(&time), "%Y%m%d_%H")
       << ".log";
    return ss.str();
}

void AsyncLogger::CleanOldLogs(const std::string& log_path) const {
    try {
        auto now = std::chrono::system_clock::now();
        auto three_days_ago = now - std::chrono::hours(72); // 3天前

        for (const auto& entry : std::filesystem::directory_iterator(log_path)) {
            if (entry.is_regular_file()) {
                const auto& path = entry.path();
                if (path.extension() == ".log") {
                    auto filename = path.filename().string();
                    // 解析文件名中的时间
                    if (filename.length() >= 13) { // log_YYYYMMDD_HH.log
                        std::tm tm = {};
                        std::istringstream ss(filename.substr(4, 8) + filename.substr(9, 2));
                        ss >> std::get_time(&tm, "%Y%m%d%H");
                        if (!ss.fail()) {
                            auto file_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                            if (file_time < three_days_ago) {
                                std::filesystem::remove(path);
                            }
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        // 清理失败不影响正常日志记录
        spdlog::error("Failed to clean old logs: {}", e.what());
    }
}

bool AsyncLogger::Init(const std::string& log_path) {
    try {
        base_path_ = log_path;

        // 创建日志目录
        if (!std::filesystem::exists(log_path)) {
            std::filesystem::create_directories(log_path);
        }

        // 清理旧日志文件
        CleanOldLogs(log_path);

        // 初始化异步日志线程池（队列大小8192，1个工作线程）
        spdlog::init_thread_pool(8192, 1);

        // 创建控制台sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        // 创建当前小时的日志文件
        std::string current_log_file = GenerateHourlyFilename(log_path);
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            current_log_file,
            0,      // 不限制单个文件大小
            1       // 只保留一个文件，因为我们用文件名来区分
        );

        // 创建异步logger
        logger_ = std::make_shared<spdlog::async_logger>(
            "async_logger",
            spdlog::sinks_init_list{console_sink, file_sink},
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block
        );

        // 设置日志格式：[时间][日志级别][线程id][文件名:行号] 日志内容
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%^%l%$][%t][%s:%#] %v");

        // 设置默认的日志级别为debug
        logger_->set_level(spdlog::level::debug);

        // 注册logger为默认logger
        spdlog::set_default_logger(logger_);

        // 启动定时器线程，用于检查日志文件切换
        std::thread([this]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::minutes(1));

                static std::string last_filename;
                std::string current_filename = GenerateHourlyFilename(base_path_);

                if (current_filename != last_filename) {
                    // 重新初始化logger
                    this->Init(base_path_);
                    last_filename = current_filename;
                }
            }
        }).detach();

        return true;
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "AsyncLogger initialization failed: " << ex.what() << std::endl;
        return false;
    }
}

void AsyncLogger::SetLevel(spdlog::level::level_enum level) {
    if (logger_) {
        logger_->set_level(level);
    }
}

void AsyncLogger::Flush() {
    if (logger_) {
        logger_->flush();
    }
}

AsyncLogger::~AsyncLogger() {
    if (logger_) {
        logger_->flush();
        spdlog::drop_all();
    }
}

}  // namespace common
}  // namespace framework
