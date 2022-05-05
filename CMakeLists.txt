cmake_minimum_required(VERSION 3.22)
project(cdemo C)

set(CMAKE_C_STANDARD 23)

add_executable(cdemo main.c)
target_link_libraries(cdemo ev)