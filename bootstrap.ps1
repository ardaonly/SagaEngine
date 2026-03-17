$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $PSCommandPath
Set-Location $SCRIPT_DIR

$TOOLCHAIN_DIR = Join-Path $SCRIPT_DIR ".toolchain"
$PYTHON_DIR = Join-Path $TOOLCHAIN_DIR "python"
$VERSION_FILE = Join-Path $TOOLCHAIN_DIR "version.json"
$DOWNLOAD_DIR = Join-Path $TOOLCHAIN_DIR "downloads"

Write-Host "=== SagaEngine Toolchain Bootstrap ===" -ForegroundColor Cyan
Write-Host "Toolchain Directory: $TOOLCHAIN_DIR" -ForegroundColor Gray
Write-Host ""

# ─────────────────────────────────────────────────────────────
# LOAD VERSION CONFIG
# ─────────────────────────────────────────────────────────────
if (-not (Test-Path $VERSION_FILE)) {
    Write-Error "version.json not found: $VERSION_FILE"
    exit 1
}

$versionConfig = Get-Content $VERSION_FILE | ConvertFrom-Json
$PYTHON_VERSION = $versionConfig.python.version
$CONAN_VERSION = $versionConfig.conan.version
$NINJA_VERSION = $versionConfig.ninja.version

# Extract minor version for path (3.12.7 → 312)
$PYTHON_MINOR = $PYTHON_VERSION.Split('.')[0] + $PYTHON_VERSION.Split('.')[1]

Write-Host "Required Versions:" -ForegroundColor Yellow
Write-Host "  Python: $PYTHON_VERSION"
Write-Host "  Conan:  $CONAN_VERSION"
Write-Host "  Ninja:  $NINJA_VERSION"
Write-Host ""

# ─────────────────────────────────────────────────────────────
# CHECK EXISTING TOOLCHAIN
# ─────────────────────────────────────────────────────────────
$PYTHON_EXE = Join-Path $PYTHON_DIR "python.exe"
$PYTHON_VERSION_FILE = Join-Path $PYTHON_DIR ".version"

$toolchainValid = $false
if (Test-Path $PYTHON_EXE) {
    if (Test-Path $PYTHON_VERSION_FILE) {
        $installedVersion = Get-Content $PYTHON_VERSION_FILE
        if ($installedVersion -eq $PYTHON_VERSION) {
            Write-Host "Toolchain already installed (Python $installedVersion)" -ForegroundColor Green
            $toolchainValid = $true
        } else {
            Write-Host "Version mismatch: $installedVersion vs $PYTHON_VERSION" -ForegroundColor Yellow
        }
    }
}

# ─────────────────────────────────────────────────────────────
# DOWNLOAD AND EXTRACT PYTHON (EMBED ZIP)
# ─────────────────────────────────────────────────────────────
if (-not $toolchainValid) {
    Write-Host "Installing Python $PYTHON_VERSION..." -ForegroundColor Yellow
    
    # Clean existing
    if (Test-Path $PYTHON_DIR) {
        Remove-Item -Recurse -Force $PYTHON_DIR
    }
    New-Item -ItemType Directory -Force -Path $PYTHON_DIR | Out-Null
    New-Item -ItemType Directory -Force -Path $DOWNLOAD_DIR | Out-Null
    
    # Download Python embeddable zip
    $PYTHON_ZIP = Join-Path $DOWNLOAD_DIR "python-$PYTHON_VERSION-embed-amd64.zip"
    $PYTHON_URL = "https://www.python.org/ftp/python/$PYTHON_VERSION/python-$PYTHON_VERSION-embed-amd64.zip"
    
    if (-not (Test-Path $PYTHON_ZIP)) {
        Write-Host "  Downloading: $PYTHON_URL" -ForegroundColor Gray
        Invoke-WebRequest -Uri $PYTHON_URL -OutFile $PYTHON_ZIP -UseBasicParsing
    }
    
    # Extract
    Write-Host "  Extracting..." -ForegroundColor Gray
    Expand-Archive -Path $PYTHON_ZIP -DestinationPath $PYTHON_DIR -Force
    Remove-Item $PYTHON_ZIP
    
    # Configure python312._pth (CRITICAL - enables site-packages)
    $PTH_FILE = Join-Path $PYTHON_DIR "python$PYTHON_MINOR._pth"
    if (Test-Path $PTH_FILE) {
        Write-Host "  Configuring python$PYTHON_MINOR._pth..." -ForegroundColor Gray
        
        $pthContent = @'
python$PYTHON_VERSION.zip
DLLs\
Lib\
.
import site
'@
        $pthContent = $pthContent -replace '\$PYTHON_VERSION', $PYTHON_VERSION
        Set-Content -Path $PTH_FILE -Value $pthContent
    }
    
    # Create site-packages directory
    $SITE_PACKAGES = Join-Path $PYTHON_DIR "Lib\site-packages"
    New-Item -ItemType Directory -Force -Path $SITE_PACKAGES | Out-Null
    
    # Save version
    $PYTHON_VERSION | Set-Content $PYTHON_VERSION_FILE
    
    Write-Host "  Python extracted successfully" -ForegroundColor Green
}

