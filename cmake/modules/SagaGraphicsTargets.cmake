function(saga_create_graphics_targets)
    # --- Graphics Public Shell ----------------------------------------------
    add_library(SagaGraphics INTERFACE)

    target_include_directories(SagaGraphics INTERFACE
        ${SAGA_ROOT}/Engine/Public
    )

    set_target_properties(SagaGraphics PROPERTIES
        FOLDER "Engine/Graphics"
    )

    # --- Graphics Private Shell ---------------------------------------------
    add_library(SagaGraphicsPrivate STATIC)
    saga_apply_compiler_flags(SagaGraphicsPrivate)

    target_sources(SagaGraphicsPrivate PRIVATE
        ${SAGA_ROOT}/Engine/Private/SagaEngine/Graphics/GraphicsPrivateAnchor.cpp
        ${SAGA_ROOT}/Engine/Private/SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.cpp
    )

    target_include_directories(SagaGraphicsPrivate PRIVATE
        ${SAGA_ROOT}/Engine/Private
    )

    target_link_libraries(SagaGraphicsPrivate PUBLIC
        SagaGraphics
    )
    target_link_libraries(SagaGraphicsPrivate PRIVATE
        SagaDiligentBackend
    )

    set_target_properties(SagaGraphicsPrivate PROPERTIES
        FOLDER "Engine/Graphics"
    )
endfunction()
