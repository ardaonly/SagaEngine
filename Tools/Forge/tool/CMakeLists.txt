# ─── Forge — Standalone Build Orchestration Frontend ─────────────────────────
#
# Forge is a Cargo-flavoured build frontend for C++. It is intentionally
# self-contained:
#   - No walk-up to find any engine root or marker file.
#   - No dependency on Conan, LLVM, Qt, or any third-party library.
#   - No knowledge of any specific project, engine, or repository.
#
# CMake and Conan are backends called through adapter classes.
# This file can become a standalone repository root with zero rewrites.
#
cmake_minimum_required(VERSION 3.22)

project(forge
    VERSION     0.1.0
    LANGUAGES   CXX
    DESCRIPTION "Standalone build helper (Forge)"
)

# ─── Toolchain Defaults ─────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ─── Sources ────────────────────────────────────────────────────────────────

set(FORGE_HEADERS
    include/Forge/BuildModel.h
    include/Forge/BuildScheduler.h
    include/Forge/CMakeAdapter.h
    include/Forge/ConanAdapter.h
    include/Forge/EnvProbe.h
    include/Forge/Hardware.h
    include/Forge/Manifest.h
    include/Forge/ProcessRunner.h
    include/Forge/ToolEnv.h
)

set(FORGE_SOURCES
    src/main.cpp
    src/BuildModel.cpp
    src/BuildScheduler.cpp
    src/CMakeAdapter.cpp
    src/ConanAdapter.cpp
    src/EnvProbe.cpp
    src/Hardware.cpp
    src/Manifest.cpp
    src/ProcessRunner.cpp
    src/ToolEnv.cpp
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/forge"
    )
endif()

message(STATUS "Forge ${PROJECT_VERSION}: configured (standalone build frontend)")
