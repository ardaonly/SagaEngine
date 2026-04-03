macro(saga_collect_sources out_var dir)
    file(GLOB_RECURSE ${out_var} CONFIGURE_DEPENDS "${SAGA_ROOT}/${dir}/*.cpp")
endmacro()

function(saga_create_engine_targets)

    saga_collect_sources(ENGINE_SOURCES   Engine/src)
    saga_collect_sources(BACKEND_SOURCES  Backends/src)
    saga_collect_sources(RUNTIME_SOURCES  Runtime/src)
    saga_collect_sources(SERVER_SOURCES   Server/src)

    # main.cpp
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/[Mm]ain\\.cpp$")

    # Engine library - platform, ECS, render, simulation, networking
    add_library(SagaEngine STATIC)
    saga_apply_compiler_flags(SagaEngine)
    saga_link_thirdparty(SagaEngine)
    target_sources(SagaEngine PRIVATE
        ${ENGINE_SOURCES}
        ${RUNTIME_SOURCES}
        ${SERVER_SOURCES}
    )
    target_include_directories(SagaEngine PUBLIC
        ${SAGA_ROOT}/Engine/include
        ${SAGA_ROOT}/Runtime/include
        ${SAGA_ROOT}/Server/include
    )

    # Backend layer - persistence, veritabanı, services
    add_library(SagaBackend STATIC)
    saga_apply_compiler_flags(SagaBackend)
    saga_link_thirdparty(SagaBackend)
    target_link_libraries(SagaBackend PRIVATE SagaEngine)
    target_sources(SagaBackend PRIVATE ${BACKEND_SOURCES})
    target_include_directories(SagaBackend PUBLIC
        ${SAGA_ROOT}/Backends/include
    )

    # client, server, editor
    add_executable(SagaApp    ${SAGA_ROOT}/Apps/Client/main.cpp)
    add_executable(SagaServer ${SAGA_ROOT}/Apps/Server/main.cpp)
    add_executable(SagaEditor ${SAGA_ROOT}/Apps/Editor/main.cpp)

    foreach(app SagaApp SagaServer SagaEditor)
        saga_apply_compiler_flags(${app})
        saga_link_thirdparty(${app})
        target_link_libraries(${app} PRIVATE SagaEngine SagaBackend)
    endforeach()

endfunction()