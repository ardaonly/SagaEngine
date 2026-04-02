$BUILD_BRAND = "SagaForge"
$ErrorActionPreference = "Continue"

$SCRIPT_DIR = Split-Path -Parent $PSCommandPath
if (-not $SCRIPT_DIR) {
    $SCRIPT_DIR = $PSScriptRoot
}
Set-Location $SCRIPT_DIR

$VERSION_FILE    = Join-Path $SCRIPT_DIR "version.json"
$TOOLCHAIN_DIR   = Join-Path $SCRIPT_DIR ".toolchain"
$PYTHON_DIR      = Join-Path $TOOLCHAIN_DIR "python"
$PYTHON_PATH_FILE = Join-Path $TOOLCHAIN_DIR "python_path.txt"
$LOG_FILE        = Join-Path $TOOLCHAIN_DIR "bootstrap.log"
$PROFILE_DIR     = Join-Path $SCRIPT_DIR "profiles"
$BOOTSTRAP_SCRIPT = Join-Path $SCRIPT_DIR "bootstrap.ps1"

function Resolve-ProfilePath {
    param([Parameter(Mandatory=$true)][string]$Name)

    $path = Join-Path $PROFILE_DIR $Name
    if (-not (Test-Path $path)) {
        throw "Profile file not found: $path"
    }
    return $path
}

function Get-ATLStatus {
    $vsPath = Find-VisualStudioBuildTools
    if (-not $vsPath) {
        return [pscustomobject]@{
            Found = $false
            Ok = $false
            Message = "Visual Studio Build Tools 2022 not found."
        }
    }

    $candidateRoots = @(
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"),
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"),
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"),
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC")
    )

    foreach ($root in $candidateRoots) {
        if (-not (Test-Path $root)) { continue }

        $header = Get-ChildItem $root -Recurse -Filter "atlcomcli.h" -ErrorAction SilentlyContinue |
                  Select-Object -First 1

        if ($header) {
            return [pscustomobject]@{
                Found = $true
                Ok = $true
                HeaderPath = $header.FullName
                Message = "OK"
            }
        }
    }

    return [pscustomobject]@{
        Found = $true
        Ok = $false
        Message = "ATL header atlcomcli.h not found. Install ATL support in Visual Studio Installer."
    }
}

function Get-ActiveMSVCVersion {
    $clCmd = Get-Command cl -ErrorAction SilentlyContinue
    if (-not $clCmd) { return $null }

    try {
        $output = & $clCmd.Source 2>&1 | ForEach-Object { "$_" }
        foreach ($line in $output) {
            if ($line -match '\b(19\.\d+\.\d+)\b') {
                return [version]$matches[1]
            }
        }
    } catch {}

    return $null
}

function Get-PipPackageVersion {
    param(
        [Parameter(Mandatory=$true)]
        [string]$PythonExe,

        [Parameter(Mandatory=$true)]
        [string]$PackageName
    )

    $show = & $PythonExe -m pip show $PackageName 2>&1
    if ($LASTEXITCODE -ne 0) { return $null }

    foreach ($line in $show) {
        if ($line -match '^Version:\s*(.+)$') {
            return $matches[1].Trim()
        }
    }

    return $null
}

function Get-CommandVersion {
    param(
        [Parameter(Mandatory=$true)]
        [string]$CommandName
    )

    $cmd = Get-Command $CommandName -ErrorAction SilentlyContinue
    if (-not $cmd) { return $null }

    try {
        $firstLine = & $CommandName --version 2>&1 | Select-Object -First 1
        if ($firstLine -match '(\d+\.\d+\.\d+)') {
            return [version]$matches[1]
        }
    }
    catch {
        return $null
    }

    return $null
}

function Get-MSVCStatus {
    $vsPath = Find-VisualStudioBuildTools
    if (-not $vsPath) {
        return [pscustomobject]@{
            Found = $false
            Ok = $false
            Message = "Visual Studio Build Tools 2022 not found."
        }
    }

    $activeVersion = Get-ActiveMSVCVersion

    if (-not $activeVersion) {
        return [pscustomobject]@{
            Found = $true
            Ok = $false
            Message = "MSVC compiler not found in environment (cl.exe missing)"
        }
    }

    $requiredMajor = 19
    $requiredMinor = 38

    $ok = ($activeVersion.Major -eq $requiredMajor -and $activeVersion.Minor -eq $requiredMinor)

    return [pscustomobject]@{
        Found = $true
        Ok = $ok
        VsPath = $vsPath
        ToolsetVersion = $activeVersion
        Message = if ($ok) {
            "OK"
        } else {
            "Wrong MSVC version: $activeVersion (required: 19.38.x / toolset 14.38)"
        }
    }
}

function Get-CMakeVersion {
    $cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmakeCmd) { return $null }

    $firstLine = & cmake --version 2>&1 | Select-Object -First 1
    if ($firstLine -match '(\d+\.\d+\.\d+)') {
        return [version]$matches[1]
    }

    return $null
}

