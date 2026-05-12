cmake_minimum_required(VERSION 3.22)

project(PrismMinimalExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(prism-minimal-example
    src/SampleComponent.cpp
)

target_include_directories(prism-minimal-example
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
