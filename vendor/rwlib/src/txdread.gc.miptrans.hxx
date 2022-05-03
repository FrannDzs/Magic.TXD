/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.gc.miptrans.hxx
*  PURPOSE:     Gamecube native texture transcoding helpers
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

// Mipmap transformation between framework-friendly format and native format.

#ifndef _GAMECUBE_NATIVE_MIPMAP_TRANSFORMATION_
#define _GAMECUBE_NATIVE_MIPMAP_TRANSFORMATION_

#ifdef RWLIB_INCLUDE_NATIVETEX_GAMECUBE

#include "txdread.d3d.dxt.hxx"

#include "txdread.miputil.hxx"

#include "txdread.memcodec.hxx"

namespace rw
{

template <typename callbackType>
AINLINE void GCProcessRandomAccessTileSurface(
    uint32 mipWidth, uint32 mipHeight,
    uint32 clusterWidth, uint32 clusterHeight,
    uint32 clusterCount,
    bool isSwizzled,
    const callbackType& cb
)
{
    using namespace memcodec::permutationUtilities;

    if ( isSwizzled )
    {
        ProcessTextureLayerPackedTiles(
            mipWidth, mipHeight,
            clusterWidth, clusterHeight,
            clusterCount,
            cb
        );
    }
    else
    {
        GenericProcessTileLayerPerCluster <linearTileProcessor> (
            mipWidth, mipHeight,
            clusterWidth, clusterHeight, clusterCount,
            cb
        );
    }
}

AINLINE void GCGetTiledCoordFromLinear(
    uint32 linearX, uint32 linearY,
    uint32 surfWidth, uint32 surfHeight,
    uint32 clusterWidth, uint32 clusterHeight,
    bool isSwizzled,
    uint32& tiled_x, uint32& tiled_y
)
{
    using namespace memcodec::permutationUtilities;

    if ( isSwizzled )
    {
        GetPackedTiledCoordFromLinear(
            linearX, linearY,
            surfWidth, surfHeight,
            clusterWidth, clusterHeight,
            tiled_x, tiled_y
        );
    }
    else
    {
        GenericGetTiledCoordFromLinear <linearTileProcessor> (
            linearX, linearY,
            surfWidth, surfHeight,
            clusterWidth, clusterHeight,
            tiled_x, tiled_y
        );
    }
}

template <typename callbackType>
AINLINE void GCProcessTiledCoordsFromLinear(
    uint32 linearX, uint32 linearY,
    uint32 surfWidth, uint32 surfHeight,
    uint32 clusterWidth, uint32 clusterHeight, uint32 clusterCount,
    bool isFormatSwizzled,
    const callbackType& cb
)
{
    using namespace memcodec::permutationUtilities;

    if ( isFormatSwizzled )
    {
        ProcessPackedTiledCoordsFromLinear(
            linearX, linearY,
            surfWidth, surfHeight,
            clusterWidth, clusterHeight, clusterCount,
            cb
        );
    }
    else
    {
        GenericProcessTiledCoordsFromLinear <linearTileProcessor> (
            linearX, linearY,
            surfWidth, surfHeight,
            clusterWidth, clusterHeight, clusterCount,
            cb
        );
    }
}

inline void DXTIndexListInverseCopy( uint32& dstIndexList, uint32 srcIndexList, uint32 blockPixelWidth, uint32 blockPixelHeight )
{
    dstIndexList = 0;

    for ( uint32 local_y = 0; local_y < blockPixelHeight; local_y++ )
    {
        for ( uint32 local_x = 0; local_x < blockPixelWidth; local_x++ )
        {
            uint32 item = fetchDXTIndexList( srcIndexList, local_x, local_y );

            uint32 real_localDstX = ( blockPixelWidth - local_x - 1 );
            uint32 real_localDstY = ( blockPixelHeight - local_y - 1 );

            putDXTIndexList( dstIndexList, real_localDstX, real_localDstY, item );
        }
    }
}

// Gamecube DXT1 compression struct.
template <typename dispatchType, typename reEstablishColorProc>
AINLINE void readGCNativeColor(
    constRasterRow& srcRow, dispatchType& srcDispatch, uint32 src_pos_x,
    uint32 cluster_index,
    const reEstablishColorProc& reCB,
    abstractColorItem& colorItem
)
{
    if ( srcDispatch.getNativeFormat() == GVRFMT_RGBA8888 )
    {
        // This is actually a more complicated fetch operation.

        // Get alpha and red components.
        if ( cluster_index == 0 )
        {
            struct alpha_red
            {
                uint8 alpha;
                uint8 red;
            };

            alpha_red srcTuple = srcRow.readStructItem <alpha_red> ( src_pos_x );

            destscalecolorn( srcTuple.alpha, colorItem.rgbaColor.a );
            destscalecolorn( srcTuple.red, colorItem.rgbaColor.r );
            colorItem.rgbaColor.g = color_defaults <decltype( colorItem.rgbaColor.g )>::zero;
            colorItem.rgbaColor.b = color_defaults <decltype( colorItem.rgbaColor.b )>::zero;

            colorItem.model = COLORMODEL_RGBA;
        }
        // Get the green and blue components.
        else if ( cluster_index == 1 )
        {
            struct green_blue
            {
                uint8 green;
                uint8 blue;
            };

            green_blue srcTuple = srcRow.readStructItem <green_blue> ( src_pos_x );

            // We want to update the color with green and blue.
            reCB( colorItem );

            destscalecolorn( srcTuple.green, colorItem.rgbaColor.g );
            destscalecolorn( srcTuple.blue, colorItem.rgbaColor.b );
        }
        else
        {
            assert( 0 );
        }
    }
    else
    {
        // Cannot have that! And it realistically is not being done.
        // But putting this check is good as reference for the future.
        assert( cluster_index == 0 );

        srcDispatch.getColor( srcRow, src_pos_x, colorItem );
    }
}

inline void copyPaletteIndexAcrossSurfaces(
    const void *srcTexels, uint32 srcLayerWidth, uint32 srcLayerHeight, const rasterRowSize& srcRowSize,
    void *dstTexels, uint32 dstSurfWidth, uint32 dstSurfHeight, const rasterRowSize& dstRowSize,
    uint32 src_pos_x, uint32 src_pos_y,
    uint32 dst_pos_x, uint32 dst_pos_y,
    uint32 srcDepth, ePaletteType srcPaletteType,
    uint32 dstDepth, ePaletteType dstPaletteType,
    uint32 paletteSize
)
{
    if ( dst_pos_x < dstSurfWidth && dst_pos_y < dstSurfHeight )
    {
        rasterRow dstRow = getTexelDataRow( dstTexels, dstRowSize, dst_pos_y );

        if ( src_pos_x < srcLayerWidth && src_pos_y < srcLayerHeight )
        {
            // We can copy the palette index.
            constRasterRow srcRow = getConstTexelDataRow( srcTexels, srcRowSize, src_pos_y );

            copyPaletteItemGeneric(
                srcRow, dstRow,
                src_pos_x, srcDepth, srcPaletteType,
                dst_pos_x, dstDepth, dstPaletteType,
                paletteSize
            );
        }
        else
        {
            // Clear if we could not get a source index.
            setpaletteindex( dstRow, dst_pos_x, dstDepth, dstPaletteType, 0 );
        }
    }
}

typedef dxt1_block <endian::big_endian> gc_dxt1_block;

inline void ConvertGCMipmapToRasterFormat(
    Interface *engineInterface,
    uint32 mipWidth, uint32 mipHeight, uint32 layerWidth, uint32 layerHeight, void *texelSource, uint32 dataSize,
    eGCNativeTextureFormat internalFormat, eGCPixelFormat palettePixelFormat,
    ePaletteType paletteType, const void *paletteData, uint32 paletteSize,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder,
    eCompressionType dstCompressionType,
    uint32& dstSurfWidthOut, uint32& dstSurfHeightOut,
    void*& dstTexelDataOut, uint32& dstDataSizeOut
)
{
    // Check if we are a raw format that can be converted on a per-sample basis.
    if ( isGVRNativeFormatRawSample( internalFormat ) )
    {
        uint32 srcDepth = getGCInternalFormatDepth( internalFormat );

        // Get swizzling properties of the format we want to decode from.
        uint32 clusterWidth = 1;
        uint32 clusterHeight = 1;

        uint32 clusterCount = 1;

        bool isClusterFormat =
            getGVRNativeFormatClusterDimensions(
                srcDepth,
                clusterWidth, clusterHeight,
                clusterCount
            );

        if ( !isClusterFormat )
        {
            throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_CLUSTERPROPSFAIL" );
        }

        // Allocate the destination buffer.
        rasterRowSize dstRowSize = getRasterDataRowSize( layerWidth, dstDepth, dstRowAlignment );

        uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, layerHeight );

        void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

        try
        {
            rasterRowSize srcRowSize = getGCRasterDataRowSize( mipWidth, srcDepth );

            if ( internalFormat == GVRFMT_PAL_4BIT || internalFormat == GVRFMT_PAL_8BIT )
            {
                assert( paletteType != PALETTE_NONE );

                // Swizzle/unswizzle the palette items.
                // This is pretty simple, anyway.
                GCProcessRandomAccessTileSurface(
                    mipWidth, mipHeight,
                    clusterWidth, clusterHeight, 1,
                    true,
                    [&]( uint32 dst_pos_x, uint32 dst_pos_y, uint32 src_pos_x, uint32 src_pos_y, uint32 cluster_index )
                {
                    // We do this to not overcomplicate the code.
                    copyPaletteIndexAcrossSurfaces(
                        texelSource, mipWidth, mipHeight, srcRowSize,
                        dstTexels, layerWidth, layerHeight, dstRowSize,
                        src_pos_x, src_pos_y,
                        dst_pos_x, dst_pos_y,
                        srcDepth, paletteType,
                        dstDepth, paletteType,
                        paletteSize
                    );
                });
            }
            else
            {
                // Decide whether we have to swizzle the format.
                bool isFormatSwizzled = isGVRNativeFormatSwizzled( internalFormat );

                // Set up the GC color dispatcher.
                gcColorDispatch srcDispatch(
                    internalFormat, GVRPIX_NO_PALETTE,
                    COLOR_RGBA,
                    0, PALETTE_NONE, nullptr, 0
                );

                // We need a destination dispatcher.
                colorModelDispatcher dstDispatch(
                    dstRasterFormat, dstColorOrder, dstDepth,
                    nullptr, 0, PALETTE_NONE
                );

                // If we store things in multi-clustered format, we promise the runtime
                // that we can store the same amount of data in a more spread-out way, hence
                // the buffer size if supposed to stay the same (IMPORTANT).
                uint32 clusterGCItemWidth = mipWidth * clusterCount;
                uint32 clusterGCItemHeight = mipHeight;

                GCProcessRandomAccessTileSurface(
                    mipWidth, mipHeight,
                    clusterWidth, clusterHeight, clusterCount,
                    isFormatSwizzled,
                    [&]( uint32 dst_pos_x, uint32 dst_pos_y, uint32 src_pos_x, uint32 src_pos_y, uint32 cluster_index )
                {
                    // We are unswizzling.
                    if ( dst_pos_x < layerWidth && dst_pos_y < layerHeight )
                    {
                        // Just do a naive movement for now.
                        rasterRow dstRow = getTexelDataRow( dstTexels, dstRowSize, dst_pos_y );

                        abstractColorItem colorItem;

                        bool hasColor = false;

                        if ( src_pos_x < clusterGCItemWidth && src_pos_y < clusterGCItemHeight )
                        {
                            constRasterRow srcRow = getConstTexelDataRow( texelSource, srcRowSize, src_pos_y );

                            readGCNativeColor(
                                srcRow, srcDispatch, src_pos_x,
                                cluster_index,
                                [&]( abstractColorItem& colorItem )
                                {
                                    // We want to update the color with green and blue.
                                    dstDispatch.getColor( dstRow, dst_pos_x, colorItem );

                                    assert( colorItem.model == COLORMODEL_RGBA );
                                }, colorItem
                            );

                            hasColor = true;
                        }

                        if ( !hasColor )
                        {
                            // If we could not get a valid color, we set it to cleared state.
                            dstDispatch.setClearedColor( colorItem );
                        }

                        // Put the destination color.
                        dstDispatch.setColor( dstRow, dst_pos_x, colorItem );
                    }
                });
            }
        }
        catch( ... )
        {
            engineInterface->PixelFree( dstTexels );

            throw;
        }

        // Return the buffer.
        // Since we return raw texels, we must return layer dimensions.
        dstSurfWidthOut = layerWidth;
        dstSurfHeightOut = layerHeight;
        dstTexelDataOut = dstTexels;
        dstDataSizeOut = dstDataSize;
    }
    else if ( internalFormat == GVRFMT_CMP )
    {
        // Convert the GC DXT1 layer directly into a framework-friendly DXT1 layer!

        // gc_dxt1_block is Gamecube DXT1 struct.
        // dxt1_block is framework-friendly DXT1 struct.

        const uint32 dxt_blockPixelWidth = 4u;
        const uint32 dxt_blockPixelHeight = 4u;

        // Calculate the DXT dimensions that are actually used by us.
        uint32 plainSurfWidth = ALIGN_SIZE( layerWidth, dxt_blockPixelWidth );
        uint32 plainSurfHeight = ALIGN_SIZE( layerHeight, dxt_blockPixelHeight );

        // Allocate a destination buffer.
        uint32 dxtBlocksWidth = ( plainSurfWidth / dxt_blockPixelWidth );
        uint32 dxtBlocksHeight = ( plainSurfHeight / dxt_blockPixelHeight );

        uint32 dxtBlockCount = ( dxtBlocksWidth * dxtBlocksHeight );

        uint32 dxtDataSize = ( dxtBlockCount * getDXTBlockSize( 1 ) );

        void *dstTexels = engineInterface->PixelAllocate( dxtDataSize );

        typedef dxt1_block <endian::little_endian> frm_dxt1_block;

        try
        {
            // We want to iterate through all allocated GC DXT1 blocks
            // and process the valid ones.
            uint32 gcBlocksWidth = ( mipWidth / dxt_blockPixelWidth );
            uint32 gcBlocksHeight = ( mipHeight / dxt_blockPixelHeight );

            const gc_dxt1_block *gcBlocks = (const gc_dxt1_block*)texelSource;

            frm_dxt1_block *dstBlocks = (frm_dxt1_block*)dstTexels;

            memcodec::permutationUtilities::ProcessTextureLayerPackedTiles(
                gcBlocksWidth, gcBlocksHeight,
                2, 2,
                1,
                [&]( uint32 dst_pos_x, uint32 dst_pos_y, uint32 src_pos_x, uint32 src_pos_y, uint32 cluster_index )
            {
                // We unswizzle.
                if ( dst_pos_x < dxtBlocksWidth && dst_pos_y < dxtBlocksHeight )
                {
                    // Get the destination block ptr.
                    uint32 dstBlockIndex = ( dst_pos_x + dst_pos_y * dxtBlocksWidth );

                    frm_dxt1_block *dstBlock = ( dstBlocks + dstBlockIndex );

                    // Attempt to get the source block.
                    if ( src_pos_x < gcBlocksWidth && src_pos_y < gcBlocksHeight )
                    {
                        uint32 srcBlockIndex = ( src_pos_x + src_pos_y * gcBlocksWidth );

                        const gc_dxt1_block *srcBlock = ( gcBlocks + srcBlockIndex );

                        // Copy things over properly.
                        dstBlock->col0 = srcBlock->col0;
                        dstBlock->col1 = srcBlock->col1;

                        // The index list needs special handling.
                        uint32 dstIndexList = 0;

                        DXTIndexListInverseCopy( dstIndexList, srcBlock->indexList, dxt_blockPixelWidth, dxt_blockPixelHeight );

                        dstBlock->indexList = dstIndexList;
                    }
                    else
                    {
                        // We sort of just clear our block.
                        rgb565 clearColor;
                        clearColor.val = 0;

                        dstBlock->col0 = clearColor;
                        dstBlock->col1 = clearColor;
                        dstBlock->indexList = 0;
                    }
                }
            });

            // We successfully converted!
        }
        catch( ... )
        {
            engineInterface->PixelFree( dstTexels );

            throw;
        }

        // Return the stuff.
        dstSurfWidthOut = plainSurfWidth;
        dstSurfHeightOut = plainSurfHeight;
        dstTexelDataOut = dstTexels;
        dstDataSizeOut = dxtDataSize;
    }
    else
    {
        throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_UNKINTERNFMT" );
    }
}

