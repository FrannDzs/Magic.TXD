/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/natimage.pvr.cpp
*  PURPOSE:     PVR native image format implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

// PowerVR file format support for RenderWare, because mobile games tend to use it.
// This file format started out as an inspiration over DDS while adding support for
// many Imagination Technologies formats (PVR 2bpp, PVR 4bpp, ETC, ....).

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_PVR_NATIVEIMG

#include "natimage.hxx"

#include "txdread.pvr.hxx"
#include "txdread.d3d.hxx"
#include "txdread.miputil.hxx"
#include "txdread.d3d8.hxx"
#include "txdread.d3d9.hxx"

#include "txdread.d3d8.layerpipe.hxx"
#include "txdread.d3d9.layerpipe.hxx"

#include "streamutil.hxx"

#include "pixelutil.hxx"

namespace rw
{

// A trend of Linux-invented file formats is that they come in dynamic endianness.
// Instead of standardizing the endianness to a specific value they allow you to write it
// in whatever way and the guys writing the parser gotta make a smart enough implementation
// to detect any case.
// First they refuse to ship static libraries and then they also suck at file formats?

// I gotta give ImgTec credit for the pretty thorough documentation of their formats.

// We implement the legacy formats first.
enum class ePVRLegacyPixelFormat
{
    ARGB_4444,
    ARGB_1555,
    RGB_565,
    RGB_555,
    RGB_888,
    ARGB_8888,
    ARGB_8332,
    I8,
    AI88,
    MONOCHROME,
    V_Y1_U_Y0,      // 2x2 block format, 8bit depth
    Y1_V_Y0_U,      // same as above, but reordered.
    PVRTC2,
    PVRTC4,

    // Secondary formats, appears to be clones?
    ARGB_4444_SEC = 0x10,
    ARGB_1555_SEC,
    ARGB_8888_SEC,
    RGB_565_SEC,
    RGB_555_SEC,
    RGB_888_SEC,
    I8_SEC,
    AI88_SEC,
    PVRTC2_SEC,
    PVRTC4_SEC,
    BGRA_8888,  // I guess some lobbyist wanted easy convertability to PVR from DDS?

    // Special types.
    DXT1 = 0x20,
    DXT2,       // it is nice to see that PVR decided to support this format!
    DXT3,
    DXT4,       // this one aswell.
    DXT5,
    RGB332,
    AL_44,
    LVU_655,
    XLVU_8888,
    QWVU_8888,
    ABGR_2101010,
    ARGB_2101010,
    AWVU_2101010,
    GR_1616,
    VU_1616,
    ABGR_16161616,
    R_16F,
    GR_1616F,
    ABGR_16161616F,
    R_32F,
    GR_3232F,
    ABGR_32323232F,
    ETC,                // 4x4 block format, 4bit depth

