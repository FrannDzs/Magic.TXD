/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwimaging.tga.cpp
*  PURPOSE:     TGA image format implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwimaging.hxx"
#include "rwimaging.rasterread.hxx"

#include <sdk/PluginHelpers.h>

#include "pixelformat.hxx"

#include "streamutil.hxx"

namespace rw
{

#ifdef RWLIB_INCLUDE_TGA_IMAGING
// .tga RenderWare imaging extension.
// this image format is natively implemented, so no additional .lib is required.

typedef uint8 BYTE;
typedef uint16 WORD;
typedef uint32 DWROD;

#pragma pack(push,1)
struct TgaHeader
{
    endian::p_little_endian <BYTE> IDLength;        /* 00h  Size of Image ID field */
    endian::p_little_endian <BYTE> ColorMapType;    /* 01h  Color map type */
    endian::p_little_endian <BYTE> ImageType;       /* 02h  Image type code */
    endian::p_little_endian <WORD> CMapStart;       /* 03h  Color map origin */
    endian::p_little_endian <WORD> CMapLength;      /* 05h  Color map length */
    endian::p_little_endian <BYTE> CMapDepth;       /* 07h  Depth of color map entries */
    endian::p_little_endian <WORD> XOffset;         /* 08h  X origin of image */
    endian::p_little_endian <WORD> YOffset;         /* 0Ah  Y origin of image */
    endian::p_little_endian <WORD> Width;           /* 0Ch  Width of image */
    endian::p_little_endian <WORD> Height;          /* 0Eh  Height of image */
    endian::p_little_endian <BYTE> PixelDepth;      /* 10h  Image pixel size */

