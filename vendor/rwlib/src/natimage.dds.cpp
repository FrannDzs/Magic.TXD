/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/natimage.dds.cpp
*  PURPOSE:     DDS native image format implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_DDS_NATIVEIMG

#include "natimage.hxx"

// Include all native texture environments that we support.
#include "txdread.d3d.hxx"
#include "txdread.d3d8.hxx"
#include "txdread.d3d9.hxx"
#include "txdread.xbox.hxx"

// Include layer helpers for really complicated things.
#include "txdread.d3d8.layerpipe.hxx"
#include "txdread.d3d9.layerpipe.hxx"
#include "txdread.xbox.layerpipe.hxx"

#include "streamutil.hxx"

#include "txdread.common.hxx"

#include "txdread.d3d.genmip.hxx"

#include "pixelutil.hxx"

// Our first and coolest native image plugin, the DirectDraw Surface format!
// This native image format has support for the D3D8, D3D9 and XBOX native textures.

namespace rw
{

static const imaging_filename_ext dds_file_extensions[] =
{
    { "dds", true }
};

static const natimg_supported_native_desc dds_supported_nat_textures[] =
{
    { "Direct3D8" },
    { "Direct3D9" },
    { "XBOX" }
};

// DDS main header flags.
#define DDSD_CAPS               0x00000001
#define DDSD_HEIGHT             0x00000002
#define DDSD_WIDTH              0x00000004
#define DDSD_PITCH              0x00000008
#define DDSD_PIXELFORMAT        0x00001000
#define DDSD_MIPMAPCOUNT        0x00020000
#define DDSD_LINEARSIZE         0x00080000
#define DDSD_DEPTH              0x00800000

// Pixel format flags.
// TODO: there are some interesting new fields, we gotta add support for them.
#define DDPF_ALPHAPIXELS        0x00000001
#define DDPF_ALPHA              0x00000002
#define DDPF_FOURCC             0x00000004
#define DDPF_PALETTEINDEXED4    0x00000008
#define DDPF_PALETTEINDEXEDTO8  0x00000010
#define DDPF_PALETTEINDEXED8    0x00000020
#define DDPF_RGB                0x00000040
#define DDPF_COMPRESSED         0x00000080
#define DDPF_RGBTOYUV           0x00000100
#define DDPF_YUV                0x00000200
#define DDPF_ZBUFFER            0x00000400
#define DDPF_PALETTEINDEXED1    0x00000800
#define DDPF_PALETTEINDEXED2    0x00001000
#define DDPF_ZPIXELS            0x00002000
#define DDPF_STENCILBUFFER      0x00004000
#define DDPF_ALPHAPREMULT       0x00008000
#define DDPF_LUMINANCE          0x00020000
#define DDPF_BUMPLUMINANCE      0x00040000
#define DDPF_BUMPDUDV           0x00080000

// Capability flags.
#define DDSCAPS_RESERVED1       0x00000001
#define DDSCAPS_ALPHA           0x00000002
#define DDSCAPS_BACKBUFFER      0x00000004
#define DDSCAPS_COMPLEX         0x00000008
#define DDSCAPS_FLIP            0x00000010
#define DDSCAPS_FRONTBUFFER     0x00000020
#define DDSCAPS_OFFSCREENPLAIN  0x00000040
#define DDSCAPS_OVERLAY         0x00000080
#define DDSCAPS_PALETTE         0x00000100
#define DDSCAPS_PRIMARYSURFACE  0x00000200
#define DDSCAPS_SYSTEMMEMORY    0x00000800
#define DDSCAPS_TEXTURE         0x00001000
#define DDSCAPS_3DDEVICE        0x00002000
#define DDSCAPS_VIDEOMEMORY     0x00004000
#define DDSCAPS_VISIBLE         0x00008000
#define DDSCAPS_WRITEONLY       0x00010000
#define DDSCAPS_ZBUFFER         0x00020000
#define DDSCAPS_OWNDC           0x00040000
#define DDSCAPS_LIVEVIDEO       0x00080000
#define DDSCAPS_HWCODEC         0x00100000
#define DDSCAPS_MODEX           0x00200000
#define DDSCAPS_MIPMAP          0x00400000
#define DDSCAPS_ALLOCONLOAD     0x04000000
#define DDSCAPS_VIDEOPORT       0x08000000
#define DDSCAPS_LOCALVIDMEM     0x10000000
#define DDSCAPS_NONLOCALVIDMEM  0x20000000
#define DDSCAPS_STANDARDVGAMODE 0x40000000
#define DDSCAPS_OPTIMIZED       0x80000000

#define DDSCAPS2_HARDWAREDEINTERLACE    0x00000002
#define DDSCAPS2_HINTDYNAMIC            0x00000004
#define DDSCAPS2_HINTSTATIC             0x00000008
#define DDSCAPS2_TEXTUREMANAGE          0x00000010
#define DDSCAPS2_OPAQUE                 0x00000080
#define DDSCAPS2_HINTANTIALIASING       0x00000100
#define DDSCAPS2_CUBEMAP                0x00000200
#define DDSCAPS2_CUBEMAP_POSITIVEX      0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX      0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY      0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY      0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ      0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ	    0x00008000
#define DDSCAPS2_MIPMAPSUBLEVEL         0x00010000
#define DDSCAPS2_D3DTEXTUREMANAGE       0x00020000
#define DDSCAPS2_DONOTPERSIST           0x00040000
#define DDSCAPS2_STEREOSURFACELEFT      0x00080000
#define DDSCAPS2_VOLUME                 0x00200000

inline uint32 getDDSRasterDataRowAlignment( void )
{
    // The DDS native image uses byte alignment in its files.
    return 1;
}

inline rasterRowSize getDDSRasterDataRowSize( uint32 surfWidth, uint32 depth )
{
    return getRasterDataRowSize( surfWidth, depth, getDDSRasterDataRowAlignment() );
}

struct dds_pixelformat
{
    endian::p_little_endian <uint32> dwSize;
    endian::p_little_endian <uint32> dwFlags;
    endian::p_little_endian <uint32> dwFourCC;
    endian::p_little_endian <uint32> dwRGBBitCount;
    endian::p_little_endian <uint32> dwRBitMask;
    endian::p_little_endian <uint32> dwGBitMask;
    endian::p_little_endian <uint32> dwBBitMask;
    endian::p_little_endian <uint32> dwABitMask;
};

struct dds_header
{
    endian::p_little_endian <uint32> dwSize;
    endian::p_little_endian <uint32> dwFlags;
    endian::p_little_endian <uint32> dwHeight;
    endian::p_little_endian <uint32> dwWidth;
    endian::p_little_endian <uint32> dwPitchOrLinearSize;
    endian::p_little_endian <uint32> dwDepth;
    endian::p_little_endian <uint32> dwMipmapCount;
    endian::p_little_endian <uint32> dwReserved1[11];

    dds_pixelformat ddspf;

    endian::p_little_endian <uint32> dwCaps;
    endian::p_little_endian <uint32> dwCaps2;
    endian::p_little_endian <uint32> dwCaps3;
    endian::p_little_endian <uint32> dwCaps4;
    endian::p_little_endian <uint32> dwReserved2;

    static inline bool calculateSurfaceDimensions( uint32 dwFourCC, uint32 layerWidth, uint32 layerHeight, uint32& mipWidthOut, uint32& mipHeightOut )
    {
        // Per recommendation of MSDN, we calculate the stride ourselves.
        switch( dwFourCC )
        {
        case D3DFMT_R8G8B8:
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_R3G3B2:
        case D3DFMT_A8:
        case D3DFMT_A8R3G3B2:
        case D3DFMT_X4R4G4B4:
        case D3DFMT_A2B10G10R10:
        case D3DFMT_A8B8G8R8:
        case D3DFMT_X8B8G8R8:
        case D3DFMT_G16R16:
        case D3DFMT_A2R10G10B10:
        case D3DFMT_A16B16G16R16:
        case D3DFMT_A8P8:
        case D3DFMT_P8:
        case D3DFMT_L8:
        case D3DFMT_A8L8:
        case D3DFMT_A4L4:
        case D3DFMT_V8U8:
        case D3DFMT_L6V5U5:
        case D3DFMT_X8L8V8U8:
        case D3DFMT_Q8W8V8U8:
        case D3DFMT_V16U16:
        case D3DFMT_A2W10V10U10:
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D32:
        case D3DFMT_D15S1:
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D16:
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D24FS8:
        case D3DFMT_L16:
        case D3DFMT_Q16W16V16U16:
        case D3DFMT_R16F:
        case D3DFMT_G16R16F:
        case D3DFMT_A16B16G16R16F:
        case D3DFMT_R32F:
        case D3DFMT_G32R32F:
        case D3DFMT_A32B32G32R32F:
        case D3DFMT_CxV8U8:
            // uncompressed.
            mipWidthOut = layerWidth;
            mipHeightOut = layerHeight;
            return true;
        case D3DFMT_UYVY:
        case D3DFMT_R8G8_B8G8:
        case D3DFMT_YUY2:
        case D3DFMT_G8R8_G8B8:
            // 2x2 block format.
            mipWidthOut = ALIGN_SIZE( layerWidth, 2u );
            mipHeightOut = ALIGN_SIZE( layerHeight, 2u );
            return true;
        case D3DFMT_DXT1:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            // block compression format.
            mipWidthOut = ALIGN_SIZE( layerWidth, 4u );
            mipHeightOut = ALIGN_SIZE( layerHeight, 4u );
            return true;
        }

        return false;
    }

    // The idea is that we store compressed formats using linear size.
    inline static bool shouldDDSFourCCStoreLinearSize( uint32 fourCC )
    {
        switch( fourCC )
        {
        // DXT block compression.
        case D3DFMT_DXT1:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
        // Special YUV compression.
        case D3DFMT_UYVY:
        case D3DFMT_R8G8_B8G8:
        case D3DFMT_YUY2:
        case D3DFMT_G8R8_G8B8:
            return true;
        }

        return false;
    }

    static inline bool doesFormatSupportPitch( D3DFORMAT d3dFormat )
    {
        switch( d3dFormat )
        {
        case D3DFMT_R8G8B8:
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_R3G3B2:
        case D3DFMT_A8:
        case D3DFMT_A8R3G3B2:
        case D3DFMT_X4R4G4B4:
        case D3DFMT_A2B10G10R10:
        case D3DFMT_A8B8G8R8:
        case D3DFMT_X8B8G8R8:
        case D3DFMT_G16R16:
        case D3DFMT_A2R10G10B10:
        case D3DFMT_A16B16G16R16:
        case D3DFMT_A8P8:
        case D3DFMT_P8:
        case D3DFMT_L8:
        case D3DFMT_A8L8:
        case D3DFMT_A4L4:
        case D3DFMT_V8U8:
        case D3DFMT_L6V5U5:
        case D3DFMT_X8L8V8U8:
        case D3DFMT_Q8W8V8U8:
        case D3DFMT_V16U16:
        case D3DFMT_A2W10V10U10:
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D32:
        case D3DFMT_D15S1:
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D16:
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D24FS8:
        case D3DFMT_L16:
        case D3DFMT_Q16W16V16U16:
        case D3DFMT_R16F:
        case D3DFMT_G16R16F:
        case D3DFMT_A16B16G16R16F:
        case D3DFMT_R32F:
        case D3DFMT_G32R32F:
        case D3DFMT_A32B32G32R32F:
        case D3DFMT_CxV8U8:
            // uncompressed.
            return true;
        case D3DFMT_UYVY:
        case D3DFMT_R8G8_B8G8:
        case D3DFMT_YUY2:
        case D3DFMT_G8R8_G8B8:
            // 2x2 block format.
            return false;
        case D3DFMT_DXT1:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            // block compression format.
            return false;
        default:
            // Not supported.
            break;
        }

        // No idea about anything else.
        // Let's say it does not support pitch.
        return false;
    }
};

inline void calculateSurfaceDimensions(
    uint32 layerWidth, uint32 layerHeight, bool hasValidFourCC, uint32 fourCC,
    uint32& surfWidthOut, uint32& surfHeightOut
)
{
    bool hasValidDimensions = false;

    if ( !hasValidDimensions )
    {
        // We most likely want to decide by four cc.
        if ( hasValidFourCC )
        {
            bool couldGet = dds_header::calculateSurfaceDimensions(
                fourCC, layerWidth, layerHeight, surfWidthOut, surfHeightOut
            );

            if ( couldGet )
            {
                // Most likely correct.
                hasValidDimensions = true;
            }
        }
    }

    if ( !hasValidDimensions )
    {
        // For formats we do not know about, we assume the texture has proper dimensions already.
        surfWidthOut = layerWidth;
        surfHeightOut = layerHeight;

        hasValidDimensions = true;
    }
}

enum class eDDSPixelFormatType
{
    FMT_RGB,        // RGB bitmasks valid
    FMT_YUV,        // RGB == YUV and bitmasks are valid
    FMT_LUM,        // R == LUM bit mask and valid
    FMT_ALPHA,      // R == alpha bit mask and valid
    FMT_FOURCC,     // compressed color data, fourCC valid
    FMT_PAL4,       // 16 palette colors
    FMT_PAL8        // 256 palette colors
};

inline static bool isDDSPixelFormatPalette( eDDSPixelFormatType type )
{
    return ( type == eDDSPixelFormatType::FMT_PAL4 || type == eDDSPixelFormatType::FMT_PAL8 );
}

inline static uint32 getDDSPixelFormatPaletteCount( eDDSPixelFormatType type )
{
    if ( type == eDDSPixelFormatType::FMT_PAL4 )
    {
        return 16;
    }
    else if ( type == eDDSPixelFormatType::FMT_PAL8 )
    {
        return 256;
    }

    return 0;
}

inline static const wchar_t* getDDSPixelFormatTypeName( eDDSPixelFormatType type )
{
    if ( type == eDDSPixelFormatType::FMT_RGB )
    {
        return L"RGB";
    }
    else if ( type == eDDSPixelFormatType::FMT_YUV )
    {
        return L"YUV";
    }
    else if ( type == eDDSPixelFormatType::FMT_LUM )
    {
        return L"LUM";
    }
    else if ( type == eDDSPixelFormatType::FMT_ALPHA )
    {
        return L"alpha";
    }
    else if ( type == eDDSPixelFormatType::FMT_FOURCC )
    {
        return L"fourCC";
    }
    else if ( type == eDDSPixelFormatType::FMT_PAL4 )
    {
        return L"PAL4";
    }
    else if ( type == eDDSPixelFormatType::FMT_PAL8 )
    {
        return L"PAL8";
    }

    return L"unknown";
}

struct rgba_mask_info
{
    uint32 redMask;
    uint32 greenMask;
    uint32 blueMask;
    uint32 alphaMask;
    uint32 depth;

