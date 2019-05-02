cmake_minimum_required(VERSION 3.13)
project(NBLogDemo)

set(CMAKE_CXX_STANDARD 11)

add_executable(NBLogDemo main.cpp)
target_link_libraries(NBLogDemo
        NBLog)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/NBLog)