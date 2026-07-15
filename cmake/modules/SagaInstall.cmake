function(saga_setup_install)
    include(GNUInstallDirs)

    # Install the canonical mixed-license index and all repository license
    # texts with the development SDK. Their presence does not make the
    # planned MPL-2.0 migration effective; LICENSE remains authoritative
    # for resolving the current mixed-license state.
    install(FILES
        "${SAGA_ROOT}/LICENSE"
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
        SagaEngine
        SagaRuntimeLib
        SagaServerLib
        SagaBackend
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
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
