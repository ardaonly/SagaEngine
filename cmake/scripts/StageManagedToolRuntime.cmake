# SPDX-License-Identifier: MPL-2.0

foreach(_required IN ITEMS SAGA_RUNTIME_ARCHIVE SAGA_RUNTIME_URL
                           SAGA_RUNTIME_SHA512 SAGA_STAGE_ROOT SAGA_MANIFEST)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "StageManagedToolRuntime: missing ${_required}")
    endif()
endforeach()

get_filename_component(_archive_parent "${SAGA_RUNTIME_ARCHIVE}" DIRECTORY)
file(MAKE_DIRECTORY "${_archive_parent}")
if(EXISTS "${SAGA_RUNTIME_ARCHIVE}")
    file(SHA512 "${SAGA_RUNTIME_ARCHIVE}" _actual_hash)
    if(NOT _actual_hash STREQUAL SAGA_RUNTIME_SHA512)
        message(FATAL_ERROR
            "Preseeded managed runtime archive has the wrong SHA-512: ${SAGA_RUNTIME_ARCHIVE}")
    endif()
else()
    file(DOWNLOAD "${SAGA_RUNTIME_URL}" "${SAGA_RUNTIME_ARCHIVE}"
        EXPECTED_HASH "SHA512=${SAGA_RUNTIME_SHA512}"
        TLS_VERIFY ON
        SHOW_PROGRESS
        STATUS _download_status)
    list(GET _download_status 0 _download_code)
    if(NOT _download_code EQUAL 0)
        list(GET _download_status 1 _download_message)
        message(FATAL_ERROR "Managed runtime download failed: ${_download_message}")
    endif()
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar tf "${SAGA_RUNTIME_ARCHIVE}"
    RESULT_VARIABLE _list_result
    OUTPUT_VARIABLE _entries
    ERROR_VARIABLE _list_error)
if(NOT _list_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect managed runtime archive: ${_list_error}")
endif()
string(REPLACE "\r\n" "\n" _entries "${_entries}")
string(REPLACE "\n" ";" _entry_list "${_entries}")
foreach(_entry IN LISTS _entry_list)
    if(_entry MATCHES "^[/\\\\]" OR
       _entry MATCHES "(^|[/\\\\])\\.\\.([/\\\\]|$)")
        message(FATAL_ERROR "Managed runtime archive contains unsafe path: ${_entry}")
    endif()
endforeach()

set(_runtime_root "${SAGA_STAGE_ROOT}/runtime/dotnet")
file(REMOVE_RECURSE "${_runtime_root}")
file(MAKE_DIRECTORY "${_runtime_root}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xzf "${SAGA_RUNTIME_ARCHIVE}"
    WORKING_DIRECTORY "${_runtime_root}"
    RESULT_VARIABLE _extract_result
    ERROR_VARIABLE _extract_error)
if(NOT _extract_result EQUAL 0)
    message(FATAL_ERROR "Managed runtime extraction failed: ${_extract_error}")
endif()

if(NOT EXISTS "${_runtime_root}/dotnet" OR
   NOT IS_DIRECTORY "${_runtime_root}/shared/Microsoft.NETCore.App")
    message(FATAL_ERROR "Extracted archive is not a Microsoft.NETCore.App runtime")
endif()
if(IS_DIRECTORY "${_runtime_root}/sdk" OR
   IS_DIRECTORY "${_runtime_root}/shared/Microsoft.AspNetCore.App")
    message(FATAL_ERROR "Managed runtime stage contains forbidden SDK or ASP.NET payload")
endif()

file(COPY_FILE "${SAGA_MANIFEST}"
    "${SAGA_STAGE_ROOT}/managed-tool-runtime.json" ONLY_IF_DIFFERENT)
file(MAKE_DIRECTORY "${SAGA_STAGE_ROOT}/licenses/ThirdParty/dotnet")
foreach(_notice IN ITEMS LICENSE.txt ThirdPartyNotices.txt)
    if(NOT EXISTS "${_runtime_root}/${_notice}")
        message(FATAL_ERROR "Runtime archive omitted ${_notice}")
    endif()
    file(COPY_FILE "${_runtime_root}/${_notice}"
        "${SAGA_STAGE_ROOT}/licenses/ThirdParty/dotnet/${_notice}"
        ONLY_IF_DIFFERENT)
endforeach()

foreach(_tool IN ITEMS
        "SagaProjectKit;sagaproject"
        "SagaScript;sagascript"
        "SagaPackager;sagapack")
    list(GET _tool 0 _directory)
    list(GET _tool 1 _assembly)
    foreach(_suffix IN ITEMS dll runtimeconfig.json deps.json)
        if(NOT EXISTS
           "${SAGA_STAGE_ROOT}/tools/${_directory}/${_assembly}.${_suffix}")
            message(FATAL_ERROR
                "Managed tool publish omitted ${_directory}/${_assembly}.${_suffix}")
        endif()
    endforeach()
    if(EXISTS "${SAGA_STAGE_ROOT}/tools/${_directory}/${_assembly}")
        message(FATAL_ERROR "Managed tool publish produced forbidden apphost: ${_assembly}")
    endif()
endforeach()

file(GLOB_RECURSE _managed_debug_symbols
    "${SAGA_STAGE_ROOT}/tools/*.pdb")
if(_managed_debug_symbols)
    file(REMOVE ${_managed_debug_symbols})
endif()

file(GLOB_RECURSE _managed_payload_files LIST_DIRECTORIES false
    "${SAGA_STAGE_ROOT}/runtime/dotnet/*"
    "${SAGA_STAGE_ROOT}/tools/*")
foreach(_payload IN LISTS _managed_payload_files)
    file(CHMOD "${_payload}" PERMISSIONS
        OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endforeach()
file(CHMOD "${_runtime_root}/dotnet" PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

file(WRITE "${SAGA_STAGE_ROOT}/.stage-complete" "managed-runtime=10.0.10\n")
