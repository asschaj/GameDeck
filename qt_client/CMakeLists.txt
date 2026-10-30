cmake_minimum_required(VERSION 3.16)
project(gamedeck_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets WebSockets Network)

add_executable(gamedeck_client
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/GameClient.h
    src/GameClient.cpp
)

target_link_libraries(gamedeck_client PRIVATE
    Qt5::Widgets
    Qt5::WebSockets
    Qt5::Network
)

# 某些环境 /usr/local/lib 下存在旧版 Qt（5.12），会被优先加载导致
# "version `Qt_5.15' not found"。设置 RPATH 强制使用系统 Qt 5.15。
get_target_property(_qt_core_loc Qt5::Core IMPORTED_LOCATION_RELEASE)
if(NOT _qt_core_loc)
    get_target_property(_qt_core_loc Qt5::Core IMPORTED_LOCATION)
endif()
if(_qt_core_loc AND EXISTS "${_qt_core_loc}")
    get_filename_component(_qt_lib_dir "${_qt_core_loc}" DIRECTORY)
    set_target_properties(gamedeck_client PROPERTIES
        BUILD_WITH_INSTALL_RPATH ON
        INSTALL_RPATH "${_qt_lib_dir}"
    )
    message(STATUS "gamedeck_client RPATH -> ${_qt_lib_dir}")
endif()
