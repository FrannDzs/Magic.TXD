call "_userconf.bat"

if %_BLD_VALID%==0 (goto do_nothing)

REM rd /S /Q qt5
set _DIRBACK_BLDSCRIPT="%CD%"
cd /D "%_TMPOUTPATH_GIT%"
git clone git://code.qt.io/qt/qt5.git
cd qt5

set _ROOT=%CD%
SET PATH=%_ROOT%\qtbase\bin;%_ROOT%\gnuwin32\bin;%PATH%
SET _ROOT=

git checkout 5.15
perl init-repository ^
    --module-subset=qtbase,qtimageformats
call "%_DIRBACK_BLDSCRIPT%/configure_repo.bat"
nmake
nmake install
cd /D "%_DIRBACK_BLDSCRIPT%"
robocopy "%_TMPOUTPATH%" "%QTOUTNAME%" /MOVE /E

:do_nothing