# SPDX-License-Identifier: MPL-2.0
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

function(saga_add_installable_public_module_includes target_name)
    set(_build_includes)
    foreach(_include IN LISTS SAGA_MODULE_PUBLIC_INCLUDE_DIRS)
        list(APPEND _build_includes "$<BUILD_INTERFACE:${_include}>")
    endforeach()
    get_target_property(_target_type ${target_name} TYPE)
    if(_target_type STREQUAL "INTERFACE_LIBRARY")
        target_include_directories(${target_name} INTERFACE
            ${_build_includes}
            $<INSTALL_INTERFACE:include>
        )
    else()
        target_include_directories(${target_name} PUBLIC
            ${_build_includes}
            $<INSTALL_INTERFACE:include>
        )
    endif()
endfunction()

function(saga_create_engine_targets)
    saga_get_registered_sources(SagaShared SHARED_SOURCES)
    saga_get_registered_sources(SagaCollaboration COLLABORATION_SOURCES)
    saga_collect_sources(ASSET_PIPELINE_SOURCES Tools/AssetPipeline/src)
    saga_collect_sources(SAGA_STRESS_ARENA_SOURCES Tests/Support/Stress/SagaStressArena/src)
    saga_collect_sources(SAGA_CHAOS_LAB_SOURCES Tools/Developer/DistributionCheck/ChaosLab/src)
    saga_collect_sources(SAGA_STATE_CHECK_SOURCES Tools/Developer/DistributionCheck/StateCheck/src)
    saga_collect_sources(MULTIPLAYER_SANDBOX_HEADLESS_SOURCES Samples/MultiplayerSandbox/Source/Headless)
    saga_get_registered_sources(SagaDiligentRuntime SAGA_DILIGENT_RUNTIME_SOURCES)
    saga_get_registered_sources(SagaPlatformSDL SAGA_PLATFORM_SDL_SOURCES)
    saga_get_registered_sources(SagaUiBackend UI_BACKEND_SOURCES)
    saga_get_registered_sources(SagaPersistenceBackend PERSISTENCE_BACKEND_SOURCES)
    saga_get_registered_sources(SagaSandboxLib SANDBOX_SOURCES)
    saga_get_registered_sources(SagaEditorLabLib EDITORLAB_SOURCES)
    saga_get_registered_sources(SagaCoreLog CORE_LOG_SOURCES)
    saga_get_registered_sources(SagaDiagnostics DIAGNOSTICS_SOURCES)
    saga_get_graphics_core_sources(SAGA_GRAPHICS_CORE_SOURCES)
    set(SAGA_DILIGENT_GRAPHICS_BACKEND_IMPL_SOURCES ${SAGA_DILIGENT_RUNTIME_SOURCES})

    # --- Core Log Module ---------------------------------------------------
    add_library(SagaCoreLog STATIC)
    saga_apply_compiler_flags(SagaCoreLog)

    target_sources(SagaCoreLog PRIVATE
        ${CORE_LOG_SOURCES}
    )

    saga_add_installable_public_module_includes(SagaCoreLog)
    target_include_directories(SagaCoreLog
        PRIVATE
            ${SAGA_MODULE_PRIVATE_INCLUDE_DIRS}
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

    saga_add_installable_public_module_includes(SagaDiagnostics)
    target_include_directories(SagaDiagnostics
        PRIVATE
            ${SAGA_MODULE_PRIVATE_INCLUDE_DIRS}
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
    add_library(SagaShared INTERFACE)
    saga_add_installable_public_module_includes(SagaShared)

    set_target_properties(SagaShared PROPERTIES
        FOLDER "Shared"
    )

    # --- Collaboration Service Library ---------------------------------------
    add_library(SagaCollaboration STATIC)
    saga_apply_compiler_flags(SagaCollaboration)

    target_sources(SagaCollaboration PRIVATE
        ${COLLABORATION_SOURCES}
    )

    saga_add_installable_public_module_includes(SagaCollaboration)

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

    # --- Engine Library ------------------------------------------------------
    saga_create_registered_object_modules(SagaEngine SAGA_ENGINE_MODULE_TARGETS)
    add_library(SagaEngine STATIC)
    saga_apply_compiler_flags(SagaEngine)
    saga_compose_registered_objects(SagaEngine)

    saga_add_installable_public_module_includes(SagaEngine)

    add_library(SagaDotnetHost INTERFACE)
    target_link_libraries(SagaDotnetHost INTERFACE
        "$<BUILD_INTERFACE:${SAGA_DOTNET_NETHOST_LIBRARY}>")

    target_link_libraries(SagaEngine PUBLIC
        SagaCoreLog
        SagaGraphics
        SagaShared
        glm::glm
    )
    target_link_libraries(SagaEngine PRIVATE
        OpenSSL::Crypto
        SagaDiagnostics
        SagaDotnetHost
        ${CMAKE_DL_LIBS}
    )

    # --- SDL Platform Implementation ----------------------------------------
    add_library(SagaPlatformSDL STATIC)
    saga_apply_compiler_flags(SagaPlatformSDL)

    target_sources(SagaPlatformSDL PRIVATE
        ${SAGA_PLATFORM_SDL_SOURCES}
    )

    target_include_directories(SagaPlatformSDL PRIVATE
        ${SAGA_MODULE_PRIVATE_INCLUDE_DIRS}
    )

    target_link_libraries(SagaPlatformSDL PRIVATE
        SagaEngine
        SDL2::SDL2
    )

    set_target_properties(SagaPlatformSDL PROPERTIES
        FOLDER "Engine/Platform"
    )

    # --- Diligent Runtime ----------------------------------------------------
    add_library(SagaDiligentRuntime STATIC)
    saga_apply_compiler_flags(SagaDiligentRuntime)

    target_sources(SagaDiligentRuntime PRIVATE
        ${SAGA_DILIGENT_RUNTIME_SOURCES}
    )

    target_include_directories(SagaDiligentRuntime
        PUBLIC
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        PRIVATE
            ${SAGA_MODULE_PRIVATE_INCLUDE_DIRS}
    )

    target_link_libraries(SagaDiligentRuntime
        PUBLIC
            SagaGraphicsCore
        PRIVATE
            SagaCoreLog
    )
    saga_link_diligent_backend(SagaDiligentRuntime)

    set_target_properties(SagaDiligentRuntime PROPERTIES
        FOLDER "Engine/Graphics"
    )

    # --- Diligent Render Backend --------------------------------------------
    add_library(SagaDiligentBackend STATIC)
    saga_apply_compiler_flags(SagaDiligentBackend)

    saga_get_registered_sources(SagaDiligentBackend SAGA_DILIGENT_BACKEND_SOURCES)
    target_sources(SagaDiligentBackend PRIVATE ${SAGA_DILIGENT_BACKEND_SOURCES})

    target_include_directories(SagaDiligentBackend
        PUBLIC
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        PRIVATE
            ${SAGA_MODULE_PRIVATE_INCLUDE_DIRS}
    )

    target_link_libraries(SagaDiligentBackend
        PUBLIC
            SagaEngine
        PRIVATE
            SagaDiligentRuntime
    )

    set_target_properties(SagaDiligentBackend PROPERTIES
        FOLDER "Engine/Render"
    )

    # --- Runtime Role Library ------------------------------------------------
    saga_create_registered_object_modules(
        SagaRuntimeLib SAGA_RUNTIME_MODULE_TARGETS)
    add_library(SagaRuntimeLib STATIC)
    saga_apply_compiler_flags(SagaRuntimeLib)
    saga_compose_registered_objects(SagaRuntimeLib)

    saga_add_installable_public_module_includes(SagaRuntimeLib)

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
    saga_create_registered_object_modules(
        SagaServerLib SAGA_SERVER_MODULE_TARGETS)
    add_library(SagaServerLib STATIC)
    saga_apply_compiler_flags(SagaServerLib)
    saga_compose_registered_objects(SagaServerLib)

    saga_add_installable_public_module_includes(SagaServerLib)

    target_link_libraries(SagaServerLib PUBLIC
        SagaEngine
        SagaShared
    )

    target_link_libraries(SagaServerLib PRIVATE
        SagaDiagnostics
        SagaCollaboration
        asio::asio
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
        ${SAGA_ROOT}/Tests/Support/Stress/SagaStressArena/include
    )

    target_link_libraries(SagaStressArenaLib PUBLIC
        SagaServerLib
        SagaDiagnostics
    )

    set_target_properties(SagaStressArenaLib PROPERTIES
        FOLDER "Tests/Support/Stress"
    )

    add_executable(SagaStressArena
        ${SAGA_ROOT}/Tests/Support/Stress/SagaStressArena/cli/main.cpp
    )
    saga_apply_compiler_flags(SagaStressArena)
    saga_link_thirdparty(SagaStressArena)
    target_link_libraries(SagaStressArena PRIVATE
        SagaStressArenaLib
    )
    set_target_properties(SagaStressArena PROPERTIES
        FOLDER "Tests/Support/Stress"
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
        ${SAGA_ROOT}/Tools/Developer/DistributionCheck/ChaosLab/include
        ${SAGA_ROOT}/Tests/Support/Stress/SagaStressArena/include
    )

    target_link_libraries(SagaChaosLabLib PUBLIC
        SagaStressArenaLib
        SagaServerLib
        SagaDiagnostics
    )

    target_link_libraries(SagaChaosLabLib PRIVATE
        nlohmann_json::nlohmann_json
    )

    set_target_properties(SagaChaosLabLib PROPERTIES
        FOLDER "Tools/Developer/DistributionCheck"
    )

    add_executable(SagaChaosLab
        ${SAGA_ROOT}/Tools/Developer/DistributionCheck/ChaosLab/cli/main.cpp
    )
    saga_apply_compiler_flags(SagaChaosLab)
    saga_link_thirdparty(SagaChaosLab)
    target_link_libraries(SagaChaosLab PRIVATE
        SagaChaosLabLib
    )
    set_target_properties(SagaChaosLab PROPERTIES
        FOLDER "Tools/Developer/DistributionCheck"
    )

    # --- Saga State Check Tool ---------------------------------------------
    # Deterministic state snapshot comparison foundation.
    add_library(SagaStateCheckLib STATIC)
    saga_apply_compiler_flags(SagaStateCheckLib)

    target_sources(SagaStateCheckLib PRIVATE
        ${SAGA_STATE_CHECK_SOURCES}
    )

    target_include_directories(SagaStateCheckLib PUBLIC
        ${SAGA_ROOT}/Tools/Developer/DistributionCheck/StateCheck/include
    )

    target_link_libraries(SagaStateCheckLib PRIVATE
        nlohmann_json::nlohmann_json
    )

    set_target_properties(SagaStateCheckLib PROPERTIES
        FOLDER "Tools/Developer/DistributionCheck"
    )

    # --- MultiplayerSandbox Headless Tool ----------------------------------
    # Bounded server-side sample proof for the Technical Evaluation vertical.
    add_library(MultiplayerSandboxHeadlessLib STATIC)
    saga_apply_compiler_flags(MultiplayerSandboxHeadlessLib)
    saga_link_thirdparty(MultiplayerSandboxHeadlessLib)

    target_sources(MultiplayerSandboxHeadlessLib PRIVATE
        ${MULTIPLAYER_SANDBOX_HEADLESS_SOURCES}
    )

    target_include_directories(MultiplayerSandboxHeadlessLib PUBLIC
        ${SAGA_ROOT}/Samples/MultiplayerSandbox/Source/HeadlessInclude
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
        ${SAGA_ROOT}/Samples/MultiplayerSandbox/Source/HeadlessCli/main.cpp
    )
    saga_apply_compiler_flags(MultiplayerSandboxHeadless)
    saga_link_thirdparty(MultiplayerSandboxHeadless)
    target_link_libraries(MultiplayerSandboxHeadless PRIVATE
        MultiplayerSandboxHeadlessLib
    )
    set_target_properties(MultiplayerSandboxHeadless PROPERTIES
        FOLDER "Tools/MultiplayerSandboxHeadless"
    )

    # --- UI Backend Library --------------------------------------------------
    add_library(SagaUiBackend STATIC)
    saga_apply_compiler_flags(SagaUiBackend)
    saga_link_rmlui(SagaUiBackend)
    target_link_libraries(SagaUiBackend PRIVATE
        SagaEngine
    )
    target_sources(SagaUiBackend PRIVATE
        ${UI_BACKEND_SOURCES}
    )
    target_include_directories(SagaUiBackend PUBLIC
        $<BUILD_INTERFACE:${SAGA_ROOT}/Engine/Source/Runtime/UI/Public>
        $<INSTALL_INTERFACE:include>
    )

    # --- Persistence Backend Library -----------------------------------------
    add_library(SagaPersistenceBackend STATIC)
    saga_apply_compiler_flags(SagaPersistenceBackend)
    target_link_libraries(SagaPersistenceBackend PRIVATE
        SagaEngine
        libpqxx::pqxx
        redis++::redis++_static
    )
    target_sources(SagaPersistenceBackend PRIVATE
        ${PERSISTENCE_BACKEND_SOURCES}
    )
    target_include_directories(SagaPersistenceBackend PUBLIC
        $<BUILD_INTERFACE:${SAGA_ROOT}/Engine/Source/Runtime/Persistence/Public>
        $<INSTALL_INTERFACE:include>
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
    )

    target_link_libraries(SagaSandboxLib PRIVATE
        SagaPlatformSDL
    )

    target_sources(SagaSandboxLib PRIVATE
        ${SANDBOX_SOURCES}
    )

    target_include_directories(SagaSandboxLib PRIVATE
        ${SAGA_ROOT}/Engine/Source/Programs/SagaSandbox/Private/include
    )

    target_include_directories(SagaSandboxLib PRIVATE
        ${SAGA_ROOT}/Engine/Source/Programs/SagaSandbox/Private/src
    )

    # --- Editor Library -------------------------------------------------------
    # All editor logic lives in editor modules and is composed by Saga.
    saga_create_registered_object_modules(
        SagaEditorLib SAGA_EDITOR_MODULE_TARGETS)
    add_library(SagaEditorLib STATIC)
    saga_apply_compiler_flags(SagaEditorLib)
    saga_compose_registered_objects(SagaEditorLib)

    target_include_directories(SagaEditorLib PUBLIC
        ${SAGA_ROOT}/Engine/Source/Editor/EditorCore/Public
        ${SAGA_ROOT}/Engine/Source/Editor/EditorFramework/Public
        ${SAGA_ROOT}/Engine/Source/Editor/EditorQt/Public
        ${SAGA_ROOT}/Engine/Source/Editor/EditorQtRender/Public
        ${SAGA_ROOT}/Engine/Source/Editor/EditorAuthoring/Public
        ${SAGA_ROOT}/Engine/Source/Editor/VisualBlocksEditor/Public
        ${SAGA_ROOT}/Engine/Source/Editor/EditorCollaboration/Public
    )
    # Qt remains public because SagaEditorQt public headers expose Qt widgets.
    target_link_libraries(SagaEditorLib PUBLIC
        Qt6::Core
        Qt6::Widgets
        SagaEngine
    )

    target_link_libraries(SagaEditorLib PRIVATE
        SagaShared
        SagaCollaboration
        SagaDiligentRuntime
        SagaRuntimeLib
        OpenSSL::Crypto
    )

    set_target_properties(SagaEditorLib PROPERTIES
        FOLDER "Editor"
    )

    # Snapshot-only standalone build entry. The source and coordinator use no
    # Qt APIs; editor linkage supplies the existing project/build authorities.
    saga_get_registered_sources(SagaStandaloneBuild SAGA_STANDALONE_BUILD_SOURCES)
    add_executable(SagaStandaloneBuild ${SAGA_STANDALONE_BUILD_SOURCES})
    saga_apply_compiler_flags(SagaStandaloneBuild)
    saga_link_thirdparty(SagaStandaloneBuild)
    target_link_libraries(SagaStandaloneBuild PRIVATE SagaEditorLib)
    set_target_properties(SagaStandaloneBuild PROPERTIES
        FOLDER "Programs/SagaStandaloneBuild")

    # --- EditorLab Development Library ---------------------------------------
    # Shared by the standalone EditorLab executable and optional dev bridges.
    add_library(SagaEditorLabLib STATIC)
    saga_apply_compiler_flags(SagaEditorLabLib)

    target_sources(SagaEditorLabLib PRIVATE
        ${EDITORLAB_SOURCES}
    )

    target_include_directories(SagaEditorLabLib
        PUBLIC
            ${SAGA_ROOT}/Engine/Source/Programs/SagaEditorLab/Private/include
        PRIVATE
            ${SAGA_ROOT}/Engine/Source/Programs/SagaEditorLab/Private/src
    )

    target_link_libraries(SagaEditorLabLib PUBLIC
        SagaEditorLib
    )

    target_link_libraries(SagaEditorLabLib PRIVATE
        Qt6::Core
        Qt6::Widgets
    )

    set_target_properties(SagaEditorLabLib PROPERTIES
        FOLDER "Programs/SagaEditorLab"
    )

    # --- Saga Product Orchestration Library -----------------------------------
    saga_create_registered_object_modules(
        SagaProductLib SAGA_PRODUCT_MODULE_TARGETS)
    foreach(_module_target IN LISTS SAGA_PRODUCT_MODULE_TARGETS)
        target_compile_definitions(${_module_target} PRIVATE
            SAGA_PRODUCT_VERSION="${CMAKE_PROJECT_VERSION}"
            SAGA_PRODUCT_GIT_COMMIT="${SAGA_GIT_COMMIT}"
            SAGA_PRODUCT_PLATFORM="${CMAKE_SYSTEM_NAME}"
        )
    endforeach()
    add_library(SagaProductLib STATIC)
    saga_apply_compiler_flags(SagaProductLib)
    saga_compose_registered_objects(SagaProductLib)

    target_link_libraries(SagaProductLib PUBLIC
        SagaEngine
        SagaShared
        SagaCollaboration
        Qt6::Core
        Qt6::Widgets
    )

    target_link_libraries(SagaProductLib PRIVATE
        SagaAssetPipelineLib
        SagaEditorLib
        OpenSSL::Crypto
    )

    target_compile_definitions(SagaProductLib PUBLIC
        SAGA_PRODUCT_VERSION="${CMAKE_PROJECT_VERSION}"
        SAGA_PRODUCT_GIT_COMMIT="${SAGA_GIT_COMMIT}"
        SAGA_PRODUCT_PLATFORM="${CMAKE_SYSTEM_NAME}"
    )
    set_target_properties(SagaProductLib PROPERTIES
        FOLDER "Programs/SagaLauncher"
    )

    if(SAGA_WITH_EDITORLAB_DEV_PANEL)
        add_library(SagaEditorLabBridge STATIC
            ${SAGA_ROOT}/Engine/Source/Developer/SagaDev/SagaEditorLabBridge.h
            ${SAGA_ROOT}/Engine/Source/Developer/SagaDev/SagaEditorLabBridge.cpp
        )
        saga_apply_compiler_flags(SagaEditorLabBridge)
        saga_link_thirdparty(SagaEditorLabBridge)
        target_include_directories(SagaEditorLabBridge PUBLIC
            ${SAGA_ROOT}/Engine/Source/Developer/SagaDev
        )
        target_link_libraries(SagaEditorLabBridge PUBLIC
            SagaEditorLabLib
        )
        set_target_properties(SagaEditorLabBridge PROPERTIES
            FOLDER "Developer/SagaDev"
        )
    endif()

    # --- Application Executables ----------------------------------------------
    saga_get_registered_sources(Saga SAGA_LAUNCHER_PROGRAM_SOURCES)
    qt_add_executable(Saga WIN32 ${SAGA_LAUNCHER_PROGRAM_SOURCES})

    saga_apply_compiler_flags(Saga)
    saga_link_thirdparty(Saga)
    target_include_directories(Saga PRIVATE
        ${SAGA_ROOT}/Engine/Source/Programs/SagaLauncher/Private
    )
    target_link_libraries(Saga PRIVATE SagaProductLib SagaEditorLib)
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
        FOLDER      "Programs/SagaLauncher"
    )

    saga_get_registered_sources(SagaRuntime SAGA_RUNTIME_PROGRAM_SOURCES)

    # Runtime orchestration is app-private, but it is shared by the runtime
    # executable and behavior/unit tests. Compile it once under a single owner
    # instead of re-listing implementation .cpp files in each consumer.
    set(SAGA_RUNTIME_APP_PRIVATE_SOURCES)
    set(SAGA_RUNTIME_EXECUTABLE_SOURCES)
    foreach(SAGA_RUNTIME_PROGRAM_SOURCE IN LISTS SAGA_RUNTIME_PROGRAM_SOURCES)
        if(SAGA_RUNTIME_PROGRAM_SOURCE MATCHES
           "/Runtime(Host|LaunchSession|ArtifactSession|RenderSession|ScriptSession|EvidenceSession|FrameLoop|GameplayResultWriter|GameplaySession|StateStore|TriggerSession|UiPromptController|UiSession)\\.(cpp|h)$")
            list(APPEND SAGA_RUNTIME_APP_PRIVATE_SOURCES
                "${SAGA_RUNTIME_PROGRAM_SOURCE}")
        else()
            list(APPEND SAGA_RUNTIME_EXECUTABLE_SOURCES
                "${SAGA_RUNTIME_PROGRAM_SOURCE}")
        endif()
    endforeach()

    list(LENGTH SAGA_RUNTIME_APP_PRIVATE_SOURCES
         SAGA_RUNTIME_APP_PRIVATE_SOURCE_COUNT)
    if(NOT SAGA_RUNTIME_APP_PRIVATE_SOURCE_COUNT EQUAL 26)
        message(FATAL_ERROR
            "SagaRuntimeAppLib expected 26 app-private orchestration sources, "
            "but found ${SAGA_RUNTIME_APP_PRIVATE_SOURCE_COUNT}. "
            "Update the ownership filter when Runtime orchestration changes.")
    endif()

    add_library(SagaRuntimeAppLib STATIC
        ${SAGA_RUNTIME_APP_PRIVATE_SOURCES}
    )
    saga_apply_compiler_flags(SagaRuntimeAppLib)
    saga_link_thirdparty(SagaRuntimeAppLib)
    target_include_directories(SagaRuntimeAppLib PUBLIC
        ${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Private
    )
    target_link_libraries(SagaRuntimeAppLib PUBLIC
        SagaRuntimeLib
        SagaEngine
        SagaUiBackend
        SagaDiligentRuntime
        SagaPlatformSDL
    )
    set_target_properties(SagaRuntimeAppLib PROPERTIES
        FOLDER "Programs/SagaRuntime"
    )

    add_executable(SagaRuntime ${SAGA_RUNTIME_EXECUTABLE_SOURCES})

    saga_apply_compiler_flags(SagaRuntime)
    saga_link_thirdparty(SagaRuntime)
    target_link_libraries(SagaRuntime PRIVATE
        SagaRuntimeAppLib
    )

    set_target_properties(SagaRuntime PROPERTIES
        OUTPUT_NAME "SagaRuntime"
        FOLDER      "Programs/SagaRuntime"
    )

    # --- EditorLab Executable ------------------------------------------------
    # Development-only lab surface for deterministic editor scenario experiments.
    saga_get_registered_sources(EditorLab SAGA_EDITORLAB_PROGRAM_SOURCES)
    qt_add_executable(EditorLab WIN32 ${SAGA_EDITORLAB_PROGRAM_SOURCES})

    target_include_directories(EditorLab PRIVATE
        ${SAGA_ROOT}/Engine/Source/Programs/SagaEditorLab/Private/include
        ${SAGA_ROOT}/Engine/Source/Programs/SagaEditorLab/Private/src
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
        FOLDER      "Programs/SagaEditorLab"
    )

    # --- Sandbox Executable ---------------------------------------------------
    set(_saga_sandbox_main "${SAGA_ROOT}/Engine/Source/Programs/SagaSandbox/Private/Launchers/SandboxMain.cpp")

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
        ${SAGA_ROOT}/Engine/Source/Programs/SagaSandbox/Private/include
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
        FOLDER      "Programs/SagaSandbox"
    )

    foreach(_saga_registered_target IN ITEMS
        SagaCoreLog SagaDiagnostics SagaCollaboration SagaEngine
        SagaPlatformSDL SagaDiligentRuntime SagaDiligentBackend SagaRuntimeLib
        SagaServerLib SagaUiBackend SagaPersistenceBackend SagaSandboxLib
        SagaEditorLib SagaEditorLabLib
        SagaProductLib Saga SagaRuntime EditorLab SagaSandbox)
        if(TARGET ${_saga_registered_target})
            saga_apply_registered_usage(${_saga_registered_target})
        endif()
    endforeach()

    file(GENERATE
        OUTPUT "${CMAKE_BINARY_DIR}/generated/SagaBackendOwnership-$<CONFIG>.txt"
        CONTENT
"SagaUiBackend=$<TARGET_PROPERTY:SagaUiBackend,LINK_LIBRARIES>
SagaPersistenceBackend=$<TARGET_PROPERTY:SagaPersistenceBackend,LINK_LIBRARIES>
SagaRuntimeAppLib=$<TARGET_PROPERTY:SagaRuntimeAppLib,LINK_LIBRARIES>
SagaRuntime=$<TARGET_PROPERTY:SagaRuntime,LINK_LIBRARIES>
SagaSandboxLib=$<TARGET_PROPERTY:SagaSandboxLib,LINK_LIBRARIES>
SagaEditorLib=$<TARGET_PROPERTY:SagaEditorLib,LINK_LIBRARIES>
SagaProductLib=$<TARGET_PROPERTY:SagaProductLib,LINK_LIBRARIES>
")

endfunction()
