#!/bin/bash

# General configuration globals.
_DIRBACK_BLDSCRIPT="$(pwd)"
_TMPOUTPATH_GIT="/tmp/_qt5build_temp"

if [ -f "_userconf.sh" ]; then
    # The dot actually allows you to modify variables of the parent.
    # Pretty handy. Even though a lot of meowy people on the internet say it ain't possible.
    . ./_userconf.sh
fi

# Remove anything from a previous run.
rm -r -f "$_TMPOUTPATH_GIT"

# Fetch the source code.
mkdir -p "$_TMPOUTPATH_GIT"
cd "$_TMPOUTPATH_GIT"
git clone git://code.qt.io/qt/qt5.git
cd qt5

# Set-up codebase.
git checkout 5.15
perl init-repository --module-subset=qtbase,qtimageformats
./configure -static -release -platform linux-g++ -opensource \
    -nomake examples -nomake tests -no-opengl -no-mtdev -dbus-linked -fontconfig \
    -qt-zlib -qt-libpng -qt-libjpeg -system-freetype -qt-harfbuzz -qt-sqlite \
    -qt-pcre \
    -prefix "$_TMPOUTPATH_GIT/install"

# Perform the build.
make
make install

cd ../install/lib

# Move all static lib files into their right place.
_LIBTARGET_DIR="$_DIRBACK_BLDSCRIPT/lib/linux"

find . -name "*.a" -exec mkdir -p "$(dirname "$_LIBTARGET_DIR/{}")" \; -exec mv "{}" "$(dirname "$_LIBTARGET_DIR/{}")" \;
cd ..
mv plugins "$_LIBTARGET_DIR"

# Remove the temporary output directory.
rm -r -f "$_TMPOUTPATH_GIT"

# Reinstate the current directory.
cd "$_DIRBACK_BLDSCRIPT"