    struct
    {
        unsigned char numAttrBits : 4;
        unsigned char imageOrdering : 2;
        unsigned char reserved : 2;
    } ImageDescriptor; /* 11h  Image descriptor byte */
};
#pragma pack(pop)

inline uint32 getTGATexelDataRowAlignment( void )
{
    // I believe that TGA image data is packed by bytes.
    // The documentation does not mention anything!
    return 1;
}

inline rasterRowSize getTGARasterDataRowSize( uint32 width, uint32 depth )
{
    return getRasterDataRowSize( width, depth, getTGATexelDataRowAlignment() );
}

static void writeTGAPixels(
    Interface *engineInterface,
    const void *texelSource, uint32 texWidth, uint32 texHeight,
    eRasterFormat srcRasterFormat, uint32 srcItemDepth, uint32 srcRowAlignment, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcMaxPalette,
    eRasterFormat dstRasterFormat, uint32 dstItemDepth, uint32 dstRowAlignment,
    eColorOrdering srcColorOrder, eColorOrdering tgaColorOrder,
    Stream *tgaStream
)
{
    // Get the row size of the source colors.
    rasterRowSize srcRowSize = getRasterDataRowSize( texWidth, srcItemDepth, srcRowAlignment );

    bool needsConv =
        doesRawMipmapBufferNeedFullConversion(
            texWidth,
            srcRasterFormat, srcItemDepth, srcRowAlignment, srcColorOrder, srcPaletteType,
            dstRasterFormat, dstItemDepth, dstRowAlignment, tgaColorOrder, PALETTE_NONE
        );

    if ( needsConv )
    {
        // Get the data size.
        rasterRowSize tgaRowSize = getRasterDataRowSize( texWidth, dstItemDepth, dstRowAlignment );

        uint32 texelDataSize = getRasterDataSizeByRowSize( tgaRowSize, texHeight );

        // Allocate a buffer for the fixed pixel data.
        void *tgaColors = engineInterface->PixelAllocate( texelDataSize );

        try
        {
            // Transform the raw color data.
            colorModelDispatcher fetchDispatch( srcRasterFormat, srcColorOrder, srcItemDepth, srcPaletteData, srcMaxPalette, srcPaletteType );
            colorModelDispatcher putDispatch( dstRasterFormat, tgaColorOrder, dstItemDepth, nullptr, 0, PALETTE_NONE );

            copyTexelDataEx(
                texelSource, tgaColors,
                fetchDispatch, putDispatch,
                texWidth, texHeight,
                0, 0,
                0, 0,
                srcRowSize, tgaRowSize
            );

            // Write the entire buffer at once.
            tgaStream->write((const void*)tgaColors, texelDataSize);
        }
        catch( ... )
        {
            // If there was any error, screw it.
            engineInterface->PixelFree( tgaColors );

            throw;
        }

        // Free memory.
        engineInterface->PixelFree( tgaColors );
    }
    else
    {
        // Simply write the color source.
        uint32 texelDataSize = getRasterDataSizeByRowSize( srcRowSize, texHeight );

        tgaStream->write((const void*)texelSource, texelDataSize);
    }
}

inline bool getTGARasterFormat( unsigned int pixelDepth, unsigned int pixelAlphaCount, eRasterFormat& dstRasterFormat, uint32& dstDepth )
{
    if ( pixelDepth == 32 )
    {
        if ( pixelAlphaCount == 8 )
        {
            dstRasterFormat = RASTER_8888;
            dstDepth = 32;
            return true;
        }
        else if ( pixelAlphaCount == 0 )
        {
            dstRasterFormat = RASTER_888;
            dstDepth = 32;
            return true;
        }
    }
    else if ( pixelDepth == 24 )
    {
        if ( pixelAlphaCount == 0 )
        {
            dstRasterFormat = RASTER_888;
            dstDepth = 24;
            return true;
        }
    }
    else if ( pixelDepth == 16 )
    {
        if ( pixelAlphaCount == 1 )
        {
            dstRasterFormat = RASTER_1555;
            dstDepth = 16;
            return true;
        }
        else if ( pixelAlphaCount == 0 )
        {
            dstRasterFormat = RASTER_565;
            dstDepth = 16;
            return true;
        }
        else if ( pixelAlphaCount == 4 )
        {
            dstRasterFormat = RASTER_4444;
            dstDepth = 16;
            return true;
        }
    }

    return false;
}

static const imaging_filename_ext tga_ext[] =
{
    { "TGA", true }
};

struct tgaImagingExtension : public imagingFormatExtension
{
    inline void Initialize( Interface *engineInterface )
    {
        // We can now address the imaging environment and register ourselves, quite exciting.
        RegisterImagingFormat( engineInterface, "Truevision Raster Graphics", IMAGING_COUNT_EXT(tga_ext), tga_ext, this );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        // Remove us again.
        UnregisterImagingFormat( engineInterface, this );
    }

    bool IsStreamCompatible( Interface *engineInterface, Stream *inputStream ) const override
    {
        // We do a quick structural check whether it is even possible that we are a TGA.
        TgaHeader possibleHeader;

        size_t readCount = inputStream->read( &possibleHeader, sizeof( possibleHeader ) );

        if ( readCount != sizeof( possibleHeader ) )
        {
            return false;
        }

        // We need to check definitive TGA format parameters.
        // Extra steps are necessary because TGA has no magic number.
        uint8 colorMapType = possibleHeader.ColorMapType;
        {
            if ( colorMapType != 0 && colorMapType != 1 )
            {
                return false;
            }

            if ( possibleHeader.CMapLength > 256 )
            {
                return false;
            }

            uint8 imageType = possibleHeader.ImageType;

            if ( imageType != 0 && imageType != 1 && imageType != 2 && imageType != 3 &&
                 imageType != 9 && imageType != 10 && imageType != 11 )
            {
                return false;
            }
        }

        // Now skip the ID stuff, if present.
        {
            uint8 idLength = possibleHeader.IDLength;

            if ( idLength != 0 )
            {
                skipAvailable( inputStream, idLength );
            }
        }

        // Read the palette stuff.
        if ( colorMapType == 1 )
        {
            uint32 palDataSize = getPaletteDataSize( possibleHeader.CMapLength, possibleHeader.CMapDepth );

            skipAvailable( inputStream, palDataSize );
        }

        // Now read the image data.
        rasterRowSize tgaRowSize = getRasterDataRowSize( possibleHeader.Width, possibleHeader.PixelDepth, getTGATexelDataRowAlignment() );

        uint32 colorDataSize = getRasterDataSizeByRowSize( tgaRowSize, possibleHeader.Height );

        skipAvailable( inputStream, colorDataSize );

        return true;
    }