    D3DFORMAT format;
};

static const rgba_mask_info rgba_masks[] =
{
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000, 24, D3DFMT_R8G8B8 },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, 32, D3DFMT_A8R8G8B8 },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000, 32, D3DFMT_X8R8G8B8 },
    { 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000, 16, D3DFMT_R5G6B5 },
    { 0x00007C00, 0x000003E0, 0x0000001F, 0x00000000, 16, D3DFMT_X1R5G5B5 },
    { 0x00007C00, 0x000003E0, 0x0000001F, 0x00008000, 16, D3DFMT_A1R5G5B5 },
    { 0x00000F00, 0x000000F0, 0x0000000F, 0x0000F000, 16, D3DFMT_A4R4G4B4 },
    { 0x000000E0, 0x0000001C, 0x00000003, 0x00000000, 8,  D3DFMT_R3G3B2 },
    { 0x000000E0, 0x0000001C, 0x00000003, 0x0000FF00, 16, D3DFMT_A8R3G3B2 },
    { 0x00000F00, 0x000000F0, 0x0000000F, 0x00000000, 16, D3DFMT_X4R4G4B4 },
    { 0x000003FF, 0x000FFC00, 0x3FF00000, 0xC0000000, 32, D3DFMT_A2B10G10R10 },
    { 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000, 32, D3DFMT_A8B8G8R8 },
    { 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000, 32, D3DFMT_X8B8G8R8 },
    { 0x0000FFFF, 0xFFFF0000, 0x00000000, 0x00000000, 32, D3DFMT_G16R16 },
    { 0x3FF00000, 0x000FFC00, 0x000003FF, 0xC0000000, 32, D3DFMT_A2R10G10B10 }
};

struct yuv_mask_info
{
    uint32 yMask;
    uint32 uMask;
    uint32 vMask;
    uint32 depth;

    D3DFORMAT format;
};

static const yuv_mask_info yuv_masks[] =
{
    { 0x00000000, 0x000000FF, 0x0000FF00, 16, D3DFMT_V8U8 },
    { 0x00000000, 0x0000FFFF, 0xFFFF0000, 32, D3DFMT_V16U16 }
};

struct lum_mask_info
{
    uint32 lumMask;
    uint32 alphaMask;
    uint32 depth;

    D3DFORMAT format;
};

static const lum_mask_info lum_masks[] =
{
    { 0x000000FF, 0x00000000,  8, D3DFMT_L8 },
    { 0x000000FF, 0x0000FF00, 16, D3DFMT_A8L8 },
    { 0x0000000F, 0x000000F0,  8, D3DFMT_A4L4 }
};

struct alpha_mask_info
{
    uint32 alphaMask;
    uint32 depth;

    D3DFORMAT format;
};

static const alpha_mask_info alpha_masks[] =
{
    { 0x000000FF, 8, D3DFMT_A8 }
};

#define ELMCNT( x ) ( sizeof(x) / sizeof(*x) )

inline bool getDDSMappingFromD3DFormat(
    D3DFORMAT d3dFormat,
    eDDSPixelFormatType& formatTypeOut,
    uint32& redMaskOut, uint32& greenMaskOut, uint32& blueMaskOut, uint32& alphaMaskOut, uint32& fourCCOut
)
{
    uint32 redMask = 0;
    uint32 greenMask = 0;
    uint32 blueMask = 0;
    uint32 alphaMask = 0;
    uint32 fourCC = 0;

    bool hasDirectMapping = false;

    eDDSPixelFormatType formatType;

    if ( !hasDirectMapping )
    {
        // Check RGBA first.
        uint32 rgba_count = ELMCNT( rgba_masks );

        for ( uint32 n = 0; n < rgba_count; n++ )
        {
            const rgba_mask_info& curInfo = rgba_masks[ n ];

            if ( curInfo.format == d3dFormat )
            {
                redMask = curInfo.redMask;
                greenMask = curInfo.greenMask;
                blueMask = curInfo.blueMask;
                alphaMask = curInfo.alphaMask;

                formatType = eDDSPixelFormatType::FMT_RGB;
                hasDirectMapping = true;
                break;
            }
        }
    }

    if ( !hasDirectMapping )
    {
        // Check luminance next.
        uint32 lum_count = ELMCNT( lum_masks );

        for ( uint32 n = 0; n < lum_count; n++ )
        {
            const lum_mask_info& curInfo = lum_masks[ n ];

            if ( curInfo.format == d3dFormat )
            {
                redMask = curInfo.lumMask;
                alphaMask = curInfo.alphaMask;

                formatType = eDDSPixelFormatType::FMT_LUM;
                hasDirectMapping = true;
                break;
            }
        }
    }

    if ( !hasDirectMapping )
    {
        // Check YUV next.
        uint32 yuv_count = ELMCNT( yuv_masks );

        for ( uint32 n = 0; n < yuv_count; n++ )
        {
            const yuv_mask_info& curInfo = yuv_masks[ n ];

            if ( curInfo.format == d3dFormat )
            {
                redMask = curInfo.yMask;
                greenMask = curInfo.uMask;
                blueMask = curInfo.vMask;

                formatType = eDDSPixelFormatType::FMT_YUV;
                hasDirectMapping = true;
                break;
            }
        }
    }

    if ( !hasDirectMapping )
    {
        // Check alpha next.
        uint32 alpha_count = ELMCNT( alpha_masks );

        for ( uint32 n = 0; n < alpha_count; n++ )
        {
            const alpha_mask_info& curInfo = alpha_masks[ n ];

            if ( curInfo.format == d3dFormat )
            {
                alphaMask = curInfo.alphaMask;

                formatType = eDDSPixelFormatType::FMT_ALPHA;
                hasDirectMapping = true;
                break;
            }
        }
    }

    if ( !hasDirectMapping )
    {
        // Some special D3DFORMAT fields are supported by DDS.
        if ( d3dFormat == D3DFMT_DXT1 ||
             d3dFormat == D3DFMT_DXT2 ||
             d3dFormat == D3DFMT_DXT3 ||
             d3dFormat == D3DFMT_DXT4 ||
             d3dFormat == D3DFMT_DXT5 )
        {
            fourCC = d3dFormat;

            formatType = eDDSPixelFormatType::FMT_FOURCC;
            hasDirectMapping = true;
        }
    }

    if ( hasDirectMapping )
    {
        formatTypeOut = formatType;
        redMaskOut = redMask;
        greenMaskOut = greenMask;
        blueMaskOut = blueMask;
        alphaMaskOut = alphaMask;
        fourCCOut = fourCC;
    }

    return hasDirectMapping;
}

struct ddsNativeImageFormatTypeManager : public nativeImageTypeManager
{
    struct ddsNativeImage
    {
        inline ddsNativeImage( Interface *engineInterface ) : mipmaps( eir::constr_with_alloc::DEFAULT, engineInterface )
        {
            this->engineInterface = engineInterface;

            // Initialize with defaults.
            memset( this->reserved1, 0, sizeof( this->reserved1 ) );

            this->pf_type = eDDSPixelFormatType::FMT_RGB;
            this->pf_fourCC = 0;
            this->pf_redMask = 0;
            this->pf_greenMask = 0;
            this->pf_blueMask = 0;
            this->pf_alphaMask = 0;
            this->pf_hasAlphaChannel = false;

            this->bitDepth = 0;

            // Capabilities are determined when getting actual texel data.
            this->caps2 = 0;
            this->caps3 = 0;
            this->caps4 = 0;
            this->reserved2 = 0;

            this->paletteData = nullptr;
        }

        inline ddsNativeImage( const ddsNativeImage& right ) : mipmaps( right.mipmaps )
        {
            // Take over the data directly.
            this->engineInterface = right.engineInterface;
            memcpy( this->reserved1, right.reserved1, sizeof( this->reserved1 ) );

            this->pf_type = right.pf_type;
            this->pf_fourCC = right.pf_fourCC;
            this->pf_redMask = right.pf_redMask;
            this->pf_greenMask = right.pf_greenMask;
            this->pf_blueMask = right.pf_blueMask;
            this->pf_alphaMask = right.pf_alphaMask;
            this->pf_hasAlphaChannel = right.pf_hasAlphaChannel;

            this->caps2 = right.caps2;
            this->caps3 = right.caps3;
            this->caps4 = right.caps4;
            this->reserved2 = right.reserved2;

            // Color data.
            //this->mipmaps = right.mipmaps;
            this->paletteData = right.paletteData;
        }

        inline ~ddsNativeImage( void )
        {
            // We are not freeing things over here.
            // You must remember to free texel data when deleting the image yourself.
        }

        Interface *engineInterface;

        // Fields that are important for a DDS image.
        uint32 reserved1[11];

        // Pixel format properties.
        eDDSPixelFormatType pf_type;
        uint32 pf_fourCC;
        uint32 pf_redMask;
        uint32 pf_greenMask;
        uint32 pf_blueMask;
        uint32 pf_alphaMask;
        bool pf_hasAlphaChannel;

        uint32 bitDepth;

        uint32 caps2;
        uint32 caps3;
        uint32 caps4;
        uint32 reserved2;

        // Palette data.
        void *paletteData;

        // Mipmap data.
        typedef genmip::mipmapLayer mipmap_t;

        typedef rwVector <mipmap_t> mipmaps_t;

        mipmaps_t mipmaps;
    };

    // Construction API.
    void ConstructImage( Interface *engineInterface, void *imageMem ) const override
    {
        new (imageMem) ddsNativeImage( engineInterface );
    }

    void CopyConstructImage( Interface *engineInterface, void *imageMem, const void *srcImageMem ) const override
    {
        new (imageMem) ddsNativeImage( *(const ddsNativeImage*)srcImageMem );
    }

    void DestroyImage( Interface *engineInterface, void *imageMem ) const override
    {
        ( (ddsNativeImage*)imageMem )->~ddsNativeImage();
    }

    // Description API.
    const char* GetBestSupportedNativeTexture( Interface *engineInterface, const void *imageMem ) const override
    {
        // The DDS native texture works best with the strongest native texture, of course.
        return "Direct3D9";
    }

    // Pixel manipulation API.
    void ClearImageData( Interface *engineInterface, void *imageMem, bool deallocate ) const override
    {
        ddsNativeImage *ddsImage = (ddsNativeImage*)imageMem;

        // This routine is used to free memory from the DDS before destruction.
        if ( deallocate )
        {
            if ( void *paletteData = ddsImage->paletteData )
            {
                engineInterface->PixelFree( paletteData );
            }

            genmip::deleteMipmapLayers( engineInterface, ddsImage->mipmaps );
        }

        // Clear data now.
        ddsImage->mipmaps.Clear();

        ddsImage->paletteData = nullptr;

        // We always should reset the image format even though it is not required.
        memset( ddsImage->reserved1, 0, sizeof( ddsImage->reserved1 ) );

        ddsImage->pf_type = eDDSPixelFormatType::FMT_RGB;
        ddsImage->pf_fourCC = 0;
        ddsImage->pf_redMask = 0;
        ddsImage->pf_greenMask = 0;
        ddsImage->pf_blueMask = 0;
        ddsImage->pf_alphaMask = 0;
        ddsImage->pf_hasAlphaChannel = false;

        ddsImage->bitDepth = 0;

        ddsImage->caps2 = 0;
        ddsImage->caps3 = 0;
        ddsImage->caps4 = 0;
        ddsImage->reserved2 = 0;
    }

    void ClearPaletteData( Interface *engineInterface, void *imageMem, bool deallocate ) const override
    {
        // We want to clear everything related to palette.
        ddsNativeImage *ddsImage = (ddsNativeImage*)imageMem;

        if ( deallocate )
        {
            if ( void *paletteData = ddsImage->paletteData )
            {
                engineInterface->PixelFree( paletteData );
            }
        }

        // Clear the buffer.
        // We are allowed to turn into an incomplete image.
        ddsImage->paletteData = nullptr;
    }

    inline static void getDDSPaletteRasterFormat( eRasterFormat& rasterFormatOut, eColorOrdering& colorOrderOut )
    {
        rasterFormatOut = RASTER_8888;
        colorOrderOut = COLOR_RGBA;
    }

