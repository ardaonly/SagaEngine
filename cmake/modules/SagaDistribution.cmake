# ─── SAGA production distribution staging ─────────────────────────────────────

function(_saga_distribution_platform out_var)
    if(WIN32)
        set(_platform "Windows")
    elseif(APPLE)
        set(_platform "macOS")
    elseif(UNIX)
        set(_platform "Linux")
    else()
        set(_platform "${CMAKE_SYSTEM_NAME}")
    endif()

    set(${out_var} "${_platform}" PARENT_SCOPE)
endfunction()

function(_saga_collect_target_links target_name out_var)
    get_target_property(_direct ${target_name} LINK_LIBRARIES)
    get_target_property(_interface ${target_name} INTERFACE_LINK_LIBRARIES)

    set(_links)
    if(_direct)
        list(APPEND _links ${_direct})
    endif()
    if(_interface)
        list(APPEND _links ${_interface})
    endif()

    set(${out_var} "${_links}" PARENT_SCOPE)
endfunction()

function(_saga_assert_no_qt_link target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "SAGA distribution target '${target_name}' does not exist")
    endif()

    set(_visited)
    set(_queue ${target_name})

    while(_queue)
        list(POP_FRONT _queue _current)
        if(_current IN_LIST _visited)
            continue()
        endif()
        list(APPEND _visited ${_current})

        if(NOT TARGET ${_current})
            continue()
        endif()

        _saga_collect_target_links(${_current} _links)
        foreach(_link IN LISTS _links)
            if(_link MATCHES "Qt[0-9]?::|Qt6::")
                message(FATAL_ERROR
                    "${target_name} must remain Qt-free for production distribution; "
                    "found Qt dependency '${_link}' through '${_current}'")
            endif()

            if(TARGET ${_link})
                list(APPEND _queue ${_link})
            endif()
        endforeach()
    endwhile()
endfunction()

