cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not permitted. Run CMake from a separate build directory.")
endif()

project(SagaEngine VERSION 0.1.0 LANGUAGES CXX)

set(SAGA_ROOT "${CMAKE_SOURCE_DIR}" CACHE PATH "Engine root directory" FORCE)
message(STATUS "Engine root: ${SAGA_ROOT}")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")



include(SagaVersioning)
include(SagaCompilerFlags)
include(SagaFindATL)
include(SagaThirdparty)
include(SagaTargets)
include(SagaTests)
include(SagaInstall)
include(SagaAssets)

saga_init_versioning()
saga_setup_thirdparty()



saga_create_engine_targets()
saga_setup_tests()
saga_setup_assets()
saga_setup_install()

message(STATUS "SagaEngine ${SAGA_ENGINE_VERSION}")
message(STATUS "Build: ${CMAKE_BUILD_TYPE}")
message(STATUS "Reproducible: ${SAGA_BUILD_REPRODUCIBLE}")