# ─────────────────────────────────────────────────────────────
# BOOTSTRAP PIP (get-pip.py)
# ─────────────────────────────────────────────────────────────
Write-Host "Bootstrapping pip..." -ForegroundColor Gray

$GET_PIP_PATH = Join-Path $DOWNLOAD_DIR "get-pip.py"
$GET_PIP_URL = "https://bootstrap.pypa.io/get-pip.py"

if (-not (Test-Path $GET_PIP_PATH)) {
    Invoke-WebRequest -Uri $GET_PIP_URL -OutFile $GET_PIP_PATH -UseBasicParsing
}

& $PYTHON_EXE $GET_PIP_PATH --quiet 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to bootstrap pip"
    exit 1
}

Write-Host "  pip bootstrapped" -ForegroundColor Green

# Upgrade pip
Write-Host "Upgrading pip..." -ForegroundColor Gray
& $PYTHON_EXE -m pip install --quiet --upgrade pip 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) {
    $pipVersion = & $PYTHON_EXE -m pip --version 2>&1 | Select-Object -First 1
    Write-Host "  $pipVersion" -ForegroundColor Green
} else {
    Write-Host "  pip upgrade skipped" -ForegroundColor Yellow
}

# ─────────────────────────────────────────────────────────────
# INSTALL CONAN
# ─────────────────────────────────────────────────────────────
Write-Host "Checking Conan..." -ForegroundColor Gray
$conanCheck = & $PYTHON_EXE -m pip show conan 2>&1 | Select-String "Version:" | Out-String
if ($LASTEXITCODE -ne 0 -or $conanCheck -notlike "*$CONAN_VERSION*") {
    Write-Host "  Installing Conan $CONAN_VERSION..." -ForegroundColor Yellow
    & $PYTHON_EXE -m pip install --quiet conan==$CONAN_VERSION
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  Conan installed" -ForegroundColor Green
    } else {
        Write-Error "Failed to install Conan"
        exit 1
    }
} else {
    $version = $conanCheck -replace "Version:\s*", ""
    Write-Host "  Conan $version already installed" -ForegroundColor Green
}

# ─────────────────────────────────────────────────────────────
# INSTALL NINJA
# ─────────────────────────────────────────────────────────────
Write-Host "Checking Ninja..." -ForegroundColor Gray
$ninjaCheck = & $PYTHON_EXE -m pip show ninja 2>&1 | Select-String "Version:" | Out-String
if ($LASTEXITCODE -ne 0) {
    Write-Host "  Installing Ninja $NINJA_VERSION..." -ForegroundColor Yellow
    & $PYTHON_EXE -m pip install --quiet ninja==$NINJA_VERSION
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  Ninja installed" -ForegroundColor Green
    } else {
        Write-Error "Failed to install Ninja"
        exit 1
    }
} else {
    $version = $ninjaCheck -replace "Version:\s*", ""
    Write-Host "  Ninja $version already installed" -ForegroundColor Green
}

# ─────────────────────────────────────────────────────────────
# CLEANUP DOWNLOADS
# ─────────────────────────────────────────────────────────────
if (Test-Path $DOWNLOAD_DIR) {
    Remove-Item -Recurse -Force $DOWNLOAD_DIR
    Write-Host "Cleanup: Download files removed" -ForegroundColor Gray
}

# ─────────────────────────────────────────────────────────────
# VERIFY TOOLCHAIN
# ─────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "=== Verifying Toolchain ===" -ForegroundColor Cyan

$pythonVersion = & $PYTHON_EXE --version 2>&1
Write-Host "  Python: $pythonVersion" -ForegroundColor Green

$conanVersion = & $PYTHON_EXE -m conan --version 2>&1
Write-Host "  Conan: $conanVersion" -ForegroundColor Green

$ninjaVersion = & $PYTHON_EXE -m ninja --version 2>&1
Write-Host "  Ninja: $ninjaVersion" -ForegroundColor Green

# ─────────────────────────────────────────────────────────────
# SETUP COMPLETE
# ─────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "=== Toolchain Setup Complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "Toolchain Location: $TOOLCHAIN_DIR" -ForegroundColor Gray
Write-Host "Python Executable: $PYTHON_EXE" -ForegroundColor Gray
Write-Host ""
Write-Host "Next step: .\build.ps1 setup" -ForegroundColor Cyan
Write-Host ""