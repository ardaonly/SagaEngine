$ErrorActionPreference = "Stop"
$SCRIPT_DIR = Split-Path -Parent $PSCommandPath
Set-Location $SCRIPT_DIR

$TOOLCHAIN_DIR = Join-Path $SCRIPT_DIR ".toolchain"
$PYTHON_DIR = Join-Path $TOOLCHAIN_DIR "python"
$VERSION_FILE = Join-Path $TOOLCHAIN_DIR "version.json"
$DOWNLOAD_DIR = Join-Path $TOOLCHAIN_DIR "downloads"
$LOG_FILE = Join-Path $TOOLCHAIN_DIR "bootstrap.log"
$PYTHON_PATH_FILE = Join-Path $TOOLCHAIN_DIR "python_path.txt"

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] [$Level] $Message"
    Add-Content -Path $LOG_FILE -Value $logEntry -Force
    $color = switch ($Level) {
        "SUCCESS" { "Green" }
        "WARN" { "Yellow" }
        "ERROR" { "Red" }
        "START" { "Cyan" }
        default { "White" }
    }
    Write-Host $Message -ForegroundColor $color
}

function Test-FileHash {
    param(
        [string]$FilePath,
        [string]$ExpectedHash
    )
    if ([string]::IsNullOrWhiteSpace($ExpectedHash)) {
        Write-Log "SHA256 verification skipped (no hash provided)" "WARN"
        return $true
    }
    $actualHash = (Get-FileHash -Path $FilePath -Algorithm SHA256).Hash.ToLower()
    $expectedHash = $ExpectedHash.ToLower()
    if ($actualHash -eq $expectedHash) {
        Write-Log "SHA256 verified: $FilePath" "SUCCESS"
        return $true
    }
    Write-Log "SHA256 MISMATCH!" "ERROR"
    Write-Log "  Expected: $expectedHash" "ERROR"
    Write-Log "  Actual:   $actualHash" "ERROR"
    return $false
}

function Get-PythonZipName {
    param([string]$Version)
    $parts = $Version.Split(".")
    return "python$($parts[0])$($parts[1]).zip"
}

function Verify-PythonCore {
    param([string]$PythonExe)

    Write-Log "Verifying Python core..." "INFO"

    $testOutput = & $PythonExe -c "print('OK')" 2>&1
    if ($LASTEXITCODE -ne 0 -or ($testOutput | Out-String).Trim() -ne "OK") {
        Write-Log "Python basic test failed!" "ERROR"
        return $false
    }

    $encodingsTest = & $PythonExe -c "import encodings; print('OK')" 2>&1
    if ($LASTEXITCODE -ne 0 -or ($encodingsTest | Out-String).Trim() -ne "OK") {
        Write-Log "encodings module test failed!" "ERROR"
        return $false
    }

    return $true
}

function Verify-Pip {
    param([string]$PythonExe)
    
    try {
        $pipTest = & $PythonExe -m pip --version 2>&1
        return ($LASTEXITCODE -eq 0)
    } catch {
        return $false
    }
}

function Bootstrap-Pip {
    param([string]$PythonExe)
    Write-Log "Bootstrapping pip..." "INFO"
    New-Item -ItemType Directory -Force -Path $DOWNLOAD_DIR | Out-Null
    $getPipPath = Join-Path $DOWNLOAD_DIR "get-pip.py"
    $getPipUrl = "https://bootstrap.pypa.io/get-pip.py"
    
    if (-not (Test-Path $getPipPath)) {
        Write-Log "Downloading get-pip.py..." "INFO"
        try {
            Invoke-WebRequest -Uri $getPipUrl -OutFile $getPipPath -UseBasicParsing
            Write-Log "get-pip.py downloaded" "SUCCESS"
        } catch {
            Write-Log "get-pip.py download failed: $_" "ERROR"
            return $false
        }
    }
    
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $pipOutput = & $PythonExe $getPipPath 2>&1
    $ErrorActionPreference = $oldPreference

    if ($LASTEXITCODE -ne 0) {
        Write-Log "pip installation failed!" "ERROR"
        Write-Log ($pipOutput | Out-String) "ERROR"
        return $false
    }
    if (-not (Verify-Pip -PythonExe $PythonExe)) {
        Write-Log "pip verification failed!" "ERROR"
        return $false
    }
    Write-Log "pip installed successfully" "SUCCESS"
    return $true
}

