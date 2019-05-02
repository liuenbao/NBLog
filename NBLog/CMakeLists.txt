cmake_minimum_required(VERSION 3.0.0)
project(NBLog C)

#set(CMAKE_C_STANDARD 99)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(SRC_SET
        src/NBLog.c
        src/NBLogAppender.c
        src/NBLogFileAppender.c
        src/NBLogConsoleAppender.c
        src/NBCircularMmap.c
        src/libae/ae.c
        src/libae/anet.c)

add_library(NBLog ${SRC_SET})
set_property(TARGET NBLog PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_property(TARGET NBLog PROPERTY POSITION_INDEPENDENT_CODE ON)