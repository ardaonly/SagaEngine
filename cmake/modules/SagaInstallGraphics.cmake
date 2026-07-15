function(saga_setup_graphics_install)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY "${SAGA_ROOT}/Engine/Source/Runtime/RHI/Public/SagaEngine/Graphics"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SagaEngine"
        COMPONENT SagaDevelopment
    )

    # Diligent's upstream install rules are intentionally left untouched.
    # Install the graphics dependency license payload explicitly as part of
    # SagaDevelopment so component-based SDK installations remain complete.
    set(_saga_diligent_license_destination
        "Licenses/ThirdParty/Vendor/Diligent")

    install(FILES
        "${SAGA_ROOT}/Vendor/Diligent/ThirdParty/glew/LICENSE.txt"
        DESTINATION "${_saga_diligent_license_destination}"
        RENAME "GLEW-License.txt"
        COMPONENT SagaDevelopment
    )

    install(FILES
        "${SAGA_ROOT}/Vendor/Diligent/ThirdParty/SPIRV-Headers/LICENSE"
        DESTINATION "${_saga_diligent_license_destination}"
        RENAME "SPIRV-Headers-License.txt"
        COMPONENT SagaDevelopment
    )

    install(FILES
        "${SAGA_ROOT}/Vendor/Diligent/ThirdParty/SPIRV-Tools/LICENSE"
        DESTINATION "${_saga_diligent_license_destination}"
        RENAME "SPIRV-Tools-License.txt"
        COMPONENT SagaDevelopment
    )

    install(FILES
        "${SAGA_ROOT}/Vendor/Diligent/ThirdParty/glslang/LICENSE.txt"
        DESTINATION "${_saga_diligent_license_destination}"
        RENAME "GLSLang-License.txt"
        COMPONENT SagaDevelopment
    )

    install(FILES
        "${SAGA_ROOT}/Vendor/Diligent/ThirdParty/SPIRV-Cross/LICENSE"
        DESTINATION "${_saga_diligent_license_destination}"
        RENAME "SPIRV-Cross-License.txt"
        COMPONENT SagaDevelopment
    )

    install(FILES
        "${SAGA_ROOT}/Vendor/Diligent/ThirdParty/volk/LICENSE.md"
        DESTINATION "${_saga_diligent_license_destination}"
        RENAME "Volk-License.md"
        COMPONENT SagaDevelopment
    )

    install(FILES
        "${SAGA_ROOT}/Vendor/Diligent/ThirdParty/xxHash/LICENSE"
        DESTINATION "${_saga_diligent_license_destination}"
        RENAME "xxHash-License.txt"
        COMPONENT SagaDevelopment
    )

    install(FILES
        "${SAGA_ROOT}/Vendor/Diligent/ThirdParty/stb/stb_image_write_license.txt"
        DESTINATION "${_saga_diligent_license_destination}"
        COMPONENT SagaDevelopment
    )

    install(FILES
        "${SAGA_ROOT}/Vendor/Diligent/ThirdParty/DirectXShaderCompiler/LICENSE.TXT"
        DESTINATION "${_saga_diligent_license_destination}"
        RENAME "DXC-License.txt"
        COMPONENT SagaDevelopment
    )

    install(FILES
        "${SAGA_ROOT}/Vendor/Diligent/ThirdParty/DirectXShaderCompiler/ThirdPartyNotices.txt"
        DESTINATION "${_saga_diligent_license_destination}"
        RENAME "DXC-ThirdPartyNotices.txt"
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
