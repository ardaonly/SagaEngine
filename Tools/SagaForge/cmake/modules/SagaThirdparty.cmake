function(saga_setup_thirdparty)
    find_package(asio CONFIG REQUIRED)
    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Widgets)
    find_package(libpqxx CONFIG REQUIRED)
    find_package(hiredis CONFIG REQUIRED)
    find_package(redis++ CONFIG REQUIRED)
    find_package(diligent-core CONFIG REQUIRED)
    find_package(GTest CONFIG REQUIRED)
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
    )
endfunction()