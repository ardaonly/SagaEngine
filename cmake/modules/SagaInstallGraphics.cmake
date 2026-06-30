function(saga_setup_graphics_install)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY "${SAGA_ROOT}/Engine/Public/SagaEngine/Graphics"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SagaEngine"
        COMPONENT SagaDevelopment
    )

    install(TARGETS
        SagaGraphics
        SagaGraphicsCore
        EXPORT SagaEngineTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT SagaDevelopment
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT SagaDevelopment
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT SagaDevelopment
    )

    install(EXPORT SagaEngineTargets
        FILE SagaEngineTargets.cmake
        NAMESPACE SagaEngine::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SagaEngine"
        COMPONENT SagaDevelopment
    )

    configure_package_config_file(
        "${SAGA_ROOT}/cmake/templates/SagaEngineConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/SagaEngineConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SagaEngine"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/SagaEngineConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/SagaEngineConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/SagaEngineConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SagaEngine"
        COMPONENT SagaDevelopment
    )
endfunction()
