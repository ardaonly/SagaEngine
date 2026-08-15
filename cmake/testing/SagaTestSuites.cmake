# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)

function(_saga_suite_property_name suite suffix out_var)
    string(MAKE_C_IDENTIFIER "${suite}" _suite_id)
    string(TOUPPER "${_suite_id}" _suite_id)
    set(${out_var} "SAGA_TEST_SUITE_${_suite_id}_${suffix}" PARENT_SCOPE)
endfunction()

function(_saga_json_quote value out_var)
    string(REPLACE "\\" "\\\\" _escaped "${value}")
    string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
    string(REPLACE "\n" "\\n" _escaped "${_escaped}")
    set(${out_var} "\"${_escaped}\"" PARENT_SCOPE)
endfunction()

function(_saga_json_array out_var)
    set(_items)
    foreach(_value IN LISTS ARGN)
        _saga_json_quote("${_value}" _quoted)
        list(APPEND _items "${_quoted}")
    endforeach()
    string(JOIN ", " _joined ${_items})
    set(${out_var} "[${_joined}]" PARENT_SCOPE)
endfunction()

function(saga_define_test_suite)
    cmake_parse_arguments(SUITE "DYNAMIC" "NAME;DESCRIPTION;ENVIRONMENT;TIMEOUT"
        "TESTS;TARGETS" ${ARGN})
    if(NOT SUITE_NAME OR (NOT SUITE_TESTS AND NOT SUITE_DYNAMIC))
        message(FATAL_ERROR "Saga test suites require NAME and TESTS")
    endif()

    get_property(_suites GLOBAL PROPERTY SAGA_TEST_SUITES)
    if(SUITE_NAME IN_LIST _suites)
        message(FATAL_ERROR "Duplicate Saga test suite: ${SUITE_NAME}")
    endif()

    foreach(_test IN LISTS SUITE_TESTS)
        if(NOT TEST "${_test}")
            message(FATAL_ERROR
                "Suite ${SUITE_NAME} references unregistered CTest test ${_test}")
        endif()
        set_property(TEST "${_test}" APPEND PROPERTY LABELS "suite-${SUITE_NAME}")
        get_property(_existing_timeout TEST "${_test}" PROPERTY TIMEOUT)
        if(NOT _existing_timeout OR _existing_timeout LESS SUITE_TIMEOUT)
            set_property(TEST "${_test}" PROPERTY TIMEOUT "${SUITE_TIMEOUT}")
        endif()
    endforeach()

    set_property(GLOBAL APPEND PROPERTY SAGA_TEST_SUITES "${SUITE_NAME}")
    foreach(_field IN ITEMS DESCRIPTION ENVIRONMENT TIMEOUT TESTS TARGETS)
        _saga_suite_property_name("${SUITE_NAME}" "${_field}" _property)
        set_property(GLOBAL PROPERTY "${_property}" "${SUITE_${_field}}")
    endforeach()
endfunction()

function(_saga_add_python_test name)
    cmake_parse_arguments(PY "" "SCRIPT;TIMEOUT" "ARGS" ${ARGN})
    if(NOT PY_SCRIPT)
        message(FATAL_ERROR "Python repository test ${name} has no SCRIPT")
    endif()
    add_test(NAME "${name}"
        COMMAND "${SAGA_SUITE_PYTHON}" "${PY_SCRIPT}" ${PY_ARGS})
    set_tests_properties("${name}" PROPERTIES
        WORKING_DIRECTORY "${SAGA_ROOT}"
        TIMEOUT "${PY_TIMEOUT}")
endfunction()

