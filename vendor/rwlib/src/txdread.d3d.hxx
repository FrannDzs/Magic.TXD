/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.d3d.hxx
*  PURPOSE:     Direct3D-based texture helpers
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _SHARED_DIRECT3D_LEGACY_NATIVETEX_INCLUDE_
#define _SHARED_DIRECT3D_LEGACY_NATIVETEX_INCLUDE_

#include "txdread.nativetex.hxx"
#include "txdread.common.hxx"

// Important definitions from the D3D9 SDK.

/* Formats
 * Most of these names have the following convention:
 *      A = Alpha
 *      R = Red
 *      G = Green
 *      B = Blue
 *      X = Unused Bits
 *      P = Palette
 *      L = Luminance
 *      U = dU coordinate for BumpMap
 *      V = dV coordinate for BumpMap
 *      S = Stencil
 *      D = Depth (e.g. Z or W buffer)
 *      C = Computed from other channels (typically on certain read operations)
 *
 *      Further, the order of the pieces are from MSB first; hence
 *      D3DFMT_A8L8 indicates that the high byte of this two byte
 *      format is alpha.
 *
 *      D3DFMT_D16_LOCKABLE indicates:
 *           - An integer 16-bit value.
 *           - An app-lockable surface.
 *
 *      D3DFMT_D32F_LOCKABLE indicates:
 *           - An IEEE 754 floating-point value.
 *           - An app-lockable surface.
 *
 *      All Depth/Stencil formats except D3DFMT_D16_LOCKABLE and D3DFMT_D32F_LOCKABLE indicate:
 *          - no particular bit ordering per pixel, and
 *          - are not app lockable, and
 *          - the driver is allowed to consume more than the indicated
 *            number of bits per Depth channel (but not Stencil channel).
 */
#define MAKEFOURCC_RW(ch0, ch1, ch2, ch3)                                                   \
            ((rw::uint32)(rw::uint8)(ch0) | ((rw::uint32)(rw::uint8)(ch1) << 8) |           \
            ((rw::uint32)(rw::uint8)(ch2) << 16) | ((rw::uint32)(rw::uint8)(ch3) << 24 ))

enum D3DFORMAT : rw::uint32
{
    D3DFMT_UNKNOWN              =  0,

    D3DFMT_R8G8B8               = 20,
    D3DFMT_A8R8G8B8             = 21,
    D3DFMT_X8R8G8B8             = 22,
    D3DFMT_R5G6B5               = 23,
    D3DFMT_X1R5G5B5             = 24,
    D3DFMT_A1R5G5B5             = 25,
    D3DFMT_A4R4G4B4             = 26,
    D3DFMT_R3G3B2               = 27,
    D3DFMT_A8                   = 28,
    D3DFMT_A8R3G3B2             = 29,
    D3DFMT_X4R4G4B4             = 30,
    D3DFMT_A2B10G10R10          = 31,
    D3DFMT_A8B8G8R8             = 32,
    D3DFMT_X8B8G8R8             = 33,
    D3DFMT_G16R16               = 34,
    D3DFMT_A2R10G10B10          = 35,
    D3DFMT_A16B16G16R16         = 36,

    D3DFMT_A8P8                 = 40,
    D3DFMT_P8                   = 41,

    D3DFMT_L8                   = 50,
    D3DFMT_A8L8                 = 51,
    D3DFMT_A4L4                 = 52,

    D3DFMT_V8U8                 = 60,
    D3DFMT_L6V5U5               = 61,
    D3DFMT_X8L8V8U8             = 62,
    D3DFMT_Q8W8V8U8             = 63,
    D3DFMT_V16U16               = 64,
    D3DFMT_A2W10V10U10          = 67,

    D3DFMT_UYVY                 = MAKEFOURCC_RW('U', 'Y', 'V', 'Y'),
    D3DFMT_R8G8_B8G8            = MAKEFOURCC_RW('R', 'G', 'B', 'G'),
    D3DFMT_YUY2                 = MAKEFOURCC_RW('Y', 'U', 'Y', '2'),
    D3DFMT_G8R8_G8B8            = MAKEFOURCC_RW('G', 'R', 'G', 'B'),
    D3DFMT_DXT1                 = MAKEFOURCC_RW('D', 'X', 'T', '1'),
    D3DFMT_DXT2                 = MAKEFOURCC_RW('D', 'X', 'T', '2'),
    D3DFMT_DXT3                 = MAKEFOURCC_RW('D', 'X', 'T', '3'),
    D3DFMT_DXT4                 = MAKEFOURCC_RW('D', 'X', 'T', '4'),
    D3DFMT_DXT5                 = MAKEFOURCC_RW('D', 'X', 'T', '5'),

