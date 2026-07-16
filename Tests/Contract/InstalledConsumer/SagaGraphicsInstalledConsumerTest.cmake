if(NOT DEFINED SAGA_BUILD_DIR)
    message(FATAL_ERROR "SAGA_BUILD_DIR is required")
endif()
if(NOT DEFINED SAGA_INSTALL_PREFIX)
    message(FATAL_ERROR "SAGA_INSTALL_PREFIX is required")
endif()
if(NOT DEFINED SAGA_DOWNSTREAM_SOURCE_DIR)
    message(FATAL_ERROR "SAGA_DOWNSTREAM_SOURCE_DIR is required")
endif()
if(NOT DEFINED SAGA_DOWNSTREAM_BINARY_DIR)
    message(FATAL_ERROR "SAGA_DOWNSTREAM_BINARY_DIR is required")
endif()
if(NOT DEFINED SAGA_SOURCE_ROOT)
    message(FATAL_ERROR "SAGA_SOURCE_ROOT is required")
endif()

file(REMOVE_RECURSE "${SAGA_INSTALL_PREFIX}" "${SAGA_DOWNSTREAM_BINARY_DIR}")
file(MAKE_DIRECTORY "${SAGA_INSTALL_PREFIX}" "${SAGA_DOWNSTREAM_BINARY_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${SAGA_BUILD_DIR}"
            --prefix "${SAGA_INSTALL_PREFIX}"
            --component SagaDevelopment
    RESULT_VARIABLE _install_result
)
if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR "SagaEngine install failed with ${_install_result}")
endif()

# Verify that the installed development SDK carries its canonical licensing
# material together with the exported graphics package.
set(_required_installed_files
    "Licenses/Saga/LICENSE"
    "Licenses/Saga/THIRD_PARTY_NOTICES.md"
    "Licenses/Saga/LICENSES/0BSD.txt"
    "Licenses/Saga/LICENSES/Apache-2.0.txt"
    "Licenses/Saga/LICENSES/CC-BY-4.0.txt"
    "Licenses/Saga/LICENSES/CC0-1.0.txt"
    "Licenses/Saga/LICENSES/MPL-2.0.txt"

    "Licenses/ThirdParty/Vendor/Diligent/DXC-License.txt"
    "Licenses/ThirdParty/Vendor/Diligent/DXC-ThirdPartyNotices.txt"
    "Licenses/ThirdParty/Vendor/Diligent/GLEW-License.txt"
    "Licenses/ThirdParty/Vendor/Diligent/GLSLang-License.txt"
    "Licenses/ThirdParty/Vendor/Diligent/SPIRV-Cross-License.txt"
    "Licenses/ThirdParty/Vendor/Diligent/SPIRV-Headers-License.txt"
    "Licenses/ThirdParty/Vendor/Diligent/SPIRV-Tools-License.txt"
    "Licenses/ThirdParty/Vendor/Diligent/Volk-License.md"
    "Licenses/ThirdParty/Vendor/Diligent/stb_image_write_license.txt"
    "Licenses/ThirdParty/Vendor/Diligent/xxHash-License.txt"

    "include/SagaEngine/Graphics/Graphics.h"
    "lib/cmake/SagaEngine/SagaEngineConfig.cmake"
    "lib/cmake/SagaEngine/SagaEngineConfigVersion.cmake"
    "lib/cmake/SagaEngine/SagaEngineTargets.cmake"
)

set(_missing_installed_files)
set(_empty_installed_files)

foreach(_relative_path IN LISTS _required_installed_files)
    set(_installed_path "${SAGA_INSTALL_PREFIX}/${_relative_path}")

    if(NOT EXISTS "${_installed_path}")
        list(APPEND _missing_installed_files "${_relative_path}")
        continue()
    endif()

    file(SIZE "${_installed_path}" _installed_size)
    if(_installed_size EQUAL 0)
        list(APPEND _empty_installed_files "${_relative_path}")
    endif()
