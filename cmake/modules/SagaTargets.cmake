# --- Tools/SagaForge/cmake/modules/SagaTargets.cmake -------------------------

macro(saga_collect_sources out_var dir)
    file(GLOB_RECURSE ${out_var} CONFIGURE_DEPENDS
        "${SAGA_ROOT}/${dir}/*.cpp"
    )
endmacro()

macro(saga_configure_executable_output_directory)
    set(SAGA_EXECUTABLE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin"
        CACHE PATH "Directory for CMake-built executable outputs" FORCE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SAGA_EXECUTABLE_OUTPUT_DIR}")
    foreach(_saga_output_config IN ITEMS
        DEBUG
        RELEASE
        RELWITHDEBINFO
        MINSIZEREL
    )
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_saga_output_config}
            "${SAGA_EXECUTABLE_OUTPUT_DIR}")
    endforeach()
endmacro()

function(saga_create_engine_targets)

    saga_collect_sources(SHARED_SOURCES Shared/src)
    saga_collect_sources(COLLABORATION_SOURCES Collaboration/src)
    saga_collect_sources(ASSET_PIPELINE_SOURCES Tools/AssetPipeline/src)
    saga_collect_sources(SAGA_EDITOR_COMPOSITION_SOURCES Tools/SagaEditorComposition/src)
    saga_collect_sources(SAGA_PIPELINE_SOURCES Tools/SagaPipeline/src)
    saga_collect_sources(SAGA_STRESS_ARENA_SOURCES Tools/SagaStressArena/src)
    saga_collect_sources(SAGA_CHAOS_LAB_SOURCES Tools/SagaChaosLab/src)
    saga_collect_sources(SAGA_STATE_CHECK_SOURCES Tools/SagaStateCheck/src)
    saga_collect_sources(MULTIPLAYER_SANDBOX_HEADLESS_SOURCES Tools/MultiplayerSandboxHeadless/src)
    saga_collect_sources(ENGINE_SOURCES  Engine/Private)
    saga_collect_sources(DILIGENT_BACKEND_SOURCES Engine/Private/SagaEngine/Render/Backend/Diligent)
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
    list(FILTER ENGINE_SOURCES  EXCLUDE REGEX ".*/Engine/Private/SagaEngine/Render/Backend/Diligent/.*\\.cpp$")
    list(FILTER SANDBOX_SOURCES EXCLUDE REGEX ".*/Launchers/.*\\.cpp$")
    # Editor/src/main.cpp is a legacy stub; Apps/Editor/main.cpp is the launcher.
    list(FILTER EDITOR_SOURCES  EXCLUDE REGEX ".*/[Mm]ain\\.cpp$")
    list(FILTER SAGA_PRODUCT_SOURCES EXCLUDE REGEX ".*/[Mm]ain\\.cpp$")
    list(FILTER SAGA_PRODUCT_SOURCES EXCLUDE REGEX ".*/SagaQtStaticPlugins\\.cpp$")

    # --- Core Log Module ---------------------------------------------------
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

    # --- Diagnostics Module ------------------------------------------------
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
    target_link_libraries(SagaDiagnostics PRIVATE
        nlohmann_json::nlohmann_json
    )

    set_target_properties(SagaDiagnostics PROPERTIES
        FOLDER "Engine/Diagnostics"
    )

    # --- Shared Contract Library ---------------------------------------------
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

    # --- Collaboration Service Library ---------------------------------------
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
    target_link_libraries(SagaCollaboration PRIVATE
        nlohmann_json::nlohmann_json
    )

    set_target_properties(SagaCollaboration PROPERTIES
        FOLDER "Collaboration"
    )

    # --- Asset Pipeline Tool Library ----------------------------------------
    add_library(SagaAssetPipelineLib STATIC)
    saga_apply_compiler_flags(SagaAssetPipelineLib)

    target_sources(SagaAssetPipelineLib PRIVATE
        ${ASSET_PIPELINE_SOURCES}
    )

    target_include_directories(SagaAssetPipelineLib PUBLIC
        ${SAGA_ROOT}/Tools/AssetPipeline/include
    )

    target_link_libraries(SagaAssetPipelineLib PRIVATE
        nlohmann_json::nlohmann_json
    )

    set_target_properties(SagaAssetPipelineLib PROPERTIES
        FOLDER "Tools/AssetPipeline"
    )

    # --- Saga Pipeline Tool --------------------------------------------------
    # Saga-specific workflow orchestration lives here so Forge, Prism, SDE, and
    # SagaTools can remain generic or thin.
    add_library(SagaPipelineLib STATIC)
    saga_apply_compiler_flags(SagaPipelineLib)

    target_sources(SagaPipelineLib PRIVATE
        ${SAGA_PIPELINE_SOURCES}
    )

    target_include_directories(SagaPipelineLib PUBLIC
        ${SAGA_ROOT}/Tools/SagaPipeline/include
    )

    set_target_properties(SagaPipelineLib PROPERTIES
        FOLDER "Tools/SagaPipeline"
    )

    add_executable(saga-pipeline
        ${SAGA_ROOT}/Tools/SagaPipeline/cli/main.cpp
    )
    saga_apply_compiler_flags(saga-pipeline)
    target_link_libraries(saga-pipeline PRIVATE
        SagaPipelineLib
    )
    set_target_properties(saga-pipeline PROPERTIES
        FOLDER "Tools/SagaPipeline"
    )

    # --- Saga Editor Composition Tool ----------------------------------------
    # Saga-specific artifact generation is intentionally outside the standalone
    # SDE package tree. This tool consumes SDE's public facade and graph APIs.
    if(SAGA_WITH_SDE)
        add_library(SagaEditorCompositionLib STATIC)
        saga_apply_compiler_flags(SagaEditorCompositionLib)

        target_sources(SagaEditorCompositionLib PRIVATE
            ${SAGA_EDITOR_COMPOSITION_SOURCES}
        )

        target_include_directories(SagaEditorCompositionLib PUBLIC
            ${SAGA_ROOT}/Tools/SagaEditorComposition/include
        )

        target_link_libraries(SagaEditorCompositionLib
            PUBLIC
                SDE::Core
            PRIVATE
                nlohmann_json::nlohmann_json
        )

        set_target_properties(SagaEditorCompositionLib PROPERTIES
            FOLDER "Tools/SagaEditorComposition"
        )

        add_executable(saga-editor-composition-compiler
            ${SAGA_ROOT}/Tools/SagaEditorComposition/cli/main.cpp
        )
        saga_apply_compiler_flags(saga-editor-composition-compiler)
        target_link_libraries(saga-editor-composition-compiler PRIVATE
            SagaEditorCompositionLib
        )
        set_target_properties(saga-editor-composition-compiler PROPERTIES
            FOLDER "Tools/SagaEditorComposition"
        )
    else()
        message(STATUS
            "saga-editor-composition-compiler skipped because SAGA_WITH_SDE is OFF")
    endif()

    # --- Graphics Public Shell ----------------------------------------------
    add_library(SagaGraphics INTERFACE)

    target_include_directories(SagaGraphics INTERFACE
        ${SAGA_ROOT}/Engine/Public
    )

    set_target_properties(SagaGraphics PROPERTIES
        FOLDER "Engine/Graphics"
    )

    # --- Engine Library ------------------------------------------------------
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
        ${SAGA_OPENSSL_INCLUDE_DIRS}
        ${SAGA_DOTNET_HOST_INCLUDE_DIR}
    )

    target_link_libraries(SagaEngine PUBLIC
        SagaCoreLog
        SagaShared
    )
    target_link_libraries(SagaEngine PRIVATE
        ${SAGA_DOTNET_NETHOST_LIBRARY}
        ${CMAKE_DL_LIBS}
    )

    # --- Diligent Render Backend --------------------------------------------
    add_library(SagaDiligentBackend STATIC)
    saga_apply_compiler_flags(SagaDiligentBackend)

    target_sources(SagaDiligentBackend PRIVATE
        ${DILIGENT_BACKEND_SOURCES}
    )

    target_include_directories(SagaDiligentBackend
        PUBLIC
            ${SAGA_ROOT}/Engine/Public
        PRIVATE
            ${SAGA_ROOT}/Engine/Private
    )

    target_link_libraries(SagaDiligentBackend
        PUBLIC
            SagaEngine
        PRIVATE
            imgui::imgui
    )
    saga_link_diligent_backend(SagaDiligentBackend)

    set_target_properties(SagaDiligentBackend PROPERTIES
        FOLDER "Engine/Render"
    )

    # --- Runtime Role Library ------------------------------------------------
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
    )

    target_link_libraries(SagaRuntimeLib PRIVATE
        SagaCollaboration
    )

    set_target_properties(SagaRuntimeLib PROPERTIES
        FOLDER "Runtime"
    )

    # --- Server Authority Library --------------------------------------------
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
    )

    target_link_libraries(SagaServerLib PRIVATE
        SagaDiagnostics
        SagaCollaboration
    )

    set_target_properties(SagaServerLib PROPERTIES
        FOLDER "Server"
    )

    # --- Saga Stress Arena Tool --------------------------------------------
    # Direct local diagnostics harness for bounded ZoneServer stress/soak proof.
    add_library(SagaStressArenaLib STATIC)
    saga_apply_compiler_flags(SagaStressArenaLib)
    saga_link_thirdparty(SagaStressArenaLib)

    target_sources(SagaStressArenaLib PRIVATE
        ${SAGA_STRESS_ARENA_SOURCES}
    )

    target_include_directories(SagaStressArenaLib PUBLIC
        ${SAGA_ROOT}/Tools/SagaStressArena/include
        ${SAGA_ROOT}/Server/include
        ${SAGA_ROOT}/Engine/Public
    )

    target_link_libraries(SagaStressArenaLib PRIVATE
        SagaServerLib
        SagaDiagnostics
    )

    set_target_properties(SagaStressArenaLib PROPERTIES
        FOLDER "Tools/SagaStressArena"
    )

    add_executable(SagaStressArena
        ${SAGA_ROOT}/Tools/SagaStressArena/cli/main.cpp
    )
    saga_apply_compiler_flags(SagaStressArena)
    saga_link_thirdparty(SagaStressArena)
    target_link_libraries(SagaStressArena PRIVATE
        SagaStressArenaLib
    )
    set_target_properties(SagaStressArena PROPERTIES
        FOLDER "Tools/SagaStressArena"
    )

    # --- Saga Chaos Lab Tool -----------------------------------------------
    # Bounded tool-level chaos profile runner around SagaStressArena.
    add_library(SagaChaosLabLib STATIC)
    saga_apply_compiler_flags(SagaChaosLabLib)
    saga_link_thirdparty(SagaChaosLabLib)

    target_sources(SagaChaosLabLib PRIVATE
        ${SAGA_CHAOS_LAB_SOURCES}
    )

    target_include_directories(SagaChaosLabLib PUBLIC
        ${SAGA_ROOT}/Tools/SagaChaosLab/include
        ${SAGA_ROOT}/Tools/SagaStressArena/include
        ${SAGA_ROOT}/Server/include
        ${SAGA_ROOT}/Engine/Public
    )

    target_link_libraries(SagaChaosLabLib PRIVATE
        SagaStressArenaLib
        SagaServerLib
        SagaDiagnostics
        nlohmann_json::nlohmann_json
    )

    set_target_properties(SagaChaosLabLib PROPERTIES
        FOLDER "Tools/SagaChaosLab"
    )

    add_executable(SagaChaosLab
        ${SAGA_ROOT}/Tools/SagaChaosLab/cli/main.cpp
    )
    saga_apply_compiler_flags(SagaChaosLab)
    saga_link_thirdparty(SagaChaosLab)
    target_link_libraries(SagaChaosLab PRIVATE
        SagaChaosLabLib
    )
    set_target_properties(SagaChaosLab PROPERTIES
        FOLDER "Tools/SagaChaosLab"
    )

    # --- Saga State Check Tool ---------------------------------------------
    # Deterministic state snapshot comparison foundation.
    add_library(SagaStateCheckLib STATIC)
    saga_apply_compiler_flags(SagaStateCheckLib)

    target_sources(SagaStateCheckLib PRIVATE
        ${SAGA_STATE_CHECK_SOURCES}
    )

    target_include_directories(SagaStateCheckLib PUBLIC
        ${SAGA_ROOT}/Tools/SagaStateCheck/include
    )

    target_link_libraries(SagaStateCheckLib PRIVATE
        nlohmann_json::nlohmann_json
    )

    set_target_properties(SagaStateCheckLib PROPERTIES
        FOLDER "Tools/SagaStateCheck"
    )

    # --- MultiplayerSandbox Headless Tool ----------------------------------
    # Bounded server-side sample proof for the Technical Preview vertical.
    add_library(MultiplayerSandboxHeadlessLib STATIC)
    saga_apply_compiler_flags(MultiplayerSandboxHeadlessLib)
    saga_link_thirdparty(MultiplayerSandboxHeadlessLib)

    target_sources(MultiplayerSandboxHeadlessLib PRIVATE
        ${MULTIPLAYER_SANDBOX_HEADLESS_SOURCES}
    )

    target_include_directories(MultiplayerSandboxHeadlessLib PUBLIC
        ${SAGA_ROOT}/Tools/MultiplayerSandboxHeadless/include
        ${SAGA_ROOT}/Server/include
        ${SAGA_ROOT}/Engine/Public
    )

    target_link_libraries(MultiplayerSandboxHeadlessLib PRIVATE
        SagaServerLib
        SagaEngine
        nlohmann_json::nlohmann_json
    )

    set_target_properties(MultiplayerSandboxHeadlessLib PROPERTIES
        FOLDER "Tools/MultiplayerSandboxHeadless"
    )

    add_executable(MultiplayerSandboxHeadless
        ${SAGA_ROOT}/Tools/MultiplayerSandboxHeadless/cli/main.cpp
    )
    saga_apply_compiler_flags(MultiplayerSandboxHeadless)
    saga_link_thirdparty(MultiplayerSandboxHeadless)
    target_link_libraries(MultiplayerSandboxHeadless PRIVATE
        MultiplayerSandboxHeadlessLib
    )
    set_target_properties(MultiplayerSandboxHeadless PROPERTIES
        FOLDER "Tools/MultiplayerSandboxHeadless"
    )

    # --- Backend Library -----------------------------------------------------
    add_library(SagaBackend STATIC)
    saga_apply_compiler_flags(SagaBackend)
    saga_link_thirdparty(SagaBackend)
    saga_link_rmlui(SagaBackend)

    target_link_libraries(SagaBackend PRIVATE SagaEngine)

    target_sources(SagaBackend PRIVATE
        ${BACKEND_SOURCES}
    )

    target_include_directories(SagaBackend PUBLIC
        ${SAGA_ROOT}/Backends/include
    )

    # --- Sandbox Library ------------------------------------------------------
    add_library(SagaSandboxLib STATIC)
    saga_apply_compiler_flags(SagaSandboxLib)
    saga_link_thirdparty(SagaSandboxLib)

    target_link_libraries(SagaSandboxLib PUBLIC
        SagaEngine
        SagaDiligentBackend
        SagaRuntimeLib
        SagaServerLib
        SagaBackend
    )

    target_sources(SagaSandboxLib PRIVATE
        ${SANDBOX_SOURCES}
    )

    target_include_directories(SagaSandboxLib PRIVATE
        ${SAGA_ROOT}/Apps/Sandbox/include
    )

    target_include_directories(SagaSandboxLib PRIVATE
        ${SAGA_ROOT}/Apps/Sandbox/src
    )

    # --- Editor Library -------------------------------------------------------
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
    target_include_directories(SagaEditorLib PRIVATE
        ${SAGA_OPENSSL_INCLUDE_DIRS}
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
        OpenSSL::Crypto
    )

    if(SAGA_WITH_SDE)
        target_link_libraries(SagaEditorLib PRIVATE SDE::Core)
    endif()
    target_compile_definitions(SagaEditorLib PRIVATE
        SAGA_WITH_SDE=$<BOOL:${SAGA_WITH_SDE}>
    )

    set_target_properties(SagaEditorLib PROPERTIES
        FOLDER "Editor"
    )

    # --- EditorLab Development Library ---------------------------------------
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
    )

    target_link_libraries(SagaEditorLabLib PRIVATE
        Qt6::Core
        Qt6::Widgets
    )

    set_target_properties(SagaEditorLabLib PROPERTIES
        FOLDER "Apps/EditorLab"
    )

    # --- Saga Product Orchestration Library -----------------------------------
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
        )

        target_link_libraries(SagaProductLib PRIVATE
            SagaAssetPipelineLib
            SagaBackend
            SagaEditorLib
            SDE::Core
        )

        target_compile_definitions(SagaProductLib PUBLIC
            SAGA_PRODUCT_VERSION="${CMAKE_PROJECT_VERSION}"
            SAGA_PRODUCT_GIT_COMMIT="${SAGA_GIT_COMMIT}"
            SAGA_PRODUCT_PLATFORM="${CMAKE_SYSTEM_NAME}"
            SAGA_BUILTIN_BASIC_WORKSPACE_ROOT="${SAGA_ROOT}/Apps/Saga/Definitions/BasicWorkspace"
        )
        target_compile_definitions(SagaProductLib PRIVATE SAGA_WITH_SDE=1)

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

    # --- Application Executables ----------------------------------------------
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
        ${SAGA_ROOT}/Apps/Runtime/RuntimeAssetStartupBootstrap.hpp
        ${SAGA_ROOT}/Apps/Runtime/RuntimeAssetStartupBootstrap.cpp
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

    # --- SagaEditor Executable ------------------------------------------------
    # qt_add_executable with WIN32 generates the WinMain shim on Windows so
    # Apps/Editor/main.cpp can use a standard int main() on all platforms.
    qt_add_executable(SagaEditor WIN32
        ${SAGA_ROOT}/Apps/Editor/main.cpp
        ${SAGA_ROOT}/Apps/Editor/SagaEditorQtStaticPlugins.cpp
    )

    target_include_directories(SagaEditor PRIVATE
        ${SAGA_ROOT}/Editor/include   # access SagaEditor/App/* headers
    )

    saga_apply_compiler_flags(SagaEditor)

    target_link_libraries(SagaEditor PRIVATE
        SagaEditorLib   # all editor logic + transitive Qt6::Widgets
    )

    if(TARGET Qt6::QXcbIntegrationPlugin)
        if(TARGET qt::qt)
            target_link_libraries(SagaEditor PRIVATE qt::qt)
        else()
            target_link_libraries(SagaEditor PRIVATE
                Qt6::QXcbIntegrationPlugin
            )
        endif()
        target_compile_definitions(SagaEditor PRIVATE
            SAGA_EDITOR_IMPORT_QT_XCB_PLUGIN
        )

        set(_saga_editor_qt_platform_plugin_dirs)
        if(DEFINED qt_Qt6_QXcbIntegrationPlugin_LIB_DIRS_RELWITHDEBINFO)
            list(APPEND _saga_editor_qt_platform_plugin_dirs
                ${qt_Qt6_QXcbIntegrationPlugin_LIB_DIRS_RELWITHDEBINFO}
            )
        endif()
        get_target_property(_saga_editor_qt_xcb_plugin_dirs
            Qt6::QXcbIntegrationPlugin
            INTERFACE_LINK_DIRECTORIES
        )
        if(_saga_editor_qt_xcb_plugin_dirs)
            foreach(_saga_editor_qt_plugin_dir
                    IN LISTS _saga_editor_qt_xcb_plugin_dirs)
                string(REGEX REPLACE
                    "^\\$<\\$<CONFIG:[^>]+>:(.*)>$"
                    "\\1"
                    _saga_editor_qt_plugin_dir
                    "${_saga_editor_qt_plugin_dir}"
                )
                list(APPEND _saga_editor_qt_platform_plugin_dirs
                    "${_saga_editor_qt_plugin_dir}"
                )
            endforeach()
        endif()

        find_library(SAGA_QT_OFFSCREEN_PLATFORM_PLUGIN
            NAMES qoffscreen
            PATHS ${_saga_editor_qt_platform_plugin_dirs}
            NO_DEFAULT_PATH
        )
        if(SAGA_QT_OFFSCREEN_PLATFORM_PLUGIN)
            target_link_libraries(SagaEditor PRIVATE
                ${SAGA_QT_OFFSCREEN_PLATFORM_PLUGIN}
            )
            target_compile_definitions(SagaEditor PRIVATE
                SAGA_EDITOR_IMPORT_QT_OFFSCREEN_PLUGIN
            )
        endif()
    endif()

    set_target_properties(SagaEditor PROPERTIES
        OUTPUT_NAME "SagaEditor"
        FOLDER      "Apps"
    )

    # --- SagaEditorComposer Executable ---------------------------------------
    # Visual authoring app for editor composition source. It remains separate
    # from Apps/Editor runtime and invokes pipeline tools only as processes.
    qt_add_executable(SagaEditorComposer WIN32
        ${SAGA_ROOT}/Apps/SagaEditorComposer/main.cpp
        ${SAGA_ROOT}/Apps/SagaEditorComposer/SagaEditorComposerQtStaticPlugins.cpp
    )

    target_include_directories(SagaEditorComposer PRIVATE
        ${SAGA_ROOT}/Editor/include
    )

    saga_apply_compiler_flags(SagaEditorComposer)

    target_link_libraries(SagaEditorComposer PRIVATE
        SagaEditorLib
        Qt6::Core
        Qt6::Widgets
    )

    if(TARGET Qt6::QXcbIntegrationPlugin)
        if(TARGET qt::qt)
            target_link_libraries(SagaEditorComposer PRIVATE qt::qt)
        else()
            target_link_libraries(SagaEditorComposer PRIVATE
                Qt6::QXcbIntegrationPlugin
            )
        endif()
        target_compile_definitions(SagaEditorComposer PRIVATE
            SAGA_EDITOR_COMPOSER_IMPORT_QT_XCB_PLUGIN
        )

        set(_saga_editor_composer_qt_platform_plugin_dirs)
        if(DEFINED qt_Qt6_QXcbIntegrationPlugin_LIB_DIRS_RELWITHDEBINFO)
            list(APPEND _saga_editor_composer_qt_platform_plugin_dirs
                ${qt_Qt6_QXcbIntegrationPlugin_LIB_DIRS_RELWITHDEBINFO}
            )
        endif()
        get_target_property(_saga_editor_composer_qt_xcb_plugin_dirs
            Qt6::QXcbIntegrationPlugin
            INTERFACE_LINK_DIRECTORIES
        )
        if(_saga_editor_composer_qt_xcb_plugin_dirs)
            foreach(_saga_editor_composer_qt_plugin_dir
                    IN LISTS _saga_editor_composer_qt_xcb_plugin_dirs)
                string(REGEX REPLACE
                    "^\\$<\\$<CONFIG:[^>]+>:(.*)>$"
                    "\\1"
                    _saga_editor_composer_qt_plugin_dir
                    "${_saga_editor_composer_qt_plugin_dir}"
                )
                list(APPEND _saga_editor_composer_qt_platform_plugin_dirs
                    "${_saga_editor_composer_qt_plugin_dir}"
                )
            endforeach()
        endif()

        find_library(SAGA_QT_COMPOSER_OFFSCREEN_PLATFORM_PLUGIN
            NAMES qoffscreen
            PATHS ${_saga_editor_composer_qt_platform_plugin_dirs}
            NO_DEFAULT_PATH
        )
        if(SAGA_QT_COMPOSER_OFFSCREEN_PLATFORM_PLUGIN)
            target_link_libraries(SagaEditorComposer PRIVATE
                ${SAGA_QT_COMPOSER_OFFSCREEN_PLATFORM_PLUGIN}
            )
            target_compile_definitions(SagaEditorComposer PRIVATE
                SAGA_EDITOR_COMPOSER_IMPORT_QT_OFFSCREEN_PLUGIN
            )
        endif()
    endif()

    set_target_properties(SagaEditorComposer PROPERTIES
        OUTPUT_NAME "SagaEditorComposer"
        FOLDER      "Apps"
    )

    # --- EditorLab Executable ------------------------------------------------
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

    # --- Sandbox Executable ---------------------------------------------------
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

    target_include_directories(SagaSandbox PRIVATE
        ${SAGA_ROOT}/Apps/Sandbox/include
    )

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

    # --- RenderClientSmokeTest -------------------------------------------
    # Headless, CI-safe smoke test for the RenderClientApp facade.
    # No GPU, no window - exercises bind/update/release and multi-camera slots
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
