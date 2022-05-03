/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwimaging.rasterread.hxx
*  PURPOSE:     Helpers to read raster data from byte streams.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RWIMAGING_RASTER_READ_HELPERS_
#define _RWIMAGING_RASTER_READ_HELPERS_

#include "pixelformat.hxx"

#include <sdk/DataUtil.h>

namespace rw
{

template <typename callbackType>
AINLINE void bufferedRasterTransformedIteration(
    Interface *rwEngine, const void *buf, uint32 width, uint32 height,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, uint32 srcPaletteCount,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType, uint32 dstPaletteCount,
    bool reverse_vertical,
    callbackType&& cb
)
{
    rwVector <char> rowbuf( eir::constr_with_alloc::DEFAULT, rwEngine );
    rwVector <char> rowbuf_transformed( eir::constr_with_alloc::DEFAULT, rwEngine );

    // Decide the rowsize that we need.
    rasterRowSize dstRowSize = getRasterDataRowSize( width, dstDepth, dstRowAlignment );
    size_t requiredRowByteSize = dstRowSize.estimateByteSize();

    rowbuf.Resize( requiredRowByteSize );
    
    if ( dstRowAlignment == 0 )
    {
        rowbuf_transformed.Resize( requiredRowByteSize );
    }

    memset( rowbuf.GetData(), 0, requiredRowByteSize );

    // Variables for packed reading.
    size_t packed_bitoff = 0;

    for ( uint32 rowi = 0; rowi < height; rowi++ )
    {
        uint32 actual_rowi = ( reverse_vertical ? height - 1 - rowi : rowi );

        moveTexels(
            buf, rowbuf.GetData(),
            0, actual_rowi,
            0, 0,
            width, 1,
            width, height,
            srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteCount,
            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstPaletteCount
        );

        if ( dstRowAlignment > 0 )
        {
            cb( rowbuf.GetData(), requiredRowByteSize );
        }
        else
        {
            // Try to fill in as much as possible into the row buffer.
            size_t avail_bits = dstRowSize.getBitSize();
            size_t srcbititer = 0;

            while ( srcbititer < avail_bits )
            {
                size_t num_writebits = std::min( requiredRowByteSize * 8u - packed_bitoff, avail_bits - srcbititer );

                eir::moveBits(  rowbuf_transformed.GetData(), rowbuf.GetData(), srcbititer, packed_bitoff, num_writebits );

                srcbititer += num_writebits;

                packed_bitoff += num_writebits;

                // We actually push the buffer now.
                if ( packed_bitoff == requiredRowByteSize * 8u )
                {
                    cb( rowbuf_transformed.GetData(), requiredRowByteSize );

                    // Reset writing again.
                    packed_bitoff = 0;
                }
            }
        }
    }
}

template <typename callbackType>
AINLINE void rasterReadRows(
    Interface *rwEngine, void *buf, uint32 height, uint32 depth, const rasterRowSize& rowSize, bool reverse_vertical, bool reverse_horizontal,
    callbackType&& cb
)
{
    rwVector <char> rowbuf( eir::constr_with_alloc::DEFAULT, rwEngine );

    uint32 requiredRowByteSize = rowSize.estimateByteSize();

    rowbuf.Resize( requiredRowByteSize );

    // Row writing runtime variables.
    size_t rowbitoff = 0;
    uint32 currowi = 0;
    size_t numrowbits = rowSize.getBitSize();

    // We just request bytes from the source, as much as we have to.
    // Then write all the rows.
    // We check in the end whether we received all of the data.
    uint32 curbytereq = 0;
    uint32 databytesize = getRasterDataSizeByRowSize( rowSize, height );

    while ( curbytereq < databytesize )
    {
        uint32 curreadcnt = std::min( requiredRowByteSize, databytesize - curbytereq );

        cb( rowbuf.GetData(), curreadcnt );

        // Now write the new bits into rows.
        size_t biti = 0;

        while ( biti < curreadcnt * 8u )
        {
            uint32 actual_rowi = ( reverse_vertical ? height - currowi - 1 : currowi );

            rasterRow row = rasterRow::from_rowsize <false> ( buf, rowSize, actual_rowi );

            size_t rowcanwritebits = numrowbits - rowbitoff;
            size_t numwritebits;

            if ( reverse_horizontal )
            {
                // We have to write in depth units in reverse order.
                numwritebits = std::min <size_t> ( rowcanwritebits, depth );

                row.writeBits( rowbuf.GetData(), numrowbits - depth - biti, numwritebits, biti );
            }
            else
            {
                // Finish writing the current row with the data we got.
                numwritebits = std::min( rowcanwritebits, curreadcnt * 8u - biti );

                row.writeBits( rowbuf.GetData(), rowbitoff, numwritebits, biti );
            }

            biti += numwritebits;
            rowbitoff += numwritebits;

            if ( rowbitoff == numrowbits )
            {
                currowi++;
                rowbitoff = 0;
            }
        }

        curbytereq += curreadcnt;
    }

#ifdef _DEBUG
    assert( currowi == height );
#endif //_DEBUG
}

} // namespace rw

#endif //_RWIMAGING_RASTER_READ_HELPERS_