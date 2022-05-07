cmake_minimum_required(VERSION 3.5)

project(flashlight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wpedantic -pthread")
add_executable(flashlight TcpConnection.h main.cpp ConcurrentQueue.h Log.h)
