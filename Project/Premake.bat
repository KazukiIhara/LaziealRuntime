@echo off
setlocal
cd /d "%~dp0"

call "..\Dependencies\LaziealGraphicsFramework\Project\Premake.bat"
if errorlevel 1 exit /b 1

pushd "Premake"
premake5.exe vs2026
set "result=%errorlevel%"
popd

exit /b %result%
