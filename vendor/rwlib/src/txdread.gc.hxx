/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.gc.hxx
*  PURPOSE:     Gamecube native texture internal header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifdef RWLIB_INCLUDE_NATIVETEX_GAMECUBE

#include "txdread.nativetex.hxx"

// The Gamecube native texture is stored in big-endian format.
#define PLATFORMDESC_GAMECUBE   6

#include "txdread.d3d.genmip.hxx"

#include "pixelformat.hxx"

#include "txdread.common.hxx"

namespace rw
{

inline uint32 getGCTextureDataRowAlignment( void )
{
    // I guess it is aligned by 4? I am not sure yet.
    return 4;
}

inline rasterRowSize getGCRasterDataRowSize( uint32 planeWidth, uint32 depth )
{
    return getRasterDataRowSize( planeWidth, depth, getGCTextureDataRowAlignment() );
}

enum eGCRasterFormat
{
    GCRASTER_DEFAULT,
    GCRASTER_565 = 2,
    GCRASTER_RGB5A3,
    GCRASTER_8888 = 5,
    GCRASTER_888 = 6
};

// Used from version 3.3.0.2
enum eGCPaletteFormat
{
    GCPALFMT_DEFAULT,
    GCPALFMT_RGB5A3,
    GCPALFMT_RGB565
};

enum eGCNativeTextureFormat : unsigned char
{
    GVRFMT_LUM_4BIT,
    GVRFMT_LUM_8BIT,
    GVRFMT_LUM_4BIT_ALPHA,
    GVRFMT_LUM_8BIT_ALPHA,
    GVRFMT_RGB565,
    GVRFMT_RGB5A3,
    GVRFMT_RGBA8888,
    GVRFMT_PAL_4BIT = 0x8,
    GVRFMT_PAL_8BIT,
    GVRFMT_CMP = 0xE
};

inline bool getGCInternalFormatFromRasterFormat( eGCRasterFormat rasterFormat, eGCNativeTextureFormat& internalFormatOut )
{
    if ( rasterFormat == GCRASTER_565 )
    {
        internalFormatOut = GVRFMT_RGB565;
        return true;
    }
    else if ( rasterFormat == GCRASTER_RGB5A3 )
    {
        internalFormatOut = GVRFMT_RGB5A3;
        return true;
    }
    else if ( rasterFormat == GCRASTER_8888 ||
              rasterFormat == GCRASTER_888 )
    {
        internalFormatOut = GVRFMT_RGBA8888;
        return true;
    }

    return false;
}

inline eGCRasterFormat getGCRasterFormatFromInternalFormat( eGCNativeTextureFormat internalFormat )
{
    if ( internalFormat == GVRFMT_RGB565 )
    {
        return GCRASTER_565;
    }
    else if ( internalFormat == GVRFMT_RGB5A3 )
    {
        return GCRASTER_RGB5A3;
    }
    else if ( internalFormat == GVRFMT_RGBA8888 )
    {
        return GCRASTER_8888;
    }

    return GCRASTER_DEFAULT;
};

inline uint32 getGCPaletteSize( eGCNativeTextureFormat internalFormat )
{
    uint32 paletteSize = 0;

    if ( internalFormat == GVRFMT_PAL_4BIT )
    {
        paletteSize = 16;
    }
    else if ( internalFormat == GVRFMT_PAL_8BIT )
    {
        paletteSize = 256;
    }

    return paletteSize;
}

inline ePaletteType getPaletteTypeFromGCNativeFormat( eGCNativeTextureFormat internalFormat )
{
    if ( internalFormat == GVRFMT_PAL_4BIT )
    {
        return PALETTE_4BIT_LSB;
    }
    else if ( internalFormat == GVRFMT_PAL_8BIT )
    {
        return PALETTE_8BIT;
    }

    return PALETTE_NONE;
}

inline bool isGVRNativeFormatSwizzled( eGCNativeTextureFormat internalFormat )
{
    if ( internalFormat == GVRFMT_LUM_8BIT ||
         internalFormat == GVRFMT_CMP )
    {
        return false;
    }

    return true;
}

inline bool isGVRNativeFormatRawSample( eGCNativeTextureFormat internalFormat )
{
    if ( internalFormat == GVRFMT_LUM_4BIT ||
         internalFormat == GVRFMT_LUM_8BIT ||
         internalFormat == GVRFMT_LUM_4BIT_ALPHA ||
         internalFormat == GVRFMT_LUM_8BIT_ALPHA ||
         internalFormat == GVRFMT_RGB565 ||
         internalFormat == GVRFMT_RGB5A3 ||
         internalFormat == GVRFMT_RGBA8888 ||
         internalFormat == GVRFMT_PAL_4BIT ||
         internalFormat == GVRFMT_PAL_8BIT )
    {
        return true;
    }

    return false;
}

enum eGCPixelFormat : char
{
    GVRPIX_NO_PALETTE = -1,
    GVRPIX_LUM_ALPHA,
    GVRPIX_RGB565,
    GVRPIX_RGB5A3
};

inline eGCPaletteFormat getGCPaletteFormatFromPalettePixelFormat( eGCPixelFormat palettePixelFormat )
{
    if ( palettePixelFormat == GVRPIX_RGB565 )
    {
        return GCPALFMT_RGB565;
    }
    else if ( palettePixelFormat == GVRPIX_RGB5A3 )
    {
        return GCPALFMT_RGB5A3;
    }

    return GCPALFMT_DEFAULT;
}

// Used by kinda all versions.
inline void gcReadCommonRasterFormatFlags(
    const rwGenericRasterFormatFlags& rasterFormatFlags,
    ePaletteType& paletteTypeOut, bool& hasMipmapsOut, bool& autoMipmapsOut
)
{
    // We care about the palette type.
    rwSerializedPaletteType serPalType = (rwSerializedPaletteType)rasterFormatFlags.data.palType;
    {
        if ( serPalType == RWSER_PALETTE_NONE )
        {
            paletteTypeOut = PALETTE_NONE;
        }
        else if ( serPalType == RWSER_PALETTE_PAL4 )
        {
            paletteTypeOut = PALETTE_4BIT_LSB;
        }
        else if ( serPalType == RWSER_PALETTE_PAL8 )
        {
            paletteTypeOut = PALETTE_8BIT;
        }
        else
        {
            throw NativeTextureInvalidConfigurationException( "Gamecube", L"GAMECUBE_INVALIDCFG_PALFMTMAP" );
        }
    }

    // Generic properties.
    hasMipmapsOut = rasterFormatFlags.data.hasMipmaps;
    autoMipmapsOut = rasterFormatFlags.data.autoMipmaps;
}

inline bool getGVRNativeFormatFromPaletteFormat(
    eGCPixelFormat palettePixelFormat,
    eColorOrdering& colorOrderOut, eGCNativeTextureFormat& internalFormatOut
)
{
    if ( palettePixelFormat == GVRPIX_LUM_ALPHA )
    {
        internalFormatOut = GVRFMT_LUM_8BIT_ALPHA;
        colorOrderOut = COLOR_RGBA; // doesnt matter.
        return true;
    }
    else if ( palettePixelFormat == GVRPIX_RGB565 )
    {
        internalFormatOut = GVRFMT_RGB565;
        colorOrderOut = COLOR_BGRA;
        return true;
    }
    else if ( palettePixelFormat == GVRPIX_RGB5A3 )
    {
        internalFormatOut = GVRFMT_RGB5A3;
        colorOrderOut = COLOR_RGBA;
        return true;
    }

    return false;
}

inline uint32 getGCInternalFormatDepth( eGCNativeTextureFormat format )
{
    uint32 depth = 0;

    if ( format == GVRFMT_LUM_4BIT ||
         format == GVRFMT_PAL_4BIT )
    {
        depth = 4;
    }
    else if ( format == GVRFMT_LUM_4BIT_ALPHA ||
              format == GVRFMT_LUM_8BIT ||
              format == GVRFMT_PAL_8BIT )
    {
        depth = 8;
    }
    else if ( format == GVRFMT_RGB565 ||
              format == GVRFMT_RGB5A3 ||
              format == GVRFMT_LUM_8BIT_ALPHA )
    {
        depth = 16;
    }
    else if ( format == GVRFMT_RGBA8888 )
    {
        depth = 32;
    }
    else if ( format == GVRFMT_CMP )
    {
        depth = 4;
    }

    return depth;
}

// Returns the recommended raster format for the configuration.
inline bool getRecommendedGCNativeTextureRasterFormat(
    eGCNativeTextureFormat format, eGCPixelFormat paletteFormat,
    eRasterFormat& rasterFormatOut, uint32& depthOut, eColorOrdering& colorOrderOut,
    ePaletteType& paletteTypeOut,
    eCompressionType& compressionTypeOut
)
{
    bool hasFormat = true;

    if ( format == GVRFMT_PAL_4BIT ||
         format == GVRFMT_PAL_8BIT )
    {
        bool hasPaletteFormat = false;

        if ( paletteFormat == GVRPIX_LUM_ALPHA )
        {
            rasterFormatOut = RASTER_LUM_ALPHA;
            colorOrderOut = COLOR_RGBA;

            hasPaletteFormat = true;
        }
        else if ( paletteFormat == GVRPIX_RGB565 )
        {
            rasterFormatOut = RASTER_565;
            colorOrderOut = COLOR_RGBA;

            hasPaletteFormat = true;
        }
        else if ( paletteFormat == GVRPIX_RGB5A3 )
        {
            // No direct equivalent.
            rasterFormatOut = RASTER_8888;
            colorOrderOut = COLOR_RGBA;

            hasPaletteFormat = true;
        }

        if ( hasPaletteFormat )
        {
            // We must make sure that the palette description we return
            // completely matches the description of the native format!
            paletteTypeOut = getPaletteTypeFromGCNativeFormat( format );
            depthOut = getGCInternalFormatDepth( format );
        }

        hasFormat = hasPaletteFormat;

        // Palette textures are never "compressed".
        compressionTypeOut = RWCOMPRESS_NONE;
    }
    else
    {
        eCompressionType compressionType = RWCOMPRESS_NONE;

        if ( format == GVRFMT_LUM_4BIT )
        {
            rasterFormatOut = RASTER_LUM;
            depthOut = 4;
            colorOrderOut = COLOR_RGBA;
        }
        else if ( format == GVRFMT_LUM_8BIT )
        {
            rasterFormatOut = RASTER_LUM;
            depthOut = 8;
            colorOrderOut = COLOR_RGBA;
        }
        else if ( format == GVRFMT_LUM_4BIT_ALPHA )
        {
            rasterFormatOut = RASTER_LUM_ALPHA;
            depthOut = 8;
            colorOrderOut = COLOR_RGBA;
        }
        else if ( format == GVRFMT_LUM_8BIT_ALPHA )
        {
            rasterFormatOut = RASTER_LUM_ALPHA;
            depthOut = 16;
            colorOrderOut = COLOR_RGBA;
        }
        else if ( format == GVRFMT_RGB565 )
        {
            rasterFormatOut = RASTER_565;
            depthOut = 16;
            colorOrderOut = COLOR_RGBA;
        }
        else if ( format == GVRFMT_RGB5A3 )
        {
            // Since we do not search for direct equivalent, we return the thing
            // that can represent it in best quality.
            rasterFormatOut = RASTER_8888;
            depthOut = 32;
            colorOrderOut = COLOR_RGBA;
        }
        else if ( format == GVRFMT_RGBA8888 )
        {
            rasterFormatOut = RASTER_8888;
            depthOut = 32;
            colorOrderOut = COLOR_RGBA;
        }
        else if ( format == GVRFMT_CMP )
        {
            // We have to natively convert DXT blocks.
            rasterFormatOut = RASTER_DEFAULT;
            depthOut = 8;
            colorOrderOut = COLOR_RGBA;

            compressionType = RWCOMPRESS_DXT1;
        }
        else
        {
            // Should never happen.
            hasFormat = false;
        }
        
        if ( hasFormat )
        {
            paletteTypeOut = PALETTE_NONE;
            compressionTypeOut = compressionType;
        }
    }

    return hasFormat;
}

inline uint32 getGVRPixelFormatDepth( eGCPixelFormat format )
{
    uint32 depth = 0;

    if ( format == GVRPIX_LUM_ALPHA )
    {
        depth = 16;
    }
    else if ( format == GVRPIX_RGB565 )
    {
        depth = 16;
    }
    else if ( format == GVRPIX_RGB5A3 )
    {
        depth = 16;
    }

    return depth;
}

// Parameters that are required to swizzle/unswizzle the texel data.
inline bool getGVRNativeFormatClusterDimensions(
    uint32 formatDepth,
    uint32& clusterWidthOut, uint32& clusterHeightOut,
    uint32& clusterCountOut
)
{
    if ( formatDepth == 4 )
    {
        clusterWidthOut = 8;
        clusterHeightOut = 8;

        clusterCountOut = 1;
        return true;
    }
    else if ( formatDepth == 8 )
    {
        clusterWidthOut = 8;
        clusterHeightOut = 4;

        clusterCountOut = 1;
        return true;
    }
    else if ( formatDepth == 16 )
    {
        clusterWidthOut = 4;
        clusterHeightOut = 4;

        clusterCountOut = 1;
        return true;
    }
    else if ( formatDepth == 32 )
    {
        clusterWidthOut = 4;
        clusterHeightOut = 4;

        // If we are a RGBA8888 texture, we have two tiles per 4x4 region.
        clusterCountOut = 2;
        return true;
    }

    return false;
}

// Shared pixel formats.
struct pixelRGB5A3_t
{
    union
    {
        struct
        {
            uint16 pad : 15;
            uint16 hasNoAlpha : 1;
        };
        struct
        {
            uint16 b : 5;
            uint16 g : 5;
            uint16 r : 5;
        } no_alpha;
        struct
        {
            uint16 b : 4;
            uint16 g : 4;
            uint16 r : 4;
            uint16 a : 3;
        } with_alpha;
    };
};

struct gcColorDispatch
{
private:
    eGCNativeTextureFormat internalFormat;
    eGCPixelFormat palettePixelFormat;
    uint32 itemDepth;
    eColorOrdering colorOrder;
    ePaletteType paletteType;
    const void *paletteData;
    uint32 maxpalette;

