cmake_minimum_required(VERSION 3.16)
project(gamedeck LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 查找 Boost（需要 system, 以及 Beast 的头文件）
find_package(Boost REQUIRED COMPONENTS system)
# Boost.Beast 是 header-only，只需 include

add_executable(gamedeck_server
    src/main.cpp
    src/Server.cpp
    src/Session.cpp
    src/GameManager.cpp
    src/GameRegistry.cpp
)

target_include_directories(gamedeck_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(gamedeck_server PRIVATE
    Boost::system
)

# 需要 pthread（Boost.Asio 依赖）
find_package(Threads REQUIRED)
target_link_libraries(gamedeck_server PRIVATE Threads::Threads)