    void GetStorageCapabilities( pixelCapabilities& capsOut ) const override
    {
        capsOut.supportsDXT1 = false;
        capsOut.supportsDXT2 = false;
        capsOut.supportsDXT3 = false;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = false;
        capsOut.supportsPalette = true;
    }

    enum eTGAOrientation
    {
        TGAORIENT_BOTTOMLEFT,
        TGAORIENT_BOTTOMRIGHT,
        TGAORIENT_TOPLEFT,
        TGAORIENT_TOPRIGHT
    };

    void DeserializeImage( Interface *engineInterface, Stream *inputStream, imagingLayerTraversal& outputTexels ) const override
    {
        // Read the header and decide about color format stuff.
        TgaHeader headerData;

        size_t headerReadCount = inputStream->read( &headerData, sizeof( headerData ) );

        if ( headerReadCount != sizeof( headerData ) )
        {
            throw ImagingStructuralErrorException( "TGA", L"TGA_STRUCTERR_HEADERFAIL" );
        }

        // Decide about how to map it to RenderWare data.
        eRasterFormat dstRasterFormat;
        uint32 dstDepth;
        eColorOrdering dstColorOrder = COLOR_BGRA;  // TGA is always BGRA

        ePaletteType dstPaletteType = PALETTE_NONE;

        bool hasRasterFormat = false;

        uint32 dstItemDepth;

        bool hasPalette = ( headerData.ColorMapType == 1 );
        bool requiresPalette = false;

        if ( headerData.ImageType == 1 ) // with palette.
        {
            if ( hasPalette == false )
            {
                throw ImagingStructuralErrorException( "TGA", L"TGA_STRUCTERR_MISSINGPAL" );
            }

            if ( headerData.PixelDepth == 4 || headerData.PixelDepth == 8 )
            {
                bool hasPixelRasterFormat = getTGARasterFormat( headerData.CMapDepth, headerData.ImageDescriptor.numAttrBits, dstRasterFormat, dstDepth );

                if ( hasPixelRasterFormat )
                {
                    dstItemDepth = headerData.PixelDepth;

                    // Set correct palette type.
                    if ( dstItemDepth == 4 )
                    {
                        dstPaletteType = PALETTE_4BIT;
                    }
                    else if ( dstItemDepth == 8 )
                    {
                        dstPaletteType = PALETTE_8BIT;
                    }
                    else
                    {
                        assert( 0 );
                    }

                    hasRasterFormat = true;
                }
            }
            else
            {
                throw ImagingStructuralErrorException( "TGA", L"TGA_STRUCTERR_COLORMAPDEPTH" );
            }

            requiresPalette = true;
        }
        else if ( headerData.ImageType == 2 ) // without palette, raw colors.
        {
            hasRasterFormat = getTGARasterFormat( headerData.PixelDepth, headerData.ImageDescriptor.numAttrBits, dstRasterFormat, dstDepth );

            if ( hasRasterFormat )
            {
                dstItemDepth = dstDepth;
            }
        }
        else if ( headerData.ImageType == 3 ) // grayscale.
        {
            if ( headerData.ImageDescriptor.numAttrBits == 0 )
            {
                dstRasterFormat = RASTER_LUM;
                dstDepth = 8;
                dstItemDepth = 8;

                hasRasterFormat = true;
            }
            else if ( headerData.ImageDescriptor.numAttrBits == 8 )
            {
                dstRasterFormat = RASTER_LUM_ALPHA;
                dstDepth = 16;
                dstItemDepth = 16;

                hasRasterFormat = true;
            }
        }
        else // TODO: add RLE support.
        {
            throw ImagingStructuralErrorException( "TGA", L"TGA_STRUCTERR_UNKIMAGETYPE" );
        }

        if ( hasRasterFormat == false )
        {
            throw ImagingStructuralErrorException( "TGA", L"TGA_STRUCTERR_UNKFORMATMAPPING" );
        }

        // Make sure we set proper orientation.
        eTGAOrientation tgaOrient = TGAORIENT_TOPLEFT;

        uint32 serOrient = headerData.ImageDescriptor.imageOrdering;

        if ( serOrient == 0 )
        {
            tgaOrient = TGAORIENT_BOTTOMLEFT;
        }
        else if ( serOrient == 1 )
        {
            tgaOrient = TGAORIENT_BOTTOMRIGHT;
        }
        else if ( serOrient == 2 )
        {
            tgaOrient = TGAORIENT_TOPLEFT;
        }
        else if ( serOrient == 3 )
        {
            tgaOrient = TGAORIENT_TOPRIGHT;
        }

        // Skip the image id, if we have one.
        if ( headerData.IDLength != 0 )
        {
            inputStream->skip( headerData.IDLength );
        }

        // Read the palette, if present.
        void *paletteData = nullptr;
        uint32 paletteSize = headerData.CMapLength;

        if ( hasPalette )
        {
            uint32 paletteDataSize = getPaletteDataSize( paletteSize, dstDepth );

            // Only save the palette if we really need it.
            if ( requiresPalette )
            {
                checkAhead( inputStream, paletteDataSize );

                paletteData = engineInterface->PixelAllocate( paletteDataSize );

                try
                {
                    size_t readCount = inputStream->read( paletteData, paletteDataSize );

                    if ( readCount != paletteDataSize )
                    {
                        throw ImagingStructuralErrorException( "TGA", L"TGA_STRUCTERR_RDPALDATA" );
                    }
                }
                catch( ... )
                {
                    engineInterface->PixelFree( paletteData );

                    paletteData = nullptr;

                    throw;
                }
            }
            else
            {
                // Just skip this stuff.
                if ( paletteDataSize != 0 )
                {
                    inputStream->skip( paletteDataSize );
                }
            }
        }

        try
        {
            // The data can be oriented in four different ways, to the liking of the serializer.
            // We should allow any orientation, but we only store in TOPLEFT.
            bool flip_horizontal = false;
            bool flip_vertical = false;

            if ( tgaOrient == TGAORIENT_TOPLEFT )
            {
                flip_horizontal = false;
                flip_vertical = false;
            }
            else if ( tgaOrient == TGAORIENT_TOPRIGHT )
            {
                flip_horizontal = true;
                flip_vertical = false;
            }
            else if ( tgaOrient == TGAORIENT_BOTTOMLEFT )
            {
                flip_horizontal = false;
                flip_vertical = true;
            }
            else if ( tgaOrient == TGAORIENT_BOTTOMRIGHT )
            {
                flip_horizontal = true;
                flip_vertical = true;
            }
            else
            {
                assert( 0 );
            }

            bool canDirectlyAcquire = ( tgaOrient == TGAORIENT_TOPLEFT );

            // Now read the color/index data.
            uint32 width = headerData.Width;
            uint32 height = headerData.Height;

            rasterRowSize tgaRowSize = getTGARasterDataRowSize( width, dstItemDepth );

            uint32 rasterDataSize = getRasterDataSizeByRowSize( tgaRowSize, height );

            checkAhead( inputStream, rasterDataSize );

            void *texelData = engineInterface->PixelAllocate( rasterDataSize );

            try
            {
                if ( canDirectlyAcquire )
                {
                    size_t rasterReadCount = inputStream->read( texelData, rasterDataSize );

                    if ( rasterReadCount != rasterDataSize )
                    {
                        throw ImagingStructuralErrorException( "TGA", L"TGA_STRUCTERR_RDIMGDATA" );
                    }
                }
                else
                {
                    rasterReadRows(
                        engineInterface, texelData, height, dstItemDepth, tgaRowSize, flip_vertical, flip_horizontal,
                        [&]( void *rowbuf, size_t reqCount )
                        {
                            // Read the source row.
                            size_t rowReadCount = inputStream->read( rowbuf, reqCount );

                            if ( rowReadCount != reqCount )
                            {
                                throw ImagingStructuralErrorException( "TGA", L"TGA_STRUCTERR_TRUNCATEDREAD" );
                            }
                        }
                    );
                }
            }
            catch( ... )
            {
                engineInterface->PixelFree( texelData );

                throw;
            }

            // Nothing can go wrong now.
            outputTexels.layerWidth = width;
            outputTexels.layerHeight = height;
            outputTexels.mipWidth = width;
            outputTexels.mipHeight = height;
            outputTexels.texelSource = texelData;
            outputTexels.dataSize = rasterDataSize;

            outputTexels.rasterFormat = dstRasterFormat;
            outputTexels.depth = dstItemDepth;
            outputTexels.rowAlignment = getTGATexelDataRowAlignment();
            outputTexels.colorOrder = dstColorOrder;
            outputTexels.paletteType = dstPaletteType;
            outputTexels.paletteData = paletteData;
            outputTexels.paletteSize = paletteSize;
            outputTexels.compressionType = RWCOMPRESS_NONE;
        }
        catch( ... )
        {
            if ( paletteData )
            {
                engineInterface->PixelFree( paletteData );

                paletteData = nullptr;
            }

            throw;
        }

        // We are done!
    }