template <typename dispatchType>
AINLINE void writeGCNativeColor(
    rasterRow& dstRow, dispatchType& dstDispatch, uint32 dst_pos_x,
    uint32 cluster_index,
    const abstractColorItem& colorItem
)
{
    if ( dstDispatch.getNativeFormat() == GVRFMT_RGBA8888 )
    {
        uint8 r, g, b, a;
        colorItem2RGBA( colorItem, r, g, b, a );

        if ( cluster_index == 0 )
        {
            struct alpha_red
            {
                uint8 alpha, red;
            };

            alpha_red ar;
            ar.alpha = a;
            ar.red = r;

            dstRow.writeStructItem( ar, dst_pos_x );
        }
        else if ( cluster_index == 1 )
        {
            struct green_blue
            {
                uint8 green, blue;
            };

            green_blue gb;
            gb.green = g;
            gb.blue = b;

            dstRow.writeStructItem( gb, dst_pos_x );
        }
        else
        {
            assert( 0 );
        }
    }
    else
    {
        // We are a simple color, so no problem.
        dstDispatch.setColor( dstRow, dst_pos_x, colorItem );
    }
}

inline void TranscodeIntoNativeGCLayer(
    Interface *engineInterface,
    uint32 mipWidth, uint32 mipHeight, uint32 layerWidth, uint32 layerHeight, const void *srcTexels, uint32 srcDataSize,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder,
    ePaletteType srcPaletteType, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eGCNativeTextureFormat internalFormat, eGCPixelFormat palettePixelFormat,
    uint32& dstSurfWidthOut, uint32& dstSurfHeightOut,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    // We once again decide logic by the native format.
    // One could decide by source format, but I think that the native format is what we care most
    // about. Also, we are most flexible about the framework formats.
    // So going by native format is really good.
    if ( isGVRNativeFormatRawSample( internalFormat ) )
    {
        assert( srcCompressionType == RWCOMPRESS_NONE );

        // Get properties about the native surface for allocation
        // and processing.
        uint32 nativeDepth = getGCInternalFormatDepth( internalFormat );

        uint32 clusterWidth, clusterHeight;
        uint32 clusterCount;

        bool gotClusterProps =
            getGVRNativeFormatClusterDimensions(
                nativeDepth,
                clusterWidth, clusterHeight,
                clusterCount
           );

        if ( !gotClusterProps )
        {
            throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_CLUSTERPROPSFAIL" );
        }

        // Now that we have all the properties we need, we can allocate
        // the native surface.
        uint32 gcSurfWidth = ALIGN_SIZE( layerWidth, clusterWidth );
        uint32 gcSurfHeight = ALIGN_SIZE( layerHeight, clusterHeight );

        rasterRowSize gcRowSize = getGCRasterDataRowSize( gcSurfWidth, nativeDepth );

        uint32 gcDataSize = getRasterDataSizeByRowSize( gcRowSize, gcSurfHeight );

        void *gcTexels = engineInterface->PixelAllocate( gcDataSize );

        try
        {
            // Process the layer.
            rasterRowSize srcRowSize = getRasterDataRowSize( mipWidth, srcDepth, srcRowAlignment );

            if ( internalFormat == GVRFMT_PAL_4BIT || internalFormat == GVRFMT_PAL_8BIT )
            {
                assert( srcPaletteType != PALETTE_NONE );

                ePaletteType dstPaletteType = getPaletteTypeFromGCNativeFormat( internalFormat );

                assert( dstPaletteType != PALETTE_NONE );

                // Transform palette indice.
                GCProcessRandomAccessTileSurface(
                    gcSurfWidth, gcSurfHeight,
                    clusterWidth, clusterHeight,
                    1,
                    true,
                    [&]( uint32 src_pos_x, uint32 src_pos_y, uint32 dst_pos_x, uint32 dst_pos_y, uint32 cluster_index )
                {
                    // We are swizzling.
                    copyPaletteIndexAcrossSurfaces(
                        srcTexels, layerWidth, layerHeight, srcRowSize,
                        gcTexels, gcSurfWidth, gcSurfHeight, gcRowSize,
                        src_pos_x, src_pos_y,
                        dst_pos_x, dst_pos_y,
                        srcDepth, srcPaletteType,
                        nativeDepth, dstPaletteType,
                        srcPaletteSize
                    );
                });
            }
            else
            {
                assert( srcPaletteType == PALETTE_NONE );

                bool isSurfaceSwizzled = isGVRNativeFormatSwizzled( internalFormat );

                // Create the source and destination color model dispatchers.
                colorModelDispatcher srcDispatch(
                    srcRasterFormat, srcColorOrder, srcDepth,
                    nullptr, 0, PALETTE_NONE
                );

                gcColorDispatch dstDispatch(
                    internalFormat, GVRPIX_NO_PALETTE,
                    COLOR_RGBA,
                    0, PALETTE_NONE, nullptr, 0
                );

                // Set up valid clustered dimensions.
                uint32 clusterGCItemWidth = gcSurfWidth * clusterCount;
                uint32 clusterGCItemHeight = gcSurfHeight;

                // Process the color data.
                GCProcessRandomAccessTileSurface(
                    gcSurfWidth, gcSurfHeight,
                    clusterWidth, clusterHeight,
                    clusterCount,
                    isSurfaceSwizzled,
                    [&]( uint32 src_pos_x, uint32 src_pos_y, uint32 dst_pos_x, uint32 dst_pos_y, uint32 cluster_index )
                {
                    // We are swizzling.
                    if ( dst_pos_x < clusterGCItemWidth && dst_pos_y < clusterGCItemHeight )
                    {
                        // Pretty dangerous to intermix cluster dimensions with regular dimensions.
                        // But it works here because the y dimension is left untouched.
                        rasterRow dstRow = getTexelDataRow( gcTexels, gcRowSize, dst_pos_y );

                        // Get the color to put into the row.
                        abstractColorItem colorItem;
                        bool gotColor = false;

                        if ( src_pos_x < layerWidth && src_pos_y < layerHeight )
                        {
                            constRasterRow srcRow = getConstTexelDataRow( srcTexels, srcRowSize, src_pos_y );

                            srcDispatch.getColor( srcRow, src_pos_x, colorItem );

                            gotColor = true;
                        }

                        // If we could not get a color, we just put a cleared one.
                        if ( !gotColor )
                        {
                            srcDispatch.setClearedColor( colorItem );
                        }

                        // Write properly into the destination.
                        // The important tidbit is that we need to double-cluster the RGBA8888 format.

                        writeGCNativeColor(
                            dstRow, dstDispatch, dst_pos_x,
                            cluster_index,
                            colorItem
                        );
                    }
                });
            }
        }
        catch( ... )
        {
            engineInterface->PixelFree( gcTexels );

            throw;
        }

        // Give the data to the runtime.
        dstSurfWidthOut = gcSurfWidth;
        dstSurfHeightOut = gcSurfHeight;
        dstTexelsOut = gcTexels;
        dstDataSizeOut = gcDataSize;
    }
    else if ( internalFormat == GVRFMT_CMP )
    {
        // We can only receive DXT1 compressed surfaces anyway.
        assert( srcCompressionType == RWCOMPRESS_DXT1 );

        const uint32 dxt_blockPixelWidth = 4u;
        const uint32 dxt_blockPixelHeight = 4u;

        // Allocate the destination native DXT1 surface.
        uint32 gcSurfWidth = ALIGN_SIZE( layerWidth, dxt_blockPixelWidth * 2 );
        uint32 gcSurfHeight = ALIGN_SIZE( layerHeight, dxt_blockPixelHeight * 2 );

        uint32 gcDXTBlocksWidth = ( gcSurfWidth / dxt_blockPixelWidth );
        uint32 gcDXTBlocksHeight = ( gcSurfHeight / dxt_blockPixelHeight );

        uint32 gcDXTBlockCount = ( gcDXTBlocksWidth * gcDXTBlocksHeight );

        uint32 gcDataSize = ( gcDXTBlockCount * sizeof( gc_dxt1_block ) );

        void *gcTexels = engineInterface->PixelAllocate( gcDataSize );

        typedef dxt1_block <endian::little_endian> frm_dxt1_block;

        try
        {
            // Get properties about the source DXT1 layer.
            uint32 srcDXTBlocksWidth = ( mipWidth / dxt_blockPixelWidth );
            uint32 srcDXTBlocksHeight = ( mipHeight / dxt_blockPixelHeight );

            gc_dxt1_block *dstBlocks = (gc_dxt1_block*)gcTexels;

            // Process the framework DXT1 structs into native GC DXT1 structs.
            memcodec::permutationUtilities::ProcessTextureLayerPackedTiles(
                gcDXTBlocksWidth, gcDXTBlocksHeight,
                2, 2,
                1,
                [&]( uint32 src_pos_x, uint32 src_pos_y, uint32 dst_pos_x, uint32 dst_pos_y, uint32 cluster_index )
            {
                // We are swizzling.
                if ( dst_pos_x < gcDXTBlocksWidth && dst_pos_y < gcDXTBlocksHeight )
                {
                    // Get the destination block ptr.
                    uint32 dstBlockIndex = ( dst_pos_x + gcDXTBlocksWidth * dst_pos_y );

                    gc_dxt1_block *dstBlock = ( dstBlocks + dstBlockIndex );

                    if ( src_pos_x < srcDXTBlocksWidth && src_pos_y < srcDXTBlocksHeight )
                    {
                        uint32 srcBlockIndex = ( src_pos_x + srcDXTBlocksWidth * src_pos_y );

                        const frm_dxt1_block *srcBlock = ( (const frm_dxt1_block*)srcTexels + srcBlockIndex );

                        dstBlock->col0 = srcBlock->col0;
                        dstBlock->col1 = srcBlock->col1;

                        // Indexlist needs special handling.
                        uint32 dstIndexList = 0;

                        DXTIndexListInverseCopy( dstIndexList, srcBlock->indexList, dxt_blockPixelWidth, dxt_blockPixelHeight );

                        dstBlock->indexList = dstIndexList;
                    }
                    else
                    {
                        // Just clear the block.
                        rgb565 clearColor;
                        clearColor.val = 0;

                        dstBlock->col0 = clearColor;
                        dstBlock->col1 = clearColor;
                        dstBlock->indexList = 0;
                    }
                }
            });
        }
        catch( ... )
        {
            engineInterface->PixelFree( gcTexels );

            throw;
        }

        // Return stuff to the runtime.
        dstSurfWidthOut = gcSurfWidth;
        dstSurfHeightOut = gcSurfHeight;
        dstTexelsOut = gcTexels;
        dstDataSizeOut = gcDataSize;
    }
    else
    {
        throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_UNKSWIZZLEFMT" );
    }
}

