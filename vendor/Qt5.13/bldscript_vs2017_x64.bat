set QTOUTNAME="qt513_static_x64_vs2017"

REM Search for compilation script.
set SEARCHNAME="vcvars64.bat"
call search_vs2017_file.bat

REM Set up the compiler environment.
set _OLDCD=%CD%
call "%VC_FILEFIND%"
cd %_OLDCD%

REM Set Qt build target (IMPORTANT).
set QTPLAT_PUSH="win32-msvc2017"
set QTPLAT_MSC_VER=1909

call bldscript.bat