    eColorModel usedColorModel;

public:
    AINLINE gcColorDispatch(
        eGCNativeTextureFormat internalFormat, eGCPixelFormat palettePixelFormat,
        eColorOrdering colorOrder,
        uint32 itemDepth, ePaletteType paletteType, const void *paletteData, uint32 maxpalette
    )
    {
        this->internalFormat = internalFormat;
        this->palettePixelFormat = palettePixelFormat;
        this->itemDepth = itemDepth;
        this->colorOrder = colorOrder;
        this->paletteType = paletteType;
        this->paletteData = paletteData;
        this->maxpalette = maxpalette;

        // Decide about color model.
        if ( internalFormat == GVRFMT_PAL_4BIT ||
             internalFormat == GVRFMT_PAL_8BIT )
        {
            if ( palettePixelFormat == GVRPIX_LUM_ALPHA )
            {
                this->usedColorModel = COLORMODEL_LUMINANCE;
            }
            else if ( palettePixelFormat == GVRPIX_RGB565 ||
                      palettePixelFormat == GVRPIX_RGB5A3 )
            {
                this->usedColorModel = COLORMODEL_RGBA;
            }
            else
            {
                throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_MAPPALFMTTOCOLOR" );
            }
        }
        else if ( internalFormat == GVRFMT_LUM_4BIT ||
                  internalFormat == GVRFMT_LUM_8BIT ||
                  internalFormat == GVRFMT_LUM_4BIT_ALPHA ||
                  internalFormat == GVRFMT_LUM_8BIT_ALPHA )
        {
            this->usedColorModel = COLORMODEL_LUMINANCE;
        }
        else if ( internalFormat == GVRFMT_RGB565 ||
                  internalFormat == GVRFMT_RGB5A3 ||
                  internalFormat == GVRFMT_RGBA8888 )
        {
            this->usedColorModel = COLORMODEL_RGBA;
        }
        else
        {
            throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_MAPINTERNFMTTOCOLOR" );
        }
    }

