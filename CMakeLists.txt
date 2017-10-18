cmake_minimum_required(VERSION 3.8)
project(md3)

link_libraries(crypto)

set(CMAKE_C_STANDARD 90)

set(SOURCE_FILES main.c)
add_executable(md3 ${SOURCE_FILES})