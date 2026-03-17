function(saga_setup_thirdparty)
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan/conanbuildinfo.cmake")
        message(FATAL_ERROR "Run './build.sh setup' before CMake configure")
    endif()
    
    include("${CMAKE_BINARY_DIR}/conan/conanbuildinfo.cmake")
    
    find_package(Boost 1.84.0 REQUIRED COMPONENTS system filesystem)
    find_package(asio 1.29.0 REQUIRED)
    find_package(GTest 1.14.0 REQUIRED)
    find_package(FreeType 2.13.2 REQUIRED)
    find_package(RmlUi 5.1 REQUIRED)
    find_package(Qt6 6.6.2 REQUIRED COMPONENTS Core Gui Widgets)
    find_package(libpqxx 7.9.0 REQUIRED)
    find_package(hiredis 1.1.0 REQUIRED)
    find_package(redis-plus-plus 1.3.12 REQUIRED)
    find_package(OpenSSL 3.2.0 REQUIRED)
    find_package(ZLIB 1.3.1 REQUIRED)
    
    if(SAGA_ENABLE_VULKAN)
        find_package(Vulkan 1.3.275 REQUIRED)
    endif()
    
    if(SAGA_ENABLE_OPENGL)
        find_package(OpenGL REQUIRED)
    endif()
endfunction()

function(saga_link_thirdparty target_name)
    target_link_libraries(${target_name} PRIVATE
        Boost::system
        Boost::filesystem
        asio::asio
        GTest::gtest
        GTest::gmock
        freetype::freetype
        RmlUi::RmlUi
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        pqxx::pqxx
        hiredis::hiredis
        redis-plus-plus::redis-plus-plus
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        SagaVersion
    )
    
    if(SAGA_ENABLE_VULKAN)
        target_link_libraries(${target_name} PRIVATE Vulkan::Vulkan)
        target_compile_definitions(${target_name} PRIVATE SAGA_VULKAN_ENABLED)
    endif()
    
    if(SAGA_ENABLE_OPENGL)
        target_link_libraries(${target_name} PRIVATE OpenGL::GL)
        target_compile_definitions(${target_name} PRIVATE SAGA_OPENGL_ENABLED)
    endif()
    
    if(SAGA_ENABLE_D3D11)
        target_compile_definitions(${target_name} PRIVATE SAGA_D3D11_ENABLED)
        if(WIN32)
            target_link_libraries(${target_name} PRIVATE d3d11 dxgi)
        endif()
    endif()
    
    if(SAGA_ENABLE_D3D12)
        target_compile_definitions(${target_name} PRIVATE SAGA_D3D12_ENABLED)
        if(WIN32)
            target_link_libraries(${target_name} PRIVATE d3d12 dxgi dxguid)
        endif()
    endif()
    
    if(SAGA_ENABLE_METAL)
        target_compile_definitions(${target_name} PRIVATE SAGA_METAL_ENABLED)
        if(APPLE)
            target_link_libraries(${target_name} PRIVATE "-framework Metal" "-framework QuartzCore")
        endif()
    endif()
endfunction()