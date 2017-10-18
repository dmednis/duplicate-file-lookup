cmake_minimum_required(VERSION 3.8)
project(duplicate-file-lookup)

link_libraries(crypto)

set(CMAKE_C_STANDARD 90)

set(SOURCE_FILES main.c)
add_executable(duplicate-file-lookup ${SOURCE_FILES})