function Install-Python {
    param(
        [string]$Version,
        [string]$Url,
        [string]$Sha256
    )
    Write-Log "Installing Python $Version (embeddable)..." "INFO"
    if (Test-Path $PYTHON_DIR) {
        Write-Log "Removing existing Python installation..." "WARN"
        Remove-Item -Recurse -Force $PYTHON_DIR -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Force -Path $PYTHON_DIR | Out-Null
    New-Item -ItemType Directory -Force -Path $DOWNLOAD_DIR | Out-Null
    $pythonZip = Join-Path $DOWNLOAD_DIR "python-$Version-embed-amd64.zip"
    if (-not (Test-Path $pythonZip)) {
        Write-Log "Downloading: $Url" "INFO"
        Invoke-WebRequest -Uri $Url -OutFile $pythonZip -UseBasicParsing
        Write-Log "Download complete: $pythonZip" "SUCCESS"
    }
    if (-not (Test-FileHash -FilePath $pythonZip -ExpectedHash $Sha256)) {
        Write-Log "Python download corrupted, removing..." "ERROR"
        Remove-Item $pythonZip -Force -ErrorAction SilentlyContinue
        exit 1
    }
    Write-Log "Extracting to: $PYTHON_DIR" "INFO"
    Expand-Archive -Path $pythonZip -DestinationPath $PYTHON_DIR -Force
    Write-Log "Configuring Python path..." "INFO"
    $zipFile = Get-ChildItem $PYTHON_DIR -Filter "python*.zip" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $zipFile) {
        Write-Log "python*.zip not found in $PYTHON_DIR" "ERROR"
        exit 1
    }
    $pythonMinor = $Version.Split('.')[0] + $Version.Split('.')[1]
    $pthFile = Join-Path $PYTHON_DIR "python$pythonMinor._pth"
    $pthContent = @"
$($zipFile.Name)
python310.zip
DLLs\
Lib\
Lib\site-packages
Scripts\
.
import site
"@
    Set-Content -Path $pthFile -Value $pthContent -Force
    Write-Log "_pth configured: $($zipFile.Name)" "SUCCESS"
    $sitePackages = Join-Path $PYTHON_DIR "Lib\site-packages"
    New-Item -ItemType Directory -Force -Path $sitePackages | Out-Null
    Write-Log "site-packages created" "SUCCESS"
    $Version | Set-Content (Join-Path $PYTHON_DIR ".version") -Force
    Write-Log "Python extracted successfully" "SUCCESS"
}

function Install-Package {
    param(
        [string]$PythonExe,
        [string]$PackageName,
        [string]$Version
    )
    Write-Log "Installing $PackageName $Version..." "INFO"
    
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $result = & $PythonExe -m pip install --quiet "$PackageName==$Version" 2>&1
    $ErrorActionPreference = $oldPreference

    if ($LASTEXITCODE -eq 0) {
        Write-Log "$PackageName installed" "SUCCESS"
        return $true
    }
    Write-Log "$PackageName installation failed!" "ERROR"
    Write-Log ($result | Out-String) "ERROR"
    return $false
}

function Ensure-Package {
    param(
        [string]$PythonExe,
        [string]$PackageName,
        [string]$Version
    )
    
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"

    $show = & $PythonExe -m pip show $PackageName 2>&1

    $ErrorActionPreference = $oldPreference

    if ($LASTEXITCODE -eq 0 -and ($show | Select-String -Pattern "^Version:\s*$([regex]::Escape($Version))$")) {
        Write-Log "$PackageName $Version already installed" "SUCCESS"
        return $true
    }
    return (Install-Package -PythonExe $PythonExe -PackageName $PackageName -Version $Version)
}

