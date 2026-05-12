function(saga_apply_compiler_flags target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    
    if(SAGA_BUILD_REPRODUCIBLE)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(${target_name} PRIVATE /Brepro)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -fno-ident -ffile-prefix-map=${CMAKE_SOURCE_DIR}=/saga)
        endif()
    endif()
    
    target_compile_options(${target_name} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/MP>
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
        $<$<CXX_COMPILER_ID:MSVC>:/permissive->
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:GNU>:-Wall>
        $<$<CXX_COMPILER_ID:GNU>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU>:-Wpedantic>
        $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic -fdiagnostics-color=always>
    )

    target_compile_definitions(${target_name} PRIVATE
        $<$<CONFIG:Debug>:SAGA_DEBUG>
        $<$<CONFIG:Release>:SAGA_RELEASE>
        $<$<CONFIG:RelWithDebInfo>:SAGA_RELWITHDEBINFO>
        $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS>
        $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0601>
        SAGA_VERSION="${SAGA_ENGINE_VERSION}"
        SAGA_COMMIT="${SAGA_GIT_COMMIT}"
        SAGMA_BUILD_TIMESTAMP="${SAGA_BUILD_TIMESTAMP}"
    )

    if(SAGA_ENABLE_PROFILING)
        target_compile_definitions(${target_name} PRIVATE SAGA_PROFILING_ENABLED)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(${target_name} PRIVATE /PROFILE)
        endif()
    endif()
    
    if(SAGA_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
        include(CheckIPOSupported)
        check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
        if(lto_supported)
            set_property(TARGET ${target_name} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/Engine
        ${CMAKE_SOURCE_DIR}/Backends
        ${CMAKE_SOURCE_DIR}/Runtime
        ${CMAKE_BINARY_DIR}/generated
    )
endfunction()