# Epoll-TCP-Server
基于 epoll 的 tcp 简单echo服务器 
## 项目简介
平台: `Ubuntu24.04`  
编译器: Clang 18.1.3 x86_64-pc-linux-gnu  
C++版本: `C++17`/`C++20`  
构建工具: `CMake ` 
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
