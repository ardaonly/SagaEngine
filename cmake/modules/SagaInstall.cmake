function(saga_setup_install)
    include(GNUInstallDirs)
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
    )
endfunction()
