# Magic.TXD

> RenderWare TXD Editor created by DK22Pac and The_GTA

Magic.TXD is a powerful tool designed to edit RenderWare texture files from various commercially released RW games, including the 3D-era Grand Theft Auto games.

It is built upon the magic-rw library and the Qt framework.

**Official Source:** [https://osdn.net/projects/magic-txd/](https://osdn.net/projects/magic-txd/)

## Building and Deploying Magic.TXD

In this guide, we'll walk you through the process of compiling and deploying Magic.TXD from source. Please set aside approximately **two hours** for the initial setup, which involves the following steps:

1. Downloading the source code from the Subversion repository.
2. Building Qt from source.
3. Compiling Eir and other vendors.
4. Compiling Magic.TXD itself.
5. Setting up the output directory.

Once the initial setup is complete, rebuilding Magic.TXD for adding new functionality should be much faster.

### Step 1: Fetching the Source Code

- For Windows: Install [TortoiseSVN](https://tortoisesvn.net/) and use it to download the complete codebase.
- For Linux: Install the **subversion** package using `sudo apt-get install subversion`.

Make sure to have Perl installed as it's needed to run the Qt repository initialization script.

Checkout the source code from the official HTTPS link: [https://svn.osdn.net/svnroot/magic-txd/](https://svn.osdn.net/svnroot/magic-txd/)

### Step 2: Building Qt

Since Magic.TXD links statically to the Qt framework, you need to compile Qt from source. The supported compilers are GCC (with Code::Blocks) and MSBUILD (with Visual Studio), both supporting at least the C++20 standard.

- Navigate to your Magic.TXD SVN working copy.
- Go to the **vendor/Qt5.15** path (latest version at the time of writing).

#### Windows Building

- Run one of the **bldscript_*.bat** files matching your development environment.
- To build only for your target architecture, execute for x86 (32bit) or x64 (64bit).
- You can customize the building location by creating a **_user.bat** file in your Qt vendor directory.

#### Linux Building

- Ensure you have all required system packages installed (see command in the guide for Qt5.15).
- Open a terminal in the Qt vendor directory and execute `./buildscript.sh`.

### Step 3: Compiling Magic.TXD

Open your supported IDE tool and load the build workspace/solution file from the Magic.TXD build directory.

- On Windows, choose a target architecture matching your compiled Qt version.
- On Linux, the target architecture doesn't matter.

Build the **magic-txd project**, and the associated dependencies will be automatically built.

### Step 4: Setting up the Output Directory

The output directory contains the executable files. Copy the following folders into it, similar to a Program Files installation folder on Windows:

1. **resources** folder
2. **languages** folder
3. **data** folder

Run the **Magic.TXD** executable and start editing textures!

## Important Hints

We periodically update the Qt version. When this happens, recompile Qt.

## Used Technology:

- [Compressonator](http://gpuopen.com/gaming-product/compressonator/) by Advanced Micro Devices
- [libimagequant](https://github.com/pornel/pngquant/tree/master/lib) by pornel
- [libjpeg](http://libjpeg.sourceforge.net/) by Tom Lane and the Independent JPEG Group (IJG)
- [libpng](http://www.libpng.org/pub/png/libpng.html)
- [libsquish](https://sourceforge.net/projects/libsquish/)
- [libtiff](http://www.libtiff.org/) by Sam Leffler, Frank Warmerdam, Andrey Kiselev, Mike Welles, and Dwight Kelly.
- [LZO](http://www.oberhumer.com/opensource/lzo/) by Markus F.X.J. Oberhumer
- [PVRTexLib](https://community.imgtec.com/developers/powervr/graphics-sdk/) by Imagination Technologies Limited
- [Qt5](https://www.qt.io/)
- [zlib](https://www.zlib.net/) by Jean-loup Gailly and Mark Adler
