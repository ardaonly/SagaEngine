function(saga_setup_thirdparty)
    find_package(asio CONFIG REQUIRED)
    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Widgets)
    find_package(libpqxx CONFIG REQUIRED)
    find_package(hiredis CONFIG REQUIRED)
    find_package(redis++ CONFIG REQUIRED)
    find_package(diligent-core CONFIG REQUIRED)
    find_package(GTest CONFIG REQUIRED)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(rapidcheck CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)

    # ── ATL for Diligent static backends ──────────────────────────────────
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
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        libpqxx::pqxx
        hiredis::hiredis
        redis++::redis++_static
        diligent-core::diligent-core
        SDL2::SDL2
        imgui::imgui
        glm::glm
    )

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