function(saga_setup_distribution)
    if(NOT SAGA_WITH_SDE)
        add_custom_target(SagaDistribution
            COMMAND ${CMAKE_COMMAND} -E echo
                    "SagaDistribution requires SAGA_WITH_SDE=ON because Saga is the product entry point."
            COMMAND ${CMAKE_COMMAND} -E false
            COMMENT "Saga distribution requires SDE"
            VERBATIM
        )
        set_target_properties(SagaDistribution PROPERTIES
            FOLDER "Distribution"
        )
        return()
    endif()

    set(SAGA_DISTRIBUTION_VERSION "${CMAKE_PROJECT_VERSION}"
        CACHE STRING "SAGA production distribution version label" FORCE)

    _saga_distribution_platform(SAGA_DISTRIBUTION_PLATFORM)

    set(SAGA_DISTRIBUTION_NAME
        "SAGA-${SAGA_DISTRIBUTION_PLATFORM}-v${SAGA_DISTRIBUTION_VERSION}")
    set(SAGA_DISTRIBUTION_DIR
        "${CMAKE_BINARY_DIR}/dist/${SAGA_DISTRIBUTION_NAME}"
        CACHE PATH "SAGA staged production distribution directory")

    foreach(_target IN ITEMS Saga SagaRuntime SagaServer)
        if(NOT TARGET ${_target})
            message(FATAL_ERROR "SAGA distribution requires target '${_target}'")
        endif()
    endforeach()

    _saga_assert_no_qt_link(SagaRuntime)
    _saga_assert_no_qt_link(SagaServer)

    if(UNIX AND NOT APPLE)
        set_target_properties(Saga SagaRuntime SagaServer PROPERTIES
            INSTALL_RPATH "$ORIGIN/../lib"
        )
    endif()

    set(_version_json "${CMAKE_BINARY_DIR}/dist/version.json")
    configure_file(
        "${SAGA_ROOT}/cmake/templates/saga_version.json.in"
        "${_version_json}"
        @ONLY
    )

    install(TARGETS Saga
        RUNTIME_DEPENDENCY_SET SagaDistributionRuntimeDeps
        RUNTIME DESTINATION bin COMPONENT SagaDistribution
        BUNDLE  DESTINATION .   COMPONENT SagaDistribution
    )

    install(RUNTIME_DEPENDENCY_SET SagaDistributionRuntimeDeps
        PRE_EXCLUDE_REGEXES
            "api-ms-.*"
            "ext-ms-.*"
        POST_EXCLUDE_REGEXES
            "^/lib/.*"
            "^/lib64/.*"
            "^/usr/lib/.*"
            "^/usr/lib64/.*"
            ".*system32/.*"
        RUNTIME DESTINATION bin COMPONENT SagaDistribution
        LIBRARY DESTINATION lib COMPONENT SagaDistribution
        FRAMEWORK DESTINATION lib COMPONENT SagaDistribution
    )

    install(FILES "${_version_json}"
        DESTINATION .
        COMPONENT SagaDistribution
    )

    install(CODE
        "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/assets\")\n"
        "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/config\")\n"
        COMPONENT SagaDistribution
    )

    get_filename_component(_saga_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
    install(DIRECTORY "${_saga_qt_prefix}/plugins/"
        DESTINATION plugins
        COMPONENT SagaDistribution
        OPTIONAL
        FILES_MATCHING
            PATTERN "*.so"
            PATTERN "*.dylib"
            PATTERN "*.dll"
    )

    install(CODE
        "file(WRITE \"\${CMAKE_INSTALL_PREFIX}/bin/qt.conf\" \"[Paths]\\nPlugins = ../plugins\\n\")\n"
        COMPONENT SagaDistribution
    )

    set(_saga_fontconfig_res)
    file(GLOB _saga_fontconfig_data_files CONFIGURE_DEPENDS
        "${CMAKE_BINARY_DIR}/generators/Fontconfig-*-data.cmake"
        "${CMAKE_BINARY_DIR}/generators/module-Fontconfig-*-data.cmake"
    )
    foreach(_saga_fontconfig_data IN LISTS _saga_fontconfig_data_files)
        include("${_saga_fontconfig_data}" OPTIONAL)
        foreach(_fontconfig_cfg IN ITEMS RELWITHDEBINFO RELEASE DEBUG MINSIZEREL)
            set(_fontconfig_var "fontconfig_PACKAGE_FOLDER_${_fontconfig_cfg}")
            if(DEFINED ${_fontconfig_var})
                set(_fontconfig_root "${${_fontconfig_var}}")
                if(EXISTS "${_fontconfig_root}/res/etc/fonts/fonts.conf")
                    set(_saga_fontconfig_res
                        "${_fontconfig_root}/res/etc/fonts")
                    break()
                endif()
            endif()
        endforeach()
        if(_saga_fontconfig_res)
            break()
        endif()
    endforeach()

    if(TARGET Fontconfig::Fontconfig)
        get_target_property(_fontconfig_includes
            Fontconfig::Fontconfig INTERFACE_INCLUDE_DIRECTORIES)
        foreach(_fontconfig_include IN LISTS _fontconfig_includes)
            string(REGEX REPLACE
                "^\\$<\\$<CONFIG:[^>]+>:(.*)>$"
                "\\1"
                _fontconfig_include_path
                "${_fontconfig_include}")
            if(EXISTS "${_fontconfig_include_path}/fontconfig/fontconfig.h")
                get_filename_component(_fontconfig_root
                    "${_fontconfig_include_path}/.." ABSOLUTE)
                if(EXISTS "${_fontconfig_root}/res/etc/fonts/fonts.conf")
                    set(_saga_fontconfig_res
                        "${_fontconfig_root}/res/etc/fonts")
                    break()
                endif()
            endif()
        endforeach()
    endif()
    if(NOT _saga_fontconfig_res)
        foreach(_fontconfig_cfg IN ITEMS RELWITHDEBINFO RELEASE DEBUG MINSIZEREL)
            set(_fontconfig_var "fontconfig_PACKAGE_FOLDER_${_fontconfig_cfg}")
            if(DEFINED ${_fontconfig_var})
                set(_fontconfig_root "${${_fontconfig_var}}")
                if(EXISTS "${_fontconfig_root}/res/etc/fonts/fonts.conf")
                    set(_saga_fontconfig_res
                        "${_fontconfig_root}/res/etc/fonts")
                    break()
                endif()
            endif()
        endforeach()
    endif()

    if(_saga_fontconfig_res)
        install(DIRECTORY "${_saga_fontconfig_res}/"
            DESTINATION config/fontconfig
            COMPONENT SagaDistribution
        )
    endif()

    install(DIRECTORY "${SAGA_ROOT}/Apps/Saga/Definitions/BasicWorkspace/"
        DESTINATION config/workspaces/basic
        COMPONENT SagaDistribution
    )

    if(EXISTS "${SAGA_ROOT}/LICENSES")
        install(DIRECTORY "${SAGA_ROOT}/LICENSES/"
            DESTINATION LICENSES
            COMPONENT SagaDistribution
        )
    endif()

    if(EXISTS "${SAGA_ROOT}/LICENSE.md")
        install(FILES "${SAGA_ROOT}/LICENSE.md"
            DESTINATION LICENSES
            RENAME LICENSE-SAGA.md
            COMPONENT SagaDistribution
        )
    endif()

    add_custom_target(SagaDistribution
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${SAGA_DISTRIBUTION_DIR}"
        COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}"
                --prefix "${SAGA_DISTRIBUTION_DIR}"
                --component SagaDistribution
        DEPENDS Saga
        COMMENT "Staging ${SAGA_DISTRIBUTION_NAME}"
        VERBATIM
    )

    set_target_properties(SagaDistribution PROPERTIES
        FOLDER "Distribution"
    )

    message(STATUS "SAGA distribution: ${SAGA_DISTRIBUTION_DIR}")
endfunction()
