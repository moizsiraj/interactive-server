cmake_minimum_required(VERSION 3.16)
project(interactiveServer)

set(CMAKE_CXX_STANDARD 14)

add_executable(interactiveServer server.cpp client.cpp)