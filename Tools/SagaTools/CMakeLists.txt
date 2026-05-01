# ─── tools — Standalone Dispatcher ──────────────────────────────────────────
#
# `tools` is a thin command-line dispatcher. It maps short logical names
# (e.g. "forge", "prism") to executable paths and launches them as child
# processes. It owns no tool logic, depends on no other tool, and links
# against no third-party library.
#
# This CMakeLists is fully self-contained:
#   - It does NOT walk up to find SagaEngineRoot.marker.
#   - It does NOT find_package(SagaEngine), Conan, or any third-party libs.
#   - It does NOT depend on Forge or Prism in any way.
#   - It builds a single executable: tools (tools.exe on Windows).
#
# The day this folder is extracted into its own repository, this file
# becomes the new repo's top-level CMakeLists with zero rewrites.
#
cmake_minimum_required(VERSION 3.22)

project(saga_tools
    VERSION     0.1.0
    LANGUAGES   CXX
    DESCRIPTION "Standalone tool dispatcher for the SagaEngine ecosystem"
)

# ─── Toolchain Defaults ─────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ─── Sources ────────────────────────────────────────────────────────────────

set(TOOLS_HEADERS
    include/SagaTools/Encoding.h
    include/SagaTools/Registry.h
    include/SagaTools/Resolver.h
    include/SagaTools/ProcessRunner.h
)

set(TOOLS_SOURCES
    src/main.cpp
    src/Encoding.cpp
    src/Registry.cpp
    src/Resolver.cpp
    src/ProcessRunner.cpp
)

# ─── Executable Target ──────────────────────────────────────────────────────

add_executable(tools ${TOOLS_SOURCES} ${TOOLS_HEADERS})

target_include_directories(tools
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    # /W4 /permissive- — strict warnings.
    # /utf-8           — interpret source files (and string literals) as
    #                    UTF-8; the dispatcher emits the U+2192 RIGHT ARROW
    #                    in [tools] log lines and that has to round-trip.
    # _CRT_SECURE_NO_WARNINGS — silence C4996 on `getenv` etc. The standard
    #                    C functions are correct here; MSVC's deprecation
    #                    is opinionated, not portable.
    target_compile_options(tools PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(tools PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(tools PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ─── Registry Co-deployment ─────────────────────────────────────────────────
#
# Stage the default registry next to the binary so `tools list` works
# straight out of the build tree without environment setup.
#
add_custom_command(TARGET tools POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:tools>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/tools.registry.json
        $<TARGET_FILE_DIR:tools>/config/tools.registry.json
    COMMENT "tools: staging default registry"
)

# ─── Install Rules ──────────────────────────────────────────────────────────

include(GNUInstallDirs)

install(TARGETS tools
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES config/tools.registry.json
    DESTINATION ${CMAKE_INSTALL_BINDIR}/config
)

message(STATUS "tools: configured (standalone dispatcher, no engine / forge / prism deps)")