    static inline bool getTGAFullColorConfiguration(
        eRasterFormat srcRasterFormat,
        eRasterFormat& dstRasterFormatOut, uint32& dstColorDepthOut, uint32& dstAlphaBitsOut
    )
    {
        if ( srcRasterFormat == RASTER_1555 )
        {
            dstRasterFormatOut = RASTER_1555;
            dstColorDepthOut = 16;
            dstAlphaBitsOut = 1;
            return true;
        }
        else if ( srcRasterFormat == RASTER_565 )
        {
            dstRasterFormatOut = RASTER_565;
            dstColorDepthOut = 16;
            dstAlphaBitsOut = 0;
            return true;
        }
        else if ( srcRasterFormat == RASTER_4444 )
        {
            dstRasterFormatOut = RASTER_4444;
            dstColorDepthOut = 16;
            dstAlphaBitsOut = 4;
            return true;
        }
        else if ( srcRasterFormat == RASTER_8888 ||
                    srcRasterFormat == RASTER_888 )
        {
            dstRasterFormatOut = RASTER_8888;
            dstColorDepthOut = 32;
            dstAlphaBitsOut = 8;
            return true;
        }
        else if ( srcRasterFormat == RASTER_555 )
        {
            dstRasterFormatOut = RASTER_565;
            dstColorDepthOut = 16;
            dstAlphaBitsOut = 0;
            return true;
        }

        return false;
    }

