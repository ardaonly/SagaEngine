function(saga_setup_assets)
    set(ASSET_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Assets")
    set(ASSET_BUILD_DIR "${CMAKE_BINARY_DIR}/assets")
    set(ASSET_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/Bin/$<CONFIG>/assets")
    
    file(MAKE_DIRECTORY ${ASSET_BUILD_DIR})
    file(MAKE_DIRECTORY ${ASSET_OUTPUT_DIR})
    
    set(SHADER_SOURCE_DIR "${ASSET_SOURCE_DIR}/Shaders")
    set(SHADER_BUILD_DIR "${ASSET_BUILD_DIR}/shaders")
    
    if(SAGA_ENABLE_VULKAN)
        file(GLOB SHADER_FILES "${SHADER_SOURCE_DIR}/*.hlsl")
        foreach(shader ${SHADER_FILES})
            get_filename_component(shader_name ${shader} NAME_WE)
            set(shader_output "${SHADER_BUILD_DIR}/${shader_name}.spv")
            
            add_custom_command(
                OUTPUT ${shader_output}
                COMMAND glslc ${shader} -o ${shader_output} --target-env=vulkan1.3
                DEPENDS ${shader}
                COMMENT "Compiling Vulkan shader: ${shader_name}"
            )
            list(APPEND SHADER_OUTPUTS ${shader_output})
        endforeach()
    endif()
    
    if(SHADER_OUTPUTS)
        add_custom_target(SagaShaders ALL DEPENDS ${SHADER_OUTPUTS})
        
        add_custom_command(
            TARGET SagaShaders POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_BUILD_DIR} ${ASSET_OUTPUT_DIR}/shaders
            COMMENT "Copying shaders to output directory"
        )
    endif()
    
    file(GLOB CONFIG_FILES "${ASSET_SOURCE_DIR}/Config/*.json")
    foreach(config ${CONFIG_FILES})
        get_filename_component(config_name ${config} NAME)
        add_custom_command(
            OUTPUT ${ASSET_OUTPUT_DIR}/config/${config_name}
            COMMAND ${CMAKE_COMMAND} -E copy ${config} ${ASSET_OUTPUT_DIR}/config/${config_name}
            DEPENDS ${config}
        )
    endforeach()
endfunction()