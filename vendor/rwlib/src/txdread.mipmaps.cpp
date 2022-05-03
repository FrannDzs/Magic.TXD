/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.mipmaps.cpp
*  PURPOSE:     Mipmap utilities for textures.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.raster.hxx"

namespace rw
{

uint32 Raster::getMipmapCount( void ) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    uint32 mipmapCount = 0;

    if ( PlatformTexture *platformTex = this->platformData )
    {
        texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

        if ( texProvider )
        {
            mipmapCount = GetNativeTextureMipmapCount( engineInterface, platformTex, texProvider );
        }
    }

    return mipmapCount;
}

void Raster::clearMipmaps( void )
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

    // Make sure we are mutable.
    NativeCheckRasterMutable( this );

    Interface *engineInterface = this->engineInterface;

    PlatformTexture *platformTex = this->platformData;
        
    if ( platformTex )
    {
        texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

        // Clear mipmaps. This is image data starting from level index 1.
        nativeTextureBatchedInfo info;

        texProvider->GetTextureInfo( engineInterface, platformTex, info );

        if ( info.mipmapCount > 1 )
        {
            // Call into the native type provider.
            texProvider->ClearMipmaps( engineInterface, platformTex );
        }

        // Now we can have automatically generated mipmaps!
    }
}

inline uint32 pow2( uint32 val )
{
    uint32 n = 1;

    while ( val )
    {
        n *= 2;

        val--;
    }

    return n;
}

// TODO: really get rid of this.

inline bool performMipmapFiltering(
    eMipmapGenerationMode mipGenMode,
    Bitmap& srcBitmap, eColorModel model, uint32 srcPosX, uint32 srcPosY, uint32 mipProcessWidth, uint32 mipProcessHeight,
    abstractColorItem& colorItem
)
{
    bool success = false;

    if ( mipGenMode == MIPMAPGEN_DEFAULT )
    {
        if ( model == COLORMODEL_RGBA )
        {
            additive_expand <decltype( colorItem.rgbaColor.r )> redSumm = 0;
            additive_expand <decltype( colorItem.rgbaColor.g )> greenSumm = 0;
            additive_expand <decltype( colorItem.rgbaColor.b )> blueSumm = 0;
            additive_expand <decltype( colorItem.rgbaColor.a )> alphaSumm = 0;

            // Loop through the texels and calculate a blur.
            uint32 addCount = 0;

            for ( uint32 y = 0; y < mipProcessHeight; y++ )
            {
                for ( uint32 x = 0; x < mipProcessWidth; x++ )
                {
                    rwAbstractColorItem colorItem;

                    bool hasColor = srcBitmap.browsecolorex(
                        x + srcPosX, y + srcPosY,
                        colorItem
                    );

                    if ( hasColor )
                    {
                        // Add colors together.
                        redSumm += colorItem.rgbaColor.r;
                        greenSumm += colorItem.rgbaColor.g;
                        blueSumm += colorItem.rgbaColor.b;
                        alphaSumm += colorItem.rgbaColor.a;

                        addCount++;
                    }
                }
            }

            if ( addCount != 0 )
            {
                // Calculate the real color.
                colorItem.rgbaColor.r = std::min( redSumm / addCount, color_defaults <decltype( redSumm )>::one );
                colorItem.rgbaColor.g = std::min( greenSumm / addCount, color_defaults <decltype( greenSumm )>::one );
                colorItem.rgbaColor.b = std::min( blueSumm / addCount, color_defaults <decltype( blueSumm )>::one );
                colorItem.rgbaColor.a = std::min( alphaSumm / addCount, color_defaults <decltype( alphaSumm )>::one );

                colorItem.model = COLORMODEL_RGBA;

                success = true;
            }
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            additive_expand <decltype( colorItem.luminance.lum )> lumSumm = 0;
            additive_expand <decltype( colorItem.luminance.alpha )> alphaSumm = 0;

            // Loop through the texels and calculate a blur.
            uint32 addCount = 0;

            for ( uint32 y = 0; y < mipProcessHeight; y++ )
            {
                for ( uint32 x = 0; x < mipProcessWidth; x++ )
                {
                    uint8 lumm, a;

                    bool hasColor = srcBitmap.browselum(
                        x + srcPosX, y + srcPosY,
                        lumm, a
                    );

                    if ( hasColor )
                    {
                        // Add colors together.
                        lumSumm += lumm;
                        alphaSumm += a;

                        addCount++;
                    }
                }
            }

            if ( addCount != 0 )
            {
                // Calculate the real color.
                colorItem.luminance.lum = std::min( lumSumm / addCount, color_defaults <decltype( lumSumm )>::one );
                colorItem.luminance.alpha = std::min( alphaSumm / addCount, color_defaults <decltype( alphaSumm )>::one );

                colorItem.model = COLORMODEL_LUMINANCE;

                success = true;
            }
        }
    }

    return success;
}

