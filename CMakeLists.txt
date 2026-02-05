cmake_minimum_required(VERSION 3.20)

project(tinix LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(tinix ${SOURCES})

target_include_directories(tinix PRIVATE include)

include(CTest)
enable_testing()
add_subdirectory(test)