function Get-ToolchainConfig {
    return Get-Content $VERSION_FILE -Raw | ConvertFrom-Json
}
$TOOLCHAIN = Get-ToolchainConfig
$PYTHON_VERSION = $TOOLCHAIN.python.version
$CONAN_VERSION   = $TOOLCHAIN.conan.version
$NINJA_VERSION   = $TOOLCHAIN.ninja.version
$CMAKE_VERSION   = $TOOLCHAIN.cmake.version
$LOCKFILE_PATH   = Join-Path $SCRIPT_DIR $TOOLCHAIN.lockfile.path

$DEFAULT_PROFILE = "generated-windows-msvc"
$DEFAULT_PRESET  = "windows-msvc-14.38"

function Find-Toolchain-Python {
    Write-Host "Detecting Toolchain Python..." -ForegroundColor Gray

    $directPython = Join-Path $TOOLCHAIN_DIR "python\python.exe"
    if (Test-Path $directPython) {
        try {
            $version = & $directPython --version 2>&1
            if ($LASTEXITCODE -eq 0) {
                Write-Host "  Found: $directPython ($version)" -ForegroundColor Green
                return @{ Cmd = $directPython; Version = $version; IsToolchain = $true }
            }
        } catch {}
    }

    if (Test-Path $PYTHON_PATH_FILE) {
        $savedPath = (Get-Content $PYTHON_PATH_FILE -Raw).Trim()
        if ($savedPath -and (Test-Path $savedPath)) {
            try {
                $version = & $savedPath --version 2>&1
                if ($LASTEXITCODE -eq 0) {
                    Write-Host "  Found: $savedPath ($version)" -ForegroundColor Green
                    return @{ Cmd = $savedPath; Version = $version; IsToolchain = $true }
                }
            } catch {}
        }
    }

    Write-Host "  Python path file not found: $PYTHON_PATH_FILE" -ForegroundColor Yellow
    return $null
}

$PYTHON_INFO = Find-Toolchain-Python

if (-not $PYTHON_INFO) {
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Yellow
    Write-Host "  TOOLCHAIN PYTHON NOT FOUND" -ForegroundColor Yellow
    Write-Host "=========================================" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Bootstrapping toolchain..." -ForegroundColor Cyan
    Write-Host ""

    if (Test-Path $BOOTSTRAP_SCRIPT) {
        & $BOOTSTRAP_SCRIPT
        if ($LASTEXITCODE -ne 0) {
            Write-Host ""
            Write-Host "=========================================" -ForegroundColor Red
            Write-Host "  BOOTSTRAP FAILED" -ForegroundColor Red
            Write-Host "=========================================" -ForegroundColor Red
            Write-Host ""
            Write-Host "Check log: $LOG_FILE" -ForegroundColor Yellow
            exit 1
        }
        $PYTHON_INFO = Find-Toolchain-Python
    } else {
        Write-Host "bootstrap.ps1 not found: $BOOTSTRAP_SCRIPT" -ForegroundColor Red
        exit 1
    }
}

if (-not $PYTHON_INFO) {
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host "  PYTHON NOT FOUND AFTER BOOTSTRAP" -ForegroundColor Red
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Check log: $LOG_FILE" -ForegroundColor Yellow
    exit 1
}

$PYTHON_CMD = $PYTHON_INFO.Cmd
$CONAN_CMD = Join-Path (Split-Path $PYTHON_CMD -Parent) "Scripts\conan.exe"

Write-Host "  Using: $PYTHON_CMD" -ForegroundColor Gray
Write-Host ""

function Find-MsvcToolset {
    param([string]$RequiredVersion)

    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
    )

    $msvcRoot = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1

    if (-not $msvcRoot) {
        throw "MSVC root not found under any known VS 2022 edition path."
    }

    $match = Get-ChildItem -Path $msvcRoot -Directory |
             Where-Object { $_.Name.StartsWith($RequiredVersion) } |
             Sort-Object Name -Descending |
             Select-Object -First 1

    if (-not $match) {
        if ($env:SAGA_CI_MODE -eq "true") {
            $match = Get-ChildItem -Path $msvcRoot -Directory |
                     Sort-Object Name -Descending |
                     Select-Object -First 1
            Write-Host "  [Toolchain] 14.38 not found, using available toolset: $($match.Name)" -ForegroundColor Yellow
        } else {
            throw "MSVC toolset $RequiredVersion not found under $msvcRoot. Install it via Visual Studio Installer -> Individual Components -> MSVC v143 14.38."
        }
    }

    return $match.FullName
}

function Get-ClExePath {
    param([string]$ToolsetRoot)

    $clPath = Join-Path $ToolsetRoot "bin\Hostx64\x64\cl.exe"

    if (-not (Test-Path $clPath)) {
        throw "cl.exe not found at: $clPath"
    }

    return $clPath
}



function Find-Pip {
    param([string]$PythonCmd)
    try {
        $result = & $PythonCmd -m pip --version 2>&1
        if ($LASTEXITCODE -eq 0) {
            return $PythonCmd
        }
    } catch {
        return $null
    }
    return $null
}