    inline eGCNativeTextureFormat getNativeFormat( void ) const
    {
        return this->internalFormat;
    }

private:
    // RGB color types.
    struct pixel565_t
    {
        uint16 r : 5;
        uint16 g : 6;
        uint16 b : 5;
    };

    struct pixel32_t
    {
        uint8 r;
        uint8 g;
        uint8 b;
        uint8 a;
    };

    template <typename colorNumberType>
    AINLINE static bool gcbrowsetexelrgba(
        const constRasterRow& texelSource, uint32 colorIndex, eGCNativeTextureFormat internalFormat, eGCPixelFormat palettePixelFormat,
        eColorOrdering colorOrder,
        uint32 itemDepth, ePaletteType paletteType, const void *paletteData, uint32 maxpalette,
        colorNumberType& redOut, colorNumberType& greenOut, colorNumberType& blueOut, colorNumberType& alphaOut
    )
    {
        bool hasColor = false;

        constRasterRow realTexelSource;
        uint32 realColorIndex = 0;
        eGCNativeTextureFormat realInternalFormat;

        bool couldResolveSource = false;

        colorNumberType prered, pregreen, preblue, prealpha;

        if (paletteType != PALETTE_NONE)
        {
            realTexelSource = paletteData;

            uint8 paletteIndex;

            bool couldResolvePalIndex = getpaletteindex(texelSource, paletteType, maxpalette, itemDepth, colorIndex, paletteIndex);

            if (couldResolvePalIndex)
            {
                realColorIndex = paletteIndex;

                // Get the internal format from the palette pixel type.
                if ( palettePixelFormat == GVRPIX_LUM_ALPHA )
                {
                    realInternalFormat = GVRFMT_LUM_8BIT_ALPHA;
                }
                else if ( palettePixelFormat == GVRPIX_RGB565 )
                {
                    realInternalFormat = GVRFMT_RGB565;
                }
                else if ( palettePixelFormat == GVRPIX_RGB5A3 )
                {
                    realInternalFormat = GVRFMT_RGB5A3;
                }
                else
                {
                    assert( 0 );

                    return false;
                }

                couldResolveSource = true;
            }
        }
        else
        {
            realTexelSource = texelSource;
            realColorIndex = colorIndex;
            realInternalFormat = internalFormat;

            couldResolveSource = true;
        }

        if ( !couldResolveSource )
            return false;

        if ( realInternalFormat == GVRFMT_RGB565 )
        {
            pixel565_t srcData = realTexelSource.readStructItem <endian::big_endian <pixel565_t>> ( realColorIndex );

            destscalecolor( srcData.r, 31, prered );
            destscalecolor( srcData.g, 63, pregreen );
            destscalecolor( srcData.b, 31, preblue );
            prealpha = color_defaults <colorNumberType>::one;

            hasColor = true;
        }
        else if ( realInternalFormat == GVRFMT_RGB5A3 )
        {
            pixelRGB5A3_t srcData = realTexelSource.readStructItem <endian::big_endian <pixelRGB5A3_t>> ( realColorIndex );

            if ( !srcData.hasNoAlpha )
            {
                destscalecolor( srcData.with_alpha.r, 15, prered );
                destscalecolor( srcData.with_alpha.g, 15, pregreen );
                destscalecolor( srcData.with_alpha.b, 15, preblue );
                destscalecolor( srcData.with_alpha.a, 7, prealpha );
            }
            else
            {
                destscalecolor( srcData.no_alpha.r, 31, prered );
                destscalecolor( srcData.no_alpha.g, 31, pregreen );
                destscalecolor( srcData.no_alpha.b, 31, preblue );
                prealpha = color_defaults <colorNumberType>::one;
            }

            hasColor = true;
        }
        else if ( realInternalFormat == GVRFMT_RGBA8888 )
        {
            pixel32_t srcData = realTexelSource.readStructItem <endian::big_endian <pixel32_t>> ( realColorIndex );

            destscalecolorn( srcData.r, prered );
            destscalecolorn( srcData.g, pregreen );
            destscalecolorn( srcData.b, preblue );
            destscalecolorn( srcData.a, prealpha );

            hasColor = true;
        }

        if ( hasColor )
        {
            // Deal with color order.
            if ( colorOrder == COLOR_RGBA )
            {
                redOut = prered;
                greenOut = pregreen;
                blueOut = preblue;
                alphaOut = prealpha;
            }
            else if ( colorOrder == COLOR_BGRA )
            {
                redOut = preblue;
                greenOut = pregreen;
                blueOut = prered;
                alphaOut = prealpha;
            }
            else
            {
                return false;
            }
        }

        return hasColor;
    }