inline void TransformRawGCMipmapLayer(
    Interface *engineInterface,
    void *srcTexels, uint32 layerWidth, uint32 layerHeight,
    eGCNativeTextureFormat srcInternalFormat, eGCNativeTextureFormat dstInternalFormat,
    uint32 srcMipWidth, uint32 srcMipHeight,
    uint32 srcDepth, uint32 dstDepth,
    uint32 srcClusterWidth, uint32 srcClusterHeight, uint32 srcClusterCount,
    uint32 dstClusterWidth, uint32 dstClusterHeight, uint32 dstClusterCount,
    bool isSrcFormatSwizzled, bool isDstFormatSwizzled,
    uint32 paletteSize,
    uint32& dstMipWidthOut, uint32& dstMipHeightOut,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    // TODO: if it becomes cool enough, optimize this algorithm so that it can
    // transform texels into the source buffer under certain circumstances directly,
    // thus avoiding unneccessary memory allocation.

    rasterRowSize srcRowSize = getGCRasterDataRowSize( srcMipWidth, srcDepth );

    // Allocate the destination surface, if necessary.
    uint32 dstMipWidth = ALIGN_SIZE( layerWidth, dstClusterWidth );
    uint32 dstMipHeight = ALIGN_SIZE( layerHeight, dstClusterHeight );

    rasterRowSize dstRowSize = getGCRasterDataRowSize( dstMipWidth, dstDepth );

    uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, dstMipHeight );

    void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

    // Kind of depends if we are palette or color data.
    // We must not change contracts from palette to color or other way round.

    try
    {
        for ( uint32 layerY = 0; layerY < layerHeight; layerY++ )
        {
            for ( uint32 layerX = 0; layerX < layerWidth; layerX++ )
            {
                if ( dstInternalFormat == GVRFMT_PAL_4BIT || dstInternalFormat == GVRFMT_PAL_8BIT )
                {
                    assert( srcInternalFormat == GVRFMT_PAL_4BIT || srcInternalFormat == GVRFMT_PAL_8BIT );

                    // This is a pretty simple depth item one-cluster copy operation!
                    assert( srcClusterCount == 1 && dstClusterCount == 1 );

                    const ePaletteType srcPaletteType = getPaletteTypeFromGCNativeFormat( srcInternalFormat );

                    assert( srcPaletteType != PALETTE_NONE );

                    const ePaletteType dstPaletteType = getPaletteTypeFromGCNativeFormat( dstInternalFormat );

                    assert( dstPaletteType != PALETTE_NONE );
                    assert( srcPaletteType == dstPaletteType );

                    // Get the source coordinate.
                    uint32 src_tiled_x, src_tiled_y;

                    GCGetTiledCoordFromLinear(
                        layerX, layerY,
                        srcMipWidth, srcMipHeight,
                        srcClusterWidth, srcClusterHeight,
                        isSrcFormatSwizzled,
                        src_tiled_x, src_tiled_y
                    );

                    // Get the destination coordinate.
                    uint32 dst_tiled_x, dst_tiled_y;

                    GCGetTiledCoordFromLinear(
                        layerX, layerY,
                        dstMipWidth, dstMipHeight,
                        dstClusterWidth, dstClusterHeight,
                        isDstFormatSwizzled,
                        dst_tiled_x, dst_tiled_y
                    );

                    // Copy the item.
                    copyPaletteIndexAcrossSurfaces(
                        srcTexels, srcMipWidth, srcMipHeight, srcRowSize,
                        dstTexels, dstMipWidth, dstMipHeight, dstRowSize,
                        src_tiled_x, src_tiled_y,
                        dst_tiled_x, dst_tiled_y,
                        srcDepth, srcPaletteType,
                        dstDepth, dstPaletteType,
                        paletteSize
                    );
                }
                else
                {
                    assert( srcInternalFormat != GVRFMT_PAL_4BIT && srcInternalFormat != GVRFMT_PAL_8BIT );

                    gcColorDispatch srcDispatch(
                        srcInternalFormat, GVRPIX_NO_PALETTE, COLOR_RGBA,
                        0, PALETTE_NONE, nullptr, 0
                    );

                    gcColorDispatch dstDispatch(
                        dstInternalFormat, GVRPIX_NO_PALETTE, COLOR_RGBA,
                        0, PALETTE_NONE, nullptr, 0
                    );

                    // Get the color from the source.
                    abstractColorItem colorItem;
                    colorItem.setClearedColor( COLORMODEL_RGBA );   // for safety we start with a cleared color.

                    GCProcessTiledCoordsFromLinear(
                        layerX, layerY, srcMipWidth, srcMipHeight,
                        srcClusterWidth, srcClusterHeight, srcClusterCount,
                        isSrcFormatSwizzled,
                        [&]( uint32 tiled_x, uint32 tiled_y, uint32 cluster_index )
                    {
                        // Safety is a big concern of mine.
                        if ( tiled_x < srcMipWidth * srcClusterCount && tiled_y < srcMipHeight )
                        {
                            constRasterRow srcRow = getConstTexelDataRow( srcTexels, srcRowSize, tiled_y );

                            readGCNativeColor(
                                srcRow, srcDispatch, tiled_x,
                                cluster_index,
                                []( abstractColorItem& colorItem ){},   // nothing to do here.
                                colorItem
                            );
                        }
                    });

                    // Now the source color should be properly initialized.
                    // We should put it into the destination slot.

                    GCProcessTiledCoordsFromLinear(
                        layerX, layerY, dstMipWidth, dstMipHeight,
                        dstClusterWidth, dstClusterHeight, dstClusterCount,
                        isDstFormatSwizzled,
                        [&]( uint32 tiled_x, uint32 tiled_y, uint32 cluster_index )
                    {
                        if ( tiled_x < dstMipWidth * dstClusterCount && tiled_y < dstMipHeight )
                        {
                            rasterRow dstRow = getTexelDataRow( dstTexels, dstRowSize, tiled_y );

                            // Put the color, I guess.
                            // Or what we want to put from it anyway.
                            writeGCNativeColor(
                                dstRow, dstDispatch, tiled_x,
                                cluster_index, colorItem
                            );
                        }
                    });
                }
            }
        }
    }
    catch( ... )
    {
        // Since an error occured we wont be managing our destination surface anymore.
        // So we have to free its memory.
        engineInterface->PixelFree( dstTexels );

        throw;
    }

    // Return important stuff to the runtime.
    dstMipWidthOut = dstMipWidth;
    dstMipHeightOut = dstMipHeight;
    dstTexelsOut = dstTexels;
    dstDataSizeOut = dstDataSize;
}

