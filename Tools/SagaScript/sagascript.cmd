@echo off
setlocal
set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..\..") do set REPOSITORY_ROOT=%%~fI
set /p ENGINE_VERSION=<"%REPOSITORY_ROOT%\VERSION"
if defined SAGA_MANAGED_TOOL_STAGE (
    set STAGE_ROOT=%SAGA_MANAGED_TOOL_STAGE%
) else (
    set STAGE_ROOT=%REPOSITORY_ROOT%\Build\RelWithDebInfo-%ENGINE_VERSION%\ManagedToolRuntime
)
set PRIVATE_HOST=%STAGE_ROOT%\runtime\dotnet\dotnet.exe
set TOOL_DLL=%STAGE_ROOT%\tools\SagaScript\sagascript.dll
if not exist "%PRIVATE_HOST%" goto MissingStage
if not exist "%TOOL_DLL%" goto MissingStage
set DOTNET_ROOT=%STAGE_ROOT%\runtime\dotnet
set DOTNET_ROOT_X64=%DOTNET_ROOT%
set DOTNET_HOST_PATH=%PRIVATE_HOST%
set DOTNET_MULTILEVEL_LOOKUP=0
set DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
"%PRIVATE_HOST%" "%TOOL_DLL%" %*
exit /b %ERRORLEVEL%

:MissingStage
echo sagascript: private managed tool stage is missing; build SagaManagedToolRuntimeStage 1>&2
exit /b 127
