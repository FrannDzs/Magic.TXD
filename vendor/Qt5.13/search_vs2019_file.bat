set VC_FILEFIND=

echo trying to find %SEARCHNAME%...
@ECHO off

for /r "C:\Program Files (x86)\Microsoft Visual Studio\2019\" %%a in (*%SEARCHNAME%) do (set VC_FILEFIND=%%~dpnxa)

@ECHO on
echo result file: %VC_FILEFIND%