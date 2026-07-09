function(saga_get_graphics_core_sources out_var)
    set(${out_var}
        "${SAGA_ROOT}/Engine/Private/SagaEngine/Graphics/Backend/NullGraphicsBackend.cpp"
        "${SAGA_ROOT}/Engine/Private/SagaEngine/Graphics/Bindings/GraphicsBindingValidation.cpp"
        PARENT_SCOPE
    )
endfunction()

function(saga_create_graphics_targets)
    saga_get_graphics_core_sources(SAGA_GRAPHICS_CORE_SOURCES)

    # --- Graphics Core Implementation ---------------------------------------
    add_library(SagaGraphicsCore STATIC)
    saga_apply_compiler_flags(SagaGraphicsCore)

    target_sources(SagaGraphicsCore PRIVATE
        ${SAGA_GRAPHICS_CORE_SOURCES}
    )

    target_include_directories(SagaGraphicsCore PUBLIC
        $<BUILD_INTERFACE:${SAGA_ROOT}/Engine/Public>
        $<INSTALL_INTERFACE:include>
    )

    set_target_properties(SagaGraphicsCore PROPERTIES
        FOLDER "Engine/Graphics"
    )

    # --- Graphics Public Shell ----------------------------------------------
    add_library(SagaGraphics INTERFACE)

    target_include_directories(SagaGraphics INTERFACE
        $<BUILD_INTERFACE:${SAGA_ROOT}/Engine/Public>
        $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(SagaGraphics INTERFACE
        SagaGraphicsCore
    )

    set_target_properties(SagaGraphics PROPERTIES
        FOLDER "Engine/Graphics"
    )

    # --- Graphics Private Shell ---------------------------------------------
    add_library(SagaGraphicsPrivate STATIC)
    saga_apply_compiler_flags(SagaGraphicsPrivate)

    target_sources(SagaGraphicsPrivate PRIVATE
        ${SAGA_ROOT}/Engine/Private/SagaEngine/Graphics/GraphicsPrivateAnchor.cpp
    )

    target_include_directories(SagaGraphicsPrivate PRIVATE
        ${SAGA_ROOT}/Engine/Private
    )

    target_link_libraries(SagaGraphicsPrivate PUBLIC
        SagaGraphics
    )
    target_link_libraries(SagaGraphicsPrivate PRIVATE
        SagaDiligentRuntime
    )

    set_target_properties(SagaGraphicsPrivate PROPERTIES
        FOLDER "Engine/Graphics"
    )
endfunction()

function(saga_assert_source_has_single_owner source expected_owner)
    get_property(_saga_buildsystem_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    get_filename_component(_saga_source_abs
        "${source}" ABSOLUTE BASE_DIR "${SAGA_ROOT}")
    set(_saga_source_owners)

    foreach(_saga_target IN LISTS _saga_buildsystem_targets)
        get_target_property(_saga_target_sources ${_saga_target} SOURCES)
        if(NOT _saga_target_sources)
            continue()
        endif()

        foreach(_saga_target_source IN LISTS _saga_target_sources)
            if(_saga_target_source MATCHES "^\\$<")
                continue()
            endif()

            get_filename_component(_saga_target_source_abs
                "${_saga_target_source}" ABSOLUTE BASE_DIR "${SAGA_ROOT}")
            if(_saga_target_source_abs STREQUAL _saga_source_abs)
                list(APPEND _saga_source_owners ${_saga_target})
            endif()
        endforeach()
    endforeach()

    list(LENGTH _saga_source_owners _saga_owner_count)
    if(NOT _saga_owner_count EQUAL 1 OR
       NOT _saga_source_owners STREQUAL "${expected_owner}")
        message(FATAL_ERROR
            "Source must have exactly one owner (${expected_owner}): "
            "${_saga_source_abs}; owners=${_saga_source_owners}")
    endif()
endfunction()

function(saga_assert_diligent_graphics_backend_single_owner)
    saga_get_graphics_core_sources(_saga_graphics_core_sources)
    foreach(_saga_graphics_core_source IN LISTS _saga_graphics_core_sources)
        saga_assert_source_has_single_owner(
            "${_saga_graphics_core_source}"
            SagaGraphicsCore)
    endforeach()

    file(GLOB_RECURSE _saga_diligent_graphics_backend_impl_sources CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Engine/Private/SagaEngine/Graphics/Backends/Diligent/*.cpp"
    )

    foreach(_saga_diligent_graphics_source IN LISTS _saga_diligent_graphics_backend_impl_sources)
        saga_assert_source_has_single_owner(
            "${_saga_diligent_graphics_source}"
            SagaDiligentRuntime)
    endforeach()
endfunction()
