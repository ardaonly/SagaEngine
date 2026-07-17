function(saga_setup_thirdparty)
    find_package(asio CONFIG REQUIRED)
    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Widgets)
    find_package(libpqxx CONFIG REQUIRED)
    find_package(hiredis CONFIG REQUIRED)
    find_package(redis++ CONFIG REQUIRED)
    find_package(Vulkan QUIET)
    find_package(GTest CONFIG REQUIRED)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(rmlui CONFIG REQUIRED)
    find_package(rapidcheck CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    find_program(SAGA_DOTNET_EXECUTABLE dotnet REQUIRED)

    set(_saga_rmlui_target "")
    foreach(_saga_candidate IN ITEMS
        rmlui::rmlui
        RmlUi::RmlUi
        RmlUi::Core
        RmlUi::RmlUi_Core
    )
        if(TARGET ${_saga_candidate})
            set(_saga_rmlui_target ${_saga_candidate})
            break()
        endif()
    endforeach()

    if(NOT _saga_rmlui_target)
        message(FATAL_ERROR
            "rmlui package was found, but no supported CMake target was exported")
    endif()

    set(SAGA_RMLUI_TARGET "${_saga_rmlui_target}" CACHE INTERNAL
        "RmlUi CMake target used by the backend-owned UI adapter")

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

    if(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(SAGA_DOTNET_HOST_RID "win-x64")
        else()
            set(SAGA_DOTNET_HOST_RID "win-x86")
        endif()
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
            set(SAGA_DOTNET_HOST_RID "osx-arm64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
            set(SAGA_DOTNET_HOST_RID "osx-x64")
        else()
            message(FATAL_ERROR
                "Unsupported macOS architecture for .NET hosting: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()
    elseif(UNIX)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
            set(SAGA_DOTNET_HOST_RID "linux-arm64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
            set(SAGA_DOTNET_HOST_RID "linux-x64")
        else()
            message(FATAL_ERROR
                "Unsupported Linux architecture for .NET hosting: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()
    else()
        message(FATAL_ERROR
            "Unsupported platform for .NET hosting: ${CMAKE_SYSTEM_NAME}")
    endif()

    file(GLOB SAGA_DOTNET_HOST_NATIVE_DIR_CANDIDATES
        LIST_DIRECTORIES true
        "${SAGA_DOTNET_ROOT}/packs/Microsoft.NETCore.App.Host.${SAGA_DOTNET_HOST_RID}/*/runtimes/${SAGA_DOTNET_HOST_RID}/native"
    )
    if(NOT SAGA_DOTNET_HOST_NATIVE_DIR_CANDIDATES)
        message(FATAL_ERROR
            "No .NET native host pack found for ${SAGA_DOTNET_HOST_RID} under ${SAGA_DOTNET_ROOT}/packs")
    endif()
    list(SORT SAGA_DOTNET_HOST_NATIVE_DIR_CANDIDATES
        COMPARE NATURAL
        ORDER DESCENDING)
    list(GET SAGA_DOTNET_HOST_NATIVE_DIR_CANDIDATES 0
        SAGA_DOTNET_HOST_NATIVE_DIR)
    message(STATUS
        "Saga .NET native host: ${SAGA_DOTNET_HOST_RID} -> ${SAGA_DOTNET_HOST_NATIVE_DIR}")

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
    set(SAGA_DOTNET_HOST_RID "${SAGA_DOTNET_HOST_RID}" CACHE INTERNAL
        "dotnet native host runtime identifier")
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

function(saga_link_rmlui target_name)
    target_compile_definitions(${target_name} PRIVATE
        RMLUI_STATIC_LIB
    )

    target_link_libraries(${target_name} PRIVATE
        ${SAGA_RMLUI_TARGET}
    )
endfunction()

function(saga_link_thirdparty target_name)
    target_link_libraries(${target_name} PRIVATE
        asio::asio
        libpqxx::pqxx
        hiredis::hiredis
        redis++::redis++_static
        SDL2::SDL2
        imgui::imgui
        glm::glm
        nlohmann_json::nlohmann_json
    )

    if(DEFINED imgui_RES_DIRS)
        target_include_directories(${target_name} PRIVATE
            ${imgui_RES_DIRS}/bindings
        )
    endif()
endfunction()
