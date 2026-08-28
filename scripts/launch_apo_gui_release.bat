@echo off
rem Launches build-release\deploy\apo_gui.exe, rebuilding it first ONLY if
rem something under src/, cmake/, or the top-level CMakeLists.txt changed
rem since the last build (via is_release_stale.ps1 - a fast, ~0.2s check,
rem not a real nmake/cmake invocation - see that script's own header
rem comment for why: a real no-op `nmake` check alone costs ~2.5s on this
rem project, plus another ~1.5s for vcvars64.bat, which made every single
rem launch feel sluggish even when nothing had changed). So an unchanged
rem codebase launches about as fast as double-clicking apo_gui.exe
rem directly; a real rebuild only pays the multi-second build cost when
rem one is actually needed.
rem
rem Caveat: the real build step below calls `nmake` directly (not
rem `cmake --build`), so it won't pick up a source file being ADDED or
rem REMOVED from CMakeLists.txt. After a change like that, run
rem `cmake -S . -B build-release` by hand once.

setlocal
set "REPO=%~dp0.."
set "DEPLOY_EXE=%REPO%\build-release\deploy\apo_gui.exe"

powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0is_release_stale.ps1" -RepoDir "%REPO%" -DeployExe "%DEPLOY_EXE%"
if not errorlevel 1 (
    start "" "%DEPLOY_EXE%"
    exit /b 0
)

echo Source changed since the last build - rebuilding...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo Could not set up the Visual Studio build environment - is Visual Studio Build Tools installed at the expected path?
    pause
    exit /b 1
)

cd /d "%REPO%\build-release"
nmake apo_gui deploy
if errorlevel 1 (
    echo.
    echo Build failed - see errors above.
    pause
    exit /b 1
)

start "" "%DEPLOY_EXE%"
endlocal
exit /b 0