    D3DFMT_D16_LOCKABLE         = 70,
    D3DFMT_D32                  = 71,
    D3DFMT_D15S1                = 73,
    D3DFMT_D24S8                = 75,
    D3DFMT_D24X8                = 77,
    D3DFMT_D24X4S4              = 79,
    D3DFMT_D16                  = 80,

    D3DFMT_D32F_LOCKABLE        = 82,
    D3DFMT_D24FS8               = 83,

    D3DFMT_L16                  = 81,

    D3DFMT_VERTEXDATA           =100,
    D3DFMT_INDEX16              =101,
    D3DFMT_INDEX32              =102,

    D3DFMT_Q16W16V16U16         =110,

    D3DFMT_MULTI2_ARGB8         = MAKEFOURCC_RW('M','E','T','1'),

    // Floating point surface formats

    // s10e5 formats (16-bits per channel)
    D3DFMT_R16F                 = 111,
    D3DFMT_G16R16F              = 112,
    D3DFMT_A16B16G16R16F        = 113,

    // IEEE s23e8 formats (32-bits per channel)
    D3DFMT_R32F                 = 114,
    D3DFMT_G32R32F              = 115,
    D3DFMT_A32B32G32R32F        = 116,

    D3DFMT_CxV8U8               = 117
};

#include "txdread.d3d.genmip.hxx"

