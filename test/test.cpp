#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define PORT 8080

int create_and_bind(int port) {
    int listen_sock;
    struct sockaddr_in addr;

    // 创建 socket
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("socket");
        return -1;
    }

    // 设置地址可重用（避免 "Address already in use" 错误）
    int optval = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        close(listen_sock);
        return -1;
    }

    // 绑定地址和端口
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有接口
    addr.sin_port = htons(port);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_sock);
        return -1;
    }

    // 开始监听
    if (listen(listen_sock, SOMAXCONN) == -1) { // SOMAXCONN 是系统允许的最大 backlog
        perror("listen");
        close(listen_sock);
        return -1;
    }

    return listen_sock;
}

int make_socket_non_blocking(int sock) {
    int flags, s;
    flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sock, F_SETFL, flags);
    if (s == -1) {
        perror("fcntl");
        return -1;
    }

    return 0;
}

int main(void) {
    int listen_sock, epoll_fd;
    struct epoll_event ev, events[MAX_EVENTS];

    // 1. 创建并绑定监听 socket
    listen_sock = create_and_bind(PORT);
    if (listen_sock == -1) {
        exit(EXIT_FAILURE);
    }

    // 2. 将监听 socket 设为非阻塞
    if (make_socket_non_blocking(listen_sock) == -1) {
        exit(EXIT_FAILURE);
    }

    // 3. 创建 epoll 实例
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // 4. 将监听 socket 添加到 epoll 实例中，监听 EPOLLIN (可读) 事件
    ev.events = EPOLLIN; // 水平触发 (LT) 模式
    // ev.events = EPOLLIN | EPOLLET; // 边缘触发 (ET) 模式
    ev.data.fd = listen_sock;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // 主事件循环
    while (1) {
        int nfds, i;

        // 等待事件发生
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); // -1 表示无限等待
        if (nfds == -1) {
            if (errno == EINTR) {
                // 被信号中断，继续循环
                continue;
            } else {
                perror("epoll_wait");
                break;
            }
        }

        // 处理所有就绪的事件
        for (i = 0; i < nfds; ++i) {
            // 情况1: 监听 socket 就绪 (有新连接)
            if (events[i].data.fd == listen_sock) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_sock;

                // 循环 accept，直到没有新连接 (ET模式下必须这样做)
                while (1) {
                    client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
                    if (client_sock == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 没有更多连接可接受
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    // 将客户端 socket 设为非阻塞
                    if (make_socket_non_blocking(client_sock) == -1) {
                        close(client_sock);
                        break;
                    }

                    // 将客户端 socket 添加到 epoll 实例
                    ev.events = EPOLLIN | EPOLLRDHUP; // EPOLLRDHUP 检测对端关闭连接
                    // ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET; // 如果使用 ET 模式
                    ev.data.fd = client_sock;

                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &ev) == -1) {
                        perror("epoll_ctl: client_sock");
                        close(client_sock);
                        break;
                    }

                    printf("New connection from %s:%d (fd=%d)\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_sock);
                }
            }
            // 情况2: 客户端 socket 就绪 (可读或出错)
            else {
                int client_fd = events[i].data.fd;
                ssize_t count;
                char buf[BUFFER_SIZE];

                // 处理读事件或错误事件
                if (events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
                    while (1) {
                        count = read(client_fd, buf, sizeof(buf));
                        if (count == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                // 数据已读完
                                break;
                            } else {
                                perror("read");
                                break;
                            }
                        } else if (count == 0) {
                            // 对端关闭连接
                            printf("Connection closed on fd %d\n", client_fd);
                            break;
                        }

                        // 处理接收到的数据 (这里简单回显)
                        printf("Received %zd bytes from fd %d\n", count, client_fd);
                        // 回显数据
                        if (write(client_fd, buf, count) != count) {
                            perror("write");
                            break;
                        }
                    }

                    // 如果 read 返回 0 或出错，则关闭连接
                    close(client_fd);
                    // 从 epoll 实例中移除 (可选，close 会自动移除)
                    // epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    printf("Closed connection on fd %d\n", client_fd);
                }
                // 注意: 这个简单示例没有处理 EPOLLOUT (可写) 事件。
                // 在实际应用中，如果需要写大量数据，应该注册 EPOLLOUT 事件，
                // 并在 epoll_wait 返回时写入，避免阻塞。
            }
        }
    }

    close(listen_sock);
    close(epoll_fd);
    return EXIT_SUCCESS;
}
