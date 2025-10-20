// simple_log.h
#pragma once

#include <string_view>
#include <memory>
#include <mutex>
#include <fstream>

namespace xiunneg {
class SimpleLoggerInterface {
public:
    enum class LoggerMode {
        CONSOLE_ONLY = 1, // 仅控制台
        FILE_ONLY,        // 仅文件
        CONSOLE_AND_FILE  // 控制台与文件
    };

public:
    virtual void info(std::string_view) = 0;
    virtual void error(std::string_view) = 0;
    virtual ~SimpleLoggerInterface() = default;
};

class SimpleLogger : public SimpleLoggerInterface {
private:
    static constexpr std::string_view logger_file_path_ = "simple_logger.log";

    std::unique_ptr<std::fstream> log_file_;

    std::string name_;
    bool use_console_;
    bool use_file_;
    std::mutex logger_mtx_;

public:
    SimpleLogger(const std::string &name, LoggerMode mode);

    ~SimpleLogger();

    void info(std::string_view log) override;

    void error(std::string_view log) override;

private:
    void init();

    std::string get_now_timestamp_string();
};
} // namespace xiunneg
