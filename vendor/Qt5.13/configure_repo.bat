call configure.bat -static -static-runtime -debug-and-release -mp ^
    -platform %QTPLAT_PUSH% ^
    -opensource -nomake examples -nomake tests ^
    -opengl desktop -prefix %_TMPOUTPATH%