Write-Log "=== SagaEngine Toolchain Bootstrap ===" "START"
Write-Log "Toolchain Directory: $TOOLCHAIN_DIR" "INFO"

if (-not (Test-Path $VERSION_FILE)) {
    Write-Log "version.json not found: $VERSION_FILE" "ERROR"
    exit 1
}

$versionConfig = Get-Content $VERSION_FILE | ConvertFrom-Json
$PYTHON_VERSION = $versionConfig.python.version
$CONAN_VERSION = $versionConfig.conan.version
$NINJA_VERSION = $versionConfig.ninja.version

$PYTHON_EXE = Join-Path $PYTHON_DIR "python.exe"

$toolchainValid = $false
if (Test-Path $PYTHON_EXE) {
    $versionFile = Join-Path $PYTHON_DIR ".version"
    if (Test-Path $versionFile) {
        $installedVersion = (Get-Content $versionFile | Select-Object -First 1).Trim()
        if ($installedVersion -eq $PYTHON_VERSION -and (Verify-PythonCore -PythonExe $PYTHON_EXE)) {
            Write-Log "Toolchain already installed (Python $installedVersion)" "SUCCESS"
            $toolchainValid = $true
        }
    }
}

if (-not (Test-Path $PYTHON_EXE) -or -not (Verify-PythonCore -PythonExe $PYTHON_EXE)) {
    Install-Python -Version $PYTHON_VERSION -Url $versionConfig.python.url -Sha256 $versionConfig.python.sha256
    if (-not (Verify-PythonCore -PythonExe $PYTHON_EXE)) {
        exit 1
    }
}

if (-not (Verify-Pip -PythonExe $PYTHON_EXE)) {
    if (-not (Bootstrap-Pip -PythonExe $PYTHON_EXE)) {
        exit 1
    }
}

if (-not (Verify-Pip -PythonExe $PYTHON_EXE)) {
    Write-Log "pip verification failed after bootstrap!" "ERROR"
    exit 1
}

if (-not (Ensure-Package -PythonExe $PYTHON_EXE -PackageName "conan" -Version $CONAN_VERSION)) {
    exit 1
}

if (-not (Ensure-Package -PythonExe $PYTHON_EXE -PackageName "ninja" -Version $NINJA_VERSION)) {
    exit 1
}

Write-Log "Saving Python path to: $PYTHON_PATH_FILE" "INFO"
$PYTHON_EXE | Set-Content $PYTHON_PATH_FILE -Force

Write-Log "" "INFO"
Write-Log "=== Verifying Toolchain ===" "INFO"

$pythonVersion = & $PYTHON_EXE --version 2>&1
Write-Log "Python: $pythonVersion" "SUCCESS"

$CONAN_EXE = Join-Path $PYTHON_DIR "Scripts\conan.exe"
$conanVersion = & $CONAN_EXE --version 2>&1
Write-Log "Conan: $conanVersion" "SUCCESS"

$NINJA_EXE = Join-Path $PYTHON_DIR "Scripts\ninja.exe"
$ninjaVersion = & $NINJA_EXE --version 2>&1
Write-Log "Ninja: $ninjaVersion" "SUCCESS"

if (Test-Path $DOWNLOAD_DIR) {
    Remove-Item -Recurse -Force $DOWNLOAD_DIR -ErrorAction SilentlyContinue
    Write-Log "Download files cleaned up" "INFO"
}

Write-Log "" "INFO"
Write-Log "=== Toolchain Setup Complete ===" "SUCCESS"
Write-Log "" "INFO"
Write-Log "Python: $PYTHON_EXE" "INFO"
Write-Log "Path File: $PYTHON_PATH_FILE" "INFO"
Write-Log "Log: $LOG_FILE" "INFO"
Write-Log "" "INFO"
Write-Log "Next: .\build.ps1 setup" "INFO"