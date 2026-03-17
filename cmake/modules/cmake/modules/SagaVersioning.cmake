function(saga_init_versioning)
    find_package(Git QUIET)
    
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE SAGA_GIT_COMMIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE GIT_RESULT
        )
        
        if(GIT_RESULT EQUAL 0)
            set(SAGA_GIT_COMMIT "${SAGA_GIT_COMMIT}" CACHE INTERNAL "Git commit")
        endif()
        
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE SAGA_GIT_DESCRIBE
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(SAGA_GIT_DESCRIBE "${SAGA_GIT_DESCRIBE}" CACHE INTERNAL "Git describe")
    endif()
    
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/BUILD_VERSION")
        file(WRITE "${CMAKE_SOURCE_DIR}/BUILD_VERSION" "${SAGA_ENGINE_VERSION}")
    endif()
    
    file(READ "${CMAKE_SOURCE_DIR}/BUILD_VERSION" SAGA_ENGINE_VERSION)
    string(STRIP "${SAGA_ENGINE_VERSION}" SAGA_ENGINE_VERSION)
    
    set(SAGA_ENGINE_VERSION "${SAGA_ENGINE_VERSION}" CACHE INTERNAL "Engine version")
    set(SAGA_BUILD_TIMESTAMP "${CMAKE_SOURCE_DIR}" CACHE INTERNAL "Build timestamp")
    
    if(SAGA_BUILD_REPRODUCIBLE)
        set(SOURCE_DATE_EPOCH "0" CACHE INTERNAL "Reproducible build")
        set(CMAKE_AR_OPTIONS "D" CACHE INTERNAL "Deterministic archives")
    endif()
    
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/templates/Version.h.in"
        "${CMAKE_BINARY_DIR}/generated/SagaVersion.h"
        @ONLY
    )
    
    add_library(SagaVersion INTERFACE)
    target_include_directories(SagaVersion INTERFACE ${CMAKE_BINARY_DIR}/generated)
endfunction()