$PIP_CMD = Find-Pip -PythonCmd $PYTHON_CMD
if (-not $PIP_CMD) {
    Write-Host "  Bootstrapping pip..." -ForegroundColor Yellow
    & $PYTHON_CMD -m ensurepip --upgrade 2>&1 | Out-Null
    $PIP_CMD = Find-Pip -PythonCmd $PYTHON_CMD
}

function Find-VisualStudioBuildTools {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }

    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if (-not $path) { return $null }

    return $path.Trim()
}

function Get-BuildMode {
    if ($env:SAGA_CI_MODE -eq "true") { return "ci" }
    return "dev"
}

function Get-BuildPolicy {
    $mode = Get-BuildMode
    return $TOOLCHAIN.build_modes.$mode.build_policy
}

function Ensure-Lockfile {
    param([string]$ProfileName)

    $mode = Get-BuildMode
    $lockExists = Test-Path $LOCKFILE_PATH

    if (-not $lockExists) {
        if (-not $TOOLCHAIN.lockfile.required) { return }

        if ($mode -eq "ci" -and -not $TOOLCHAIN.build_modes.ci.allow_missing) {
            throw "Lockfile missing in CI: $LOCKFILE_PATH"
        }
    }

    if (-not $lockExists -and $mode -eq "dev") {
        & $CONAN_CMD lock create . `
            --profile:host=(Join-Path $PROFILE_DIR "$ProfileName-host") `
            --profile:build=(Join-Path $PROFILE_DIR "$ProfileName-build") `
            --lockfile-out=$LOCKFILE_PATH `
            --build=$TOOLCHAIN.build_modes.dev.build_policy

        if ($LASTEXITCODE -ne 0) {
            throw "Failed to create lockfile"
        }
    }
}

function Ensure-PythonPackage {
    param(
        [string]$PythonExe,
        [string]$Package,
        [string]$Version
    )

    $show = & $PythonExe -m pip show $Package 2>&1
    if ($LASTEXITCODE -eq 0 -and ($show | Select-String -Pattern "^Version:\s*$([regex]::Escape($Version))$")) {
        return
    }

    & $PythonExe -m pip install --quiet "$Package==$Version"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install $Package==$Version"
    }
}

function Ensure-CMake {
    $required = [version]$CMAKE_VERSION
    $cmakeVersion = Get-CMakeVersion

    if ($cmakeVersion -and $cmakeVersion -eq $required) {
        $toolScripts = Join-Path (Split-Path $PYTHON_CMD -Parent) "Scripts"
        if (Test-Path $toolScripts) {
            $env:PATH = "$toolScripts;$env:PATH"
        }
        return
    }

    Write-Host "  CMake: installing pinned toolchain version $CMAKE_VERSION" -ForegroundColor Yellow
    & $PYTHON_CMD -m pip install --quiet "cmake==$CMAKE_VERSION"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to ensure CMake $CMAKE_VERSION"
    }

    $toolScripts = Join-Path (Split-Path $PYTHON_CMD -Parent) "Scripts"
    if (Test-Path $toolScripts) {
        $env:PATH = "$toolScripts;$env:PATH"
    }

    $cmakeVersion = Get-CMakeVersion
    if (-not $cmakeVersion -or $cmakeVersion -ne $required) {
        throw "CMake mismatch after setup. Required: $required, Found: $cmakeVersion"
    }
}

function Reset-Sccache {
    param([string]$ClExePath)

    $sccacheDir = Join-Path $SCRIPT_DIR "cache\sccache"
    $sccacheExe = Join-Path $sccacheDir "sccache.exe"

    if (-not (Test-Path $sccacheExe)) {
        $inPath = Get-Command sccache -ErrorAction SilentlyContinue
        if ($inPath) {
            $sccacheExe = $inPath.Source
            Write-Host "  [Sccache] Found in PATH: $sccacheExe" -ForegroundColor Gray
        }
        else {
            Write-Host "  [Sccache] Not found, downloading..." -ForegroundColor Yellow

            $releaseApi = "https://api.github.com/repos/mozilla/sccache/releases/latest"
            $release    = Invoke-RestMethod -Uri $releaseApi -UseBasicParsing
            $asset      = $release.assets | Where-Object { $_.name -match "x86_64-pc-windows-msvc\.zip$" } | Select-Object -First 1

            if (-not $asset) {
                throw "Could not find sccache Windows release asset on GitHub."
            }

            $zipPath = Join-Path $env:TEMP "sccache.zip"
            $unzipDir = Join-Path $env:TEMP "sccache_extracted"

            Write-Host "  [Sccache] Downloading $($asset.name)..." -ForegroundColor Gray
            Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zipPath -UseBasicParsing

            if (Test-Path $unzipDir) { Remove-Item $unzipDir -Recurse -Force }
            Expand-Archive -Path $zipPath -DestinationPath $unzipDir

            $extracted = Get-ChildItem -Path $unzipDir -Filter "sccache.exe" -Recurse | Select-Object -First 1
            if (-not $extracted) {
                throw "sccache.exe not found in downloaded archive."
            }

            New-Item -ItemType Directory -Force -Path $sccacheDir | Out-Null
            Copy-Item -Path $extracted.FullName -Destination $sccacheExe -Force

            Remove-Item $zipPath   -Force -ErrorAction SilentlyContinue
            Remove-Item $unzipDir  -Recurse -Force -ErrorAction SilentlyContinue

            Write-Host "  [Sccache] Installed to: $sccacheExe" -ForegroundColor Green
        }
    }
    else {
        Write-Host "  [Sccache] Found local binary: $sccacheExe" -ForegroundColor Gray
    }

    $env:SCCACHE_COMPILER_OVERRIDE = $ClExePath
    $env:SCCACHE_DIR                = $sccacheDir

    & $sccacheExe --stop-server  2>&1 | Out-Null
    Start-Sleep -Milliseconds 500
    & $sccacheExe --start-server 2>&1 | Out-Null

    $sccacheBinDir = Split-Path $sccacheExe -Parent
    if ($env:PATH -notmatch [regex]::Escape($sccacheBinDir)) {
        $env:PATH = "$sccacheBinDir;$env:PATH"
    }

    Write-Host "  [Sccache] Server restarted. Compiler override: $ClExePath" -ForegroundColor Green
}

