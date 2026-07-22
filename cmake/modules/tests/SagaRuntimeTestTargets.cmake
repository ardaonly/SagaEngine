# SPDX-License-Identifier: MPL-2.0
    # --- Campaign 3 verified state/prompt seam tests -----------------------
    add_executable(SagaRuntimeVisibleSliceTests
        ${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Tests/RuntimeVisibleSliceTests.cpp
        ${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Private/RuntimeStateStore.cpp
        ${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Private/RuntimeUiPromptController.cpp
    )
    target_link_libraries(SagaRuntimeVisibleSliceTests PRIVATE
        SagaEngine
        SagaRuntimeLib
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
    )
    target_include_directories(SagaRuntimeVisibleSliceTests PRIVATE
        ${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Private
        ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
    )
    add_test(NAME SagaRuntimeVisibleSliceTests COMMAND SagaRuntimeVisibleSliceTests)
    set_tests_properties(SagaRuntimeVisibleSliceTests PROPERTIES
        LABELS "unit;runtime;runtime-ui;visible-slice;starter-arena;campaign3")

    # --- Script lifecycle tests ---------------------------------------------
    #
    # Build the initial script-host lifecycle seam without the broad runtime,
    # server, editor, or product test graph.
    if(EXISTS "${SAGA_SCRIPT_LIFECYCLE_TEST_SOURCE}")
        add_executable(SagaScriptLifecycleTests
            ${SAGA_SCRIPT_LIFECYCLE_TEST_SOURCE}
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/ScriptLifecycleService.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/ScriptPackageValidator.cpp
        )
        target_link_libraries(SagaScriptLifecycleTests PRIVATE
            SagaShared
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
        )
        target_include_directories(SagaScriptLifecycleTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_OPENSSL_INCLUDE_DIRS}
        )
        target_compile_definitions(SagaScriptLifecycleTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME SagaScriptLifecycleTests COMMAND SagaScriptLifecycleTests)
        set_tests_properties(SagaScriptLifecycleTests PROPERTIES LABELS "unit;runtime;scripting")
    endif()

    # --- Runtime startup preflight facade tests ------------------------------
    if(EXISTS "${SAGA_RUNTIME_STARTUP_PREFLIGHT_TEST_SOURCE}")
        add_executable(RuntimeStartupPreflightTests
            ${SAGA_RUNTIME_STARTUP_PREFLIGHT_TEST_SOURCE}
        )
        target_link_libraries(RuntimeStartupPreflightTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeStartupPreflightTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        add_test(NAME RuntimeStartupPreflightTests COMMAND RuntimeStartupPreflightTests)
        set_tests_properties(RuntimeStartupPreflightTests PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Runtime startup session facade tests --------------------------------
    if(EXISTS "${SAGA_RUNTIME_STARTUP_SESSION_TEST_SOURCE}")
        add_executable(RuntimeStartupSessionTests
            ${SAGA_RUNTIME_STARTUP_SESSION_TEST_SOURCE}
        )
        target_link_libraries(RuntimeStartupSessionTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeStartupSessionTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        add_test(NAME RuntimeStartupSessionTests COMMAND RuntimeStartupSessionTests)
        set_tests_properties(RuntimeStartupSessionTests PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Runtime startup diagnostics facade tests ----------------------------
    if(EXISTS "${SAGA_RUNTIME_STARTUP_DIAGNOSTICS_TEST_SOURCE}")
        add_executable(RuntimeStartupDiagnosticsTests
            ${SAGA_RUNTIME_STARTUP_DIAGNOSTICS_TEST_SOURCE}
        )
        target_link_libraries(RuntimeStartupDiagnosticsTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeStartupDiagnosticsTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        add_test(NAME RuntimeStartupDiagnosticsTests COMMAND RuntimeStartupDiagnosticsTests)
        set_tests_properties(RuntimeStartupDiagnosticsTests PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Runtime asset catalog facade tests --------------------------------
    if(EXISTS "${SAGA_RUNTIME_ASSET_CATALOG_TEST_SOURCE}")
        add_executable(RuntimeAssetCatalogTests
            ${SAGA_RUNTIME_ASSET_CATALOG_TEST_SOURCE}
        )
        target_link_libraries(RuntimeAssetCatalogTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeAssetCatalogTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        target_compile_definitions(RuntimeAssetCatalogTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME RuntimeAssetCatalogTests COMMAND RuntimeAssetCatalogTests)
        set_tests_properties(RuntimeAssetCatalogTests PROPERTIES
            LABELS "unit;runtime;asset;package")
    endif()

    # --- Runtime asset bootstrap facade tests -------------------------------
    if(EXISTS "${SAGA_RUNTIME_ASSET_BOOTSTRAP_TEST_SOURCE}")
        add_executable(RuntimeAssetBootstrapTests
            ${SAGA_RUNTIME_ASSET_BOOTSTRAP_TEST_SOURCE}
        )
        target_link_libraries(RuntimeAssetBootstrapTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeAssetBootstrapTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        target_compile_definitions(RuntimeAssetBootstrapTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME RuntimeAssetBootstrapTests COMMAND RuntimeAssetBootstrapTests)
        set_tests_properties(RuntimeAssetBootstrapTests PROPERTIES
            LABELS "unit;runtime;asset;package")
    endif()

    # --- SagaRuntime asset bootstrap helper seam tests ----------------------
    if(EXISTS "${SAGA_RUNTIME_ASSET_STARTUP_BOOTSTRAP_TEST_SOURCE}")
        add_executable(RuntimeAssetStartupBootstrapTests
            ${SAGA_RUNTIME_ASSET_STARTUP_BOOTSTRAP_TEST_SOURCE}
            ${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Private/RuntimeAssetStartupBootstrap.cpp
        )
        target_link_libraries(RuntimeAssetStartupBootstrapTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeAssetStartupBootstrapTests PRIVATE
            ${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Private
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        target_compile_definitions(RuntimeAssetStartupBootstrapTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME RuntimeAssetStartupBootstrapTests
            COMMAND RuntimeAssetStartupBootstrapTests)
        set_tests_properties(RuntimeAssetStartupBootstrapTests PROPERTIES
            LABELS "unit;runtime;asset;package;app")
    endif()

    # --- Runtime asset bootstrap diagnostics tests --------------------------
    if(EXISTS "${SAGA_RUNTIME_ASSET_BOOTSTRAP_DIAGNOSTICS_TEST_SOURCE}")
        add_executable(RuntimeAssetBootstrapDiagnosticsTests
            ${SAGA_RUNTIME_ASSET_BOOTSTRAP_DIAGNOSTICS_TEST_SOURCE}
        )
        target_link_libraries(RuntimeAssetBootstrapDiagnosticsTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeAssetBootstrapDiagnosticsTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        add_test(NAME RuntimeAssetBootstrapDiagnosticsTests
            COMMAND RuntimeAssetBootstrapDiagnosticsTests)
        set_tests_properties(RuntimeAssetBootstrapDiagnosticsTests PROPERTIES
            LABELS "unit;runtime;asset;package")
    endif()

    # --- Runtime package fixture smoke tests --------------------------------
    if(EXISTS "${SAGA_RUNTIME_PACKAGE_SMOKE_TEST_SOURCE}")
        add_executable(RuntimePackageSmokeTests
            ${SAGA_RUNTIME_PACKAGE_SMOKE_TEST_SOURCE}
        )
        target_link_libraries(RuntimePackageSmokeTests PRIVATE
            SagaEngine
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimePackageSmokeTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        target_compile_definitions(RuntimePackageSmokeTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME RuntimePackageSmokeTests COMMAND RuntimePackageSmokeTests)
        set_tests_properties(RuntimePackageSmokeTests PROPERTIES
            LABELS "unit;runtime;asset;package")
    endif()

    # --- Runtime service lifecycle value tests ------------------------------
    if(EXISTS "${SAGA_RUNTIME_SERVICE_LIFECYCLE_TEST_SOURCE}")
        add_executable(RuntimeServiceLifecycleTests
            ${SAGA_RUNTIME_SERVICE_LIFECYCLE_TEST_SOURCE}
        )
        target_link_libraries(RuntimeServiceLifecycleTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeServiceLifecycleTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        add_test(NAME RuntimeServiceLifecycleTests COMMAND RuntimeServiceLifecycleTests)
        set_tests_properties(RuntimeServiceLifecycleTests PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Runtime service registry tests -------------------------------------
    if(EXISTS "${SAGA_RUNTIME_SERVICE_REGISTRY_TEST_SOURCE}")
        add_executable(RuntimeServiceRegistryTests
            ${SAGA_RUNTIME_SERVICE_REGISTRY_TEST_SOURCE}
        )
        target_link_libraries(RuntimeServiceRegistryTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeServiceRegistryTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        add_test(NAME RuntimeServiceRegistryTests COMMAND RuntimeServiceRegistryTests)
        set_tests_properties(RuntimeServiceRegistryTests PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Runtime service registry diagnostic tests --------------------------
    if(EXISTS "${SAGA_RUNTIME_SERVICE_REGISTRY_DIAGNOSTICS_TEST_SOURCE}")
        add_executable(RuntimeServiceRegistryDiagnosticsTests
            ${SAGA_RUNTIME_SERVICE_REGISTRY_DIAGNOSTICS_TEST_SOURCE}
        )
        target_link_libraries(RuntimeServiceRegistryDiagnosticsTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeServiceRegistryDiagnosticsTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        target_compile_definitions(RuntimeServiceRegistryDiagnosticsTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME RuntimeServiceRegistryDiagnosticsTests
            COMMAND RuntimeServiceRegistryDiagnosticsTests)
        set_tests_properties(RuntimeServiceRegistryDiagnosticsTests
            PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Script binding runtime tests ---------------------------------------
    if(EXISTS "${SAGA_SCRIPT_BINDING_RUNTIME_TEST_SOURCE}")
        add_executable(ScriptBindingRuntimeTests
            ${SAGA_SCRIPT_BINDING_RUNTIME_TEST_SOURCE}
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/ScriptBindingManifestLoader.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/ScriptBindingRuntime.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/ScriptLifecycleService.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/ScriptPackageValidator.cpp
        )
        target_link_libraries(ScriptBindingRuntimeTests PRIVATE
            SagaShared
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
        )
        target_include_directories(ScriptBindingRuntimeTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_OPENSSL_INCLUDE_DIRS}
        )
        target_compile_definitions(ScriptBindingRuntimeTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
            SAGA_NUGET_PACKAGES="${CMAKE_BINARY_DIR}/.dotnet/home/.nuget/packages"
        )
        add_test(NAME ScriptBindingRuntimeTests COMMAND ScriptBindingRuntimeTests)
        set_tests_properties(ScriptBindingRuntimeTests PROPERTIES LABELS "unit;runtime;scripting")
    endif()

    # --- C# script host bridge tests ---------------------------------------
    if(EXISTS "${SAGA_CSHARP_SCRIPT_HOST_TEST_SOURCE}" AND
       TARGET SagaScriptRuntimeBridge)
        set(SAGA_CSHARP_SCRIPT_FIXTURE_PROJECT
            "${SAGA_ROOT}/Tests/Fixtures/CSharpScriptHost/CSharpScriptHostFixture.csproj"
        )
        set(SAGA_CSHARP_SCRIPT_FIXTURE_OUTPUT_DIR
            "${CMAKE_BINARY_DIR}/Managed/CSharpScriptHostFixture"
        )
        set(SAGA_CSHARP_SCRIPT_FIXTURE_ASSEMBLY
            "${SAGA_CSHARP_SCRIPT_FIXTURE_OUTPUT_DIR}/CSharpScriptHostFixture.scripts.dll"
        )
        add_custom_command(
            OUTPUT "${SAGA_CSHARP_SCRIPT_FIXTURE_ASSEMBLY}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${SAGA_CSHARP_SCRIPT_FIXTURE_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E env
                DOTNET_CLI_HOME=/tmp/sagascript-dotnet-home
                NUGET_PACKAGES=/tmp/sagascript-nuget
                DOTNET_CLI_TELEMETRY_OPTOUT=1
                "${SAGA_DOTNET_EXECUTABLE}" build
                "${SAGA_CSHARP_SCRIPT_FIXTURE_PROJECT}"
                --configuration Release
                --output "${SAGA_CSHARP_SCRIPT_FIXTURE_OUTPUT_DIR}"
                --nologo
                --verbosity minimal
            DEPENDS
                SagaScriptRuntimeBridge
                "${SAGA_CSHARP_SCRIPT_FIXTURE_PROJECT}"
                "${SAGA_ROOT}/Tests/Fixtures/CSharpScriptHost/TestScripts.cs"
            COMMENT "Building C# script host fixture assembly"
            VERBATIM
        )
        add_custom_target(CSharpScriptHostFixture
            DEPENDS "${SAGA_CSHARP_SCRIPT_FIXTURE_ASSEMBLY}"
        )

        add_executable(CSharpScriptHostTests
            ${SAGA_CSHARP_SCRIPT_HOST_TEST_SOURCE}
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Tests/CSharpScriptHostIdentityTests.hpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHost.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostCallbacks.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostCoreClr.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostInstances.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostInvocation.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostSupport.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostVerifiedGeneration.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/ScriptLifecycleService.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/ScriptPackageValidator.cpp
        )
        add_dependencies(CSharpScriptHostTests
            SagaScriptRuntimeBridge
            CSharpScriptHostFixture
        )
        target_link_libraries(CSharpScriptHostTests PRIVATE
            SagaShared
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
            ${SAGA_DOTNET_NETHOST_LIBRARY}
            ${CMAKE_DL_LIBS}
        )
        target_include_directories(CSharpScriptHostTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_OPENSSL_INCLUDE_DIRS}
            ${SAGA_DOTNET_HOST_INCLUDE_DIR}
        )
        target_compile_definitions(CSharpScriptHostTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
            SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY="${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
            SAGA_CSHARP_SCRIPT_FIXTURE_DIR="${SAGA_CSHARP_SCRIPT_FIXTURE_OUTPUT_DIR}"
            SAGA_DOTNET_ROOT="${SAGA_DOTNET_ROOT}"
        )
        add_test(NAME CSharpScriptHostTests COMMAND CSharpScriptHostTests)
        set_tests_properties(CSharpScriptHostTests PROPERTIES LABELS "unit;runtime;scripting;csharp")
    endif()

    # --- Editorless C# gameplay proof tests --------------------------------
    if(EXISTS "${SAGA_CSHARP_GAMEPLAY_PROOF_TEST_SOURCE}" AND
       TARGET SagaScriptRuntimeBridge)
        add_executable(CSharpGameplayProofTests
            ${SAGA_CSHARP_GAMEPLAY_PROOF_TEST_SOURCE}
            ${SAGA_ROOT}/Engine/Source/Runtime/ECS/Private/SagaEngine/ECS/ComponentRegistration.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/ECS/Private/SagaEngine/ECS/ComponentRegistry.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Simulation/Private/SagaEngine/Simulation/WorldState.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHost.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostCallbacks.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostCoreClr.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostInstances.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostInvocation.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostSupport.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/CSharpScriptHostVerifiedGeneration.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/ScriptLifecycleService.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/ScriptPackageValidator.cpp
            ${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Private/SagaEngine/Scripting/WorldStateScriptWorld.cpp
        )
        add_dependencies(CSharpGameplayProofTests
            SagaScriptRuntimeBridge
        )
        target_link_libraries(CSharpGameplayProofTests PRIVATE
            SagaCoreLog
            SagaShared
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
            ${SAGA_DOTNET_NETHOST_LIBRARY}
            ${CMAKE_DL_LIBS}
        )
        target_include_directories(CSharpGameplayProofTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_OPENSSL_INCLUDE_DIRS}
            ${SAGA_DOTNET_HOST_INCLUDE_DIR}
        )
        target_compile_definitions(CSharpGameplayProofTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
            SAGA_NUGET_PACKAGES="${CMAKE_BINARY_DIR}/.dotnet/home/.nuget/packages"
            SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY="${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
            SAGA_DOTNET_EXECUTABLE="${SAGA_DOTNET_EXECUTABLE}"
            SAGA_DOTNET_ROOT="${SAGA_DOTNET_ROOT}"
        )
        add_test(NAME CSharpGameplayProofTests COMMAND CSharpGameplayProofTests)
        set_tests_properties(CSharpGameplayProofTests PROPERTIES LABELS "unit;runtime;scripting;csharp")
    endif()

    # --- Script binding runtime thin runner --------------------------------
    if(TARGET SagaScriptRuntimeBridge AND
       EXISTS "${SAGA_ROOT}/Tests/Contract/GeneratedArtifacts/ScriptBindingRuntimeRunner.cpp")
        add_executable(ScriptBindingRuntimeRunner
            ${SAGA_ROOT}/Tests/Contract/GeneratedArtifacts/ScriptBindingRuntimeRunner.cpp
        )
        add_dependencies(ScriptBindingRuntimeRunner SagaScriptRuntimeBridge)
        target_link_libraries(ScriptBindingRuntimeRunner PRIVATE
            SagaEngine
            SagaShared
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
            ${SAGA_DOTNET_NETHOST_LIBRARY}
            ${CMAKE_DL_LIBS}
        )
        target_include_directories(ScriptBindingRuntimeRunner PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_OPENSSL_INCLUDE_DIRS}
            ${SAGA_DOTNET_HOST_INCLUDE_DIR}
        )
        target_compile_definitions(ScriptBindingRuntimeRunner PRIVATE
            SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY="${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
            SAGA_DOTNET_ROOT="${SAGA_DOTNET_ROOT}"
        )
        set_target_properties(ScriptBindingRuntimeRunner PROPERTIES
            FOLDER "Tests/Tools"
        )
        if(TARGET ScriptBindingRuntimeTests)
            add_dependencies(ScriptBindingRuntimeTests ScriptBindingRuntimeRunner)
            target_compile_definitions(ScriptBindingRuntimeTests PRIVATE
                SAGA_SCRIPT_BINDING_RUNTIME_RUNNER="$<TARGET_FILE:ScriptBindingRuntimeRunner>"
                SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY="${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
                SAGA_DOTNET_EXECUTABLE="${SAGA_DOTNET_EXECUTABLE}"
                SAGA_DOTNET_ROOT="${SAGA_DOTNET_ROOT}"
            )
        endif()
    endif()
