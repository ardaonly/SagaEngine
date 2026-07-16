include(CMakeParseArguments)

function(_saga_register_module kind module_name)
    set(one_value_args MODULE_DIR TARGET)
    set(multi_value_args
        SOURCES
        PUBLIC_DEPS
        PRIVATE_DEPS
        PUBLIC_INCLUDES
        PRIVATE_INCLUDES
    )
    cmake_parse_arguments(ARG "" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT ARG_MODULE_DIR OR NOT ARG_TARGET)
        message(FATAL_ERROR "${kind} module ${module_name} requires MODULE_DIR and TARGET")
    endif()

    set(_sources)
    foreach(_source IN LISTS ARG_SOURCES)
        if(IS_ABSOLUTE "${_source}" OR
           _source MATCHES "(^|/)\\.\\.(/|$)")
            message(FATAL_ERROR
                "${kind} module ${module_name} source must stay below "
                "${ARG_MODULE_DIR}: ${_source}")
        endif()
        list(APPEND _sources "${SAGA_ROOT}/${ARG_MODULE_DIR}/${_source}")
    endforeach()

    set_property(GLOBAL APPEND PROPERTY "SAGA_MODULE_${ARG_TARGET}_SOURCES" ${_sources})
    if(NOT kind STREQUAL "program")
        set_property(GLOBAL APPEND PROPERTY "SAGA_MODULE_${ARG_TARGET}_PUBLIC_INCLUDES"
            "${SAGA_ROOT}/${ARG_MODULE_DIR}/Public")
    endif()
    set_property(GLOBAL APPEND PROPERTY "SAGA_MODULE_${ARG_TARGET}_PUBLIC_INCLUDES"
        ${ARG_PUBLIC_INCLUDES})
    set_property(GLOBAL APPEND PROPERTY "SAGA_MODULE_${ARG_TARGET}_PRIVATE_INCLUDES"
        "${SAGA_ROOT}/${ARG_MODULE_DIR}/Private"
        ${ARG_PRIVATE_INCLUDES})
    set_property(GLOBAL APPEND PROPERTY "SAGA_MODULE_${ARG_TARGET}_PUBLIC_DEPS" ${ARG_PUBLIC_DEPS})
    set_property(GLOBAL APPEND PROPERTY "SAGA_MODULE_${ARG_TARGET}_PRIVATE_DEPS" ${ARG_PRIVATE_DEPS})

    if(ARG_TARGET IN_LIST SAGA_COMPOSITION_TARGETS)
        set(_object_target "Saga${module_name}Module")
        get_property(_existing_aggregate GLOBAL
            PROPERTY "SAGA_OBJECT_${_object_target}_AGGREGATE")
        if(_existing_aggregate AND NOT _existing_aggregate STREQUAL ARG_TARGET)
            message(FATAL_ERROR
                "Object module ${_object_target} is already assigned to "
                "${_existing_aggregate}, not ${ARG_TARGET}")
        endif()
        set_property(GLOBAL PROPERTY
            "SAGA_OBJECT_${_object_target}_AGGREGATE" "${ARG_TARGET}")
        set_property(GLOBAL PROPERTY
            "SAGA_OBJECT_${_object_target}_MODULE_DIR"
            "${SAGA_ROOT}/${ARG_MODULE_DIR}")
        set_property(GLOBAL APPEND PROPERTY
            "SAGA_OBJECT_${_object_target}_SOURCES" ${_sources})
        set_property(GLOBAL APPEND PROPERTY
            "SAGA_OBJECT_${_object_target}_PUBLIC_DEPS" ${ARG_PUBLIC_DEPS})
        set_property(GLOBAL APPEND PROPERTY
            "SAGA_OBJECT_${_object_target}_PRIVATE_DEPS" ${ARG_PRIVATE_DEPS})
        set_property(GLOBAL APPEND PROPERTY
            "SAGA_OBJECT_${_object_target}_PUBLIC_INCLUDES"
            ${ARG_PUBLIC_INCLUDES})
        set_property(GLOBAL APPEND PROPERTY
            "SAGA_OBJECT_${_object_target}_PRIVATE_INCLUDES"
            ${ARG_PRIVATE_INCLUDES})
        set_property(GLOBAL APPEND PROPERTY
            "SAGA_COMPOSITION_${ARG_TARGET}_OBJECTS" "${_object_target}")
    endif()
endfunction()

function(saga_add_runtime_module module_name)
    _saga_register_module(runtime "${module_name}" ${ARGN})
endfunction()

function(saga_add_editor_module module_name)
    _saga_register_module(editor "${module_name}" ${ARGN})
endfunction()

function(saga_add_program program_name)
    _saga_register_module(program "${program_name}" ${ARGN})
endfunction()

function(saga_get_registered_sources target_name out_var)
    get_property(_sources GLOBAL PROPERTY "SAGA_MODULE_${target_name}_SOURCES")
    set(${out_var} ${_sources} PARENT_SCOPE)
