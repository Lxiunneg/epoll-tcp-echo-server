// server.h
#pragma once

// my module
#include "simple_log.h"

// Linux
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdint.h> // 位长控制
#include <memory>   // 智能指针
#include <atomic>   // 并发安全

namespace xiunneg {

// 缓冲区大小
constexpr uint32_t BUFFER_SIZE = 1024 * 1024; // 1MB

/// @brief 基于 Epoll 的高并发 TCP 服务器类
class Server {
    using Logger = std::shared_ptr<SimpleLoggerInterface>; // 日志器接口

public:
    // epoll 触发模式枚举
    enum class EpollEventMode : uint32_t {
        LT = EPOLLIN,          // 水平触发
        ET = EPOLLIN | EPOLLET // 边缘触发
    };

    /// @brief 配置类
    struct Config {
        // socket
        uint16_t port;
        uint16_t socket_max_conn = 1024;

        // epoll
        EpollEventMode ev_mode = EpollEventMode::ET;
        uint32_t max_epoll_events = 10;
    };

private:
    // Linux Socket
    int socket_fd_;                         // 套接字 文件描述符
    int epoll_fd_;                          // epoll实例 文件描述符
    epoll_event ev_;                        // epoll 事件
    std::unique_ptr<epoll_event[]> events_; //
    sockaddr_in address_;

    Config config_;
    Logger logger_;

    std::atomic<bool> running_;

public:
    Server(const Config &config);

    // 链式调用，设置接口
    Server &set_logger(Logger logger) {
        logger_ = logger;
        return *this;
    }

    // 启停接口
    void run();
    void stop();

private:
    // 主循环任务
    void work();

    /// @brief 创建 socket
    void create_socket();

    /// @brief 设置 socket 为非阻塞
    int make_socket_non_blocking(int fd);

    /// @brief 创建 epoll
    void create_epoll();
};

} // namespace xiunneg
