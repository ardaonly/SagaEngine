@echo off
setlocal EnableDelayedExpansion

cd /d "%~dp0"

set CONAN_VERSION=2.0.17
set DEFAULT_PROFILE=windows-msvc-14.38
set DEFAULT_PRESET=windows-msvc-14.38
set PROFILE=%DEFAULT_PROFILE%
set PRESET=%DEFAULT_PRESET%
set CONFIG=RelWithDebInfo
set COMMAND=

REM ─────────────────────────────────────────────────────────────
REM PYTHON DETECTION
REM ─────────────────────────────────────────────────────────────
set PYTHON_CMD=
for %%i in (py python python3) do (
    where %%i >nul 2>&1
    if !errorlevel! equ 0 (
        set PYTHON_CMD=%%i
        goto :found_python
    )
)

:found_python
if "%PYTHON_CMD%"=="" (
    echo ERROR: Python not found
    echo Please install Python 3.8+ from https://python.org
    exit /b 1
)

set PIP_CMD=%PYTHON_CMD% -m pip

REM ─────────────────────────────────────────────────────────────
REM PARSE ARGUMENTS
REM ─────────────────────────────────────────────────────────────
:parse_args
if "%~1"=="" goto :end_parse
if /i "%~1"=="setup" set COMMAND=setup& shift & goto :parse_args
if /i "%~1"=="build" set COMMAND=build& shift & goto :parse_args
if /i "%~1"=="test" set COMMAND=test& shift & goto :parse_args
if /i "%~1"=="clean" set COMMAND=clean& shift & goto :parse_args
if /i "%~1"=="package" set COMMAND=package& shift & goto :parse_args
if /i "%~1"=="docker" set COMMAND=docker& shift & goto :parse_args
if /i "%~1"=="help" set COMMAND=help& shift & goto :parse_args
if /i "%~1"=="-Profile" set PROFILE=%~2& set PRESET=%~2& shift & shift & goto :parse_args
if /i "%~1"=="-Config" set CONFIG=%~2& shift & shift & goto :parse_args
if /i "%~1"=="-Help" set COMMAND=help& shift & goto :parse_args
shift
goto :parse_args

:end_parse
if "%COMMAND%"=="" set COMMAND=help

REM ─────────────────────────────────────────────────────────────
REM COMMANDS
REM ─────────────────────────────────────────────────────────────
if /i "%COMMAND%"=="help" goto :cmd_help
if /i "%COMMAND%"=="setup" goto :cmd_setup
if /i "%COMMAND%"=="build" goto :cmd_build
if /i "%COMMAND%"=="test" goto :cmd_test
if /i "%COMMAND%"=="clean" goto :cmd_clean
if /i "%COMMAND%"=="package" goto :cmd_package
if /i "%COMMAND%"=="docker" goto :cmd_docker

:cmd_help
echo SagaEngine Build System
echo.
echo Usage: build.bat [command] [options]
echo.
echo Commands:
echo     setup     Install dependencies and configure
echo     build     Build the project
echo     test      Run tests
echo     clean     Clean build artifacts
echo     package   Create release package
echo     docker    Build in Docker container
echo     help      Show this help
echo.
echo Options:
echo     -Profile PROFILE    Build profile (default: %DEFAULT_PROFILE%)
echo     -Config CONFIG      Build config (default: %CONFIG%)
echo.
echo Python: %PYTHON_CMD%
echo Pip:    %PIP_CMD%
goto :end

:cmd_setup
echo === SagaEngine Setup ===
echo Profile: %PROFILE%
echo Preset:  %PRESET%
echo.

if not exist "conan.lock" (
    echo Generating lockfile...
    conan lock create conanfile.py --profile=profiles/%PROFILE% --lockfile-out=conan.lock --build=missing
)

echo Installing dependencies...
conan install . --profile=profiles/%PROFILE% --lockfile=conan.lock --build=missing --output-folder=Build/%PRESET%

echo Configuring build...
cmake --preset %PRESET%

echo.
echo === Setup Complete ===
echo Run: build.bat build
goto :end

:cmd_build
if not exist "conan.lock" (
    echo ERROR: conan.lock not found. Run 'build.bat setup' first.
    exit /b 1
)

echo === Building SagaEngine ===
echo Preset: %PRESET%
echo Config: %CONFIG%
echo.

set SCCACHE_DIR=%~dp0cache\sccache
if not exist "%SCCACHE_DIR%" mkdir "%SCCACHE_DIR%"
set SCCACHE_DIR=%SCCACHE_DIR%

cmake --build --preset %PRESET% --config %CONFIG%

echo.
echo === Build Complete ===
goto :end

:cmd_test
if not exist "conan.lock" (
    echo ERROR: conan.lock not found. Run 'build.bat setup' first.
    exit /b 1
)

echo === Running Tests ===
echo.

ctest --test-dir Build\%PRESET% --output-on-failure --build-config %CONFIG%

echo.
echo === Tests Complete ===
goto :end

:cmd_clean
echo === Cleaning ===
rmdir /s /q Build 2>nul
rmdir /s /q Bin 2>nul
rmdir /s /q Install 2>nul
rmdir /s /q packages 2>nul
del /q cache\sccache\* 2>nul
echo === Clean Complete ===
goto :end

:cmd_package
if not exist "conan.lock" (
    echo ERROR: conan.lock not found. Run 'build.bat setup' first.
    exit /b 1
)

echo === Creating Package ===

set /p SAGA_VERSION=<BUILD_VERSION 2>nul
if "%SAGA_VERSION%"=="" set SAGA_VERSION=0.1.0

set PACKAGE_NAME=SagaEngine-%SAGA_VERSION%-%PROFILE%
set PACKAGE_DIR=packages\%PACKAGE_NAME%

mkdir "%PACKAGE_DIR%" 2>nul
xcopy /E /Y Bin\%CONFIG%\* "%PACKAGE_DIR%\" 2>nul
copy Build\%PRESET%\sbom.json "%PACKAGE_DIR%\" 2>nul

cd packages
tar -czf "%PACKAGE_NAME%.tar.gz" "%PACKAGE_NAME%"
cd ..

echo Package: packages\%PACKAGE_NAME%.tar.gz
echo === Package Complete ===
goto :end

:cmd_docker
echo === Building in Docker ===

where docker >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker required
    exit /b 1
)

docker build -t sagaengine:build .
docker run --rm -v "%~dp0:C:\saga" sagaengine:build ./build.bat build -Profile %PROFILE%

echo === Docker Build Complete ===
goto :end

:end
endlocal