@echo off

echo Creating installer for modern OS.

setlocal
echo Building 32bit Magic.TXD
set BUILD_CONFIG=Release
set BUILD_PLATFORM=Win32

call "C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/bin/vcvars32.bat"

call "compile_and_installer.bat"
endlocal

setlocal
echo Building 64bit Magic.TXD
set BUILD_CONFIG=Release
set BUILD_PLATFORM=x64

call "C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/bin/amd64/vcvars64.bat"

call "compile_and_installer.bat"
endlocal

echo Generating installer.

"C:/Program Files (x86)/NSIS/makensis.exe" mgtxd_install.nsi

echo Creating installer for legacy OS.

setlocal
echo Building 32bit Magic.TXD (legacy)
set BUILD_CONFIG=Release_legacy
set BUILD_PLATFORM=Win32

call "C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/bin/vcvars32.bat"

call "compile_and_installer.bat"
endlocal

echo Generating installer.

"C:/Program Files (x86)/NSIS/makensis.exe" mgtxd_install_xp.nsi

echo Finished.