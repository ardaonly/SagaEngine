# SPDX-License-Identifier: MPL-2.0
function(saga_setup_install)
    include(GNUInstallDirs)

    # Install the primary MPL-2.0 text, mixed-license scope guide, and all
    # canonical repository license texts with the development SDK.
    install(FILES
        "${SAGA_ROOT}/LICENSE"
        "${SAGA_ROOT}/LICENSING.md"
        "${SAGA_ROOT}/LICENSES/THIRD_PARTY_NOTICES.md"
        DESTINATION "Licenses/Saga"
        COMPONENT SagaDevelopment
    )

    install(DIRECTORY "${SAGA_ROOT}/LICENSES/"
        DESTINATION "Licenses/Saga/LICENSES"
        COMPONENT SagaDevelopment
    )
    install(TARGETS
        SagaCoreLog
        SagaDiagnostics
        SagaShared
        SagaCollaboration
        SagaDotnetHost
        SagaEngine
        SagaRuntimeLib
        SagaServerLib
        SagaUiBackend
        SagaPersistenceBackend
        EXPORT SagaEngineTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT SagaDevelopment
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT SagaDevelopment
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT SagaDevelopment
    )

    foreach(_saga_public_include IN LISTS SAGA_MODULE_PUBLIC_INCLUDE_DIRS)
        install(DIRECTORY "${_saga_public_include}/"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            COMPONENT SagaDevelopment)
    endforeach()

    saga_setup_graphics_install()
endfunction()
