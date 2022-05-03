/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        rwconf.h
*  PURPOSE:
*       This file is the central configuration file of rwtools.
*       Here you can turn on/off features at compiletime and tweak trivial things.
*       Follow the instructions in the comments.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/



#ifndef _RENDERWARE_CONFIGURATION_
#define _RENDERWARE_CONFIGURATION_

// Define these macros to configure the inclusion of standard native textures.
// It is safe to remove any native textures because the engine is structured in plugin format.
#define RWLIB_INCLUDE_NATIVETEX_D3D8
#define RWLIB_INCLUDE_NATIVETEX_D3D9
#define RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2
#define RWLIB_INCLUDE_NATIVETEX_PSP
#define RWLIB_INCLUDE_NATIVETEX_XBOX
#define RWLIB_INCLUDE_NATIVETEX_GAMECUBE
#define RWLIB_INCLUDE_NATIVETEX_S3TC_MOBILE
#define RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
#define RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE
#define RWLIB_INCLUDE_NATIVETEX_ATC_MOBILE

// Define this macro if you have a "xdk/" folder in your vendors with all the
// XBOX Development Kit headers. This will use correct and optimized swizzling
// algorithms. Having this macro defined is recommended.
#define _USE_XBOX_SDK_

// Define this macro if you want to include imaging support in your rwlib compilation.
// This will allow you to store texel data of textures in popular picture formats, such as TGA.
#define RWLIB_INCLUDE_IMAGING

// Define those if you want to ship with specific imaging format support.
// If you undefine those, you can save quite some library space.
#define RWLIB_INCLUDE_TGA_IMAGING
#define RWLIB_INCLUDE_BMP_IMAGING
#define RWLIB_INCLUDE_PNG_IMAGING
#define RWLIB_INCLUDE_JPEG_IMAGING
#define RWLIB_INCLUDE_GIF_IMAGING
#define RWLIB_INCLUDE_TIFF_IMAGING

// Define those if you want to include support for certain native image formats.
// Native image formats are more powerful than generic imaging formats but are more complicated.
#define RWLIB_INCLUDE_DDS_NATIVEIMG
#define RWLIB_INCLUDE_PVR_NATIVEIMG

// Define this if you want to enable the "libimagequant" library for palletizing
// image data. This is highly recommended, as that library produces high
// quality color-mapped images.
#define RWLIB_INCLUDE_LIBIMAGEQUANT

// Define this if you want to use framework entry points for RenderWare in your project.
// Those can be used to create managed RenderWare applications.
#define RWLIB_INCLUDE_FRAMEWORK_ENTRYPOINTS

#endif //_RENDERWARE_CONFIGURATION_