inline void* GetGCPaletteData(
    Interface *engineInterface,
    const void *paletteData, uint32 paletteSize,
    eGCPixelFormat palettePixelFormat,
    eRasterFormat dstRasterFormat, eColorOrdering dstColorOrder
)
{
    assert( palettePixelFormat != GVRPIX_NO_PALETTE );

    // We have to convert the colors from GC format to framework format.
    // In the future we might think about directly acquiring those colors, but that is for later.
    // After all, it is important to define endian-ness even for colors.

    uint32 dstPalRasterDepth = Bitmap::getRasterFormatDepth( dstRasterFormat );

    uint32 dstPalDataSize = getPaletteDataSize( paletteSize, dstPalRasterDepth );

    void *dstPaletteData = engineInterface->PixelAllocate( dstPalDataSize );

    try
    {
        // TODO: make a distinction between serialized gc format identifiers and framework
        // ones to have a more secure code-base.

        eGCNativeTextureFormat internalFormat = GVRFMT_RGB5A3;
        eColorOrdering colorOrder = COLOR_RGBA;

        bool couldConvertFormat = getGVRNativeFormatFromPaletteFormat( palettePixelFormat, colorOrder, internalFormat );

        assert( couldConvertFormat == true );

        // Convert the palette entries.
        gcColorDispatch srcDispatch(
            internalFormat, GVRPIX_NO_PALETTE,  // we put raw colors, so no palette.
            colorOrder,
            0, PALETTE_NONE, nullptr, 0     // those parameters are only required if you want to read palette indices.
        );

        colorModelDispatcher dstDispatch(
            dstRasterFormat, dstColorOrder, dstPalRasterDepth,
            nullptr, 0, PALETTE_NONE
        );

        rasterRow palrow = dstPaletteData;

        for ( uint32 index = 0; index < paletteSize; index++ )
        {
            abstractColorItem colorItem;

            srcDispatch.getColor( paletteData, index, colorItem );

            dstDispatch.setColor( palrow, index, colorItem );
        }

        // Success!
    }
    catch( ... )
    {
        engineInterface->PixelFree( dstPaletteData );

        throw;
    }

    // Return the newly generated palette.
    return dstPaletteData;
}

