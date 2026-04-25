# ─── SagaFindATL.cmake ────────────────────────────────────────────────────────
#
# Thorough search for Microsoft ATL (atls.lib) required by Diligent's static
# GPU backends.  ATL ships as an optional VS component and its lib directory
# is never on the default linker search path.
#
# Search strategy (in order):
#   1. VCToolsInstallDir environment variable (set by vcvarsall / Dev Prompt)
#   2. Derive from CMAKE_CXX_COMPILER path (cl.exe → walk up to MSVC root)
#   3. Scan all MSVC toolset versions under the detected VS installation
#   4. vswhere.exe — query all VS instances for the ATL component
#   5. Brute-force scan of Program Files for any VS installation
#
# Result: if found, calls link_directories() and sets SAGA_ATL_FOUND in
#         parent scope.  If not found, emits a FATAL_ERROR with install
#         instructions.

function(saga_find_atl)

    # Architecture suffix for the lib subdirectory.
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_arch "x64")
    else()
        set(_arch "x86")
    endif()

    set(_atl_lib_dir "")

    # ── Strategy 1: VCToolsInstallDir env var ─────────────────────────────
    if(DEFINED ENV{VCToolsInstallDir} AND NOT _atl_lib_dir)
        set(_candidate "$ENV{VCToolsInstallDir}/atlmfc/lib/${_arch}")
        cmake_path(NORMAL_PATH _candidate)
        if(EXISTS "${_candidate}/atls.lib")
            set(_atl_lib_dir "${_candidate}")
            message(STATUS "[ATL] Found via VCToolsInstallDir: ${_atl_lib_dir}")
        endif()
    endif()

    # ── Strategy 2: Derive from cl.exe compiler path ──────────────────────
    #   cl.exe lives at: .../MSVC/<ver>/bin/Host<arch>/<arch>/cl.exe
    #   ATL lives at:    .../MSVC/<ver>/atlmfc/lib/<arch>/atls.lib
    if(NOT _atl_lib_dir)
        cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH _dir)   # .../x64
        cmake_path(GET _dir PARENT_PATH _dir)                  # .../Hostx64
        cmake_path(GET _dir PARENT_PATH _dir)                  # .../bin
        cmake_path(GET _dir PARENT_PATH _msvc_ver_dir)         # .../MSVC/<ver>
        set(_candidate "${_msvc_ver_dir}/atlmfc/lib/${_arch}")
        cmake_path(NORMAL_PATH _candidate)
        if(EXISTS "${_candidate}/atls.lib")
            set(_atl_lib_dir "${_candidate}")
            message(STATUS "[ATL] Found via compiler path: ${_atl_lib_dir}")
        endif()
    endif()

    # ── Strategy 3: Scan sibling MSVC toolset versions ────────────────────
    #   If the active toolset doesn't have ATL, another installed version might.
    #   .../MSVC/<ver> → go up one level → .../MSVC/ → glob all versions.
    if(NOT _atl_lib_dir AND DEFINED _msvc_ver_dir)
        cmake_path(GET _msvc_ver_dir PARENT_PATH _msvc_root)  # .../MSVC/
        file(GLOB _versions LIST_DIRECTORIES true "${_msvc_root}/*")
        # Sort descending so we prefer the newest toolset.
        list(SORT _versions ORDER DESCENDING)
        foreach(_ver_dir IN LISTS _versions)
            set(_candidate "${_ver_dir}/atlmfc/lib/${_arch}")
            if(EXISTS "${_candidate}/atls.lib")
                set(_atl_lib_dir "${_candidate}")
                message(STATUS "[ATL] Found in sibling toolset: ${_atl_lib_dir}")
                break()
            endif()
        endforeach()
    endif()

    # ── Strategy 4: vswhere.exe — query all VS instances ──────────────────
    if(NOT _atl_lib_dir)
        set(_vswhere_paths
            "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe"
            "$ENV{ProgramFiles}/Microsoft Visual Studio/Installer/vswhere.exe"
            "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
            "C:/Program Files/Microsoft Visual Studio/Installer/vswhere.exe"
        )
        set(_vswhere "")
        foreach(_vw IN LISTS _vswhere_paths)
            if(EXISTS "${_vw}")
                set(_vswhere "${_vw}")
                break()
            endif()
        endforeach()

        if(_vswhere)
            # Get all VS installation paths.
            execute_process(
                COMMAND "${_vswhere}" -all -products * -property installationPath
                OUTPUT_VARIABLE _vs_paths
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            # vswhere outputs one path per line.
            string(REPLACE "\n" ";" _vs_path_list "${_vs_paths}")
            foreach(_vs_path IN LISTS _vs_path_list)
                string(STRIP "${_vs_path}" _vs_path)
                if(NOT _vs_path)
                    continue()
                endif()
                # Scan all MSVC versions under this VS instance.
                file(GLOB _tool_versions LIST_DIRECTORIES true
                    "${_vs_path}/VC/Tools/MSVC/*"
                )
                list(SORT _tool_versions ORDER DESCENDING)
                foreach(_tv IN LISTS _tool_versions)
                    set(_candidate "${_tv}/atlmfc/lib/${_arch}")
                    if(EXISTS "${_candidate}/atls.lib")
                        set(_atl_lib_dir "${_candidate}")
                        message(STATUS "[ATL] Found via vswhere (${_vs_path}): ${_atl_lib_dir}")
                        break()
                    endif()
                endforeach()
                if(_atl_lib_dir)
                    break()
                endif()
            endforeach()
        endif()
    endif()

    # ── Strategy 5: Brute-force scan of Program Files ─────────────────────
    if(NOT _atl_lib_dir)
        set(_pf_roots
            "C:/Program Files/Microsoft Visual Studio"
            "C:/Program Files (x86)/Microsoft Visual Studio"
            "D:/Program Files/Microsoft Visual Studio"
            "D:/Program Files (x86)/Microsoft Visual Studio"
        )
        set(_vs_years "2022" "2019" "2017")
        set(_vs_editions "Community" "Professional" "Enterprise" "BuildTools" "Preview")

        foreach(_pf IN LISTS _pf_roots)
            foreach(_year IN LISTS _vs_years)
                foreach(_ed IN LISTS _vs_editions)
                    set(_vs_base "${_pf}/${_year}/${_ed}/VC/Tools/MSVC")
                    if(IS_DIRECTORY "${_vs_base}")
                        file(GLOB _tool_versions LIST_DIRECTORIES true "${_vs_base}/*")
                        list(SORT _tool_versions ORDER DESCENDING)
                        foreach(_tv IN LISTS _tool_versions)
                            set(_candidate "${_tv}/atlmfc/lib/${_arch}")
                            if(EXISTS "${_candidate}/atls.lib")
                                set(_atl_lib_dir "${_candidate}")
                                message(STATUS "[ATL] Found via scan (${_year}/${_ed}): ${_atl_lib_dir}")
                                break()
                            endif()
                        endforeach()
                        if(_atl_lib_dir)
                            break()
                        endif()
                    endif()
                endforeach()
                if(_atl_lib_dir)
                    break()
                endif()
            endforeach()
            if(_atl_lib_dir)
                break()
            endif()
        endforeach()
    endif()

    # ── Result ────────────────────────────────────────────────────────────
    if(_atl_lib_dir)
        link_directories("${_atl_lib_dir}")

        # Also add ATL include path for headers (atlbase.h etc.)
        # It's at the same level: .../atlmfc/include
        cmake_path(GET _atl_lib_dir PARENT_PATH _atl_parent)    # .../atlmfc/lib
        cmake_path(GET _atl_parent PARENT_PATH _atlmfc_root)    # .../atlmfc
        set(_atl_inc "${_atlmfc_root}/include")
        if(IS_DIRECTORY "${_atl_inc}")
            include_directories(SYSTEM "${_atl_inc}")
            message(STATUS "[ATL] Include path: ${_atl_inc}")
        endif()

        set(SAGA_ATL_FOUND TRUE PARENT_SCOPE)
        message(STATUS "[ATL] atls.lib registered for Diligent GPU backends.")
    else()
        message(FATAL_ERROR
            "=================================================================\n"
            " ATL (atls.lib) NOT FOUND — required by Diligent GPU backends.\n"
            "\n"
            " Install via Visual Studio Installer:\n"
            "   1. Open Visual Studio Installer\n"
            "   2. Click 'Modify' on your VS installation\n"
            "   3. Go to 'Individual Components' tab\n"
            "   4. Search for 'ATL'\n"
            "   5. Check 'C++ ATL for latest build tools (x86 & x64)'\n"
            "   6. Click 'Modify' to install (~100 MB)\n"
            "=================================================================\n"
        )
    endif()

endfunction()
