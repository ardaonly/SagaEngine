function(saga_setup_graphics_install)
    include(GNUInstallDirs)

    install(DIRECTORY "${SAGA_ROOT}/Engine/Public/SagaEngine/Graphics"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SagaEngine"
        COMPONENT SagaDevelopment
    )
endfunction()
