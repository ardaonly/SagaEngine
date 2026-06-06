option(SAGA_USE_VENDOR_DILIGENT "Use vendored DiligentCore source" ON)
option(SAGA_ENABLE_VULKAN "Enable Diligent Vulkan backend" ON)
option(SAGA_ENABLE_OPENGL "Enable Diligent OpenGL backend" ON)
option(SAGA_ENABLE_D3D11 "Enable Diligent D3D11 backend" ON)
option(SAGA_ENABLE_D3D12 "Enable Diligent D3D12 backend" OFF)
option(SAGA_ENABLE_METAL "Enable Diligent Metal backend" OFF)

function(saga_setup_diligent_vendor)
    if(NOT SAGA_USE_VENDOR_DILIGENT)
        message(FATAL_ERROR
            "SAGA_USE_VENDOR_DILIGENT=OFF is not supported in this migration. "
            "DiligentCore is no longer resolved from Conan.")
    endif()

    set(_saga_diligent_root "${SAGA_ROOT}/Vendor/Diligent")
    if(NOT EXISTS "${_saga_diligent_root}/CMakeLists.txt")
        message(FATAL_ERROR
            "Vendored DiligentCore is missing. Initialize the submodule with:\n"
            "  git submodule update --init --recursive Vendor/Diligent\n"
            "Expected CMakeLists.txt at: ${_saga_diligent_root}/CMakeLists.txt")
    endif()

    set(DILIGENT_INSTALL_CORE OFF CACHE BOOL
        "Do not install vendored DiligentCore from SagaEngine builds" FORCE)
    set(DILIGENT_INSTALL_FX OFF CACHE BOOL
        "SagaEngine R1 vendors DiligentCore only, not DiligentFX" FORCE)
    set(DILIGENT_INSTALL_TOOLS OFF CACHE BOOL
        "SagaEngine R1 vendors DiligentCore only, not DiligentTools" FORCE)
    set(DILIGENT_INSTALL_SAMPLES OFF CACHE BOOL
        "SagaEngine R1 vendors DiligentCore only, not Diligent samples" FORCE)

    add_subdirectory("${_saga_diligent_root}" "${CMAKE_BINARY_DIR}/Vendor/Diligent")

    add_library(VendorDiligent INTERFACE)

    set(_saga_diligent_targets "")
    if(WIN32 AND SAGA_ENABLE_D3D12)
        list(APPEND _saga_diligent_targets
            Diligent-GraphicsEngineD3D12-static
        )
    endif()

    if(SAGA_ENABLE_VULKAN)
        list(APPEND _saga_diligent_targets
            Diligent-GraphicsEngineVk-static
        )
    endif()

    if(SAGA_ENABLE_OPENGL)
        list(APPEND _saga_diligent_targets
            Diligent-GraphicsEngineOpenGL-static
        )
    endif()

    if(WIN32 AND SAGA_ENABLE_D3D11)
        list(APPEND _saga_diligent_targets
            Diligent-GraphicsEngineD3D11-static
        )
    endif()

    set(_saga_resolved_diligent_targets "")
    foreach(_saga_diligent_target IN LISTS _saga_diligent_targets)
        if(TARGET ${_saga_diligent_target})
            list(APPEND _saga_resolved_diligent_targets ${_saga_diligent_target})
        endif()
    endforeach()

    if(NOT _saga_resolved_diligent_targets)
        message(FATAL_ERROR
            "Vendored DiligentCore configured, but no supported graphics engine "
            "targets were found. Checked: ${_saga_diligent_targets}")
    endif()

    target_link_libraries(VendorDiligent INTERFACE
        ${_saga_resolved_diligent_targets}
    )

    target_include_directories(VendorDiligent INTERFACE
        "${_saga_diligent_root}/Common/interface"
        "${_saga_diligent_root}/Graphics/GraphicsTools/interface"
    )

    if(Vulkan_FOUND)
        target_link_libraries(VendorDiligent INTERFACE Vulkan::Vulkan)
    endif()

    if(WIN32)
        target_link_libraries(VendorDiligent INTERFACE
            d3dcompiler.lib
            dxgi.lib
            d3d12.lib
        )
    endif()
endfunction()

function(saga_link_diligent_backend target_name)
    target_link_libraries(${target_name} PRIVATE
        VendorDiligent
    )
endfunction()
