# Magic.TXD
> RenderWare TXD editor created by DK22Pac and The_GTA

*Aims to support RW texture files of all commercially released RW games, such as 3D-era GTA.*

*Based on the magic-rw and Qt framework.*

**Official source:** https://osdn.net/projects/magic-txd/

# How to build and deploy Magic.TXD
In this article I want to show you how to compile Magic.TXD and effectively run it from source. We need at least **two hours of your spare time** for initial building setup, which includes

1. downloading the source code from the Subversion repository
2. building Qt from source
3. compiling Eir + other vendors
4. compiling Magic.TXD itself
5. setting up the output directory

Once finished it should be fast to rebuild it, for example if adding new functionality. So let’s begin!

## Step 1: Fetching the Source Code
On Windows you should install [TortoiseSVN](https://tortoisesvn.net/ "TortoiseSVN") to get the entire codebase downloaded (inluding svn:externals). On Linux you require the **subversion apt-get package** from your distribution.

`sudo apt-get install subversion`

`sudp apt-get install perl`

You need Perl to run **the Qt repository initialization script**. On Windows you should install [ActiveState Perl](https://www.activestate.com/activeperl "ActiveState Perl") with perl.exe visible through the **PATH enviroment variable**.

Then SVN checkout from the official HTTPS link: https://svn.osdn.net/svnroot/magic-txd/

## Step 2: Building Qt
Since we **link statically to the Qt framework** you must compile it from source using your main compiler. Our building process supports the following compilers:

1. [GCC](https://gcc.gnu.org/ "GCC") using [Code::Blocks](http://www.codeblocks.org/ "Code::Blocks")
2. [MSBUILD](https://docs.microsoft.com/de-de/visualstudio/msbuild/msbuild "MSBUILD") using [Visual Studio](https://www.visualstudio.com/es/ "Visual Studio")

Your compiler has to at least support the **C++20 standard**.

Now head over to your Magic.TXD SVN working copy. Then browse into the **vendor/Qt5.15** path (latest version at time of writing).

### Windows building
On Windows you need to execute one of the **bldscript_*.bat** files that matches your development environment. If you only want to run Qt then it is recommended to build for **only your target architecture** (x86 for 32bit, x64 for 64bit). Full deployment requires all architectures, but remember that Qt building takes a **long time**.

Before you execute it you must select a **building location** since Qt building can be **heavy on your disk space**. By default the building system assumes you have a **D:/** HDD drive with at least 10GB free space. Otherwise create a **_user.bat** file in your Qt vendor directory with the following content:

`set _TMPOUTPATH=E:/qt_compile/`

`set _TMPOUTPATH_GIT=E:/qt5_git/`

where **E:/** points to your HDD disk drive with enough free space.

![image](https://user-images.githubusercontent.com/76663897/188028110-f87a8f06-9e2b-42de-b2c8-88d1693c458b.png)

Once finished building you should see a **new folder** that contains the static Qt build. Go into it and copy the entire contents of the **lib folder** into the target architecture folder inside of the Magic.TXD Qt vendor directory -> lib folder. Then copy the **plugins** folder into the same target folder aswell.

### Linux building
On Linux you need to make sure that you downloaded all required **system packages**, for example using **apt-get** on Ubuntu. This can be done using the following command (for Qt5.15):

`sudo apt-get install libx11-dev libx11-xcb-dev libfreetype6-dev libice-dev libicu-dev libxcb1-dev libsm-dev libxi-dev libdbus-1-dev libglib2.0-dev libxrender-dev libfontconfig1-dev libxkbcommon-x11-dev libxkbcommon-dev '^libxcb.*-dev' libglu1-mesa-dev libxrender-dev libfontconfig1-dev libxext-dev libxfixes-dev`

After that you have to **open a terminal** in the Qt vendor directory and execute

`./buildscript.sh`

This script should compile the required Qt components and **automatically place them inside the lib folder** (if everything went right which you can never be 100% sure on Linux).

## Step 3: Compiling Magic.TXD
Open one of the **supported IDE tools** and open the associated build workspace/solution file inside of the Magic.TXD build directory. On Windows you should select a target architecture for which you compiled Qt. On Linux it does not matter.

Then just build the **magic-txd project**. Since we are using the workspace/solution file it will automatically build all dependencies that got checked-out alongside Magic.TXD inside of the vendor directory, like magic-rw or FileSystem.

Magic.TXD executable files are located inside of the **output directory**.

## Step 4: Setting up the output directory
Since this folder is the executable path you need to copy all resources into here, just like you would find them inside of the **Program Files installation folder** on Windows.

1. **resources** folder
2. **languages** folder
3. **data** folder

Then just start the **Magic.TXD** executable. Have fun!

## Important Hints
We change Qt version every once in a few months. Please be wary when this happens and recompile Qt in the event.

## Used technology:

[Compressonator](http://gpuopen.com/gaming-product/compressonator/) by Advanced Micro Devices

[libimagequant](https://github.com/pornel/pngquant/tree/master/lib) libimagequant by [pornel](https://github.com/pornel)

[libjpeg](http://libjpeg.sourceforge.net/) by Tom Lane and the Independent JPEG Group (IJG)

[libpng](http://www.libpng.org/pub/png/libpng.html) 

[libsquish](https://sourceforge.net/projects/libsquish/)

[libtiff](http://www.libtiff.org/) by Sam Leffler, Frank Warmerdam, Andrey Kiselev, Mike Welles and Dwight Kelly.

[LZO](http://www.oberhumer.com/opensource/lzo/) by Markus F.X.J. Oberhumer

[PVRTexLib](https://community.imgtec.com/developers/powervr/graphics-sdk/) by Imagination Technologies Limited

[Qt5](https://www.qt.io/)

[zlib](https://www.zlib.net/) by Jean-loup Gailly and Mark Adler