template <typename dataType, typename containerType>
inline void ensurePutIntoArray( dataType dataToPut, containerType& container, uint32 putIndex )
{
    if ( container.size() <= putIndex )
    {
        container.resize( putIndex + 1 );
    }

    container[ putIndex ] = dataToPut;
}

// TODO: maybe in the future I will combine the resize filtering with this mipmap generation logic.
// For now I see no need to, especially since both use the same logic.

void Raster::generateMipmaps( uint32 maxMipmapCount, eMipmapGenerationMode mipGenMode )
{
    // Grab the bitmap of this texture, so we can generate mipmaps.
    Bitmap textureBitmap = this->getBitmap();

    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

    // Make sure we are mutable.
    NativeCheckRasterMutable( this );

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

    // We cannot do anything if we do not have a first level (base texture).
    nativeTextureBatchedInfo nativeInfo;

    texProvider->GetTextureInfo( engineInterface, platformTex, nativeInfo );

    uint32 oldMipmapCount = nativeInfo.mipmapCount;

    if ( oldMipmapCount == 0 )
        return;

    // Do the generation.
    // We process the image in 2x2 blocks for the level index 1, 4x4 for level index 2, ...
    uint32 firstLevelWidth, firstLevelHeight;
    textureBitmap.getSize( firstLevelWidth, firstLevelHeight );

    uint32 firstLevelDepth = textureBitmap.getDepth();
    uint32 firstLevelRowAlignment = textureBitmap.getRowAlignment();

    eRasterFormat tmpRasterFormat = textureBitmap.getFormat();
    eColorOrdering tmpColorOrder = textureBitmap.getColorOrder();

    mipGenLevelGenerator mipLevelGen( firstLevelWidth, firstLevelHeight );

    if ( !mipLevelGen.isValidLevel() )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_INTERNERR_MIPGEN_INVMIPDIMMS" );
    }

    uint32 curMipProcessWidth = 1;
    uint32 curMipProcessHeight = 1;

    uint32 curMipIndex = 0;

    uint32 actualNewMipmapCount = oldMipmapCount;

    while ( true )
    {
        bool canProcess = false;

        if ( !canProcess )
        {
            if ( curMipIndex >= oldMipmapCount )
            {
                canProcess = true;
            }
        }

        if ( canProcess )
        {
            // Check whether the current gen index does not overshoot the max gen count.
            if ( curMipIndex >= maxMipmapCount )
            {
                break;
            }

            // Get the processing dimensions.
            uint32 mipWidth = mipLevelGen.getLevelWidth();
            uint32 mipHeight = mipLevelGen.getLevelHeight();

            // Allocate the new bitmap.
            rasterRowSize texRowSize = getRasterDataRowSize( mipWidth, firstLevelDepth, firstLevelRowAlignment );

            uint32 texDataSize = getRasterDataSizeByRowSize( texRowSize, mipHeight );

            void *newtexels = engineInterface->PixelAllocate( texDataSize );

            texNativeTypeProvider::acquireFeedback_t acquireFeedback;

            bool couldAdd = false;

            try
            {
                // Process the pixels.
                bool hasAlpha = false;

                colorModelDispatcher putDispatch( tmpRasterFormat, tmpColorOrder, firstLevelDepth, nullptr, 0, PALETTE_NONE );

                eColorModel srcColorModel = textureBitmap.getColorModel();
                   
                for ( uint32 mip_y = 0; mip_y < mipHeight; mip_y++ )
                {
                    rasterRow dstRow = getTexelDataRow( newtexels, texRowSize, mip_y );

                    for ( uint32 mip_x = 0; mip_x < mipWidth; mip_x++ )
                    {
                        // Get the color for this pixel.
                        abstractColorItem colorItem;

                        // Perform a filter operation on the currently selected texture block.
                        uint32 mipProcessWidth = curMipProcessWidth;
                        uint32 mipProcessHeight = curMipProcessHeight;

                        bool couldPerform = performMipmapFiltering(
                            mipGenMode,
                            textureBitmap, srcColorModel,
                            mip_x * mipProcessWidth, mip_y * mipProcessHeight,
                            mipProcessWidth, mipProcessHeight,
                            colorItem
                        );

                        // Put the color.
                        if ( couldPerform == true )
                        {
                            // Decide if we have alpha.
                            if ( srcColorModel == COLORMODEL_RGBA )
                            {
                                if ( colorItem.rgbaColor.a != color_defaults <decltype( colorItem.rgbaColor.a )>::one )
                                {
                                    hasAlpha = true;
                                }
                            }
                            else if ( srcColorModel == COLORMODEL_LUMINANCE )
                            {
                                if ( colorItem.luminance.alpha != color_defaults <decltype( colorItem.luminance.alpha )>::one )
                                {
                                    hasAlpha = true;
                                }
                            }

                            putDispatch.setColor( dstRow, mip_x, colorItem );
                        }
                        else
                        {
                            // We do have alpha.
                            hasAlpha = true;

                            putDispatch.clearColor( dstRow, mip_x );
                        }
                    }
                }

                // Push the texels into the texture.
                rawMipmapLayer rawMipLayer;

                rawMipLayer.mipData.width = mipWidth;
                rawMipLayer.mipData.height = mipHeight;

                rawMipLayer.mipData.layerWidth = mipWidth;   // layer dimensions.
                rawMipLayer.mipData.layerHeight = mipHeight;

                rawMipLayer.mipData.texels = newtexels;
                rawMipLayer.mipData.dataSize = texDataSize;

                rawMipLayer.rasterFormat = tmpRasterFormat;
                rawMipLayer.depth = firstLevelDepth;
                rawMipLayer.rowAlignment = firstLevelRowAlignment;
                rawMipLayer.colorOrder = tmpColorOrder;
                rawMipLayer.paletteType = PALETTE_NONE;
                rawMipLayer.paletteData = nullptr;
                rawMipLayer.paletteSize = 0;
                rawMipLayer.compressionType = RWCOMPRESS_NONE;
                    
                rawMipLayer.hasAlpha = hasAlpha;

                rawMipLayer.isNewlyAllocated = true;

                couldAdd = texProvider->AddMipmapLayer(
                    engineInterface, platformTex, rawMipLayer, acquireFeedback
                );
            }
            catch( ... )
            {
                // We have not successfully pushed the texels, so deallocate us.
                engineInterface->PixelFree( newtexels );

                throw;
            }

            if ( couldAdd == false || acquireFeedback.hasDirectlyAcquired == false )
            {
                // If the texture has not directly acquired the texels, we must free our copy.
                engineInterface->PixelFree( newtexels );
            }

            if ( couldAdd == false )
            {
                // If we failed to add any mipmap, we abort operation.
                break;
            }

            // We have a new mipmap.
            actualNewMipmapCount++;
        }

        // Increment mip index.
        curMipIndex++;

        // Process parameters.
        bool shouldContinue = mipLevelGen.incrementLevel();

        if ( shouldContinue )
        {
            if ( mipLevelGen.didIncrementWidth() )
            {
                curMipProcessWidth *= 2;
            }

            if ( mipLevelGen.didIncrementHeight() )
            {
                curMipProcessHeight *= 2;
            }
        }
        else
        {
            break;
        }
    }
}

}