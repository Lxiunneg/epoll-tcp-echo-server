// server.cpp
#include "server.h"

#include <fmt/format.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>

using namespace xiunneg;

Server::Server(const Config &config) : config_(config),
                                       socket_fd_(-1),
                                       epoll_fd_(-1),
                                       running_(false) {
}

void Server::create_socket() {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ == -1) {
        throw std::runtime_error("Server::create_socket(): socket() 套接字创建失败");
    }

    logger_->info("套接字创建成功!");

    int optval = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        close(socket_fd_);
        throw std::runtime_error("Server::create_socket(): setsockopt() 设置地址重用失败");
    }

    logger_->info("套接字设置地址重用成功!");

    memset(&address_, 0, sizeof(address_));
    address_.sin_family = AF_INET;
    address_.sin_addr.s_addr = htonl(INADDR_ANY);
    address_.sin_port = htons(config_.port);

    if (bind(socket_fd_, (sockaddr *)&address_, sizeof(address_)) == -1) {
        close(socket_fd_);
        throw std::runtime_error("Server::create_socket(): bind() 绑定套接字失败");
    }

    logger_->info(fmt::format("套接字已成功绑定 0.0.0.0:{}", config_.port));

    if (listen(socket_fd_, config_.socket_max_conn) == -1) {
        close(socket_fd_);
        throw std::runtime_error("Server::create_socket(): listen() 监听端口失败");
    }

    logger_->info(fmt::format("套接字已监听,运行最大连接数:{}", config_.socket_max_conn));
}

int Server::make_socket_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        return -1;
    }

    logger_->info("设置套接字为非阻塞模式成功!");
    return 0;
}

void Server::create_epoll() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        throw std::runtime_error("Server::create_epoll(): epoll_create1() 创建 epoll 实例失败");
    }

    logger_->info("创建 epoll 实例成功!");

    ev_.events = static_cast<uint32_t>(config_.ev_mode);
    ev_.data.fd = socket_fd_;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_fd_, &ev_) == -1) {
        close(epoll_fd_);
        throw std::runtime_error("Server::create_epoll: epoll_ctl() 添加监听 socket 失败");
    }

    events_ = std::make_unique<epoll_event[]>(config_.max_epoll_events);
    logger_->info("将监听 socket 添加到 epoll 实例成功!");
}

void Server::work() {
    logger_->info(fmt::format("服务器成功运行在 0.0.0.0:{}", config_.port));

    while (running_.load()) { // 支持外部 stop()
        int nfds = epoll_wait(epoll_fd_, events_.get(), config_.max_epoll_events, -1);

        if (!running_.load()) break; // 再次检查，确保快速退出

        if (nfds == -1) {
            if (errno == EINTR) continue;
            logger_->error(fmt::format("epoll_wait error: {}", strerror(errno)));
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            // 1. 新连接到来
            if (events_[i].data.fd == socket_fd_) {
                while (true) {
                    struct sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(socket_fd_, (struct sockaddr *)&client_addr, &client_len);

                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; // 没有更多连接
                        } else {
                            logger_->error(fmt::format("accept() error: {}", strerror(errno)));
                            break;
                        }
                    }

                    if (make_socket_non_blocking(client_fd) == -1) {
                        logger_->error(fmt::format("无法将客户端套接字 {} 设置为非阻塞", client_fd));
                        close(client_fd);
                        continue;
                    }

                    ev_.events = EPOLLIN | EPOLLRDHUP; // 可读 + 对端关闭
                    if (config_.ev_mode == EpollEventMode::RT) {
                        ev_.events |= EPOLLET; // 边缘触发
                    }
                    ev_.data.fd = client_fd;

                    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev_) == -1) {
                        logger_->error(fmt::format("epoll 添加 client_fd={} 时发生错误: {}", client_fd, strerror(errno)));
                        close(client_fd);
                    } else {
                        logger_->info(fmt::format("有来自 {}:{} 的新连接 (fd={})",
                                                  inet_ntoa(client_addr.sin_addr),
                                                  ntohs(client_addr.sin_port), client_fd));
                    }
                }
            }
            // 2. 已连接客户端有事件
            else {
                const int client_fd = events_[i].data.fd;

                // 连接关闭或错误
                if (events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    logger_->info(fmt::format("关闭客户端 fd {} (RDHUP/HUP/ERR)", client_fd));
                    close(client_fd);
                    continue;
                }

                // 可读事件
                if (events_[i].events & EPOLLIN) {
                    char buf[BUFFER_SIZE];
                    ssize_t n;
                    while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
                        logger_->info(fmt::format("接收来自 fd={} 到 {} 个字节.接收到的消息为:{}", client_fd, n, buf));

                        // Echo 回去
                        ssize_t sent = 0;
                        while (sent < n) {
                            ssize_t result = write(client_fd, buf + sent, n - sent);
                            if (result <= 0) {
                                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                    // 非阻塞写满，稍后重试（本例中直接断开）
                                    break;
                                } else {
                                    logger_->error(fmt::format("回写 fd={} 错误: {}", client_fd, strerror(errno)));
                                    close(client_fd);
                                    goto next_event; // 跳出嵌套循环
                                }
                            }
                            sent += result;
                        }

                        logger_->info(fmt::format("已回显消息到客户端[fd={}]: {}", client_fd, buf));
                    }

                    if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        logger_->error(fmt::format("客户端 fd={} 错误: {}", client_fd, strerror(errno)));
                    } else if (n == 0) {
                        // 客户端正常关闭连接
                        logger_->info(fmt::format("客户端 fd={} 关闭", client_fd));
                    }
                    // 注意：这里我们不主动 close，由 EPOLLRDHUP 触发更安全
                }
            }
        next_event:;
        }
    }

    logger_->info("服务器工作循环结束");
}

void Server::run() {
    create_socket();
    make_socket_non_blocking(socket_fd_);
    create_epoll();
    running_.store(true);
    work();
}

void Server::stop() {
    running_.store(false); // 中断主循环

    // 唤醒阻塞的 epoll_wait
    // 可以通过关闭 socket 或写管道等方式，这里简单关闭 listen socket
    if (socket_fd_ != -1) {
        close(socket_fd_);
        socket_fd_ = -1;
    }

    logger_->info("服务器正在关闭...");
}