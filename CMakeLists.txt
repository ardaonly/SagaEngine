cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not permitted. Run CMake from a separate build directory.")
endif()

file(READ "${CMAKE_SOURCE_DIR}/VERSION" SAGA_PROJECT_VERSION)
string(STRIP "${SAGA_PROJECT_VERSION}" SAGA_PROJECT_VERSION)
project(SagaEngine VERSION ${SAGA_PROJECT_VERSION} LANGUAGES CXX)

set(SAGA_ROOT "${CMAKE_SOURCE_DIR}" CACHE PATH "Engine root directory" FORCE)
message(STATUS "Engine root: ${SAGA_ROOT}")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

# ─── Optional, packaged tooling integrations ─────────────────────────────────
#
# SDE (Tools/SystemDefinitionEngine) is built and shipped as an independent
# library — it is not absorbed into the engine build tree. Set SAGA_WITH_SDE=ON
# to consume the packaged library through find_package; the matching Conan
# requirement is added by the engine's conanfile via -o &:with_sde=True.
#
option(SAGA_WITH_SDE "Link engine targets against the packaged SDE library" OFF)
option(SAGA_WITH_EDITORLAB_DEV_PANEL "Mount the dev-only EditorLab panel in Saga editor mode" OFF)

include(SagaVersioning)
include(SagaCompilerFlags)
include(SagaFindATL)
include(SagaThirdparty)
include(CTest)
include(SagaTargets)
include(SagaTests)
include(SagaInstall)
include(SagaAssets)
include(SagaDistribution)

saga_configure_executable_output_directory()
saga_init_versioning()
saga_setup_thirdparty()

if(SAGA_WITH_SDE)
    find_package(SDE CONFIG REQUIRED)
    message(STATUS "SDE integration: enabled (linking against SDE::Core)")
else()
    message(STATUS "SDE integration: disabled (set SAGA_WITH_SDE=ON to enable)")
endif()

saga_create_engine_targets()
saga_setup_tests()
saga_setup_assets()
saga_setup_install()
saga_setup_distribution()

message(STATUS "SagaEngine ${SAGA_ENGINE_VERSION}")
message(STATUS "Build: ${CMAKE_BUILD_TYPE}")
message(STATUS "Reproducible: ${SAGA_BUILD_REPRODUCIBLE}")
