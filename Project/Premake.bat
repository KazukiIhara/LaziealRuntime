@echo off
setlocal
cd /d "%~dp0"

pushd "..\Dependencies\LaziealGraphicsFramework\Project\Premake"
premake5.exe vs2026
if errorlevel 1 (
    popd
    exit /b 1
)
popd

pushd "Premake"
premake5.exe vs2026
set "result=%errorlevel%"
popd

exit /b %result%
