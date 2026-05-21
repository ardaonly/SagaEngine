@echo off
set SCRIPT_DIR=%~dp0
set REQUIRED_SDK_MAJOR=10
set REQUIRED_TARGET_FRAMEWORK=net10.0

if /I "%~1"=="compile" (
    dotnet --list-sdks 2>nul | findstr /R "^%REQUIRED_SDK_MAJOR%\." >nul
    if errorlevel 1 (
        call :WriteMissingSdkDiagnostics %*
        exit /b 1
    )
)

dotnet run --project "%SCRIPT_DIR%SagaScript.csproj" -- %*
exit /b %ERRORLEVEL%

:WriteMissingSdkDiagnostics
set DIAGNOSTICS_PATH=
set JSON_OUTPUT=0
:ParseArgs
if "%~1"=="" goto EmitDiagnostic
if /I "%~1"=="--diagnostics" (
    set DIAGNOSTICS_PATH=%~2
    shift
    shift
    goto ParseArgs
)
if /I "%~1"=="--json" (
    set JSON_OUTPUT=1
    shift
    goto ParseArgs
)
shift
goto ParseArgs

:EmitDiagnostic
set DIAGNOSTIC_CODE=Script.Toolchain.DotNetSdkMissing
set DIAGNOSTIC_MESSAGE=SagaScript compile requires a .NET 10 SDK for target framework net10.0. Install a .NET 10 SDK and rerun; SagaScript will not downgrade target frameworks automatically.
if not "%DIAGNOSTICS_PATH%"=="" (
    for %%I in ("%DIAGNOSTICS_PATH%") do if not "%%~dpI"=="" mkdir "%%~dpI" 2>nul
    > "%DIAGNOSTICS_PATH%" (
        echo {
        echo   "schemaVersion": 1,
        echo   "toolName": "sagascript",
        echo   "toolchainVersion": "0.0.8-dev",
        echo   "inputs": [],
        echo   "summary": {
        echo     "infoCount": 0,
        echo     "warningCount": 0,
        echo     "errorCount": 1,
        echo     "securityCount": 0,
        echo     "hasBlockingDiagnostics": true
        echo   },
        echo   "diagnostics": [
        echo     {
        echo       "severity": "Error",
        echo       "code": "%DIAGNOSTIC_CODE%",
        echo       "message": "%DIAGNOSTIC_MESSAGE%"
        echo     }
        echo   ]
        echo }
    )
)
if "%JSON_OUTPUT%"=="1" (
    if not "%DIAGNOSTICS_PATH%"=="" (
        type "%DIAGNOSTICS_PATH%"
    ) else (
        echo {"schemaVersion":1,"toolName":"sagascript","toolchainVersion":"0.0.8-dev","inputs":[],"summary":{"infoCount":0,"warningCount":0,"errorCount":1,"securityCount":0,"hasBlockingDiagnostics":true},"diagnostics":[{"severity":"Error","code":"%DIAGNOSTIC_CODE%","message":"%DIAGNOSTIC_MESSAGE%"}]}
    )
) else (
    echo Error %DIAGNOSTIC_CODE%: %DIAGNOSTIC_MESSAGE% 1>&2
)
exit /b 0