function(saga_register_local_test_suites)
    find_program(SAGA_SUITE_PYTHON NAMES python3 REQUIRED)

    _saga_add_python_test(SagaMaintainabilityPolicy
        SCRIPT "${SAGA_ROOT}/Tools/Developer/RepoAudit/maintainability_audit.py"
        ARGS --repo-root "${SAGA_ROOT}" --check
        TIMEOUT 120)
    add_test(NAME SagaLicenseUnitTests
        COMMAND "${SAGA_SUITE_PYTHON}" -m unittest discover
            -s "${SAGA_ROOT}/Tools/Licensing/tests" -p "test_*.py")
    set_tests_properties(SagaLicenseUnitTests PROPERTIES
        WORKING_DIRECTORY "${SAGA_ROOT}" TIMEOUT 180)
    _saga_add_python_test(SagaLicenseTextPolicy
        SCRIPT "${SAGA_ROOT}/Tools/Licensing/verify_license_texts.py"
        ARGS --root "${SAGA_ROOT}" --strict-cleanup
        TIMEOUT 120)
    _saga_add_python_test(SagaLicensePolicy
        SCRIPT "${SAGA_ROOT}/Tools/Licensing/validate_license_policy.py"
        ARGS --root "${SAGA_ROOT}" --strict
        TIMEOUT 180)
    _saga_add_python_test(SagaWorkflowPolicy
        SCRIPT "${SAGA_ROOT}/Tools/Developer/CI/check_github_workflows.py"
        ARGS --repo-root "${SAGA_ROOT}"
        TIMEOUT 60)
    add_test(NAME SagaBoundaryPolicy
        COMMAND "${SAGA_ROOT}/Tools/Developer/BoundaryCheck/verify")
    set_tests_properties(SagaBoundaryPolicy PROPERTIES
        WORKING_DIRECTORY "${SAGA_ROOT}" TIMEOUT 180)
    add_test(NAME SagaWikiPolicy
        COMMAND "${SAGA_ROOT}/scripts/wiki" verify)
    set_tests_properties(SagaWikiPolicy PROPERTIES
        WORKING_DIRECTORY "${SAGA_ROOT}" TIMEOUT 120)
    add_test(NAME SagaCiToolTests
        COMMAND "${SAGA_SUITE_PYTHON}" -m unittest discover
            -s "${SAGA_ROOT}/Tools/Developer/CI/tests" -p "test_*.py")
    set_tests_properties(SagaCiToolTests PROPERTIES
        WORKING_DIRECTORY "${SAGA_ROOT}" TIMEOUT 120)

    _saga_add_python_test(SagaProjectKitCliTests
        SCRIPT "${SAGA_ROOT}/Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py"
        TIMEOUT 600)
    _saga_add_python_test(SagaScriptCliTests
        SCRIPT "${SAGA_ROOT}/Tools/SagaScript/tests/test_sagascript_cli.py"
        TIMEOUT 600)
    _saga_add_python_test(SagaScriptRuntimeContractTests
        SCRIPT "${SAGA_ROOT}/Tools/SagaScript/tests/test_sagascript_toolchain_check.py"
        TIMEOUT 60)
    _saga_add_python_test(SagaPackagerCliTests
        SCRIPT "${SAGA_ROOT}/Tools/SagaPackager/tests/test_sagapack_cli.py"
        TIMEOUT 900)
    set_tests_properties(
        SagaProjectKitCliTests SagaScriptCliTests SagaScriptRuntimeContractTests
        SagaPackagerCliTests PROPERTIES
        ENVIRONMENT "SAGA_BUILD_DIR=${CMAKE_BINARY_DIR};SAGA_NATIVE_BIN_DIR=${CMAKE_BINARY_DIR}/bin"
        RESOURCE_LOCK "saga-managed-sample-state")

    foreach(_tool IN ITEMS ProjectKit Script Packager)
        if(_tool STREQUAL "ProjectKit")
            set(_directory "${SAGA_MANAGED_PROJECTKIT_DIRECTORY}")
            set(_assembly "${SAGA_MANAGED_PROJECTKIT_ASSEMBLY}")
        elseif(_tool STREQUAL "Script")
            set(_directory "${SAGA_MANAGED_SCRIPT_DIRECTORY}")
            set(_assembly "${SAGA_MANAGED_SCRIPT_ASSEMBLY}")
        else()
            set(_directory "${SAGA_MANAGED_PACKAGER_DIRECTORY}")
            set(_assembly "${SAGA_MANAGED_PACKAGER_ASSEMBLY}")
        endif()
        add_test(NAME "Saga${_tool}StagedCliSmoke"
            COMMAND "${SAGA_DOTNET_EXECUTABLE}"
                "${SAGA_MANAGED_TOOL_RUNTIME_STAGE}/tools/${_directory}/${_assembly}.dll"
                --help)
        set_tests_properties("Saga${_tool}StagedCliSmoke" PROPERTIES
            LABELS "managed;tool;smoke"
            TIMEOUT 60)
    endforeach()

    if(TARGET SagaEditorAuthoringModuleTests)
        add_test(NAME StandaloneBuildContractTests
            COMMAND SagaEditorAuthoringModuleTests
                --gtest_filter=StandaloneBuildCoordinatorTests.*)
        set_tests_properties(StandaloneBuildContractTests PROPERTIES
            LABELS "standalone;publication;recovery;failure-injection"
            RESOURCE_LOCK "saga-standalone-publication"
            RUN_SERIAL TRUE
            TIMEOUT 900)
    endif()

    saga_define_test_suite(
        NAME quick
        DESCRIPTION "Bounded repository, architecture, native, and managed validation"
        ENVIRONMENT "local-no-display-no-gpu, developer-dotnet-sdk"
        TIMEOUT 900
        TESTS
            SagaMaintainabilityPolicy SagaLicenseUnitTests SagaLicenseTextPolicy
            SagaLicensePolicy SagaWorkflowPolicy SagaBoundaryPolicy SagaWikiPolicy
            SagaCiToolTests RepoAuditTests ArchitectureTests CoreModuleTests
            MathModuleTests DiagnosticsModuleTests SagaProjectKitStagedCliSmoke
            SagaScriptStagedCliSmoke SagaPackagerStagedCliSmoke
            SagaScriptRuntimeContractTests
        TARGETS
            SagaCoreModuleTests SagaMathModuleTests SagaDiagnosticsModuleTests
            SagaArchitectureTests SagaScriptRuntimeBridge SagaManagedToolRuntimeStage)

    saga_define_test_suite(
        NAME native-fast
        DESCRIPTION "Display-independent Runtime and Editor native module tests"
        ENVIRONMENT "local-no-display-no-gpu"
        TIMEOUT 600
        TESTS
            CoreModuleTests MathModuleTests DiagnosticsModuleTests ECSModuleTests
            AssetsModuleTests ResourcesModuleTests InputModuleTests AudioModuleTests
            ScriptingModuleTests NetworkingModuleTests ReplicationModuleTests
            PersistenceModuleTests SimulationModuleTests WorldModuleTests
            ServerAuthorityModuleTests SagaRuntimeModuleTests EditorCoreModuleTests
            EditorFrameworkModuleTests EditorAuthoringModuleTests
            VisualBlocksEditorModuleTests EditorCollaborationModuleTests
            EditorExperimentalModuleTests SagaRuntimeHostUnitTests
            SagaAssetIdentityGeneratorTests
        TARGETS
            SagaCoreModuleTests SagaMathModuleTests SagaDiagnosticsModuleTests
            SagaECSModuleTests SagaAssetsModuleTests SagaResourcesModuleTests
            SagaInputModuleTests SagaAudioModuleTests SagaScriptingModuleTests
            SagaNetworkingModuleTests SagaReplicationModuleTests
            SagaPersistenceModuleTests SagaSimulationModuleTests SagaWorldModuleTests
            SagaServerAuthorityModuleTests SagaSagaRuntimeModuleTests
            SagaEditorCoreModuleTests SagaEditorFrameworkModuleTests
            SagaEditorAuthoringModuleTests SagaVisualBlocksEditorModuleTests
            SagaEditorCollaborationModuleTests SagaEditorExperimentalModuleTests
            SagaRuntimeHostUnitTests SagaAssetIdentityGeneratorTests)

    saga_define_test_suite(
        NAME managed-tools
        DESCRIPTION "Staged SagaProjectKit, SagaScript, and SagaPackager startup contracts"
        ENVIRONMENT "developer-dotnet-sdk, private-runtime-contract"
        TIMEOUT 900
        TESTS SagaProjectKitStagedCliSmoke SagaScriptStagedCliSmoke
            SagaPackagerStagedCliSmoke SagaScriptRuntimeContractTests
        TARGETS SagaManagedToolRuntimeStage SagaScriptRuntimeBridge)

    saga_define_test_suite(
        NAME architecture
        DESCRIPTION "Repository ownership and dependency-boundary validation"
        ENVIRONMENT "local-no-display-no-gpu"
        TIMEOUT 300
        TESTS RepoAuditTests SagaLinuxDistributionWhitelistTests
            ArchitectureTests EditorQtPublicAbiBoundaryTests SagaBoundaryPolicy
        TARGETS SagaArchitectureTests)

    saga_define_test_suite(
        NAME headless
        DESCRIPTION "Deterministic headless Runtime and sample contracts"
        ENVIRONMENT "headless-no-display-no-gpu"
        TIMEOUT 300
        TESTS MultiplayerSandboxHeadlessTests SagaRuntimeHostUnitTests
            SagaRuntimeVisibleSliceTests
        TARGETS MultiplayerSandboxHeadlessTests SagaRuntimeHostUnitTests
            SagaRuntimeVisibleSliceTests)

    if(TARGET SagaLauncherUiTests)
        saga_define_test_suite(
            NAME product-xcb
            DYNAMIC
            DESCRIPTION "Qt/XCB product workflow validation under an explicit display"
            ENVIRONMENT "xcb-display, qt6, private-managed-runtime, product-executables"
            TIMEOUT 600
            TARGETS SagaLauncherUiTests SagaRuntime SagaManagedToolRuntimeStage)
    endif()

    if(TEST StandaloneBuildContractTests)
        saga_define_test_suite(
            NAME standalone
            DESCRIPTION "Immutable snapshot, staging, verification, publication, and recovery contracts"
            ENVIRONMENT "local-no-display, private-managed-runtime"
            TIMEOUT 900
            TESTS StandaloneBuildContractTests
            TARGETS SagaEditorAuthoringModuleTests SagaStandaloneBuild
                SagaManagedToolRuntimeStage)
    endif()