    // I guess late additions.
    A8 = 0x40,
    VU_88,
    L16,
    L8,
    AL_88,
    UYVY,               // 2x2 block format, 8bit depth (V_Y1_U_Y0, another reordering)
    YUY2                // 2x2 block format, 8bit depth
};  // 55

// This is quite a gamble that I take, especially since PVR is just an advanced inspiration from DDS anyway.
inline uint32 getPVRNativeImageRowAlignment( void )
{
    // Just like DDS.
    return 1;
}

inline rasterRowSize getPVRNativeImageRasterDataRowSize( uint32 surfWidth, uint32 depth )
{
    return getRasterDataRowSize( surfWidth, depth, getPVRNativeImageRowAlignment() );
}

// We need to classify raster formats in a way to process them properly.
enum class ePVRLegacyPixelFormatType
{
    UNKNOWN,
    RGBA,
    LUMINANCE,
    COMPRESSED
};

inline ePVRLegacyPixelFormatType getPVRLegacyPixelFormatType( ePVRLegacyPixelFormat format )
{
    switch( format )
    {
    case ePVRLegacyPixelFormat::ARGB_4444:
    case ePVRLegacyPixelFormat::ARGB_1555:
    case ePVRLegacyPixelFormat::RGB_565:
    case ePVRLegacyPixelFormat::RGB_555:
    case ePVRLegacyPixelFormat::RGB_888:
    case ePVRLegacyPixelFormat::ARGB_8888:
    case ePVRLegacyPixelFormat::ARGB_8332:
    case ePVRLegacyPixelFormat::ARGB_4444_SEC:
    case ePVRLegacyPixelFormat::ARGB_1555_SEC:
    case ePVRLegacyPixelFormat::ARGB_8888_SEC:
    case ePVRLegacyPixelFormat::RGB_565_SEC:
    case ePVRLegacyPixelFormat::RGB_555_SEC:
    case ePVRLegacyPixelFormat::RGB_888_SEC:
    case ePVRLegacyPixelFormat::BGRA_8888:
    case ePVRLegacyPixelFormat::RGB332:
    case ePVRLegacyPixelFormat::ABGR_2101010:
    case ePVRLegacyPixelFormat::ARGB_2101010:
    case ePVRLegacyPixelFormat::GR_1616:
    case ePVRLegacyPixelFormat::ABGR_16161616:
    case ePVRLegacyPixelFormat::R_16F:
    case ePVRLegacyPixelFormat::GR_1616F:
    case ePVRLegacyPixelFormat::ABGR_16161616F:
    case ePVRLegacyPixelFormat::R_32F:
    case ePVRLegacyPixelFormat::GR_3232F:
    case ePVRLegacyPixelFormat::ABGR_32323232F:
        // Those formats are RGBA samples.
        return ePVRLegacyPixelFormatType::RGBA;
    case ePVRLegacyPixelFormat::I8:
    case ePVRLegacyPixelFormat::AI88:
    case ePVRLegacyPixelFormat::MONOCHROME:
    case ePVRLegacyPixelFormat::I8_SEC:
    case ePVRLegacyPixelFormat::AI88_SEC:
    case ePVRLegacyPixelFormat::AL_44:
    case ePVRLegacyPixelFormat::L16:
    case ePVRLegacyPixelFormat::L8:
    case ePVRLegacyPixelFormat::AL_88:
        return ePVRLegacyPixelFormatType::LUMINANCE;
    case ePVRLegacyPixelFormat::V_Y1_U_Y0:
    case ePVRLegacyPixelFormat::Y1_V_Y0_U:
    case ePVRLegacyPixelFormat::PVRTC2:
    case ePVRLegacyPixelFormat::PVRTC4:
    case ePVRLegacyPixelFormat::PVRTC2_SEC:
    case ePVRLegacyPixelFormat::PVRTC4_SEC:
    case ePVRLegacyPixelFormat::DXT1:
    case ePVRLegacyPixelFormat::DXT2:
    case ePVRLegacyPixelFormat::DXT3:
    case ePVRLegacyPixelFormat::DXT4:
    case ePVRLegacyPixelFormat::DXT5:
    case ePVRLegacyPixelFormat::ETC:
    case ePVRLegacyPixelFormat::UYVY:
    case ePVRLegacyPixelFormat::YUY2:
        return ePVRLegacyPixelFormatType::COMPRESSED;
    default:
        break;
    }

    // We do not care about anything else.
    return ePVRLegacyPixelFormatType::UNKNOWN;
}

// Returns whether the given format supports an alpha channel.
inline bool doesPVRLegacyFormatHaveAlphaChannel( ePVRLegacyPixelFormat pixelFormat )
{
    switch( pixelFormat )
    {
    case ePVRLegacyPixelFormat::ARGB_4444:
    case ePVRLegacyPixelFormat::ARGB_4444_SEC:
    case ePVRLegacyPixelFormat::ARGB_1555:
    case ePVRLegacyPixelFormat::ARGB_1555_SEC:
    case ePVRLegacyPixelFormat::ARGB_8888:
    case ePVRLegacyPixelFormat::ARGB_8888_SEC:
    case ePVRLegacyPixelFormat::ARGB_8332:
    case ePVRLegacyPixelFormat::AI88:
    case ePVRLegacyPixelFormat::AI88_SEC:
    case ePVRLegacyPixelFormat::PVRTC2:
    case ePVRLegacyPixelFormat::PVRTC2_SEC:
    case ePVRLegacyPixelFormat::PVRTC4:
    case ePVRLegacyPixelFormat::PVRTC4_SEC:
    case ePVRLegacyPixelFormat::BGRA_8888:
    case ePVRLegacyPixelFormat::AL_44:
    case ePVRLegacyPixelFormat::ABGR_2101010:
    case ePVRLegacyPixelFormat::ARGB_2101010:
    case ePVRLegacyPixelFormat::AWVU_2101010:
    case ePVRLegacyPixelFormat::ABGR_16161616:
    case ePVRLegacyPixelFormat::ABGR_16161616F:
    case ePVRLegacyPixelFormat::ABGR_32323232F:
    case ePVRLegacyPixelFormat::A8:
    case ePVRLegacyPixelFormat::AL_88:
        return true;
    default:
        break;
    }

    return false;
}

// We can read and write samples for RGBA and LUMINANCE based samples.
struct pvrColorDispatcher
{
    AINLINE pvrColorDispatcher(
        ePVRLegacyPixelFormat pixelFormat, ePVRLegacyPixelFormatType formatType,
        bool isLittleEndian
    )
    {
        this->pixelFormat = pixelFormat;
        this->colorModel = formatType;
        this->isLittleEndian = isLittleEndian;
    }

private:
    template <template <typename numberType> class endianness, typename colorNumberType>
    static AINLINE bool browsetexelrgba(
        const constRasterRow& srcTexels, uint32 colorIndex, ePVRLegacyPixelFormat pixelFormat,
        colorNumberType& redOut, colorNumberType& greenOut, colorNumberType& blueOut, colorNumberType& alphaOut
    )
    {
        if ( pixelFormat == ePVRLegacyPixelFormat::ARGB_4444 ||
             pixelFormat == ePVRLegacyPixelFormat::ARGB_4444_SEC )
        {
            struct color
            {
                unsigned short alpha : 4;
                unsigned short blue : 4;
                unsigned short green : 4;
                unsigned short red : 4;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolor( item.alpha, 15, alphaOut );
            destscalecolor( item.red, 15, redOut );
            destscalecolor( item.green, 15, greenOut );
            destscalecolor( item.blue, 15, blueOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ARGB_1555 ||
                  pixelFormat == ePVRLegacyPixelFormat::ARGB_1555_SEC )
        {
            struct color
            {
                unsigned short alpha : 1;
                unsigned short blue : 5;
                unsigned short green : 5;
                unsigned short red : 5;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            solve1bitalpha( item.alpha, alphaOut );
            destscalecolor( item.red, 31, redOut );
            destscalecolor( item.green, 31, greenOut );
            destscalecolor( item.blue, 31, blueOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::RGB_565 ||
                  pixelFormat == ePVRLegacyPixelFormat::RGB_565_SEC )
        {
            struct color
            {
                unsigned short blue : 5;
                unsigned short green : 6;
                unsigned short red : 5;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolor( item.red, 31, redOut );
            destscalecolor( item.green, 63, greenOut );
            destscalecolor( item.blue, 31, blueOut );
            alphaOut = color_defaults <decltype( alphaOut )>::one;
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::RGB_555 ||
                  pixelFormat == ePVRLegacyPixelFormat::RGB_555_SEC )
        {
            struct color
            {
                unsigned short unused : 1;
                unsigned short blue : 5;
                unsigned short green : 5;
                unsigned short red : 5;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolor( item.red, 31, redOut );
            destscalecolor( item.green, 31, greenOut );
            destscalecolor( item.blue, 31, blueOut );
            alphaOut = color_defaults <decltype( alphaOut )>::one;
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::RGB_888 ||
                  pixelFormat == ePVRLegacyPixelFormat::RGB_888_SEC )
        {
            struct color
            {
                unsigned char blue;
                unsigned char green;
                unsigned char red;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.red, redOut );
            destscalecolorn( item.green, greenOut );
            destscalecolorn( item.blue, blueOut );
            alphaOut = color_defaults <decltype( alphaOut )>::one;
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ARGB_8888 ||
                  pixelFormat == ePVRLegacyPixelFormat::ARGB_8888_SEC )
        {
            struct color
            {
                unsigned char red;
                unsigned char green;
                unsigned char blue;
                unsigned char alpha;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.alpha, alphaOut );
            destscalecolorn( item.red, redOut );
            destscalecolorn( item.green, greenOut );
            destscalecolorn( item.blue, blueOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ARGB_8332 )
        {
            struct color
            {
                unsigned short alpha : 8;
                unsigned short red : 3;
                unsigned short green : 3;
                unsigned short blue : 2;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolor( item.alpha, 255, alphaOut );
            destscalecolor( item.red, 7, redOut );
            destscalecolor( item.green, 7, greenOut );
            destscalecolor( item.blue, 3, blueOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::BGRA_8888 )
        {
            struct color
            {
                unsigned char blue;
                unsigned char green;
                unsigned char red;
                unsigned char alpha;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.blue, blueOut );
            destscalecolorn( item.green, greenOut );
            destscalecolorn( item.red, redOut );
            destscalecolorn( item.alpha, alphaOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::RGB332 )
        {
            struct color
            {
                unsigned char red : 3;
                unsigned char green : 3;
                unsigned char blue : 2;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolor( item.red, 7, redOut );
            destscalecolor( item.green, 7, greenOut );
            destscalecolor( item.blue, 3, blueOut );
            alphaOut = color_defaults <decltype( alphaOut )>::one;
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ABGR_2101010 )
        {
            struct color
            {
                uint32 alpha : 2;
                uint32 red : 10;
                uint32 green : 10;
                uint32 blue : 10;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolor( item.alpha, 3, alphaOut );
            destscalecolor( item.red, 1023, redOut );
            destscalecolor( item.green, 1023, greenOut );
            destscalecolor( item.blue, 1023, blueOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ARGB_2101010 )
        {
            struct color
            {
                uint32 alpha : 2;
                uint32 blue : 10;
                uint32 green : 10;
                uint32 red : 10;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolor( item.alpha, 3, alphaOut );
            destscalecolor( item.red, 1023, redOut );
            destscalecolor( item.green, 1023, greenOut );
            destscalecolor( item.blue, 1023, blueOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::GR_1616 )
        {
            struct color
            {
                unsigned short green;
                unsigned short red;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.green, greenOut );
            destscalecolorn( item.red, redOut );
            blueOut = color_defaults <decltype( blueOut )>::zero;
            alphaOut = color_defaults <decltype( alphaOut )>::one;
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ABGR_16161616 )
        {
            struct color
            {
                unsigned short alpha;
                unsigned short blue;
                unsigned short green;
                unsigned short red;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.alpha, alphaOut );
            destscalecolorn( item.blue, blueOut );
            destscalecolorn( item.green, greenOut );
            destscalecolorn( item.blue, blueOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::R_32F )
        {
            struct color
            {
                float red;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.red, redOut );
            blueOut = color_defaults <decltype( blueOut )>::zero;
            greenOut = color_defaults <decltype( greenOut )>::zero;
            alphaOut = color_defaults <decltype( alphaOut )>::one;
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::GR_3232F )
        {
            struct color
            {
                float green;
                float red;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.green, greenOut );
            destscalecolorn( item.red, redOut );
            blueOut = color_defaults <decltype( blueOut )>::zero;
            alphaOut = color_defaults <decltype( alphaOut )>::one;
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ABGR_32323232F )
        {
            struct color
            {
                float alpha;
                float blue;
                float green;
                float red;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.alpha, alphaOut );
            destscalecolorn( item.blue, blueOut );
            destscalecolorn( item.green, greenOut );
            destscalecolorn( item.red, redOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::A8 )
        {
            struct color
            {
                unsigned char alpha;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.alpha, alphaOut );
            redOut = color_defaults <decltype( redOut )>::zero;
            greenOut = color_defaults <decltype( greenOut )>::zero;
            blueOut = color_defaults <decltype( blueOut )>::zero;
            return true;
        }

        return false;
    }

    template <template <typename numberType> class endianness, typename colorNumberType>
    static AINLINE bool puttexelrgba(
        rasterRow& dstTexels, uint32 colorIndex, ePVRLegacyPixelFormat pixelFormat,
        colorNumberType red, colorNumberType green, colorNumberType blue, colorNumberType alpha
    )
    {
        if ( pixelFormat == ePVRLegacyPixelFormat::ARGB_4444 ||
             pixelFormat == ePVRLegacyPixelFormat::ARGB_4444_SEC )
        {
            struct color
            {
                unsigned short alpha : 4;
                unsigned short blue : 4;
                unsigned short green : 4;
                unsigned short red : 4;
            };

            color item;
            item.alpha      = putscalecolor <uint8> ( alpha, 15 );
            item.red        = putscalecolor <uint8> ( red, 15 );
            item.green      = putscalecolor <uint8> ( green, 15 );
            item.blue       = putscalecolor <uint8> ( blue, 15 );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ARGB_1555 ||
                  pixelFormat == ePVRLegacyPixelFormat::ARGB_1555_SEC )
        {
            struct color
            {
                unsigned short alpha : 1;
                unsigned short blue : 5;
                unsigned short green : 5;
                unsigned short red : 5;
            };

            color item;
            item.alpha      = resolve1bitalpha( alpha );
            item.red        = putscalecolor <uint8> ( red, 31 );
            item.green      = putscalecolor <uint8> ( green, 31 );
            item.blue       = putscalecolor <uint8> ( blue, 31 );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::RGB_565 ||
                  pixelFormat == ePVRLegacyPixelFormat::RGB_565_SEC )
        {
            struct color
            {
                unsigned short blue : 5;
                unsigned short green : 6;
                unsigned short red : 5;
            };

            color item;
            item.red        = putscalecolor <uint8> ( red, 31 );
            item.green      = putscalecolor <uint8> ( green, 63 );
            item.blue       = putscalecolor <uint8> ( blue, 31 );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::RGB_555 ||
                  pixelFormat == ePVRLegacyPixelFormat::RGB_555_SEC )
        {
            struct color
            {
                unsigned short unused : 1;
                unsigned short blue : 5;
                unsigned short green : 5;
                unsigned short red : 5;
            };

            color item;
            item.red        = putscalecolor <uint8> ( red, 31 );
            item.green      = putscalecolor <uint8> ( green, 31 );
            item.blue       = putscalecolor <uint8> ( blue, 31 );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::RGB_888 ||
                  pixelFormat == ePVRLegacyPixelFormat::RGB_888_SEC )
        {
            struct color
            {
                unsigned char red;
                unsigned char green;
                unsigned char blue;
            };

            color item;
            destscalecolorn( red, item.red );
            destscalecolorn( green, item.green );
            destscalecolorn( blue, item.blue );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ARGB_8888 ||
                  pixelFormat == ePVRLegacyPixelFormat::ARGB_8888_SEC )
        {
            struct color
            {
                unsigned char red;
                unsigned char green;
                unsigned char blue;
                unsigned char alpha;
            };

            color item;
            destscalecolorn( alpha, item.alpha );
            destscalecolorn( red, item.red );
            destscalecolorn( green, item.green );
            destscalecolorn( blue, item.blue );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ARGB_8332 )
        {
            struct color
            {
                unsigned short alpha : 8;
                unsigned short red : 3;
                unsigned short green : 3;
                unsigned short blue : 2;
            };

            color item;
            item.alpha      = putscalecolor <uint8> ( alpha, 255 );
            item.red        = putscalecolor <uint8> ( red, 7 );
            item.green      = putscalecolor <uint8> ( green, 7 );
            item.blue       = putscalecolor <uint8> ( blue, 3 );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::BGRA_8888 )
        {
            struct color
            {
                unsigned char blue;
                unsigned char green;
                unsigned char red;
                unsigned char alpha;
            };

            color item;
            destscalecolorn( blue, item.blue );
            destscalecolorn( green, item.green );
            destscalecolorn( red, item.red );
            destscalecolorn( alpha, item.alpha );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::RGB332 )
        {
            struct color
            {
                unsigned char red : 3;
                unsigned char green : 3;
                unsigned char blue : 2;
            };

            color item;
            item.red        = putscalecolor <uint8> ( red, 7 );
            item.green      = putscalecolor <uint8> ( green, 7 );
            item.blue       = putscalecolor <uint8> ( blue, 3 );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ABGR_2101010 )
        {
            struct color
            {
                uint32 alpha : 2;
                uint32 blue : 10;
                uint32 green : 10;
                uint32 red : 10;
            };

            color item;
            item.alpha      = putscalecolor <uint8> ( alpha, 3 );
            item.red        = putscalecolor <uint16> ( red, 1023 );
            item.green      = putscalecolor <uint16> ( green, 1023 );
            item.blue       = putscalecolor <uint16> ( blue, 1023 );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ARGB_2101010 )
        {
            struct color
            {
                uint32 alpha : 2;
                uint32 red : 10;
                uint32 green : 10;
                uint32 blue : 10;
            };

            color item;
            item.alpha      = putscalecolor <uint8> ( alpha, 3 );
            item.red        = putscalecolor <uint16> ( red, 1023 );
            item.green      = putscalecolor <uint16> ( green, 1023 );
            item.blue       = putscalecolor <uint16> ( blue, 1023 );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::GR_1616 )
        {
            struct color
            {
                unsigned short green;
                unsigned short red;
            };

            color item;
            destscalecolorn( green, item.green );
            destscalecolorn( red, item.red );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ABGR_16161616 )
        {
            struct color
            {
                unsigned short alpha;
                unsigned short blue;
                unsigned short green;
                unsigned short red;
            };

            color item;
            destscalecolorn( alpha, item.alpha );
            destscalecolorn( blue, item.blue );
            destscalecolorn( green, item.green );
            destscalecolorn( red, item.red );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::R_32F )
        {
            struct color
            {
                float red;
            };

            color item;
            destscalecolorn( red, item.red );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::GR_3232F )
        {
            struct color
            {
                float green;
                float red;
            };

            color item;
            destscalecolorn( red, item.red );
            destscalecolorn( green, item.green );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::ABGR_32323232F )
        {
            struct color
            {
                float alpha;
                float blue;
                float green;
                float red;
            };

            color item;
            destscalecolorn( alpha, item.alpha );
            destscalecolorn( blue, item.blue );
            destscalecolorn( green, item.green );
            destscalecolorn( red, item.red );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }

        return false;
    }

    template <template <typename numberType> class endianness, typename colorNumberType>
    static AINLINE bool browsetexellum(
        const constRasterRow& srcTexels, uint32 colorIndex, ePVRLegacyPixelFormat pixelFormat,
        colorNumberType& lumOut, colorNumberType& alphaOut
    )
    {
        if ( pixelFormat == ePVRLegacyPixelFormat::I8 ||
             pixelFormat == ePVRLegacyPixelFormat::I8_SEC ||
             pixelFormat == ePVRLegacyPixelFormat::L8 )
        {
            struct color
            {
                unsigned char intensity;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.intensity, lumOut );
            alphaOut = color_defaults <colorNumberType>::one;
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::AI88 ||
                  pixelFormat == ePVRLegacyPixelFormat::AI88_SEC ||
                  pixelFormat == ePVRLegacyPixelFormat::AL_88 )
        {
            struct color
            {
                unsigned char intensity;
                unsigned char alpha;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.intensity, lumOut );
            destscalecolorn( item.alpha, alphaOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::AL_44 )
        {
            struct color
            {
                unsigned char luminance : 4;
                unsigned char alpha : 4;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolor( item.luminance, 15, lumOut );
            destscalecolor( item.alpha, 15, alphaOut );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::L16 )
        {
            struct color
            {
                unsigned short luminance;
            };

            color item = srcTexels.readStructItem <endianness <color>> ( colorIndex );

            destscalecolorn( item.luminance, lumOut );
            alphaOut = color_defaults <colorNumberType>::one;
            return true;
        }

        return false;
    }

    template <template <typename numberType> class endianness, typename colorNumberType>
    static AINLINE bool puttexellum(
        rasterRow& dstTexels, uint32 colorIndex, ePVRLegacyPixelFormat pixelFormat,
        colorNumberType lum, colorNumberType alpha
    )
    {
        if ( pixelFormat == ePVRLegacyPixelFormat::I8 ||
             pixelFormat == ePVRLegacyPixelFormat::I8_SEC ||
             pixelFormat == ePVRLegacyPixelFormat::L8 )
        {
            struct color
            {
                unsigned char intensity;
            };

            color item;
            destscalecolorn( lum, item.intensity );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::AI88 ||
                  pixelFormat == ePVRLegacyPixelFormat::AI88_SEC ||
                  pixelFormat == ePVRLegacyPixelFormat::AL_88 )
        {
            struct color
            {
                unsigned char intensity;
                unsigned char alpha;
            };

            color item;
            destscalecolorn( alpha, item.alpha );
            destscalecolorn( lum, item.intensity );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::AL_44 )
        {
            struct color
            {
                unsigned char lum : 4;
                unsigned char alpha : 4;
            };

            color item;
            item.alpha      = putscalecolor <uint8> ( alpha, 15 );
            item.lum        = putscalecolor <uint8> ( lum, 15 );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }
        else if ( pixelFormat == ePVRLegacyPixelFormat::L16 )
        {
            struct color
            {
                unsigned short lum;
            };

            color item;
            destscalecolorn( lum, item.lum );

            dstTexels.writeStructItem <endianness <color>> ( item, colorIndex );
            return true;
        }

        return false;
    }

public:
    template <typename colorNumberType>
    AINLINE bool getRGBA( const constRasterRow& srcTexels, uint32 colorIndex, colorNumberType& redOut, colorNumberType& greenOut, colorNumberType& blueOut, colorNumberType& alphaOut )
    {
        ePVRLegacyPixelFormatType colorModel = this->colorModel;

        bool gotColor = false;

        if ( colorModel == ePVRLegacyPixelFormatType::RGBA )
        {
            if ( this->isLittleEndian )
            {
                gotColor =
                    browsetexelrgba <endian::little_endian> (
                        srcTexels, colorIndex, this->pixelFormat,
                        redOut, greenOut, blueOut, alphaOut
                    );
            }
            else
            {
                gotColor =
                    browsetexelrgba <endian::big_endian> (
                        srcTexels, colorIndex, this->pixelFormat,
                        redOut, greenOut, blueOut, alphaOut
                    );
            }
        }
        else if ( colorModel == ePVRLegacyPixelFormatType::LUMINANCE )
        {
            colorNumberType lum;

            gotColor = this->getLuminance( srcTexels, colorIndex, lum, alphaOut );

            if ( gotColor )
            {
                redOut = lum;
                greenOut = lum;
                blueOut = lum;
            }
        }
        else
        {
            throw NativeImagingInvalidParameterException( "PVR", L"PVR_INVALIDPARAM_RGBACOLORMODEL", nullptr );
        }

        return gotColor;
    }

    template <typename colorNumberType>
    AINLINE bool getLuminance( const constRasterRow& srcTexels, uint32 colorIndex, colorNumberType& lumOut, colorNumberType& alphaOut )
    {
        ePVRLegacyPixelFormatType colorModel = this->colorModel;

        bool gotColor = false;

        if ( colorModel == ePVRLegacyPixelFormatType::RGBA )
        {
            colorNumberType red, green, blue;

            gotColor = this->getRGBA( srcTexels, colorIndex, red, green, blue, alphaOut );

            if ( gotColor )
            {
                lumOut = rgb2lum( red, green, blue );
            }
        }
        else if ( colorModel == ePVRLegacyPixelFormatType::LUMINANCE )
        {
            if ( this->isLittleEndian )
            {
                gotColor =
                    browsetexellum <endian::little_endian> (
                        srcTexels, colorIndex, this->pixelFormat,
                        lumOut, alphaOut
                    );
            }
            else
            {
                gotColor =
                    browsetexellum <endian::big_endian> (
                        srcTexels, colorIndex, this->pixelFormat,
                        lumOut, alphaOut
                    );
            }
        }
        else
        {
            throw NativeImagingInvalidParameterException( "PVR", L"PVR_INVALIDPARAM_LUMCOLORMODEL", nullptr );
        }

        return gotColor;
    }

    template <typename colorNumberType>
    AINLINE bool setRGBA( rasterRow& dstTexels, uint32 colorIndex, colorNumberType red, colorNumberType green, colorNumberType blue, colorNumberType alpha )
    {
        ePVRLegacyPixelFormatType colorModel = this->colorModel;

        bool didPut = false;

        if ( colorModel == ePVRLegacyPixelFormatType::RGBA )
        {
            if ( this->isLittleEndian )
            {
                didPut =
                    puttexelrgba <endian::little_endian> (
                        dstTexels, colorIndex, this->pixelFormat,
                        red, green, blue, alpha
                    );
            }
            else
            {
                didPut =
                    puttexelrgba <endian::big_endian> (
                        dstTexels, colorIndex, this->pixelFormat,
                        red, green, blue, alpha
                    );
            }
        }
        else if ( colorModel == ePVRLegacyPixelFormatType::LUMINANCE )
        {
            colorNumberType lum = rgb2lum( red, green, blue );

            didPut =
                this->setLuminance( dstTexels, colorIndex, lum, alpha );
        }
        else
        {
            throw NativeImagingInvalidParameterException( "PVR", L"PVR_INVALIDPARAM_RGBACOLORMODEL", nullptr );
        }

        return didPut;
    }

    template <typename colorNumberType>
    AINLINE bool setLuminance( rasterRow& dstTexels, uint32 colorIndex, colorNumberType lum, colorNumberType alpha )
    {
        ePVRLegacyPixelFormatType colorModel = this->colorModel;

        bool didPut = false;

        if ( colorModel == ePVRLegacyPixelFormatType::RGBA )
        {
            didPut =
                this->setRGBA( dstTexels, colorIndex, lum, lum, lum, alpha );
        }
        else if ( colorModel == ePVRLegacyPixelFormatType::LUMINANCE )
        {
            if ( this->isLittleEndian )
            {
                didPut =
                    puttexellum <endian::little_endian> (
                        dstTexels, colorIndex, this->pixelFormat,
                        lum, alpha
                    );
            }
            else
            {
                didPut =
                    puttexellum <endian::big_endian> (
                        dstTexels, colorIndex, this->pixelFormat,
                        lum, alpha
                    );
            }
        }
        else
        {
            throw NativeImagingInvalidParameterException( "PVR", L"PVR_INVALIDPARAM_LUMCOLORMODEL", nullptr );
        }

        return didPut;
    }

    // Generic color push and fetch.
    AINLINE void getColor( const constRasterRow& srcTexels, uint32 colorIndex, abstractColorItem& colorOut )
    {
        ePVRLegacyPixelFormatType pvrColorModel = this->colorModel;

        if ( pvrColorModel == ePVRLegacyPixelFormatType::RGBA )
        {
            colorOut.model = COLORMODEL_RGBA;

            bool gotColor =
                this->getRGBA(
                    srcTexels, colorIndex,
                    colorOut.rgbaColor.r, colorOut.rgbaColor.g, colorOut.rgbaColor.b, colorOut.rgbaColor.a
                );

            if ( !gotColor )
            {
                colorOut.rgbaColor.r = 0;
                colorOut.rgbaColor.g = 0;
                colorOut.rgbaColor.b = 0;
                colorOut.rgbaColor.a = 0;
            }
        }
        else if ( pvrColorModel == ePVRLegacyPixelFormatType::LUMINANCE )
        {
            colorOut.model = COLORMODEL_LUMINANCE;

            bool gotColor =
                this->getLuminance(
                    srcTexels, colorIndex,
                    colorOut.luminance.lum, colorOut.luminance.alpha
                );

            if ( !gotColor )
            {
                colorOut.luminance.lum = 0;
                colorOut.luminance.alpha = 0;
            }
        }
        else
        {
            throw NativeImagingInvalidParameterException( "PVR", L"PVR_INVALIDPARAM_COLORMODEL", nullptr );
        }
    }

    AINLINE void setColor( rasterRow& dstTexels, uint32 colorIndex, const abstractColorItem& color )
    {
        eColorModel rwColorModel = color.model;

        if ( rwColorModel == COLORMODEL_RGBA )
        {
            this->setRGBA(
                dstTexels, colorIndex,
                color.rgbaColor.r, color.rgbaColor.g, color.rgbaColor.b, color.rgbaColor.a
            );
        }
        else if ( rwColorModel == COLORMODEL_LUMINANCE )
        {
            this->setLuminance(
                dstTexels, colorIndex,
                color.luminance.lum, color.luminance.alpha
            );
        }
        else
        {
            throw NativeImagingInvalidParameterException( "PVR", L"PVR_INVALIDPARAM_COLORMODEL", nullptr );
        }
    }

    AINLINE void setClearedColor( abstractColorItem& theItem )
    {
        // Not really important.
        theItem.setClearedColor( COLORMODEL_LUMINANCE );
    }

private:
    ePVRLegacyPixelFormat pixelFormat;
    ePVRLegacyPixelFormatType colorModel;
    bool isLittleEndian;
};

// Under some conditions, we can directly acquire certain pixel formats into RW sample types.
static inline void getPVRRasterFormatMapping(
    ePVRLegacyPixelFormat format, bool isLittleEndian,
    eRasterFormat& rasterFormatOut, eColorOrdering& colorOrderingOut, eCompressionType& compressionTypeOut,
    bool& isDirectMappingOut
)
{
    // We do have to experiment with things for now.
    if ( format == ePVRLegacyPixelFormat::ARGB_4444 ||
         format == ePVRLegacyPixelFormat::ARGB_4444_SEC )
    {
        rasterFormatOut = RASTER_4444;
        colorOrderingOut = COLOR_ABGR;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::ARGB_1555 ||
              format == ePVRLegacyPixelFormat::ARGB_1555_SEC )
    {
        rasterFormatOut = RASTER_1555;
        colorOrderingOut = COLOR_BGRA;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = false;     // the sample structure is different.
    }
    else if ( format == ePVRLegacyPixelFormat::RGB_555 ||
              format == ePVRLegacyPixelFormat::RGB_555_SEC )
    {
        rasterFormatOut = RASTER_555;
        colorOrderingOut = COLOR_BGRA;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = false;     // the sample structure is different.
    }
    else if ( format == ePVRLegacyPixelFormat::RGB_565 ||
              format == ePVRLegacyPixelFormat::RGB_565_SEC )
    {
        rasterFormatOut = RASTER_565;
        colorOrderingOut = COLOR_BGRA;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::ARGB_8888 ||
              format == ePVRLegacyPixelFormat::ARGB_8888_SEC )
    {
        rasterFormatOut = RASTER_8888;
        colorOrderingOut = COLOR_RGBA;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::RGB_888 ||
              format == ePVRLegacyPixelFormat::RGB_888_SEC )
    {
        rasterFormatOut = RASTER_888;
        colorOrderingOut = COLOR_RGBA;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::I8 ||
              format == ePVRLegacyPixelFormat::I8_SEC ||
              format == ePVRLegacyPixelFormat::L8 )
    {
        rasterFormatOut = RASTER_LUM;
        colorOrderingOut = COLOR_RGBA;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::AI88 ||
              format == ePVRLegacyPixelFormat::AI88_SEC ||
              format == ePVRLegacyPixelFormat::AL_88 )
    {
        rasterFormatOut = RASTER_LUM_ALPHA;
        colorOrderingOut = COLOR_RGBA;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::AL_44 )
    {
        rasterFormatOut = RASTER_LUM_ALPHA;
        colorOrderingOut = COLOR_RGBA;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::BGRA_8888 )
    {
        rasterFormatOut = RASTER_8888;
        colorOrderingOut = COLOR_BGRA;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::DXT1 )
    {
        rasterFormatOut = RASTER_DEFAULT;
        colorOrderingOut = COLOR_RGBA;
        compressionTypeOut = RWCOMPRESS_DXT1;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::DXT2 )
    {
        rasterFormatOut = RASTER_DEFAULT;
        colorOrderingOut = COLOR_RGBA;
        compressionTypeOut = RWCOMPRESS_DXT2;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::DXT3 )
    {
        rasterFormatOut = RASTER_DEFAULT;
        colorOrderingOut = COLOR_RGBA;
        compressionTypeOut = RWCOMPRESS_DXT3;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::DXT4 )
    {
        rasterFormatOut = RASTER_DEFAULT;
        colorOrderingOut = COLOR_RGBA;
        compressionTypeOut = RWCOMPRESS_DXT4;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else if ( format == ePVRLegacyPixelFormat::DXT5 )
    {
        rasterFormatOut = RASTER_DEFAULT;
        colorOrderingOut = COLOR_RGBA;
        compressionTypeOut = RWCOMPRESS_DXT5;

        isDirectMappingOut = ( isLittleEndian == true );
    }
    else
    {
        // There is no real close representation for this format, so we use full color.
        rasterFormatOut = RASTER_8888;
        colorOrderingOut = COLOR_BGRA;
        compressionTypeOut = RWCOMPRESS_NONE;

        isDirectMappingOut = false;
    }
}

inline void getPVRLegacyRawColorFormatLink(
    eRasterFormat rasterFormat, uint32 depth, eColorOrdering colorOrder,
    ePVRLegacyPixelFormat& pixelFormatOut, bool& canDirectlyAcquireOut
)
{
    if ( rasterFormat == RASTER_1555 )
    {
        pixelFormatOut = ePVRLegacyPixelFormat::ARGB_1555_SEC;

        canDirectlyAcquireOut = false;  // the sample structure is different.
    }
    else if ( rasterFormat == RASTER_565 )
    {
        pixelFormatOut = ePVRLegacyPixelFormat::RGB_565_SEC;

        canDirectlyAcquireOut = ( depth == 16 && colorOrder == COLOR_BGRA );
    }
    else if ( rasterFormat == RASTER_4444 )
    {
        pixelFormatOut = ePVRLegacyPixelFormat::ARGB_4444_SEC;

        canDirectlyAcquireOut = ( depth == 16 && colorOrder == COLOR_ABGR );
    }
    else if ( rasterFormat == RASTER_LUM )
    {
        pixelFormatOut = ePVRLegacyPixelFormat::I8_SEC;

        canDirectlyAcquireOut = ( depth == 8 );
    }
    else if ( rasterFormat == RASTER_8888 )
    {
        bool isColorOrderFine = false;

        if ( colorOrder == COLOR_RGBA )
        {
            pixelFormatOut = ePVRLegacyPixelFormat::ARGB_8888_SEC;

            isColorOrderFine = true;
        }
        else if ( colorOrder == COLOR_BGRA )
        {
            pixelFormatOut = ePVRLegacyPixelFormat::BGRA_8888;

            isColorOrderFine = true;
        }
        else
        {
            pixelFormatOut = ePVRLegacyPixelFormat::ARGB_8888_SEC;
        }

        canDirectlyAcquireOut = ( isColorOrderFine && depth == 32 );
    }
    else if ( rasterFormat == RASTER_888 )
    {
        pixelFormatOut = ePVRLegacyPixelFormat::RGB_888_SEC;

        canDirectlyAcquireOut = ( depth == 24 && colorOrder == COLOR_RGBA );
    }
    else if ( rasterFormat == RASTER_555 )
    {
        pixelFormatOut = ePVRLegacyPixelFormat::RGB_555_SEC;

        canDirectlyAcquireOut = false;  // the sample structure is different.
    }
    else if ( rasterFormat == RASTER_LUM_ALPHA )
    {
        if ( depth == 8 )
        {
            pixelFormatOut = ePVRLegacyPixelFormat::AL_44;

            canDirectlyAcquireOut = true;
        }
        else if ( depth == 16 )
        {
            pixelFormatOut = ePVRLegacyPixelFormat::AL_88;

            canDirectlyAcquireOut = true;
        }
        else
        {
            pixelFormatOut = ePVRLegacyPixelFormat::AL_88;

            canDirectlyAcquireOut = false;
        }
    }
    else
    {
        // No idea about the structure due to a change in the library or just
        // a weird format. We simply output it as good quality.
        pixelFormatOut = ePVRLegacyPixelFormat::BGRA_8888;

        canDirectlyAcquireOut = false;
    }
}

// I guess each format has to have a fixed depth.
static inline uint32 getPVRLegacyFormatDepth( ePVRLegacyPixelFormat format )
{
    switch( format )
    {
    case ePVRLegacyPixelFormat::ARGB_4444:
    case ePVRLegacyPixelFormat::ARGB_1555:
    case ePVRLegacyPixelFormat::RGB_565:
    case ePVRLegacyPixelFormat::RGB_555:
    case ePVRLegacyPixelFormat::ARGB_8332:
    case ePVRLegacyPixelFormat::AI88:
    case ePVRLegacyPixelFormat::ARGB_4444_SEC:
    case ePVRLegacyPixelFormat::ARGB_1555_SEC:
    case ePVRLegacyPixelFormat::RGB_565_SEC:
    case ePVRLegacyPixelFormat::RGB_555_SEC:
    case ePVRLegacyPixelFormat::AI88_SEC:
    case ePVRLegacyPixelFormat::LVU_655:
    case ePVRLegacyPixelFormat::R_16F:
    case ePVRLegacyPixelFormat::VU_88:
    case ePVRLegacyPixelFormat::L16:
    case ePVRLegacyPixelFormat::AL_88:
        return 16;
    case ePVRLegacyPixelFormat::RGB_888:
    case ePVRLegacyPixelFormat::RGB_888_SEC:
        return 24;
    case ePVRLegacyPixelFormat::ARGB_8888:
    case ePVRLegacyPixelFormat::ARGB_8888_SEC:
    case ePVRLegacyPixelFormat::BGRA_8888:
    case ePVRLegacyPixelFormat::XLVU_8888:
    case ePVRLegacyPixelFormat::QWVU_8888:
    case ePVRLegacyPixelFormat::ABGR_2101010:
    case ePVRLegacyPixelFormat::ARGB_2101010:
    case ePVRLegacyPixelFormat::AWVU_2101010:
    case ePVRLegacyPixelFormat::GR_1616:
    case ePVRLegacyPixelFormat::VU_1616:
    case ePVRLegacyPixelFormat::GR_1616F:
    case ePVRLegacyPixelFormat::R_32F:
        return 32;
    case ePVRLegacyPixelFormat::I8:
    case ePVRLegacyPixelFormat::V_Y1_U_Y0:
    case ePVRLegacyPixelFormat::Y1_V_Y0_U:
    case ePVRLegacyPixelFormat::UYVY:
    case ePVRLegacyPixelFormat::YUY2:
    case ePVRLegacyPixelFormat::I8_SEC:
    case ePVRLegacyPixelFormat::DXT2:
    case ePVRLegacyPixelFormat::DXT3:
    case ePVRLegacyPixelFormat::DXT4:
    case ePVRLegacyPixelFormat::DXT5:
    case ePVRLegacyPixelFormat::RGB332:
    case ePVRLegacyPixelFormat::AL_44:
    case ePVRLegacyPixelFormat::A8:
    case ePVRLegacyPixelFormat::L8:
        return 8;
    case ePVRLegacyPixelFormat::MONOCHROME:
        return 1;
    case ePVRLegacyPixelFormat::PVRTC2:
    case ePVRLegacyPixelFormat::PVRTC2_SEC:
        return 2;
    case ePVRLegacyPixelFormat::PVRTC4:
    case ePVRLegacyPixelFormat::PVRTC4_SEC:
    case ePVRLegacyPixelFormat::DXT1:
    case ePVRLegacyPixelFormat::ETC:
        return 4;
    case ePVRLegacyPixelFormat::ABGR_16161616:
    case ePVRLegacyPixelFormat::ABGR_16161616F:
    case ePVRLegacyPixelFormat::GR_3232F:
        return 64;
    case ePVRLegacyPixelFormat::ABGR_32323232F:
        return 128;
    }

    assert( 0 );

    // Doesnt really happen, if the format is valid.
    return 0;
}

static inline uint32 getPVRLegacyFormatDXTType( ePVRLegacyPixelFormat pixelFormat )
{
    uint32 dxtType = 0;

    if ( pixelFormat == ePVRLegacyPixelFormat::DXT1 )
    {
        dxtType = 1;
    }
    else if ( pixelFormat == ePVRLegacyPixelFormat::DXT2 )
    {
        dxtType = 2;
    }
    else if ( pixelFormat == ePVRLegacyPixelFormat::DXT3 )
    {
        dxtType = 3;
    }
    else if ( pixelFormat == ePVRLegacyPixelFormat::DXT4 )
    {
        dxtType = 4;
    }
    else if ( pixelFormat == ePVRLegacyPixelFormat::DXT5 )
    {
        dxtType = 5;
    }

    return dxtType;
}

static inline void getPVRLegacyFormatSurfaceDimensions(
    ePVRLegacyPixelFormat format,
    uint32 layerWidth, uint32 layerHeight,
    uint32& surfWidthOut, uint32& surfHeightOut
)
{
    switch( format )
    {
    case ePVRLegacyPixelFormat::V_Y1_U_Y0:
    case ePVRLegacyPixelFormat::Y1_V_Y0_U:
    case ePVRLegacyPixelFormat::UYVY:
    case ePVRLegacyPixelFormat::YUY2:
        // 2x2 block format.
        surfWidthOut = ALIGN_SIZE( layerWidth, 2u );
        surfHeightOut = ALIGN_SIZE( layerHeight, 2u );
        return;
    case ePVRLegacyPixelFormat::DXT1:
    case ePVRLegacyPixelFormat::DXT2:
    case ePVRLegacyPixelFormat::DXT3:
    case ePVRLegacyPixelFormat::DXT4:
    case ePVRLegacyPixelFormat::DXT5:
    case ePVRLegacyPixelFormat::ETC:
        // 4x4 block compression.
        surfWidthOut = ALIGN_SIZE( layerWidth, 4u );
        surfHeightOut = ALIGN_SIZE( layerHeight, 4u );
        return;
#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
    case ePVRLegacyPixelFormat::PVRTC2:
    case ePVRLegacyPixelFormat::PVRTC4:
    case ePVRLegacyPixelFormat::PVRTC2_SEC:
    case ePVRLegacyPixelFormat::PVRTC4_SEC:
    {
        // 16x8 or 8x8 block compresion.
        uint32 comprBlockWidth, comprBlockHeight;
        getPVRCompressionBlockDimensions( getPVRLegacyFormatDepth( format ), comprBlockWidth, comprBlockHeight );

        surfWidthOut = ALIGN_SIZE( layerWidth, comprBlockWidth );
        surfHeightOut = ALIGN_SIZE( layerHeight, comprBlockHeight );
        return;
    }
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
    default:
        break;
    }

    // Everything else is considered raw sample, so layer dimms == surf dimms.
    surfWidthOut = layerWidth;
    surfHeightOut = layerHeight;
}

#if 0

// To properly support PVR files, we must also know about "twiddling", ImgTec's special form of "swizzling".
// This is really bad, as I do not have much time.
// NOTE: squaredSurfDimm must be power-of-two and squared!!!
template <typename callbackType>
static AINLINE void ProcessPVRTwiddle( uint32 squaredSurfDimm, callbackType& cb, uint32 linX = 0, uint32 linY = 0, uint32 packedIndex = 0 )
{
    // http://downloads.isee.biz/pub/files/igep-dsp-gst-framework-3_40_00/Graphics_SDK_4_05_00_03/GFX_Linux_SDK/OGLES/SDKPackage/Utilities/PVRTexTool/Documentation/PVRTexTool.Reference%20Manual.1.11f.External.pdf
    // Look at page 18.
    // Ignore the rectangular twiddling part, as I think it is out of scope.

    // We do it recursively.
    if ( squaredSurfDimm == 1 )
    {
        cb( linX, linY, px, py );
    }
    else
    {
        uint32 halfDimm = ( squaredSurfDimm / 2 );
        uint32 squared_halfDimm = halfDimm * halfDimm;

        ProcessPVRTwiddle( halfDimm, cb, linX, linY, packedIndex );  // TOP LEFT
        ProcessPVRTwiddle( halfDimm, cb, linX, linY + halfDimm, packedIndex + squared_halfDimm );
        ProcessPVRTwiddle( halfDimm, cb, linX + halfDimm, linY, packedIndex + squared_halfDimm * 2 );
        ProcessPVRTwiddle( halfDimm, cb, linX + halfDimm, linY + halfDimm, packedIndex + squared_halfDimm * 3 );
    }
}

#endif //disabled.

// Certain cases require endian-adjustment of DXT blocks, so do that here.
template <template <typename numberType> class src_endianness, template <typename numberType> class dst_endianness>
AINLINE void CopyTransformDXTBlock(
    ePVRLegacyPixelFormat dxtTypeFmt,
    const void *srcTexels, void *dstTexels, uint32 block_index
)
{
    if ( dxtTypeFmt == ePVRLegacyPixelFormat::DXT1 )
    {
        const dxt1_block <src_endianness> *srcBlock = (const dxt1_block <src_endianness>*)srcTexels + block_index;

        dxt1_block <dst_endianness> *dstBlock = (dxt1_block <dst_endianness>*)dstTexels + block_index;

        dstBlock->col0 = srcBlock->col0;
        dstBlock->col1 = srcBlock->col1;
        dstBlock->indexList = srcBlock->indexList;
    }
    else if ( dxtTypeFmt == ePVRLegacyPixelFormat::DXT2 ||
              dxtTypeFmt == ePVRLegacyPixelFormat::DXT3 )
    {
        const dxt2_3_block <src_endianness> *srcBlock = (const dxt2_3_block <src_endianness>*)srcTexels + block_index;

        dxt2_3_block <dst_endianness> *dstBlock = (dxt2_3_block <dst_endianness>*)dstTexels + block_index;

        dstBlock->alphaList = srcBlock->alphaList;
        dstBlock->col0 = srcBlock->col0;
        dstBlock->col1 = srcBlock->col1;
        dstBlock->indexList = srcBlock->indexList;
    }
    else if ( dxtTypeFmt == ePVRLegacyPixelFormat::DXT4 ||
              dxtTypeFmt == ePVRLegacyPixelFormat::DXT5 )
    {
        const dxt4_5_block <src_endianness> *srcBlock = (const dxt4_5_block <src_endianness>*)srcTexels + block_index;

        dxt4_5_block <dst_endianness> *dstBlock = (dxt4_5_block <dst_endianness>*)dstTexels + block_index;

        dstBlock->alphaPreMult[0] = srcBlock->alphaPreMult[0];
        dstBlock->alphaPreMult[1] = srcBlock->alphaPreMult[1];
        dstBlock->alphaList = srcBlock->alphaList;
        dstBlock->col0 = srcBlock->col0;
        dstBlock->col1 = srcBlock->col1;
        dstBlock->indexList = srcBlock->indexList;
    }
    else
    {
        assert( 0 );
    }
}

struct pvr_legacy_formatField
{
    uint32 pixelFormat : 8;
    uint32 mipmapsPresent : 1;
    uint32 dataIsTwiddled : 1;
    uint32 containsNormalData : 1;
    uint32 hasBorder : 1;
    uint32 isCubeMap : 1;
    uint32 mipmapsHaveDebugColoring : 1;
    uint32 isVolumeTexture : 1;
    uint32 hasAlphaChannel_pvrtc : 1;
    uint32 isVerticallyFlipped : 1;
    uint32 pad : 15;
};

template <template <typename numberType> class endianness>
struct pvr_header_ver1
{
    endianness <uint32>     height;
    endianness <uint32>     width;
    endianness <uint32>     mipmapCount;

    endianness <pvr_legacy_formatField>     flags;

    endianness <uint32>     surfaceSize;
    endianness <uint32>     bitsPerPixel;
    endianness <uint32>     redMask;    // we really do not care about those masks.
    endianness <uint32>     greenMask;
    endianness <uint32>     blueMask;
    endianness <uint32>     alphaMask;
};
static_assert( ( sizeof( pvr_header_ver1 <endian::p_little_endian> ) + sizeof( uint32 ) ) == 44, "invalid byte-size for PVR version 1 header" );

template <template <typename numberType> class endianness>
struct pvr_header_ver2
{
    endianness <uint32>     height;
    endianness <uint32>     width;
    endianness <uint32>     mipmapCount;

    endianness <pvr_legacy_formatField>     flags;

    endianness <uint32>     surfaceSize;
    endianness <uint32>     bitsPerPixel;
    endianness <uint32>     redMask;
    endianness <uint32>     greenMask;
    endianness <uint32>     blueMask;
    endianness <uint32>     alphaMask;

    endianness <uint32>     pvr_id;
    endianness <uint32>     numberOfSurfaces;
};
static_assert( ( sizeof( pvr_header_ver2 <endian::p_little_endian> ) + sizeof( uint32 ) ) == 52, "invalid byte-size for PVR version 2 header" );

// Meta-information about the PVR format.
static const natimg_supported_native_desc pvr_natimg_suppnattex[] =
{
    { "Direct3D8" },
    { "Direct3D9" },
    { "PowerVR" }
};

static const imaging_filename_ext pvr_natimg_fileExt[] =
{
    { "PVR", true }
};

struct pvrNativeImageTypeManager : public nativeImageTypeManager
{
    struct pvrNativeImage
    {
        inline void resetFormat( void )
        {
            this->pixelFormat = ePVRLegacyPixelFormat::ARGB_4444;
            this->dataIsTwiddled = false;
            this->containsNormalData = false;
            this->hasBorder = false;
            this->isCubeMap = false;
            this->mipmapsHaveDebugColoring = false;
            this->isVolumeTexture = false;
            this->hasAlphaChannel_pvrtc = false;
            this->isVerticallyFlipped = false;

            // Reset cached properties.
            this->bitDepth = 0;

            // We really like the little-endian format.
            this->isLittleEndian = true;
        }

        inline pvrNativeImage( Interface *engineInterface ) : mipmaps( eir::constr_with_alloc::DEFAULT, engineInterface )
        {
            this->engineInterface = engineInterface;

            this->resetFormat();
        }

        // We do not have to make special destructors or copy constructors.
        // The default ones are perfectly fine.
        // Remember that deallocation of data is done by the framework itself!

        Interface *engineInterface;

        // Those fields are specialized for the legacy PVR format for now.
        ePVRLegacyPixelFormat pixelFormat;
        bool dataIsTwiddled;
        bool containsNormalData;
        bool hasBorder;
        bool isCubeMap;
        bool mipmapsHaveDebugColoring;
        bool isVolumeTexture;
        bool hasAlphaChannel_pvrtc;
        bool isVerticallyFlipped;

        // Properties that we cache.
        uint32 bitDepth;

        // Now for the color data itself.
        typedef genmip::mipmapLayer mipmap_t;

        typedef rwVector <mipmap_t> mipmaps_t;

        mipmaps_t mipmaps;

        // Meta-data.
        bool isLittleEndian;
    };

    void ConstructImage( Interface *engineInterface, void *imageMem ) const override
    {
        new (imageMem) pvrNativeImage( engineInterface );
    }

    void CopyConstructImage( Interface *engineInterface, void *imageMem, const void *srcImageMem ) const override
    {
        //new (imageMem) pvrNativeImage( *(const pvrNativeImage*)srcImageMem );
        throw NativeImagingUnsupportedOperationException( "PVR", eOperationType::COPY_CONSTRUCT, nullptr );
    }

    void DestroyImage( Interface *engineInterface, void *imageMem ) const override
    {
        ( (pvrNativeImage*)imageMem )->~pvrNativeImage();
    }

    const char* GetBestSupportedNativeTexture( Interface *engineInterface, const void *imageMem ) const override
    {
        // TODO. It kinda depends on the properties.
        return "PowerVR";
    }

    void ClearImageData( Interface *engineInterface, void *imageMem, bool deallocate ) const override
    {
        pvrNativeImage *natImg = (pvrNativeImage*)imageMem;

        // In this routine we clear mipmap and palette data, basically everything from this image.
        if ( deallocate )
        {
            genmip::deleteMipmapLayers( engineInterface, natImg->mipmaps );
        }

        // Clear all color data references.
        natImg->mipmaps.Clear();

        // Reset the image.
        natImg->resetFormat();
    }

    void ClearPaletteData( Interface *engineInterface, void *imageMem, bool deallocate ) const override
    {
        // PVR native images do not support palette.
    }

    void ReadFromNativeTexture( Interface *engineInterface, void *imageMem, const char *nativeTexName, void *nativeTexMem, acquireFeedback_t& feedbackOut ) const override
    {
        // Writing texels into the PVR native image should be pretty ez.
        pvrNativeImage *natImg = (pvrNativeImage*)imageMem;

        eRasterFormat frm_pvrRasterFormat;
        uint32 frm_pvrDepth;
        uint32 frm_pvrRowAlignment;
        eColorOrdering frm_pvrColorOrder;

        ePaletteType frm_pvrPaletteType = PALETTE_NONE;
        void *frm_pvrPaletteData = nullptr;
        uint32 frm_pvrPaletteSize = 0;

        bool frm_isPaletteNewlyAllocated = false;

        eCompressionType frm_pvrCompressionType;

        bool isFrameworkData = false;

#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
        ePVRInternalFormat pvrtc_comprType;
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE

        bool isPVRTC = false;

        // Meta properties.
        uint8 texRasterType;
        bool texAutoMipmaps;
        bool texCubeMap;
        bool texHasAlpha;

        pvrNativeImage::mipmaps_t srcLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

        bool srcLayersIsNewlyAllocated;

        // Well, we always write stuff in little-endian, because the native textures are in that format.
        bool isLittleEndian = true;

        bool hasProcessedNatTex = false;

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D8
        if ( !hasProcessedNatTex && strcmp( nativeTexName, "Direct3D8" ) == 0 )
        {
            NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)nativeTexMem;

            d3d8FetchPixelDataFromTexture <pvrNativeImage::mipmap_t> (
                engineInterface,
                nativeTex,
                srcLayers,
                frm_pvrRasterFormat, frm_pvrDepth, frm_pvrRowAlignment, frm_pvrColorOrder,
                frm_pvrPaletteType, frm_pvrPaletteData, frm_pvrPaletteSize, frm_pvrCompressionType,
                texRasterType, texAutoMipmaps, texHasAlpha,
                srcLayersIsNewlyAllocated
            );

            // Direct3D8 native texture does not support cubemaps.
            texCubeMap = false;

            isFrameworkData = true;

            frm_isPaletteNewlyAllocated = srcLayersIsNewlyAllocated;

            hasProcessedNatTex = true;
        }
#endif //RWLIB_INCLUDE_NATIVETEX_D3D8

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9
        if ( !hasProcessedNatTex && strcmp( nativeTexName, "Direct3D9" ) == 0 )
        {
            NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)nativeTexMem;

            d3d9FetchPixelDataFromTexture <pvrNativeImage::mipmap_t> (
                engineInterface,
                nativeTex,
                srcLayers,
                frm_pvrRasterFormat, frm_pvrDepth, frm_pvrRowAlignment, frm_pvrColorOrder,
                frm_pvrPaletteType, frm_pvrPaletteData, frm_pvrPaletteSize, frm_pvrCompressionType,
                texRasterType, texCubeMap, texAutoMipmaps, texHasAlpha,
                srcLayersIsNewlyAllocated
            );

            isFrameworkData = true;

            frm_isPaletteNewlyAllocated = srcLayersIsNewlyAllocated;

            hasProcessedNatTex = true;
        }
#endif //RWLIB_INCLUDE_NATIVETEX_D3D9

#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
        if ( !hasProcessedNatTex && strcmp( nativeTexName, "PowerVR" ) == 0 )
        {
            // We want to take stuff directly.
            NativeTexturePVR *nativeTex = (NativeTexturePVR*)nativeTexMem;

            size_t mipmapCount = nativeTex->mipmaps.GetCount();

            srcLayers.Resize( mipmapCount );

            for ( size_t n = 0; n < mipmapCount; n++ )
            {
                const NativeTexturePVR::mipmapLayer& srcLayer = nativeTex->mipmaps[ n ];

                uint32 surfWidth = srcLayer.width;
                uint32 surfHeight = srcLayer.height;

                uint32 layerWidth = srcLayer.layerWidth;
                uint32 layerHeight = srcLayer.layerHeight;

                void *mipTexels = srcLayer.texels;
                uint32 mipDataSize = srcLayer.dataSize;

                // Just move it over ;)
                pvrNativeImage::mipmap_t newLayer;
                newLayer.width = surfWidth;
                newLayer.height = surfHeight;

                newLayer.layerWidth = layerWidth;
                newLayer.layerHeight = layerHeight;

                newLayer.texels = mipTexels;
                newLayer.dataSize = mipDataSize;

                srcLayers[ n ] = std::move( newLayer );
            }

            pvrtc_comprType = nativeTex->internalFormat;

            texRasterType = 4;
            texAutoMipmaps = false;
            texCubeMap = false;
            texHasAlpha = nativeTex->hasAlpha;

            isPVRTC = true;

            // No need to allocate new copies because PVRTC can be directly written :)
            srcLayersIsNewlyAllocated = false;

            hasProcessedNatTex = true;
        }
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE

        if ( !hasProcessedNatTex )
        {
            throw NativeImagingInvalidParameterException( "PVR", L"NATIVETEX_FRIENDLYNAME", nullptr );
        }

        assert( isFrameworkData || isPVRTC );

        try
        {
            size_t mipmapCount = srcLayers.GetCount();

            uint32 dstRowAlignment = getPVRNativeImageRowAlignment();

            // Now that we have data, we want to turn it into a valid PVR format link.
            // We might need conversion of data.
            ePVRLegacyPixelFormat pvrPixelFormat;
            uint32 pvrDepth;

            bool pvr_hasAlphaChannel_pvrtc = false;

            if ( isFrameworkData )
            {
                // We can always take DXTn data directly, but sometimes have to convert color samples.
                if ( frm_pvrCompressionType == RWCOMPRESS_NONE )
                {
                    // Get a color format link.
                    bool canDirectlyAcquireColor;

                    getPVRLegacyRawColorFormatLink( frm_pvrRasterFormat, frm_pvrDepth, frm_pvrColorOrder, pvrPixelFormat, canDirectlyAcquireColor );

                    // We do cache this property.
                    pvrDepth = getPVRLegacyFormatDepth( pvrPixelFormat );

                    // Must not forget that color can also travel as palette-type.
                    // Since PVR does not support it, we have to convert.
                    bool canDirectlyAcquire = ( frm_pvrPaletteType == PALETTE_NONE && canDirectlyAcquireColor );

                    if ( canDirectlyAcquire )
                    {
                        // There is also the factor of row alignment.
                        // Make sure our buffers are properly aligned.
                        bool alignError =
                            doesPixelDataNeedAddressabilityAdjustment(
                                srcLayers,
                                frm_pvrDepth, frm_pvrRowAlignment,
                                pvrDepth, dstRowAlignment
                            );

                        if ( alignError )
                        {
                            // We unfortunately have to create new buffers.
                            canDirectlyAcquire = false;
                        }
                    }

                    // If we have to convert, then do it.
                    if ( !canDirectlyAcquire )
                    {
                        // Prepare the color pipelines.
                        colorModelDispatcher srcDispatch(
                            frm_pvrRasterFormat, frm_pvrColorOrder, frm_pvrDepth,
                            frm_pvrPaletteData, frm_pvrPaletteSize, frm_pvrPaletteType
                        );

                        ePVRLegacyPixelFormatType pvrColorModel = getPVRLegacyPixelFormatType( pvrPixelFormat );

                        pvrColorDispatcher dstDispatch(
                            pvrPixelFormat, pvrColorModel, isLittleEndian
                        );

                        pvrNativeImage::mipmaps_t convLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

                        convLayers.Resize( mipmapCount );

                        try
                        {
                            for ( size_t n = 0; n < mipmapCount; n++ )
                            {
                                pvrNativeImage::mipmap_t& srcLayer = srcLayers[ n ];

                                // It is guarranteed that we are raw-sample-type.
                                uint32 layerWidth = srcLayer.layerWidth;
                                uint32 layerHeight = srcLayer.layerHeight;

                                void *srcTexels = srcLayer.texels;
                                //uint32 srcDataSize = srcLayer.dataSize;

                                rasterRowSize srcRowSize = getRasterDataRowSize( layerWidth, frm_pvrDepth, frm_pvrRowAlignment );

                                // Allocate the destination layer.
                                rasterRowSize dstRowSize = getPVRNativeImageRasterDataRowSize( layerWidth, pvrDepth );

                                uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, layerHeight );

                                void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

                                try
                                {
                                    // Convert!
                                    copyTexelDataEx(
                                        srcTexels, dstTexels,
                                        srcDispatch, dstDispatch,
                                        layerWidth, layerHeight,
                                        0, 0,
                                        0, 0,
                                        srcRowSize, dstRowSize
                                    );
                                }
                                catch( ... )
                                {
                                    engineInterface->PixelFree( dstTexels );

                                    throw;
                                }

                                // If there were new layers previously, free the old ones.
                                if ( srcLayersIsNewlyAllocated )
                                {
                                    engineInterface->PixelFree( srcTexels );

                                    srcLayer.texels = nullptr;
                                }

                                // Move as new layer.
                                pvrNativeImage::mipmap_t newLayer;
                                newLayer.width = layerWidth;
                                newLayer.height = layerHeight;

                                newLayer.layerWidth = layerWidth;
                                newLayer.layerHeight = layerHeight;

                                newLayer.texels = dstTexels;
                                newLayer.dataSize = dstDataSize;

                                convLayers[ n ] = std::move( newLayer );
                            }
                        }
                        catch( ... )
                        {
                            genmip::deleteMipmapLayers( engineInterface, convLayers );

                            throw;
                        }

                        // Replace the layers, finally.
                        srcLayers = std::move( convLayers );

                        srcLayersIsNewlyAllocated = true;
                    }
                }
                else if ( frm_pvrCompressionType == RWCOMPRESS_DXT1 )
                {
                    pvrPixelFormat = ePVRLegacyPixelFormat::DXT1;

                    pvrDepth = 4;
                }
                else if ( frm_pvrCompressionType == RWCOMPRESS_DXT2 )
                {
                    pvrPixelFormat = ePVRLegacyPixelFormat::DXT2;

                    pvrDepth = 8;
                }
                else if ( frm_pvrCompressionType == RWCOMPRESS_DXT3 )
                {
                    pvrPixelFormat = ePVRLegacyPixelFormat::DXT3;

                    pvrDepth = 8;
                }
                else if ( frm_pvrCompressionType == RWCOMPRESS_DXT4 )
                {
                    pvrPixelFormat = ePVRLegacyPixelFormat::DXT4;

                    pvrDepth = 8;
                }
                else if ( frm_pvrCompressionType == RWCOMPRESS_DXT5 )
                {
                    pvrPixelFormat = ePVRLegacyPixelFormat::DXT5;

                    pvrDepth = 8;
                }
                else
                {
                    throw NativeImagingInternalErrorException( "PVR", L"PVR_INTERNERR_UNSUPPCOMPR" );
                }
            }
#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
            else if ( isPVRTC )
            {
                // This is a simple direct acquisition.
                if ( pvrtc_comprType == GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG )
                {
                    pvrPixelFormat = ePVRLegacyPixelFormat::PVRTC2_SEC;
                    pvrDepth = 2;

                    pvr_hasAlphaChannel_pvrtc = false;
                }
                else if ( pvrtc_comprType == GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG )
                {
                    pvrPixelFormat = ePVRLegacyPixelFormat::PVRTC2_SEC;
                    pvrDepth = 2;

                    pvr_hasAlphaChannel_pvrtc = true;
                }
                else if ( pvrtc_comprType == GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG )
                {
                    pvrPixelFormat = ePVRLegacyPixelFormat::PVRTC4_SEC;
                    pvrDepth = 4;

                    pvr_hasAlphaChannel_pvrtc = false;
                }
                else if ( pvrtc_comprType == GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG )
                {
                    pvrPixelFormat = ePVRLegacyPixelFormat::PVRTC4_SEC;
                    pvrDepth = 4;

                    pvr_hasAlphaChannel_pvrtc = true;
                }
                else
                {
                    throw NativeImagingNativeTextureInvalidParameterException( "PVR", "PowerVR", L"PVR_INVALIDCFG_PVRCOMPRTYPE", nullptr );
                }
            }
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
            else
            {
                assert( 0 );
            }

            // Now we get to write things!
            natImg->mipmaps = std::move( srcLayers );
            natImg->pixelFormat = pvrPixelFormat;
            natImg->dataIsTwiddled = false;
            natImg->containsNormalData = false;
            natImg->hasBorder = false;
            natImg->isCubeMap = texCubeMap;
            natImg->mipmapsHaveDebugColoring = false;
            natImg->isVolumeTexture = false;
            natImg->hasAlphaChannel_pvrtc = pvr_hasAlphaChannel_pvrtc;
            natImg->isVerticallyFlipped = false;

            // We want to write some cached things aswell.
            natImg->bitDepth = pvrDepth;

            // And some meta-data info.
            natImg->isLittleEndian = isLittleEndian;

            // Done writing to native image :)
        }
        catch( ... )
        {
            // On error, clear all kind of color data that was temporary.
            if ( srcLayersIsNewlyAllocated )
            {
                genmip::deleteMipmapLayers( engineInterface, srcLayers );

                if ( frm_pvrPaletteData )
                {
                    engineInterface->PixelFree( frm_pvrPaletteData );
                }
            }

            throw;
        }

        // Since we never take the palette, clear it if it was allocated.
        if ( frm_isPaletteNewlyAllocated )
        {
            if ( frm_pvrPaletteData )
            {
                engineInterface->PixelFree( frm_pvrPaletteData );
            }
        }

        // Inform the runtime of direct acquisition.
        feedbackOut.hasDirectlyAcquired = ( srcLayersIsNewlyAllocated == false );
        feedbackOut.hasDirectlyAcquiredPalette = false; // never.
    }

    void WriteToNativeTexture( Interface *engineInterface, void *imageMem, const char *nativeTexName, void *nativeTexMem, acquireFeedback_t& feedbackOut ) const override
    {
        pvrNativeImage *natImg = (pvrNativeImage*)imageMem;

        // Let's first try putting PVR stuff into native textures.
        ePVRLegacyPixelFormat pixelFormat = natImg->pixelFormat;

        bool isLittleEndian = natImg->isLittleEndian;

        size_t mipmapCount = natImg->mipmaps.GetCount();

        // Short out if there is nothing to do.
        if ( mipmapCount == 0 )
            return;

        // Determine the target capabilities.
        bool isDirect3D8 = false;
        bool isDirect3D9 = false;
        bool isPowerVR = false;

        if ( strcmp( nativeTexName, "Direct3D8" ) == 0 )
        {
            isDirect3D8 = true;
        }
        else if ( strcmp( nativeTexName, "Direct3D9" ) == 0 )
        {
            isDirect3D9 = true;
        }
        else if ( strcmp( nativeTexName, "PowerVR" ) == 0 )
        {
            isPowerVR = true;
        }
        else
        {
            throw NativeImagingNativeTextureInvalidParameterException( "PVR", "PowerVR", L"NATIVETEX_FRIENDLYNAME", nullptr );
        }

        // Decide how we can push texels to the native image.
        bool wantsFrameworkInput = ( isDirect3D8 || isDirect3D9 );
        bool wantsPVRTC = ( isPowerVR );

        // For whatever format it wants, there is always a direct mapping possibility.
        bool hasFormatDirectMapping = false;

        // We first want to see if we can just directly acquire the color data.
        // In essense, we want to detect what kind of color data we actually have.
        ePVRLegacyPixelFormatType colorFormatType = getPVRLegacyPixelFormatType( pixelFormat );

        if ( colorFormatType == ePVRLegacyPixelFormatType::UNKNOWN )
        {
            throw NativeImagingInternalErrorException( "PVR", L"PVR_INTERNERR_UNSUPPCOLORFMT" );
        }

        // Check if we are PVRTC compressed.
        bool isPVRTC_compressed = false;

#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
        ePVRInternalFormat pvrtc_comprType;
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE

        if ( colorFormatType == ePVRLegacyPixelFormatType::COMPRESSED )
        {
            if ( pixelFormat == ePVRLegacyPixelFormat::PVRTC2 || pixelFormat == ePVRLegacyPixelFormat::PVRTC2_SEC )
            {
#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
                if ( natImg->hasAlphaChannel_pvrtc )
                {
                    pvrtc_comprType = GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
                }
                else
                {
                    pvrtc_comprType = GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
                }
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE

                isPVRTC_compressed = true;
            }
            else if ( pixelFormat == ePVRLegacyPixelFormat::PVRTC4 || pixelFormat == ePVRLegacyPixelFormat::PVRTC4_SEC )
            {
#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
                if ( natImg->hasAlphaChannel_pvrtc )
                {
                    pvrtc_comprType = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
                }
                else
                {
                    pvrtc_comprType = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
                }
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE

                isPVRTC_compressed = true;
            }
        }

        // Also verify if we even can handle the PVRTC compression.
#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
        pvrNativeTextureTypeProvider *pvrNativeEnv = nullptr;
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE

        if ( isPVRTC_compressed || wantsPVRTC )
        {
#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
            pvrNativeEnv = pvrNativeTextureTypeProviderRegister.get().GetPluginStruct( (EngineInterface*)engineInterface );

            if ( !pvrNativeEnv )
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
            {
                throw NativeImagingInternalErrorException( "PVR", L"PVR_INTERNERR_NOPVRNATIVETEXTYPE" );
            }
        }

        uint32 pvr_bitDepth = natImg->bitDepth;

        // FRAMEWORK DIRECT MAPPING PARAMS.
        eRasterFormat frm_pvrRasterFormat;
        uint32 frm_pvrDepth = pvr_bitDepth;
        uint32 frm_pvrRowAlignment = getPVRNativeImageRowAlignment();
        eColorOrdering frm_pvrColorOrder;

        eCompressionType frm_pvrCompressionType;

        bool pvr_hasAlpha;

        pvrNativeImage::mipmaps_t *useColorLayers = &natImg->mipmaps;

        bool areLayersNewlyAllocated = false;

        // Boolean whether image data was taken by reference in the native textures.
        bool hasDirectlyAcquired = false;

        // Maybe we want to allocate new layers.
        pvrNativeImage::mipmaps_t tmpMipmapLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

        try
        {
            if ( wantsFrameworkInput )
            {
                getPVRRasterFormatMapping( pixelFormat, isLittleEndian, frm_pvrRasterFormat, frm_pvrColorOrder, frm_pvrCompressionType, hasFormatDirectMapping );

                // If we do not have a framework compatible format, we convert to a framework compatible one.
                if ( !hasFormatDirectMapping )
                {
                    frm_pvrDepth = Bitmap::getRasterFormatDepth( frm_pvrRasterFormat );
                    frm_pvrRowAlignment = 4;    // for good measure.

                    // We could be PVRTC compressed, so we need to decompress.
                    if ( colorFormatType == ePVRLegacyPixelFormatType::COMPRESSED )
                    {
                        bool hasProcessedCompression = false;

#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
                        if ( !hasProcessedCompression && isPVRTC_compressed )
                        {
                            // Prepare decompression params.
                            // Some handles to compressor params.
                            pvrNativeTextureTypeProvider::PVRPixelType pvrSrcPixelType = pvrNativeEnv->PVRGetCachedPixelType( pvrtc_comprType );

                            pvrNativeTextureTypeProvider::PVRPixelType pvrDstPixelType = pvrNativeEnv->pvrPixelType_rgba8888;

                            // Decompress the layers.
                            pvrNativeImage::mipmaps_t transLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

                            transLayers.Resize( mipmapCount );

                            try
                            {
                                for ( size_t n = 0; n < mipmapCount; n++ )
                                {
                                    pvrNativeImage::mipmap_t& srcLayer = (*useColorLayers)[ n ];

                                    uint32 surfWidth = srcLayer.width;
                                    uint32 surfHeight = srcLayer.height;

                                    uint32 layerWidth = srcLayer.layerWidth;
                                    uint32 layerHeight = srcLayer.layerHeight;

                                    void *srcTexels = srcLayer.texels;
                                    uint32 srcDataSize = srcLayer.dataSize;

                                    // We reuse code from the PowerVR native texture :)
                                    void *dstTexels;
                                    uint32 dstDataSize;

                                    pvrNativeEnv->DecompressPVRMipmap(
                                        engineInterface,
                                        surfWidth, surfHeight, layerWidth, layerHeight, srcTexels,
                                        RASTER_8888, 32, COLOR_RGBA,
                                        frm_pvrRasterFormat, frm_pvrDepth, frm_pvrRowAlignment, frm_pvrColorOrder,
                                        pvrSrcPixelType, pvrDstPixelType,
                                        dstTexels, dstDataSize
                                    );

                                    // If we previously had new mipmap layers, free them.
                                    if ( areLayersNewlyAllocated )
                                    {
                                        engineInterface->PixelFree( srcTexels );

                                        srcLayer.texels = nullptr;
                                    }

                                    // Since we are decompressed now, surf == layer dimms.
                                    pvrNativeImage::mipmap_t newLayer;
                                    newLayer.width = layerWidth;
                                    newLayer.height = layerHeight;

                                    newLayer.layerWidth = layerWidth;
                                    newLayer.layerHeight = layerHeight;

                                    newLayer.texels = dstTexels;
                                    newLayer.dataSize = dstDataSize;

                                    transLayers[ n ] = std::move( newLayer );
                                }
                            }
                            catch( ... )
                            {
                                genmip::deleteMipmapLayers( engineInterface, transLayers );

                                throw;
                            }

                            // Just like in the case of DDS native image, we could optimize this.
                            tmpMipmapLayers = std::move( transLayers );

                            areLayersNewlyAllocated = true;

                            useColorLayers = &tmpMipmapLayers;

                            hasProcessedCompression = true;
                        }
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE

                        if ( !hasProcessedCompression && (
                                  pixelFormat == ePVRLegacyPixelFormat::DXT1 ||
                                  pixelFormat == ePVRLegacyPixelFormat::DXT2 ||
                                  pixelFormat == ePVRLegacyPixelFormat::DXT3 ||
                                  pixelFormat == ePVRLegacyPixelFormat::DXT4 ||
                                  pixelFormat == ePVRLegacyPixelFormat::DXT5 ) )
                        {
                            // To convert DXT to compatible (for us or the PVR image) format, we just have to byte-swap them to the correct endianness.
                            pvrNativeImage::mipmaps_t convLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

                            convLayers.Resize( mipmapCount );

                            try
                            {
                                // Do endian swap into native texture format.
                                for ( size_t n = 0; n < mipmapCount; n++ )
                                {
                                    pvrNativeImage::mipmap_t& srcLayer = (*useColorLayers )[ n ];

                                    uint32 surfWidth = srcLayer.width;
                                    uint32 surfHeight = srcLayer.height;

                                    uint32 layerWidth = srcLayer.layerWidth;
                                    uint32 layerHeight = srcLayer.layerHeight;

                                    void *srcTexels = srcLayer.texels;
                                    uint32 srcDataSize = srcLayer.dataSize;

                                    // Get DXT properties.
                                    uint32 dxtBlocksWidth = ( surfWidth / 4u );
                                    uint32 dxtBlocksHeight = ( surfHeight / 4u );

                                    uint32 dxtBlocksCount = ( dxtBlocksWidth * dxtBlocksHeight );

                                    // Swap!
                                    void *dstTexels = engineInterface->PixelAllocate( srcDataSize );

                                    try
                                    {
                                        // Swap the DXT blocks.
                                        for ( uint32 block_index = 0; block_index < dxtBlocksCount; block_index++ )
                                        {
                                            CopyTransformDXTBlock <endian::big_endian, endian::little_endian> ( pixelFormat, srcTexels, dstTexels, block_index );
                                        }
                                    }
                                    catch( ... )
                                    {
                                        engineInterface->PixelFree( dstTexels );

                                        throw;
                                    }

                                    // If we have temporary layers, free them.
                                    if ( areLayersNewlyAllocated )
                                    {
                                        engineInterface->PixelFree( dstTexels );
                                    }

                                    pvrNativeImage::mipmap_t newLayer;
                                    newLayer.width = surfWidth;
                                    newLayer.height = surfHeight;

                                    newLayer.layerWidth = layerWidth;
                                    newLayer.layerHeight = layerHeight;

                                    newLayer.texels = dstTexels;
                                    newLayer.dataSize = srcDataSize;

                                    convLayers[ n ] = std::move( newLayer );
                                }
                            }
                            catch( ... )
                            {
                                genmip::deleteMipmapLayers( engineInterface, convLayers );

                                throw;
                            }

                            tmpMipmapLayers = std::move( convLayers );

                            areLayersNewlyAllocated = true;

                            useColorLayers = &tmpMipmapLayers;

                            hasProcessedCompression = true;
                        }

                        if ( !hasProcessedCompression )
                        {
                            throw NativeImagingInternalErrorException( "PVR", L"PVR_INTERNERR_UNSUPPCOMPR" );
                        }
                    }
                    else if ( colorFormatType == ePVRLegacyPixelFormatType::RGBA ||
                              colorFormatType == ePVRLegacyPixelFormatType::LUMINANCE )
                    {
                        // Prepare the color pipelines.
                        pvrColorDispatcher srcDispatch( pixelFormat, colorFormatType, isLittleEndian );
                        colorModelDispatcher dstDispatch( frm_pvrRasterFormat, frm_pvrColorOrder, frm_pvrDepth, nullptr, 0, PALETTE_NONE );

                        // Perform easy conversion from PVR color samples to framework samples.
                        pvrNativeImage::mipmaps_t convLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

                        convLayers.Resize( mipmapCount );

                        try
                        {
                            for ( size_t n = 0; n < mipmapCount; n++ )
                            {
                                pvrNativeImage::mipmap_t& srcLayer = (*useColorLayers)[ n ];

                                // Since we are RGBA or LUMIANCE type data, surf dimms == layer dimms.
                                uint32 layerWidth = srcLayer.layerWidth;
                                uint32 layerHeight = srcLayer.layerHeight;

                                void *srcTexels = srcLayer.texels;
                                //uint32 srcDataSize = srcLayer.dataSize;

                                rasterRowSize srcRowSize = getPVRNativeImageRasterDataRowSize( layerWidth, pvr_bitDepth );

                                // Just transform stuff.
                                rasterRowSize dstRowSize = getRasterDataRowSize( layerWidth, frm_pvrDepth, frm_pvrRowAlignment );

                                uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, layerHeight );

                                void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

                                try
                                {
                                    copyTexelDataEx(
                                        srcTexels, dstTexels,
                                        srcDispatch, dstDispatch,
                                        layerWidth, layerHeight,
                                        0, 0,
                                        0, 0,
                                        srcRowSize, dstRowSize
                                    );
                                }
                                catch( ... )
                                {
                                    engineInterface->PixelFree( dstTexels );

                                    throw;
                                }

                                // If we had newly allocated color buffers, free them.
                                if ( areLayersNewlyAllocated )
                                {
                                    engineInterface->PixelFree( srcTexels );

                                    srcLayer.texels = nullptr;
                                }

                                // Put stuff into the layers.
                                pvrNativeImage::mipmap_t newLayer;
                                newLayer.width = layerWidth;
                                newLayer.height = layerHeight;

                                newLayer.layerWidth = layerWidth;
                                newLayer.layerHeight = layerHeight;

                                newLayer.texels = dstTexels;
                                newLayer.dataSize = dstDataSize;

                                convLayers[ n ] = std::move( newLayer );
                            }
                        }
                        catch( ... )
                        {
                            genmip::deleteMipmapLayers( engineInterface, convLayers );

                            throw;
                        }

                        // Set stuff as active.
                        tmpMipmapLayers = std::move( convLayers );

                        areLayersNewlyAllocated = true;

                        useColorLayers = &tmpMipmapLayers;
                    }
                    else
                    {
                        throw NativeImagingInvalidConfigurationException( "PVR", L"PVR_INVALIDCFG_PVRIMGTYPE" );
                    }
                }

                // Now since we have things in framework format, we calculate the alpha flag pretty easily.
                pvr_hasAlpha = frameworkCalculateHasAlpha(
                    engineInterface,
                    *useColorLayers,
                    frm_pvrRasterFormat, frm_pvrDepth, frm_pvrRowAlignment, frm_pvrColorOrder,
                    PALETTE_NONE, nullptr, 0, frm_pvrCompressionType
                );
            }
#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
            else if ( wantsPVRTC )
            {
                // We can directly map if we are already PVRTC compressed.
                hasFormatDirectMapping = isPVRTC_compressed;

                // If we have no PVRTC data, we must compress to it.
                if ( !hasFormatDirectMapping )
                {
                    // I dont take any gambles for the PVR native image format, as it is not that important.
                    // Will have to overhaul this code anyway, improve the code sharing, optimize away some hurdles, etc.
                    // With that said, PowerVR images must be power-of-two before being compressed to PVRTC, and I DO NOT DO THAT HERE.
                    {
                        nativeTextureSizeRules sizeRules;
                        getPVRNativeTextureSizeRules( sizeRules );

                        if ( sizeRules.verifyMipmaps( *useColorLayers ) == false )
                        {
                            throw NativeImagingNativeTextureInvalidConfigurationException( "PVR", "PowerVR", L"PVR_INVALIDCFG_NOPO2" );
                        }
                    }

                    ePVRLegacyPixelFormat tmpPixelFormat = pixelFormat;
                    ePVRLegacyPixelFormatType tmpPixelFormatType = colorFormatType;
                    uint32 tmpPixelDepth = pvr_bitDepth;

                    // Decompress anything that is compressed.
                    // If we cannot, then we fail.
                    if ( colorFormatType == ePVRLegacyPixelFormatType::COMPRESSED )
                    {
                        tmpPixelFormat = ePVRLegacyPixelFormat::BGRA_8888;
                        tmpPixelFormatType = ePVRLegacyPixelFormatType::RGBA;
                        tmpPixelDepth = 32;

                        // Check if we are DXT compressed.
                        uint32 dxtType = getPVRLegacyFormatDXTType( pixelFormat );

                        // Handle DXT compression.
                        if ( dxtType != 0 )
                        {
                            pvrColorDispatcher putDispatch( tmpPixelFormat, tmpPixelFormatType, endian::is_little_endian() );

                            pvrNativeImage::mipmaps_t convLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

                            convLayers.Resize( mipmapCount );

                            try
                            {
                                for ( size_t n = 0; n < mipmapCount; n++ )
                                {
                                    pvrNativeImage::mipmap_t& srcLayer = (*useColorLayers)[ n ];

                                    uint32 surfWidth = srcLayer.width;
                                    uint32 surfHeight = srcLayer.height;

                                    uint32 layerWidth = srcLayer.layerWidth;
                                    uint32 layerHeight = srcLayer.layerHeight;

                                    void *srcTexels = srcLayer.texels;

                                    // Decompress the things into a good format.
                                    void *dstTexels;
                                    uint32 dstDataSize;

                                    genericDecompressTexelsUsingDXT <endian::little_endian> (
                                        engineInterface,
                                        dxtType, engineInterface->GetDXTRuntime(),
                                        surfWidth, surfHeight, 1,
                                        layerWidth, layerHeight,
                                        srcTexels, putDispatch, tmpPixelDepth,
                                        dstTexels, dstDataSize
                                    );

                                    // If we had temporary mipmap layers, free them.
                                    if ( areLayersNewlyAllocated )
                                    {
                                        engineInterface->PixelFree( srcTexels );

                                        srcLayer.texels = nullptr;
                                    }

                                    // Store the new layer.
                                    // The new layer is in raw format.
                                    pvrNativeImage::mipmap_t newLayer;
                                    newLayer.width = layerWidth;
                                    newLayer.height = layerHeight;

                                    newLayer.layerWidth = layerWidth;
                                    newLayer.layerHeight = layerHeight;

                                    newLayer.texels = dstTexels;
                                    newLayer.dataSize = dstDataSize;

                                    convLayers[ n ] = std::move( newLayer );
                                }
                            }
                            catch( ... )
                            {
                                genmip::deleteMipmapLayers( engineInterface, convLayers );

                                throw;
                            }

                            tmpMipmapLayers = std::move( convLayers );

                            areLayersNewlyAllocated = true;

                            useColorLayers = &tmpMipmapLayers;
                        }
                        else
                        {
                            throw NativeImagingInternalErrorException( "PVR", L"PVR_INTERNERR_UNSUPPCOMPR" );
                        }
                    }

                    // We have a fixed color format now, so set up the dispatcher.
                    pvrColorDispatcher tmpColorDispatch( tmpPixelFormat, tmpPixelFormatType, endian::is_little_endian() );

                    // At this point we must decide by the color data itself whether the texture has alpha or not.
                    bool shouldHaveAlpha = false;

                    if ( doesPVRLegacyFormatHaveAlphaChannel( tmpPixelFormat ) )
                    {
                        const pvrNativeImage::mipmap_t& baseLayer = (*useColorLayers)[ 0 ];

                        // Try to calculate it to the best of our abilities.
                        // Since we are raw colors, that is pretty simple.
                        shouldHaveAlpha =
                            rawGenericColorBufferHasAlpha(
                                baseLayer.layerWidth, baseLayer.layerHeight,
                                baseLayer.texels, baseLayer.dataSize,
                                tmpColorDispatch, tmpPixelDepth, frm_pvrRowAlignment
                            );
                    }

                    // We take the predicate for compression from the PowerVR native texture.
                    pvrtc_comprType = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
                    {
                        const pvrNativeImage::mipmap_t& baseLayer = (*useColorLayers)[ 0 ];

                        pvrtc_comprType = pvrNativeEnv->GetRecommendedPVRCompressionFormat( baseLayer.layerWidth, baseLayer.layerHeight, shouldHaveAlpha );
                    }

                    uint32 comprBitDepth = getDepthByPVRFormat( pvrtc_comprType );

                    // Prepare PVR compression params.
                    pvrNativeTextureTypeProvider::PVRPixelType pvrSrcPixelType = pvrNativeEnv->pvrPixelType_rgba8888;

                    pvrNativeTextureTypeProvider::PVRPixelType pvrDstPixelType = pvrNativeEnv->PVRGetCachedPixelType( pvrtc_comprType );

                    uint32 pvrBlockWidth, pvrBlockHeight;

                    getPVRCompressionBlockDimensions( comprBitDepth, pvrBlockWidth, pvrBlockHeight );

                    // Compress!
                    pvrNativeImage::mipmaps_t convLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

                    convLayers.Resize( mipmapCount );

                    try
                    {
                        for ( size_t n = 0; n < mipmapCount; n++ )
                        {
                            pvrNativeImage::mipmap_t& srcLayer = (*useColorLayers)[ n ];

                            // Remember: we assume we got raw texels already!
                            uint32 layerWidth = srcLayer.layerWidth;
                            uint32 layerHeight = srcLayer.layerHeight;

                            void *srcTexels = srcLayer.texels;
                            uint32 srcDataSize = srcLayer.dataSize;

                            // Generic compression task.
                            uint32 dstSurfWidth, dstSurfHeight;
                            void *dstTexels;
                            uint32 dstDataSize;

                            pvrNativeEnv->GenericCompressMipmapToPVR(
                                engineInterface,
                                layerWidth, layerHeight, srcTexels,
                                tmpColorDispatch, tmpPixelDepth, frm_pvrRowAlignment,
                                RASTER_8888, 32, COLOR_RGBA,
                                pvrSrcPixelType, pvrDstPixelType,
                                pvrBlockWidth, pvrBlockHeight,
                                comprBitDepth,
                                dstSurfWidth, dstSurfHeight,
                                dstTexels, dstDataSize
                            );

                            // If we have temp color data, free it.
                            if ( areLayersNewlyAllocated )
                            {
                                engineInterface->PixelFree( srcTexels );

                                srcLayer.texels = nullptr;
                            }

                            // Store the new layer.
                            pvrNativeImage::mipmap_t newLayer;
                            newLayer.width = dstSurfWidth;
                            newLayer.height = dstSurfHeight;

                            newLayer.layerWidth = layerWidth;
                            newLayer.layerHeight = layerHeight;

                            newLayer.texels = dstTexels;
                            newLayer.dataSize = dstDataSize;

                            convLayers[ n ] = std::move( newLayer );
                        }
                    }
                    catch( ... )
                    {
                        genmip::deleteMipmapLayers( engineInterface, convLayers );

                        throw;
                    }

                    // Replace the layers now.
                    tmpMipmapLayers = std::move( convLayers );

                    areLayersNewlyAllocated = true;

                    useColorLayers = &tmpMipmapLayers;
                }
                else
                {
                    // In terms of alpha value calculation, we trust that the user picked a proper compression type.
                    // So if he picked a compression type with alpha, lets assume the thing has alpha.
                    pvr_hasAlpha = natImg->hasAlphaChannel_pvrtc;
                }
            }
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
            else
            {
                assert( 0 );
            }

            uint32 pvr_rasterType = 4;
            bool pvr_cubeMap = natImg->isCubeMap;
            bool pvr_autoMipmaps = false;

            bool hasProcessedNatTex = false;

            // Since we really need to debug some things, we just like directly push color data for now.
#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9
            if ( !hasProcessedNatTex && isDirect3D9 )
            {
                assert( wantsFrameworkInput == true );

                NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)nativeTexMem;

                d3d9AcquirePixelDataToTexture <pvrNativeImage::mipmap_t> (
                    engineInterface,
                    nativeTex,
                    (*useColorLayers),
                    frm_pvrRasterFormat, frm_pvrDepth, frm_pvrRowAlignment, frm_pvrColorOrder,
                    PALETTE_NONE, nullptr, 0, frm_pvrCompressionType,
                    pvr_rasterType, pvr_cubeMap, pvr_autoMipmaps, pvr_hasAlpha,
                    hasDirectlyAcquired
                );

                hasProcessedNatTex = true;
            }
#endif //RWLIB_INCLUDE_NATIVETEX_D3D9

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D8
            if ( !hasProcessedNatTex && isDirect3D8 )
            {
                assert( wantsFrameworkInput == true );

                NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)nativeTexMem;

                d3d8AcquirePixelDataToTexture <pvrNativeImage::mipmap_t> (
                    engineInterface,
                    nativeTex,
                    (*useColorLayers),
                    frm_pvrRasterFormat, frm_pvrDepth, frm_pvrRowAlignment, frm_pvrColorOrder,
                    PALETTE_NONE, nullptr, 0, frm_pvrCompressionType,
                    pvr_rasterType, pvr_autoMipmaps, pvr_hasAlpha,
                    hasDirectlyAcquired
                );

                hasProcessedNatTex = true;
            }
#endif //RWLIB_INCLUDE_NATIVETEX_D3D8

#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
            if ( !hasProcessedNatTex && isPowerVR )
            {
                assert( wantsPVRTC == true );

                // We directly put PVRTC compressed data into the native texture.
                // It is a simple native texture anyway, so ezpz.
                NativeTexturePVR *nativeTex = (NativeTexturePVR*)nativeTexMem;

                nativeTex->mipmaps.Resize( mipmapCount );

                for ( size_t n = 0; n < mipmapCount; n++ )
                {
                    const pvrNativeImage::mipmap_t& srcLayer = (*useColorLayers)[ n ];

                    uint32 surfWidth = srcLayer.width;
                    uint32 surfHeight = srcLayer.height;

                    uint32 layerWidth = srcLayer.layerWidth;
                    uint32 layerHeight = srcLayer.layerHeight;

                    void *mipTexels = srcLayer.texels;
                    uint32 mipDataSize = srcLayer.dataSize;

                    // We just directly put the mipmaps into there.
                    NativeTexturePVR::mipmapLayer newLayer;
                    newLayer.width = surfWidth;
                    newLayer.height = surfHeight;

                    newLayer.layerWidth = srcLayer.layerHeight;
                    newLayer.layerHeight = srcLayer.layerHeight;

                    newLayer.texels = mipTexels;
                    newLayer.dataSize = mipDataSize;

                    nativeTex->mipmaps[ n ] = std::move( newLayer );
                }

                // Configure the format.
                nativeTex->internalFormat = pvrtc_comprType;
                nativeTex->hasAlpha = pvr_hasAlpha;

                // Clear some unknown stuff.
                nativeTex->unk1 = 0;
                nativeTex->unk8 = 0;

                // We directly acquired what we have gotten from the runtime.
                hasDirectlyAcquired = true;

                hasProcessedNatTex = true;
            }
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE

            if ( !hasProcessedNatTex )
            {
                assert( 0 );
            }
        }
        catch( ... )
        {
            // We have to release temporary data.
            if ( areLayersNewlyAllocated )
            {
                genmip::deleteMipmapLayers( engineInterface, tmpMipmapLayers );
            }

            throw;
        }

        // If we had temporary layers and they were not taken by the native textures,
        // we have to release their memory.
        if ( areLayersNewlyAllocated && hasDirectlyAcquired == false )
        {
            genmip::deleteMipmapLayers( engineInterface, tmpMipmapLayers );
        }

        // If the mipmaps were not taken, then we need to clear them.
        feedbackOut.hasDirectlyAcquired = ( hasFormatDirectMapping && hasDirectlyAcquired );
        feedbackOut.hasDirectlyAcquiredPalette = true;  // we do not support palette.
    }

    template <typename structType>
    static inline bool readStreamStruct( Stream *stream, structType& structOut )
    {
        size_t readCount = stream->read( &structOut, sizeof( structType ) );

        return ( readCount == sizeof( structType ) );
    }

    static inline bool readLegacyVersionHeader(
        Stream *inputStream,
        uint32& widthOut, uint32& heightOut,
        uint32& mipmapCountOut,
        pvr_legacy_formatField& formatFieldOut,
        uint32& surfaceSizeOut,
        uint32& bitsPerPixelOut,
        uint32& redMaskOut, uint32& greenMaskOut, uint32& blueMaskOut, uint32& alphaMaskOut,
        bool& isLittleEndianOut
    )
    {
        union header_data_union
        {
            inline header_data_union( void ) : header_size_data()
            {}

            char header_size_data[4];
            endian::p_little_endian <uint32> le_header_size;
            endian::p_big_endian <uint32> be_header_size;
        };

        header_data_union header_data;

        if ( !readStreamStruct( inputStream, header_data.header_size_data ) )
        {
            return false;
        }

        // Try little endian first.
        {
            uint32 header_size = header_data.le_header_size;

            if ( header_size == 44 )
            {
                pvr_header_ver1 <endian::p_little_endian> header;

                if ( !readStreamStruct( inputStream, header ) )
                {
                    return false;
                }

                widthOut = header.width;
                heightOut = header.height;
                mipmapCountOut = header.mipmapCount;
                formatFieldOut = header.flags;
                surfaceSizeOut = header.surfaceSize;
                bitsPerPixelOut = header.bitsPerPixel;
                redMaskOut = header.redMask;
                greenMaskOut = header.greenMask;
                blueMaskOut = header.blueMask;
                alphaMaskOut = header.alphaMask;

                isLittleEndianOut = true;
                return true;
            }
            else if ( header_size == 52 )
            {
                pvr_header_ver2 <endian::p_little_endian> header;

                if ( !readStreamStruct( inputStream, header ) )
                {
                    return false;
                }

                // Verify PVR id.
                if ( header.pvr_id != 0x21525650 )
                {
                    return false;
                }

                widthOut = header.width;
                heightOut = header.height;
                mipmapCountOut = header.mipmapCount;
                formatFieldOut = header.flags;
                surfaceSizeOut = header.surfaceSize;
                bitsPerPixelOut = header.bitsPerPixel;
                redMaskOut = header.redMask;
                greenMaskOut = header.greenMask;
                blueMaskOut = header.blueMask;
                alphaMaskOut = header.alphaMask;

                // TODO: verify PVR ID

                isLittleEndianOut = true;
                return true;
            }
        }

        // Now do big endian.
        {
            uint32 header_size = header_data.be_header_size;

            if ( header_size == 44 )
            {
                pvr_header_ver1 <endian::big_endian> header;

                if ( !readStreamStruct( inputStream, header ) )
                {
                    return false;
                }

                widthOut = header.width;
                heightOut = header.height;
                mipmapCountOut = header.mipmapCount;
                formatFieldOut = header.flags;
                surfaceSizeOut = header.surfaceSize;
                bitsPerPixelOut = header.bitsPerPixel;
                redMaskOut = header.redMask;
                greenMaskOut = header.greenMask;
                blueMaskOut = header.blueMask;
                alphaMaskOut = header.alphaMask;

                isLittleEndianOut = false;
                return true;
            }
            else if ( header_size == 52 )
            {
                pvr_header_ver2 <endian::big_endian> header;

                if ( !readStreamStruct( inputStream, header ) )
                {
                    return false;
                }

                // Verify PVR id.
                if ( header.pvr_id != 0x21525650 )
                {
                    return false;
                }

                widthOut = header.width;
                heightOut = header.height;
                mipmapCountOut = header.mipmapCount;
                formatFieldOut = header.flags;
                surfaceSizeOut = header.surfaceSize;
                bitsPerPixelOut = header.bitsPerPixel;
                redMaskOut = header.redMask;
                greenMaskOut = header.greenMask;
                blueMaskOut = header.blueMask;
                alphaMaskOut = header.alphaMask;

                // TODO: verify PVR ID

                isLittleEndianOut = false;
                return true;
            }
        }

        // Could not find a proper header (legacy).
        return false;
    }

    static inline bool isValidPVRLegacyPixelFormat( ePVRLegacyPixelFormat pixelFormat )
    {
        if ( pixelFormat != ePVRLegacyPixelFormat::ARGB_4444 &&
             pixelFormat != ePVRLegacyPixelFormat::ARGB_1555 &&
             pixelFormat != ePVRLegacyPixelFormat::RGB_565 &&
             pixelFormat != ePVRLegacyPixelFormat::RGB_555 &&
             pixelFormat != ePVRLegacyPixelFormat::ARGB_8888 &&
             pixelFormat != ePVRLegacyPixelFormat::ARGB_8332 &&
             pixelFormat != ePVRLegacyPixelFormat::I8 &&
             pixelFormat != ePVRLegacyPixelFormat::AI88 &&
             pixelFormat != ePVRLegacyPixelFormat::MONOCHROME &&
             pixelFormat != ePVRLegacyPixelFormat::V_Y1_U_Y0 &&
             pixelFormat != ePVRLegacyPixelFormat::Y1_V_Y0_U &&
             pixelFormat != ePVRLegacyPixelFormat::PVRTC2 &&
             pixelFormat != ePVRLegacyPixelFormat::PVRTC4 &&
             pixelFormat != ePVRLegacyPixelFormat::ARGB_4444_SEC &&
             pixelFormat != ePVRLegacyPixelFormat::ARGB_1555_SEC &&
             pixelFormat != ePVRLegacyPixelFormat::ARGB_8888_SEC &&
             pixelFormat != ePVRLegacyPixelFormat::RGB_565_SEC &&
             pixelFormat != ePVRLegacyPixelFormat::RGB_555_SEC &&
             pixelFormat != ePVRLegacyPixelFormat::RGB_888_SEC &&
             pixelFormat != ePVRLegacyPixelFormat::I8_SEC &&
             pixelFormat != ePVRLegacyPixelFormat::AI88_SEC &&
             pixelFormat != ePVRLegacyPixelFormat::PVRTC2_SEC &&
             pixelFormat != ePVRLegacyPixelFormat::PVRTC4_SEC &&
             pixelFormat != ePVRLegacyPixelFormat::BGRA_8888 &&
             pixelFormat != ePVRLegacyPixelFormat::DXT1 &&
             pixelFormat != ePVRLegacyPixelFormat::DXT2 &&
             pixelFormat != ePVRLegacyPixelFormat::DXT3 &&
             pixelFormat != ePVRLegacyPixelFormat::DXT4 &&
             pixelFormat != ePVRLegacyPixelFormat::DXT5 &&
             pixelFormat != ePVRLegacyPixelFormat::RGB332 &&
             pixelFormat != ePVRLegacyPixelFormat::AL_44 &&
             pixelFormat != ePVRLegacyPixelFormat::LVU_655 &&
             pixelFormat != ePVRLegacyPixelFormat::XLVU_8888 &&
             pixelFormat != ePVRLegacyPixelFormat::QWVU_8888 &&
             pixelFormat != ePVRLegacyPixelFormat::ABGR_2101010 &&
             pixelFormat != ePVRLegacyPixelFormat::ARGB_2101010 &&
             pixelFormat != ePVRLegacyPixelFormat::AWVU_2101010 &&
             pixelFormat != ePVRLegacyPixelFormat::GR_1616 &&
             pixelFormat != ePVRLegacyPixelFormat::VU_1616 &&
             pixelFormat != ePVRLegacyPixelFormat::ABGR_16161616 &&
             pixelFormat != ePVRLegacyPixelFormat::R_16F &&
             pixelFormat != ePVRLegacyPixelFormat::GR_1616F &&
             pixelFormat != ePVRLegacyPixelFormat::ABGR_16161616F &&
             pixelFormat != ePVRLegacyPixelFormat::R_32F &&
             pixelFormat != ePVRLegacyPixelFormat::GR_3232F &&
             pixelFormat != ePVRLegacyPixelFormat::ABGR_32323232F &&
             pixelFormat != ePVRLegacyPixelFormat::ETC &&
             pixelFormat != ePVRLegacyPixelFormat::A8 &&
             pixelFormat != ePVRLegacyPixelFormat::VU_88 &&
             pixelFormat != ePVRLegacyPixelFormat::L16 &&
             pixelFormat != ePVRLegacyPixelFormat::L8 &&
             pixelFormat != ePVRLegacyPixelFormat::AL_88 &&
             pixelFormat != ePVRLegacyPixelFormat::UYVY &&
             pixelFormat != ePVRLegacyPixelFormat::YUY2 )
        {
            return false;
        }

        return true;
    }

    bool IsStreamNativeImage( Interface *engineInterface, Stream *inputStream ) const override
    {
        // Try to read some shitty PVR files.
        // We need to support both ver1 and ver2.

        uint32 width, height;
        uint32 mipmapCount;
        pvr_legacy_formatField formatField;
        uint32 surfaceSize;
        uint32 bitsPerPixel;
        uint32 redMask;
        uint32 blueMask;
        uint32 greenMask;
        uint32 alphaMask;

        bool isLittleEndian;

        bool hasLegacyFormatHeader =
            readLegacyVersionHeader(
                inputStream,
                width, height,
                mipmapCount,
                formatField,
                surfaceSize,
                bitsPerPixel,
                redMask, blueMask, greenMask, alphaMask,
                isLittleEndian
            );

        if ( !hasLegacyFormatHeader )
        {
            // We now have no support for non-legacy formats.
            return false;
        }

        // In the legacy format, the mipmapCount excludes the main surface.
        mipmapCount++;

        // Make sure we got a valid format.
        // There cannot be more formats than were specified.
        ePVRLegacyPixelFormat pixelFormat = (ePVRLegacyPixelFormat)formatField.pixelFormat;

        if ( !isValidPVRLegacyPixelFormat( pixelFormat ) )
        {
            return false;
        }

        // Determine the pixel format and what the properties mean to us.
        // Unfortunately, we are not going to be able to support each pixel format thrown at us.
        // This is because things like UVWA require special interpretation, quite frankly
        // cannot be mapped to general color data if there is not a perfect match.

        uint32 format_bitDepth = getPVRLegacyFormatDepth( pixelFormat );

        assert( format_bitDepth != 0 );

        // Verify that all color layers are present.
        mipGenLevelGenerator mipGen( width, height );

        if ( mipGen.isValidLevel() == false )
        {
            return false;
        }

        uint32 mip_index = 0;

        while ( mip_index < mipmapCount )
        {
            bool didEstablishLevel = true;

            if ( mip_index != 0 )
            {
                didEstablishLevel = mipGen.incrementLevel();
            }

            if ( !didEstablishLevel )
            {
                break;
            }

            // Get the data linear size, since we always can.
            uint32 mipLayerWidth = mipGen.getLevelWidth();
            uint32 mipLayerHeight = mipGen.getLevelHeight();

            // For we need the surface dimensions.
            uint32 mipSurfWidth, mipSurfHeight;

            getPVRLegacyFormatSurfaceDimensions( pixelFormat, mipLayerWidth, mipLayerHeight, mipSurfWidth, mipSurfHeight );

            // So now for the calculation part.
            rasterRowSize texRowSize = getPVRNativeImageRasterDataRowSize( mipSurfWidth, format_bitDepth );

            uint32 texDataSize = getRasterDataSizeByRowSize( texRowSize, mipSurfHeight );

            skipAvailable( inputStream, texDataSize );

            // Next level.
            mip_index++;
        }

        // We are a valid PVR!
        return true;
    }

    void ReadNativeImage( Interface *engineInterface, void *imageMem, Stream *inputStream ) const override
    {
        // Let's read those suckers.

        uint32 width, height;
        uint32 mipmapCount;
        pvr_legacy_formatField formatField;
        uint32 surfaceSize;
        uint32 bitsPerPixel;
        uint32 redMask;
        uint32 blueMask;
        uint32 greenMask;
        uint32 alphaMask;

        bool isLittleEndian;

        bool hasLegacyFormatHeader =
            readLegacyVersionHeader(
                inputStream,
                width, height,
                mipmapCount,
                formatField,
                surfaceSize,
                bitsPerPixel,
                redMask, blueMask, greenMask, alphaMask,
                isLittleEndian
            );

        if ( !hasLegacyFormatHeader )
        {
            // We now have no support for non-legacy formats.
            throw NativeImagingStructuralErrorException( "PVR", L"PVR_STRUCTERR_HEADERFAIL" );
        }

        // In the legacy format, the mipmapCount excludes the main surface.
        mipmapCount++;

        // Verify properties of the image file.
        // Make sure we got a valid format.
        // There cannot be more formats than were specified.
        ePVRLegacyPixelFormat pixelFormat = (ePVRLegacyPixelFormat)formatField.pixelFormat;

        if ( !isValidPVRLegacyPixelFormat( pixelFormat ) )
        {
            throw NativeImagingStructuralErrorException( "PVR", L"PVR_STRUCTERR_PIXELFMT" );
        }

        uint32 format_bitDepth = getPVRLegacyFormatDepth( pixelFormat );

        // Verify bit depth.
        if ( bitsPerPixel != format_bitDepth )
        {
            engineInterface->PushWarningToken( L"PVR_WARN_INVALID_BITSPERPIXEL" );
        }

        // We do not support certain image files for now.
        if ( formatField.isCubeMap )
        {
            throw NativeImagingInternalErrorException( "PVR", L"PVR_INTERNERR_NOCUBEMAP" );
        }

        if ( formatField.isVolumeTexture )
        {
            throw NativeImagingInternalErrorException( "PVR", L"PVR_INTERNERR_NOVOLUMETEX" );
        }

        if ( formatField.isVerticallyFlipped )
        {
            throw NativeImagingInternalErrorException( "PVR", L"PVR_INTERNERR_NOVERTFLIP" );
        }

        // If the native image says that it is twiddled, it must follow HARD RULES.
        // * width and height must be POWER-OF-TWO and SQUARE.
        bool isTwiddled = formatField.dataIsTwiddled;

        if ( isTwiddled )
        {
            nativeTextureSizeRules sizeRules;
            sizeRules.powerOfTwo = true;
            sizeRules.squared = true;

            if ( !sizeRules.IsMipmapSizeValid( width, height ) )
            {
                throw NativeImagingStructuralErrorException( "PVR", L"PVR_STRUCTERR_TWIDDLEFAIL" );
            }
        }

        // We do not support certain twiddling configurations.
        ePVRLegacyPixelFormatType colorFormatType = getPVRLegacyPixelFormatType( pixelFormat );

        if ( isTwiddled )
        {
            // Compressed and twiddled has no meaning, so we ignore it.
            if ( colorFormatType != ePVRLegacyPixelFormatType::COMPRESSED )
            {
                throw NativeImagingInternalErrorException( "PVR", L"PVR_INTERNERR_NOTWIDDLE" );
            }
        }

        // Time to store some properties.
        pvrNativeImage *natImg = (pvrNativeImage*)imageMem;

        natImg->pixelFormat = pixelFormat;
        natImg->dataIsTwiddled = isTwiddled;
        natImg->containsNormalData = formatField.containsNormalData;
        natImg->hasBorder = formatField.hasBorder;
        natImg->isCubeMap = false;              // TODO
        natImg->mipmapsHaveDebugColoring = formatField.mipmapsHaveDebugColoring;
        natImg->isVolumeTexture = false;        // TODO
        natImg->hasAlphaChannel_pvrtc = formatField.hasAlphaChannel_pvrtc;
        natImg->isVerticallyFlipped = formatField.isVerticallyFlipped;

        // Store cached properties.
        natImg->bitDepth = format_bitDepth;

        // And meta-properties.
        natImg->isLittleEndian = isLittleEndian;

        // Turns out the guys at Imagination do not care about the color bitmasks.
        // So we do not care either.

        // Read the color data now.
        mipGenLevelGenerator mipGen( width, height );

        if ( !mipGen.isValidLevel() )
        {
            throw NativeImagingStructuralErrorException( "PVR", L"PVR_STRUCTERR_MIPDIMS" );
        }

        // We want to read only as much surface data as the image tells us is available.
        uint32 remaining_surfDataSize = surfaceSize;

        uint32 mip_index = 0;

        while ( mip_index < mipmapCount )
        {
            bool didEstablishLevel = true;

            if ( mip_index != 0 )
            {
                didEstablishLevel = mipGen.incrementLevel();
            }

            if ( !didEstablishLevel )
            {
                // We are prematurely finished.
                break;
            }

            // Actually get the mipmap properties and store the data now.
            uint32 mipLayerWidth = mipGen.getLevelWidth();
            uint32 mipLayerHeight = mipGen.getLevelHeight();

            uint32 mipSurfWidth, mipSurfHeight;
            getPVRLegacyFormatSurfaceDimensions( pixelFormat, mipLayerWidth, mipLayerHeight, mipSurfWidth, mipSurfHeight );

            // NOTE: even though there is no row-size for each PVR native image pixel format (e.g. compressed), this style
            // of calculating the linear size if perfectly compatible.
            rasterRowSize texRowSize = getPVRNativeImageRasterDataRowSize( mipSurfWidth, format_bitDepth );

            uint32 texDataSize = getRasterDataSizeByRowSize( texRowSize, mipSurfHeight );

            // Check if we can read this layer even.
            if ( remaining_surfDataSize < texDataSize )
            {
                throw NativeImagingStructuralErrorException( "PVR", L"PVR_STRUCTERR_TRUNCATEDMIPDATA" );
            }

            remaining_surfDataSize -= texDataSize;

            // Check if we even have the data in the stream.
            checkAhead( inputStream, texDataSize );

            void *mipTexels = engineInterface->PixelAllocate( texDataSize );

            try
            {
                // Read the stuff.
                size_t readCount = inputStream->read( mipTexels, texDataSize );

                if ( readCount != texDataSize )
                {
                    throw NativeImagingStructuralErrorException( "PVR", L"PVR_STRUCTERR_MIPDATAREAD" );
                }

                // Store our surface.
                pvrNativeImage::mipmap_t newLayer;
                newLayer.width = mipSurfWidth;
                newLayer.height = mipSurfHeight;

                newLayer.layerWidth = mipLayerWidth;
                newLayer.layerHeight = mipLayerHeight;

                newLayer.texels = mipTexels;
                newLayer.dataSize = texDataSize;

                natImg->mipmaps.AddToBack( std::move( newLayer ) );
            }
            catch( ... )
            {
                // We kinda failed, so clear data.
                engineInterface->PixelFree( mipTexels );

                throw;
            }

            // Next level.
            mip_index++;
        }

        if ( mip_index != mipmapCount )
        {
            engineInterface->PushWarningToken( L"PVR_WARN_MIPCOUNTPARSEMISMATCH" );
        }

        // Check that we read all surface data.
        if ( remaining_surfDataSize != 0 )
        {
            engineInterface->PushWarningToken( L"PVR_WARN_REMAININGUNPARSEDSURFDATA" );

            // Skip those bytes.
            inputStream->skip( remaining_surfDataSize );
        }

        // Finito. :)
    }

    void WriteNativeImage( Interface *engineInterface, const void *imageMem, Stream *outputStream ) const override
    {
        // What we have read, and verified, we can easily write back.
        // PVR is a really weird format anyway.

        pvrNativeImage *natImg = (pvrNativeImage*)imageMem;

        // We actually want to support writing either, little endian and big endian.
        bool isLittleEndian = natImg->isLittleEndian;

        size_t mipmapCount = natImg->mipmaps.GetCount();

        if ( mipmapCount == 0 )
        {
            throw NativeImagingInternalErrorException( "PVR", L"PVR_INTERNERR_EMPTYWRITE" );
        }

        // Prepare the format field.
        pvr_legacy_formatField formatField;
        formatField.pixelFormat = (uint8)natImg->pixelFormat;
        formatField.mipmapsPresent = ( mipmapCount > 1 );
        formatField.dataIsTwiddled = natImg->dataIsTwiddled;
        formatField.containsNormalData = natImg->containsNormalData;
        formatField.hasBorder = natImg->hasBorder;
        formatField.isCubeMap = natImg->isCubeMap;
        formatField.mipmapsHaveDebugColoring = natImg->mipmapsHaveDebugColoring;
        formatField.isVolumeTexture = natImg->isVolumeTexture;
        formatField.hasAlphaChannel_pvrtc = natImg->hasAlphaChannel_pvrtc;
        formatField.isVerticallyFlipped = natImg->isVerticallyFlipped;
        formatField.pad = 0;

        // Calculate the accumulated surface size.
        uint32 totalSurfaceSize = 0;

        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            const pvrNativeImage::mipmap_t& srcLayer = natImg->mipmaps[ n ];

            totalSurfaceSize += srcLayer.dataSize;
        }

        // I guess we should always be writing version two legacy files, if on point.
        // Those file formats are considered legacy already, geez...
        uint32 ver2_headerSize = 52;

        // Need the base layer.
        const pvrNativeImage::mipmap_t& baseLayer = natImg->mipmaps[ 0 ];

        if ( isLittleEndian )
        {
            // First write the header size.
            endian::little_endian <uint32> header_size = ver2_headerSize;

            outputStream->write( &header_size, sizeof( header_size ) );

            pvr_header_ver2 <endian::p_little_endian> header;
            header.height = baseLayer.layerHeight;
            header.width = baseLayer.layerWidth;
            header.mipmapCount = (uint32)( mipmapCount - 1 );
            header.flags = formatField;
            header.surfaceSize = totalSurfaceSize;
            header.bitsPerPixel = natImg->bitDepth;
            header.redMask = 0; // nobody cares, even ImgTec doesnt.
            header.greenMask = 0;
            header.blueMask = 0;
            header.alphaMask = 0;
            header.pvr_id = 0x21525650;
            header.numberOfSurfaces = 1;

            outputStream->write( &header, sizeof( header ) );
        }
        else
        {
            // First write the header size.
            endian::big_endian <uint32> header_size = ver2_headerSize;

            outputStream->write( &header_size, sizeof( header_size ) );

            pvr_header_ver2 <endian::big_endian> header;
            header.height = baseLayer.layerHeight;
            header.width = baseLayer.layerWidth;
            header.mipmapCount = (uint32)( mipmapCount - 1 );
            header.flags = formatField;
            header.surfaceSize = totalSurfaceSize;
            header.bitsPerPixel = natImg->bitDepth;
            header.redMask = 0; // nobody cares, even ImgTec doesnt.
            header.greenMask = 0;
            header.blueMask = 0;
            header.alphaMask = 0;
            header.pvr_id = 0x21525650;
            header.numberOfSurfaces = 1;

            outputStream->write( &header, sizeof( header ) );
        }

        // Now write the image data.
        // As you may have noticed the PVR native image has no palette support.
        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            const pvrNativeImage::mipmap_t& mipLayer = natImg->mipmaps[ n ];

            uint32 mipDataSize = mipLayer.dataSize;

            const void *mipTexels = mipLayer.texels;

            outputStream->write( mipTexels, mipDataSize );
        }

        // Done.
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        RegisterNativeImageType(
            engineInterface,
            this,
            "PVR", sizeof( pvrNativeImage ), alignof( pvrNativeImage ), "PowerVR Image",
            pvr_natimg_fileExt, countof( pvr_natimg_fileExt ),
            pvr_natimg_suppnattex, countof( pvr_natimg_suppnattex )
        );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        UnregisterNativeImageType( engineInterface, "PVR" );
    }
};

static optional_struct_space <PluginDependantStructRegister <pvrNativeImageTypeManager, RwInterfaceFactory_t>> pvrNativeImageTypeManagerRegister;

void registerPVRNativeImageTypeEnv( void )
{
    pvrNativeImageTypeManagerRegister.Construct( engineFactory );
}

void unregisterPVRNativeImageTypeEnv( void )
{
    pvrNativeImageTypeManagerRegister.Destroy();
}

};

#endif //RWLIB_INCLUDE_PVR_NATIVEIMG