function Assert-ClVersion {
    param(
        [string]$ClExePath,
        [string]$RequiredVersion
    )

    $fileVersion = (Get-Item $ClExePath).VersionInfo.FileVersion

    if (-not $fileVersion) {
        throw "Could not read file version from cl.exe at: $ClExePath"
    }

    $compilerMinor = $RequiredVersion.Split('.')[1]

    if ($fileVersion -notmatch "^19\.$compilerMinor\.") {
        if ($env:SAGA_CI_MODE -eq "true") {
            Write-Host "  [Toolchain] cl.exe version differs from pinned (required: 19.$compilerMinor.x, found: $fileVersion)" -ForegroundColor Yellow
        } else {
            throw "cl.exe version mismatch. Required: 19.$compilerMinor.x -- Found: $fileVersion"
        }
    }

    Write-Host "  [Toolchain] cl.exe verified: $ClExePath" -ForegroundColor Green
    Write-Host "  [Toolchain] File version: $fileVersion" -ForegroundColor Gray
}

function Assert-VisualStudioPrereqs {
    $msvc = Get-MSVCStatus

    if (-not $msvc.Found) {
        throw $msvc.Message
    }

    if (-not $msvc.Ok) {
        throw $msvc.Message
    }

    return $msvc.VsPath
}

function Get-VCToolsVersion {
    param([string]$VsPath)

    $vcRoot = Join-Path $VsPath "VC\Tools\MSVC"
    if (-not (Test-Path $vcRoot)) { throw "MSVC toolset not found under $vcRoot" }

    $toolsets = Get-ChildItem $vcRoot -Directory | Sort-Object Name -Descending
    if (-not $toolsets) { throw "No MSVC toolset directories found" }

    return $toolsets[0].Name
}

function Write-GeneratedConanProfiles {
    param(
        [string]$VsPath,
        [string]$Preset,
        [string]$ProfileName = "generated-windows-msvc"
    )

    New-Item -ItemType Directory -Force -Path $TOOLCHAIN_DIR, $PYTHON_DIR, $DOWNLOAD_DIR | Out-Null
    $vcVersion = Get-VCToolsVersion -VsPath $VsPath

    $hostProfile = @"
[settings]
os=Windows
arch=x86_64
build_type=RelWithDebInfo
compiler=msvc
compiler.version=193
compiler.cppstd=20
compiler.runtime=dynamic
compiler.runtime_type=Release
[options]
*:fPIC=True
*:shared=False
[conf]
tools.cmake.cmaketoolchain:generator=Ninja
tools.microsoft.msbuild:vs_version=17
[buildenv]
CC=cl
CXX=cl
"@

    $buildProfile = @"
[settings]
os=Windows
arch=x86_64
build_type=RelWithDebInfo
compiler=msvc
compiler.version=193
compiler.cppstd=20
compiler.runtime=dynamic
compiler.runtime_type=Release
[conf]
tools.cmake.cmaketoolchain:generator=Ninja
tools.microsoft.msbuild:vs_version=17
[buildenv]
CC=cl
CXX=cl
"@

    Set-Content -Path (Join-Path $PROFILE_DIR "$ProfileName-host") -Value $hostProfile -Encoding UTF8
    Set-Content -Path (Join-Path $PROFILE_DIR "$ProfileName-build") -Value $buildProfile -Encoding UTF8
}

