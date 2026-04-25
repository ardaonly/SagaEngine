# ─── Tools/SagaForge/cmake/modules/SagaTargets.cmake ─────────────────────────

macro(saga_collect_sources out_var dir)
    file(GLOB_RECURSE ${out_var} CONFIGURE_DEPENDS
        "${SAGA_ROOT}/${dir}/*.cpp"
    )
endmacro()

function(saga_create_engine_targets)

    saga_collect_sources(ENGINE_SOURCES  Engine/src)
    saga_collect_sources(BACKEND_SOURCES Backends/src)
    saga_collect_sources(RUNTIME_SOURCES  Runtime/src)
    saga_collect_sources(SERVER_SOURCES   Server/src)
    saga_collect_sources(SANDBOX_SOURCES  Apps/Sandbox/src)
    saga_collect_sources(EDITOR_SOURCES   Editor/src)

    # Launcher dosyalarını library kaynaklarından çıkar
    list(FILTER ENGINE_SOURCES  EXCLUDE REGEX ".*/[Mm]ain\\.cpp$")
    list(FILTER SANDBOX_SOURCES EXCLUDE REGEX ".*/Launchers/.*\\.cpp$")
    # Editor/src/main.cpp (eski stub) library'ye girmesin; Apps/Editor/main.cpp launcher'dır.
    list(FILTER EDITOR_SOURCES  EXCLUDE REGEX ".*/[Mm]ain\\.cpp$")

    # ─── Engine Library ──────────────────────────────────────────────────────
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

    # ─── Backend Library ─────────────────────────────────────────────────────
    add_library(SagaBackend STATIC)
    saga_apply_compiler_flags(SagaBackend)
    saga_link_thirdparty(SagaBackend)

    target_link_libraries(SagaBackend PRIVATE SagaEngine)

    target_sources(SagaBackend PRIVATE
        ${BACKEND_SOURCES}
    )

    target_include_directories(SagaBackend PUBLIC
        ${SAGA_ROOT}/Backends/include
    )

    # ─── Sandbox Library ──────────────────────────────────────────────────────
    add_library(SagaSandboxLib STATIC)
    saga_apply_compiler_flags(SagaSandboxLib)
    saga_link_thirdparty(SagaSandboxLib)

    target_link_libraries(SagaSandboxLib PUBLIC
        SagaEngine
        SagaBackend
    )

    target_sources(SagaSandboxLib PRIVATE
        ${SANDBOX_SOURCES}
    )

    target_include_directories(SagaSandboxLib PUBLIC
        ${SAGA_ROOT}/Apps/Sandbox/include
    )

    target_include_directories(SagaSandboxLib PRIVATE
        ${SAGA_ROOT}/Apps/Sandbox/src
    )

    # ─── Editor Library ───────────────────────────────────────────────────────
    # All editor logic lives in Editor/. Apps/Editor/main.cpp is the thin launcher.
    # Qt handles the WinMain shim on Windows via qt_add_executable(WIN32).
    add_library(SagaEditorLib STATIC)
    saga_apply_compiler_flags(SagaEditorLib)

    target_sources(SagaEditorLib PRIVATE
        ${EDITOR_SOURCES}
    )

    target_include_directories(SagaEditorLib PUBLIC
        ${SAGA_ROOT}/Editor/include
    )

    # Qt is already pulled in via saga_link_thirdparty, but SagaEditorLib needs
    # qt_standard_project_setup() to have run (done in the root CMakeLists).
    target_link_libraries(SagaEditorLib PUBLIC
        Qt6::Core
        Qt6::Widgets
    )

    target_link_libraries(SagaEditorLib PRIVATE
        SagaEngine
        SagaBackend
    )

    set_target_properties(SagaEditorLib PROPERTIES
        FOLDER "Editor"
    )

    # ─── Uygulama Executable'ları ─────────────────────────────────────────────
    # Client app sources: main.cpp + ClientHost (ClientNetworkSession is inline in .cpp)
    set(SAGA_CLIENT_SOURCES
        ${SAGA_ROOT}/Apps/Client/main.cpp
        ${SAGA_ROOT}/Apps/Client/ClientHost.h
        ${SAGA_ROOT}/Apps/Client/ClientHost.cpp
    )

    add_executable(SagaApp ${SAGA_CLIENT_SOURCES})

    target_include_directories(SagaApp PRIVATE
        ${SAGA_ROOT}/Apps/Client
    )

    target_compile_definitions(SagaApp PRIVATE SDL_MAIN_HANDLED)

    saga_apply_compiler_flags(SagaApp)
    saga_link_thirdparty(SagaApp)
    target_link_libraries(SagaApp PRIVATE
        SagaEngine
        SagaBackend
    )

    set_target_properties(SagaApp PROPERTIES
        OUTPUT_NAME "SagaClient"
        FOLDER      "Apps"
    )

    add_executable(SagaServer
        ${SAGA_ROOT}/Apps/Server/main.cpp
        ${SAGA_ROOT}/Apps/Server/TestSnapshotServer.h
        ${SAGA_ROOT}/Apps/Server/TestSnapshotServer.cpp
    )

    target_include_directories(SagaServer PRIVATE
        ${SAGA_ROOT}/Apps/Server
    )

    saga_apply_compiler_flags(SagaServer)
    saga_link_thirdparty(SagaServer)
    target_link_libraries(SagaServer PRIVATE SagaEngine SagaBackend)

    # ─── SagaEditor Executable ────────────────────────────────────────────────
    # qt_add_executable with WIN32 generates the WinMain shim on Windows so
    # Apps/Editor/main.cpp can use a standard int main() on all platforms.
    qt_add_executable(SagaEditor WIN32
        ${SAGA_ROOT}/Apps/Editor/main.cpp
    )

    target_include_directories(SagaEditor PRIVATE
        ${SAGA_ROOT}/Editor/include   # access SagaEditor/App/* headers
    )

    saga_apply_compiler_flags(SagaEditor)

    target_link_libraries(SagaEditor PRIVATE
        SagaEditorLib   # all editor logic + transitive Qt6::Widgets
    )

    set_target_properties(SagaEditor PROPERTIES
        OUTPUT_NAME "SagaEditor"
        FOLDER      "Apps"
    )

    # ─── Sandbox Executable ───────────────────────────────────────────────────
    set(_saga_sandbox_main "${SAGA_ROOT}/Apps/Sandbox/Launchers/SandboxMain.cpp")

    if(NOT EXISTS "${_saga_sandbox_main}")
        message(FATAL_ERROR "SagaSandbox launcher not found: ${_saga_sandbox_main}")
    endif()

    add_executable(SagaSandbox "${_saga_sandbox_main}")

    target_compile_definitions(SagaSandbox PRIVATE SDL_MAIN_HANDLED)

    get_target_property(_saga_sandbox_sources_before SagaSandbox SOURCES)
    message(STATUS "[SagaSandbox] SOURCES before helpers = ${_saga_sandbox_sources_before}")
    message(STATUS "[SagaSandbox] main path = ${_saga_sandbox_main}")
    message(STATUS "[SagaSandbox] exists = $<BOOL:${_saga_sandbox_main}>")

    saga_apply_compiler_flags(SagaSandbox)
    saga_link_thirdparty(SagaSandbox)

    get_target_property(_saga_sandbox_sources_after_helpers SagaSandbox SOURCES)
    message(STATUS "[SagaSandbox] SOURCES after helpers = ${_saga_sandbox_sources_after_helpers}")

    # WHOLE_ARCHIVE ensures self-registering scenario statics are not
    # stripped by the linker (MSVC discards unreferenced .obj from STATIC libs).
    target_link_libraries(SagaSandbox PRIVATE
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,SagaSandboxLib>"
    )

    get_target_property(_saga_sandbox_sources_final SagaSandbox SOURCES)
    message(STATUS "[SagaSandbox] SOURCES final = ${_saga_sandbox_sources_final}")

    set_target_properties(SagaSandbox PROPERTIES
        OUTPUT_NAME "SagaSandbox"
        FOLDER      "Apps"
    )

    # ─── RenderClientSmokeTest ───────────────────────────────────────────
    # Headless, CI-safe smoke test for the RenderClientApp facade.
    # No GPU, no window — exercises bind/update/release and multi-camera slots
    # against NullRenderBackend.
    set(_saga_render_smoke "${SAGA_ROOT}/Apps/RenderClientSmokeTest/main.cpp")
    if(EXISTS "${_saga_render_smoke}")
        add_executable(RenderClientSmokeTest "${_saga_render_smoke}")
        saga_apply_compiler_flags(RenderClientSmokeTest)
        saga_link_thirdparty(RenderClientSmokeTest)
        target_link_libraries(RenderClientSmokeTest PRIVATE
            SagaEngine
            SagaBackend
        )
        set_target_properties(RenderClientSmokeTest PROPERTIES
            FOLDER "Apps"
        )
        add_test(NAME RenderClientSmokeTest COMMAND RenderClientSmokeTest)
    endif()

endfunction()