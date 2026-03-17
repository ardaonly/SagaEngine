$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $PSCommandPath
Set-Location $SCRIPT_DIR

Write-Host "=== SagaEngine Build System ===" -ForegroundColor Cyan
Write-Host "Directory: $SCRIPT_DIR" -ForegroundColor Gray
Write-Host ""

# ─────────────────────────────────────────────────────────────
# PYTHON REQUIREMENTS
# ─────────────────────────────────────────────────────────────
$PYTHON_REQUIRED_MAJOR = 3
$PYTHON_REQUIRED_MINOR = 12
$PYTHON_MIN_MINOR = 10
$PYTHON_MAX_MINOR = 12

function Find-Python {
    Write-Host "Detecting Python..." -ForegroundColor Gray
    
    $pythonCmds = @("py", "python", "python3")
    
    foreach ($cmd in $pythonCmds) {
        try {
            # FIXED: Use single quotes and % formatting (no f-strings)
            $version = & $cmd -c 'import sys; v=sys.version_info; print("%d.%d.%d" % (v.major, v.minor, v.micro))'
            if ($LASTEXITCODE -eq 0) {
                $parts = $version -split '\.'
                $major = [int]$parts[0]
                $minor = [int]$parts[1]
                
                if ($major -eq $PYTHON_REQUIRED_MAJOR -and $minor -ge $PYTHON_MIN_MINOR -and $minor -le $PYTHON_MAX_MINOR) {
                    Write-Host "  Found: $cmd (Python $version)" -ForegroundColor Green
                    return @{ Cmd = $cmd; Version = $version; Minor = $minor }
                }
            }
        } catch {
            continue
        }
    }
    
    return $null
}

$PYTHON_INFO = Find-Python