    void WriteToNativeTexture( Interface *engineInterface, void *imageMem, const char *nativeTexName, void *nativeTexMem, acquireFeedback_t& feedbackOut ) const override
    {
        ddsNativeImage *ddsImage = (ddsNativeImage*)imageMem;

        // We take things from a multitude of native textures.

        // A DDS file is supposed to be directly acquirable into a Direct3D 9 native texture.
        // The ones that we cannot aquire we consider malformed.

        eDDSPixelFormatType formatType = ddsImage->pf_type;

        // To acquire it, we must map it to a valid thing for us.
        // This is if it either has a d3dFormat or a palette format.
        D3DFORMAT dds_d3dFormat = D3DFMT_UNKNOWN;
        bool dds_hasValidFormat = false;

        ePaletteType dds_paletteType = PALETTE_NONE;
        uint32 dds_paletteSize = 0;
        bool dds_hasPaletteFormat = false;

        uint32 dds_bitDepth = ddsImage->bitDepth;
        bool dds_hasBitDepth = ( dds_bitDepth != 0 );

        bool dds_hasAlphaChannel = ddsImage->pf_hasAlphaChannel;

        if ( formatType == eDDSPixelFormatType::FMT_FOURCC )
        {
            dds_d3dFormat = (D3DFORMAT)ddsImage->pf_fourCC;

            // We can acquire any anonymous format, so thats fine.
            dds_hasValidFormat = true;
        }
        else if ( formatType == eDDSPixelFormatType::FMT_RGB )
        {
            size_t rgba_count = ELMCNT( rgba_masks );

            uint32 redMask = ddsImage->pf_redMask;
            uint32 greenMask = ddsImage->pf_greenMask;
            uint32 blueMask = ddsImage->pf_blueMask;
            uint32 alphaMask = ( dds_hasAlphaChannel ? ddsImage->pf_alphaMask : 0 );

            if ( !dds_hasBitDepth )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_INVALIDBITDEPTH_RGBFMT" );
            }

            for ( size_t n = 0; n < rgba_count; n++ )
            {
                const rgba_mask_info& curInfo = rgba_masks[ n ];

                if ( curInfo.redMask == redMask &&
                     curInfo.greenMask == greenMask &&
                     curInfo.blueMask == blueMask &&
                     curInfo.alphaMask == alphaMask &&
                     curInfo.depth == dds_bitDepth )
                {
                    // We found something.
                    dds_d3dFormat = curInfo.format;

                    dds_hasValidFormat = true;
                    break;
                }
            }

            if ( !dds_hasValidFormat )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_RGB_TO_D3DFORMAT" );
            }
        }
        else if ( formatType == eDDSPixelFormatType::FMT_YUV )
        {
            size_t yuv_count = ELMCNT( yuv_masks );

            uint32 yMask = ddsImage->pf_redMask;
            uint32 uMask = ddsImage->pf_greenMask;
            uint32 vMask = ddsImage->pf_blueMask;
            uint32 aMask = ( dds_hasAlphaChannel ? ddsImage->pf_alphaMask : 0 );

            if ( aMask != 0x00000000 )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_ALPHAYUV" );
            }

            if ( !dds_hasBitDepth )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_INVALIDBITDEPTH_YUVFMT" );
            }

            for ( size_t n = 0; n < yuv_count; n++ )
            {
                const yuv_mask_info& curInfo = yuv_masks[ n ];

                if ( curInfo.yMask == yMask &&
                     curInfo.uMask == uMask &&
                     curInfo.vMask == vMask &&
                     curInfo.depth == dds_bitDepth )
                {
                    dds_d3dFormat = curInfo.format;

                    dds_hasValidFormat = true;
                    break;
                }
            }

            if ( !dds_hasValidFormat )
            {
                throw NativeImagingInternalErrorException( "DDS", L"DDS_STRUCTERR_YUV_TO_D3DFORMAT" );
            }
        }
        else if ( formatType == eDDSPixelFormatType::FMT_LUM )
        {
            size_t lum_count = ELMCNT( lum_masks );

            uint32 lumMask = ddsImage->pf_redMask;
            uint32 alphaMask = ( dds_hasAlphaChannel ? ddsImage->pf_alphaMask : 0 );

            if ( !dds_hasBitDepth )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_INVALIDBITDEPTH_LUMFMT" );
            }

            for ( size_t n = 0; n < lum_count; n++ )
            {
                const lum_mask_info& curInfo = lum_masks[ n ];

                if ( curInfo.lumMask == lumMask &&
                     curInfo.alphaMask == alphaMask &&
                     curInfo.depth == dds_bitDepth )
                {
                    dds_d3dFormat = curInfo.format;

                    dds_hasValidFormat = true;
                    break;
                }
            }

            if ( !dds_hasValidFormat )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_LUM_TO_D3DFORMAT" );
            }
        }
        else if ( formatType == eDDSPixelFormatType::FMT_ALPHA )
        {
            size_t alpha_count = ELMCNT( alpha_masks );

            uint32 alphaMask = ddsImage->pf_alphaMask;

            if ( !dds_hasBitDepth )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_INVALIDBITDEPTH_ALPHAFMT" );
            }

            for ( size_t n = 0; n < alpha_count; n++ )
            {
                const alpha_mask_info& curInfo = alpha_masks[ n ];

                if ( curInfo.alphaMask == alphaMask &&
                     curInfo.depth == dds_bitDepth )
                {
                    dds_d3dFormat = curInfo.format;

                    dds_hasValidFormat = true;
                    break;
                }
            }

            if ( !dds_hasValidFormat )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_ALPHA_TO_D3DFORMAT" );
            }
        }
        else if ( isDDSPixelFormatPalette( formatType ) )
        {
            if ( !dds_hasBitDepth )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_INVALIDBITDEPTH_PALFMT" );
            }

            // Determine the actual palette type.
            if ( formatType == eDDSPixelFormatType::FMT_PAL4 )
            {
                dds_paletteType = PALETTE_4BIT;
            }
            else if ( formatType == eDDSPixelFormatType::FMT_PAL8 )
            {
                dds_paletteType = PALETTE_8BIT;
            }
            else
            {
                assert( 0 );
            }

            dds_hasPaletteFormat = true;

            // Check whether the bit depth is valid.
            bool validDepth = false;

            if ( dds_paletteType == PALETTE_4BIT )
            {
                validDepth = ( dds_bitDepth == 4 || dds_bitDepth == 8 );
            }
            else if ( dds_paletteType == PALETTE_8BIT )
            {
                validDepth = ( dds_bitDepth == 8 );
            }

            if ( !validDepth )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_INVALIDBITDEPTH_PALFMT" );
            }

            dds_paletteSize = getPaletteItemCount( dds_paletteType );

            // We have no D3DFORMAT.
            dds_hasValidFormat = false;
        }
        else
        {
            throw NativeImagingInternalErrorException( "DDS", L"DDS_INTERNERR_UNKNOWNFMT" );
        }

        if ( !dds_hasValidFormat && !dds_hasPaletteFormat )
        {
            throw NativeImagingInternalErrorException( "DDS", L"DDS_INTERNERR_NOVALIDMAPPING" );
        }

        // Either way, get the native texture type provider.
        texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, nativeTexMem );

        // Now we have a valid mapping to a Direct3D 9 native texture, the strongest mapping possible.
        // We have to determine how we can convert this a valid format for the actually requested native texture.
        pixelCapabilities nativeTexCaps;
        texProvider->GetPixelCapabilities( nativeTexCaps );

        bool isDirect3D9 = false;
        bool isDirect3D8 = false;
        bool isXBOX = false;

        if ( strcmp( nativeTexName, "Direct3D8" ) == 0 )
        {
            isDirect3D8 = true;
        }
        else if ( strcmp( nativeTexName, "Direct3D9" ) == 0 )
        {
            isDirect3D9 = true;
        }
        else if ( strcmp( nativeTexName, "XBOX" ) == 0 )
        {
            isXBOX = true;
        }
        else
        {
            throw NativeTextureInvalidParameterException( "DDS", L"NATIVEIMG_PARAM_NATIVETEXTYPE", nullptr );
        }

        // For the first part, dermine the actual format of the DDS data.

        // Perform the mapping process.

        // Check whether we are DXT compressed.
        eCompressionType dds_compressionType = RWCOMPRESS_NONE;

        // Determine whether this is a direct format link.
        bool dds_d3dRasterFormatLink = false;

        d3dpublic::nativeTextureFormatHandler *dds_usedFormatHandler = nullptr;

        // Determine the mapping to original RW types.
        eRasterFormat dds_rasterFormat = RASTER_DEFAULT;
        eColorOrdering dds_colorOrder = COLOR_BGRA;

        if ( dds_hasValidFormat )
        {
            // DXT compressed?
            dds_compressionType = getFrameworkCompressionTypeFromD3DFORMAT( dds_d3dFormat );

            bool isVirtualMapping = false;

            // TODO: fix the virtual raster format decision here, because its decided wrong as DDS surfaces
            // do not come with an explicit alpha flag: we have to calculate the actual alpha flag before this!

            bool hasRepresentingFormat = getRasterFormatFromD3DFormat(
                dds_d3dFormat, dds_hasAlphaChannel || formatType == eDDSPixelFormatType::FMT_ALPHA,
                dds_rasterFormat, dds_colorOrder, isVirtualMapping
            );

            if ( hasRepresentingFormat )
            {
                if ( isVirtualMapping == false )
                {
                    dds_d3dRasterFormatLink = true;
                }
            }
            else
            {
#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9
                if ( dds_compressionType == RWCOMPRESS_NONE )
                {
                    // We could still have an extension that takes care of us.
                    // That is if we have the Direct3D 9 native texture environment.
                    d3d9NativeTextureTypeProvider *d3d9Env = (d3d9NativeTextureTypeProvider*)GetNativeTextureTypeProviderByName( engineInterface, "Direct3D9" );

                    if ( d3d9Env )
                    {
                        dds_usedFormatHandler = d3d9Env->GetFormatHandler( dds_d3dFormat );
                    }
                }
#endif //RWLIB_INCLUDE_NATIVETEX_D3D9
            }
        }
        else if ( dds_hasPaletteFormat )
        {
            // Palette are always encoded the same.
            dds_d3dRasterFormatLink = true;

            getDDSPaletteRasterFormat( dds_rasterFormat, dds_colorOrder );

            dds_d3dFormat = D3DFMT_P8;
        }
        else
        {
            assert( 0 );
        }

        // These are some properties which have to do for now.
        uint32 dds_rasterType = 4;  // bitmap raster type.
        bool dds_isCubemap = false; // for now no support.
        bool dds_autoMipmaps = false;   // I guess?

        // We care about if we have actually directly taken the DDS texel buffers.
        bool hasTakenDDSTexelBuffers;
        bool hasTakenDDSPaletteBuffer;

        // Give pixel data to the runtime appropriately.
