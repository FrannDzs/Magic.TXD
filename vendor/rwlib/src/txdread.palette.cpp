/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.palette.cpp
*  PURPOSE:     Palette generation implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include <bitset>
#include <cmath>

#include "pixelformat.hxx"

#include "txdread.d3d.hxx"

#include "txdread.palette.hxx"

#include "txdread.raster.hxx"

#ifdef RWLIB_INCLUDE_LIBIMAGEQUANT
// Include the libimagequant library headers.
#include <libimagequant.h>
#endif //RWLIB_INCLUDE_LIBIMAGEQUANT

namespace rw
{

inline void nativePaletteRemap(
    Interface *engineInterface,
    palettizer& conv, ePaletteType convPaletteFormat, uint32 convItemDepth,
    const void *texelSource, uint32 mipWidth, uint32 mipHeight,
    ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteCount,
    eRasterFormat srcRasterFormat, eColorOrdering srcColorOrder, uint32 srcItemDepth,
    uint32 srcRowAlignment, uint32 dstRowAlignment,
    void*& texelsOut, uint32& dataSizeOut
)
{
    if ( convItemDepth != 4 && convItemDepth != 8 )
    {
        // Unsupported depth.
        throw InvalidConfigurationException( eSubsystemType::PALETTE, L"PALETTE_REMAP_INVALIDCFG_UNSUPPPALDEPTH" );
    }

    rasterRowSize srcRowSize = getRasterDataRowSize( mipWidth, srcItemDepth, srcRowAlignment );

    // Allocate appropriate memory.
    rasterRowSize dstRowSize = getRasterDataRowSize( mipWidth, convItemDepth, dstRowAlignment );

    uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, mipHeight );

    void *newTexelData = engineInterface->PixelAllocate( dstDataSize );

    try
    {
        colorModelDispatcher fetchDispatch( srcRasterFormat, srcColorOrder, srcItemDepth, srcPaletteData, srcPaletteCount, srcPaletteType );

        for ( uint32 row = 0; row < mipHeight; row++ )
        {
            constRasterRow srcRow = getConstTexelDataRow( texelSource, srcRowSize, row );
            rasterRow dstRow = getTexelDataRow( newTexelData, dstRowSize, row );

            for ( uint32 col = 0; col < mipWidth; col++ )
            {
                // Browse each texel of the original image and link it to a palette entry.
                uint8 red, green, blue, alpha;
                bool hasColor = fetchDispatch.getRGBA(srcRow, col, red, green, blue, alpha);

                if ( !hasColor )
                {
                    red = 0;
                    green = 0;
                    blue = 0;
                    alpha = 0;
                }

                uint32 paletteIndex = conv.getclosestlink(red, green, blue, alpha);

                // Store it in the palette data.
                setpaletteindex(dstRow, col, convItemDepth, convPaletteFormat, paletteIndex);
            }
        }
    }
    catch( ... )
    {
        engineInterface->PixelFree( newTexelData );

        throw;
    }

    // Give the parameters to the runtime.
    texelsOut = newTexelData;
    dataSizeOut = dstDataSize;
}

#ifdef RWLIB_INCLUDE_LIBIMAGEQUANT
struct _fetch_texel_libquant_traverse
{
    pixelDataTraversal *pixelData;

    uint32 mipIndex;
};

static void _fetch_image_data_libquant(liq_color row_out[], int row_index, int width, void *user_info)
{
    _fetch_texel_libquant_traverse *info = (_fetch_texel_libquant_traverse*)user_info;

    pixelDataTraversal *pixelData = info->pixelData;

    // Fetch the color row.
    pixelDataTraversal::mipmapResource& mipLayer = pixelData->mipmaps[ info->mipIndex ];

    const void *texelSource = mipLayer.texels;

    eRasterFormat rasterFormat = pixelData->rasterFormat;
    ePaletteType paletteType = pixelData->paletteType;

    uint32 itemDepth = pixelData->depth;

    eColorOrdering colorOrder = pixelData->colorOrder;

    void *palColors = pixelData->paletteData;
    uint32 palColorCount = pixelData->paletteSize;

    // Get the row of colors.
    rasterRowSize srcRowSize = getRasterDataRowSize( mipLayer.layerWidth, itemDepth, pixelData->rowAlignment );

    constRasterRow srcRow = getConstTexelDataRow( texelSource, srcRowSize, row_index );

    colorModelDispatcher fetchDispatch( rasterFormat, colorOrder, itemDepth, palColors, palColorCount, paletteType );

    for (int n = 0; n < width; n++)
    {
        // Fetch the color.
        uint8 r = 0;
        uint8 g = 0;
        uint8 b = 0;
        uint8 a = 0;

        fetchDispatch.getRGBA( srcRow, n, r, g, b, a );

        // Store the color.
        liq_color& outColor = row_out[ n ];
        outColor.r = r;
        outColor.g = g;
        outColor.b = b;
        outColor.a = a;
    }
}
#endif //RWLIB_INCLUDE_LIBIMAGEQUANT

