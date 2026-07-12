@echo off

pushd ..
Scripts\Premake\Windows\premake5.exe --file=Build.lua vs2026
popd

pause