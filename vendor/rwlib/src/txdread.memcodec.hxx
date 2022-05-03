/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.memcodec.hxx
*  PURPOSE:
*       General memory encoding routines (so called swizzling).
*       It is recommended to use this file if you need stable, proofed algorithms.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef RW_TEXTURE_MEMORY_ENCODING
#define RW_TEXTURE_MEMORY_ENCODING

// Optimized algorithms are frowned upon, because in general it is hard to proove their correctness.

namespace rw
{

namespace memcodec
{

// Common utilities for permutation providers.
namespace permutationUtilities
{
    template <typename processorType, typename callbackType>
    AINLINE void GenericProcessTiledCoordsFromLinear(
        uint32 linearX, uint32 linearY, uint32 surfWidth, uint32 surfHeight,
        uint32 clusterWidth, uint32 clusterHeight, uint32 clusterCount,
        const callbackType& cb
    )
    {
        uint32 cluster_inside_x = ( linearX % clusterWidth );
        uint32 cluster_inside_y = ( linearY % clusterHeight );

        uint32 cluster_col = ( linearX / clusterWidth );
        uint32 cluster_row = ( linearY / clusterHeight );

        uint32 alignedSurfWidth = ALIGN_SIZE( surfWidth, clusterWidth );
        uint32 alignedSurfHeight = ALIGN_SIZE( surfHeight, clusterHeight );

        processorType proc( alignedSurfWidth, alignedSurfHeight, clusterWidth, clusterHeight, clusterCount );

        for ( uint32 cluster_index = 0; cluster_index < clusterCount; cluster_index++ )
        {
            uint32 tiled_x, tiled_y;
            proc.Get(
                cluster_col, cluster_row,
                cluster_inside_x, cluster_inside_y, cluster_index,
                tiled_x, tiled_y
            );

            cb( tiled_x, tiled_y, cluster_index );
        }
    }

    // Unoptimized packed tile processor for placing 2D tiles linearly into a buffer, basically
    // improving lookup performance through cache-friendlyness.
    // Makes sense when sending 2D color data to a highly-parallel simple device like a GPU.
    struct packedTileProcessor
    {
        AINLINE packedTileProcessor( uint32 alignedSurfWidth, uint32 alignedSurfHeight, uint32 clusterWidth, uint32 clusterHeight, uint32 clusterCount )
        {
            this->clusterWidth = clusterWidth;

            this->globalClustersPerWidth = ( alignedSurfWidth / clusterWidth );
            this->globalClustersPerHeight = ( alignedSurfHeight / clusterHeight );

            uint32 globalClusterWidth = ( clusterWidth * clusterCount );
            uint32 globalClusterHeight = ( clusterHeight );

            this->globalClusterWidth = globalClusterWidth;
            this->globalClusterHeight = globalClusterHeight;

            this->clusteredSurfWidth = ( alignedSurfWidth * clusterCount );
            this->clusteredSurfHeight = ( alignedSurfHeight );

            // This is the size in indices of one tile.
            this->local_clusterIndexSize = ( clusterWidth * clusterHeight );
            this->globalClusterIndexSize = ( globalClusterWidth * globalClusterHeight );
        }

        AINLINE void Get(
            uint32 globalClusterX, uint32 globalClusterY,
            uint32 localClusterX, uint32 localClusterY, uint32 cluster_index,
            uint32& tiled_x, uint32& tiled_y
        )
        {
            // TODO: optimize this, so that it can be put into a one-time initializator.
            uint32 local_cluster_advance_index = ( localClusterX + localClusterY * this->clusterWidth );

            uint32 global_cluster_advance_index = ( globalClusterX + globalClusterY * this->globalClustersPerWidth ) * this->globalClusterIndexSize;

            uint32 cluster_advance_index = ( local_cluster_advance_index + global_cluster_advance_index );

            // Now the actual mutating part.
            uint32 per_cluster_advance_index = ( cluster_advance_index + this->local_clusterIndexSize * cluster_index );

            tiled_x = ( per_cluster_advance_index % this->clusteredSurfWidth );
            tiled_y = ( per_cluster_advance_index / this->clusteredSurfWidth );
        }

    private:
        uint32 clusterWidth;
        uint32 globalClusterWidth, globalClusterHeight;
        uint32 clusteredSurfWidth, clusteredSurfHeight;

        uint32 local_clusterIndexSize;
        uint32 globalClusterIndexSize;

        uint32 globalClustersPerWidth, globalClustersPerHeight;
    };

    // A way to get clustered tiled coordinates for formats.
    template <typename callbackType>
    AINLINE void ProcessPackedTiledCoordsFromLinear(
        uint32 linearX, uint32 linearY, uint32 surfWidth, uint32 surfHeight,
        uint32 clusterWidth, uint32 clusterHeight, uint32 clusterCount,
        const callbackType& cb
    )
    {
        GenericProcessTiledCoordsFromLinear <packedTileProcessor> (
            linearX, linearY, surfWidth, surfHeight,
            clusterWidth, clusterHeight, clusterCount,
            cb
        );
    }

