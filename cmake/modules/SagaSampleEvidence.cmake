# --- SagaSampleEvidence.cmake -----------------------------------------------

function(saga_setup_sample_executable_evidence)
    option(SAGA_ENABLE_SAMPLE_EXECUTABLE_EVIDENCE
        "Run bounded sample executable evidence during the default build" ON)

    if(NOT SAGA_ENABLE_SAMPLE_EXECUTABLE_EVIDENCE)
        message(STATUS "Sample executable evidence: disabled")
        return()
    endif()

    set(_saga_sample_evidence_report
        "${CMAKE_BINARY_DIR}/reports/sample_executable_evidence_report.json")
    set(_saga_sample_evidence_mirror
        "${SAGA_ROOT}/build/reports/sample_executable_evidence_report.json")

    set(_saga_sample_evidence_dependencies
        SagaRuntime
        MultiplayerSandboxHeadless
    )

    if(TARGET Saga)
        list(APPEND _saga_sample_evidence_dependencies Saga)
    endif()

    add_custom_target(SagaSampleExecutableEvidence ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_BINARY_DIR}/reports"
        COMMAND "${SAGA_ROOT}/Tools/Developer/ClaimCheck/verify-sample-executable-evidence"
            --source-root "${SAGA_ROOT}"
            --build-dir "${CMAKE_BINARY_DIR}"
            --bin-dir "${SAGA_EXECUTABLE_OUTPUT_DIR}"
            --report-out "${_saga_sample_evidence_report}"
            --mirror-report-out "${_saga_sample_evidence_mirror}"
        DEPENDS
            ${_saga_sample_evidence_dependencies}
        BYPRODUCTS
            "${_saga_sample_evidence_report}"
            "${_saga_sample_evidence_mirror}"
        COMMENT "Running bounded sample executable evidence"
        VERBATIM
    )

    set_target_properties(SagaSampleExecutableEvidence PROPERTIES
        FOLDER "Evidence"
    )
endfunction()