namespace rw
{

// We ought to know about those bit depths.
inline bool getD3DFORMATBitDepth( D3DFORMAT dwFourCC, uint32& depthOut )
{
    switch( dwFourCC )
    {
    case D3DFMT_R8G8B8:             depthOut = 24; break;
    case D3DFMT_A8R8G8B8:           depthOut = 32; break;
    case D3DFMT_X8R8G8B8:           depthOut = 32; break;
    case D3DFMT_R5G6B5:             depthOut = 16; break;
    case D3DFMT_X1R5G5B5:           depthOut = 16; break;
    case D3DFMT_A1R5G5B5:           depthOut = 16; break;
    case D3DFMT_A4R4G4B4:           depthOut = 16; break;
    case D3DFMT_R3G3B2:             depthOut = 8; break;
    case D3DFMT_A8:                 depthOut = 8; break;
    case D3DFMT_A8R3G3B2:           depthOut = 16; break;
    case D3DFMT_X4R4G4B4:           depthOut = 16; break;
    case D3DFMT_A2B10G10R10:        depthOut = 32; break;
    case D3DFMT_A8B8G8R8:           depthOut = 32; break;
    case D3DFMT_X8B8G8R8:           depthOut = 32; break;
    case D3DFMT_G16R16:             depthOut = 32; break;
    case D3DFMT_A2R10G10B10:        depthOut = 32; break;
    case D3DFMT_A16B16G16R16:       depthOut = 64; break;
    case D3DFMT_A8P8:               depthOut = 16; break;
    case D3DFMT_P8:                 depthOut = 8; break;
    case D3DFMT_L8:                 depthOut = 8; break;
    case D3DFMT_A8L8:               depthOut = 16; break;
    case D3DFMT_A4L4:               depthOut = 8; break;
    case D3DFMT_V8U8:               depthOut = 16; break;
    case D3DFMT_L6V5U5:             depthOut = 16; break;
    case D3DFMT_X8L8V8U8:           depthOut = 32; break;
    case D3DFMT_Q8W8V8U8:           depthOut = 32; break;
    case D3DFMT_V16U16:             depthOut = 32; break;
    case D3DFMT_A2W10V10U10:        depthOut = 32; break;
    case D3DFMT_UYVY:               depthOut = 8; break;
    case D3DFMT_R8G8_B8G8:          depthOut = 16; break;
    case D3DFMT_YUY2:               depthOut = 8; break;
    case D3DFMT_G8R8_G8B8:          depthOut = 16; break;
    case D3DFMT_DXT1:               depthOut = 4; break;
    case D3DFMT_DXT2:               depthOut = 8; break;
    case D3DFMT_DXT3:               depthOut = 8; break;
    case D3DFMT_DXT4:               depthOut = 8; break;
    case D3DFMT_DXT5:               depthOut = 8; break;
    case D3DFMT_D16_LOCKABLE:       depthOut = 16; break;
    case D3DFMT_D32:                depthOut = 32; break;
    case D3DFMT_D15S1:              depthOut = 16; break;
    case D3DFMT_D24S8:              depthOut = 32; break;
    case D3DFMT_D24X8:              depthOut = 32; break;
    case D3DFMT_D24X4S4:            depthOut = 32; break;
    case D3DFMT_D16:                depthOut = 16; break;
    case D3DFMT_D32F_LOCKABLE:      depthOut = 32; break;
    case D3DFMT_D24FS8:             depthOut = 32; break;
    case D3DFMT_L16:                depthOut = 16; break;
    case D3DFMT_INDEX16:            depthOut = 16; break;
    case D3DFMT_INDEX32:            depthOut = 32; break;
    case D3DFMT_Q16W16V16U16:       depthOut = 64; break;
    case D3DFMT_R16F:               depthOut = 16; break;
    case D3DFMT_G16R16F:            depthOut = 32; break;
    case D3DFMT_A16B16G16R16F:      depthOut = 64; break;
    case D3DFMT_R32F:               depthOut = 32; break;
    case D3DFMT_G32R32F:            depthOut = 64; break;
    case D3DFMT_A32B32G32R32F:      depthOut = 128; break;
    case D3DFMT_CxV8U8:             depthOut = 16; break;
    default: return false;
    }

    return true;
}

inline eCompressionType getFrameworkCompressionTypeFromD3DFORMAT( D3DFORMAT fmt )
{
    if ( fmt == D3DFMT_DXT1 )
    {
        return RWCOMPRESS_DXT1;
    }
    else if ( fmt == D3DFMT_DXT2 )
    {
        return RWCOMPRESS_DXT2;
    }
    else if ( fmt == D3DFMT_DXT3 )
    {
        return RWCOMPRESS_DXT3;
    }
    else if ( fmt == D3DFMT_DXT4 )
    {
        return RWCOMPRESS_DXT4;
    }
    else if ( fmt == D3DFMT_DXT5 )
    {
        return RWCOMPRESS_DXT5;
    }

    // TODO: if there is any other compression we support, update this.
    return RWCOMPRESS_NONE;
}

inline bool getRasterFormatFromD3DFormat(
    D3DFORMAT d3dFormat, bool canHaveAlpha,
    eRasterFormat& rasterFormatOut, eColorOrdering& colorOrderOut, bool& isVirtualRasterFormatOut
)
{
    bool isValidFormat = false;
    bool isVirtualRasterFormat = false;

    if (d3dFormat == D3DFMT_A8R8G8B8)
    {
        rasterFormatOut = RASTER_8888;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_X8R8G8B8)
    {
        rasterFormatOut = RASTER_888;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_R8G8B8)
    {
        rasterFormatOut = RASTER_888;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_R5G6B5)
    {
        rasterFormatOut = RASTER_565;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_X1R5G5B5)
    {
        rasterFormatOut = RASTER_555;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_A1R5G5B5)
    {
        rasterFormatOut = RASTER_1555;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_A4R4G4B4)
    {
        rasterFormatOut = RASTER_4444;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_A8B8G8R8)
    {
        rasterFormatOut = RASTER_8888;

        colorOrderOut = COLOR_RGBA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_X8B8G8R8)
    {
        rasterFormatOut = RASTER_888;

        colorOrderOut = COLOR_RGBA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_L8)
    {
        rasterFormatOut = RASTER_LUM;

        // Actually, there is no such thing as a color order for luminance textures.
        // We set this field so we make things happy.
        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_A4L4 ||
             d3dFormat == D3DFMT_A8L8)
    {
        rasterFormatOut = RASTER_LUM_ALPHA;

        // See above.
        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_DXT1)
    {
        rasterFormatOut = getVirtualRasterFormat( canHaveAlpha, RWCOMPRESS_DXT1 );

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;

        isVirtualRasterFormat = true;
    }
    else if (d3dFormat == D3DFMT_DXT2 || d3dFormat == D3DFMT_DXT3)
    {
        const eCompressionType comprType = ( d3dFormat == D3DFMT_DXT2 ? RWCOMPRESS_DXT2 : RWCOMPRESS_DXT3 );

        rasterFormatOut = getVirtualRasterFormat( canHaveAlpha, comprType );

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;

        isVirtualRasterFormat = true;
    }
    else if (d3dFormat == D3DFMT_DXT4 || d3dFormat == D3DFMT_DXT5)
    {
        const eCompressionType comprType = ( d3dFormat == D3DFMT_DXT4 ? RWCOMPRESS_DXT4 : RWCOMPRESS_DXT5 );

        rasterFormatOut = getVirtualRasterFormat( canHaveAlpha, comprType );

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;

        isVirtualRasterFormat = true;
    }
    else if (d3dFormat == D3DFMT_P8)
    {
        // We cannot be a palette texture without having actual palette data.
        isValidFormat = false;
    }
    
    if ( isValidFormat )
    {
        isVirtualRasterFormatOut = isVirtualRasterFormat;
    }

    return isValidFormat;
}

inline void sharedd3dConvertRasterFormatCompatible( eRasterFormat& rasterFormat, eColorOrdering& colorOrder, uint32& depth, D3DFORMAT& d3dFormatOut )
{
    eRasterFormat srcRasterFormat = rasterFormat;
    uint32 srcDepth = depth;
    eColorOrdering srcColorOrder = colorOrder;

    if ( srcRasterFormat == RASTER_1555 )
    {
        depth = 16;
        colorOrder = COLOR_BGRA;
        d3dFormatOut = D3DFMT_A1R5G5B5;
    }
    else if ( srcRasterFormat == RASTER_565 )
    {
        depth = 16;
        colorOrder = COLOR_BGRA;
        d3dFormatOut = D3DFMT_R5G6B5;
    }
    else if ( srcRasterFormat == RASTER_4444 )
    {
        depth = 16;
        colorOrder = COLOR_BGRA;
        d3dFormatOut = D3DFMT_A4R4G4B4;
    }
    else if ( srcRasterFormat == RASTER_8888 )
    {
        depth = 32;

        // Can be both RGBA and BGRA.
        if ( srcColorOrder == COLOR_RGBA )
        {
            d3dFormatOut = D3DFMT_A8B8G8R8;
        }
        else if ( srcColorOrder == COLOR_BGRA )
        {
            d3dFormatOut = D3DFMT_A8R8G8B8;
        }
        else
        {
            // We kinda force a color reordering here.
            colorOrder = COLOR_BGRA;
            d3dFormatOut = D3DFMT_A8R8G8B8;
        }
    }
    else if ( srcRasterFormat == RASTER_888 )
    {
        if ( srcColorOrder == COLOR_BGRA )
        {
            if ( srcDepth == 24 )
            {
                d3dFormatOut = D3DFMT_R8G8B8;
            }
            else if ( srcDepth == 32 )
            {
                d3dFormatOut = D3DFMT_X8R8G8B8;
            }
            else
            {
                // We force to adjust the depth.
                depth = 32;
                d3dFormatOut = D3DFMT_X8R8G8B8;
            }
        }
        else if ( srcColorOrder == COLOR_RGBA )
        {
            depth = 32;
            d3dFormatOut = D3DFMT_X8B8G8R8;
        }
        else
        {
            // We force expanding to standard format.
            depth = 32;
            colorOrder = COLOR_BGRA;
            d3dFormatOut = D3DFMT_X8R8G8B8;
        }
    }
    else if ( srcRasterFormat == RASTER_555 )
    {
        depth = 16;
        colorOrder = COLOR_BGRA;
        d3dFormatOut = D3DFMT_X1R5G5B5;
    }
    else if ( srcRasterFormat == RASTER_LUM )
    {
        // We only support 8bit LUM, obviously.
        depth = 8;
        d3dFormatOut = D3DFMT_L8;
    }
    // NEW formats :3
    else if ( srcRasterFormat == RASTER_LUM_ALPHA )
    {
        if ( srcDepth == 8 )
        {
            d3dFormatOut = D3DFMT_A4L4;
        }
        else if ( srcDepth == 16 )
        {
            d3dFormatOut = D3DFMT_A8L8;
        }
        else
        {
            // Be safe and convert to maximum quality.
            rasterFormat = RASTER_8888;
            depth = 32;
            colorOrder = COLOR_BGRA;
            d3dFormatOut = D3DFMT_A8R8G8B8;
        }
    }
    else
    {
        // Any unknown raster formats need conversion to full quality.
        rasterFormat = RASTER_8888;
        depth = 32;
        colorOrder = COLOR_BGRA;
        d3dFormatOut = D3DFMT_A8R8G8B8;
    }
}

inline uint32 getD3DTextureDataRowAlignment( void )
{
    // We somehow always align our texture data rows by DWORD.
    // This is meant to speed up the access of rows, by the hardware driver.
    // In Direct3D, valid values are 1, 2, 4, or 8.
    return sizeof( rw::uint32 );
}

inline rasterRowSize getD3DRasterDataRowSize( uint32 texWidth, uint32 depth )
{
    return getRasterDataRowSize( texWidth, depth, getD3DTextureDataRowAlignment() );
}

inline uint32 getD3DPaletteCount(ePaletteType paletteType)
{
    uint32 reqPalCount = 0;

    if (paletteType == PALETTE_4BIT)
    {
        reqPalCount = 32;
    }
    else if (paletteType == PALETTE_8BIT)
    {
        reqPalCount = 256;
    }
    else
    {
        assert( 0 );
    }

    return reqPalCount;
}

// Function to get a default raster format string.
template <typename... stringArgs>
inline void getDefaultRasterFormatString( eRasterFormat rasterFormat, uint32 itemDepth, ePaletteType paletteType, eColorOrdering colorOrder, eir::String <char, stringArgs...>& formatOut )
{
    // Put info about pixel type.
    bool isColorOrderImportant = false;
    {
        if ( rasterFormat == RASTER_DEFAULT )
        {
            formatOut += "undefined";
        }
        else if ( rasterFormat == RASTER_1555 )
        {
            formatOut += "1555";

            isColorOrderImportant = true;
        }
        else if ( rasterFormat == RASTER_565 )
        {
            formatOut += "565";

            isColorOrderImportant = true;
        }
        else if ( rasterFormat == RASTER_4444 )
        {
            formatOut += "4444";

            isColorOrderImportant = true;
        }
        else if ( rasterFormat == RASTER_LUM )
        {
            if ( itemDepth == 8 )
            {
                formatOut += "LUM8";
            }
            else if ( itemDepth == 4 )
            {
                formatOut += "LUM4";
            }
            else
            {
                formatOut += "LUM";
            }
        }
        else if ( rasterFormat == RASTER_8888 )
        {
            formatOut += "8888";

            isColorOrderImportant = true;
        }
        else if ( rasterFormat == RASTER_888 )
        {
            formatOut += "888";

            if ( itemDepth == 24 )
            {
                formatOut += " 24bit";
            }

            isColorOrderImportant = true;
        }
        else if ( rasterFormat == RASTER_16 )
        {
            formatOut += "depth_16";
        }
        else if ( rasterFormat == RASTER_24 )
        {
            formatOut += "depth_24";
        }
        else if ( rasterFormat == RASTER_32 )
        {
            formatOut += "depth_32";
        }
        else if ( rasterFormat == RASTER_555 )
        {
            formatOut += "555";

            isColorOrderImportant = true;
        }
        // NEW formats :3
        else if ( rasterFormat == RASTER_LUM_ALPHA )
        {
            if ( itemDepth == 8 )
            {
                formatOut += "LUM4_A4";
            }
            else if ( itemDepth == 16 )
            {
                formatOut += "LUM8_A8";
            }
            else
            {
                formatOut += "LUM_A";
            }
        }
        else
        {
            formatOut += "unknown";
        }
    }

    // Put info about palette type.
    if ( paletteType != PALETTE_NONE )
    {
        if ( paletteType == PALETTE_4BIT ||
             paletteType == PALETTE_4BIT_LSB )
        {
            formatOut += " PAL4";
        }
        else if ( paletteType == PALETTE_8BIT )
        {
            formatOut += " PAL8";
        }
        else
        {
            formatOut += " PAL";
        }
    }

    // Put info about color order
    if ( isColorOrderImportant )
    {
        if ( colorOrder == COLOR_RGBA )
        {
            formatOut += " RGBA";
        }
        else if ( colorOrder == COLOR_BGRA )
        {
            formatOut += " BGRA";
        }
        else if ( colorOrder == COLOR_ABGR )
        {
            formatOut += " ABGR";
        }
    }
}

};

#endif //_SHARED_DIRECT3D_LEGACY_NATIVETEX_INCLUDE_