#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9
        if ( isDirect3D9 )
        {
            // For the Direct3D9 native texture, we definately want to always directly acquire our data.
            // This is because we have a D3DFORMAT mapping to color data.

            // If the DDS comes as palette data, there are cases when we have to convert the data.
            // So we are not always about directly acquiring things.
            uint32 dstDepth = ( dds_hasBitDepth ? dds_bitDepth : 0 );
            ePaletteType dstPaletteType = dds_paletteType;

            // If we have a palette format, actually determine the parameters we need.
            if ( dstPaletteType != PALETTE_NONE )
            {
                d3d9::convertCompatiblePaletteType( dstDepth, dstPaletteType );
            }

            // For the raster format link, we take what the DDS provides us with.

            NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)nativeTexMem;

            const uint32 dstRowAlignment = getD3DTextureDataRowAlignment();

            // Process the palette data, if present/required.
            void *dstPaletteData = nullptr;

            if ( dstPaletteType != PALETTE_NONE )
            {
                // The D3D9 native tex is RGBA and accepts 32bit 8888, exactly the same as
                // DDS is providing. No conversion is required.
                dstPaletteData = ddsImage->paletteData;
            }

            size_t mipmapCount = ddsImage->mipmaps.GetCount();

            // Data inside of the DDS file can be aligned in whatever way.
            // If the rows are already DWORD aligned, we can directly read this mipmap data.
            // Otherwise we have to read row-by-row.

            // Check whether we can simply take all the mipmap layers from the DDS image.
            // If we can we do, otherwise we need to make copies.
            bool canTakeAllLayers;
            {
                bool isLinearSizeType = dds_header::shouldDDSFourCCStoreLinearSize( dds_d3dFormat );

                if ( isLinearSizeType )
                {
                    // If we are a linear size type, we are basically compressed.
                    // In those formats there is no pitch.
                    canTakeAllLayers = true;
                }
                else
                {
                    assert( dds_hasBitDepth == true );

                    // Is there any incompatible change?
                    if ( dstPaletteType != dds_paletteType || dstDepth != dds_bitDepth )
                    {
                        canTakeAllLayers = false;
                    }
                    else
                    {
                        bool hasSolidPitch = (
                            doesPixelDataNeedAddressabilityAdjustment(
                                ddsImage->mipmaps,
                                dds_bitDepth, getDDSRasterDataRowAlignment(),
                                dstDepth, dstRowAlignment
                            ) == false );

                        canTakeAllLayers = hasSolidPitch;
                    }
                }
            }

            // We might need the byte addressing modes.
            eir::eByteAddressingMode srcByteAddrMode, dstByteAddrMode;

            if ( !canTakeAllLayers )
            {
                srcByteAddrMode = getByteAddressingModeForRasterFormat( dds_paletteType );
                dstByteAddrMode = getByteAddressingModeForRasterFormat( dstPaletteType );
            }

            // Begin writing to the texture.
            for ( uint32 n = 0; n < mipmapCount; n++ )
            {
                const ddsNativeImage::mipmap_t& srcLayer = ddsImage->mipmaps[ n ];

                // Read the mipmap data.
                uint32 layerWidth = srcLayer.layerWidth;
                uint32 layerHeight = srcLayer.layerHeight;

                uint32 surfWidth = srcLayer.width;
                uint32 surfHeight = srcLayer.height;

                void *srcTexels = srcLayer.texels;
                uint32 mipDataSize = srcLayer.dataSize;

                // We could have to transform our texels.
                void *dstTexels = srcTexels;
                uint32 dstDataSize = mipDataSize;

                if ( !canTakeAllLayers )
                {
                    assert( dds_hasBitDepth == true );

                    DepthCopyTransformMipmapLayer(
                        engineInterface,
                        surfWidth, surfHeight, srcTexels, mipDataSize,
                        dds_bitDepth, getDDSRasterDataRowAlignment(), srcByteAddrMode,
                        dstDepth, dstRowAlignment, dstByteAddrMode,
                        dstTexels, dstDataSize
                    );
                }

                try
                {
                    // Store this mipmap layer.
                    NativeTextureD3D9::mipmapLayer mipLevel;
                    mipLevel.layerWidth = layerWidth;
                    mipLevel.layerHeight = layerHeight;
                    mipLevel.width = surfWidth;
                    mipLevel.height = surfHeight;
                    mipLevel.texels = dstTexels;
                    mipLevel.dataSize = dstDataSize;

                    // Add this layer.
                    nativeTex->mipmaps.AddToBack( std::move( mipLevel ) );
                }
                catch( ... )
                {
                    engineInterface->PixelFree( dstTexels );

                    throw;
                }
            }

            // Set format information.
            nativeTex->rasterFormat = dds_rasterFormat;
            nativeTex->depth = dstDepth;
            nativeTex->colorOrdering = dds_colorOrder;

            nativeTex->paletteType = dstPaletteType;
            nativeTex->paletteSize = dds_paletteSize;
            nativeTex->palette = dstPaletteData;

            nativeTex->d3dFormat = dds_d3dFormat;

            nativeTex->d3dRasterFormatLink = dds_d3dRasterFormatLink;
            nativeTex->anonymousFormatLink = dds_usedFormatHandler;

            nativeTex->colorComprType = dds_compressionType;

            // Up to the best of our abilities, actually calculate the alpha flag.
            bool dds_hasAlpha = false;
            {
                // We have to determine whether this native texture actually has transparent data.
                bool shouldHaveAlpha = ( formatType == eDDSPixelFormatType::FMT_ALPHA || dds_hasAlphaChannel );

                uint32 dxtType = 0;
                bool isDXTCompressed = IsDXTCompressionType( dds_compressionType, dxtType );

                if ( isDXTCompressed || dds_d3dRasterFormatLink || dds_usedFormatHandler != nullptr )
                {
                    bool hasAlphaByTexels = false;

                    if ( isDXTCompressed )
                    {
                        // We traverse DXT mipmap layers and check them.
                        for ( uint32 n = 0; n < mipmapCount; n++ )
                        {
                            const NativeTextureD3D9::mipmapLayer& mipLayer = nativeTex->mipmaps[ n ];

                            bool hasLayerAlpha = dxtMipmapCalculateHasAlpha(
                                mipLayer.width, mipLayer.height, mipLayer.layerWidth, mipLayer.layerHeight, mipLayer.texels,
                                dxtType
                            );

                            if ( hasLayerAlpha )
                            {
                                hasAlphaByTexels = true;
                                break;
                            }
                        }
                    }
                    else
                    {
                        assert( dds_hasBitDepth == true );

                        // Determine the raster format we should use for the mipmap alpha check.
                        eRasterFormat mipRasterFormat;
                        uint32 mipDepth;
                        eColorOrdering mipColorOrder;

                        bool directCheck = false;

                        if ( dds_d3dRasterFormatLink )
                        {
                            mipRasterFormat = dds_rasterFormat;
                            mipDepth = dds_bitDepth;
                            mipColorOrder = dds_colorOrder;

                            directCheck = true;
                        }
                        else if ( dds_usedFormatHandler != nullptr )
                        {
                            dds_usedFormatHandler->GetTextureRWFormat( mipRasterFormat, mipDepth, mipColorOrder );

                            directCheck = false;
                        }

                        // Check by raster format whether alpha data is even possible.
                        if ( canRasterFormatHaveAlpha( mipRasterFormat ) )
                        {
                            for ( uint32 n = 0; n < mipmapCount; n++ )
                            {
                                const NativeTextureD3D9::mipmapLayer& mipLayer = nativeTex->mipmaps[ n ];

                                uint32 mipWidth = mipLayer.width;
                                uint32 mipHeight = mipLayer.height;

                                uint32 srcDataSize = mipLayer.dataSize;
                                uint32 dstDataSize = srcDataSize;

                                void *srcTexels = mipLayer.texels;
                                void *dstTexels = srcTexels;

                                if ( !directCheck )
                                {
                                    rasterRowSize dstStride = getD3DRasterDataRowSize( mipWidth, mipDepth );

#ifdef _DEBUG
                                    assert( dstStride.mode == eRasterDataRowMode::ALIGNED );
#endif //_DEBUG

                                    dstDataSize = getRasterDataSizeByRowSize( dstStride, mipHeight );

                                    dstTexels = engineInterface->PixelAllocate( dstDataSize );

                                    try
                                    {
                                        dds_usedFormatHandler->ConvertToRW( srcTexels, mipWidth, mipHeight, dstStride.byteSize, srcDataSize, dstTexels );
                                    }
                                    catch( ... )
                                    {
                                        engineInterface->PixelFree( dstTexels );

                                        throw;
                                    }
                                }

                                bool hasMipmapAlpha = false;

                                try
                                {
                                    hasMipmapAlpha =
                                        rawMipmapCalculateHasAlpha(
                                            engineInterface,
                                            mipWidth, mipHeight, dstTexels, dstDataSize,
                                            mipRasterFormat, mipDepth, dstRowAlignment, mipColorOrder, dds_paletteType, dstPaletteData, dds_paletteSize
                                        );
                                }
                                catch( ... )
                                {
                                    if ( srcTexels != dstTexels )
                                    {
                                        engineInterface->PixelFree( dstTexels );
                                    }

                                    throw;
                                }

                                // Free texels if we allocated any.
                                if ( srcTexels != dstTexels )
                                {
                                    engineInterface->PixelFree( dstTexels );
                                }

                                if ( hasMipmapAlpha )
                                {
                                    // If anything has alpha, we can break.
                                    hasAlphaByTexels = true;
                                    break;
                                }
                            }
                        }
                    }

                    // Give data back to the runtime.
                    shouldHaveAlpha = hasAlphaByTexels;
                }

                dds_hasAlpha = shouldHaveAlpha;
            }

            // More advanced stuff.
            nativeTex->hasAlpha = dds_hasAlpha;
            nativeTex->autoMipmaps = dds_autoMipmaps;
            nativeTex->isCubeTexture = dds_isCubemap;

            // Are we actually original RW compatible? Let's find out!
            {
                bool isOriginalRWCompatible = false;

                if ( dds_d3dRasterFormatLink )
                {
                    isOriginalRWCompatible = isRasterFormatOriginalRWCompatible( dds_rasterFormat, dds_colorOrder, dstDepth, dstPaletteType );
                }

                nativeTex->isOriginalRWCompatible = isOriginalRWCompatible;
            }

            nativeTex->rasterType = dds_rasterType;

            // So have we taken the surface texel buffers?
            hasTakenDDSTexelBuffers = canTakeAllLayers;

            // If there was a palette, we can always directly take it.
            hasTakenDDSPaletteBuffer = true;
        }
        else
#endif //RWLIB_INCLUDE_NATIVETEX_D3D9
        {
            // Independent from compatibility transformation, we have to bring framework-incompatible formats down to earth.
            eRasterFormat frmRasterFormat;
            uint32 frmDepth;
            uint32 frmRowAlignment;
            eColorOrdering frmColorOrder;

            ePaletteType frmPaletteType = PALETTE_NONE;
            void *frmPaletteData = nullptr;
            uint32 frmPaletteSize = 0;

            eCompressionType frmCompressionType = RWCOMPRESS_NONE;

            bool frmRasterFormatLink;

            size_t mipmapCount = ddsImage->mipmaps.GetCount();

            ddsNativeImage::mipmaps_t *useMipmapLayers;
            bool isNewlyAllocatedMipmaps = false;

            ddsNativeImage::mipmaps_t tmpMipmapArray( eir::constr_with_alloc::DEFAULT, engineInterface );

            try
            {
                if ( dds_usedFormatHandler )
                {
                    dds_usedFormatHandler->GetTextureRWFormat( frmRasterFormat, frmDepth, frmColorOrder );

                    frmRowAlignment = 4;    // for good measure.

                    isNewlyAllocatedMipmaps = true;

                    // Unpack the layers.
                    tmpMipmapArray.Resize( mipmapCount );

                    for ( size_t n = 0; n < mipmapCount; n++ )
                    {
                        const ddsNativeImage::mipmap_t& srcLayer = ddsImage->mipmaps[ n ];

                        uint32 surfWidth = srcLayer.width;
                        uint32 surfHeight = srcLayer.height;

                        uint32 layerWidth = srcLayer.layerWidth;
                        uint32 layerHeight = srcLayer.layerHeight;

                        const void *srcTexels = srcLayer.texels;
                        //uint32 srcDataSize = srcLayer.dataSize;

                        // Allocate the unpacked-data surface.
                        rasterRowSize dstRowSize = getRasterDataRowSize( layerWidth, frmDepth, frmRowAlignment );

#ifdef _DEBUG
                        assert( dstRowSize.mode == eRasterDataRowMode::ALIGNED );
#endif //_DEBUG

                        uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, layerHeight );

                        void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

                        try
                        {
                            // IMPORTANT: if you design a format extension, you pretty much MUST know how to convert from
                            // layer to surface dimensions yourself. This is YOUR responsibility!

                            // Transform stuff.
                            dds_usedFormatHandler->ConvertToRW(
                                srcTexels, layerWidth, layerHeight,
                                dstRowSize.byteSize, dstDataSize,
                                dstTexels
                            );
                        }
                        catch( ... )
                        {
                            engineInterface->PixelFree( dstTexels );

                            throw;
                        }

                        ddsNativeImage::mipmap_t newLayer;

                        newLayer.width = surfWidth;
                        newLayer.height = surfHeight;

                        newLayer.layerWidth = layerWidth;
                        newLayer.layerHeight = layerHeight;

                        newLayer.texels = dstTexels;
                        newLayer.dataSize = dstDataSize;

                        tmpMipmapArray[ n ] = std::move( newLayer );
                    }

                    frmRasterFormatLink = true;

                    useMipmapLayers = &tmpMipmapArray;
                }
                else
                {
                    frmRasterFormat = dds_rasterFormat;
                    frmDepth = dds_bitDepth;
                    frmRowAlignment = getDDSRasterDataRowAlignment();
                    frmColorOrder = dds_colorOrder;

                    frmPaletteType = dds_paletteType;
                    frmPaletteData = ddsImage->paletteData;
                    frmPaletteSize = dds_paletteSize;

                    frmCompressionType = dds_compressionType;

                    frmRasterFormatLink = dds_d3dRasterFormatLink;

                    useMipmapLayers = &ddsImage->mipmaps;
                    isNewlyAllocatedMipmaps = false;
                }

                // Anything else than Direct3D9 is forced to framework formats.
                // Let us make sure we got them.

                uint32 dstDepth;
                uint32 dstRowAlignment;

                eRasterFormat dstRasterFormat;
                eColorOrdering dstColorOrder;
                ePaletteType dstPaletteType;
                uint32 dstPaletteSize;

                eCompressionType dstCompressionType;

                bool hasDestinationRasterFormat = false;

                if ( frmRasterFormatLink || frmCompressionType != RWCOMPRESS_NONE )
                {
                    // We have something to worry about.
                    dstRasterFormat = frmRasterFormat;
                    dstDepth = frmDepth;
                    dstRowAlignment = frmRowAlignment;
                    dstColorOrder = frmColorOrder;
                    dstPaletteType = frmPaletteType;
                    dstPaletteSize = frmPaletteSize;
                    dstCompressionType = frmCompressionType;

                    hasDestinationRasterFormat = true;
                }

                // If we do not have our destination format by now, we have failed utterly.
                if ( !hasDestinationRasterFormat )
                {
                    throw NativeImagingInternalErrorException( "DDS", L"DDS_INTERNERR_NATTEXMOVE" );
                }

                // We must ensure that the destination format complies with the native texture input rules.
                TransformDestinationRasterFormat(
                    engineInterface,
                    dstRasterFormat, dstDepth, getDDSRasterDataRowAlignment(), dstColorOrder, dstPaletteType, dstPaletteSize, dstCompressionType,
                    dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstPaletteSize, dstCompressionType,
                    nativeTexCaps, dds_hasAlphaChannel  // not perfect alpha flag but okay.
                );

                // We never want to change palette type over here.
                assert( frmPaletteType == dstPaletteType );

                // There are cases when DDS files travel in unoptimized pixel data. The runtime can
                // automatically optimize the pixel data for hardware if you pass true to
                // SetCompatTransformNativeImaging. Doing so tells the runtime that you prefer compatibility
                // over staying true to the formats.
                bool hasRedirect = false;

                if ( dstCompressionType == RWCOMPRESS_NONE )
                {
                    assert( dds_hasBitDepth == true );

                    if ( engineInterface->GetCompatTransformNativeImaging() )
                    {
                        uint32 recDepth;
                        eColorOrdering recColorOrder;

                        bool hasRecDepth, hasRecColorOrder;

                        texProvider->GetRecommendedRasterFormat( dstRasterFormat, dstPaletteType, recDepth, hasRecDepth, recColorOrder, hasRecColorOrder );

                        if ( hasRecDepth && recDepth != dstDepth )
                        {
                            dstDepth = recDepth;

                            hasRedirect = true;
                        }

                        if ( hasRecColorOrder && recColorOrder != dstColorOrder )
                        {
                            dstColorOrder = recColorOrder;

                            hasRedirect = true;
                        }
                    }
                }

                (void)hasRedirect;

                // TODO: maybe also make sure that the surfaces meet size rules.
                // for now we error out if we get too large DDS images.

                // Create a compliant temporary mipmap vector if we need one.
                bool doMipmapsNeedConversion =
                    doesPixelDataNeedConversion(
                        *useMipmapLayers,
                        frmRasterFormat, frmDepth, frmRowAlignment, frmColorOrder, frmPaletteType, frmCompressionType,
                        dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstCompressionType
                    );

                if ( doMipmapsNeedConversion )
                {
                    if ( !isNewlyAllocatedMipmaps )
                    {
                        tmpMipmapArray.Resize( mipmapCount );
                    }

                    isNewlyAllocatedMipmaps = true;

                    for ( size_t n = 0; n < mipmapCount; n++ )
                    {
                        const ddsNativeImage::mipmap_t& srcLayer = (*useMipmapLayers)[ n ];

                        uint32 surfWidth = srcLayer.width;
                        uint32 surfHeight = srcLayer.height;

                        uint32 layerWidth = srcLayer.layerWidth;
                        uint32 layerHeight = srcLayer.layerHeight;

                        void *srcTexels = srcLayer.texels;
                        uint32 srcDataSize = srcLayer.dataSize;

                        // Alright, do the conversion.
                        uint32 dstSurfWidth = 0;
                        uint32 dstSurfHeight = 0;
                        void *dstTexels = nullptr;
                        uint32 dstDataSize = 0;

                        bool didConvert = ConvertMipmapLayerNative(
                            engineInterface,
                            surfWidth, surfHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
                            frmRasterFormat, frmDepth, frmRowAlignment, frmColorOrder, frmPaletteType, frmPaletteData, frmPaletteSize, frmCompressionType,
                            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, frmPaletteData, dstPaletteSize, dstCompressionType,
                            false,
                            dstSurfWidth, dstSurfHeight,
                            dstTexels, dstDataSize
                        );

                        assert( didConvert == true );

                        // Free the texel layer if required.
                        if ( isNewlyAllocatedMipmaps )
                        {
                            engineInterface->PixelFree( srcTexels );
                        }

                        // Store the new stuff.
                        ddsNativeImage::mipmap_t newLayer;

                        newLayer.width = dstSurfWidth;
                        newLayer.height = dstSurfHeight;

                        newLayer.layerWidth = layerWidth;
                        newLayer.layerHeight = layerHeight;

                        newLayer.texels = dstTexels;
                        newLayer.dataSize = dstDataSize;

                        tmpMipmapArray[ n ] = std::move( newLayer );
                    }

                    useMipmapLayers = &tmpMipmapArray;
                }

                // Also check if we have to convert the palette data.
                void *dstPaletteData = nullptr;

                if ( dstPaletteType != PALETTE_NONE )
                {
                    assert( frmPaletteType != PALETTE_NONE );

                    TransformPaletteData(
                        engineInterface,
                        frmPaletteData,
                        frmPaletteSize, dstPaletteSize,
                        frmRasterFormat, frmColorOrder,
                        dstRasterFormat, dstColorOrder,
                        false,
                        dstPaletteData
                    );
                }

                bool hasDirectlyAcquired;

                try
                {
                    // Calculate the alpha flag.
                    bool hasActualAlpha =
                        frameworkCalculateHasAlpha(
                            engineInterface,
                            *useMipmapLayers,
                            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder,
                            dstPaletteType, dstPaletteData, dstPaletteSize, dstCompressionType
                        );

                    bool hasProcessedAlphaNatTex = false;

                    // Now it is time to push data to the native texture!
#ifdef RWLIB_INCLUDE_NATIVETEX_D3D8
                    if ( !hasProcessedAlphaNatTex && isDirect3D8 )
                    {
                        NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)nativeTexMem;

                        d3d8AcquirePixelDataToTexture <ddsNativeImage::mipmap_t> (
                            engineInterface,
                            nativeTex,
                            *useMipmapLayers,
                            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder,
                            dstPaletteType, dstPaletteData, dstPaletteSize, dstCompressionType,
                            dds_rasterType, dds_autoMipmaps, hasActualAlpha,
                            hasDirectlyAcquired
                        );

                        hasProcessedAlphaNatTex = true;
                    }
#endif //RWLIB_INCLUDE_NATIVETEX_D3D8

#ifdef RWLIB_INCLUDE_NATIVETEX_XBOX
                    if ( !hasProcessedAlphaNatTex && isXBOX )
                    {
                        NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)nativeTexMem;

                        xboxAcquirePixelDataToTexture <ddsNativeImage::mipmap_t> (
                            engineInterface,
                            nativeTex,
                            *useMipmapLayers,
                            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder,
                            dstPaletteType, dstPaletteData, dstPaletteSize, dstCompressionType,
                            dds_rasterType, hasActualAlpha, dds_autoMipmaps,
                            hasDirectlyAcquired
                        );

                        hasProcessedAlphaNatTex = true;
                    }