endfunction()

function(saga_create_registered_object_modules aggregate_target out_var)
    get_property(_object_targets GLOBAL
        PROPERTY "SAGA_COMPOSITION_${aggregate_target}_OBJECTS")
    if(_object_targets)
        list(REMOVE_DUPLICATES _object_targets)
    endif()

    foreach(_object_target IN LISTS _object_targets)
        get_property(_module_dir GLOBAL
            PROPERTY "SAGA_OBJECT_${_object_target}_MODULE_DIR")
        get_property(_sources GLOBAL
            PROPERTY "SAGA_OBJECT_${_object_target}_SOURCES")
        get_property(_public_deps GLOBAL
            PROPERTY "SAGA_OBJECT_${_object_target}_PUBLIC_DEPS")
        get_property(_private_deps GLOBAL
            PROPERTY "SAGA_OBJECT_${_object_target}_PRIVATE_DEPS")
        get_property(_public_includes GLOBAL
            PROPERTY "SAGA_OBJECT_${_object_target}_PUBLIC_INCLUDES")
        get_property(_private_includes GLOBAL
            PROPERTY "SAGA_OBJECT_${_object_target}_PRIVATE_INCLUDES")

        add_library(${_object_target} OBJECT)
        saga_apply_compiler_flags(${_object_target})
        target_sources(${_object_target} PRIVATE ${_sources})
        target_include_directories(${_object_target} PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            "${_module_dir}"
            "${_module_dir}/Public"
            "${_module_dir}/Private"
            "${_module_dir}/Source"
            ${_public_includes}
            ${_private_includes}
        )
        if(_public_deps OR _private_deps)
            target_link_libraries(${_object_target} PRIVATE
                ${_public_deps}
                ${_private_deps}
            )
        endif()
        set_target_properties(${_object_target} PROPERTIES
            FOLDER "Modules/${aggregate_target}"
        )
    endforeach()

    set(${out_var} ${_object_targets} PARENT_SCOPE)
endfunction()

function(saga_compose_registered_objects aggregate_target)
    get_property(_object_targets GLOBAL
        PROPERTY "SAGA_COMPOSITION_${aggregate_target}_OBJECTS")
    if(_object_targets)
        list(REMOVE_DUPLICATES _object_targets)
    endif()
    foreach(_object_target IN LISTS _object_targets)
        target_sources(${aggregate_target} PRIVATE
            "$<TARGET_OBJECTS:${_object_target}>"
        )
    endforeach()
endfunction()

function(saga_apply_registered_module target_name)
    get_property(_sources GLOBAL PROPERTY "SAGA_MODULE_${target_name}_SOURCES")
    get_property(_public_includes GLOBAL PROPERTY "SAGA_MODULE_${target_name}_PUBLIC_INCLUDES")
    get_property(_private_includes GLOBAL PROPERTY "SAGA_MODULE_${target_name}_PRIVATE_INCLUDES")
    get_property(_public_deps GLOBAL PROPERTY "SAGA_MODULE_${target_name}_PUBLIC_DEPS")
    get_property(_private_deps GLOBAL PROPERTY "SAGA_MODULE_${target_name}_PRIVATE_DEPS")

    if(_sources)
        target_sources(${target_name} PRIVATE ${_sources})
    endif()
    if(_public_includes)
        list(REMOVE_DUPLICATES _public_includes)
        set(_build_public_includes)
        foreach(_include IN LISTS _public_includes)
            list(APPEND _build_public_includes "$<BUILD_INTERFACE:${_include}>")
        endforeach()
        target_include_directories(${target_name} PUBLIC
            ${_build_public_includes}
            $<INSTALL_INTERFACE:include>)
    endif()
    if(_private_includes)
        list(REMOVE_DUPLICATES _private_includes)
        target_include_directories(${target_name} PRIVATE ${_private_includes})
    endif()
    if(_public_deps)
        list(REMOVE_DUPLICATES _public_deps)
        target_link_libraries(${target_name} PUBLIC ${_public_deps})
    endif()
    if(_private_deps)
        list(REMOVE_DUPLICATES _private_deps)
        target_link_libraries(${target_name} PRIVATE ${_private_deps})
    endif()
endfunction()

