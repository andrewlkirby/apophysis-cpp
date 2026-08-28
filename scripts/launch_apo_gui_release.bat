@echo off
rem Rebuilds the Release apo_gui.exe (and its self-contained deploy\ copy)
rem if anything changed since the last launch, then runs it - so the
rem Desktop shortcut always launches current code instead of whatever was
rem last built by hand. Incremental: if nothing changed, the nmake step
rem below is a fast no-op check, not a full rebuild.
rem
rem Caveat: this calls `nmake` directly (not `cmake --build`) so it stays
rem fast on the common case (editing existing source files) - it does NOT
rem re-run CMake's own configure step, so it won't pick up a source file
rem being ADDED or REMOVED from CMakeLists.txt. After a change like that,
rem run `cmake -S . -B build-release` by hand once.

setlocal
set "REPO=%~dp0.."
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

start "" "%REPO%\build-release\deploy\apo_gui.exe"
endlocal
exit /b 0