#endif //RWLIB_INCLUDE_NATIVETEX_XBOX

                    if ( !hasProcessedAlphaNatTex )
                    {
                        // Never happens, just for maintenance sake.
                        assert( 0 );
                    }
                }
                catch( ... )
                {
                    // Clean up on error.
                    if ( frmPaletteData != dstPaletteData )
                    {
                        engineInterface->PixelFree( dstPaletteData );
                    }

                    throw;
                }

                // If the palette buffer was acquired, we do not clean it.
                if ( frmPaletteData != dstPaletteData )
                {
                    if ( !hasDirectlyAcquired )
                    {
                        engineInterface->PixelFree( dstPaletteData );
                    }
                }

                // If the texel data has been directly acquired, we do not want to release
                // the buffers, because they are now owned by the texture!
                if ( hasDirectlyAcquired && isNewlyAllocatedMipmaps )
                {
                    useMipmapLayers->Clear();
                }

                // Let the runtime know whether the texels from the DDS have been directly taken.
                hasTakenDDSTexelBuffers = ( hasDirectlyAcquired && !isNewlyAllocatedMipmaps );

                // This is sort of legacy API, so we take palette and mipmap buffers at the same time.
                hasTakenDDSPaletteBuffer = ( hasDirectlyAcquired && ( dstPaletteData == ddsImage->paletteData ) );
            }
            catch( ... )
            {
                // If there was any exception, we must clean things up.
                if ( isNewlyAllocatedMipmaps )
                {
                    genmip::deleteMipmapLayers( engineInterface, tmpMipmapArray );
                }

                throw;
            }

            // Clean up after ourselves.
            if ( isNewlyAllocatedMipmaps )
            {
                // Free the mipmap array.
                genmip::deleteMipmapLayers( engineInterface, tmpMipmapArray );
            }
        }

        // So, have we taken the buffers?
        feedbackOut.hasDirectlyAcquired = hasTakenDDSTexelBuffers;
        feedbackOut.hasDirectlyAcquiredPalette = hasTakenDDSPaletteBuffer;

        // We are done! I guess.
    }

    void ReadFromNativeTexture( Interface *engineInterface, void *imageMem, const char *nativeTexName, void *nativeTexMem, acquireFeedback_t& feedbackOut ) const override
    {
        ddsNativeImage *ddsImage = (ddsNativeImage*)imageMem;

        // Take things from a native texture and into the image.
        ddsNativeImage::mipmaps_t srcLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

        eRasterFormat srcRasterFormat;
        uint32 srcDepth;
        uint32 srcRowAlignment;
        eColorOrdering srcColorOrder;

        ePaletteType srcPaletteType;
        void *srcPaletteData;
        uint32 srcPaletteSize;

        eCompressionType srcCompressionType;

        bool srcHasRasterLink = true;
        d3dpublic::nativeTextureFormatHandler *srcAnonymousFormatHandler = nullptr;

        D3DFORMAT dds_d3dFormat;

        bool src_hasD3DFORMAT;

        // Meta props.
        uint8 srcRasterType;
        bool srcAutoMipmaps;
        bool srcCubeMap;
        bool srcHasAlpha;

        bool src_isNewlyAllocated;
        bool src_isPaletteNewlyAllocated;

        bool hasFetchedNatTex = false;

        // Depends on what native texture we have.
#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9
        if ( !hasFetchedNatTex && strcmp( nativeTexName, "Direct3D9" ) == 0 )
        {
            // This texture needs special attention.
            NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)nativeTexMem;

            // Simple give all data to the runtime and figure conversion out at a later point.
            size_t texMipmapCount = nativeTex->mipmaps.GetCount();

            srcLayers.Resize( texMipmapCount );

            for ( size_t n = 0; n < texMipmapCount; n++ )
            {
                const NativeTextureD3D9::mipmapLayer& srcLayer = nativeTex->mipmaps[ n ];

                uint32 surfWidth = srcLayer.width;
                uint32 surfHeight = srcLayer.height;

                uint32 layerWidth = srcLayer.layerWidth;
                uint32 layerHeight = srcLayer.layerHeight;

                void *mipTexels = srcLayer.texels;
                uint32 mipDataSize = srcLayer.dataSize;

                // Put it into a DDS mipmap layer.
                ddsNativeImage::mipmap_t newLayer;

                newLayer.width = surfWidth;
                newLayer.height = surfHeight;

                newLayer.layerWidth = layerWidth;
                newLayer.layerHeight = layerHeight;

                newLayer.texels = mipTexels;
                newLayer.dataSize =  mipDataSize;

                srcLayers[ n ] = std::move( newLayer );
            }

            // Give the palette aswell, directly.
            srcPaletteData = nativeTex->palette;
            srcPaletteType = nativeTex->paletteType;
            srcPaletteSize = nativeTex->paletteSize;

            // Now pass raster format parameters.
            srcRasterFormat = nativeTex->rasterFormat;
            srcDepth = nativeTex->depth;
            srcRowAlignment = getD3DTextureDataRowAlignment();
            srcColorOrder = nativeTex->colorOrdering;

            // Thankfully the D3D9 native texture is pretty openly designed.
            srcCompressionType = nativeTex->colorComprType;

            dds_d3dFormat = nativeTex->d3dFormat;
            src_hasD3DFORMAT = true;

            // We could travel with anonymous data that has specific handling.
            // In case of incompatibility we absolutely must convert things.
            srcHasRasterLink = nativeTex->d3dRasterFormatLink;
            srcAnonymousFormatHandler = nativeTex->anonymousFormatLink;

            // Meta-props, I guess.
            srcRasterType = nativeTex->rasterType;
            srcAutoMipmaps = nativeTex->autoMipmaps;
            srcCubeMap = nativeTex->isCubeTexture;
            srcHasAlpha = nativeTex->hasAlpha;

            // We have given texel data directly, so there is no allocation happening.
            src_isNewlyAllocated = false;

            hasFetchedNatTex = true;
        }
#endif //RWLIB_INCLUDE_NATIVETEX_D3D9

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D8
        if ( !hasFetchedNatTex && strcmp( nativeTexName, "Direct3D8" ) == 0 )
        {
            NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)nativeTexMem;

            d3d8FetchPixelDataFromTexture <ddsNativeImage::mipmap_t> (
                engineInterface,
                nativeTex,
                srcLayers,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder,
                srcPaletteType, srcPaletteData, srcPaletteSize, srcCompressionType,
                srcRasterType, srcAutoMipmaps, srcHasAlpha,
                src_isNewlyAllocated
            );

            srcCubeMap = false;

            src_hasD3DFORMAT = false;

            hasFetchedNatTex = true;
        }
#endif //RWLIB_INCLUDE_NATIVETEX_D3D8

#ifdef RWLIB_INCLUDE_NATIVETEX_XBOX
        if ( !hasFetchedNatTex && strcmp( nativeTexName, "XBOX" ) == 0 )
        {
            NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)nativeTexMem;

            xboxFetchPixelDataFromTexture <ddsNativeImage::mipmap_t> (
                engineInterface,
                nativeTex,
                srcLayers,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder,
                srcPaletteType, srcPaletteData, srcPaletteSize, srcCompressionType,
                srcRasterType, srcHasAlpha, srcAutoMipmaps,
                src_isNewlyAllocated
            );

            srcCubeMap = false;

            src_hasD3DFORMAT = false;

            hasFetchedNatTex = true;
        }