    template <typename processorType>
    AINLINE void GenericGetTiledCoordFromLinear(
        uint32 linearX, uint32 linearY, uint32 surfWidth, uint32 surfHeight,
        uint32 clusterWidth, uint32 clusterHeight,  // clusterCount == 1
        uint32& dst_tiled_x, uint32& dst_tiled_y
    )
    {
        // By passing 1 as clusterCount we are sure that we get one coordinate.
        // The lambda has to be triggered, exactly one time.
        GenericProcessTiledCoordsFromLinear <processorType> (
            linearX, linearY, surfWidth, surfHeight,
            clusterWidth, clusterHeight, 1,
            [&]( uint32 tiled_x, uint32 tiled_y, uint32 cluster_index )
        {
            dst_tiled_x = tiled_x;
            dst_tiled_y = tiled_y;
        });
    }

    AINLINE void GetPackedTiledCoordFromLinear(
        uint32 linearX, uint32 linearY, uint32 surfWidth, uint32 surfHeight,
        uint32 clusterWidth, uint32 clusterHeight,  // clusterCount == 1
        uint32& dst_tiled_x, uint32& dst_tiled_y
    )
    {
        GenericGetTiledCoordFromLinear <packedTileProcessor> (
            linearX, linearY,
            surfWidth, surfHeight,
            clusterWidth, clusterHeight,
            dst_tiled_x, dst_tiled_y
        );
    }

    // Optimized implementation of a packed tile processor designed for the default function.
    struct optimizedPackedTileProcessor
    {
        AINLINE optimizedPackedTileProcessor( uint32 alignedSurfWidth, uint32 alignedSurfHeight, uint32 clusterWidth, uint32 clusterHeight, uint32 clusterCount )
        {
            this->packedData_xOff = 0;
            this->packedData_yOff = 0;

            this->packed_surfWidth = ( alignedSurfWidth * clusterCount );
        }

        AINLINE void Get(
            uint32 colX, uint32 colY,
            uint32 cluster_x, uint32 cluster_y, uint32 cluster_index,
            uint32& tile_x, uint32& tile_y
        )
        {
            // Return the current stuff.
            tile_x = packedData_xOff;
            tile_y = packedData_yOff;

            // Advance the packed pointer.
            // We depend on the way the function iterates over the texels.
            packedData_xOff++;

            if ( packedData_xOff >= packed_surfWidth )
            {
                packedData_xOff = 0;
                packedData_yOff++;
            }
        }

    private:
        uint32 packedData_xOff, packedData_yOff;
        uint32 packed_surfWidth;
    };

    // Very simple processor that just returns the linear coordinate, effectively not swizzling.
    struct linearTileProcessor
    {
        AINLINE linearTileProcessor( uint32 alignedSurfWidth, uint32 alignedSurfHeight, uint32 clusterWidth, uint32 clusterHeight, uint32 clusterCount )
        {
            this->localClusterWidth = clusterWidth;

            this->globalClusterWidth = ( clusterWidth * clusterCount );
            this->globalClusterHeight = ( clusterHeight );
        }

        AINLINE void Get(
            uint32 colX, uint32 colY,
            uint32 cluster_x, uint32 cluster_y, uint32 cluster_index,
            uint32& tiled_x, uint32& tiled_y
        )
        {
            tiled_x = ( colX * this->globalClusterWidth + cluster_x + this->localClusterWidth * cluster_index );
            tiled_y = ( colY * this->globalClusterHeight + cluster_y );
        }

    private:
        uint32 localClusterWidth;

        uint32 globalClusterWidth;
        uint32 globalClusterHeight;
    };

    // Main texture layer tile processing algorithm.
    template <typename processorType, typename callbackType>
    AINLINE void GenericProcessTileLayerPerCluster(
        uint32 surfWidth, uint32 surfHeight,
        uint32 clusterWidth, uint32 clusterHeight,
        uint32 clusterCount,
        const callbackType& cb
    )
    {
        uint32 alignedSurfWidth = ALIGN_SIZE( surfWidth, clusterWidth );
        uint32 alignedSurfHeight = ALIGN_SIZE( surfHeight, clusterHeight );

        uint32 cols_width = ( alignedSurfWidth / clusterWidth );
        uint32 cols_height = ( alignedSurfHeight / clusterHeight );

        processorType proc( alignedSurfWidth, alignedSurfHeight, clusterWidth, clusterHeight, clusterCount );

        for ( uint32 colY = 0; colY < cols_height; colY++ )
        {
            uint32 col_y_pixelOff = ( colY * clusterHeight );

            for ( uint32 colX = 0; colX < cols_width; colX++ )
            {
                uint32 col_x_pixelOff = ( colX * clusterWidth );

                for ( uint32 cluster_index = 0; cluster_index < clusterCount; cluster_index++ )
                {
                    for ( uint32 cluster_y = 0; cluster_y < clusterHeight; cluster_y++ )
                    {
                        uint32 perm_y_off = ( col_y_pixelOff + cluster_y );

                        for ( uint32 cluster_x = 0; cluster_x < clusterWidth; cluster_x++ )
                        {
                            uint32 perm_x_off = ( col_x_pixelOff + cluster_x );

                            uint32 tiled_x, tiled_y;
                            proc.Get(
                                colX, colY,
                                cluster_x, cluster_y, cluster_index,
                                tiled_x, tiled_y
                            );

                            // Notify the runtime.
                            cb( perm_x_off, perm_y_off, tiled_x, tiled_y, cluster_index );
                        }
                    }
                }
            }
        }
    }

