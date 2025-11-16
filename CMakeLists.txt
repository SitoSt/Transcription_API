cmake_minimum_required(VERSION 3.16)
project(mi_api)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)

add_executable(mi_api src/server.cpp)
target_link_libraries(mi_api PRIVATE Boost::boost pthread)