function(saga_setup_install)
    include(GNUInstallDirs)

    # Install the canonical mixed-license index and all repository license
    # texts with the development SDK. Their presence does not make the
    # planned MPL-2.0 migration effective; LICENSE.md remains authoritative
    # for resolving the current mixed-license state.
    install(FILES
        "${SAGA_ROOT}/LICENSE.md"
        "${SAGA_ROOT}/docs/licensing/THIRD_PARTY_NOTICES.md"
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
        SagaEngine
        SagaRuntimeLib
        SagaServerLib
        SagaBackend
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT SagaDevelopment
    )

    install(DIRECTORY "${SAGA_ROOT}/Engine/Public/SagaEngine"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT SagaDevelopment
        PATTERN "Graphics" EXCLUDE
    )

    saga_setup_graphics_install()
endfunction()