// Custom algorithm for palettizing image data.
// This routine is called by ConvertPixelData. It should not be called from anywhere else.
void PalettizePixelData( Interface *engineInterface, pixelDataTraversal& pixelData, const pixelFormat& dstPixelFormat )
{
    // Make sure the pixelData is not compressed.
    assert( pixelData.compressionType == RWCOMPRESS_NONE );
    assert( dstPixelFormat.compressionType == RWCOMPRESS_NONE );

    ePaletteType convPaletteFormat = dstPixelFormat.paletteType;

    if (convPaletteFormat != PALETTE_8BIT &&
        convPaletteFormat != PALETTE_4BIT &&
        convPaletteFormat != PALETTE_4BIT_LSB)
    {
        throw InternalErrorException( eSubsystemType::PALETTE, L"PALETTE_INTERNERR_UNKPALFMT" );
    }

    ePaletteType srcPaletteType = pixelData.paletteType;

    // The reason for this shortcut is because the purpose of this algorithm is palettization.
    // If you want to change the raster format or anything, use ConvertPixelData!
    if (srcPaletteType == convPaletteFormat)
        return;

    // Get the source format.
    eRasterFormat srcRasterFormat = pixelData.rasterFormat;
    eColorOrdering srcColorOrder = pixelData.colorOrder;
    uint32 srcDepth = pixelData.depth;
    uint32 srcRowAlignment = pixelData.rowAlignment;

    // Get the format that we want to output in.
    eRasterFormat dstRasterFormat = dstPixelFormat.rasterFormat;
    uint32 dstDepth = dstPixelFormat.depth;
    eColorOrdering dstColorOrder = dstPixelFormat.colorOrder;
    uint32 dstRowAlignment = dstPixelFormat.rowAlignment;

    void *srcPaletteData = pixelData.paletteData;
    uint32 srcPaletteCount = pixelData.paletteSize;

    // Get palette maximums.
    uint32 maxPaletteEntries = 0;

    bool hasValidPaletteTarget = false;

    if (dstDepth == 8)
    {
        if (convPaletteFormat == PALETTE_8BIT)
        {
            maxPaletteEntries = 256;

            hasValidPaletteTarget = true;
        }
        else if (convPaletteFormat == PALETTE_4BIT || convPaletteFormat == PALETTE_4BIT_LSB)
        {
            maxPaletteEntries = 16;

            hasValidPaletteTarget = true;
        }
    }
    else if (dstDepth == 4)
    {
        if (convPaletteFormat == PALETTE_4BIT || convPaletteFormat == PALETTE_4BIT_LSB)
        {
            maxPaletteEntries = 16;

            hasValidPaletteTarget = true;
        }
    }

    if ( hasValidPaletteTarget == false )
    {
        throw InternalErrorException( eSubsystemType::PALETTE, L"PALETTE_INTERNERR_UNKPALDEPTH" );
    }

    // Do the palettization.
    bool palettizeSuccess = false;
    {
        uint32 mipmapCount = (uint32)pixelData.mipmaps.GetCount();

        // Decide what palette system to use.
        ePaletteRuntimeType useRuntime = engineInterface->GetPaletteRuntime();

        if (useRuntime == PALRUNTIME_NATIVE)
        {
            palettizer conv;

            // Linear eliminate unique texels.
            // Use only the first texture.
            if ( mipmapCount > 0 )
            {
                pixelDataTraversal::mipmapResource& mainLayer = pixelData.mipmaps[ 0 ];

                uint32 srcWidth = mainLayer.layerWidth;
                uint32 srcHeight = mainLayer.layerHeight;
                //uint32 srcStride = mainLayer.width;
                void *texelSource = mainLayer.texels;

                rasterRowSize srcRowSize = getRasterDataRowSize( srcWidth, srcDepth, srcRowAlignment );

#if 0
                // First define properties to use for linear elimination.
                for (uint32 y = 0; y < srcHeight; y++)
                {
                    for (uint32 x = 0; x < srcWidth; x++)
                    {
                        uint32 colorIndex = PixelFormat::coord2index(x, y, srcWidth);

                        uint8 red, green, blue, alpha;
                        bool hasColor = browsetexelcolor(texelSource, paletteType, paletteData, maxpalette, colorIndex, rasterFormat, red, green, blue, alpha);

                        if ( hasColor )
                        {
                            conv.characterize(red, green, blue, alpha);
                        }
                    }
                }

                // Prepare the linear elimination.
                conv.after_characterize();
#endif

                colorModelDispatcher fetchDispatch( srcRasterFormat, srcColorOrder, srcDepth, srcPaletteData, srcPaletteCount, srcPaletteType );

                // Linear eliminate.
                for (uint32 y = 0; y < srcHeight; y++)
                {
                    constRasterRow srcRow = getConstTexelDataRow( texelSource, srcRowSize, y );

                    for (uint32 x = 0; x < srcWidth; x++)
                    {
                        uint8 red, green, blue, alpha;
                        bool hasColor = fetchDispatch.getRGBA( srcRow, x, red, green, blue, alpha );

                        if ( hasColor )
                        {
                            conv.feedcolor(red, green, blue, alpha);
                        }
                    }
                }
            }

            // Construct a palette out of the remaining colors.
            conv.constructpalette(maxPaletteEntries);

            // Point each color from the original texture to the palette.
            for (uint32 n = 0; n < mipmapCount; n++)
            {
                // Create palette index memory for each mipmap.
                pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

                uint32 srcWidth = mipLayer.width;
                uint32 srcHeight = mipLayer.height;
                void *texelSource = mipLayer.texels;

                uint32 dataSize = 0;
                void *newTexelData = nullptr;

                // Remap the texels.
                nativePaletteRemap(
                    engineInterface,
                    conv, convPaletteFormat, dstDepth,
                    texelSource, srcWidth, srcHeight,
                    srcPaletteType, srcPaletteData, srcPaletteCount, srcRasterFormat, srcColorOrder, srcDepth,
                    srcRowAlignment, dstRowAlignment,
                    newTexelData, dataSize
                );

                // Replace texture data.
                if ( newTexelData != texelSource )
                {
                    if ( texelSource )
                    {
                        engineInterface->PixelFree( texelSource );
                    }

                    mipLayer.texels = newTexelData;
                }

                mipLayer.dataSize = dataSize;
            }

            // Delete the old palette data (if available).
            if (srcPaletteData != nullptr)
            {
                engineInterface->PixelFree( srcPaletteData );

                pixelData.paletteData = nullptr;
            }

            // Store the new palette texels.
            pixelData.paletteData = conv.makepalette(engineInterface, dstRasterFormat, dstColorOrder);
            pixelData.paletteSize = (uint32)conv.texelElimData.GetCount();

            palettizeSuccess = true;
        }
#ifdef RWLIB_INCLUDE_LIBIMAGEQUANT
        else if (useRuntime == PALRUNTIME_PNGQUANT)
        {
            liq_attr *quant_attr = liq_attr_create();

            if ( quant_attr == nullptr )
            {
                throw PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_LIBIMGQUANT_ATTRFAIL" );
            }

            try
            {
                liq_set_max_colors(quant_attr, maxPaletteEntries);

                _fetch_texel_libquant_traverse main_traverse;

                main_traverse.pixelData = &pixelData;
                main_traverse.mipIndex = 0;

                pixelDataTraversal::mipmapResource& mainLayer = pixelData.mipmaps[ 0 ];

                liq_image *quant_image = liq_image_create_custom(
                    quant_attr, _fetch_image_data_libquant, &main_traverse,
                    mainLayer.width, mainLayer.height,
                    1.0
                );

                if ( quant_image == nullptr )
                {
                    throw PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_LIBIMGQUANT_IMGHANDLEFAIL" );
                }

                try
                {
                    // Quant it!
                    liq_result *quant_result = liq_quantize_image(quant_attr, quant_image);

                    if (quant_result == nullptr)
                    {
                        PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_INTERNERR_LIBIMGQUANT_QUANTIZEFAIL" );
                    }

                    try
                    {
                        // Get the palette and remap all mipmaps.
                        for (uint32 n = 0; n < mipmapCount; n++)
                        {
                            pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

                            uint32 mipWidth = mipLayer.width;
                            uint32 mipHeight = mipLayer.height;

                            size_t liqPaletteSize = ( mipWidth * mipHeight ) * sizeof(unsigned char);

                            unsigned char *newPalItems = (unsigned char*)engineInterface->PixelAllocate( liqPaletteSize );

                            // Set this to true if newPalItems should not be deallocated because you actually use it.
                            bool hasUsedArray = false;

                            try
                            {
                                liq_image *srcImage = nullptr;
                                bool newImage = false;

                                _fetch_texel_libquant_traverse thisTraverse;

                                if ( n == 0 )
                                {
                                    srcImage = quant_image;
                                }
                                else
                                {
                                    // Create a new image.
                                    thisTraverse.pixelData = &pixelData;
                                    thisTraverse.mipIndex = n;

                                    srcImage = liq_image_create_custom(
                                        quant_attr, _fetch_image_data_libquant, &thisTraverse,
                                        mipWidth, mipHeight,
                                        1.0
                                    );

                                    if ( srcImage == nullptr )
                                    {
                                        throw PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_LIBIMGQUANT_IMGHANDLEFAIL" );
                                    }

                                    newImage = true;
                                }

                                try
                                {
                                    // Map it.
                                    liq_error remapOK = liq_write_remapped_image( quant_result, srcImage, newPalItems, liqPaletteSize );

                                    if ( remapOK != LIQ_OK )
                                    {
                                        throw PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_INTERNERR_LIBIMGQUANT_REMAPFAIL" );
                                    }
                                }
                                catch( ... )
                                {
                                    // A really short error handler, I know!
                                    if ( newImage )
                                    {
                                        liq_image_destroy( srcImage );
                                    }

                                    throw;
                                }

                                // Delete image (if newly allocated)
                                if (newImage)
                                {
                                    liq_image_destroy( srcImage );
                                }

                                // Update the texels.
                                engineInterface->PixelFree( mipLayer.texels );

                                mipLayer.texels = nullptr;

                                {
                                    uint32 dataSize = 0;
                                    void *newTexelArray = nullptr;

                                    rasterRowSize srcRowSize = getRasterDataRowSize( mipWidth, 8u, 1 );

                                    rasterRowSize dstRowSize = getRasterDataRowSize( mipWidth, dstDepth, dstRowAlignment );

                                    dataSize = getRasterDataSizeByRowSize( dstRowSize, mipHeight );

                                    if (dstDepth == 8 && dataSize == liqPaletteSize)
                                    {
                                        // If we have the same size as the liq palette index array,
                                        // we can simply use it.
                                        newTexelArray = newPalItems;

                                        hasUsedArray = true;
                                    }

                                    if ( !hasUsedArray )
                                    {
                                        newTexelArray = engineInterface->PixelAllocate( dataSize );

                                        try
                                        {
                                            // Copy over the items.
                                            for ( uint32 row = 0; row < mipHeight; row++ )
                                            {
                                                constRasterRow srcRow = getTexelDataRow( newPalItems, srcRowSize, row );
                                                rasterRow dstRow = getTexelDataRow( newTexelArray, dstRowSize, row );

                                                for ( uint32 col = 0; col < mipWidth; col++ )
                                                {
                                                    uint8 resVal = *((uint8*)srcRow.aligned.aligned_rowPtr + col);

                                                    setpaletteindex(dstRow, col, dstDepth, convPaletteFormat, resVal);
                                                }
                                            }
                                        }
                                        catch( ... )
                                        {
                                            // Never say never about code being able to throw exceptions.
                                            // It can always make sense in the future.
                                            engineInterface->PixelFree( newTexelArray );

                                            throw;
                                        }
                                    }

                                    // Set the texels.
                                    mipLayer.texels = newTexelArray;
                                    mipLayer.dataSize = dataSize;
                                }
                            }
                            catch( ... )
                            {
                                engineInterface->PixelFree( newPalItems );

                                throw;
                            }

                            if ( !hasUsedArray )
                            {
                                engineInterface->PixelFree( newPalItems );
                            }
                        }

                        // Delete the old palette data.
                        if (srcPaletteData != nullptr)
                        {
                            engineInterface->PixelFree( srcPaletteData );

                            // This is really important. We cannot keep things in an inconsistent state
                            // if we want exception-safe code.
                            pixelData.paletteData = nullptr;
                        }

                        // Update the texture palette data.
                        {
                            const liq_palette *palData = liq_get_palette(quant_result);

                            uint32 newPalItemCount = palData->count;

                            uint32 palDepth = Bitmap::getRasterFormatDepth(dstRasterFormat);

                            colorModelDispatcher putDispatch( dstRasterFormat, dstColorOrder, palDepth, nullptr, 0, PALETTE_NONE );

                            uint32 palDataSize = getPaletteDataSize( newPalItemCount, palDepth );

                            void *newPalArray = engineInterface->PixelAllocate( palDataSize );

                            try
                            {
                                rasterRow palrow = newPalArray;

                                for ( unsigned int n = 0; n < newPalItemCount; n++ )
                                {
                                    const liq_color& srcColor = palData->entries[ n ];

                                    putDispatch.setRGBA(palrow, n, srcColor.r, srcColor.g, srcColor.b, srcColor.a);
                                }
                            }
                            catch( ... )
                            {
                                engineInterface->PixelFree( newPalArray );

                                throw;
                            }

                            // Update texture properties.
                            pixelData.paletteData = newPalArray;
                            pixelData.paletteSize = newPalItemCount;
                        }
                    }
                    catch( ... )
                    {
                        liq_result_destroy( quant_result );

                        throw;
                    }

                    // Release resources.
                    liq_result_destroy( quant_result );
                }
                catch( ... )
                {
                    liq_image_destroy( quant_image );

                    throw;
                }

                liq_image_destroy( quant_image );
            }
            catch( ... )
            {
                // Be a good implementation and take care of errors :)
                // Even if they are unlikely to happen now, who knows about the future?
                liq_attr_destroy( quant_attr );

                throw;
            }

            liq_attr_destroy( quant_attr );

            palettizeSuccess = true;
        }
#endif //RWLIB_INCLUDE_LIBIMAGEQUANT
        else
        {
            // A safe assertion that should never be triggered.
            assert( 0 );
        }
    }

    // If the palettization was a success, we update the pixelData raster format fields.
    if ( palettizeSuccess )
    {
        if ( srcRasterFormat != dstRasterFormat )
        {
            pixelData.rasterFormat = dstRasterFormat;
        }

        if ( srcColorOrder != dstColorOrder )
        {
            pixelData.colorOrder = dstColorOrder;
        }

        // Set the new depth of the texture.
        if ( srcDepth != dstDepth )
        {
            pixelData.depth = dstDepth;
        }

        // We have a new row alignment, maybe.
        if ( srcRowAlignment != dstRowAlignment )
        {
            pixelData.rowAlignment = dstRowAlignment;
        }

        // Notify the raster about its new format.
        pixelData.paletteType = convPaletteFormat;
    }
}