    template <typename colorNumberType>
    AINLINE static bool gcputtexelrgba(
        rasterRow& dstTexels, uint32 colorIndex, eGCNativeTextureFormat internalFormat,
        eColorOrdering colorOrder,
        colorNumberType red, colorNumberType green, colorNumberType blue, colorNumberType alpha
    )
    {
        bool didSet = false;

        colorNumberType actualRed, actualGreen, actualBlue, actualAlpha;

        if ( colorOrder == COLOR_RGBA )
        {
            actualRed = red;
            actualGreen = green;
            actualBlue = blue;
            actualAlpha = alpha;
        }
        else if ( colorOrder == COLOR_BGRA )
        {
            actualRed = blue;
            actualGreen = green;
            actualBlue = red;
            actualAlpha = alpha;
        }
        else
        {
            return false;
        }
        
        // Put things in native Gamecube format.
        if ( internalFormat == GVRFMT_RGB565 )
        {
            pixel565_t dstPixel;
            dstPixel.r = putscalecolor <uint8> ( actualRed, 31 );
            dstPixel.g = putscalecolor <uint8> ( actualGreen, 63 );
            dstPixel.b = putscalecolor <uint8> ( actualBlue, 31 );

            dstTexels.writeStructItem <endian::big_endian <pixel565_t>> ( dstPixel, colorIndex );

            didSet = true;
        }
        else if ( internalFormat == GVRFMT_RGB5A3 )
        {
            pixelRGB5A3_t dstPixel;

            bool hasNoAlpha = ( alpha == 255 );

            if ( hasNoAlpha )
            {
                dstPixel.no_alpha.r = putscalecolor <uint8> ( actualRed, 31 );
                dstPixel.no_alpha.g = putscalecolor <uint8> ( actualGreen, 31 );
                dstPixel.no_alpha.b = putscalecolor <uint8> ( actualBlue, 31 );
            }
            else
            {
                dstPixel.with_alpha.a = putscalecolor <uint8> ( actualAlpha, 7 );
                dstPixel.with_alpha.r = putscalecolor <uint8> ( actualRed, 15 );
                dstPixel.with_alpha.g = putscalecolor <uint8> ( actualGreen, 15 );
                dstPixel.with_alpha.b = putscalecolor <uint8> ( actualBlue, 15 );
            }

            dstPixel.hasNoAlpha = hasNoAlpha;

            dstTexels.writeStructItem <endian::big_endian <pixelRGB5A3_t>> ( dstPixel, colorIndex );

            didSet = true;
        }
        else if ( internalFormat == GVRFMT_RGBA8888 )
        {
            pixel32_t dstPixel;
            destscalecolorn( actualRed, dstPixel.r );
            destscalecolorn( actualGreen, dstPixel.g );
            destscalecolorn( actualBlue, dstPixel.b );
            destscalecolorn( actualAlpha, dstPixel.a );

            dstTexels.writeStructItem <endian::big_endian <pixel32_t>> ( dstPixel, colorIndex );

            didSet = true;
        }

        return didSet;
    }

public:
    template <typename colorNumberType>
    AINLINE bool getRGBA( const constRasterRow& texelSource, uint32 colorIndex, colorNumberType& redOut, colorNumberType& greenOut, colorNumberType& blueOut, colorNumberType& alphaOut ) const
    {
        eColorModel ourModel = this->usedColorModel;

        bool hasColor = false;

        if ( ourModel == COLORMODEL_RGBA )
        {
            hasColor =
                gcbrowsetexelrgba(
                    texelSource, colorIndex, this->internalFormat, this->palettePixelFormat,
                    this->colorOrder,
                    this->itemDepth, this->paletteType, this->paletteData, this->maxpalette,
                    redOut, greenOut, blueOut, alphaOut
                );
        }
        else if ( ourModel == COLORMODEL_LUMINANCE )
        {
            colorNumberType lum;

            hasColor =
                this->getLuminance( texelSource, colorIndex, lum, alphaOut );

            if ( hasColor )
            {
                redOut = lum;
                greenOut = lum;
                blueOut = lum;
            }
        }

        return hasColor;
    }

