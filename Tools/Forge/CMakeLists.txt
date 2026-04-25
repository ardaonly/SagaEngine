cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds prohibited")
endif()

project(SagaEngine VERSION 0.1.0 LANGUAGES CXX)

set(_dir "${CMAKE_SOURCE_DIR}")

while(NOT EXISTS "${_dir}/SagaEngineRoot.marker")
    cmake_path(GET _dir PARENT_PATH _parent)

    if(_dir STREQUAL _parent)
        message(FATAL_ERROR "Root bulunamadı")
    endif()

    set(_dir "${_parent}")
endwhile()

set(SAGA_ROOT "${_dir}")
message(STATUS "Engine root: ${SAGA_ROOT}")
set(SAGA_ROOT "${SAGA_ROOT}" CACHE PATH "Engine root directory" FORCE)

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