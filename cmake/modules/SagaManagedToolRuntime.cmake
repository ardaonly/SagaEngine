# SPDX-License-Identifier: MPL-2.0

function(saga_setup_managed_tool_runtime)
    file(GLOB_RECURSE _managed_tool_sources CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tools/SagaProjectKit/*.cs"
        "${SAGA_ROOT}/Tools/SagaScript/*.cs"
        "${SAGA_ROOT}/Tools/SagaPackager/*.cs")
    string(JSON _runtime_url GET
        "${SAGA_MANAGED_TOOL_RUNTIME_MANIFEST_JSON}" asset url)
    string(JSON _runtime_sha512 GET
        "${SAGA_MANAGED_TOOL_RUNTIME_MANIFEST_JSON}" asset sha512)

    set(SAGA_MANAGED_TOOL_RUNTIME_STAGE
        "${CMAKE_BINARY_DIR}/ManagedToolRuntime"
        CACHE PATH "Saga-owned managed tool runtime stage")
    set(SAGA_MANAGED_TOOL_RUNTIME_CACHE
        "${CMAKE_BINARY_DIR}/downloads"
        CACHE PATH "Preseedable cache for the pinned Saga managed runtime archive")
    set(_archive
        "${SAGA_MANAGED_TOOL_RUNTIME_CACHE}/dotnet-runtime-${SAGA_MANAGED_RUNTIME_VERSION}-${SAGA_MANAGED_RUNTIME_RID}.tar.gz")
    set(_stamp "${SAGA_MANAGED_TOOL_RUNTIME_STAGE}/.stage-complete")

    add_custom_command(
        OUTPUT "${_stamp}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${SAGA_MANAGED_TOOL_RUNTIME_STAGE}/tools/SagaProjectKit"
            "${SAGA_MANAGED_TOOL_RUNTIME_STAGE}/tools/SagaScript"
            "${SAGA_MANAGED_TOOL_RUNTIME_STAGE}/tools/SagaPackager"
        COMMAND "${SAGA_DOTNET_EXECUTABLE}" publish
            "${SAGA_ROOT}/Tools/SagaProjectKit/SagaProjectKit.csproj"
            --configuration Release --verbosity minimal
            --no-self-contained -p:UseAppHost=false
            --output "${SAGA_MANAGED_TOOL_RUNTIME_STAGE}/tools/SagaProjectKit"
        COMMAND "${SAGA_DOTNET_EXECUTABLE}" publish
            "${SAGA_ROOT}/Tools/SagaScript/SagaScript.csproj"
            --configuration Release --verbosity minimal
            --no-self-contained -p:UseAppHost=false
            --output "${SAGA_MANAGED_TOOL_RUNTIME_STAGE}/tools/SagaScript"
        COMMAND "${SAGA_DOTNET_EXECUTABLE}" publish
            "${SAGA_ROOT}/Tools/SagaPackager/SagaPackager.csproj"
            --configuration Release --verbosity minimal
            --no-self-contained -p:UseAppHost=false
            --output "${SAGA_MANAGED_TOOL_RUNTIME_STAGE}/tools/SagaPackager"
        COMMAND "${CMAKE_COMMAND}"
            "-DSAGA_RUNTIME_ARCHIVE=${_archive}"
            "-DSAGA_RUNTIME_URL=${_runtime_url}"
            "-DSAGA_RUNTIME_SHA512=${_runtime_sha512}"
            "-DSAGA_STAGE_ROOT=${SAGA_MANAGED_TOOL_RUNTIME_STAGE}"
            "-DSAGA_MANIFEST=${SAGA_ROOT}/Config/managed-tool-runtime.json"
            -P "${SAGA_ROOT}/cmake/scripts/StageManagedToolRuntime.cmake"
        DEPENDS
            "${SAGA_ROOT}/Config/managed-tool-runtime.json"
            "${SAGA_ROOT}/cmake/scripts/StageManagedToolRuntime.cmake"
            "${SAGA_ROOT}/Tools/SagaProjectKit/SagaProjectKit.csproj"
            "${SAGA_ROOT}/Tools/SagaScript/SagaScript.csproj"
            "${SAGA_ROOT}/Tools/SagaPackager/SagaPackager.csproj"
            ${_managed_tool_sources}
        COMMENT "Publishing Saga managed tools and staging .NET ${SAGA_MANAGED_RUNTIME_VERSION}"
        VERBATIM)

    add_custom_target(SagaManagedToolRuntimeStage DEPENDS "${_stamp}")
    set_target_properties(SagaManagedToolRuntimeStage PROPERTIES FOLDER "Distribution")

    if(TARGET Saga)
        add_dependencies(Saga SagaManagedToolRuntimeStage)
    endif()
endfunction()
