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

option(SAGA_WITH_EDITORLAB_DEV_PANEL "Mount the dev-only EditorLab panel in SagaEditor" OFF)

include(SagaVersioning)
include(SagaCompilerFlags)
include(SagaFindATL)
include(SagaThirdparty)
include(SagaDiligentVendor)
include(CTest)
include(SagaGraphicsTargets)
include(SagaTargets)
include(SagaTests)
include(SagaInstallGraphics)
include(SagaInstall)
include(SagaAssets)
include(SagaDistribution)
include(SagaSampleEvidence)

saga_configure_executable_output_directory()
saga_init_versioning()
saga_setup_thirdparty()
saga_setup_diligent_vendor()

saga_create_graphics_targets()
saga_create_engine_targets()
saga_setup_tests()
saga_setup_assets()
saga_setup_install()
saga_setup_distribution()
saga_setup_sample_executable_evidence()
saga_assert_diligent_graphics_backend_single_owner()

message(STATUS "SagaEngine ${SAGA_ENGINE_VERSION}")
message(STATUS "Build: ${CMAKE_BUILD_TYPE}")
message(STATUS "Reproducible: ${SAGA_BUILD_REPRODUCIBLE}")