function(saga_apply_registered_usage target_name)
    get_property(_public_includes GLOBAL PROPERTY "SAGA_MODULE_${target_name}_PUBLIC_INCLUDES")
    get_property(_private_includes GLOBAL PROPERTY "SAGA_MODULE_${target_name}_PRIVATE_INCLUDES")
    get_property(_public_deps GLOBAL PROPERTY "SAGA_MODULE_${target_name}_PUBLIC_DEPS")
    get_property(_private_deps GLOBAL PROPERTY "SAGA_MODULE_${target_name}_PRIVATE_DEPS")

    if(_public_includes)
        list(REMOVE_DUPLICATES _public_includes)
        set(_build_public_includes)
        foreach(_include IN LISTS _public_includes)
            list(APPEND _build_public_includes "$<BUILD_INTERFACE:${_include}>")
        endforeach()
        target_include_directories(${target_name} PUBLIC
            ${_build_public_includes}
            $<INSTALL_INTERFACE:include>)
    endif()
    if(_private_includes)
        list(REMOVE_DUPLICATES _private_includes)
        target_include_directories(${target_name} PRIVATE ${_private_includes})
    endif()
    if(_public_deps)
        list(REMOVE_DUPLICATES _public_deps)
        target_link_libraries(${target_name} PUBLIC ${_public_deps})
    endif()
    if(_private_deps)
        list(REMOVE_DUPLICATES _private_deps)
        target_link_libraries(${target_name} PRIVATE ${_private_deps})
    endif()
endfunction()

function(saga_register_source_modules)
    set(SAGA_COMPOSITION_TARGETS
        SagaEngine
        SagaRuntimeLib
        SagaServerLib
        SagaEditorLib
        SagaProductLib
    )
    set(_module_files
        Engine/Source/Runtime/Core/CMakeLists.txt
        Engine/Source/Runtime/Math/CMakeLists.txt
        Engine/Source/Runtime/Diagnostics/CMakeLists.txt
        Engine/Source/Runtime/ECS/CMakeLists.txt
        Engine/Source/Runtime/Assets/CMakeLists.txt
        Engine/Source/Runtime/Resources/CMakeLists.txt
        Engine/Source/Runtime/Input/CMakeLists.txt
        Engine/Source/Runtime/Audio/CMakeLists.txt
        Engine/Source/Runtime/Render/CMakeLists.txt
        Engine/Source/Runtime/RHI/CMakeLists.txt
        Engine/Source/Runtime/UI/CMakeLists.txt
        Engine/Source/Runtime/Scripting/CMakeLists.txt
        Engine/Source/Runtime/Networking/CMakeLists.txt
        Engine/Source/Runtime/Replication/CMakeLists.txt
        Engine/Source/Runtime/Persistence/CMakeLists.txt
        Engine/Source/Runtime/Simulation/CMakeLists.txt
        Engine/Source/Runtime/World/CMakeLists.txt
        Engine/Source/Runtime/ServerAuthority/CMakeLists.txt
        Engine/Source/Runtime/SagaRuntime/CMakeLists.txt
        Engine/Source/Editor/EditorCore/CMakeLists.txt
        Engine/Source/Editor/EditorFramework/CMakeLists.txt
        Engine/Source/Editor/EditorQt/CMakeLists.txt
        Engine/Source/Editor/EditorAuthoring/CMakeLists.txt
        Engine/Source/Editor/VisualBlocksEditor/CMakeLists.txt
        Engine/Source/Editor/EditorCollaboration/CMakeLists.txt
        Engine/Source/Editor/EditorExperimental/CMakeLists.txt
        Engine/Source/Programs/SagaLauncher/CMakeLists.txt
        Engine/Source/Programs/SagaEditor/CMakeLists.txt
        Engine/Source/Programs/SagaRuntime/CMakeLists.txt
        Engine/Source/Programs/SagaSandbox/CMakeLists.txt
        Engine/Source/Programs/SagaEditorLab/CMakeLists.txt
        Tools/SagaPackager/CMakeLists.txt
        Tools/SagaScript/CMakeLists.txt
        Tests/Evidence/FirstPlayable/CMakeLists.txt
    )
    foreach(_module_file IN LISTS _module_files)
        include("${SAGA_ROOT}/${_module_file}")
    endforeach()

    set(_public_includes)
    set(_private_includes)
    foreach(_target IN ITEMS
        SagaCoreLog SagaDiagnostics SagaEngine SagaGraphicsCore SagaGraphicsPrivate
        SagaDiligentRuntime SagaDiligentBackend SagaPlatformSDL SagaBackend
        SagaRuntimeLib SagaServerLib SagaShared SagaCollaboration SagaEditorLib
        SagaEditorLabLib SagaSandboxLib SagaProductLib Saga SagaEditor SagaRuntime
        SagaSandbox EditorLab)
        get_property(_target_public GLOBAL PROPERTY "SAGA_MODULE_${_target}_PUBLIC_INCLUDES")
        get_property(_target_private GLOBAL PROPERTY "SAGA_MODULE_${_target}_PRIVATE_INCLUDES")
        list(APPEND _public_includes ${_target_public})
        list(APPEND _private_includes ${_target_private})
    endforeach()
    list(REMOVE_DUPLICATES _public_includes)
    list(REMOVE_DUPLICATES _private_includes)
    set(SAGA_MODULE_PUBLIC_INCLUDE_DIRS ${_public_includes} PARENT_SCOPE)
    set(SAGA_MODULE_PRIVATE_INCLUDE_DIRS ${_private_includes} PARENT_SCOPE)
endfunction()
