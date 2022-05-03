/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.natcompat.hxx
*  PURPOSE:     Compatibility transformations pixel data returned by native textures
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_NATIVE_TEXTURE_COMPAT_PRIVATE_
#define _RENDERWARE_NATIVE_TEXTURE_COMPAT_PRIVATE_

#include "txdread.d3d.dxt.hxx"

#include "txdread.nativetex.hxx"

#include "pixelutil.hxx"

#include "txdread.common.hxx"

#include <string.h>

namespace rw
{

// Compatibility routines to make sure that pixel data can be properly pushed to
// native textures.

inline void CompatibilityTransformPixelData( Interface *engineInterface, pixelDataTraversal& pixelData, const texNativeTypeProvider *capsProvider )
{
    // Get the general capabilities struct that we have to obey.
    pixelCapabilities pixelCaps;
    capsProvider->GetPixelCapabilities( pixelCaps );

    // Make sure the pixelData does not violate the capabilities struct.
    // This is done by "downcasting". It preserves maximum image quality, but increases memory requirements.

    // First get the original parameters onto stack.
    eRasterFormat srcRasterFormat = pixelData.rasterFormat;
    uint32 srcDepth = pixelData.depth;
    uint32 srcRowAlignment = pixelData.rowAlignment;
    eColorOrdering srcColorOrder = pixelData.colorOrder;
    ePaletteType srcPaletteType = pixelData.paletteType;
    //void *srcPaletteData = pixelData.paletteData;
    uint32 srcPaletteSize = pixelData.paletteSize;
    eCompressionType srcCompressionType = pixelData.compressionType;

    //uint32 srcMipmapCount = (uint32)pixelData.mipmaps.size();

    // Now decide the target format depending on the capabilities.
    eRasterFormat dstRasterFormat;
    uint32 dstDepth;
    uint32 dstRowAlignment;
    eColorOrdering dstColorOrder;
    ePaletteType dstPaletteType;
    uint32 dstPaletteSize;
    eCompressionType dstCompressionType;

    bool wantsUpdate =
        TransformDestinationRasterFormat(
            engineInterface,
            srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteSize, srcCompressionType,
            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstPaletteSize, dstCompressionType,
            pixelCaps, pixelData.hasAlpha
        );

    // Now the destination transformation is definately compatible with the native texture specification,
    // but there may be pitfalls due to ambiguity in raster format transformations.
    // Take that into account.
    if ( engineInterface->GetFixIncompatibleRasters() )
    {
        // Only meaningful for raw rasters.
        if ( dstCompressionType == RWCOMPRESS_NONE )
        {
            uint32 recDepth;
            eColorOrdering recColorOrder;

            bool hasRecDepth, hasRecColorOrder;

            capsProvider->GetRecommendedRasterFormat( dstRasterFormat, dstPaletteType, recDepth, hasRecDepth, recColorOrder, hasRecColorOrder );

            if ( hasRecDepth )
            {
                dstDepth = recDepth;

                wantsUpdate = true;
            }

            if ( hasRecColorOrder )
            {
                dstColorOrder = recColorOrder;

                wantsUpdate = true;
            }
        }
    }

    if ( wantsUpdate )
    {
        // Convert the pixels now.
        bool hasUpdated;
        {
            // Create a destination format struct.
            pixelFormat dstPixelFormat;

            dstPixelFormat.rasterFormat = dstRasterFormat;
            dstPixelFormat.depth = dstDepth;
            dstPixelFormat.rowAlignment = dstRowAlignment;
            dstPixelFormat.colorOrder = dstColorOrder;
            dstPixelFormat.paletteType = dstPaletteType;
            dstPixelFormat.compressionType = dstCompressionType;

            hasUpdated = ConvertPixelData( engineInterface, pixelData, dstPixelFormat );
        }

        // If we have updated at all, apply changes.
        if ( hasUpdated )
        {
            // We must have the correct parameters.
            // Here we verify problematic parameters only.
            // Params like rasterFormat are expected to be handled properly no matter what.
            assert( pixelData.compressionType == dstCompressionType );
        }
    }
}

static inline void TruncateMipmapLayer(
    Interface *engineInterface,
    uint32 layerWidth, uint32 layerHeight, uint32 mipWidth, uint32 mipHeight, void *texelSource, uint32 srcDataSize,
    uint32 itemDepth, uint32 rowAlignment, ePaletteType paletteType, eCompressionType compressionType,
    uint32 dstLayerWidth, uint32 dstLayerHeight,
    uint32& dstSurfWidthOut, uint32& dstSurfHeightOut,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    // We can truncate in many ways, depending on the format of the surface.
    uint32 dxtType;

    if ( IsDXTCompressionType( compressionType, dxtType ) )
    {
        // Truncate using DXT blocks.
        const uint32 dxtBlockDimm = 4u;

        uint32 dstSurfWidth = ALIGN_SIZE( dstLayerWidth, dxtBlockDimm );
        uint32 dstSurfHeight = ALIGN_SIZE( dstLayerHeight, dxtBlockDimm );

        void *dstTexels = nullptr;
        uint32 dstDataSize = 0;

        if ( mipWidth != dstSurfWidth || mipHeight != dstSurfHeight )
        {
            uint32 dstCompressedBlockCount = ( dstSurfWidth * dstSurfHeight / 16 );

            uint32 dxtBlockSize = getDXTBlockSize( dxtType );

            dstDataSize = ( dstCompressedBlockCount * dxtBlockSize );

            dstTexels = engineInterface->PixelAllocate( dstDataSize );

            try
            {
                uint32 dst_x = 0;
                uint32 dst_y = 0;

                const uint32 srcBlocksHorizontal = ( mipWidth / dxtBlockDimm );

                for ( uint32 block_index = 0; block_index < dstCompressedBlockCount; block_index++ )
                {
                    // Get the pointer to this DXT block.
                    void *dstDXTBlock = ( (char*)dstTexels + block_index * dxtBlockSize );

                    // Fill it in.
                    // We can either fetch a valid DXT block from the source surface or
                    // create a new DXT block which is entirely black.
                    if ( dst_x < mipWidth && dst_y < mipHeight )
                    {
                        // There has to be a valid DXT block.
                        // Calculate the block index that dst_x and dst_y point to in the source.
                        uint32 dxt_block_x = ( dst_x / dxtBlockDimm );
                        uint32 dxt_block_y = ( dst_y / dxtBlockDimm );

                        uint32 src_dxt_block_index = ( dxt_block_y * srcBlocksHorizontal + dxt_block_x );

                        const void *srcDXTBlock = ( (const char*)texelSource + src_dxt_block_index * dxtBlockSize );

                        memcpy( dstDXTBlock, srcDXTBlock, dxtBlockSize );
                    }
                    else
                    {
                        memset( dstDXTBlock, 0, dxtBlockSize );
                    }

                    // Increment block pointer.
                    dst_x += dxtBlockDimm;

                    if ( dst_x >= dstSurfWidth )
                    {
                        dst_x = 0;
                        dst_y += dxtBlockDimm;
                    }

                    if ( dst_y >= dstSurfHeight )
                    {
                        break;
                    }
                }
            }
            catch( ... )
            {
                engineInterface->PixelFree( dstTexels );

                throw;
            }
        }
        else
        {
            // Optimization.
            dstTexels = texelSource;
            dstDataSize = srcDataSize;
        }

        // Give results to the runtime.
        dstSurfWidthOut = dstSurfWidth;
        dstSurfHeightOut = dstSurfHeight;
        dstTexelsOut = dstTexels;
        dstDataSizeOut = dstDataSize;
    }
    else if ( compressionType == RWCOMPRESS_NONE )
    {
        // Allocate a new layer.
        rasterRowSize dstRowSize = getRasterDataRowSize( dstLayerWidth, itemDepth, rowAlignment );

        uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, dstLayerHeight );

        void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

        try
        {
            // Perform the truncation.
            // We want to fill the entire destination buffer with data, but only fill it with source pixels if they exist.
            // The other texels are cleared.
            rasterRowSize srcRowSize = getRasterDataRowSize( mipWidth, itemDepth, rowAlignment );

            for ( uint32 row = 0; row < dstLayerHeight; row++ )
            {
                constRasterRow srcRow;
                rasterRow dstRow = getTexelDataRow( dstTexels, dstRowSize, row );

                if ( row < layerHeight )
                {
                    srcRow = getConstTexelDataRow( texelSource, srcRowSize, row );
                }

                for ( uint32 col = 0; col < dstLayerWidth; col++ )
                {
                    uint32 real_col = getRealColumnIndexForRasterFormat( col, itemDepth, paletteType );

                    // If we have a row, we fetch a color item and apply it.
                    // Otherwise we just clear the coordinate.
                    if ( srcRow && col < layerWidth )
                    {
                        dstRow.writeBitsFromRow(
                            srcRow,
                            real_col * itemDepth,
                            real_col * itemDepth,
                            itemDepth
                        );
                    }
                    else
                    {
                        dstRow.setBits( false, itemDepth * real_col, itemDepth );
                    }
                }
            }
        }
        catch( ... )
        {
            engineInterface->PixelFree( dstTexels );

            throw;
        }

        // Return data.
        dstSurfWidthOut = dstLayerWidth;
        dstSurfHeightOut = dstLayerHeight;
        dstTexelsOut = dstTexels;
        dstDataSizeOut = dstDataSize;
    }
    else
    {
#ifdef _DEBUG
        // Should never happen.
        assert( 0 );
#endif //_DEBUG

        throw InternalErrorException( eSubsystemType::UTILITIES, L"MIPTRUNC_INTERNERR_INVCOMPR" );
    }
}

