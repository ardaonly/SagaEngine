cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds prohibited")
endif()

project(SagaEngine VERSION 0.1.0 LANGUAGES CXX)

cmake_path(GET CMAKE_SOURCE_DIR PARENT_PATH SAGA_ROOT)
if(NOT EXISTS "${SAGA_ROOT}/SagaEngineRoot.marker")
    message(FATAL_ERROR "SagaEngineRoot.marker not found at: ${SAGA_ROOT}")
endif()
message(STATUS "Engine root: ${SAGA_ROOT}")
set(SAGA_ROOT "${SAGA_ROOT}" CACHE PATH "Engine root directory" FORCE)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(SagaVersioning)
include(SagaCompilerFlags)
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