function Install-VisualStudioBuildTools {

    Write-Host ""
    Write-Host "Visual Studio Build Tools 2022 not found." -ForegroundColor Yellow
    Write-Host "Installing automatically..." -ForegroundColor Cyan
    Write-Host ""

    $installerUrl = "https://aka.ms/vs/17/release/vs_BuildTools.exe"
    $installerPath = Join-Path $env:TEMP "vs_BuildTools.exe"

    Write-Host "Downloading installer..." -ForegroundColor Gray

    Invoke-WebRequest $installerUrl -OutFile $installerPath

    if (-not (Test-Path $installerPath)) {
        Write-Error "Installer download failed"
        return $null
    }

    Write-Host "Running installer..." -ForegroundColor Gray

    $arguments = @(
        "--quiet"
        "--wait"
        "--norestart"
        "--nocache"
        "--add Microsoft.VisualStudio.Workload.VCTools"
        "--add Microsoft.VisualStudio.Component.VC.CMake.Project"
        "--add Microsoft.VisualStudio.Component.Windows11SDK.22621"
        "--includeRecommended"
    )

    $process = Start-Process `
        -FilePath $installerPath `
        -ArgumentList $arguments `
        -PassThru

    while (-not $process.HasExited) {

        Write-Progress `
            -Activity "Installing Visual Studio Build Tools" `
            -Status "Installing MSVC toolchain..." `
            -PercentComplete 50

        Start-Sleep 2
    }

    Write-Progress `
        -Activity "Installing Visual Studio Build Tools" `
        -Completed

    if ($process.ExitCode -ne 0) {
        Write-Error "Installation failed"
        return $null
    }

    Write-Host "Installation complete." -ForegroundColor Green

    Start-Sleep 2

    return Find-VisualStudioBuildTools
}

function Enter-VisualStudioBuildToolsEnvironment {
    $vsPath = Find-VisualStudioBuildTools
    if (-not $vsPath) { throw "Visual Studio Build Tools 2022 not found." }

    $clCmd = Get-Command cl -ErrorAction SilentlyContinue
    if ($clCmd) {
        Write-Host "  [VS] cl.exe already in PATH: $($clCmd.Source)" -ForegroundColor Gray
        return
    }

    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        throw "vcvarsall.bat not found at: $vcvarsall"
    }

    Write-Host "  [VS] Initializing MSVC 14.38 environment..." -ForegroundColor Gray

    $envDump = cmd /c "`"$vcvarsall`" x64 -vcvars_ver=14.38 && set" 2>&1
    foreach ($line in $envDump) {
        if ($line -match "^([^=]+)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }

    Write-Host "  [VS] MSVC 14.38 environment loaded." -ForegroundColor Green
}

function Check-Prerequisites {
    Write-Host ""
    Write-Host "=== Checking Prerequisites ===" -ForegroundColor Cyan

    Enter-VisualStudioBuildToolsEnvironment
    Invoke-ToolchainValidation

    Ensure-CMake

    $cmakeVersion = Get-CMakeVersion
    if ($cmakeVersion) {
        Write-Host "  CMake: $cmakeVersion" -ForegroundColor Green
    } else {
        Write-Host "  CMake: NOT FOUND" -ForegroundColor Red
        throw "CMake not available after Ensure-CMake"
    }

    $ninja = Get-Command ninja -ErrorAction SilentlyContinue
    if ($ninja) {
        $ninjaVersion = & ninja --version 2>&1
        Write-Host "  Ninja: $ninjaVersion" -ForegroundColor Green
    }
    else {
        Write-Host "  Ninja: NOT FOUND - Installing via toolchain..." -ForegroundColor Yellow
        & $PYTHON_CMD -m pip install --quiet ninja==$NINJA_VERSION
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  Ninja: Installed" -ForegroundColor Green
        }
        else {
            throw "Failed to install Ninja"
        }
    }

    $buildToolsPath = Assert-VisualStudioPrereqs
    Write-Host "  Visual Studio: $buildToolsPath" -ForegroundColor Green

    Write-Host "  Checking Conan..." -ForegroundColor Gray
    $conanFound = $false
    if (Test-Path $CONAN_CMD) {
        try {
            $conanVersion = & $CONAN_CMD --version 2>&1
            if ($LASTEXITCODE -eq 0) {
                Write-Host "  Conan: $conanVersion" -ForegroundColor Green
                $conanFound = $true
            }
        }
        catch {
            $conanFound = $false
        }
    }

    if (-not $conanFound) {
        Write-Host "  Conan: NOT FOUND - Installing..." -ForegroundColor Yellow
        & $PYTHON_CMD -m pip install --quiet conan==$CONAN_VERSION
        if ($LASTEXITCODE -eq 0 -and (Test-Path $CONAN_CMD)) {
            Write-Host "  Conan: Installed" -ForegroundColor Green
        }
        else {
            throw "Failed to install Conan"
        }
    }

    Write-Host ""
    Write-Host "  All prerequisites OK!" -ForegroundColor Green

    $conanConfigDir = Join-Path $env:USERPROFILE ".conan2"
    $globalConf = Join-Path $conanConfigDir "global.conf"
    if (Test-Path $globalConf) {
        $confContent = Get-Content $globalConf -Raw
        if ($confContent -notmatch 'tools\.build:jobs') {
            Add-Content $globalConf "`ntools.build:jobs=1"
            Write-Host "  Conan: set tools.build:jobs=1 for verbose errors" -ForegroundColor Gray
        }
    }
}

