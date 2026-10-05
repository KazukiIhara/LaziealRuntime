@echo off
setlocal
cd /d "%~dp0"

call "..\Dependencies\LaziealGraphicsFramework\Project\PremakeVS2022.bat"
if errorlevel 1 exit /b 1

pushd "Premake"
premake5.exe vs2022
set "result=%errorlevel%"
popd

exit /b %result%
