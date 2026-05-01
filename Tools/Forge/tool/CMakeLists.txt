# ─── Forge — Standalone Build Helper ────────────────────────────────────────
#
# Forge is a thin CMake wrapper for C++ projects. It is intentionally
# minimal:
#   - It does NOT walk up to find SagaEngineRoot.marker.
#   - It does NOT depend on Conan, LLVM, or any third-party library.
#   - It does NOT know about Prism, the SagaTools dispatcher, or any
#     other tool. Forge spawns `cmake`. That is its entire universe.
#
# The day this folder is extracted into its own git repository, this
# CMakeLists becomes the new repo's top-level build file with zero
# rewrites.
#
cmake_minimum_required(VERSION 3.22)

project(forge
    VERSION     0.1.0
    LANGUAGES   CXX
    DESCRIPTION "Standalone build helper (Forge)"
)

# ─── Toolchain Defaults ─────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ─── Sources ────────────────────────────────────────────────────────────────

set(FORGE_HEADERS
    include/Forge/Manifest.h
    include/Forge/ProcessRunner.h
)

set(FORGE_SOURCES
    src/main.cpp
    src/Manifest.cpp
    src/ProcessRunner.cpp
)

# ─── Executable Target ──────────────────────────────────────────────────────

add_executable(forge ${FORGE_SOURCES} ${FORGE_HEADERS})

target_include_directories(forge
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(forge PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(forge PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(forge PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ─── Install Rules ──────────────────────────────────────────────────────────

include(GNUInstallDirs)

install(TARGETS forge
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

message(STATUS "Forge: configured (standalone build helper)")
