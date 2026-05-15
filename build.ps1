<#
.SYNOPSIS
    Legacy Windows compatibility wrapper for Forge.

.DESCRIPTION
    Forge is the canonical build interface. Prefer running `forge` directly from
    a Visual Studio Developer PowerShell or Developer Command Prompt.

    This script is kept only for older automation. It initializes the Visual
    Studio Developer environment when needed, then forwards all arguments to
    `forge`.

.EXAMPLE
    forge install --profile windows-msvc
    forge configure --preset windows-msvc-14.38
    forge build

.NOTES
    Prerequisites: Visual Studio 2019 or later, Python 3, CMake 3.28+, Conan 2.0+.
    Build Forge first: python3 Tools/Forge/build.py
#>
param([Parameter(ValueFromRemainingArguments = $true)] $PassThrough)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Warning "build.ps1 is a legacy compatibility wrapper. Prefer running 'forge' directly from a Visual Studio Developer shell."

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

if (-not (Get-Command forge -ErrorAction SilentlyContinue))
{
    Write-Error "forge was not found on PATH. Build it with: python3 Tools/Forge/build.py"
    exit 1
}

forge @PassThrough