    template <typename colorNumberType>
    AINLINE bool setRGBA( rasterRow& dstTexels, uint32 colorIndex, colorNumberType red, colorNumberType green, colorNumberType blue, colorNumberType alpha )
    {
        eColorModel ourModel = this->usedColorModel;

        bool setColor = false;

        if ( ourModel == COLORMODEL_RGBA )
        {
            setColor = gcputtexelrgba(
                dstTexels, colorIndex, this->internalFormat,
                this->colorOrder,
                red, green, blue, alpha
            );
        }
        else if ( ourModel == COLORMODEL_LUMINANCE )
        {
            colorNumberType lum = rgb2lum( red, green, blue );

            this->setLuminance( dstTexels, colorIndex, lum, alpha );
        }

        return setColor;
    }

private:
    // Luminance pixel types.
    struct pixel4l_t
    {
        uint8 lum : 4;
    };

    struct pixel8l_t
    {
        uint8 lum;
    };

    struct pixel4l4a_t
    {
        uint8 lum : 4;
        uint8 alpha : 4;
    };

    struct pixel8l8a_t
    {
        uint8 lum;
        uint8 alpha;
    };

    template <typename colorNumberType>
    AINLINE static bool gcbrowsetexellum( 
        const constRasterRow& texelSource, uint32 colorIndex, eGCNativeTextureFormat internalFormat, eGCPixelFormat palettePixelFormat,
        uint32 itemDepth, ePaletteType paletteType, const void *paletteData, uint32 maxpalette,
        colorNumberType& lumOut, colorNumberType& alphaOut
    )
    {
        bool hasColor = false;

        constRasterRow realTexelSource;
        uint32 realColorIndex = 0;
        eGCNativeTextureFormat realInternalFormat;

        bool couldResolveSource = false;

        colorNumberType prelum, prealpha;

        if (paletteType != PALETTE_NONE)
        {
            realTexelSource = paletteData;

            uint8 paletteIndex;

            bool couldResolvePalIndex = getpaletteindex(texelSource, paletteType, maxpalette, itemDepth, colorIndex, paletteIndex);

            if (couldResolvePalIndex)
            {
                realColorIndex = paletteIndex;

                // Get the internal format from the palette pixel type.
                if ( palettePixelFormat == GVRPIX_LUM_ALPHA )
                {
                    realInternalFormat = GVRFMT_LUM_8BIT_ALPHA;
                }
                else if ( palettePixelFormat == GVRPIX_RGB565 )
                {
                    realInternalFormat = GVRFMT_RGB565;
                }
                else if ( palettePixelFormat == GVRPIX_RGB5A3 )
                {
                    realInternalFormat = GVRFMT_RGB5A3;
                }
                else
                {
                    assert( 0 );

                    return false;
                }

                couldResolveSource = true;
            }
        }
        else
        {
            realTexelSource = texelSource;
            realColorIndex = colorIndex;
            realInternalFormat = internalFormat;

            couldResolveSource = true;
        }

        if ( !couldResolveSource )
            return false;

        if ( realInternalFormat == GVRFMT_LUM_4BIT )
        {
            pixel4l_t srcData;
            realTexelSource.readBits( &srcData, realColorIndex * 4, 4 );

            destscalecolor( srcData.lum, 15, prelum );
            prealpha = color_defaults <colorNumberType>::one;

            hasColor = true;
        }
        else if ( realInternalFormat == GVRFMT_LUM_8BIT )
        {
            pixel8l_t srcData = realTexelSource.readStructItem <endian::big_endian <pixel8l_t>> ( realColorIndex );

            destscalecolorn( srcData.lum, prelum );
            prealpha = color_defaults <colorNumberType>::one;

            hasColor = true;
        }
        else if ( realInternalFormat == GVRFMT_LUM_4BIT_ALPHA )
        {
            pixel4l4a_t srcData = realTexelSource.readStructItem <endian::big_endian <pixel4l4a_t>> ( realColorIndex );

            destscalecolor( srcData.lum, 15, prelum );
            destscalecolor( srcData.alpha, 15, prealpha );

            hasColor = true;
        }
        else if ( realInternalFormat == GVRFMT_LUM_8BIT_ALPHA )
        {
            pixel8l8a_t srcData = realTexelSource.readStructItem <endian::big_endian <pixel8l8a_t>> ( realColorIndex );

            destscalecolorn( srcData.lum, prelum );
            destscalecolorn( srcData.alpha, prealpha );

            hasColor = true;
        }

        if ( hasColor )
        {
            lumOut = prelum;
            alphaOut = prealpha;
        }

        return hasColor;
    }