inline void* TranscodeGCPaletteData(
    Interface *engineInterface,
    const void *paletteData, uint32 paletteSize,
    eRasterFormat srcRasterFormat, eColorOrdering srcColorOrder,
    eGCPixelFormat palettePixelFormat
)
{
    assert( palettePixelFormat != GVRPIX_NO_PALETTE );
    assert( paletteSize != 0 );

    uint32 srcPalRasterDepth = Bitmap::getRasterFormatDepth( srcRasterFormat );

    // Prepare the color dispatchers.
    colorModelDispatcher srcDispatch(
        srcRasterFormat, srcColorOrder, srcPalRasterDepth,
        nullptr, 0, PALETTE_NONE
    );

    eGCNativeTextureFormat palInternalFormat;
    eColorOrdering colorOrder;

    bool couldGetFormat = getGVRNativeFormatFromPaletteFormat( palettePixelFormat, colorOrder, palInternalFormat );

    if ( !couldGetFormat )
    {
        throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_MAPPALTOINTERNFMT" );
    }

    gcColorDispatch dstDispatch(
        palInternalFormat, GVRPIX_NO_PALETTE,   // we put raw colors, so no palette.
        colorOrder,
        0, PALETTE_NONE, nullptr, 0
    );

    // We want to put our framework color format into the Gamecube color format.
    uint32 dstPalRasterDepth = getGVRPixelFormatDepth( palettePixelFormat );

    uint32 dstPalDataSize = getPaletteDataSize( paletteSize, dstPalRasterDepth );

    void *dstPaletteData = engineInterface->PixelAllocate( dstPalDataSize );

    try
    {
        rasterRow palrow = dstPaletteData;

        // Process the palette colors.
        for ( uint32 n = 0; n < paletteSize; n++ )
        {
            abstractColorItem colorItem;

            srcDispatch.getColor( paletteData, n, colorItem );

            dstDispatch.setColor( palrow, n, colorItem );
        }

        // Yay!
    }
    catch( ... )
    {
        engineInterface->PixelFree( dstPaletteData );

        throw;
    }

    // Return the stuff.
    return dstPaletteData;
}

