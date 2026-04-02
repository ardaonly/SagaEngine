function(saga_init_versioning)
    set(SAGA_ENGINE_VERSION "${CMAKE_PROJECT_VERSION}" PARENT_SCOPE)

    find_package(Git QUIET)
    if(Git_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE SAGA_GIT_COMMIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    if(NOT SAGA_GIT_COMMIT)
        set(SAGA_GIT_COMMIT "unknown")
    endif()
    set(SAGA_GIT_COMMIT "${SAGA_GIT_COMMIT}" PARENT_SCOPE)

    string(TIMESTAMP SAGA_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)
    set(SAGA_BUILD_TIMESTAMP "${SAGA_BUILD_TIMESTAMP}" PARENT_SCOPE)

    option(SAGA_BUILD_REPRODUCIBLE "Enable reproducible builds" OFF)
    option(SAGA_ENABLE_PROFILING   "Enable profiling"           OFF)
    option(SAGA_ENABLE_LTO         "Enable LTO on Release"      OFF)
endfunction()