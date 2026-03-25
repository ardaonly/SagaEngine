$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $PSCommandPath
Set-Location $SCRIPT_DIR

$PROFILE_DIR = Join-Path $SCRIPT_DIR "profiles"
$TOOLCHAIN_DIR = Join-Path $SCRIPT_DIR ".toolchain"
$PYTHON_PATH_FILE = Join-Path $TOOLCHAIN_DIR "python_path.txt"
$BOOTSTRAP_SCRIPT = Join-Path $SCRIPT_DIR "bootstrap.ps1"
$ROOT_VERSION_FILE = Join-Path $SCRIPT_DIR "version.json"
$TOOLCHAIN_VERSION_FILE = Join-Path $TOOLCHAIN_DIR "version.json"

function Resolve-ProfilePath {
    param([Parameter(Mandatory=$true)][string]$Name)

    $path = Join-Path $PROFILE_DIR $Name
    if (-not (Test-Path $path)) {
        throw "Profile file not found: $path"
    }
    return $path
}
function Resolve-VersionFile {
    $preferred = Join-Path $TOOLCHAIN_DIR "versions.json"
    $legacy = Join-Path $TOOLCHAIN_DIR "version.json"
    if (Test-Path $preferred) { return $preferred }
    if (Test-Path $legacy) { return $legacy }
    throw "No toolchain version file found. Expected .toolchain\versions.json"
}

function Get-ToolchainConfig {
    $path = Resolve-VersionFile
    return Get-Content $path -Raw | ConvertFrom-Json
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
    $cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue

    if ($TOOLCHAIN.cmake.use_system -and $cmakeCmd) {
        $current = (& cmake --version 2>&1 | Select-Object -First 1)
        if ($current -match '(\d+\.\d+\.\d+)') {
            $ver = [version]$matches[1]
            if ($ver -ge $required) { return }
        }
    }

    & $PYTHON_CMD -m pip install --quiet "cmake==$CMAKE_VERSION"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to ensure CMake $CMAKE_VERSION"
    }
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

    New-Item -ItemType Directory -Force -Path $PROFILE_DIR | Out-Null
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
    if (-not $vsPath) {
        throw "Visual Studio Build Tools 2022 not found"
    }

    $devCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $devCmd)) {
        throw "VsDevCmd.bat not found: $devCmd"
    }

    $envDump = cmd /c """$devCmd"" -arch=x64 -host_arch=x64 && set"
    foreach ($line in $envDump) {
        if ($line -match "^(.*?)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

$CONAN_VERSION = "2.0.17"
$NINJA_VERSION = "1.11.1.4"

function Check-Prerequisites {
    Write-Host ""
    Write-Host "=== Checking Prerequisites ===" -ForegroundColor Cyan

    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        $cmakeVersion = & cmake --version 2>&1 | Select-Object -First 1
        Write-Host "  CMake: $cmakeVersion" -ForegroundColor Green
    }
    else {
        Write-Host "  CMake: NOT FOUND" -ForegroundColor Red
        Write-Host "    Install: https://cmake.org/download/" -ForegroundColor Yellow
        exit 1
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
            Write-Error "Failed to install Ninja"
            exit 1
        }
    }

    $buildToolsPath = Find-VisualStudioBuildTools
    if (-not $buildToolsPath) {
        $buildToolsPath = Install-VisualStudioBuildTools
    }

    if (-not $buildToolsPath) {
        Write-Error "Visual Studio Build Tools 2022 not found or could not be installed"
        exit 1
    }

    Enter-VisualStudioBuildToolsEnvironment

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
            Write-Error "Failed to install Conan"
            exit 1
        }
    }

    Write-Host ""
    Write-Host "  All prerequisites OK!" -ForegroundColor Green
}

$DEFAULT_PROFILE = "windows-msvc"
$DEFAULT_BUILD_PROFILE = "windows-build"

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

    $lockPath = Join-Path $SCRIPT_DIR "conan.lock"

    if (-not (Test-Path $lockPath)) {
        Write-Host ""
        Write-Host "=========================================" -ForegroundColor Yellow
        Write-Host "  LOCKFILE NOT FOUND" -ForegroundColor Yellow
        Write-Host "=========================================" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Expected lockfile: $lockPath" -ForegroundColor Gray
        Write-Host "This build system no longer generates conan.lock during setup." -ForegroundColor Yellow
        Write-Host "Please commit a valid lockfile or create it explicitly in a separate step." -ForegroundColor Yellow
        exit 1
    }

    $installCmd = {
        & $CONAN_CMD install . `
            --profile:host=profiles/$Profile `
            --profile:build=profiles/windows-build `
            --lockfile=$lockPath `
            --build=missing `
            --output-folder=Build/$Preset
    }

    $probe = Invoke-Native $installCmd
    if ($probe.ExitCode -eq 0) {
        return
    }

    if ($probe.Output) { Write-Host $probe.Output -ForegroundColor Red }
    Write-Error "Failed to install dependencies using existing lockfile: $lockPath"
    exit 1
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
    Write-Host "=== SagaEngine Setup ===" -ForegroundColor Cyan
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

    Install-ConanDependencies -Profile $Profile -Preset $Preset

    Write-Host "Configuring CMake..." -ForegroundColor Yellow
    cmake --preset $Preset
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

    cmake --build --preset $Preset --config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed!"
        exit 1
    }

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
    Write-Host "  SagaEngine Build System" -ForegroundColor Cyan
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Commands:" -ForegroundColor Yellow
    Write-Host "  setup     Install dependencies and configure" -ForegroundColor White
    Write-Host "  build     Build the project" -ForegroundColor White
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
        "-Profile" { $PROFILE = $args[++$i] }
        "-Preset" { $PRESET = $args[++$i] }
        "-Config" { $CONFIG = $args[++$i] }
        default { Write-Error "Unknown option: $($args[$i])"; Show-Help; exit 1 }
    }
}

if ($null -eq $COMMAND) {
    $COMMAND = "help"
}

if ($COMMAND -ne "help") {
    Check-Prerequisites
}

switch ($COMMAND) {
    "setup" { Invoke-Setup -Profile $PROFILE -Preset $PRESET -Config $CONFIG }
    "build" { Invoke-Build -Preset $PRESET -Config $CONFIG }
    "test" { Invoke-Test -Preset $PRESET -Config $CONFIG }
    "lock" { Invoke-Lock -Profile $PROFILE }
    "clean" { Invoke-Clean }
    "help" { Show-Help }
    default { Write-Error "Unknown command: $COMMAND"; Show-Help; exit 1 }
}