    template <typename colorNumberType>
    AINLINE static bool gcputtexellum(
        rasterRow& dstTexels, uint32 colorIndex, eGCNativeTextureFormat internalFormat,
        colorNumberType lum, colorNumberType alpha
    )
    {
        bool couldSet = false;

        if ( internalFormat == GVRFMT_LUM_4BIT )
        {
            pixel4l_t srcData;

            srcData.lum = putscalecolor <uint8> ( lum, 15 );

            dstTexels.writeBits( &srcData, colorIndex * 4, 4 );

            couldSet = true;
        }
        else if ( internalFormat == GVRFMT_LUM_8BIT )
        {
            pixel8l_t dstPixel;
            destscalecolorn( lum, dstPixel.lum );

            dstTexels.writeStructItem <endian::big_endian <pixel8l_t>> ( dstPixel, colorIndex );

            couldSet = true;
        }
        else if ( internalFormat == GVRFMT_LUM_4BIT_ALPHA )
        {
            pixel4l4a_t dstTexel;
            dstTexel.lum = putscalecolor <uint8> ( lum, 15 );
            dstTexel.alpha = putscalecolor <uint8> ( alpha, 15 );

            dstTexels.writeStructItem <endian::big_endian <pixel4l4a_t>> ( dstTexel, colorIndex );

            couldSet = true;
        }
        else if ( internalFormat == GVRFMT_LUM_8BIT_ALPHA )
        {
            pixel8l8a_t dstTexel;
            destscalecolorn( lum, dstTexel.lum );
            destscalecolorn( alpha, dstTexel.alpha );

            dstTexels.writeStructItem <endian::big_endian <pixel8l8a_t>> ( dstTexel, colorIndex );

            couldSet = true;
        }

        return couldSet;
    }

public:
    template <typename colorNumberType>
    AINLINE bool getLuminance( const constRasterRow& texelSource, uint32 colorIndex, colorNumberType& lumOut, colorNumberType& alphaOut ) const
    {
        eColorModel ourModel = this->usedColorModel;

        bool hasColor = false;

        if ( ourModel == COLORMODEL_RGBA )
        {
            colorNumberType red, green, blue;

            hasColor =
                this->getRGBA( texelSource, colorIndex, red, green, blue, alphaOut );

            if ( hasColor )
            {
                lumOut = rgb2lum( red, green, blue );
            }
        }
        else if ( ourModel == COLORMODEL_LUMINANCE )
        {
            hasColor =
                gcbrowsetexellum(
                    texelSource, colorIndex, this->internalFormat, this->palettePixelFormat,
                    this->itemDepth, this->paletteType, this->paletteData, this->maxpalette,
                    lumOut, alphaOut
                );
        }

        return hasColor;
    }

    template <typename colorNumberType>
    AINLINE bool setLuminance( rasterRow& dstTexels, uint32 colorIndex, colorNumberType lum, colorNumberType alpha )
    {
        eColorModel ourModel = this->usedColorModel;

        bool couldSet = false;

        if ( ourModel == COLORMODEL_LUMINANCE )
        {
            couldSet = gcputtexellum(
                dstTexels, colorIndex, this->internalFormat,
                lum, alpha
            );
        }
        else if ( ourModel == COLORMODEL_RGBA )
        {
            couldSet = this->setRGBA( dstTexels, colorIndex, lum, lum, lum, alpha );
        }

        return couldSet;
    }

