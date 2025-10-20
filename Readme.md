# Epoll-TCP-Server
基于 epoll 的 tcp 简单echo服务器 
## 项目简介
平台: `Ubuntu24.04`  
编译器: Clang 18.1.3 x86_64-pc-linux-gnu  
C++版本: `C++17`/`C++20`  
构建工具: `CMake`   
包管理: `vcpkg`  
第三方依赖:  
- `fmt`: C++ 字符串格式化库

## 项目结构
```bash
├── CMakeLists.txt #构建文件
├── include # 头文件
│   ├── server.h # echo 服务器
│   └── simple_log.h # 简单日志器
├── main.cpp
├── Readme.md
├── src
│   ├── server.cpp
│   └── simple_log.cpp
└── test # 测试项目
    ├── CMakeLists.txt # 测试醒目构建
    └── test.cpp #测试文件
```
## 目标
### 基础功能(60分)
- 连接管理
    - 监听指定端口（如 8080），支持客户端 TCP 连接的建立与关闭 -[x]  
    - 限制最大并发连接数（如 10000，超过则拒绝新连接并返回错误码）-[x]  
- 数据处理
    - 采用 epoll ET 模式（边缘触发）处理读写事件（强制要求，考察 ET 模式的细节处理）-[x]
    - 接收客户端发送的字符串数据（支持任意长度，最长不超过 1MB），原样返回（回声功能）-[x]
    - 正确处理 “粘包” 问题（需自定义应用层协议，如用固定长度的包头标识数据长度）-[]  
- 事件处理
    - 注册EPOLLIN（读）、EPOLLOUT（写）、EPOLLERR（错误）事件，正确处理客户端异常断开（如RST包） -[] (40%)  
    - 避免 “惊群效应”（通过设置SO_REUSEPORT或主线程单独监听 + 子线程处理连接）
### 高并发与稳定性要求(30分)
- 并发能力  
    - 支持至少 1000 个客户端同时保持连接，并稳定处理每秒 1000 + 次请求（通过 socketbench 压测验证） -[]  
    - 连接建立 / 关闭的耗时≤10ms（通过压测工具统计）-[]  
- 资源管理  
    - 无文件描述符（fd）泄漏（通过`ls /proc/<pid>/fd`验证，连接关闭后 fd 数量归零） -[] 
    - 无内存泄漏（通过 valgrind 检测，长时间运行后内存占用稳定） -[]
    - 合理设置 socket 缓冲区大小（SO_RCVBUF/SO_SNDBUF），避免缓冲区溢出或性能瓶颈 -[]
- 异常处理
    - 客户端突然断开连接时，服务器无崩溃、无死锁，能自动清理资源（fd、内存）-[x]
    - 处理大量半连接（如 SYN Flood）时，通过tcp_max_syn_backlog等内核参数优化，避免服务不可用 -[x]
### 性能优化(10分)
- 略



## 启动
1. 首先确保`C++`环境存在,`CMake`已安装。  
2. 再使用`vcpkg` 安装 `fmt` 库  
    ```bash
    vcpkg install fmt
    ```
3. 进入项目文件夹,创建`build/`并构建`Makefile`:
    ```bash
    mkdir build && cd build
    cmake ..
    ```
4. 编译
    ```bash
    make
    ```
5. 运行
    ```bash
    ./Epoll_TCP_Server 
    ```
