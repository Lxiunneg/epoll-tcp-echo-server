# 最低CMake版本要求
cmake_minimum_required(VERSION 3.10)

# 1. 设置 C++ 标准为 20（CMake 3.12+ 支持）
set(CMAKE_CXX_STANDARD 20)

# 项目名称
project(Epoll_TCP_Server_Test)


file(GLOB SOURCES
    test.cpp
    "../src/*.cpp"
    "../src/*.cc"
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ../include
)

find_package(fmt CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    fmt::fmt-header-only
)