void Raster::convertToPalette( ePaletteType paletteType, eRasterFormat newRasterFormat )
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

    // Make sure we are mutable.
    NativeCheckRasterMutable( this );

    // nullptr operation.
    if ( paletteType == PALETTE_NONE )
        return;

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RasterNotInitializedException( AcquireRaster( this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    // If the raster has the same palettization as we request, we can terminate early.
    ePaletteType currentPaletteType = texProvider->GetTexturePaletteType( platformTex );

    if ( currentPaletteType == paletteType )
    {
        eRasterFormat currentRasterFormat = texProvider->GetTextureRasterFormat( platformTex );

        if ( newRasterFormat == RASTER_DEFAULT || currentRasterFormat == newRasterFormat )
        {
            return;
        }
    }

    // Get palette default capabilities.
    uint32 dstDepth;

    if ( paletteType == PALETTE_4BIT )
    {
        dstDepth = 4;
    }
    else if ( paletteType == PALETTE_8BIT )
    {
        dstDepth = 8;
    }
    else
    {
        throw PaletteRasterInvalidParameterException( AcquireRaster( this ), L"PALETTE_PALTYPE_FRIENDLYNAME", nullptr );
    }

    // Decide whether the target raster even supports palette.
    pixelCapabilities inputTransferCaps;

    texProvider->GetPixelCapabilities( inputTransferCaps );

    if ( inputTransferCaps.supportsPalette == false )
    {
        throw PaletteRasterInvalidOperationException( AcquireRaster( this ), eOperationType::WRITE, L"PALETTE_REASON_PALUNSUPPBYRASTER" );
    }

    storageCapabilities storageCaps;

    texProvider->GetStorageCapabilities( storageCaps );

    if ( storageCaps.pixelCaps.supportsPalette == false )
    {
        throw PaletteRasterInvalidOperationException( AcquireRaster( this ), eOperationType::WRITE, L"PALETTE_REASON_STORAGE_PALUNSUPPBYRASTER" );
    }

    // Alright, our native data does support palette data.
    // We now want to fetch the rasters pixel data, make it private and palettize it.
    pixelDataTraversal pixelData;

    texProvider->GetPixelDataFromTexture( engineInterface, platformTex, pixelData );

    bool hasDirectlyAcquired = false;

    try
    {
        // Unset it from the original texture.
        texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, pixelData.isNewlyAllocated == true );

        // Pixel data is now safely stand-alone.
        pixelData.SetStandalone();

        // We always want to palettize to 32bit quality, unless the user wants otherwise.
        eRasterFormat targetRasterFormat;

        if ( newRasterFormat == RASTER_DEFAULT )
        {
            if ( pixelData.hasAlpha )
            {
                targetRasterFormat = RASTER_8888;
            }
            else
            {
                targetRasterFormat = RASTER_888;
            }
        }
        else
        {
            targetRasterFormat = newRasterFormat;
        }

        // Convert the pixel data to palette.
        pixelFormat targetPixelFormat;
        targetPixelFormat.rasterFormat = targetRasterFormat;
        targetPixelFormat.depth = dstDepth;
        targetPixelFormat.rowAlignment = 4; // good measure.
        targetPixelFormat.colorOrder = pixelData.colorOrder;
        targetPixelFormat.paletteType = paletteType;
        targetPixelFormat.compressionType = RWCOMPRESS_NONE;

        bool hasConverted = ConvertPixelData( engineInterface, pixelData, targetPixelFormat );

        if ( !hasConverted )
        {
            throw InternalErrorException( eSubsystemType::PALETTE, L"PALETTE_INTERNERR_PIXELCONVFAIL" );
        }

        // Adjust dimensions, so they are correct.
        AdjustPixelDataDimensionsByFormat( engineInterface, texProvider, pixelData );

        // Now set the pixels to the texture again.
        texNativeTypeProvider::acquireFeedback_t acquireFeedback;

        texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );

        hasDirectlyAcquired = acquireFeedback.hasDirectlyAcquired;
    }
    catch( ... )
    {
        pixelData.FreePixels( engineInterface );

        throw;
    }

    if ( hasDirectlyAcquired == false )
    {
        // This should never happen.
        pixelData.FreePixels( engineInterface );
    }
    else
    {
        pixelData.DetachPixels();
    }
}