    AINLINE void getColor( const constRasterRow& texelSource, uint32 colorIndex, abstractColorItem& colorOut ) const
    {
        eColorModel colorModel = this->usedColorModel;

        colorOut.model = colorModel;

        if ( colorModel == COLORMODEL_RGBA )
        {
            bool hasColor = this->getRGBA(
                texelSource, colorIndex,
                colorOut.rgbaColor.r,
                colorOut.rgbaColor.g,
                colorOut.rgbaColor.b,
                colorOut.rgbaColor.a
            );

            if ( !hasColor )
            {
                colorOut.rgbaColor.r = 0;
                colorOut.rgbaColor.g = 0;
                colorOut.rgbaColor.b = 0;
                colorOut.rgbaColor.a = 0;
            }
        }
        else if ( colorModel == COLORMODEL_LUMINANCE )
        {
            bool hasColor = this->getLuminance(
                texelSource, colorIndex,
                colorOut.luminance.lum,
                colorOut.luminance.alpha
            );

            if ( !hasColor )
            {
                colorOut.luminance.lum = 0;
                colorOut.luminance.alpha = 0;
            }
        }
        else
        {
            throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_UNKCOLORMODEL" );
        }
    }

    AINLINE void setColor( rasterRow& dstTexels, uint32 colorIndex, const abstractColorItem& colorItem )
    {
        eColorModel ourModel = colorItem.model;

        if ( ourModel == COLORMODEL_RGBA )
        {
            this->setRGBA(
                dstTexels, colorIndex,
                colorItem.rgbaColor.r,
                colorItem.rgbaColor.g,
                colorItem.rgbaColor.b,
                colorItem.rgbaColor.a
            );
        }
        else if ( ourModel == COLORMODEL_LUMINANCE )
        {
            this->setLuminance(
                dstTexels, colorIndex,
                colorItem.luminance.lum,
                colorItem.luminance.alpha
            );
        }
        else
        {
            throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_UNKCOLORMODEL" );
        }
    }
};

struct NativeTextureGC
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureGC( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->palette = nullptr;
        this->paletteSize = 0;
        this->internalFormat = GVRFMT_RGBA8888;
        this->palettePixelFormat = GVRPIX_NO_PALETTE;
        this->autoMipmaps = false;
        this->rasterType = 4;
        this->hasAlpha = false;
        this->unk1 = 0;
        this->unk2 = 1;
        this->unk3 = 1;
        this->unk4 = 0;
    }

    inline NativeTextureGC( const NativeTextureGC& right )
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = engineInterface;
        this->texVersion = right.texVersion;

        // Copy palette information.
        {
	        if (right.palette)
            {
                uint32 palRasterDepth = getGVRPixelFormatDepth(right.palettePixelFormat);

                size_t wholeDataSize = getPaletteDataSize( right.paletteSize, palRasterDepth );

		        this->palette = engineInterface->PixelAllocate( wholeDataSize );

		        memcpy(this->palette, right.palette, wholeDataSize);
	        }
            else
            {
		        this->palette = nullptr;
	        }

            this->paletteSize = right.paletteSize;
            this->palettePixelFormat = right.palettePixelFormat;
        }

        // Copy image texel information.
        {
            copyMipmapLayers( engineInterface, right.mipmaps, this->mipmaps );
        }

        // Copy advanced properties.
        this->internalFormat = right.internalFormat;
        this->autoMipmaps = right.autoMipmaps;
        this->rasterType = right.rasterType;
        this->hasAlpha = right.hasAlpha;
        this->unk1 = right.unk1;
        this->unk2 = right.unk2;
        this->unk3 = right.unk3;
        this->unk4 = right.unk4;
    }

    inline void clearTexelData( void )
    {
        if ( this->palette )
        {
	        this->engineInterface->PixelFree( palette );

	        palette = nullptr;
        }

        deleteMipmapLayers( this->engineInterface, this->mipmaps );
    }

    inline ~NativeTextureGC( void )
    {
        this->clearTexelData();
    }

    void UpdateStructure( void );

    static inline void getSizeRules( bool isDXTCompressed, nativeTextureSizeRules& rulesOut )
    {
        rulesOut.powerOfTwo = true;
        rulesOut.squared = false;
        rulesOut.multipleOf = ( isDXTCompressed == true );
        rulesOut.multipleOfValue = ( isDXTCompressed ? 4 : 0 );
        rulesOut.maximum = true;
        rulesOut.maxVal = 1024;
    }

public:
    typedef genmip::mipmapLayer mipmapLayer;

    DEFINE_HEAP_REDIR_ALLOC_IMPL( mipRedirAlloc );

    eir::Vector <mipmapLayer, mipRedirAlloc> mipmaps;

    void *palette;
    uint32 paletteSize;

    eGCNativeTextureFormat internalFormat;
    eGCPixelFormat palettePixelFormat;

    bool autoMipmaps;
    uint8 rasterType;

    bool hasAlpha;

    // TODO: find out what these are.
    uint32 unk1;
    uint32 unk2;
    uint32 unk3;
    uint32 unk4;
};

RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( NativeTextureGC, mipRedirAlloc, mipmaps, engineInterface )

