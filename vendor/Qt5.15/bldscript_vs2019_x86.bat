set BITNESS=32bit
set QTOUTNAME="qt515_static_x86_vs2019"

REM Search for compilation script.
set SEARCHNAME="vcvars32.bat"
call search_vs2019_file.bat

REM Set up the compiler environment.
set _OLDCD=%CD%
call "%VC_FILEFIND%"
cd %_OLDCD%

REM Set Qt build target (IMPORTANT).
set QTPLAT_PUSH="win32-msvc2019"
set QTPLAT_MSC_VER=1909

call bldscript.bat