inline void AdjustPixelDataDimensions( Interface *engineInterface, pixelDataTraversal& pixelData, const nativeTextureSizeRules& sizeRules )
{
    size_t mipmapCount = pixelData.mipmaps.GetCount();

    if ( mipmapCount == 0 )
        return;

    // Get the raster format properties on the stack.
    //eRasterFormat rasterFormat = pixelData.rasterFormat;
    uint32 depth = pixelData.depth;
    uint32 rowAlignment = pixelData.rowAlignment;
    //eColorOrdering colorOrder = pixelData.colorOrder;
    ePaletteType paletteType = pixelData.paletteType;
    eCompressionType compressionType = pixelData.compressionType;

    // Check for violation of the size rules based on the base layer.
    // If the size does not match the rules, then we have to adjust the mipmaps aswell.
    const pixelDataTraversal::mipmapResource baseLayer = pixelData.mipmaps[ 0 ];

    uint32 baseLayerWidth = baseLayer.layerWidth;     // actual visible dimensions.
    uint32 baseLayerHeight = baseLayer.layerHeight;

    uint32 reqLayerWidth = baseLayerWidth;
    uint32 reqLayerHeight = baseLayerHeight;

    sizeRules.CalculateValidLayerDimensions( baseLayerWidth, baseLayerHeight, reqLayerWidth, reqLayerHeight );

    // If we comply already, we have to do nothing.
    if ( baseLayerWidth == reqLayerWidth && baseLayerHeight == reqLayerHeight )
        return;

    // We have to truncate the mipmaps.
    mipGenLevelGenerator mipGen( reqLayerWidth, reqLayerHeight );

    if ( !mipGen.isValidLevel() )
    {
        throw InternalErrorException( eSubsystemType::UTILITIES, L"MIPTRUNC_INTERNERR_MIPDIMMS" );
    }

    size_t realMipmapCount = 0;

    for ( size_t n = 0; n < mipmapCount; n++ )
    {
        bool hasEstablishedLevel = true;

        if ( n != 0 )
        {
            hasEstablishedLevel = mipGen.incrementLevel();
        }

        if ( !hasEstablishedLevel )
        {
            break;
        }

        // We truncate this layer.
        uint32 targetLayerWidth = mipGen.getLevelWidth();
        uint32 targetLayerHeight = mipGen.getLevelHeight();

        pixelDataTraversal::mipmapResource& mipLevel = pixelData.mipmaps[ n ];

        uint32 curLayerWidth = mipLevel.layerWidth;
        uint32 curLayerHeight = mipLevel.layerHeight;

        uint32 curSurfWidth = mipLevel.width;
        uint32 curSurfHeight = mipLevel.height;

        void *mipTexels = mipLevel.texels;

        if ( targetLayerWidth != curLayerWidth || targetLayerHeight != curLayerHeight )
        {
            uint32 newSurfWidth, newSurfHeight;
            void *newtexels;
            uint32 dstDataSize;

            TruncateMipmapLayer(
                engineInterface,
                curLayerWidth, curLayerHeight, curSurfWidth, curSurfHeight, mipTexels, mipLevel.dataSize,
                depth, rowAlignment, paletteType, compressionType,
                targetLayerWidth, targetLayerHeight,
                newSurfWidth, newSurfHeight,
                newtexels, dstDataSize
            );

            if ( newtexels != mipTexels )
            {
                engineInterface->PixelFree( mipTexels );

                mipLevel.texels = newtexels;
                mipLevel.dataSize = dstDataSize;
            }

            mipLevel.width = newSurfWidth;
            mipLevel.height = newSurfHeight;

            mipLevel.layerWidth = targetLayerWidth;
            mipLevel.layerHeight = targetLayerHeight;
        }

        // We have one more final mipmap level.
        realMipmapCount++;
    }

    // If we have mipmap layers that should not be added anymore, remove them.
    if ( realMipmapCount != mipmapCount )
    {
        for ( size_t n = realMipmapCount; n < mipmapCount; n++ )
        {
            pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

            // Delete this layer.
            mipLayer.Free( engineInterface );
        }

        // Reduce the total count of mipmaps.
        pixelData.mipmaps.Resize( realMipmapCount );
    }

    // Finito.
}

inline void AdjustPixelDataDimensionsByFormat( Interface *engineInterface, texNativeTypeProvider *texProvider, pixelDataTraversal& pixelData )
{
    // This is just a helper function to get the required size rules.
    nativeTextureSizeRules sizeRules;
    {
        pixelFormat pixFormat;
        pixFormat.rasterFormat = pixelData.rasterFormat;
        pixFormat.depth = pixelData.depth;
        pixFormat.rowAlignment = pixelData.rowAlignment;
        pixFormat.colorOrder = pixelData.colorOrder;
        pixFormat.paletteType = pixelData.paletteType;
        pixFormat.compressionType = pixelData.compressionType;

        texProvider->GetFormatSizeRules( pixFormat, sizeRules );
    }

    AdjustPixelDataDimensions( engineInterface, pixelData, sizeRules );
}

};

#endif //_RENDERWARE_NATIVE_TEXTURE_COMPAT_PRIVATE_