$DEFAULT_PROFILE = "windows-msvc"
$DEFAULT_BUILD_PROFILE = "windows-build"

function Invoke-Doctor {
    Write-Host ""
    Write-Host "=== $BUILD_BRAND Doctor ===" -ForegroundColor Cyan
    Write-Host ""

    Enter-VisualStudioBuildToolsEnvironment

    $issues = 0

    $cmakeVersion = Get-CommandVersion -CommandName "cmake"
    if ($cmakeVersion) {
        if ($cmakeVersion -eq [version]$CMAKE_VERSION) {
            Write-Host "  CMake: OK ($cmakeVersion)" -ForegroundColor Green
        }
        else {
            Write-Host "  CMake: FAIL (found $cmakeVersion, required $CMAKE_VERSION)" -ForegroundColor Red
            $issues++
        }
    }
    else {
        Write-Host "  CMake: FAIL (not found, required $CMAKE_VERSION)" -ForegroundColor Red
        $issues++
    }

    $ninjaPkgVersion = Get-PipPackageVersion -PythonExe $PYTHON_CMD -PackageName "ninja"
    if ($ninjaPkgVersion) {
        if ($ninjaPkgVersion -eq $NINJA_VERSION) {
            Write-Host "  Ninja: OK ($ninjaPkgVersion)" -ForegroundColor Green
        }
        else {
            Write-Host "  Ninja: FAIL (found $ninjaPkgVersion, required $NINJA_VERSION)" -ForegroundColor Red
            $issues++
        }
    }
    else {
        Write-Host "  Ninja: FAIL (not found, required $NINJA_VERSION)" -ForegroundColor Red
        $issues++
    }

    $conanPkgVersion = Get-PipPackageVersion -PythonExe $PYTHON_CMD -PackageName "conan"
    if ($conanPkgVersion) {
        if ($conanPkgVersion -eq $CONAN_VERSION) {
            Write-Host "  Conan: OK ($conanPkgVersion)" -ForegroundColor Green
        }
        else {
            Write-Host "  Conan: FAIL (found $conanPkgVersion, required $CONAN_VERSION)" -ForegroundColor Red
            $issues++
        }
    }
    else {
        Write-Host "  Conan: FAIL (not found, required $CONAN_VERSION)" -ForegroundColor Red
        $issues++
    }

    $msvc = Get-MSVCStatus
    if ($msvc.Found -and $msvc.Ok) {
        Write-Host "  Visual Studio / MSVC: OK ($($msvc.Toolset))" -ForegroundColor Green
    }
    else {
        Write-Host "  Visual Studio / MSVC: FAIL" -ForegroundColor Red
        Write-Host "    $($msvc.Message)" -ForegroundColor Yellow
        Write-Host "    Install/modify: Visual Studio Build Tools 2022 + Desktop development with C++ + MSVC v143" -ForegroundColor Yellow
        $issues++
    }

    $atl = Get-ATLStatus
    if ($atl.Found -and $atl.Ok) {
        Write-Host "  Visual Studio / ATL: OK ($($atl.HeaderPath))" -ForegroundColor Green
    }
    else {
        Write-Host "  Visual Studio / ATL: FAIL" -ForegroundColor Red
        Write-Host "    $($atl.Message)" -ForegroundColor Yellow
        Write-Host "    Install/modify: Visual Studio Build Tools 2022 + Desktop development with C++ + ATL support" -ForegroundColor Yellow
        $issues++
    }

    Write-Host ""
    if ($issues -eq 0) {
        Write-Host "  Doctor found no blocking issues." -ForegroundColor Green
        exit 0
    }
    else {
        Write-Host "  Doctor found $issues issue(s)." -ForegroundColor Red
        exit 1
    }
}

function Invoke-ToolchainValidation {
    Write-Host ""
    Write-Host "  [Toolchain] Validating MSVC 14.38..." -ForegroundColor Gray

    try {
        $toolsetRoot = Find-MsvcToolset -RequiredVersion "14.38"
        Write-Host "  [Toolchain] Toolset root: $toolsetRoot" -ForegroundColor Gray

        $clExe = Get-ClExePath -ToolsetRoot $toolsetRoot
        Assert-ClVersion -ClExePath $clExe -RequiredVersion "14.38"
        Reset-Sccache -ClExePath $clExe

        Write-Host "  [Toolchain] Validation passed." -ForegroundColor Green
    }
    catch {
        Write-Error "[Toolchain] Validation failed: $_"
        exit 1
    }
}

function Invoke-Lock {
    param(
        [string]$Profile = $DEFAULT_PROFILE,
        [string]$BuildProfile = $DEFAULT_BUILD_PROFILE
    )

    Write-Host ""
    Write-Host "=== Generating Conan Lockfile ===" -ForegroundColor Cyan

    $hostProfilePath  = Resolve-ProfilePath $Profile
    $buildProfilePath = Resolve-ProfilePath $BuildProfile

    $result = Invoke-Native {
        & $CONAN_CMD lock create . `
            --profile:host="$hostProfilePath" `
            --profile:build="$buildProfilePath" `
            --lockfile-out=conan.lock `
            --build=missing
    }

    if ($result.ExitCode -ne 0) {
        if ($result.Output) { Write-Host $result.Output -ForegroundColor Red }
        throw "Failed to create lockfile"
    }

    Write-Host "=== Lockfile Created ===" -ForegroundColor Green
}

