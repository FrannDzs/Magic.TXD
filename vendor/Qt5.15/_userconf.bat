@echo off

REM Copy those build scripts to a different folder and build Qt there.
REM I recommend you to build QT on a HDD, not your main SSD.

set _BLD_VALID=1

set _TMPOUTPATH=D:/qt_compile/
set _TMPOUTPATH_GIT=D:/qt5_git/

if exist _user.bat (call _user.bat)

mkdir "%_TMPOUTPATH%"
mkdir "%_TMPOUTPATH_GIT%"

if not exist "%_TMPOUTPATH%" (
    echo failed to create temporary output location for building
    set _BLD_VALID=0
)

if not exist "%_TMPOUTPATH_GIT%" (
    echo failed to create temporary Git clone location for building
    set _BLD_VALID=0
)