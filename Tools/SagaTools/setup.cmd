@echo off
rem ─── tools — Windows one-click bootstrap ──────────────────────────────
rem
rem  Double-click this file in Explorer (or run it from cmd / PowerShell)
rem  to build SagaTools and bootstrap forge + prism. Equivalent to:
rem      python3 Tools\SagaTools\setup.py --all
rem
rem  Why a .cmd shim?
rem    * Windows Explorer cannot launch a .py file with a click in every
rem      configuration; a .cmd file always can.
rem    * `pause` keeps the window open after the script finishes so the
rem      user can read the output (most importantly the PATH instructions).
rem    * The Python launcher `py` is the recommended entry point on
rem      Windows; `python3` falls back when `py` is missing.
rem
setlocal
pushd "%~dp0"

rem Find a usable Python: prefer `py` (Windows launcher), fall back to python3 / python.
where py        >nul 2>&1 && (set "PY=py")        || (
where python3   >nul 2>&1 && (set "PY=python3")   || (
where python    >nul 2>&1 && (set "PY=python")    || (
    echo [setup.cmd] ERROR: no Python interpreter found on PATH.
    echo [setup.cmd] Install Python 3.8+ from https://www.python.org/downloads/
    echo [setup.cmd] Make sure to tick "Add python.exe to PATH" in the installer.
    pause
    exit /b 2
)))

echo [setup.cmd] using interpreter: %PY%
%PY% "%~dp0setup.py" --all %*
set "RC=%ERRORLEVEL%"

echo.
if "%RC%"=="0" (
    echo [setup.cmd] Bootstrap finished successfully.
) else (
    echo [setup.cmd] Bootstrap exited with code %RC%.
)
echo.
echo Press any key to close this window.
pause >nul

popd
endlocal
exit /b %RC%