function Invoke-Native {
    param([scriptblock]$Script)

    $output = & $Script 2>&1
    [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output   = (($output | ForEach-Object { "$_" }) -join "`n")
    }
}

function New-ConanLockfile {
    param([string]$Profile)

    $result = Invoke-Native {
        & $CONAN_CMD lock create . `
            --profile:host=(Resolve-ProfilePath $Profile) `
            --profile:build=(Resolve-ProfilePath $BuildProfile) `
            --lockfile-out=conan.lock `
            --build=missing
    }

    if ($result.ExitCode -ne 0) {
        if ($result.Output) { Write-Host $result.Output -ForegroundColor Red }
        Write-Error "Failed to create lockfile"
        exit 1
    }
}

function Install-ConanDependencies {
    param(
        [string]$Profile,
        [string]$Preset
    )

    $uniquePath = ($env:PATH -split ';' | Where-Object { $_ -ne '' } | Select-Object -Unique) -join ';'
    $env:PATH = $uniquePath

    $toolchainPythonDir = Split-Path $PYTHON_CMD -Parent
    $toolchainScripts   = Join-Path $toolchainPythonDir "Scripts"
    $env:PATH = ($env:PATH -split ';' | Where-Object { $_ -notmatch 'WindowsApps' } | Select-Object -Unique) -join ';'
    $env:PATH = "$toolchainPythonDir;$toolchainScripts;$env:PATH"
    Write-Host "  PATH: toolchain Python injected ($($env:PATH.Length) chars)" -ForegroundColor Gray

    $lockPath = Join-Path $SCRIPT_DIR "conan.lock"

    if (-not (Test-Path $lockPath)) {
        Write-Host ""
        Write-Host "=========================================" -ForegroundColor Yellow
        Write-Host "  LOCKFILE NOT FOUND" -ForegroundColor Yellow
        Write-Host "=========================================" -ForegroundColor Yellow
        Write-Host "Run: .\build.ps1 lock" -ForegroundColor Yellow
        exit 1
    }

    Write-Host ""
    Write-Host "  Installing Conan dependencies (first run: 20-60+ min)..." -ForegroundColor Cyan
    Write-Host ""

    & $CONAN_CMD install . `
        --profile:host=profiles/$Profile `
        --profile:build=profiles/windows-build `
        --lockfile=$lockPath `
        --build=missing `
        --output-folder=Build/$Preset

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to install dependencies using lockfile: $lockPath"
        exit 1
    }

    Write-Host ""
    Write-Host "  Dependencies installed." -ForegroundColor Green
}

function Invoke-Conan {
    param(
        [Parameter(Mandatory=$true)]
        [string[]]$Args
    )

    $output = & $CONAN_CMD @Args 2>&1
    [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output   = (($output | ForEach-Object { "$_" }) -join "`n")
    }
}

function Invoke-Setup {
    param(
        [string]$Profile = $DEFAULT_PROFILE,
        [string]$Preset = $DEFAULT_PRESET,
        [string]$Config = "RelWithDebInfo"
    )

    Write-Host ""
    Write-Host "=== $BUILD_BRAND Setup ===" -ForegroundColor Cyan
    Write-Host "Profile: $Profile" -ForegroundColor Gray
    Write-Host "Preset:  $Preset" -ForegroundColor Gray
    Write-Host ""

    $profilesRoot = Join-Path $SCRIPT_DIR "profiles"
    $profilePath = Join-Path $profilesRoot $Profile
    if (-not (Test-Path $profilePath)) {
        Write-Error "Profile not found: $profilePath"
        Write-Host "Available profiles:" -ForegroundColor Yellow
        Get-ChildItem $profilesRoot -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "  - $($_.Name)" }
        exit 1
    }

    Ensure-CMake
    Install-ConanDependencies -Profile $Profile -Preset $Preset


    Write-Host "Configuring CMake..." -ForegroundColor Yellow
    $vsPath = Find-VisualStudioBuildTools
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    cmd /c "`"$vcvars`" && cmake --preset $Preset"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to configure CMake"
        exit 1
    }

    Write-Host ""
    Write-Host "=== Setup Complete ===" -ForegroundColor Green
    Write-Host "Next: .\build.ps1 build" -ForegroundColor Cyan
}

function Invoke-Build {
    param(
        [string]$Preset = $DEFAULT_PRESET,
        [string]$Config = "RelWithDebInfo"
    )

    Write-Host ""
    Write-Host "=== Building SagaEngine ===" -ForegroundColor Cyan
    Write-Host "Preset: $Preset" -ForegroundColor Gray
    Write-Host "Config: $Config" -ForegroundColor Gray
    Write-Host ""

    if (-not (Test-Path "conan.lock")) {
        Write-Error "conan.lock not found. Run '.\build.ps1 setup' first."
        exit 1
    }

    $env:SCCACHE_DIR = "$SCRIPT_DIR\cache\sccache"
    New-Item -ItemType Directory -Force -Path $env:SCCACHE_DIR | Out-Null

    $vsPath = Find-VisualStudioBuildTools
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    cmd /c "`"$vcvars`" && cmake --build --preset $Preset --config $Config"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed!"
        exit 1
    }

    Ensure-CMake

    Write-Host ""
    Write-Host "=== Build Complete ===" -ForegroundColor Green
    Write-Host "Binaries: Bin\$Config\" -ForegroundColor Cyan
}