endfunction()

function(saga_finalize_test_suites)
    if(NOT BUILD_TESTING)
        return()
    endif()

    get_property(_suites GLOBAL PROPERTY SAGA_TEST_SUITES)
    if(NOT _suites)
        message(FATAL_ERROR "No repository test suites were registered")
    endif()

    set(_catalog "{\n  \"schemaVersion\": 1,\n  \"suites\": [\n")
    set(_separator "")
    foreach(_suite IN LISTS _suites)
        foreach(_field IN ITEMS DESCRIPTION ENVIRONMENT TIMEOUT TESTS TARGETS)
            _saga_suite_property_name("${_suite}" "${_field}" _property)
            get_property(_${_field} GLOBAL PROPERTY "${_property}")
        endforeach()

        set(_aggregate "SagaSuite-${_suite}")
        add_custom_target("${_aggregate}")
        set_target_properties("${_aggregate}" PROPERTIES FOLDER "Tests/Suites")
        foreach(_target IN LISTS _TARGETS)
            if(NOT TARGET "${_target}")
                message(FATAL_ERROR
                    "Suite ${_suite} prerequisite target does not exist: ${_target}")
            endif()
            add_dependencies("${_aggregate}" "${_target}")
        endforeach()

        _saga_json_quote("${_suite}" _suite_json)
        _saga_json_quote("${_DESCRIPTION}" _description_json)
        _saga_json_quote("${_ENVIRONMENT}" _environment_json)
        _saga_json_quote("${_aggregate}" _aggregate_json)
        _saga_json_array(_tests_json ${_TESTS})
        _saga_json_array(_targets_json ${_TARGETS})
        string(APPEND _catalog "${_separator}    {\n")
        string(APPEND _catalog "      \"name\": ${_suite_json},\n")
        string(APPEND _catalog "      \"description\": ${_description_json},\n")
        string(APPEND _catalog "      \"environment\": ${_environment_json},\n")
        string(APPEND _catalog "      \"timeoutSeconds\": ${_TIMEOUT},\n")
        string(APPEND _catalog "      \"aggregateTarget\": ${_aggregate_json},\n")
        string(APPEND _catalog "      \"tests\": ${_tests_json},\n")
        string(APPEND _catalog "      \"buildTargets\": ${_targets_json}\n")
        string(APPEND _catalog "    }")
        set(_separator ",\n")
    endforeach()
    string(APPEND _catalog "\n  ]\n}\n")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
    file(WRITE "${CMAKE_BINARY_DIR}/generated/SagaTestSuites.json" "${_catalog}")
endfunction()
