cmake_minimum_required(VERSION 3.15)
project(meow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_executable(meow
    src/main.cpp
    src/api.cpp
    src/terminal.cpp
    src/render.cpp
)

target_include_directories(meow PRIVATE third_party)
target_link_libraries(meow PRIVATE CURL::libcurl Threads::Threads)
target_compile_options(meow PRIVATE -Wall -Wextra)
