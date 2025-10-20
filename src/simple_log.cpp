// simple_log.cpp
#include "simple_log.h"

// c++ string format
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include <iostream>
#include <chrono>

using namespace xiunneg;

SimpleLogger::SimpleLogger(const std::string &name, LoggerMode mode) :
    name_(name),
    use_console_(false),
    use_file_(false) {
    switch (mode) {
    case LoggerMode::CONSOLE_ONLY:
        use_console_ = true;
        break;
    case LoggerMode::FILE_ONLY:
        use_file_ = true;
        break;
    case LoggerMode::CONSOLE_AND_FILE:
        use_console_ = true;
        use_file_ = true;
        break;
    default:
        break;
    }
    init();
}

SimpleLogger::~SimpleLogger() {
    if (log_file_ && log_file_->is_open()) {
        log_file_->close();
    }
}

void SimpleLogger::info(std::string_view log) {
    std::unique_lock lck(logger_mtx_);

    std::string formatting_log = fmt::format("[{}][{}][{}] {}\n",
                                             get_now_timestamp_string(),
                                             name_,
                                             fmt::format(fmt::fg(fmt::color::green), "info"), // 将 "info" 格式化为绿色
                                             log);

    if (use_console_) {
        std::cout << formatting_log;
    }
    if (use_file_) {
        (*log_file_) << formatting_log;
    }
}

void SimpleLogger::error(std::string_view log) {
    std::unique_lock lck(logger_mtx_);

    std::string formatting_log = fmt::format("[{}][{}][{}] {}\n",
                                             get_now_timestamp_string(),
                                             name_,
                                             fmt::format(fmt::fg(fmt::color::red), "error"), // 将 "error" 格式化为红色
                                             log);

    if (use_console_) {
        std::cout << formatting_log;
    }
    if (use_file_) {
        (*log_file_) << formatting_log;
    }
}

void SimpleLogger::init() {
    if (use_file_) {
        log_file_ = std::make_unique<std::fstream>(name_ + "_" + std::string(logger_file_path_), std::ios::out | std::ios::app);
        if (!log_file_->is_open()) {
            if (use_console_) {
                std::cerr << "[ERROR] Failed to open log file: " << logger_file_path_ << std::endl;
            }
            use_file_ = false; // 禁用文件输出
        }
    }
}

std::string SimpleLogger::get_now_timestamp_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_snapshot;
#if defined(_MSC_VER)
    localtime_s(&tm_snapshot, &time_t); // Windows
#else
    localtime_r(&time_t, &tm_snapshot); // POSIX (Linux/macOS)
#endif

    // 格式：YYYY-MM-DD HH:MM:SS.mmm
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_snapshot);

    // 手动添加毫秒
    std::string result(buffer);
    result += '.' + std::to_string(ms.count());

    return result;
}