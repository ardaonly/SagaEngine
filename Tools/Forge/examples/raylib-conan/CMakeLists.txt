cmake_minimum_required(VERSION 3.22)
project(raylib-forge-demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(raylib CONFIG REQUIRED)

add_executable(raylib-forge-demo src/main.cpp)

if(TARGET raylib)
    target_link_libraries(raylib-forge-demo PRIVATE raylib)
elseif(TARGET raylib::raylib)
    target_link_libraries(raylib-forge-demo PRIVATE raylib::raylib)
else()
    message(FATAL_ERROR "raylib package was found, but no known CMake target was exported")
endif()
