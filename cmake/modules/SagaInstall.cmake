function(saga_setup_install)
    include(GNUInstallDirs)
    install(TARGETS SagaEngine SagaBackend SagaApp
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endfunction()