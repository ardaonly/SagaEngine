<#
.SYNOPSIS
    Initialize the MSVC build environment and delegate all commands to Forge.

.DESCRIPTION
    This script is a thin shim. It initializes the Visual Studio Developer
    environment (vcvarsall x64) when not already active, then passes all
    arguments directly to the `forge` executable.

    Forge is the canonical build interface. This script exists only to handle
    the MSVC environment bootstrap that forge cannot yet perform itself.

.EXAMPLE
    .\build.ps1 install --profile windows-msvc
    .\build.ps1 configure --preset windows-msvc-14.38
    .\build.ps1 build
    .\build.ps1 clean

.NOTES
    Prerequisites: Visual Studio 2019 or later, Python 3, CMake 3.28+, Conan 2.0+.
    Build Forge first: python3 Tools/Forge/tool/build.py
#>
param([Parameter(ValueFromRemainingArguments = $true)] $PassThrough)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $env:VCINSTALLDIR)
{
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere))
    {
        Write-Error "vswhere.exe not found. Install Visual Studio 2019 or later with the C++ workload."
        exit 1
    }
    $vsRoot = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsRoot)
    {
        Write-Error "No Visual Studio installation with C++ tools found."
        exit 1
    }
    $vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvarsall.bat"
    cmd /c "`"$vcvars`" x64 && set" |
        Where-Object { $_ -match "^([^=]+)=(.*)" } |
        ForEach-Object { [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process") }
    Write-Host "MSVC environment initialized."
}

forge @PassThrough