inline void TransformGCPaletteData(
    void *paletteData, uint32 paletteSize,
    eGCPixelFormat srcPalettePixelFormat, eGCPixelFormat dstPalettePixelFormat
)
{
    // Luckily this is a simple linear color conversion.
    eColorOrdering srcPalColorOrder, dstPalColorOrder;

    eGCNativeTextureFormat srcPalInternalFormat;
    eGCNativeTextureFormat dstPalInternalFormat;

    bool gotSrcPalInternalFormat = getGVRNativeFormatFromPaletteFormat( srcPalettePixelFormat, srcPalColorOrder, srcPalInternalFormat );

    // Those requests should never fail. The error checking is done for correctness sake, but since
    // the Gamecube is a pretty closed-down architecture we should be able to track down all
    // error cases easily.

    if ( !gotSrcPalInternalFormat )
    {
        throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_MAPSRCPALTOINTERNFMT" );
    }

    bool gotDstPalInternalFormat = getGVRNativeFormatFromPaletteFormat( dstPalettePixelFormat, dstPalColorOrder, dstPalInternalFormat );

    if ( !gotDstPalInternalFormat )
    {
        throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_MAPDSTPALTOINTERNFMT" );
    }

    // Convert stuff. This is pretty simple because all GC palette colors are 16 bit depth.
    uint32 srcPalDepth = getGCInternalFormatDepth( srcPalInternalFormat );
    uint32 dstPalDepth = getGCInternalFormatDepth( dstPalInternalFormat );

    assert( srcPalDepth == 16 );
    assert( dstPalDepth == srcPalDepth );

    // Prepare the color dispatchers.
    gcColorDispatch srcDispatch(
        srcPalInternalFormat, GVRPIX_NO_PALETTE, srcPalColorOrder,
        0, PALETTE_NONE, nullptr, 0
    );

    gcColorDispatch dstDispatch(
        dstPalInternalFormat, GVRPIX_NO_PALETTE, dstPalColorOrder,
        0, PALETTE_NONE, nullptr, 0
    );

    rasterRow palrow = paletteData;

    for ( uint32 n = 0; n < paletteSize; n++ )
    {
        abstractColorItem colorItem;

        srcDispatch.getColor( paletteData, n, colorItem );

        dstDispatch.setColor( palrow, n, colorItem );
    }
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_GAMECUBE

#endif //_GAMECUBE_NATIVE_MIPMAP_TRANSFORMATION_
