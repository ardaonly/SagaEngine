function(saga_setup_install)
    include(GNUInstallDirs)
    include(CPack)

    install(TARGETS SagaEngine SagaBackend SagaRuntime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(TARGETS SagaApp
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    if(SAGA_ENABLE_EDITOR)
        install(TARGETS SagaEditor
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    if(SAGA_ENABLE_SERVER)
        install(TARGETS SagaServer
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/Engine/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/SagaEngine
        FILES_MATCHING PATTERN "*.h"
    )

    set(CPACK_PACKAGE_NAME "SagaEngine")
    set(CPACK_PACKAGE_VERSION ${SAGA_ENGINE_VERSION})
    set(CPACK_GENERATOR "ZIP;TGZ")
endfunction()