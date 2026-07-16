cmake_minimum_required(VERSION 3.28)

project(SagaGraphicsInstalledConsumer LANGUAGES CXX)

find_package(SagaEngine CONFIG REQUIRED)

if(TARGET SagaGraphics)
    message(FATAL_ERROR "Downstream must not see build-tree SagaGraphics")
endif()
if(TARGET SagaEngine)
    message(FATAL_ERROR "Downstream must not see build-tree SagaEngine")
endif()
if(TARGET SagaGraphicsPrivate)
    message(FATAL_ERROR "Downstream must not see SagaGraphicsPrivate")
endif()
if(TARGET SagaDiligentBackend)
    message(FATAL_ERROR "Downstream must not see SagaDiligentBackend")
endif()
if(NOT TARGET SagaEngine::SagaGraphics)
    message(FATAL_ERROR "Installed package must export SagaEngine::SagaGraphics")
endif()
if(NOT TARGET SagaEngine::SagaGraphicsCore)
    message(FATAL_ERROR "Installed package must export SagaEngine::SagaGraphicsCore")
endif()

foreach(_required_target IN ITEMS
    SagaCoreLog
    SagaDiagnostics
    SagaShared
    SagaCollaboration
    SagaEngine
    SagaRuntimeLib
    SagaServerLib
    SagaBackend)
    if(NOT TARGET SagaEngine::${_required_target})
        message(FATAL_ERROR
            "Installed package must export SagaEngine::${_required_target}")
    endif()
endforeach()

foreach(_private_target IN ITEMS
    SagaCoreModule
    SagaMathModule
    SagaSagaRuntimeModule
    SagaServerAuthorityModule)
    if(TARGET SagaEngine::${_private_target})
        message(FATAL_ERROR
            "Installed package must not export private object target ${_private_target}")
    endif()
endforeach()

get_target_property(_links SagaEngine::SagaGraphics INTERFACE_LINK_LIBRARIES)
if(NOT _links MATCHES "SagaEngine::SagaGraphicsCore")
    message(FATAL_ERROR "SagaEngine::SagaGraphics must link SagaGraphicsCore transitively")
endif()

foreach(_target IN ITEMS SagaEngine::SagaGraphics SagaEngine::SagaGraphicsCore)
    get_target_property(_includes ${_target} INTERFACE_INCLUDE_DIRECTORIES)
    foreach(_include IN LISTS _includes)
        string(FIND "${_include}" "${SAGA_PUBLIC_SOURCE_INCLUDE_FORBIDDEN}" _source_public_pos)
        string(FIND "${_include}" "${SAGA_PRIVATE_SOURCE_INCLUDE_FORBIDDEN}" _source_private_pos)
        if(NOT _source_public_pos EQUAL -1)
            message(FATAL_ERROR "${_target} leaks source include directory: ${_include}")
        endif()
        if(NOT _source_private_pos EQUAL -1 OR _include MATCHES "Engine/Private")
            message(FATAL_ERROR "${_target} leaks private include directory: ${_include}")
        endif()
    endforeach()
endforeach()

add_executable(SagaGraphicsInstalledConsumer main.cpp)
target_compile_features(SagaGraphicsInstalledConsumer PRIVATE cxx_std_20)
target_link_libraries(SagaGraphicsInstalledConsumer PRIVATE
    SagaEngine::SagaGraphics
    SagaEngine::SagaEngine
    SagaEngine::SagaRuntimeLib
    SagaEngine::SagaServerLib
    SagaEngine::SagaBackend
)
