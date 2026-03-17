function(saga_create_engine_targets)
    add_library(SagaEngine STATIC)
    saga_apply_compiler_flags(SagaEngine)
    saga_link_thirdparty(SagaEngine)
    target_sources(SagaEngine PRIVATE
        ${CMAKE_SOURCE_DIR}/Engine/Core.cpp
        ${CMAKE_SOURCE_DIR}/Engine/Engine.cpp
        ${CMAKE_SOURCE_DIR}/Engine/World.cpp
    )

    add_library(SagaBackend STATIC)
    saga_apply_compiler_flags(SagaBackend)
    saga_link_thirdparty(SagaBackend)
    target_link_libraries(SagaBackend PRIVATE SagaEngine)
    target_sources(SagaBackend PRIVATE
        ${CMAKE_SOURCE_DIR}/Backends/Renderer.cpp
        ${CMAKE_SOURCE_DIR}/Backends/GraphicsDevice.cpp
    )

    add_library(SagaRuntime STATIC)
    saga_apply_compiler_flags(SagaRuntime)
    saga_link_thirdparty(SagaRuntime)
    target_link_libraries(SagaRuntime PRIVATE SagaEngine SagaBackend)
    target_sources(SagaRuntime PRIVATE
        ${CMAKE_SOURCE_DIR}/Runtime/Runtime.cpp
        ${CMAKE_SOURCE_DIR}/Runtime/Scene.cpp
    )

    if(SAGA_ENABLE_EDITOR)
        add_executable(SagaEditor)
        saga_apply_compiler_flags(SagaEditor)
        saga_link_thirdparty(SagaEditor)
        target_link_libraries(SagaEditor PRIVATE SagaRuntime)
        target_sources(SagaEditor PRIVATE
            ${CMAKE_SOURCE_DIR}/Editor/Main.cpp
            ${CMAKE_SOURCE_DIR}/Editor/EditorWindow.cpp
        )
    endif()

    if(SAGA_ENABLE_SERVER)
        add_executable(SagaServer)
        saga_apply_compiler_flags(SagaServer)
        saga_link_thirdparty(SagaServer)
        target_link_libraries(SagaServer PRIVATE SagaRuntime)
        target_sources(SagaServer PRIVATE
            ${CMAKE_SOURCE_DIR}/Server/Main.cpp
            ${CMAKE_SOURCE_DIR}/Server/Server.cpp
        )
    endif()

    add_executable(SagaApp)
    saga_apply_compiler_flags(SagaApp)
    saga_link_thirdparty(SagaApp)
    target_link_libraries(SagaApp PRIVATE SagaRuntime)
    target_sources(SagaApp PRIVATE
        ${CMAKE_SOURCE_DIR}/Main/Main.cpp
    )
endfunction()