    void SerializeImage( Interface *engineInterface, Stream *outputStream, const imagingLayerTraversal& inputTexels ) const override
    {
        // We are using an extended version of the TGA standard that not a lot of editors support.

        if ( inputTexels.compressionType != RWCOMPRESS_NONE )
        {
            throw ImagingInvalidConfigurationException( "TGA", L"TGA_INVALIDCFG_NOCOMPRINPUT" );
        }

        // TODO: make this an Interface property.
        const bool optimized = true;

        // Decide how to write the raster.
        eRasterFormat srcRasterFormat = inputTexels.rasterFormat;
        ePaletteType srcPaletteType = inputTexels.paletteType;
        uint32 srcItemDepth = inputTexels.depth;
        uint32 srcRowAlignment = inputTexels.rowAlignment;

        eRasterFormat dstRasterFormat;
        uint32 dstColorDepth;
        uint32 dstAlphaBits;
        bool hasDstRasterFormat = false;

        uint32 dstItemDepth = srcItemDepth;

        ePaletteType dstPaletteType;

        uint32 dstRowAlignment = getTGATexelDataRowAlignment();

        if ( !optimized )
        {
            // We output in a format that every parser should understand.
            dstRasterFormat = RASTER_8888;
            dstColorDepth = 32;
            dstAlphaBits = 8;

            dstPaletteType = PALETTE_NONE;

            hasDstRasterFormat = true;
        }
        else
        {
            if ( srcPaletteType != PALETTE_NONE )
            {
                // We palettize if present.
                dstPaletteType = PALETTE_8BIT;
                dstItemDepth = 8;

                if ( getTGAFullColorConfiguration( srcRasterFormat, dstRasterFormat, dstColorDepth, dstAlphaBits ) )
                {
                    // OK.
                }
                else
                {
                    dstRasterFormat = RASTER_8888;
                    dstColorDepth = 32;
                    dstAlphaBits = 8;
                }

                hasDstRasterFormat = true;
            }
            else
            {
                // Do no palettize.
                dstPaletteType = PALETTE_NONE;

                if ( getTGAFullColorConfiguration( srcRasterFormat, dstRasterFormat, dstColorDepth, dstAlphaBits ) )
                {
                    // OK.
                }
                // TGA in non-palette mode can store Luminance color data.
                else if ( srcRasterFormat == RASTER_LUM )
                {
                    dstRasterFormat = RASTER_LUM;
                    dstColorDepth = 8;
                    dstAlphaBits = 0;
                }
                else if ( srcRasterFormat == RASTER_LUM_ALPHA )
                {
                    dstRasterFormat = RASTER_LUM_ALPHA;
                    dstColorDepth = 16;
                    dstAlphaBits = 8;
                }
                else
                {
                    dstRasterFormat = RASTER_8888;
                    dstColorDepth = 32;
                    dstAlphaBits = 8;
                }

                // We kinda always have an answer :)
                hasDstRasterFormat = true;

                // Since we do not palettize, the item depth is the same as the color depth.
                dstItemDepth = dstColorDepth;
            }
        }

        if ( !hasDstRasterFormat )
        {
            throw ImagingInvalidConfigurationException( "TGA", L"TGA_INVALIDCFG_NOFORMATMAPPING" );
        }
        else
        {
            // Prepare the TGA header.
            TgaHeader header;

            uint32 maxpalette = inputTexels.paletteSize;

            bool isPalette = (dstPaletteType != PALETTE_NONE);

            // Decide on the target image type.
            uint8 imgType;
            {
                eColorModel targetRasterFormatColorModel = getColorModelFromRasterFormat( dstRasterFormat );

                if ( isPalette )
                {
                    // TGA palette only supports RGBA.
                    assert( targetRasterFormatColorModel == COLORMODEL_RGBA );

                    imgType = 1;
                }
                else
                {
                    if ( targetRasterFormatColorModel == COLORMODEL_RGBA )
                    {
                        imgType = 2;
                    }
                    else if ( targetRasterFormatColorModel == COLORMODEL_LUMINANCE )
                    {
                        imgType = 3;
                    }
                    else
                    {
                        // Seriously?
                        assert( 0 );

                        imgType = 0;
                    }
                }
            }

            // We want to write information about this software in the image id field.
            auto _software_info = GetRunningSoftwareInformation( (EngineInterface*)engineInterface );

            const char *image_id_data = _software_info.GetConstString();
            size_t image_id_length = _software_info.GetLength();

            // TODO: improve this.
            assert( image_id_length < 256 );

            header.IDLength = (BYTE)image_id_length;
            header.ColorMapType = ( isPalette ? 1 : 0 );
            header.ImageType = imgType;

            // The pixel depth is the number of bits a color entry is going to take (real RGBA color).
            uint32 pixelDepth = 0;

            if (isPalette)
            {
                pixelDepth = Bitmap::getRasterFormatDepth(dstRasterFormat);
            }
            else
            {
                pixelDepth = dstColorDepth;
            }

            header.CMapStart = 0;
            header.CMapLength = ( isPalette ? maxpalette : 0 );
            header.CMapDepth = ( isPalette ? pixelDepth : 0 );

            header.XOffset = 0;
            header.YOffset = 0;

            uint32 width = inputTexels.mipWidth;
            uint32 height = inputTexels.mipHeight;

            header.Width = width;
            header.Height = height;
            header.PixelDepth = ( isPalette ? dstItemDepth : dstColorDepth );

            header.ImageDescriptor.numAttrBits = dstAlphaBits;
            header.ImageDescriptor.imageOrdering = 2;   // we store pixels in topleft ordering.
            header.ImageDescriptor.reserved = 0;

            // Write the header.
            outputStream->write((const void*)&header, sizeof(header));

            // Write image ID stuff.
            if ( image_id_length != 0 )
            {
                outputStream->write( image_id_data, image_id_length );
            }

            const void *texelSource = inputTexels.texelSource;
            const void *paletteData = inputTexels.paletteData;
            eColorOrdering colorOrder = inputTexels.colorOrder;

            // Write the palette if we require.
            if (isPalette)
            {
                writeTGAPixels(
                    engineInterface,
                    paletteData, maxpalette, 1,
                    srcRasterFormat, pixelDepth, 0, PALETTE_NONE, nullptr, 0,
                    dstRasterFormat, pixelDepth, 0,
                    colorOrder, COLOR_BGRA,
                    outputStream
                );
            }

            // Now write image information.
            // If we are a palette, we simply write the color indice.
            if (isPalette)
            {
                assert( srcPaletteType != PALETTE_NONE );

                // Write a fixed version of the palette indice.
                rasterRowSize texelRowSize = getTGARasterDataRowSize( width, dstItemDepth );

                uint32 texelDataSize = getRasterDataSizeByRowSize( texelRowSize, height );

                void *fixedPalItems = engineInterface->PixelAllocate( texelDataSize );

                try
                {
                    ConvertPaletteDepth(
                        texelSource, fixedPalItems,
                        width, height,
                        srcPaletteType, dstPaletteType, maxpalette,
                        srcItemDepth, dstItemDepth,
                        srcRowAlignment, dstRowAlignment
                    );

                    outputStream->write((const void*)fixedPalItems, texelDataSize);
                }
                catch( ... )
                {
                    // We should always write exception safe code!
                    engineInterface->PixelFree( fixedPalItems );

                    throw;
                }

                // Clean up memory.
                engineInterface->PixelFree( fixedPalItems );
            }
            else
            {
                writeTGAPixels(
                    engineInterface,
                    texelSource, width, height,
                    srcRasterFormat, srcItemDepth, srcRowAlignment, srcPaletteType, paletteData, maxpalette,
                    dstRasterFormat, dstColorDepth, dstRowAlignment,
                    colorOrder, COLOR_BGRA,
                    outputStream
                );
            }
        }
    }
};

static optional_struct_space <PluginDependantStructRegister <tgaImagingExtension, RwInterfaceFactory_t>> tgaImagingEnv;

#endif //RWLIB_INCLUDE_TGA_IMAGING

void registerTGAImagingExtension( void )
{
#ifdef RWLIB_INCLUDE_TGA_IMAGING
    tgaImagingEnv.Construct( engineFactory );
#endif //RWLIB_INCLUDE_TGA_IMAGING
}

void unregisterTGAImagingExtension( void )
{
#ifdef RWLIB_INCLUDE_TGA_IMAGING
    tgaImagingEnv.Destroy();
#endif //RWLIB_INCLUDE_TGA_IMAGING
}

}
