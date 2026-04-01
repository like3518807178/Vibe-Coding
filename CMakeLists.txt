cmake_minimum_required(VERSION 3.10)

project(TinyIM VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(tinyim_v1
    src/main.cpp
    src/server.cpp
)