function Invoke-Test {
    param(
        [string]$Preset = $DEFAULT_PRESET,
        [string]$Config = "RelWithDebInfo"
    )

    Write-Host ""
    Write-Host "=== Running Tests ===" -ForegroundColor Cyan

    if (-not (Test-Path "conan.lock")) {
        Write-Error "conan.lock not found. Run '.\build.ps1 setup' first."
        exit 1
    }

    $BUILD_DIR = "Build\$Preset"
    if (-not (Test-Path $BUILD_DIR)) {
        Write-Error "Build directory not found: $BUILD_DIR"
        exit 1
    }

    $parallel = [Environment]::ProcessorCount
    ctest --test-dir $BUILD_DIR --output-on-failure --build-config $Config --parallel $parallel
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Some tests failed!"
        exit 1
    } else {
        Write-Host ""
        Write-Host "=== All Tests Passed ===" -ForegroundColor Green
    }

    Ensure-CMake
}

function Invoke-Clean {
    Write-Host ""
    Write-Host "=== Cleaning ===" -ForegroundColor Cyan
    $paths = @("Build", "Bin", "Install", "packages", "cache\sccache\*")
    foreach ($path in $paths) {
        Write-Host "  Removing: $path" -ForegroundColor Gray
        Remove-Item -Recurse -Force $path -ErrorAction SilentlyContinue
    }
    Write-Host "=== Clean Complete ===" -ForegroundColor Green
}

function Show-Help {
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host "  $BUILD_BRAND Build System" -ForegroundColor Cyan
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Commands:" -ForegroundColor Yellow
    Write-Host "  setup     Install dependencies and configure" -ForegroundColor White
    Write-Host "  build     Build the project" -ForegroundColor White
    Write-Host "  doctor    Diagnose local toolchain problems" -ForegroundColor White
    Write-Host "  test      Run tests" -ForegroundColor White
    Write-Host "  clean     Clean build artifacts" -ForegroundColor White
    Write-Host "  help      Show this help" -ForegroundColor White
    Write-Host ""
    Write-Host "Requirements:" -ForegroundColor Yellow
    Write-Host "  CMake:  3.24+ (system)" -ForegroundColor White
    Write-Host "  Python: 3.12.x (auto-installed in .toolchain/)" -ForegroundColor White
    Write-Host "  Ninja:  1.10+ (auto-installed)" -ForegroundColor White
    Write-Host "  Conan:  2.0+ (auto-installed)" -ForegroundColor White
    Write-Host ""
    Write-Host "Detected:" -ForegroundColor Yellow
    Write-Host "  Python: $($PYTHON_INFO.Version) (toolchain)" -ForegroundColor White
    Write-Host ""
}

$COMMAND = $null
$PROFILE = $DEFAULT_PROFILE
$PRESET = $DEFAULT_PRESET
$CONFIG = "RelWithDebInfo"

for ($i = 0; $i -lt $args.Length; $i++) {
    switch ($args[$i]) {
        "setup" { $COMMAND = "setup" }
        "build" { $COMMAND = "build" }
        "test" { $COMMAND = "test" }
        "clean" { $COMMAND = "clean" }
        "help" { $COMMAND = "help" }
        "lock" { $COMMAND = "lock" }
        "doctor" { $COMMAND = "doctor" }
        "-Profile" { $PROFILE = $args[++$i] }
        "-Preset" { $PRESET = $args[++$i] }
        "-Config" { $CONFIG = $args[++$i] }
        default { Write-Error "Unknown option: $($args[$i])"; Show-Help; exit 1 }
    }
}

if ($null -eq $COMMAND) {
    $COMMAND = "help"
}

if ($COMMAND -in @("setup", "build", "test", "lock")) {
    Check-Prerequisites
}

switch ($COMMAND) {
    "setup" { Invoke-Setup -Profile $PROFILE -Preset $PRESET -Config $CONFIG }
    "build" { Invoke-Build -Preset $PRESET -Config $CONFIG }
    "test" { Invoke-Test -Preset $PRESET -Config $CONFIG }
    "lock" { Invoke-Lock -Profile $PROFILE }
    "doctor" { Invoke-Doctor }
    "clean" { Invoke-Clean }
    "help" { Show-Help }
    default { Write-Error "Unknown command: $COMMAND"; Show-Help; exit 1 }
}