#endif //RWLIB_INCLUDE_NATIVETEX_XBOX

        if ( !hasFetchedNatTex )
        {
            throw NativeImagingNativeTextureInvalidParameterException( "DDS", nativeTexName, L"NATIVETEX_FRIENDLYNAME", nullptr );
        }

        (void)srcAnonymousFormatHandler;
        (void)srcCubeMap;       // todo: actually use cubemaps.

        // We kind of synchronize palette allocation with mipmap buffer allocation.
        // This is a legacy API quirk that is bound to change.
        src_isPaletteNewlyAllocated = src_isNewlyAllocated;

        try
        {
            size_t mipmapCount = srcLayers.GetCount();

            // Even though we have a format mapping to something DDS-compatible, we must bring it
            // to actual DDS-compliancy.
            // * 4bit palette must really be 4 bits in depth.
            // * row alignment must be byte-wise, not dword-wise.
            if ( !src_hasD3DFORMAT || srcHasRasterLink )
            {
                // Map the raster format to a D3DFORMAT.
                // Then this D3DFORMAT can be mapped to a DDS format.
                eRasterFormat dstRasterFormat = srcRasterFormat;
                uint32 dstDepth = srcDepth;
                uint32 dstRowAlignment = getDDSRasterDataRowAlignment();
                eColorOrdering dstColorOrder = srcColorOrder;

                ePaletteType dstPaletteType = srcPaletteType;
                uint32 dstPaletteSize = srcPaletteSize;

                eCompressionType dstCompressionType = srcCompressionType;

                if ( dstCompressionType == RWCOMPRESS_NONE )
                {
                    if ( dstPaletteType != PALETTE_NONE )
                    {
                        // We want to be doing default stuff.
                        if ( dstPaletteType == PALETTE_4BIT || dstPaletteType == PALETTE_4BIT_LSB )
                        {
                            dstPaletteType = PALETTE_4BIT;
                            dstDepth = 4;
                        }
                        else if ( dstPaletteType == PALETTE_8BIT )
                        {
                            dstDepth = 8;
                        }
                        else
                        {
                            throw NativeImagingInternalErrorException( "DDS", L"DDS_INTERNERR_UNSUPPPAL" );
                        }

                        // We want DDS color samples.
                        getDDSPaletteRasterFormat( dstRasterFormat, dstColorOrder );

                        if ( !src_hasD3DFORMAT )
                        {
                            // This is the D3DFORMAT even if we aint gonna write this ever.
                            dds_d3dFormat = D3DFMT_P8;
                        }
                    }
                    else
                    {
                        if ( !src_hasD3DFORMAT )
                        {
                            // Luckily we can share the D3DFORMAT mapping with Direct3D9 here.
                            sharedd3dConvertRasterFormatCompatible(
                                dstRasterFormat, dstColorOrder, dstDepth,
                                dds_d3dFormat
                            );
                        }
                    }
                }
                else if ( !src_hasD3DFORMAT )
                {
                    if ( dstCompressionType == RWCOMPRESS_DXT1 )
                    {
                        dds_d3dFormat = D3DFMT_DXT1;
                    }
                    else if ( dstCompressionType == RWCOMPRESS_DXT2 )
                    {
                        dds_d3dFormat = D3DFMT_DXT2;
                    }
                    else if ( dstCompressionType == RWCOMPRESS_DXT3 )
                    {
                        dds_d3dFormat = D3DFMT_DXT3;
                    }
                    else if ( dstCompressionType == RWCOMPRESS_DXT4 )
                    {
                        dds_d3dFormat = D3DFMT_DXT4;
                    }
                    else if ( dstCompressionType == RWCOMPRESS_DXT5 )
                    {
                        dds_d3dFormat = D3DFMT_DXT5;
                    }
                    else
                    {
                        throw NativeImagingInternalErrorException( "DDS", L"DDS_INTERNERR_UNSUPPCOMPR" );
                    }
                }

                // Now we have a destination format.
                // Convert the palette if we need to.
                void *dstPaletteData = srcPaletteData;

                if ( dstPaletteType != PALETTE_NONE )
                {
                    assert( srcPaletteType != PALETTE_NONE );

                    bool doPalConv =
                        doPaletteBuffersNeedFullConversion(
                            srcRasterFormat, srcColorOrder, srcPaletteSize,
                            dstRasterFormat, dstColorOrder, dstPaletteSize
                        );

                    if ( doPalConv )
                    {
                        CopyConvertPaletteData(
                            engineInterface,
                            srcPaletteData,
                            srcPaletteSize, dstPaletteSize,
                            srcRasterFormat, srcColorOrder,
                            dstRasterFormat, dstColorOrder,
                            dstPaletteData
                        );
                    }
                }

                try
                {
                    // If the destination format is different from the source format, we want to convert the source format to it.
                    bool needsMipmapConvert =
                        doesPixelDataNeedConversion(
                            srcLayers,
                            srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcCompressionType,
                            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstCompressionType
                        );

                    // REMEMBER TO SIMPLIFY THE MIPMAP CONVERSION PIPELINE
                    // WHILE KEEPING IT OPTIMIZED.
                    // There are easy ways to introduce bugs in many places where
                    // pixels are moved from place to place.
                    // Using cached arrays is much faster than allocating memory for them.
                    // We do want to replace STL containers with our own!

                    // Convert stuff.
                    if ( needsMipmapConvert )
                    {
                        ddsNativeImage::mipmaps_t convLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

                        try
                        {
                            for ( size_t n = 0; n < mipmapCount; n++ )
                            {
                                ddsNativeImage::mipmap_t& mipLayer = srcLayers[ n ];

                                uint32 surfWidth = mipLayer.width;
                                uint32 surfHeight = mipLayer.height;

                                uint32 layerWidth = mipLayer.layerWidth;
                                uint32 layerHeight = mipLayer.layerHeight;

                                void *srcTexels = mipLayer.texels;
                                uint32 mipDataSize = mipLayer.dataSize;

                                // The destination conversion output.
                                uint32 dstSurfWidth = 0;
                                uint32 dstSurfHeight = 0;
                                void *dstTexels = nullptr;
                                uint32 dstDataSize = 0;

                                bool couldConvert = ConvertMipmapLayerNative(
                                    engineInterface,
                                    surfWidth, surfHeight, layerWidth, layerHeight, srcTexels, mipDataSize,
                                    srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder,
                                    srcPaletteType, srcPaletteData, srcPaletteSize, srcCompressionType,
                                    dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder,
                                    dstPaletteType, dstPaletteData, dstPaletteSize, dstCompressionType,
                                    false,
                                    dstSurfWidth, dstSurfHeight,
                                    dstTexels, dstDataSize
                                );

                                assert( couldConvert == true );

                                // Replace the texels.
                                if ( src_isNewlyAllocated )
                                {
                                    engineInterface->PixelFree( srcTexels );
                                }

                                mipLayer.texels = nullptr;

                                // Put stuff into the destination array.
                                ddsNativeImage::mipmap_t newLayer;

                                newLayer.width = dstSurfWidth;
                                newLayer.height = dstSurfHeight;

                                newLayer.layerWidth = layerWidth;
                                newLayer.layerHeight = layerHeight;

                                newLayer.texels = dstTexels;
                                newLayer.dataSize = dstDataSize;

                                convLayers.AddToBack( std::move( newLayer ) );
                            }
                        }
                        catch( ... )
                        {
                            // On error we have to clear our temporary converted layers.
                            genmip::deleteMipmapLayers( engineInterface, convLayers );

                            throw;
                        }

                        // Replace the source arrays.
                        // TODO: optimize this with RW types.
                        srcLayers = std::move( convLayers );

                        // We have allocated new copies.
                        src_isNewlyAllocated = true;
                    }

                    // Now the color data is in a proper format!
                }
                catch( ... )
                {
                    // Free the palette data due to some error.
                    if ( dstPaletteData )
                    {
                        if ( srcPaletteData != dstPaletteData )
                        {
                            engineInterface->PixelFree( dstPaletteData );
                        }
                    }

                    throw;
                }

                // Update the raster format properties.
                srcRasterFormat = dstRasterFormat;
                srcDepth = dstDepth;
                srcRowAlignment = dstRowAlignment;
                srcColorOrder = dstColorOrder;

                src_isPaletteNewlyAllocated = ( srcPaletteData != dstPaletteData );

                srcPaletteType = dstPaletteType;
                srcPaletteSize = dstPaletteSize;
                srcPaletteData = dstPaletteData;

                srcCompressionType = dstCompressionType;
            }
            else
            {
                // In this case we have no framework-compliancy of the given source raster format.
                // This means we can operate on bitDepth and row alignment only.
                // Palette formats are excluded from this pass, factually.

                // Another contract we have to fulfill is completing DDS properties (bit depth fyi).

                uint32 dds_bitDepth = 0;

                assert( src_hasD3DFORMAT == true );

                bool hasFormatPitch = dds_header::doesFormatSupportPitch( dds_d3dFormat );

                // Formats that support pitch are automatically raw-sample types.
                bool gotDepth = getD3DFORMATBitDepth( dds_d3dFormat, dds_bitDepth );

                // We make sure just in case, so check for depth aswell.
                if ( gotDepth && hasFormatPitch )
                {
                    // We use the practical design principle that D3D data is in general formatted using
                    // most significant byte addressing mode.
                    const eir::eByteAddressingMode d3dByteAddr = eir::eByteAddressingMode::MOST_SIGNIFICANT;

                    uint32 dstRowAlignment = getDDSRasterDataRowAlignment();

                    // The only thing we really do here is check if the alignment is broken.
                    // Everything else is taken care of.
                    bool doMipNeedRealign =
                        doesPixelDataNeedAddressabilityAdjustment(
                            srcLayers,
                            dds_bitDepth, srcRowAlignment,
                            dds_bitDepth, dstRowAlignment
                        );

                    if ( doMipNeedRealign )
                    {
                        // We have to allocate new layers then which match the alignment.
                        ddsNativeImage::mipmaps_t convLayers( eir::constr_with_alloc::DEFAULT, engineInterface );

                        try
                        {
                            size_t mipmapCount = srcLayers.GetCount();

                            convLayers.Resize( mipmapCount );

                            for ( size_t n = 0; n < mipmapCount; n++ )
                            {
                                ddsNativeImage::mipmap_t& srcLayer = srcLayers[ n ];

                                uint32 surfWidth = srcLayer.width;
                                uint32 surfHeight = srcLayer.height;

                                uint32 layerWidth = srcLayer.layerWidth;
                                uint32 layerHeight = srcLayer.layerHeight;

                                void *mipTexels = srcLayer.texels;
                                uint32 mipDataSize = srcLayer.dataSize;

                                // Readjust the stuff.
                                void *dstTexels = nullptr;
                                uint32 dstDataSize = 0;

                                DepthCopyTransformMipmapLayer(
                                    engineInterface,
                                    layerWidth, layerHeight, mipTexels, mipDataSize,
                                    dds_bitDepth, srcRowAlignment, d3dByteAddr,
                                    dds_bitDepth, dstRowAlignment, d3dByteAddr,
                                    dstTexels, dstDataSize
                                );

                                // Free memory that aint required anymore.
                                if ( src_isNewlyAllocated )
                                {
                                    engineInterface->PixelFree( mipTexels );
                                }

                                // Clean this up to properly tell that the surface data is disregarded.
                                // Why? Because data is cleaned up on exception in multiple layers.
                                srcLayer.texels = nullptr;

                                // Then store the new data that is _actually_ DDS compliant.
                                ddsNativeImage::mipmap_t newLayer;

                                newLayer.width = surfWidth;
                                newLayer.height = surfHeight;

                                newLayer.layerWidth = layerWidth;
                                newLayer.layerHeight = layerHeight;

                                newLayer.texels = dstTexels;
                                newLayer.dataSize = dstDataSize;

                                convLayers[ n ] = std::move( newLayer );
                            }
                        }
                        catch( ... )
                        {
                            // Clean up temporary data because something went horribly wrong.
                            genmip::deleteMipmapLayers( engineInterface, convLayers );

                            throw;
                        }

                        // Store the stuff for the runtime now.
                        // TODO: of course optimize using RW types because
                        // here memory is temporarily allocated.
                        srcLayers = std::move( convLayers );

                        // We now are definately newly allocated.
                        src_isNewlyAllocated = true;

                        // Update properties, just in case.
                        srcRowAlignment = dstRowAlignment;
                    }
                }

                // Remember the bit depth.
                srcDepth = dds_bitDepth;
            }

            // At this point we have a D3DFORMAT linked to a RW raster format.
            // Now we can get the DDS mapping for it!
            // We can also have a palette format, in which case we do not really have a D3DFORMAT.

            eDDSPixelFormatType formatType;
            uint32 dds_redMask = 0;
            uint32 dds_greenMask = 0;
            uint32 dds_blueMask = 0;
            uint32 dds_alphaMask = 0;
            uint32 dds_fourCC = 0;

            // Check for a special mapping first that is most compatible.
            if ( srcPaletteType != PALETTE_NONE )
            {
                // Since we travel in palette data, we need a special mapping.
                if ( srcPaletteType == PALETTE_4BIT )
                {
                    formatType = eDDSPixelFormatType::FMT_PAL4;
                }
                else if ( srcPaletteType == PALETTE_8BIT )
                {
                    formatType = eDDSPixelFormatType::FMT_PAL8;
                }
                else
                {
                    throw NativeImagingInternalErrorException( "DDS", L"DDS_INTERNERR_UNSUPPDSTPAL" );
                }
            }
            else
            {
                bool hasCompatMapping =
                    getDDSMappingFromD3DFormat(
                        dds_d3dFormat,
                        formatType,
                        dds_redMask, dds_greenMask, dds_blueMask, dds_alphaMask, dds_fourCC
                    );

                if ( !hasCompatMapping )
                {
                    // We can map this format in a special post-D3D8 format.
                    // Fuck (absolute) legacy-support anyway.
                    formatType = eDDSPixelFormatType::FMT_FOURCC;

                    dds_fourCC = (uint32)dds_d3dFormat;
                }
            }

            // Decide whether the data has an alpha channel.
            bool dds_hasAlphaChannel = false;
            {
                // That is all that is actually needed I think.
                if ( dds_alphaMask != 0 )
                {
                    dds_hasAlphaChannel = true;
                }
            }

            // Everything is kinda prepared!
            memset( ddsImage->reserved1, 0, sizeof( ddsImage->reserved1 ) );
            ddsImage->pf_type = formatType;
            ddsImage->pf_hasAlphaChannel = dds_hasAlphaChannel;
            ddsImage->pf_fourCC = dds_fourCC;
            ddsImage->pf_redMask = dds_redMask;
            ddsImage->pf_greenMask = dds_greenMask;
            ddsImage->pf_blueMask = dds_blueMask;
            ddsImage->pf_alphaMask = dds_alphaMask;
            ddsImage->bitDepth = srcDepth;
            ddsImage->caps2 = 0;            // TODO: actually add cubemap support to this!
            ddsImage->caps3 = 0;
            ddsImage->caps4 = 0;
            ddsImage->reserved2 = 0;

            // Store the surface layers.
            // In the ideal case, we do not allocate any unneccessary memory!
            ddsImage->mipmaps = std::move( srcLayers );

            // And the palette data.
            ddsImage->paletteData = srcPaletteData;

            // Tell the runtime about stuff.
            feedbackOut.hasDirectlyAcquired = ( !src_isNewlyAllocated );
            feedbackOut.hasDirectlyAcquiredPalette = ( !src_isPaletteNewlyAllocated );
        }
        catch( ... )
        {
            // Free memory on error if it was allocated.
            if ( src_isNewlyAllocated )
            {
                // Clear the layers.
                genmip::deleteMipmapLayers( engineInterface, srcLayers );

                // We also have to clear the palette, if available.
                if ( srcPaletteData )
                {
                    engineInterface->PixelFree( srcPaletteData );
                }
            }

            throw;
        }

        // Done.
    }

    inline static bool calculateDDSBitDepth(
        uint32 pixelFormatFlags,
        bool hasValidFourCC, uint32 fourCC,
        const dds_header& header,
        uint32& bitDepth_out
    )
    {
        // Try to get the bitDepth conventionally.
        {
            // We could have a valid bit depth stored in the header instead.
            if ( pixelFormatFlags & ( DDPF_RGB | DDPF_YUV | DDPF_LUMINANCE ) )
            {
                // We take the value from the header.
                bitDepth_out = header.ddspf.dwRGBBitCount;
                return true;
            }
            else if ( pixelFormatFlags & DDPF_PALETTEINDEXED1 )
            {
                bitDepth_out = 1;
                return true;
            }
            else if ( pixelFormatFlags & DDPF_PALETTEINDEXED2 )
            {
                bitDepth_out = 2;
                return true;
            }
            else if ( pixelFormatFlags & DDPF_PALETTEINDEXED4 )
            {
                bitDepth_out = 4;
                return true;
            }
            else if ( pixelFormatFlags & DDPF_PALETTEINDEXED8 )
            {
                bitDepth_out = 8;
                return true;
            }
        }

        // If we failed to get the depth from the pixel format description,
        // we could be able to fetch it from the fourCC, newly interpreted as D3DFORMAT field.
        // Should definately give it a try.
        if ( hasValidFourCC )
        {
            // We get it from the D3DFORMAT field.
            // This is not exactly a D3DFORMAT field, but the flow of time
            // has made some really retarded things.
            bool couldGet = getD3DFORMATBitDepth( (D3DFORMAT)fourCC, bitDepth_out );

            if ( couldGet )
            {
                return true;
            }
        }

        // No bit depth detected :(
        return false;
    }

    inline static bool calculateDDSMipmapDataSize(
        uint32 surfWidth, uint32 surfHeight,
        bool hasValidBitDepth, uint32 bitDepth,
        uint32& mipLevelDataSizeOut
    )
    {
        rasterRowSize mipStride;
        bool hasStride = false;

        // For this we need to have valid bit depth.
        if ( hasValidBitDepth )
        {
            mipStride = getDDSRasterDataRowSize( surfWidth, bitDepth );  // byte alignment.

            hasStride = true;
        }

        if ( hasStride )
        {
            mipLevelDataSizeOut = getRasterDataSizeByRowSize( mipStride, surfHeight );
            return true;
        }
        return false;
    }

    inline static bool verifyDDSMipmapMainLayerSize(
        uint32 mipDataSize, uint32 surfHeight,
        bool isLinearSize, bool isPitch, uint32 mainLayerPitchOrLinearSize
    )
    {
        uint32 mainLayerDataSize;

        if ( isLinearSize )
        {
            // We have a data size already.
            mainLayerDataSize = mainLayerPitchOrLinearSize;
        }
        else if ( isPitch )
        {
            // We must calculate using the pitch.
            rasterRowSize rowSize = rasterRowSize::from_bytesize( mainLayerPitchOrLinearSize );

            mainLayerDataSize = getRasterDataSizeByRowSize( rowSize, surfHeight );
        }
        else
        {
            // If we did not get any property, there is nothing to verify.
            return true;
        }

        // Check if we basically match in sizes.
        return ( mainLayerDataSize == mipDataSize );
    }

    bool IsStreamNativeImage( Interface *engineInterface, Stream *inputStream ) const override
    {
        // We check kinda thoroughly.
        char magic[4];
        {
            size_t magicReadCount = inputStream->read( magic, sizeof( magic ) );

            if ( magicReadCount != sizeof( magic ) )
            {
                return false;
            }
        }

        if ( magic[0] != 'D' || magic[1] != 'D' || magic[2] != 'S' || magic[3] != ' ' )
        {
            return false;
        }

        dds_header header;
        {
            size_t headerReadCount = inputStream->read( &header, sizeof( header ) );

            if ( headerReadCount != sizeof( header ) )
            {
                return false;
            }
        }

        // Check some basic semantic properties of the header.
        if ( header.dwSize != sizeof( header ) )
        {
            return false;
        }

        if ( header.ddspf.dwSize != sizeof( header.ddspf ) )
        {
            return false;
        }

        if ( header.ddspf.dwFourCC == MAKEFOURCC_RW( 'D', 'X', '1', '0' ) )
        {
            // Direct3D 9 cannot understand DirectX 10 rasters.
            return false;
        }

        uint32 ddsFlags = header.dwFlags;

        uint32 pixelFormatFlags = header.ddspf.dwFlags;

        uint32 fourCC = header.ddspf.dwFourCC;
        bool hasValidFourCC = ( pixelFormatFlags & DDPF_FOURCC ) != 0;

        uint32 baseLayerWidth = header.dwWidth;
        uint32 baseLayerHeight = header.dwHeight;

        // Prepare the bit depth, if we ever get to use it.
        uint32 bitDepth;

        bool hasValidBitDepth =
            calculateDDSBitDepth(
                pixelFormatFlags,
                hasValidFourCC, fourCC,
                header,
                bitDepth
            );

        // Get the pitch or linear size.
        // Either way, we should be able to understand this.
        uint32 mainSurfacePitchOrLinearSize = header.dwPitchOrLinearSize;

        bool isLinearSize = ( ( ddsFlags & DDSD_LINEARSIZE ) != 0 );
        bool isPitch = ( ( ddsFlags & DDSD_PITCH ) != 0 );

        // Check that we have all the mipmap data.
        uint32 mipCount = ( ( ddsFlags & DDSD_MIPMAPCOUNT ) != 0 ? (uint32)header.dwMipmapCount : (uint32)1 );

        mipGenLevelGenerator mipGen( baseLayerWidth, baseLayerHeight );

        if ( mipGen.isValidLevel() )
        {
            for ( uint32 n = 0; n < mipCount; n++ )
            {
                bool hasThisLevel = true;

                if ( n != 0 )
                {
                    hasThisLevel = mipGen.incrementLevel();
                }

                if ( !hasThisLevel )
                {
                    break;
                }

                uint32 mipLayerWidth = mipGen.getLevelWidth();
                uint32 mipLayerHeight = mipGen.getLevelHeight();

                // Get the surface dimensions.
                uint32 surfWidth, surfHeight;

                calculateSurfaceDimensions(
                    mipLayerWidth, mipLayerHeight,
                    hasValidFourCC, fourCC,
                    surfWidth, surfHeight
                );

                // Verify that we have this mipmap data.
                uint32 mipLevelDataSize;

                bool hasDataSize = calculateDDSMipmapDataSize(
                    surfWidth, surfHeight,
                    hasValidBitDepth, bitDepth,
                    mipLevelDataSize
                );

                if ( !hasDataSize )
                {
                    // No way we can read this.
                    break;
                }

                // Verify that this data size matches with the data size provided by the file.
                if ( n == 0 )
                {
                    if ( !verifyDDSMipmapMainLayerSize( mipLevelDataSize, surfHeight, isLinearSize, isPitch, mainSurfacePitchOrLinearSize ) )
                    {
                        // Something appears to be broken about the data size.
                        // We do not really care then.
                        break;
                    }
                }

                // Check the stream for the data.
                skipAvailable( inputStream, mipLevelDataSize );
            }
        }

        return true;
    }

    inline static bool shouldWriteDDSUsingLinearSize( eDDSPixelFormatType formatType, uint32 fourCC )
    {
        bool doWriteUsingLinearSize = false;

        if ( formatType == eDDSPixelFormatType::FMT_FOURCC )
        {
            // We write using linear size if the format is known to not support pitch.
            bool doesSupportPitch = dds_header::doesFormatSupportPitch( (D3DFORMAT)fourCC );

            doWriteUsingLinearSize = ( doesSupportPitch == false );
        }

        return doWriteUsingLinearSize;
    }

    void ReadNativeImage( Interface *engineInterface, void *imageMem, Stream *inputStream ) const override
    {
        // Check the magic number.
        {
            char magic[4];

            size_t magicReadCount = inputStream->read( &magic, sizeof( magic ) );

            if ( magicReadCount != sizeof( magic ) || ( magic[0] != 'D' || magic[1] != 'D' || magic[2] != 'S' || magic[3] != ' ' ) )
            {
                // Here, instead of in the checking algorithm, we throw descriptive exceptions for malformations.
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_MAGIC" );
            }
        }

        // TODO: add more debug output about strange flag configurations that we do not support.

        // Read its header.
        dds_header header;
        {
            size_t headerReadCount = inputStream->read( &header, sizeof( header ) );

            if ( headerReadCount != sizeof( header ) )
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_HEADERFAIL" );
            }
        }

        // Verify some important things.
        if ( header.dwSize != sizeof( header ) )
        {
            throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_HEADERSIZE" );
        }

        if ( header.ddspf.dwSize != sizeof( header.ddspf ) )
        {
            throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_HEADERPIXFMTSIZE" );
        }

        // Parse the main header flags.
        // We want to verify basic things that have to be valid.
        uint32 ddsFlags = header.dwFlags;

        if ( ( ddsFlags & DDSD_DEPTH ) != 0 )
        {
            throw NativeImagingInternalErrorException( "DDS", L"DDS_INTERNERR_NODEPTH" );
        }

        if ( ( ddsFlags & DDSD_CAPS ) == 0 )
        {
            engineInterface->PushWarningToken( L"DDS_WARN_MISSING_DDSD_CAPS" );
        }

        if ( ( ddsFlags & DDSD_HEIGHT ) == 0 )
        {
            engineInterface->PushWarningToken( L"DDS_WARN_MISSING_DDSD_HEIGHT" );
        }

        if ( ( ddsFlags & DDSD_WIDTH ) == 0 )
        {
            engineInterface->PushWarningToken( L"DDS_WARN_MISSING_DDSD_WIDTH" );
        }

        if ( ( ddsFlags & DDSD_PIXELFORMAT ) == 0 )
        {
            engineInterface->PushWarningToken( L"DDS_WARN_MISSING_DDSD_PIXELFORMAT" );
        }

        // Store the contents of the DDS file in our structs.
        ddsNativeImage *ddsImage = (ddsNativeImage*)imageMem;

        memcpy( ddsImage->reserved1, header.dwReserved1, sizeof( ddsImage->reserved1 ) );

        // We have to process the image data.
        // For that we prefer fully validating the data.
        bool hasMipmapCount = ( ddsFlags & DDSD_MIPMAPCOUNT ) != 0;

        // For that we will get rid of the actual pixel format flags and put things
        // into a more understandable format.

        uint32 ddsPixelFlags = header.ddspf.dwFlags;

        bool hasAlpha = ( ddsPixelFlags & DDPF_ALPHAPIXELS ) != 0;
        bool hasFourCC = ( ddsPixelFlags & DDPF_FOURCC ) != 0;

        // We must properly map the format.
        eDDSPixelFormatType formatType;
        {
            bool hasRGB = ( ddsPixelFlags & DDPF_RGB ) != 0;
            bool hasYUV = ( ddsPixelFlags & DDPF_YUV ) != 0;
            bool hasLum = ( ddsPixelFlags & DDPF_LUMINANCE ) != 0;
            bool hasAlphaOnly = ( ddsPixelFlags & DDPF_ALPHA ) != 0;
            bool hasPal4 = ( ddsPixelFlags & DDPF_PALETTEINDEXED4 ) != 0;
            bool hasPal8 = ( ddsPixelFlags & DDPF_PALETTEINDEXED8 ) != 0;

            if ( hasRGB )
            {
                formatType = eDDSPixelFormatType::FMT_RGB;
            }
            else if ( hasYUV )
            {
                formatType = eDDSPixelFormatType::FMT_YUV;
            }
            else if ( hasLum )
            {
                formatType = eDDSPixelFormatType::FMT_LUM;
            }
            else if ( hasAlphaOnly )
            {
                formatType = eDDSPixelFormatType::FMT_ALPHA;
            }
            else if ( hasFourCC )
            {
                formatType = eDDSPixelFormatType::FMT_FOURCC;
            }
            else if ( hasPal4 )
            {
                formatType = eDDSPixelFormatType::FMT_PAL4;
            }
            else if ( hasPal8 )
            {
                formatType = eDDSPixelFormatType::FMT_PAL8;
            }
            else
            {
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_COLORFMTDETECT" );
            }

            // Is there any format conflict in this DDS file?
            if ( ( hasRGB && formatType != eDDSPixelFormatType::FMT_RGB ) ||
                 ( hasYUV && formatType != eDDSPixelFormatType::FMT_YUV ) ||
                 ( hasLum && formatType != eDDSPixelFormatType::FMT_LUM ) ||
                 ( hasAlphaOnly && formatType != eDDSPixelFormatType::FMT_ALPHA ) ||
                 ( hasPal4 && formatType != eDDSPixelFormatType::FMT_PAL4 ) ||
                 ( hasPal8 && formatType != eDDSPixelFormatType::FMT_PAL8 ) )
            {
                // Tell the user about any format conflicts.
                rwStaticString <wchar_t> formatlist;

                if ( hasRGB )
                {
                    formatlist += getDDSPixelFormatTypeName( eDDSPixelFormatType::FMT_RGB );
                }

                if ( hasYUV )
                {
                    if ( formatlist.IsEmpty() == false )
                    {
                        formatlist += L" ";
                    }

                    formatlist += getDDSPixelFormatTypeName( eDDSPixelFormatType::FMT_YUV );
                }

                if ( hasLum )
                {
                    if ( formatlist.IsEmpty() == false )
                    {
                        formatlist += L" ";
                    }

                    formatlist += getDDSPixelFormatTypeName( eDDSPixelFormatType::FMT_LUM );
                }

                if ( hasAlphaOnly )
                {
                    if ( formatlist.IsEmpty() == false )
                    {
                        formatlist += L" ";
                    }

                    formatlist += getDDSPixelFormatTypeName( eDDSPixelFormatType::FMT_ALPHA );
                }

                if ( hasPal4 )
                {
                    if ( formatlist.IsEmpty() == false )
                    {
                        formatlist += L" ";
                    }

                    formatlist += getDDSPixelFormatTypeName( eDDSPixelFormatType::FMT_PAL4 );
                }

                if ( hasPal8 )
                {
                    if ( formatlist.IsEmpty() == false )
                    {
                        formatlist += L" ";
                    }

                    formatlist += getDDSPixelFormatTypeName( eDDSPixelFormatType::FMT_PAL8 );
                }

                languageTokenMap_t templ_map;
                templ_map[ L"fmtlist" ] = std::move( formatlist );
                templ_map[ L"fmtsel" ] = getDDSPixelFormatTypeName( formatType );

                engineInterface->PushWarningTemplate( L"DDS_WARN_AMBIGUOUSFMT_TEMPLATE", templ_map );
            }
        }

        // Store the format properties.
        uint32 fourCC = header.ddspf.dwFourCC;

        ddsImage->pf_type = formatType;
        ddsImage->pf_fourCC = fourCC;
        ddsImage->pf_redMask = header.ddspf.dwRBitMask;
        ddsImage->pf_greenMask = header.ddspf.dwGBitMask;
        ddsImage->pf_blueMask = header.ddspf.dwBBitMask;
        ddsImage->pf_alphaMask = header.ddspf.dwABitMask;
        ddsImage->pf_hasAlphaChannel = hasAlpha;

        // Prepare some meta-props.
        uint32 maybeMipmapCount = ( hasMipmapCount ? (uint32)header.dwMipmapCount : (uint32)1u );

        // Next we should verify the capabilities of our DDS surface.
        {
            bool isComplex = ( header.dwCaps & DDSCAPS_COMPLEX ) != 0;

            if ( !isComplex && maybeMipmapCount > 1 )
            {
                engineInterface->PushWarningToken( L"DDS_WARN_MIPMAPS_NOCOMPLEX" );
            }

            if ( ( header.dwCaps & DDSCAPS_TEXTURE ) == 0 )
            {
                engineInterface->PushWarningToken( L"DDS_WARN_MISSING_DDSCAPS_TEXTURE" );
            }

            // We ignore any cubemap settings.
        }

        // We do not care about storing caps1 because it appears to have generation-based properties.
        // Other caps should be taken into account I guess.
        ddsImage->caps2 = header.dwCaps2;
        ddsImage->caps3 = header.dwCaps3;
        ddsImage->caps4 = header.dwCaps4;
        ddsImage->reserved2 = header.dwReserved2;

        // Read the palette, if required.
        // The palette is always in 32bit RGBA color.
        {
            void *ddsPaletteData = nullptr;

            if ( formatType == eDDSPixelFormatType::FMT_PAL4 ||
                 formatType == eDDSPixelFormatType::FMT_PAL8 )
            {
                uint32 ddsPaletteSize = 0;

                if ( formatType == eDDSPixelFormatType::FMT_PAL4 )
                {
                    ddsPaletteSize = 16;
                }
                else if ( formatType == eDDSPixelFormatType::FMT_PAL8 )
                {
                    ddsPaletteSize = 256;
                }
                else
                {
                    assert( 0 );
                }

                // Determine the raster format that is used for the DDS file.
                eRasterFormat palRasterFormat;
                eColorOrdering palColorOrder;

                getDDSPaletteRasterFormat( palRasterFormat, palColorOrder );

                // We got all properties, so read the palette!
                uint32 palRasterFormatDepth = Bitmap::getRasterFormatDepth( palRasterFormat );

                uint32 palDataSize = getPaletteDataSize( ddsPaletteSize, palRasterFormatDepth );

                checkAhead( inputStream, palDataSize );

                // Get the palette.
                void *paletteData = engineInterface->PixelAllocate( palDataSize );

                try
                {
                    size_t readCount = inputStream->read( paletteData, palDataSize );

                    if ( readCount != palDataSize )
                    {
                        throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_RDPALBUF" );
                    }
                }
                catch( ... )
                {
                    engineInterface->PixelFree( paletteData );

                    throw;
                }

                // Return stuff.
                ddsPaletteData = paletteData;
            }

            // Store the palette in the native image.
            // By storing it in the native image, we will clear it automatically on exception.
            ddsImage->paletteData = ddsPaletteData;
        }

        // Finally, the color data has to be stored in the native image.
        // We take things exactly as they are compliant to DDS format.
        uint32 bitDepth;

        bool hasValidBitDepth =
            calculateDDSBitDepth(
                ddsPixelFlags,
                hasFourCC, fourCC,
                header,
                bitDepth
            );

        ddsImage->bitDepth = ( hasValidBitDepth ? bitDepth : 0 );

        uint32 baseLayerWidth = header.dwWidth;
        uint32 baseLayerHeight = header.dwHeight;

        mipGenLevelGenerator mipGen( baseLayerWidth, baseLayerHeight );

        if ( !mipGen.isValidLevel() )
        {
            throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_MIPDIMS" );
        }

        uint32 mip_index = 0;

        while ( mip_index < maybeMipmapCount )
        {
            // Read this mipmap.
            bool gotLayer = true;

            if ( mip_index != 0 )
            {
                gotLayer = mipGen.incrementLevel();
            }

            if ( !gotLayer )
            {
                break;
            }

            uint32 mipLayerWidth = mipGen.getLevelWidth();
            uint32 mipLayerHeight = mipGen.getLevelHeight();

            // We need the surface dimensions of this mipmap.
            uint32 surfWidth, surfHeight;

            calculateSurfaceDimensions(
                mipLayerWidth, mipLayerHeight,
                hasFourCC, fourCC,
                surfWidth, surfHeight
            );

            // Next calculate the data size.
            uint32 mipDataSize;

            bool hasDataSize = calculateDDSMipmapDataSize(
                surfWidth, surfHeight,
                hasValidBitDepth, bitDepth,
                mipDataSize
            );

            if ( !hasDataSize )
            {
                // We cannot read mipmap data that has no valid known data size.
                throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_MIPSIZE" );
            }

            // Read the data.
            // The data size we fetch here has to be valid depending on the format, with byte-row-alignment.
            // More extensive verification is done in the native texture acquisition.
            checkAhead( inputStream, mipDataSize );

            void *mipTexels = engineInterface->PixelAllocate( mipDataSize );

            try
            {
                size_t mipDataReadCount = inputStream->read( mipTexels, mipDataSize );

                if ( mipDataReadCount != mipDataSize )
                {
                    throw NativeImagingStructuralErrorException( "DDS", L"DDS_STRUCTERR_RDMIPBUF" );
                }

                // Store the texels.
                ddsNativeImage::mipmap_t newLayer;
                newLayer.width = surfWidth;
                newLayer.height = surfHeight;
                newLayer.layerWidth = mipLayerWidth;
                newLayer.layerHeight = mipLayerHeight;
                newLayer.texels = mipTexels;
                newLayer.dataSize = mipDataSize;

                ddsImage->mipmaps.AddToBack( std::move( newLayer ) );
            }
            catch( ... )
            {
                engineInterface->PixelFree( mipTexels );

                throw;
            }

            // Next mipmap layer.
            mip_index++;
        }

        // We have completely read the DDS native image.
    }

    void WriteNativeImage( Interface *engineInterface, const void *imageMem, Stream *outputStream ) const override
    {
        // Write this DDS native image into a file.
        const ddsNativeImage *ddsImage = (const ddsNativeImage*)imageMem;

        size_t mipmapCount = ddsImage->mipmaps.GetCount();

        if ( mipmapCount == 0 )
        {
            throw NativeImagingInvalidConfigurationException( "DDS", L"DDS_INVALIDCFG_WRITE_NODATA" );
        }

        // First, we write the magic number.
        char magic[4];
        magic[0] = 'D';
        magic[1] = 'D';
        magic[2] = 'S';
        magic[3] = ' ';

        outputStream->write( magic, sizeof( magic ) );

        // Next I guess the header.
        // Since we maintain a pretty close-to-the-native object, we can write it directly.
        eDDSPixelFormatType formatType = ddsImage->pf_type;

        uint32 pixelFormatFlags = 0;

        if ( formatType == eDDSPixelFormatType::FMT_RGB )
        {
            pixelFormatFlags |= DDPF_RGB;
        }
        else if ( formatType == eDDSPixelFormatType::FMT_YUV )
        {
            pixelFormatFlags |= DDPF_YUV;
        }
        else if ( formatType == eDDSPixelFormatType::FMT_LUM )
        {
            pixelFormatFlags |= DDPF_LUMINANCE;
        }
        else if ( formatType == eDDSPixelFormatType::FMT_FOURCC )
        {
            pixelFormatFlags |= DDPF_FOURCC;
        }
        else if ( formatType == eDDSPixelFormatType::FMT_ALPHA )
        {
            pixelFormatFlags |= DDPF_ALPHA;
        }
        else if ( formatType == eDDSPixelFormatType::FMT_PAL4 )
        {
            pixelFormatFlags |= DDPF_PALETTEINDEXED4;
        }
        else if ( formatType == eDDSPixelFormatType::FMT_PAL8 )
        {
            pixelFormatFlags |= DDPF_PALETTEINDEXED8;
        }
        else
        {
            throw NativeImagingInvalidConfigurationException( "DDS", L"DDS_INVALIDCFG_WRITE_UNKFMT" );
        }

        if ( ddsImage->pf_hasAlphaChannel )
        {
            pixelFormatFlags |= DDPF_ALPHAPIXELS;
        }

        // We need to generate the main flags.
        uint32 ddsFlags = ( DDSD_HEIGHT | DDSD_WIDTH | DDSD_CAPS | DDSD_PIXELFORMAT );

        bool hasMipmapCount = ( mipmapCount != 1 );

        if ( hasMipmapCount )
        {
            ddsFlags |= DDSD_MIPMAPCOUNT;
        }

        // Determine whether this DDS should be written using data sizes instead of pitch.
        uint32 fourCC = ddsImage->pf_fourCC;

        bool doWriteUsingLinearSize = shouldWriteDDSUsingLinearSize( formatType, fourCC );

        if ( doWriteUsingLinearSize )
        {
            ddsFlags |= DDSD_LINEARSIZE;
        }
        else
        {
            ddsFlags |= DDSD_PITCH;
        }

        // There is always a sort of bit depth to DDS surfaces.
        uint32 bitDepth = ddsImage->bitDepth;

        // Time to get properties about the base layer.
        // We can do that because we ensure that we do not write "empty" DDS native textures.
        uint32 baseLayerWidth, baseLayerHeight;
        uint32 baseLayerPitchOrLinearSize;
        {
            const ddsNativeImage::mipmap_t& baseLayer = ddsImage->mipmaps[ 0 ];

            baseLayerWidth = baseLayer.layerWidth;
            baseLayerHeight = baseLayer.layerHeight;

            if ( doWriteUsingLinearSize )
            {
                baseLayerPitchOrLinearSize = baseLayer.dataSize;
            }
            else
            {
                if ( bitDepth == 0 )
                {
                    throw NativeImagingInvalidConfigurationException( "DDS", L"DDS_INVALIDCFG_WRITE_BITDEPTH" );
                }

                baseLayerPitchOrLinearSize = getDDSRasterDataRowSize( baseLayer.width, bitDepth ).byteSize;
            }
        }

        // Finish up the header.
        dds_header header;
        header.dwSize = sizeof( dds_header );
        header.dwFlags = ddsFlags;
        header.dwHeight = baseLayerHeight;
        header.dwWidth = baseLayerWidth;
        header.dwPitchOrLinearSize = baseLayerPitchOrLinearSize;
        header.dwDepth = 0; // we do not support this.
        header.dwMipmapCount = (uint32)mipmapCount;
        memcpy( header.dwReserved1, ddsImage->reserved1, sizeof( header.dwReserved1 ) );

        // Pixel information.
        header.ddspf.dwSize = sizeof( header.ddspf );
        header.ddspf.dwFlags = pixelFormatFlags;
        header.ddspf.dwFourCC = ( formatType == eDDSPixelFormatType::FMT_FOURCC ? fourCC : 0 );
        header.ddspf.dwRGBBitCount = ( doWriteUsingLinearSize == false ? bitDepth : 0 );
        header.ddspf.dwRBitMask = ddsImage->pf_redMask;
        header.ddspf.dwGBitMask = ddsImage->pf_greenMask;
        header.ddspf.dwBBitMask = ddsImage->pf_blueMask;
        header.ddspf.dwABitMask = ddsImage->pf_alphaMask;

        // TODO: add support for more DDS flags, especially cubemaps.

        // Now the remainder, including capabilities.
        uint32 caps1 = DDSCAPS_TEXTURE;
        {
            if ( mipmapCount > 1 )
            {
                caps1 |= DDSCAPS_MIPMAP | DDSCAPS_COMPLEX;
            }
        }

        header.dwCaps = caps1;
        header.dwCaps2 = ddsImage->caps2;
        header.dwCaps3 = ddsImage->caps3;
        header.dwCaps4 = ddsImage->caps4;
        header.dwReserved2 = ddsImage->reserved2;

        // Write the header!
        outputStream->write( &header, sizeof( header ) );

        // Write the palette, if present.
        if ( isDDSPixelFormatPalette( formatType ) )
        {
            uint32 paletteSize = getDDSPixelFormatPaletteCount( formatType );

            eRasterFormat rasterFormat;
            eColorOrdering colorOrder;

            getDDSPaletteRasterFormat( rasterFormat, colorOrder );

            uint32 palRasterFormatDepth = Bitmap::getRasterFormatDepth( rasterFormat );

            uint32 palDataSize = getPaletteDataSize( paletteSize, palRasterFormatDepth );

            // Just write it.
            outputStream->write( ddsImage->paletteData, palDataSize );
        }

        // Now write all mipmaps.
        for ( size_t mip_index = 0; mip_index < mipmapCount; mip_index++ )
        {
            const ddsNativeImage::mipmap_t& srcLayer = ddsImage->mipmaps[ mip_index ];

            // We just write things, because we make sure things are properly formatted.
            uint32 mipDataSize = srcLayer.dataSize;

            const void *srcTexels = srcLayer.texels;

            outputStream->write( srcTexels, mipDataSize );
        }

        // Done!
    }

    // General initialization.
    inline void Initialize( EngineInterface *engineInterface )
    {
        // Register ourselves.
        RegisterNativeImageType(
            engineInterface,
            this,
            "DDS", sizeof( ddsNativeImage ), alignof( ddsNativeImage ), "DirectDraw Surface",
            dds_file_extensions, countof( dds_file_extensions ),
            dds_supported_nat_textures, countof( dds_supported_nat_textures )
        );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Unregister ourselves.
        UnregisterNativeImageType( engineInterface, "DDS" );
    }
};

static optional_struct_space <PluginDependantStructRegister <ddsNativeImageFormatTypeManager, RwInterfaceFactory_t>> ddsNativeImageFormatTypeManagerRegister;

void registerDDSNativeImageFormatEnv( void )
{
    ddsNativeImageFormatTypeManagerRegister.Construct( engineFactory );
}

void unregisterDDSNativeImageFormatEnv( void )
{
    ddsNativeImageFormatTypeManagerRegister.Destroy();
}

};

#endif //RWLIB_INCLUDE_DDS_NATIVEIMG
