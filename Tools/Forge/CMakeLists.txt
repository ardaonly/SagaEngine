# ─── Forge — Standalone Build Orchestration Frontend ─────────────────────────
#
# Forge is a Cargo-flavoured build frontend for C++. It is intentionally
# self-contained at its core:
#   - No walk-up to find any engine root or marker file.
#   - No dependency on Conan, LLVM, Qt, or any third-party library.
#   - Saga-specific commands must remain external tool invocations, not linked
#     compiler, analyzer, or runtime implementations.
#
# CMake and Conan are backends called through adapter classes.
# The core remains extractable; product adapters are orchestration contracts.
#
cmake_minimum_required(VERSION 3.22)

project(forge
    VERSION     0.1.0
    LANGUAGES   CXX
    DESCRIPTION "Standalone build helper (Forge)"
)

# ─── Toolchain Defaults ─────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ─── Sources ────────────────────────────────────────────────────────────────

set(FORGE_HEADERS
    include/Forge/Adapters/ExternalGateAdapter.hpp
    include/Forge/BuildModel.h
    include/Forge/BuildScheduler.h
    include/Forge/CMakeAdapter.h
    include/Forge/ConanAdapter.h
    include/Forge/Diagnostics/DiagnosticCollector.hpp
    include/Forge/Diagnostics/ForgeDiagnostic.hpp
    include/Forge/EnvProbe.h
    include/Forge/ExitCode.h
    include/Forge/Hardware.h
    include/Forge/Manifest.h
    include/Forge/Pipeline/BuildPlan.hpp
    include/Forge/Pipeline/BuildPlanner.hpp
    include/Forge/ProcessRunner.h
    include/Forge/Reports/BuildReport.hpp
    include/Forge/Reports/BuildReportBuilder.hpp
    include/Forge/Reports/BuildReportWriter.hpp
    include/Forge/ToolEnv.h
)

set(FORGE_SOURCES
    src/main.cpp
    src/Adapters/ExternalGateAdapter.cpp
    src/BuildModel.cpp
    src/BuildScheduler.cpp
    src/CMakeAdapter.cpp
    src/ConanAdapter.cpp
    src/Diagnostics/DiagnosticCollector.cpp
    src/EnvProbe.cpp
    src/ExitCode.cpp
    src/Hardware.cpp
    src/Manifest.cpp
    src/Pipeline/BuildPlanner.cpp
    src/ProcessRunner.cpp
    src/Reports/BuildReportBuilder.cpp
    src/Reports/BuildReportWriter.cpp
    src/ToolEnv.cpp
)

# ─── Executable Target ──────────────────────────────────────────────────────

add_executable(forge ${FORGE_SOURCES} ${FORGE_HEADERS})

