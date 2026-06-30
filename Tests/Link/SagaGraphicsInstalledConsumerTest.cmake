if(NOT DEFINED SAGA_BUILD_DIR)
    message(FATAL_ERROR "SAGA_BUILD_DIR is required")
endif()
if(NOT DEFINED SAGA_INSTALL_PREFIX)
    message(FATAL_ERROR "SAGA_INSTALL_PREFIX is required")
endif()
if(NOT DEFINED SAGA_DOWNSTREAM_SOURCE_DIR)
    message(FATAL_ERROR "SAGA_DOWNSTREAM_SOURCE_DIR is required")
endif()
if(NOT DEFINED SAGA_DOWNSTREAM_BINARY_DIR)
    message(FATAL_ERROR "SAGA_DOWNSTREAM_BINARY_DIR is required")
endif()
if(NOT DEFINED SAGA_SOURCE_ROOT)
    message(FATAL_ERROR "SAGA_SOURCE_ROOT is required")
endif()

file(REMOVE_RECURSE "${SAGA_INSTALL_PREFIX}" "${SAGA_DOWNSTREAM_BINARY_DIR}")
file(MAKE_DIRECTORY "${SAGA_INSTALL_PREFIX}" "${SAGA_DOWNSTREAM_BINARY_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${SAGA_BUILD_DIR}"
            --prefix "${SAGA_INSTALL_PREFIX}"
            --component SagaDevelopment
    RESULT_VARIABLE _install_result
)
if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR "SagaEngine install failed with ${_install_result}")
endif()

set(_config
    "${SAGA_INSTALL_PREFIX}/lib/cmake/SagaEngine/SagaEngineConfig.cmake")
if(NOT EXISTS "${_config}")
    message(FATAL_ERROR "Installed SagaEngineConfig.cmake was not found")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -S "${SAGA_DOWNSTREAM_SOURCE_DIR}"
            -B "${SAGA_DOWNSTREAM_BINARY_DIR}"
            "-DCMAKE_PREFIX_PATH=${SAGA_INSTALL_PREFIX}"
            "-DSAGA_PUBLIC_SOURCE_INCLUDE_FORBIDDEN=${SAGA_SOURCE_ROOT}/Engine/Public"
            "-DSAGA_PRIVATE_SOURCE_INCLUDE_FORBIDDEN=${SAGA_SOURCE_ROOT}/Engine/Private"
    RESULT_VARIABLE _configure_result
)
if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR
        "Installed SagaGraphics downstream configure failed with "
        "${_configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${SAGA_DOWNSTREAM_BINARY_DIR}"
            --target SagaGraphicsInstalledConsumer
    RESULT_VARIABLE _build_result
)
if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR
        "Installed SagaGraphics downstream build failed with ${_build_result}")
endif()

execute_process(
    COMMAND "${SAGA_DOWNSTREAM_BINARY_DIR}/SagaGraphicsInstalledConsumer"
    RESULT_VARIABLE _run_result
)
if(NOT _run_result EQUAL 0)
    message(FATAL_ERROR
        "Installed SagaGraphics downstream executable failed with "
        "${_run_result}")
endif()
