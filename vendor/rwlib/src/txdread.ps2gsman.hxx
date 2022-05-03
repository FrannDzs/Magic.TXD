/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.d3d8.hxx
*  PURPOSE:     PlayStation 2 specific memory permutation structures.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_PLAYSTATION2_NATIVETEX_GSMAN_
#define _RENDERWARE_PLAYSTATION2_NATIVETEX_GSMAN_

// Include the main swizzling helpers.
#include "txdread.ps2shared.enc.hxx"

namespace rw
{

AINLINE bool doesRowHaveItem( size_t rowByteSize, size_t x, size_t depth )
{
    size_t rowBits = ( rowByteSize * 8u );

    size_t endOfX_bits = ( ( x + 1 ) * depth );

    return ( endOfX_bits <= rowBits );
}

struct columnPackedPermuter8x2
{
    AINLINE columnPackedPermuter8x2( eMemoryLayoutType rawType, eMemoryLayoutType encType )
    {
        this->rawProps = getMemoryLayoutTypeProperties( rawType );
        this->encProps = getMemoryLayoutTypeProperties( encType );

        colpack_width = ( rawProps.pixelWidthPerColumn / 8 );
        colpack_height = ( rawProps.pixelHeightPerColumn / 2 );

        this->depthpack = ( GetMemoryLayoutTypeGIFPackingDepth( encType ) / GetMemoryLayoutTypeGIFPackingDepth( rawType ) );

#ifdef _DEBUG
        assert( this->depthpack != 0 );
#endif //_DEBUG
    }

    AINLINE uint32 getInputWidth( void ) const
    {
        return 8 * depthpack;
    }

    AINLINE uint32 getInputHeight( void ) const
    {
        return 2;
    }

    AINLINE uint32 getOutputWidth( void ) const
    {
        return rawProps.pixelWidthPerColumn * 8 / encProps.pixelWidthPerColumn;
    }

    AINLINE uint32 getOutputHeight( void ) const
    {
        return rawProps.pixelHeightPerColumn;
    }

    AINLINE void permute( uint32 sx, uint32 sy, uint32& tx, uint32& ty, uint32 wcolidx, uint32 hcolidx ) const
    {
        uint32 packed_idx = ( sx % this->depthpack );
        uint32 pack_x = ( sx / this->depthpack );
        uint32 pack_y = sy;

        uint32 coliter_w = ( packed_idx / colpack_height );
        uint32 coliter_h = ( packed_idx % colpack_height );

        if ( ( coliter_h % 2 ) + ( hcolidx % 2 ) == 1 )
        {
            pack_x = ( pack_x + 4 ) % 8;
        }

        tx = ( coliter_w * 8 + pack_x );
        ty = ( coliter_h * 2 + pack_y );
    }

    ps2MemoryLayoutTypeProperties rawProps;
    ps2MemoryLayoutTypeProperties encProps;

    uint32 colpack_width;
    uint32 colpack_height;

    uint32 depthpack;
};

// New swizzling algorithm.
inline void* permutePS2Data(
    Interface *rwEngine,
    uint32 srcLayerWidth, uint32 srcLayerHeight, const void *srcTexels, uint32 srcDataSize,
    uint32 dstLayerWidth, uint32 dstLayerHeight,
    eMemoryLayoutType srcMemType, eMemoryLayoutType dstMemType,
    uint32 srcRowAlignment, uint32 dstRowAlignment,
    uint32& dstDataSizeOut
)
{
    uint32 srcDepth = GetMemoryLayoutTypeGIFPackingDepth( srcMemType );
    uint32 dstDepth = GetMemoryLayoutTypeGIFPackingDepth( dstMemType );

    rasterRowSize srcRowSize = getRasterDataRowSize( srcLayerWidth, srcDepth, srcRowAlignment );
    rasterRowSize dstRowSize = getRasterDataRowSize( dstLayerWidth, dstDepth, dstRowAlignment );

    uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, dstLayerHeight );

    void *dstTexels = rwEngine->PixelAllocate( dstDataSize );

    try
    {
        uint32 workWidth, workHeight;
        uint32 workDepth;

        uint32 transformed_dstLayerWidth, transformed_dstLayerHeight;

        auto permutation_logic = [&]( uint32 srcx, uint32 srcy, uint32 dstx, uint32 dsty )
        {
            if ( dstx < transformed_dstLayerWidth && dsty < transformed_dstLayerHeight )
            {
                rasterRow dstRow = getTexelDataRow( dstTexels, dstRowSize, dsty );

                bool isSrcContained = doesRasterContainItem( srcx, srcy, workDepth, srcRowSize, srcDataSize );

                if ( isSrcContained && srcx < workWidth && srcy < workHeight )
                {
                    constRasterRow srcRow = getConstTexelDataRow( srcTexels, srcRowSize, srcy );

                    dstRow.writeBitsFromRow( srcRow, (size_t)srcx * workDepth, (size_t)dstx * workDepth, workDepth );
                }
                else
                {
                    dstRow.setBits( false, (size_t)dstx * workDepth, workDepth );
                }
            }
        };

        if ( srcDepth > dstDepth )
        {
            workWidth = ( srcLayerWidth * srcDepth / dstDepth );
            workHeight = srcLayerHeight;
            workDepth = dstDepth;

            transformed_dstLayerWidth = dstLayerWidth;
            transformed_dstLayerHeight = dstLayerHeight;

            eir::permute2DBuffer(
                workWidth, workHeight, eir::nullPermuter <uint32> (), columnPackedPermuter8x2( dstMemType, srcMemType ),
                std::move( permutation_logic )
            );
        }
        else if ( dstDepth > srcDepth )
        {
            workWidth = srcLayerWidth;
            workHeight = srcLayerHeight;
            workDepth = srcDepth;

            transformed_dstLayerWidth = ( dstLayerWidth * dstDepth / srcDepth );
            transformed_dstLayerHeight = dstLayerHeight;

            eir::permute2DBuffer(
                transformed_dstLayerWidth, transformed_dstLayerHeight,
                columnPackedPermuter8x2( srcMemType, dstMemType ), eir::nullPermuter <uint32> (),
                std::move( permutation_logic )
            );
        }
        else
        {
            workWidth = srcLayerWidth;
            workHeight = srcLayerHeight;
            workDepth = srcDepth;

            transformed_dstLayerWidth = dstLayerWidth;
            transformed_dstLayerHeight = dstLayerHeight;

            eir::permute2DBuffer(
                workWidth, workHeight, eir::nullPermuter <uint32> (), eir::nullPermuter <uint32> (),
                std::move( permutation_logic )
            );
        }
    }
    catch( ... )
    {
        rwEngine->PixelFree( dstTexels );

        throw;
    }

    dstDataSizeOut = dstDataSize;
    return dstTexels;
}

} // namespace rw

#endif //_RENDERWARE_PLAYSTATION2_NATIVETEX_GSMAN_