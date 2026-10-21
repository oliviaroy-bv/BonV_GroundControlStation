cmake_minimum_required(VERSION 3.16)
project(VpsGcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

add_executable(VpsGcs
    main.cpp
    MavlinkTcpLink.h  MavlinkTcpLink.cpp
    VpsProtocol.h     VpsProtocol.cpp
    VioMapWidget.h    VioMapWidget.cpp
    MainWindow.h      MainWindow.cpp
)
target_link_libraries(VpsGcs PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)