ePaletteType Raster::getPaletteType( void ) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RasterNotInitializedException( AcquireRaster( (Raster*)this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( (Raster*)this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    return texProvider->GetTexturePaletteType( platformTex );
}

struct liq_mipmap
{
    const void *texelSource;
    rasterRowSize rowSize;

    eRasterFormat rasterFormat;
    eColorOrdering colorOrder;
    uint32 depth;
    uint32 rowAlignment;

    ePaletteType paletteType;
    const void *paletteData;
    uint32 paletteSize;
};

#ifdef RWLIB_INCLUDE_LIBIMAGEQUANT

static void liq_single_mip_rgba_fetch_callback( liq_color row_out[], int row, int width, void *ud )
{
    const liq_mipmap *imgData = (const liq_mipmap*)ud;

    const void *texelSource = imgData->texelSource;

    colorModelDispatcher fetchDispatch(
        imgData->rasterFormat, imgData->colorOrder, imgData->depth,
        imgData->paletteData, imgData->paletteSize, imgData->paletteType
    );

    const rasterRowSize& srcRowSize = imgData->rowSize;

    constRasterRow srcRowData = getConstTexelDataRow( texelSource, srcRowSize, row );

    for ( int col = 0; col < width; col++ )
    {
        liq_color& colorOut = row_out[ col ];

        fetchDispatch.getRGBA(
            srcRowData, col, colorOut.r, colorOut.g, colorOut.b, colorOut.a
        );
    }
}

#endif //RWLIB_INCLUDE_LIBIMAGEQUANT

void RemapMipmapLayer(
    Interface *engineInterface,
    eRasterFormat palRasterFormat, eColorOrdering palColorOrder,
    const void *mipTexels, uint32 mipWidth, uint32 mipHeight,
    eRasterFormat mipRasterFormat, eColorOrdering mipColorOrder, uint32 mipDepth, ePaletteType mipPaletteType, const void *mipPaletteData, uint32 mipPaletteSize,
    const void *paletteData, uint32 paletteSize, uint32 convItemDepth, ePaletteType convPaletteType,
    uint32 srcRowAlignment, uint32 dstRowAlignment,
    void*& dstTexelsOut, uint32& dstTexelDataSizeOut
)
{
    // Determine with what algorithm we should map.
    ePaletteRuntimeType palRuntimeType = engineInterface->GetPaletteRuntime();

    uint32 palItemDepth = Bitmap::getRasterFormatDepth(palRasterFormat);

    colorModelDispatcher fetchPalDispatch( palRasterFormat, palColorOrder, palItemDepth, nullptr, 0, PALETTE_NONE );

    // Even if the palette size is bigger than the indices can address, we shall only care about the actually addressible colors.
    uint32 addressiblePaletteSize = (uint32)pow( 2, convItemDepth );

    paletteSize = std::min( addressiblePaletteSize, paletteSize );

    if ( palRuntimeType == PALRUNTIME_NATIVE )
    {
        // Do some complex remapping.
        palettizer remapper;

        // Create an array with all the palette colors.
        palettizer::texelContainer_t paletteContainer;

        paletteContainer.Resize( paletteSize );

        for ( uint32 n = 0; n < paletteSize; n++ )
        {
            uint8 r, g, b, a;

            bool hasColor = fetchPalDispatch.getRGBA(paletteData, n, r, g, b, a);

            if ( !hasColor )
            {
                r = 0;
                g = 0;
                b = 0;
                a = 0;
            }

            palettizer::texel_t inTexel;
            inTexel.red = r;
            inTexel.green = g;
            inTexel.blue = b;
            inTexel.alpha = a;

            paletteContainer[ n ] = inTexel;
        }

        // Put the palette texels into the remapper.
        remapper.texelElimData = paletteContainer;

        // Do the remap.
        nativePaletteRemap(
            engineInterface,
            remapper, convPaletteType, convItemDepth,
            mipTexels, mipWidth, mipHeight, mipPaletteType, mipPaletteData, mipPaletteSize,
            mipRasterFormat, mipColorOrder, mipDepth,
            srcRowAlignment, dstRowAlignment,
            dstTexelsOut, dstTexelDataSizeOut
        );
    }
#ifdef RWLIB_INCLUDE_LIBIMAGEQUANT
    else if ( palRuntimeType == PALRUNTIME_PNGQUANT )
    {
        liq_attr *liq_attr = liq_attr_create();

        if ( liq_attr == nullptr )
        {
            throw PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_LIBIMGQUANT_ATTRFAIL" );
        }

        // The stuff we want to eventually return.
        // If this stuff was somehow allocated we free it on error, I guess.
        void *newtexels = nullptr;
        uint32 dstDataSize = 0;

        try
        {
#if 0
            // Disallow libimagequant from sorting our colors.
            liq_set_allow_palette_sorting( liq_attr, false );
#endif

            // Since we want to remap an image, we want to create an image handle.
            // Then we want to copy palette into the library's memory and use it to search for colors.
            liq_mipmap mipData;
            mipData.texelSource = mipTexels;
            mipData.rowSize = getRasterDataRowSize( mipWidth, mipDepth, srcRowAlignment );
            mipData.rasterFormat = mipRasterFormat;
            mipData.depth = mipDepth;
            mipData.rowAlignment = srcRowAlignment;
            mipData.colorOrder = mipColorOrder;
            mipData.paletteType = mipPaletteType;
            mipData.paletteData = mipPaletteData;
            mipData.paletteSize = mipPaletteSize;

            liq_image *liq_mip_layer =
                liq_image_create_custom(
                    liq_attr, liq_single_mip_rgba_fetch_callback, &mipData,
                    mipWidth, mipHeight,
                    1.0
                );

            if ( liq_mip_layer == nullptr )
            {
                throw PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_LIBIMGQUANT_IMGHANDLEFAIL" );
            }

            try
            {
                // Copy the palette in the correct order into libimagequant memory.
                liq_set_max_colors( liq_attr, paletteSize );

                for ( uint32 n = 0; n < paletteSize; n++ )
                {
                    uint8 r, g, b, a;

                    fetchPalDispatch.getRGBA( paletteData, n, r, g, b, a );

                    liq_color theColor;

                    theColor.r = r;
                    theColor.g = g;
                    theColor.b = b;
                    theColor.a = a;

                    liq_error palAddError = liq_image_add_fixed_color( liq_mip_layer, theColor );

                    if ( palAddError != LIQ_OK )
                    {
                        throw PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_LIBIMGQUANT_COLORADDFAIL" );
                    }
                }

                // Create an output buffer.
                // It must be 8 bit.
                rasterRowSize dstRowSize = getRasterDataRowSize( mipWidth, convItemDepth, dstRowAlignment );

                dstDataSize = getRasterDataSizeByRowSize( dstRowSize, mipHeight );

                newtexels = engineInterface->PixelAllocate( dstDataSize );

                // Write the remapped image.
                // This should write configuration about colors, but should not replace palette colors.
                liq_result *liq_res = liq_quantize_image( liq_attr, liq_mip_layer );

                if ( liq_res == nullptr )
                {
                    throw PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_LIBIMGQUANT_REMAPFAIL" );
                }

                try
                {
                    // If convPaletteDepth is 8, basically what libimagequant outputs as, we can directly take those pixels.
                    // Otherwise we need a temporary buffer where libimagequant will write into and we transform to correct format afterward.
                    if ( convItemDepth == 8 && dstRowSize.mode == eRasterDataRowMode::ALIGNED )
                    {
                        // Split it into rows.
                        void **rowp = (void**)engineInterface->PixelAllocate( sizeof(void*) * mipHeight, alignof(void*) );

                        try
                        {
                            for ( uint32 n = 0; n < mipHeight; n++ )
                            {
                                rowp[ n ] = getTexelDataRow( newtexels, dstRowSize, n ).aligned.aligned_rowPtr;
                            }

                            liq_error writeError = liq_write_remapped_image_rows( liq_res, liq_mip_layer, (unsigned char**)rowp );

                            if ( writeError != LIQ_OK )
                            {
                                throw PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_INTERNERR_LIBIMGQUANT_REMAPFAIL" );
                            }
                        }
                        catch( ... )
                        {
                            engineInterface->PixelFree( rowp );

                            throw;
                        }

                        engineInterface->PixelFree( rowp );
                    }
                    else
                    {
                        // Write the pixels into a temporary buffer.
                        // Then convert to the final format for ourselves.
                        uint32 liqPackedRowSize = ( mipWidth * sizeof(unsigned char) );

                        uint32 liqPackedDataSize = ( liqPackedRowSize * mipHeight );

                        void *liqBuf = engineInterface->PixelAllocate( liqPackedDataSize );

                        try
                        {
                            liq_error writeError = liq_write_remapped_image( liq_res, liq_mip_layer, liqBuf, liqPackedDataSize );

                            if ( writeError != LIQ_OK )
                            {
                                throw PaletteInternalErrorException( PALRUNTIME_PNGQUANT, L"PALETTE_INTERNERR_LIBIMGQUANT_REMAPFAIL" );
                            }

                            // Convert the palette indice.
                            ConvertPaletteDepth(
                                liqBuf, newtexels,
                                mipWidth, mipHeight,
                                PALETTE_8BIT, convPaletteType, paletteSize,
                                8, convItemDepth,
                                1, dstRowAlignment
                            );
                        }
                        catch( ... )
                        {
                            engineInterface->PixelFree( liqBuf );

                            throw;
                        }

                        // Free the temporary buffer.
                        engineInterface->PixelFree( liqBuf );
                    }
                }
                catch( ... )
                {
                    liq_result_destroy( liq_res );

                    throw;
                }

                // Clean up after ourselves.
                liq_result_destroy( liq_res );
            }
            catch( ... )
            {
                liq_image_destroy( liq_mip_layer );

                throw;
            }

            liq_image_destroy( liq_mip_layer );
        }
        catch( ... )
        {
            // Free on error, if somebody actually allocated.
            if ( newtexels )
            {
                engineInterface->PixelFree( newtexels );
            }

            liq_attr_destroy( liq_attr );

            throw;
        }

        liq_attr_destroy( liq_attr );

        assert( newtexels != nullptr && dstDataSize != 0 );

        // Return the results.
        dstTexelsOut = newtexels;
        dstTexelDataSizeOut = dstDataSize;
    }
#endif //RWLIB_INCLUDE_LIBIMAGEQUANT
}

};