struct gamecubeNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        new (objMem) NativeTextureGC( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) override
    {
        new (objMem) NativeTextureGC( *(const NativeTextureGC*)srcObjMem );
    }

    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        ((NativeTextureGC*)objMem)->~NativeTextureGC();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const
    {
        capsOut.supportsDXT1 = true;
        capsOut.supportsDXT2 = false;
        capsOut.supportsDXT3 = false;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = false;
        capsOut.supportsPalette = true;
    }

    void GetStorageCapabilities( storageCapabilities& storeCaps ) const
    {
        storeCaps.pixelCaps.supportsDXT1 = true;
        storeCaps.pixelCaps.supportsDXT2 = false;
        storeCaps.pixelCaps.supportsDXT3 = false;
        storeCaps.pixelCaps.supportsDXT4 = false;
        storeCaps.pixelCaps.supportsDXT5 = false;
        storeCaps.pixelCaps.supportsPalette = true;

        storeCaps.isCompressedFormat = false;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut ) override;
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut ) override;
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate ) override;

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version )
    {
        NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

        nativeTex->texVersion = version;

        // Make sure our content is consistent with the version that we set.
        // This matters because pre-3.3.0.2 does not support luminance raster formats.
        nativeTex->UpdateStructure();
    }

    LibraryVersion GetTextureVersion( const void *objMem )
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        // There is no good raster format representation for the GC native texture.
        // We instead return the best approximation.
        eRasterFormat recRasterFormat;
        uint32 recDepth;
        eColorOrdering recColorOrder;
        ePaletteType recPaletteType;

        eCompressionType recCompressionType;

        bool gotFormat =
            getRecommendedGCNativeTextureRasterFormat(
                nativeTex->internalFormat, nativeTex->palettePixelFormat,
                recRasterFormat, recDepth, recColorOrder, recPaletteType,
                recCompressionType
            );

        if ( !gotFormat )
        {
            recRasterFormat = RASTER_DEFAULT;
        }

        return recRasterFormat;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        return getPaletteTypeFromGCNativeFormat( nativeTex->internalFormat );
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;
        
        return ( nativeTex->internalFormat == GVRFMT_CMP );
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        if ( nativeTex->internalFormat == GVRFMT_CMP )
        {
            return RWCOMPRESS_DXT1;
        }

        return RWCOMPRESS_NONE;
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // I guess we want to be 4 byte aligned.
        return 4;
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        // We luckily are static in the size rules.
        bool isDXTCompressed = ( format.compressionType == RWCOMPRESS_DXT1 );

        NativeTextureGC::getSizeRules( isDXTCompressed, rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // I think a Gamecube native texture works very similar to PS2 and XBOX, since it is
        // hardware of the same generation.
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        bool isDXTCompressed = ( nativeTex->internalFormat == GVRFMT_CMP );

        NativeTextureGC::getSizeRules( isDXTCompressed, rulesOut );
    }

    void* GetNativeInterface( void *objMem ) override
    {
        // TODO.
        return nullptr;
    }

    void* GetDriverNativeInterface( void ) const override
    {
        // TODO.
        return nullptr;
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // Undefined.
        return 0;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "Gamecube", this, sizeof( NativeTextureGC ), alignof( NativeTextureGC ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "Gamecube" );
    }
};

namespace gamecube
{

#pragma pack(1)
// Version newer than 3.3.0.2
struct textureMetaHeaderStructGeneric35
{
    rw::texFormatInfo_serialized <endian::big_endian <uint32>> texFormat;

    endian::p_big_endian <uint32> unk1;     // gotta be some sort of gamecube registers.
    endian::p_big_endian <uint32> unk2;
    endian::p_big_endian <uint32> unk3;
    endian::p_big_endian <uint32> unk4;
    
    char name[32];
    char maskName[32];

    endian::p_big_endian <rwGenericRasterFormatFlags> rasterFormat;

    endian::p_big_endian <uint16> width;
    endian::p_big_endian <uint16> height;
    uint8 depth;
    uint8 mipmapCount;
    eGCNativeTextureFormat internalFormat;
    eGCPixelFormat palettePixelFormat;

    endian::p_big_endian <uint32> hasAlpha;
};

// Version newer than 3.3.0.0
struct textureMetaHeaderStructGeneric33
{
    rw::texFormatInfo_serialized <endian::big_endian <uint32>> texFormat;

    endian::p_big_endian <uint32> unk1;   // gotta be some sort of gamecube registers.
    endian::p_big_endian <uint32> unk2;
    endian::p_big_endian <uint32> unk3;
    endian::p_big_endian <uint32> unk4;

    char name[32];
    char maskName[32];

    endian::p_big_endian <rwGenericRasterFormatFlags> rasterFormat;

    endian::p_big_endian <uint32> hasAlpha;

    endian::p_big_endian <uint16> width;
    endian::p_big_endian <uint16> height;
    uint8 depth;
    uint8 mipmapCount;
    uint8 rasterType;

    bool isCompressed;
};

// Version older than 3.3.0.0
struct textureMetaHeaderStructGeneric32
{
    rw::texFormatInfo_serialized <endian::big_endian <uint32>> texFormat;

    char name[32];
    char maskName[32];

    endian::p_big_endian <rwGenericRasterFormatFlags> rasterFormat;

    endian::p_big_endian <uint32> hasAlpha;

    endian::p_big_endian <uint16> width;
    endian::p_big_endian <uint16> height;
    uint8 depth;
    uint8 mipmapCount;
    uint8 rasterType;

    bool isCompressed;
};
#pragma pack()

};

};

#endif //RWLIB_INCLUDE_NATIVETEX_GAMECUBE