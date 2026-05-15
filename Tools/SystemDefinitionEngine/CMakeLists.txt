# ─── SDE — System Definition Engine ──────────────────────────────────────────
#
# SDE is a standalone, repo-extractable C++ static library:
#
#   * No SagaEngine, SagaEditor, or SagaServer headers are included anywhere
#     under this directory. Verified by Tools/Scripts/check_tools_isolation.py.
#   * No Saga*.cmake module is consumed; this file describes its own targets.
#   * The only third-party dependency is nlohmann_json. Ships through Conan
#     (see conanfile.py) or any equivalent find_package source.
#
# Two consumption paths are supported:
#
#   (A) Conan package
#       conan create Tools/SystemDefinitionEngine
#       # downstream:
#       self.requires("sde/0.1.1")
#       find_package(SDE CONFIG REQUIRED)
#       target_link_libraries(<tgt> PRIVATE SDE::Core)
#
#   (B) Standalone install
#       cmake -S Tools/SystemDefinitionEngine -B build/sde -DCMAKE_INSTALL_PREFIX=<prefix>
#       cmake --build build/sde --target install
#       # downstream uses find_package(SDE CONFIG REQUIRED) with -DSDE_DIR or
#       # CMAKE_PREFIX_PATH pointing at <prefix>/lib/cmake/SDE.
#
cmake_minimum_required(VERSION 3.22)

project(SDE
    VERSION     0.1.1
    LANGUAGES   CXX
    DESCRIPTION "Model definition, validation, and compilation pipeline"
)

# ─── Toolchain defaults ──────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Detect whether SDE is the top-level project. Some defaults differ in that
# case (tests on by default when developing standalone, off when consumed).
set(SDE_IS_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(SDE_IS_TOP_LEVEL ON)
endif()

# ─── Options ─────────────────────────────────────────────────────────────────

option(SDE_BUILD_CLI   "Build the sde CLI binary"          ON)
option(SDE_BUILD_TESTS "Build the SDE test suite"          ${SDE_IS_TOP_LEVEL})
option(SDE_INSTALL     "Generate install/export rules"     ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ─── Third-party ─────────────────────────────────────────────────────────────

find_package(nlohmann_json CONFIG REQUIRED)

# ─── SDE static library ──────────────────────────────────────────────────────

file(GLOB_RECURSE SDE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SDE/*.cpp"
)

add_library(SDE STATIC ${SDE_SOURCES})
add_library(SDE::Core ALIAS SDE)
add_library(SDE::SDE ALIAS SDE) # Pre-1.0 compatibility alias.

set_target_properties(SDE PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER                    "Tools"
    EXPORT_NAME               Core
    VERSION                   ${PROJECT_VERSION}
    SOVERSION                 ${PROJECT_VERSION_MAJOR}
)

target_compile_features(SDE PUBLIC cxx_std_20)

target_include_directories(SDE
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(SDE PRIVATE nlohmann_json::nlohmann_json)

if(MSVC)
    target_compile_options(SDE PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(SDE PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ─── sde CLI binary ──────────────────────────────────────────────────────────

if(SDE_BUILD_CLI)
    add_executable(sde-cli "${CMAKE_CURRENT_SOURCE_DIR}/cli/main.cpp")
    set_target_properties(sde-cli PROPERTIES
        OUTPUT_NAME "sde"
        FOLDER      "Tools"
    )
    target_link_libraries(sde-cli PRIVATE
        SDE::Core
        nlohmann_json::nlohmann_json
    )
    if(MSVC)
        target_compile_options(sde-cli PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(sde-cli PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# ─── Tests ───────────────────────────────────────────────────────────────────

if(SDE_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    file(GLOB_RECURSE SDE_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    if(SDE_TEST_SOURCES)
        add_executable(SDETests ${SDE_TEST_SOURCES})
        target_link_libraries(SDETests PRIVATE
            SDE::Core
            GTest::gtest
            GTest::gtest_main
        )
        target_include_directories(SDETests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests"
        )
        set_target_properties(SDETests PROPERTIES FOLDER "Tools")
        add_test(NAME SDETests COMMAND SDETests)
    endif()
endif()

# ─── Install / export ────────────────────────────────────────────────────────

if(SDE_INSTALL)
    set(SDE_CMAKE_CONFIG_DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/SDE")

    install(TARGETS SDE
        EXPORT  SDETargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/SDE"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h"
            PATTERN "CompilerSession.h" EXCLUDE
    )

    if(SDE_BUILD_CLI)
        install(TARGETS sde-cli
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    install(EXPORT SDETargets
        FILE        SDETargets.cmake
        NAMESPACE   SDE::
        DESTINATION ${SDE_CMAKE_CONFIG_DESTINATION}
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SDEConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/SDEConfig.cmake"
        INSTALL_DESTINATION "${SDE_CMAKE_CONFIG_DESTINATION}"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/SDEConfigVersion.cmake"
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/SDEConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/SDEConfigVersion.cmake"
        DESTINATION ${SDE_CMAKE_CONFIG_DESTINATION}
    )

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/SDE"
        )
    endif()
endif()

message(STATUS "SDE ${PROJECT_VERSION}: configured (standalone library)")