    template <typename callbackType>
    AINLINE void ProcessTextureLayerPackedTiles(
        uint32 surfWidth, uint32 surfHeight,
        uint32 clusterWidth, uint32 clusterHeight,
        uint32 clusterCount,
        const callbackType& cb
    )
    {
        GenericProcessTileLayerPerCluster <optimizedPackedTileProcessor> (
            surfWidth, surfHeight,
            clusterWidth, clusterHeight,
            clusterCount,
            cb
        );
    }

    inline void TestTileEncoding(
        uint32 surfWidth, uint32 surfHeight,
        uint32 clusterWidth, uint32 clusterHeight, uint32 clusterCount
    )
    {
        memcodec::permutationUtilities::ProcessTextureLayerPackedTiles(
            surfWidth, surfHeight,
            clusterWidth, clusterHeight, clusterCount,
            [&]( uint32 layerX, uint32 layerY, uint32 tiled_x, uint32 tiled_y, uint32 cluster_index )
        {
            memcodec::permutationUtilities::ProcessPackedTiledCoordsFromLinear(
                layerX, layerY,
                surfWidth, surfHeight,
                clusterWidth, clusterHeight, clusterCount,
                [&]( uint32 try_tiled_x, uint32 try_tiled_y, uint32 try_cluster_index )
            {
                if ( try_cluster_index == cluster_index )
                {
                    assert( try_tiled_x == tiled_x );
                    assert( try_tiled_y == tiled_y );
                }
            });
        });
    }

    inline bool TranscodeTextureLayerTiles(
        Interface *engineInterface,
        uint32 surfWidth, uint32 surfHeight, const void *srcTexels,
        uint32 permDepth,
        uint32 srcRowAlignment, uint32 dstRowAlignment,
        uint32 clusterWidth, uint32 clusterHeight,
        bool doSwizzleOrUnswizzle,
        void*& dstTexelsOut, uint32& dstDataSizeOut
    )
    {
        // Allocate the destination array.
        rasterRowSize dstRowSize = getRasterDataRowSize( surfWidth, permDepth, dstRowAlignment );

        uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, surfHeight );

        void *dstTexels = engineInterface->PixelAllocateP( dstDataSize );

        if ( !dstTexels )
        {
            return false;
        }

        rasterRowSize srcRowSize = getRasterDataRowSize( surfWidth, permDepth, srcRowAlignment );

        try
        {
            ProcessTextureLayerPackedTiles(
                surfWidth, surfHeight,
                clusterWidth, clusterHeight, 1,
                [&]( uint32 perm_x_off, uint32 perm_y_off, uint32 packedData_xOff, uint32 packedData_yOff, uint32 cluster_index )
            {
                // Determine the pointer configuration.
                uint32 src_pos_x = 0;
                uint32 src_pos_y = 0;

                uint32 dst_pos_x = 0;
                uint32 dst_pos_y = 0;

                if ( doSwizzleOrUnswizzle )
                {
                    // We want to swizzle.
                    // This means the source is raw 2D plane, the destination is linear aligned binary buffer.
                    src_pos_x = perm_x_off;
                    src_pos_y = perm_y_off;

                    dst_pos_x = packedData_xOff;
                    dst_pos_y = packedData_yOff;
                }
                else
                {
                    // We want to unswizzle.
                    // This means the source is linear aligned binary buffer, dest is raw 2D plane.
                    src_pos_x = packedData_xOff;
                    src_pos_y = packedData_yOff;

                    dst_pos_x = perm_x_off;
                    dst_pos_y = perm_y_off;
                }

                if ( src_pos_x <= surfWidth && src_pos_y <= surfHeight &&
                     dst_pos_x <= surfWidth && dst_pos_y <= surfHeight )
                {
                    // Move data if in valid bounds.
                    constRasterRow srcRow = getConstTexelDataRow( srcTexels, srcRowSize, src_pos_y );
                    rasterRow dstRow = getTexelDataRow( dstTexels, dstRowSize, dst_pos_y );

                    dstRow.writeBitsFromRow(
                        srcRow,
                        src_pos_x * permDepth,
                        dst_pos_x * permDepth,
                        permDepth
                    );
                }
            });
        }
        catch( ... )
        {
            engineInterface->PixelFree( dstTexels );

            throw;
        }

        // Return the data to the runtime.
        dstTexelsOut = dstTexels;
        dstDataSizeOut = dstDataSize;
        return true;
    }
};

} // namespace memcodec

} // namespace rw

#endif //RW_TEXTURE_MEMORY_ENCODING