endforeach()

if(_missing_installed_files)
    list(JOIN _missing_installed_files "\n  " _missing_text)
    message(FATAL_ERROR
        "Installed SagaDevelopment payload is missing required files:\n"
        "  ${_missing_text}")
endif()

if(_empty_installed_files)
    list(JOIN _empty_installed_files "\n  " _empty_text)
    message(FATAL_ERROR
        "Installed SagaDevelopment payload contains empty files:\n"
        "  ${_empty_text}")
endif()

file(GLOB _configuration_target_files
    "${SAGA_INSTALL_PREFIX}/lib/cmake/SagaEngine/SagaEngineTargets-*.cmake")
if(NOT _configuration_target_files)
    message(FATAL_ERROR
        "Installed configuration-specific SagaEngineTargets file was not found")
endif()

set(_graphics_library_candidates
    "${SAGA_INSTALL_PREFIX}/lib/libSagaGraphicsCore.a"
    "${SAGA_INSTALL_PREFIX}/lib/libSagaGraphicsCore.so"
    "${SAGA_INSTALL_PREFIX}/lib/libSagaGraphicsCore.dylib"
    "${SAGA_INSTALL_PREFIX}/lib/SagaGraphicsCore.lib"
    "${SAGA_INSTALL_PREFIX}/bin/SagaGraphicsCore.dll"
    "${SAGA_INSTALL_PREFIX}/bin/libSagaGraphicsCore.dll"
)

set(_graphics_library_found FALSE)
foreach(_candidate IN LISTS _graphics_library_candidates)
    if(EXISTS "${_candidate}")
        set(_graphics_library_found TRUE)
        break()
    endif()
endforeach()

if(NOT _graphics_library_found)
    message(FATAL_ERROR
        "Installed SagaGraphicsCore library artifact was not found")
endif()

message(STATUS "Installed SDK licensing payload verification passed")

set(_config
    "${SAGA_INSTALL_PREFIX}/lib/cmake/SagaEngine/SagaEngineConfig.cmake")
if(NOT EXISTS "${_config}")
    message(FATAL_ERROR "Installed SagaEngineConfig.cmake was not found")
endif()

set(_downstream_configure_args
    -S "${SAGA_DOWNSTREAM_SOURCE_DIR}"
    -B "${SAGA_DOWNSTREAM_BINARY_DIR}"
    "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
    "-DCMAKE_PREFIX_PATH=${SAGA_INSTALL_PREFIX}"
    "-DSAGA_PUBLIC_SOURCE_INCLUDE_FORBIDDEN=${SAGA_SOURCE_ROOT}/Engine/Public"
    "-DSAGA_PRIVATE_SOURCE_INCLUDE_FORBIDDEN=${SAGA_SOURCE_ROOT}/Engine/Private"
)
set(_conan_toolchain "${SAGA_BUILD_DIR}/generators/conan_toolchain.cmake")
if(EXISTS "${_conan_toolchain}")
    list(APPEND _downstream_configure_args
        "-DCMAKE_TOOLCHAIN_FILE=${_conan_toolchain}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${_downstream_configure_args}
    RESULT_VARIABLE _configure_result
)
if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR
        "Installed SagaGraphics downstream configure failed with "
        "${_configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${SAGA_DOWNSTREAM_BINARY_DIR}"
            --target SagaGraphicsInstalledConsumer
    RESULT_VARIABLE _build_result
)
if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR
        "Installed SagaGraphics downstream build failed with ${_build_result}")
endif()

execute_process(
    COMMAND "${SAGA_DOWNSTREAM_BINARY_DIR}/SagaGraphicsInstalledConsumer"
    RESULT_VARIABLE _run_result
)
if(NOT _run_result EQUAL 0)
    message(FATAL_ERROR
        "Installed SagaGraphics downstream executable failed with "
        "${_run_result}")
endif()
