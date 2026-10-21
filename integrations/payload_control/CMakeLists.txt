cmake_minimum_required(VERSION 3.21)
project(RPiStatusGCS LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network SerialPort)

# Set MAVLINK_INCLUDE_DIR to the directory containing mavlink/common/mavlink.h
# Example: -DMAVLINK_INCLUDE_DIR=C:/path/to/mavlink/include
if(NOT DEFINED MAVLINK_INCLUDE_DIR)
    message(FATAL_ERROR "Set MAVLINK_INCLUDE_DIR to the folder containing mavlink/common/mavlink.h")
endif()

qt_add_executable(RPiStatusGCS
    main.cpp
    MainWindow.h MainWindow.cpp
    MavlinkStatusClient.h MavlinkStatusClient.cpp
)
target_include_directories(RPiStatusGCS PRIVATE "${MAVLINK_INCLUDE_DIR}")
target_link_libraries(RPiStatusGCS PRIVATE Qt6::Widgets Qt6::Network Qt6::SerialPort)