target_include_directories(forge
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(forge PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(forge PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(forge PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ─── Install Rules ──────────────────────────────────────────────────────────

include(GNUInstallDirs)

install(TARGETS forge
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/forge"
    )
endif()

enable_testing()

add_executable(forge_build_scheduler_tests
    tests/BuildSchedulerTests.cpp
    src/BuildScheduler.cpp
    src/Hardware.cpp
    include/Forge/BuildScheduler.h
    include/Forge/Hardware.h
)

target_include_directories(forge_build_scheduler_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(forge_build_scheduler_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(forge_build_scheduler_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME forge_build_scheduler_tests COMMAND forge_build_scheduler_tests)
set_tests_properties(forge_build_scheduler_tests PROPERTIES LABELS "tools;unit;forge")

add_executable(forge_exit_code_tests
    tests/ExitCodeTests.cpp
    src/ExitCode.cpp
    include/Forge/ExitCode.h
)

target_include_directories(forge_exit_code_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(forge_exit_code_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(forge_exit_code_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME forge_exit_code_tests COMMAND forge_exit_code_tests)
set_tests_properties(forge_exit_code_tests PROPERTIES LABELS "tools;unit;forge")

add_executable(forge_build_model_tests
    tests/BuildModelTests.cpp
    src/BuildModel.cpp
    src/Manifest.cpp
    include/Forge/BuildModel.h
    include/Forge/Manifest.h
)

target_include_directories(forge_build_model_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(forge_build_model_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(forge_build_model_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME forge_build_model_tests COMMAND forge_build_model_tests)
set_tests_properties(forge_build_model_tests PROPERTIES LABELS "tools;unit;forge")

add_executable(forge_build_plan_tests
    tests/BuildPlanTests.cpp
    src/Pipeline/BuildPlanner.cpp
    include/Forge/BuildModel.h
    include/Forge/Pipeline/BuildPlan.hpp
    include/Forge/Pipeline/BuildPlanner.hpp
)

target_include_directories(forge_build_plan_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(forge_build_plan_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(forge_build_plan_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME forge_build_plan_tests COMMAND forge_build_plan_tests)
set_tests_properties(forge_build_plan_tests PROPERTIES LABELS "tools;unit;forge")

add_executable(forge_diagnostic_collector_tests
    tests/DiagnosticCollectorTests.cpp
    src/Diagnostics/DiagnosticCollector.cpp
    include/Forge/Diagnostics/DiagnosticCollector.hpp
    include/Forge/Diagnostics/ForgeDiagnostic.hpp
    include/Forge/Pipeline/BuildPlan.hpp
    include/Forge/Reports/BuildReport.hpp
)

target_include_directories(forge_diagnostic_collector_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(forge_diagnostic_collector_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(forge_diagnostic_collector_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME forge_diagnostic_collector_tests COMMAND forge_diagnostic_collector_tests)
set_tests_properties(forge_diagnostic_collector_tests PROPERTIES LABELS "tools;unit;forge")

add_executable(forge_build_report_tests
    tests/BuildReportTests.cpp
    src/Diagnostics/DiagnosticCollector.cpp
    src/Pipeline/BuildPlanner.cpp
    src/Reports/BuildReportBuilder.cpp
    src/Reports/BuildReportWriter.cpp
    include/Forge/BuildModel.h
    include/Forge/Pipeline/BuildPlan.hpp
    include/Forge/Pipeline/BuildPlanner.hpp
    include/Forge/Reports/BuildReport.hpp
    include/Forge/Reports/BuildReportBuilder.hpp
    include/Forge/Reports/BuildReportWriter.hpp
)

target_include_directories(forge_build_report_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(forge_build_report_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(forge_build_report_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME forge_build_report_tests COMMAND forge_build_report_tests)
set_tests_properties(forge_build_report_tests PROPERTIES LABELS "tools;unit;forge")

add_executable(forge_fake_gate_tool
    tests/FakeGateTool.cpp
)

if(MSVC)
    target_compile_options(forge_fake_gate_tool PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(forge_fake_gate_tool PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(forge_tool_gate_tests
    tests/ToolGateTests.cpp
    src/ExitCode.cpp
    src/ProcessRunner.cpp
    include/Forge/ExitCode.h
    include/Forge/ProcessRunner.h
)

target_include_directories(forge_tool_gate_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(forge_tool_gate_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(forge_tool_gate_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME forge_tool_gate_tests
    COMMAND forge_tool_gate_tests $<TARGET_FILE:forge> $<TARGET_FILE:forge_fake_gate_tool>
)
set_tests_properties(forge_tool_gate_tests PROPERTIES LABELS "tools;unit;forge")

add_test(NAME forge_plan_build_json COMMAND forge plan build --json)
set_tests_properties(forge_plan_build_json PROPERTIES LABELS "tools;forge")
set_tests_properties(forge_plan_build_json
    PROPERTIES
        PASS_REGULAR_EXPRESSION "\"status\": \"Planned\""
)

add_test(NAME forge_plan_test_text COMMAND forge plan test)
set_tests_properties(forge_plan_test_text PROPERTIES LABELS "tools;forge")
set_tests_properties(forge_plan_test_text
    PROPERTIES
        PASS_REGULAR_EXPRESSION "run-tests \\[RunTests\\]"
)

add_test(NAME forge_plan_unknown_command COMMAND forge plan unknown)
set_tests_properties(forge_plan_unknown_command PROPERTIES LABELS "tools;forge")
set_tests_properties(forge_plan_unknown_command
    PROPERTIES
        WILL_FAIL TRUE
)

add_test(NAME forge_test_suite_architecture_explain
    COMMAND forge test --suite architecture --jobs 1 --explain
)
set_tests_properties(forge_test_suite_architecture_explain
    PROPERTIES
        LABELS "tools;unit;forge"
        PASS_REGULAR_EXPRESSION "--label-regex architecture -j 1"
)

add_test(NAME forge_test_suite_all_safe_explain
    COMMAND forge test --suite all-safe --jobs 1 --explain
)
set_tests_properties(forge_test_suite_all_safe_explain
    PROPERTIES
        LABELS "tools;unit;forge"
        PASS_REGULAR_EXPRESSION "--label-exclude stress\\|slow\\|load\\|timing-sensitive\\|long-running\\|gpu\\|display-required -j 1"
)

add_test(NAME forge_test_suite_rejects_label_conflict
    COMMAND forge test --suite runtime --label runtime --explain
)
set_tests_properties(forge_test_suite_rejects_label_conflict
    PROPERTIES
        LABELS "tools;unit;forge"
        WILL_FAIL TRUE
)

add_test(NAME forge_test_suite_rejects_unknown_suite
    COMMAND forge test --suite unknown --explain
)
set_tests_properties(forge_test_suite_rejects_unknown_suite
    PROPERTIES
        LABELS "tools;unit;forge"
        WILL_FAIL TRUE
)

set(FORGE_PLAN_REPORT_TEXT_DIR "${CMAKE_CURRENT_BINARY_DIR}/plan-report-text")
set(FORGE_PLAN_REPORT_JSON_DIR "${CMAKE_CURRENT_BINARY_DIR}/plan-report-json")
file(MAKE_DIRECTORY
    "${FORGE_PLAN_REPORT_TEXT_DIR}"
    "${FORGE_PLAN_REPORT_JSON_DIR}"
)

add_test(NAME forge_plan_write_report_text COMMAND forge plan build --write-report)
set_tests_properties(forge_plan_write_report_text PROPERTIES LABELS "tools;forge")
set_tests_properties(forge_plan_write_report_text
    PROPERTIES
        WORKING_DIRECTORY "${FORGE_PLAN_REPORT_TEXT_DIR}"
        PASS_REGULAR_EXPRESSION "report: build/Reports/build_report.json"
)

add_test(NAME forge_plan_write_report_json COMMAND forge plan build --json --write-report)
set_tests_properties(forge_plan_write_report_json PROPERTIES LABELS "tools;forge")
set_tests_properties(forge_plan_write_report_json
    PROPERTIES
        WORKING_DIRECTORY "${FORGE_PLAN_REPORT_JSON_DIR}"
        PASS_REGULAR_EXPRESSION "\"forge.report.path\": \"build/Reports/build_report.json\""
)

add_test(NAME forge_report_json COMMAND forge report --json)
set_tests_properties(forge_report_json PROPERTIES LABELS "tools;forge")
set_tests_properties(forge_report_json
    PROPERTIES
        WORKING_DIRECTORY "${FORGE_PLAN_REPORT_JSON_DIR}"
        DEPENDS forge_plan_write_report_json
        PASS_REGULAR_EXPRESSION "\"status\": \"Planned\""
)

add_test(NAME forge_report_build_json COMMAND forge report --build=build --json)
set_tests_properties(forge_report_build_json PROPERTIES LABELS "tools;forge")
set_tests_properties(forge_report_build_json
    PROPERTIES
        WORKING_DIRECTORY "${FORGE_PLAN_REPORT_JSON_DIR}"
        DEPENDS forge_plan_write_report_json
        PASS_REGULAR_EXPRESSION "\"forge.report.path\": \"build/Reports/build_report.json\""
)

add_test(NAME forge_report_path_json
    COMMAND forge report --path=build/Reports/build_report.json --json
)
set_tests_properties(forge_report_path_json PROPERTIES LABELS "tools;forge")
set_tests_properties(forge_report_path_json
    PROPERTIES
        WORKING_DIRECTORY "${FORGE_PLAN_REPORT_JSON_DIR}"
        DEPENDS forge_plan_write_report_json
        PASS_REGULAR_EXPRESSION "\"status\": \"Planned\""
)

add_test(NAME forge_report_text COMMAND forge report)
set_tests_properties(forge_report_text PROPERTIES LABELS "tools;forge")
set_tests_properties(forge_report_text
    PROPERTIES
        WORKING_DIRECTORY "${FORGE_PLAN_REPORT_JSON_DIR}"
        DEPENDS forge_plan_write_report_json
        PASS_REGULAR_EXPRESSION "hint: use --json to print the report"
)

add_test(NAME forge_report_missing_json COMMAND forge report --path=missing.json --json)
set_tests_properties(forge_report_missing_json PROPERTIES LABELS "tools;forge")
set_tests_properties(forge_report_missing_json
    PROPERTIES
        WORKING_DIRECTORY "${FORGE_PLAN_REPORT_JSON_DIR}"
        WILL_FAIL TRUE
)

message(STATUS "Forge ${PROJECT_VERSION}: configured (standalone build frontend)")
