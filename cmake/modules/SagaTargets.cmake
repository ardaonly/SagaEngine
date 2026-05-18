# ─── Tools/SagaForge/cmake/modules/SagaTargets.cmake ─────────────────────────

macro(saga_collect_sources out_var dir)
    file(GLOB_RECURSE ${out_var} CONFIGURE_DEPENDS
        "${SAGA_ROOT}/${dir}/*.cpp"
    )
endmacro()

function(saga_create_engine_targets)

    saga_collect_sources(SHARED_SOURCES Shared/src)
    saga_collect_sources(COLLABORATION_SOURCES Collaboration/src)
    saga_collect_sources(ASSET_PIPELINE_SOURCES Tools/AssetPipeline/src)
    saga_collect_sources(ENGINE_SOURCES  Engine/Private)
    saga_collect_sources(BACKEND_SOURCES Backends/src)
    saga_collect_sources(RUNTIME_SOURCES  Runtime/src)
    saga_collect_sources(SERVER_SOURCES   Server/src)
    saga_collect_sources(SANDBOX_SOURCES  Apps/Sandbox/src)
    saga_collect_sources(EDITOR_SOURCES   Editor/src)
    saga_collect_sources(EDITORLAB_SOURCES Apps/EditorLab/src)
    saga_collect_sources(SAGA_PRODUCT_SOURCES Apps/Saga)
    saga_collect_sources(CORE_LOG_SOURCES Engine/Private/SagaEngine/Core/Log)
    saga_collect_sources(DIAGNOSTICS_SOURCES Engine/Private/SagaEngine/Diagnostics)

    # Exclude launcher files from library sources.
    list(FILTER ENGINE_SOURCES  EXCLUDE REGEX ".*/[Mm]ain\\.cpp$")
    list(FILTER ENGINE_SOURCES  EXCLUDE REGEX ".*/Engine/Private/SagaEngine/Core/Log/.*\\.cpp$")
    list(FILTER ENGINE_SOURCES  EXCLUDE REGEX ".*/Engine/Private/SagaEngine/Diagnostics/.*\\.cpp$")
    list(FILTER SANDBOX_SOURCES EXCLUDE REGEX ".*/Launchers/.*\\.cpp$")
    # Editor/src/main.cpp is a legacy stub; Apps/Editor/main.cpp is the launcher.
    list(FILTER EDITOR_SOURCES  EXCLUDE REGEX ".*/[Mm]ain\\.cpp$")
    list(FILTER SAGA_PRODUCT_SOURCES EXCLUDE REGEX ".*/[Mm]ain\\.cpp$")
    list(FILTER SAGA_PRODUCT_SOURCES EXCLUDE REGEX ".*/SagaQtStaticPlugins\\.cpp$")

    # ─── Core Log Module ───────────────────────────────────────────────────
    add_library(SagaCoreLog STATIC)
    saga_apply_compiler_flags(SagaCoreLog)

    target_sources(SagaCoreLog PRIVATE
        ${CORE_LOG_SOURCES}
    )

    target_include_directories(SagaCoreLog
        PUBLIC
            ${SAGA_ROOT}/Engine/Public
        PRIVATE
            ${SAGA_ROOT}/Engine/Private
    )

    if(WIN32)
        target_link_libraries(SagaCoreLog PRIVATE Dbghelp)
    endif()

    set_target_properties(SagaCoreLog PROPERTIES
        FOLDER "Engine/Core"
    )

    # ─── Diagnostics Module ────────────────────────────────────────────────
    add_library(SagaDiagnostics STATIC)
    saga_apply_compiler_flags(SagaDiagnostics)

    target_sources(SagaDiagnostics PRIVATE
        ${DIAGNOSTICS_SOURCES}
    )

    target_include_directories(SagaDiagnostics
        PUBLIC
            ${SAGA_ROOT}/Engine/Public
        PRIVATE
            ${SAGA_ROOT}/Engine/Private
    )

    target_link_libraries(SagaDiagnostics PUBLIC
        SagaCoreLog
    )

    set_target_properties(SagaDiagnostics PROPERTIES
        FOLDER "Engine/Diagnostics"
    )

    # ─── Shared Contract Library ─────────────────────────────────────────────
    add_library(SagaShared STATIC)
    saga_apply_compiler_flags(SagaShared)

    target_sources(SagaShared PRIVATE
        ${SHARED_SOURCES}
    )

    target_include_directories(SagaShared PUBLIC
        ${SAGA_ROOT}/Shared/include
    )

    set_target_properties(SagaShared PROPERTIES
        FOLDER "Shared"
    )

    # ─── Collaboration Service Library ───────────────────────────────────────
    add_library(SagaCollaboration STATIC)
    saga_apply_compiler_flags(SagaCollaboration)

    target_sources(SagaCollaboration PRIVATE
        ${COLLABORATION_SOURCES}
    )

    target_include_directories(SagaCollaboration PUBLIC
        ${SAGA_ROOT}/Collaboration/include
    )

    target_link_libraries(SagaCollaboration PUBLIC
        SagaShared
    )

    set_target_properties(SagaCollaboration PROPERTIES
        FOLDER "Collaboration"
    )

    # ─── Asset Pipeline Tool Library ────────────────────────────────────────
    add_library(SagaAssetPipelineLib STATIC)
    saga_apply_compiler_flags(SagaAssetPipelineLib)

    target_sources(SagaAssetPipelineLib PRIVATE
        ${ASSET_PIPELINE_SOURCES}
    )

    target_include_directories(SagaAssetPipelineLib PUBLIC
        ${SAGA_ROOT}/Tools/AssetPipeline/include
    )

    set_target_properties(SagaAssetPipelineLib PROPERTIES
        FOLDER "Tools/AssetPipeline"
    )

    # ─── Engine Library ──────────────────────────────────────────────────────
    add_library(SagaEngine STATIC)
    saga_apply_compiler_flags(SagaEngine)
    saga_link_thirdparty(SagaEngine)

    target_sources(SagaEngine PRIVATE
        ${ENGINE_SOURCES}
    )

    target_include_directories(SagaEngine PUBLIC
        ${SAGA_ROOT}/Engine/Public
        ${SAGA_ROOT}/Runtime/include
    )

    target_include_directories(SagaEngine PRIVATE
        ${SAGA_ROOT}/Engine/Private
    )

    target_link_libraries(SagaEngine PUBLIC
        SagaCoreLog
        SagaShared
    )

    if(SAGA_WITH_SDE)
        target_link_libraries(SagaEngine PUBLIC SDE::Core)
        target_compile_definitions(SagaEngine PUBLIC SAGA_WITH_SDE=1)
    else()
        target_compile_definitions(SagaEngine PUBLIC SAGA_WITH_SDE=0)
    endif()

    # ─── Runtime Role Library ────────────────────────────────────────────────
    add_library(SagaRuntimeLib STATIC)
    saga_apply_compiler_flags(SagaRuntimeLib)
    saga_link_thirdparty(SagaRuntimeLib)

    target_sources(SagaRuntimeLib PRIVATE
        ${RUNTIME_SOURCES}
    )

    target_include_directories(SagaRuntimeLib PUBLIC
        ${SAGA_ROOT}/Runtime/include
    )

    target_link_libraries(SagaRuntimeLib PUBLIC
        SagaEngine
        SagaShared
        SagaCollaboration
    )

    set_target_properties(SagaRuntimeLib PROPERTIES
        FOLDER "Runtime"
    )

    # ─── Server Authority Library ────────────────────────────────────────────
    add_library(SagaServerLib STATIC)
    saga_apply_compiler_flags(SagaServerLib)
    saga_link_thirdparty(SagaServerLib)

    target_sources(SagaServerLib PRIVATE
        ${SERVER_SOURCES}
    )

    target_include_directories(SagaServerLib PUBLIC
        ${SAGA_ROOT}/Server/include
    )

    target_link_libraries(SagaServerLib PUBLIC
        SagaEngine
        SagaShared
        SagaCollaboration
    )

    set_target_properties(SagaServerLib PROPERTIES
        FOLDER "Server"
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
        SagaRuntimeLib
        SagaServerLib
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
        SagaShared
        SagaCollaboration
        SagaBackend
        nlohmann_json::nlohmann_json
    )

    if(SAGA_WITH_SDE)
        target_link_libraries(SagaEditorLib PRIVATE SDE::Core)
    endif()

    set_target_properties(SagaEditorLib PROPERTIES
        FOLDER "Editor"
    )

    # ─── EditorLab Development Library ───────────────────────────────────────
    # Shared by the standalone EditorLab executable and optional dev bridges.
    add_library(SagaEditorLabLib STATIC)
    saga_apply_compiler_flags(SagaEditorLabLib)

    target_sources(SagaEditorLabLib PRIVATE
        ${EDITORLAB_SOURCES}
    )

    target_include_directories(SagaEditorLabLib
        PUBLIC
            ${SAGA_ROOT}/Apps/EditorLab/include
        PRIVATE
            ${SAGA_ROOT}/Apps/EditorLab/src
    )

    target_link_libraries(SagaEditorLabLib PUBLIC
        SagaEditorLib
        Qt6::Core
        Qt6::Widgets
    )

    set_target_properties(SagaEditorLabLib PROPERTIES
        FOLDER "Apps/EditorLab"
    )

    # ─── Saga Product Orchestration Library ───────────────────────────────────
    if(SAGA_WITH_SDE)
        add_library(SagaProductLib STATIC)
        saga_apply_compiler_flags(SagaProductLib)
        saga_link_thirdparty(SagaProductLib)

        target_sources(SagaProductLib PRIVATE
            ${SAGA_PRODUCT_SOURCES}
        )

        target_include_directories(SagaProductLib PUBLIC
            ${SAGA_ROOT}/Apps/Saga
        )

        target_link_libraries(SagaProductLib PUBLIC
            SagaEngine
            SagaShared
            SagaCollaboration
            SagaRuntimeLib
            SagaServerLib
            Qt6::Core
            Qt6::Widgets
            SDE::Core
        )

        target_link_libraries(SagaProductLib PRIVATE
            SagaBackend
            SagaEditorLib
        )

        target_compile_definitions(SagaProductLib PUBLIC
            SAGA_PRODUCT_VERSION="${CMAKE_PROJECT_VERSION}"
            SAGA_PRODUCT_GIT_COMMIT="${SAGA_GIT_COMMIT}"
            SAGA_PRODUCT_PLATFORM="${CMAKE_SYSTEM_NAME}"
            SAGA_BUILTIN_BASIC_WORKSPACE_ROOT="${SAGA_ROOT}/Apps/Saga/Definitions/BasicWorkspace"
        )

        set_target_properties(SagaProductLib PROPERTIES
            FOLDER "Apps/Saga"
        )

        if(SAGA_WITH_EDITORLAB_DEV_PANEL)
            add_library(SagaEditorLabBridge STATIC
                ${SAGA_ROOT}/Apps/SagaDev/SagaEditorLabBridge.h
                ${SAGA_ROOT}/Apps/SagaDev/SagaEditorLabBridge.cpp
            )
            saga_apply_compiler_flags(SagaEditorLabBridge)
            saga_link_thirdparty(SagaEditorLabBridge)
            target_include_directories(SagaEditorLabBridge PUBLIC
                ${SAGA_ROOT}/Apps/SagaDev
            )
            target_link_libraries(SagaEditorLabBridge PUBLIC
                SagaProductLib
                SagaEditorLabLib
            )
            set_target_properties(SagaEditorLabBridge PROPERTIES
                FOLDER "Apps/SagaDev"
            )
        endif()
    endif()

    # ─── Application Executables ──────────────────────────────────────────────
    if(SAGA_WITH_SDE)
        qt_add_executable(Saga WIN32
            ${SAGA_ROOT}/Apps/Saga/main.cpp
            ${SAGA_ROOT}/Apps/Saga/SagaQtStaticPlugins.cpp
        )

        saga_apply_compiler_flags(Saga)
        saga_link_thirdparty(Saga)
        target_link_libraries(Saga PRIVATE SagaProductLib)
        if(SAGA_WITH_EDITORLAB_DEV_PANEL)
            target_link_libraries(Saga PRIVATE SagaEditorLabBridge)
            target_compile_definitions(Saga PRIVATE SAGA_WITH_EDITORLAB_DEV_PANEL=1)
        else()
            target_compile_definitions(Saga PRIVATE SAGA_WITH_EDITORLAB_DEV_PANEL=0)
        endif()
        if(TARGET qt::qt)
            target_link_libraries(Saga PRIVATE qt::qt)
        elseif(TARGET Qt6::QXcbIntegrationPlugin)
            target_link_libraries(Saga PRIVATE Qt6::QXcbIntegrationPlugin)
        endif()
        if(COMMAND qt_import_plugins AND TARGET Qt6::QXcbIntegrationPlugin)
            qt_import_plugins(Saga INCLUDE Qt6::QXcbIntegrationPlugin)
        endif()

        set_target_properties(Saga PROPERTIES
            OUTPUT_NAME "Saga"
            FOLDER      "Apps"
        )
    else()
        add_custom_target(Saga
            COMMAND ${CMAKE_COMMAND} -E echo
                    "Saga requires SAGA_WITH_SDE=ON because SDE is the mandatory workspace contract."
            COMMAND ${CMAKE_COMMAND} -E false
            COMMENT "Saga product target requires SDE"
            VERBATIM
        )
    endif()

    # SagaRuntime is the shipped runtime/player role. For v0.0.8 it reuses the
    # client host through a temporary adapter entry point; the long-term split is
    # Runtime Core + optional client UI.
    set(SAGA_RUNTIME_COMMON_SOURCES
        ${SAGA_ROOT}/Apps/Client/ClientHost.h
        ${SAGA_ROOT}/Apps/Client/ClientHost.cpp
    )

    add_executable(SagaRuntime
        ${SAGA_ROOT}/Apps/Runtime/main.cpp
        ${SAGA_RUNTIME_COMMON_SOURCES}
    )

    target_include_directories(SagaRuntime PRIVATE
        ${SAGA_ROOT}/Apps/Client
    )

    target_compile_definitions(SagaRuntime PRIVATE SDL_MAIN_HANDLED)

    saga_apply_compiler_flags(SagaRuntime)
    saga_link_thirdparty(SagaRuntime)
    target_link_libraries(SagaRuntime PRIVATE
        SagaRuntimeLib
        SagaServerLib
        SagaEngine
        SagaBackend
    )

    set_target_properties(SagaRuntime PROPERTIES
        OUTPUT_NAME "SagaRuntime"
        FOLDER      "Apps"
    )

    # Legacy/dev-only client binary. It is intentionally excluded from the
    # SAGA production distribution so product identity stays role-based.
    set(SAGA_CLIENT_SOURCES
        ${SAGA_ROOT}/Apps/Client/main.cpp
        ${SAGA_RUNTIME_COMMON_SOURCES}
    )

    add_executable(SagaApp ${SAGA_CLIENT_SOURCES})

    target_include_directories(SagaApp PRIVATE
        ${SAGA_ROOT}/Apps/Client
    )

    target_compile_definitions(SagaApp PRIVATE SDL_MAIN_HANDLED)

    saga_apply_compiler_flags(SagaApp)
    saga_link_thirdparty(SagaApp)
    target_link_libraries(SagaApp PRIVATE
        SagaRuntimeLib
        SagaServerLib
        SagaEngine
        SagaBackend
    )

    set_target_properties(SagaApp PROPERTIES
        OUTPUT_NAME "SagaClient"
        FOLDER      "Dev/Legacy"
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
    target_link_libraries(SagaServer PRIVATE SagaServerLib SagaEngine SagaBackend)

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

    # ─── EditorLab Executable ────────────────────────────────────────────────
    # Development-only lab surface for deterministic editor scenario experiments.
    qt_add_executable(EditorLab WIN32
        ${SAGA_ROOT}/Apps/EditorLab/main.cpp
        ${SAGA_ROOT}/Apps/EditorLab/EditorLabQtStaticPlugins.cpp
    )

    target_include_directories(EditorLab PRIVATE
        ${SAGA_ROOT}/Apps/EditorLab/include
        ${SAGA_ROOT}/Apps/EditorLab/src
        ${SAGA_ROOT}/Editor/include
    )

    saga_apply_compiler_flags(EditorLab)

    target_link_libraries(EditorLab PRIVATE
        SagaEditorLabLib
        Qt6::Core
        Qt6::Widgets
    )
    if(TARGET qt::qt)
        target_link_libraries(EditorLab PRIVATE qt::qt)
    elseif(TARGET Qt6::QXcbIntegrationPlugin)
        target_link_libraries(EditorLab PRIVATE Qt6::QXcbIntegrationPlugin)
    endif()
    if(COMMAND qt_import_plugins AND TARGET Qt6::QXcbIntegrationPlugin)
        qt_import_plugins(EditorLab INCLUDE Qt6::QXcbIntegrationPlugin)
    endif()

    set_target_properties(EditorLab PROPERTIES
        OUTPUT_NAME "EditorLab"
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
            SagaRuntimeLib
            SagaServerLib
            SagaEngine
            SagaBackend
        )
        set_target_properties(RenderClientSmokeTest PROPERTIES
            FOLDER "Apps"
        )
        add_test(NAME RenderClientSmokeTest COMMAND RenderClientSmokeTest)
    endif()

endfunction()