if (-not $PYTHON_INFO) {
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host "  PYTHON NOT FOUND" -ForegroundColor Red
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "SagaEngine requires Python ${PYTHON_REQUIRED_MAJOR}.${PYTHON_REQUIRED_MINOR}.x" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Download: https://www.python.org/downloads/release/python-${PYTHON_REQUIRED_MAJOR}${PYTHON_REQUIRED_MINOR}0/" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Installation Instructions:" -ForegroundColor Yellow
    Write-Host "  1. Download Python ${PYTHON_REQUIRED_MAJOR}.${PYTHON_REQUIRED_MINOR}.x (64-bit)" -ForegroundColor White
    Write-Host "  2. Run installer" -ForegroundColor White
    Write-Host "  3. Check 'Add Python to PATH'" -ForegroundColor White
    Write-Host "  4. Click 'Install Now'" -ForegroundColor White
    Write-Host ""
    Write-Host "After installation, restart PowerShell and run:" -ForegroundColor Yellow
    Write-Host "  .\build.ps1 setup" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

if ($PYTHON_INFO.Minor -lt $PYTHON_REQUIRED_MINOR) {
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Yellow
    Write-Host "  PYTHON VERSION TOO OLD" -ForegroundColor Yellow
    Write-Host "=========================================" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Found: Python $($PYTHON_INFO.Version)" -ForegroundColor Red
    Write-Host "Required: Python ${PYTHON_REQUIRED_MAJOR}.${PYTHON_REQUIRED_MINOR}.x or higher" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Please upgrade Python:" -ForegroundColor Cyan
    Write-Host "  https://www.python.org/downloads/" -ForegroundColor Cyan
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

if ($PYTHON_INFO.Minor -gt $PYTHON_MAX_MINOR) {
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Yellow
    Write-Host "  PYTHON VERSION MAY BE INCOMPATIBLE" -ForegroundColor Yellow
    Write-Host "=========================================" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Found: Python $($PYTHON_INFO.Version)" -ForegroundColor Yellow
    Write-Host "Recommended: Python ${PYTHON_REQUIRED_MAJOR}.${PYTHON_REQUIRED_MINOR}.x" -ForegroundColor Green
    Write-Host ""
    Write-Host "Continuing anyway (you may encounter issues)..." -ForegroundColor Gray
    Write-Host ""
}

$PYTHON_CMD = $PYTHON_INFO.Cmd

# ─────────────────────────────────────────────────────────────
# PIP CHECK
# ─────────────────────────────────────────────────────────────
function Find-Pip {
    param([string]$PythonCmd)
    
    Write-Host "Detecting pip..." -ForegroundColor Gray
    
    try {
        $result = & $PythonCmd -m pip --version 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  Found: $PythonCmd -m pip" -ForegroundColor Green
            return $PythonCmd
        }
    } catch {
        Write-Host "  pip not found" -ForegroundColor Red
    }
    
    return $null
}

$PIP_CMD = Find-Pip -PythonCmd $PYTHON_CMD

if (-not $PIP_CMD) {
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host "  PIP NOT FOUND" -ForegroundColor Red
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Python is installed but pip is missing." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Fix:" -ForegroundColor Cyan
    Write-Host "  $PYTHON_CMD -m ensurepip --upgrade" -ForegroundColor White
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# ─────────────────────────────────────────────────────────────
# PREREQUISITE CHECK
# ─────────────────────────────────────────────────────────────
$CONAN_VERSION = "2.0.17"
$NINJA_VERSION = "1.12.1"

function Check-Prerequisites {
    Write-Host ""
    Write-Host "=== Checking Prerequisites ===" -ForegroundColor Cyan
    
    # CMake
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        $cmakeVersion = & cmake --version 2>&1 | Select-Object -First 1
        Write-Host "  CMake: $cmakeVersion" -ForegroundColor Green
    } else {
        Write-Host "  CMake: NOT FOUND" -ForegroundColor Red
        Write-Host "    Install: https://cmake.org/download/" -ForegroundColor Yellow
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    # Ninja
    $ninja = Get-Command ninja -ErrorAction SilentlyContinue
    if ($ninja) {
        $ninjaVersion = & ninja --version 2>&1
        Write-Host "  Ninja: $ninjaVersion" -ForegroundColor Green
    } else {
        Write-Host "  Ninja: NOT FOUND" -ForegroundColor Red
        Write-Host "    Install: $PYTHON_CMD -m pip install ninja" -ForegroundColor Yellow
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    # Conan
    Write-Host "  Checking Conan..." -ForegroundColor Gray
    $conanFound = $false
    try {
        $conanVersion = & $PYTHON_CMD -m conan --version 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  Conan: $conanVersion" -ForegroundColor Green
            $conanFound = $true
        }
    } catch {
        $conanFound = $false
    }
    
    if (-not $conanFound) {
        Write-Host "  Conan: NOT FOUND - Installing..." -ForegroundColor Yellow
        & $PYTHON_CMD -m pip install --quiet conan==$CONAN_VERSION
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  Conan: Installed" -ForegroundColor Green
        } else {
            Write-Error "Failed to install Conan"
            Read-Host "Press Enter to exit"
            exit 1
        }
    }
    
    Write-Host ""
    Write-Host "  All prerequisites OK!" -ForegroundColor Green
}

# ─────────────────────────────────────────────────────────────
# SETUP COMMAND
# ─────────────────────────────────────────────────────────────
$DEFAULT_PROFILE = "windows-msvc"
$DEFAULT_PRESET = "windows-msvc-14.38"

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
    
    $profilePath = Join-Path $SCRIPT_DIR "profiles\$Profile"
    if (-not (Test-Path $profilePath)) {
        Write-Error "Profile not found: $profilePath"
        Write-Host "Available profiles:" -ForegroundColor Yellow
        Get-ChildItem "profiles\" -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "  - $($_.Name)" }
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    if (-not (Test-Path "conan.lock")) {
        Write-Host "Generating lockfile..." -ForegroundColor Yellow
        & $PYTHON_CMD -m conan lock create conanfile.py `
            --profile=profiles/$Profile `
            --lockfile-out=conan.lock `
            --build=missing
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Failed to create lockfile"
            Read-Host "Press Enter to exit"
            exit 1
        }
    } else {
        Write-Host "Lockfile found: conan.lock" -ForegroundColor Green
    }
    
    Write-Host "Installing dependencies (this may take a while)..." -ForegroundColor Yellow
    & $PYTHON_CMD -m conan install . `
        --profile=profiles/$Profile `
        --lockfile=conan.lock `
        --build=missing `
        --output-folder=Build/$Preset
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to install dependencies"
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    Write-Host "Configuring CMake..." -ForegroundColor Yellow
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to configure CMake"
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    Write-Host ""
    Write-Host "=== Setup Complete ===" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  .\build.ps1 build" -ForegroundColor White
    Write-Host "  .\build.ps1 test" -ForegroundColor White
    Write-Host ""
    Read-Host "Press Enter to continue"
}

# ─────────────────────────────────────────────────────────────
# BUILD COMMAND
# ─────────────────────────────────────────────────────────────
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
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    $env:SCCACHE_DIR = "$SCRIPT_DIR\cache\sccache"
    New-Item -ItemType Directory -Force -Path $env:SCCACHE_DIR | Out-Null
    
    cmake --build --preset $Preset --config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed!"
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    Write-Host ""
    Write-Host "=== Build Complete ===" -ForegroundColor Green
    Write-Host "Binaries: Bin\$Config\" -ForegroundColor Cyan
    Write-Host ""
    Read-Host "Press Enter to continue"
}

# ─────────────────────────────────────────────────────────────
# TEST COMMAND
# ─────────────────────────────────────────────────────────────
function Invoke-Test {
    param(
        [string]$Preset = $DEFAULT_PRESET,
        [string]$Config = "RelWithDebInfo"
    )
    
    Write-Host ""
    Write-Host "=== Running Tests ===" -ForegroundColor Cyan
    Write-Host ""
    
    if (-not (Test-Path "conan.lock")) {
        Write-Error "conan.lock not found. Run '.\build.ps1 setup' first."
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    $BUILD_DIR = "Build\$Preset"
    if (-not (Test-Path $BUILD_DIR)) {
        Write-Error "Build directory not found: $BUILD_DIR"
        Write-Host "Run '.\build.ps1 build' first" -ForegroundColor Yellow
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    $parallel = [Environment]::ProcessorCount
    ctest --test-dir $BUILD_DIR --output-on-failure --build-config $Config --parallel $parallel
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Some tests failed!"
    } else {
        Write-Host ""
        Write-Host "=== All Tests Passed ===" -ForegroundColor Green
    }
    Read-Host "Press Enter to continue"
}

# ─────────────────────────────────────────────────────────────
# CLEAN COMMAND
# ─────────────────────────────────────────────────────────────
function Invoke-Clean {
    Write-Host ""
    Write-Host "=== Cleaning ===" -ForegroundColor Cyan
    
    $paths = @("Build", "Bin", "Install", "packages", "cache\sccache\*")
    foreach ($path in $paths) {
        Write-Host "  Removing: $path" -ForegroundColor Gray
        Remove-Item -Recurse -Force $path -ErrorAction SilentlyContinue
    }
    
    Write-Host "=== Clean Complete ===" -ForegroundColor Green
    Read-Host "Press Enter to continue"
}

# ─────────────────────────────────────────────────────────────
# HELP
# ─────────────────────────────────────────────────────────────
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
    Write-Host "Options:" -ForegroundColor Yellow
    Write-Host "  -Profile PROFILE    Conan profile (default: $DEFAULT_PROFILE)" -ForegroundColor White
    Write-Host "  -Preset PRESET      CMake preset (default: $DEFAULT_PRESET)" -ForegroundColor White
    Write-Host "  -Config CONFIG      Build config (default: RelWithDebInfo)" -ForegroundColor White
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Yellow
    Write-Host "  .\build.ps1 setup" -ForegroundColor White
    Write-Host "  .\build.ps1 build -Preset windows-msvc-14.38" -ForegroundColor White
    Write-Host "  .\build.ps1 test -Config Release" -ForegroundColor White
    Write-Host ""
    Write-Host "Requirements:" -ForegroundColor Yellow
    Write-Host "  Python: ${PYTHON_REQUIRED_MAJOR}.${PYTHON_REQUIRED_MINOR}.x" -ForegroundColor White
    Write-Host "  CMake:  3.24+" -ForegroundColor White
    Write-Host "  Ninja:  1.10+" -ForegroundColor White
    Write-Host "  Conan:  2.0+ (auto-installed)" -ForegroundColor White
    Write-Host ""
    Write-Host "Detected:" -ForegroundColor Yellow
    Write-Host "  Python: $($PYTHON_INFO.Version) ($PYTHON_CMD)" -ForegroundColor White
    Write-Host ""
    Read-Host "Press Enter to continue"
}

# ─────────────────────────────────────────────────────────────
# MAIN
# ─────────────────────────────────────────────────────────────
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
    "clean" { Invoke-Clean }
    "help" { Show-Help }
    default { Write-Error "Unknown command: $COMMAND"; Show-Help; exit 1 }
}