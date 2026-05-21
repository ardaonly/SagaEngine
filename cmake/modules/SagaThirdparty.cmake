function(saga_setup_thirdparty)
    find_package(asio CONFIG REQUIRED)
    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Widgets)
    find_package(libpqxx CONFIG REQUIRED)
    find_package(hiredis CONFIG REQUIRED)
    find_package(redis++ CONFIG REQUIRED)
    find_package(diligent-core CONFIG REQUIRED)
    find_package(Vulkan QUIET)
    find_package(GTest CONFIG REQUIRED)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(rapidcheck CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    find_program(SAGA_DOTNET_EXECUTABLE dotnet REQUIRED)

    set(SAGA_OPENSSL_INCLUDE_DIRS "${OpenSSL_INCLUDE_DIRS}" CACHE INTERNAL
        "OpenSSL include directories for direct validator compilation"
    )
    if(NOT SAGA_OPENSSL_INCLUDE_DIRS)
        string(TOUPPER "${CMAKE_BUILD_TYPE}" _saga_openssl_config)
        set(_saga_openssl_package_var "openssl_PACKAGE_FOLDER_${_saga_openssl_config}")
        if(DEFINED ${_saga_openssl_package_var})
            find_path(SAGA_OPENSSL_INCLUDE_DIR
                NAMES openssl/evp.h
                HINTS "${${_saga_openssl_package_var}}/include"
                NO_DEFAULT_PATH
            )
            set(SAGA_OPENSSL_INCLUDE_DIRS "${SAGA_OPENSSL_INCLUDE_DIR}" CACHE INTERNAL
                "OpenSSL include directories for direct validator compilation"
            )
        endif()
    endif()

    get_filename_component(SAGA_DOTNET_EXECUTABLE_REAL
        "${SAGA_DOTNET_EXECUTABLE}" REALPATH)
    get_filename_component(SAGA_DOTNET_ROOT
        "${SAGA_DOTNET_EXECUTABLE_REAL}" DIRECTORY)
    file(GLOB SAGA_DOTNET_HOST_NATIVE_DIR_CANDIDATES
        "${SAGA_DOTNET_ROOT}/packs/Microsoft.NETCore.App.Host.*/*/runtimes/*/native"
    )
    list(SORT SAGA_DOTNET_HOST_NATIVE_DIR_CANDIDATES)
    list(REVERSE SAGA_DOTNET_HOST_NATIVE_DIR_CANDIDATES)
    list(GET SAGA_DOTNET_HOST_NATIVE_DIR_CANDIDATES 0
        SAGA_DOTNET_HOST_NATIVE_DIR)

    find_path(SAGA_DOTNET_HOST_INCLUDE_DIR
        NAMES nethost.h hostfxr.h coreclr_delegates.h
        HINTS "${SAGA_DOTNET_HOST_NATIVE_DIR}"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(SAGA_DOTNET_NETHOST_LIBRARY
        NAMES nethost
        HINTS "${SAGA_DOTNET_HOST_NATIVE_DIR}"
        NO_DEFAULT_PATH
        REQUIRED
    )
    set(SAGA_DOTNET_EXECUTABLE "${SAGA_DOTNET_EXECUTABLE}" CACHE INTERNAL
        "dotnet executable for managed SagaScript bridge builds")
    set(SAGA_DOTNET_ROOT "${SAGA_DOTNET_ROOT}" CACHE INTERNAL
        "dotnet root for native hostfxr/nethost lookup")
    set(SAGA_DOTNET_HOST_INCLUDE_DIR "${SAGA_DOTNET_HOST_INCLUDE_DIR}" CACHE INTERNAL
        "dotnet host native include directory")
    set(SAGA_DOTNET_NETHOST_LIBRARY "${SAGA_DOTNET_NETHOST_LIBRARY}" CACHE INTERNAL
        "dotnet nethost library")

    # -- ATL for Diligent static backends ----------------------------------
    # Diligent's D3D12/D3D11 engine factory static libs use ATL (atls.lib).
    # MSVC's ATL headers/libs are NOT on the default linker search path.
    # We do a thorough multi-strategy search to find atls.lib.
    if(MSVC)
        saga_find_atl()
    endif()
endfunction()

function(saga_link_thirdparty target_name)
    target_link_libraries(${target_name} PRIVATE
        asio::asio
        libpqxx::pqxx
        hiredis::hiredis
        redis++::redis++_static
        diligent-core::diligent-core
        SDL2::SDL2
        imgui::imgui
        glm::glm
        nlohmann_json::nlohmann_json
    )

    if(Vulkan_FOUND)
        target_link_libraries(${target_name} PRIVATE Vulkan::Vulkan)
    endif()

    # Diligent's D3D12/D3D11 static backends use D3DCompile, D3DReflect,
    # D3DCreateBlob from d3dcompiler.lib and DXGI functions from dxgi.lib.
    # The conan package only declares dxgi+shlwapi; d3dcompiler is missing.
    if(WIN32)
        target_link_libraries(${target_name} PRIVATE
            d3dcompiler.lib
            dxgi.lib
            d3d12.lib
        )
    endif()

    if(DEFINED imgui_RES_DIRS)
        target_include_directories(${target_name} PRIVATE
            ${imgui_RES_DIRS}/bindings
        )
    endif()
endfunction()
