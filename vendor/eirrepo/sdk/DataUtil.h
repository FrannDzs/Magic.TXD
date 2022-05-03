/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/DataUtil.h
*  PURPOSE:     Simple helpers for memory operations
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRREPO_DATA_UTILITIES_
#define _EIRREPO_DATA_UTILITIES_

#include <type_traits>
#include <algorithm>
#include <limits>

#include "MacroUtils.h"
#include "Exceptions.h"
#include "Endian.h"

// For uint8_t/uint16_t.
#include <stdint.h>

// For size_t.
#include <stddef.h>

// Some helpers.
namespace FSDataUtil
{

template <typename dataType>
static inline void copy_impl( const dataType *srcPtr, const dataType *srcPtrEnd, dataType *dstPtr ) noexcept
{
    static_assert( std::is_trivial <dataType>::value == true );

    while ( srcPtr != srcPtrEnd )
    {
        *dstPtr++ = *srcPtr++;
    }
}

template <typename dataType>
static inline void copy_backward_impl( const dataType *srcPtr, const dataType *srcPtrEnd, dataType *dstPtr ) noexcept
{
    static_assert( std::is_trivial <dataType>::value == true );

    while ( srcPtr != srcPtrEnd )
    {
        *(--dstPtr) = *(--srcPtrEnd);
    }
}

template <typename dataType>
static inline void assign_impl( dataType *startPtr, const dataType *endPtr, const dataType& value ) noexcept
{
    static_assert( std::is_trivial <dataType>::value == true );

    while ( endPtr > startPtr )
    {
        *startPtr++ = value;
    }
}

};

namespace eir
{

template <typename unsignedNumberType>
AINLINE unsignedNumberType ROTL( unsignedNumberType value, unsigned int rotBy )
{
    static constexpr unsigned int bitCount = sizeof(value) * 8;

    return ( value << rotBy ) | ( value >> ( bitCount - rotBy ) );
}
template <typename unsignedNumberType>
AINLINE unsignedNumberType ROTR( unsignedNumberType value, unsigned int rotBy )
{
    static constexpr unsigned int bitCount = sizeof(value) * 8;

    return ( value >> rotBy ) | ( value << ( bitCount - rotBy ) );
}

// For buffer overflow checking using depth-based writing.
AINLINE bool doesPackedBufferHaveItem( size_t dataByteSize, size_t itemIdx, size_t depth )
{
    size_t bits = ( dataByteSize * 8u );

    size_t endOfX_bits = ( ( itemIdx + 1 ) * depth );

    return ( endOfX_bits <= bits );
}

enum class eByteAddressingMode
{
    LEAST_SIGNIFICANT,
    MOST_SIGNIFICANT
};

// Supports movement of power-ot-two amount of bits.
template <typename exceptMan = eir::DefaultExceptionManager>
AINLINE void movePOTBits( void *dstBuf, const void *srcBuf, size_t numBits, size_t dstIdx, size_t srcIdx, eByteAddressingMode byteAddrMode )
{
    if ( numBits == 4 )
    {
        struct bits4_item
        {
            uint8_t prim : 4;
            uint8_t sec : 4;
        };

        size_t srcByteIdx = ( srcIdx / 2 );
        size_t srcByteSelect = ( srcIdx % 2 );

        size_t dstByteIdx = ( dstIdx / 2 );
        size_t dstByteSelect = ( dstIdx % 2 );

        const bits4_item *srcItem = (const bits4_item*)srcBuf + srcByteIdx;
        bits4_item *dstItem = (bits4_item*)dstBuf + dstByteIdx;

        if ( byteAddrMode == eByteAddressingMode::MOST_SIGNIFICANT )
        {
            if ( srcByteSelect == 0 && dstByteSelect == 0 )
            {
                dstItem->prim = srcItem->prim;
            }
            else if ( srcByteSelect == 0 )
            {
                dstItem->sec = srcItem->prim;
            }
            else if ( dstByteSelect == 0 )
            {
                dstItem->prim = srcItem->sec;
            }
            else
            {
                dstItem->sec = srcItem->sec;
            }
        }
        else if ( byteAddrMode == eByteAddressingMode::LEAST_SIGNIFICANT )
        {
            if ( srcByteSelect == 0 && dstByteSelect == 0 )
            {
                dstItem->sec = dstItem->sec;
            }
            else if ( dstByteSelect == 0 )
            {
                dstItem->sec = srcItem->prim;
            }
            else if ( srcByteSelect == 0 )
            {
                dstItem->prim = srcItem->sec;
            }
            else
            {
                dstItem->prim = srcItem->prim;
            }
        }
        else
        {
            exceptMan::throw_internal_error();
        }
    }
    else if ( numBits % 8 == 0 )
    {
        size_t itemSize = ( numBits / 8 );
        size_t srcCopyByteOff = ( srcIdx * itemSize );

        // Try to move power-of-two items.
        FSDataUtil::copy_impl(  (const char*)srcBuf + srcCopyByteOff, (const char*)srcBuf + srcCopyByteOff + itemSize, (char*)dstBuf + itemSize * dstIdx );
    }
    else
    {
        exceptMan::throw_internal_error();
    }
}

template <typename valueType, typename shiftCntType>
AINLINE void shiftBitsInDir( valueType& val, shiftCntType cnt, bool trueForwardFalseBackward ) noexcept
{
    // TODO: what about a different bit order?

    if ( trueForwardFalseBackward )
    {
        val <<= cnt;
    }
    else
    {
        val >>= cnt;
    }
}

AINLINE size_t calcAlignedByteCount( size_t bitidx, size_t bitcount ) noexcept
{
    if ( bitcount == 0 )
        return 0;

    size_t byteOnEnd = ( bitidx + bitcount - 1 ) / 8u;

    return ( byteOnEnd - bitidx / 8u ) + 1;
}

template <typename numberType>
AINLINE void trimmed_write_uint( endian::little_endian <numberType>& dst, numberType cpy, uint8_t fronttrimcnt, uint8_t backtrimcnt ) noexcept
{
    static_assert( std::is_unsigned <numberType>::value && std::is_integral <numberType>::value );

    if ( fronttrimcnt == 0 && backtrimcnt == 0 )
    {
        // Just write the entire thing.
        dst = cpy;
        return;
    }

    static constexpr uint8_t num_bits = ( sizeof(numberType) * 8 );

    // Prepare the write mask.
    numberType srcmask = 0;
    {
        numberType maskadd = 1;

        for ( uint8_t n = 0; n < fronttrimcnt; n++ )
        {
            if ( n > 0 )
            {
                shiftBitsInDir( maskadd, 1, true );
            }
            srcmask |= maskadd;
        }
    }
    {
        numberType maskadd = ( (numberType)1 << ( num_bits - 1 ) );

        for ( uint8_t n = 0; n < backtrimcnt; n++ )
        {
            if ( n > 0 )
            {
                shiftBitsInDir( maskadd, 1, false );
            }
            srcmask |= maskadd;
        }
    }

    numberType cur_dst = dst;

    cur_dst &= srcmask;
    cpy &= ~srcmask;
    dst = cur_dst | cpy;
}

union moveBitsDataContainer
{
    uint8_t cpybyte;
    uint16_t cpyword;
    uint32_t cpydword;
    uint64_t cpyqword;
};

AINLINE uint8_t getBitCountToByteBound( size_t bitidx )
{
    uint8_t lastByteUsefulBitcnt = (uint8_t)( ( bitidx % 8u ) + 1 );

    return ( 8u - lastByteUsefulBitcnt );
}

template <typename callbackType>
AINLINE void genericMaskedBitsWriter( void *dstBuf, size_t dstBitOff, size_t bitCount, callbackType&& cb ) noexcept
{
    if ( bitCount == 0 )
        return;

    size_t dstByteOff = ( dstBitOff / 8u );
    size_t dstByteEndOff = dstByteOff + calcAlignedByteCount( dstBitOff, bitCount );

    size_t dstBitLastIdx = ( dstBitOff + bitCount - 1 );
    uint8_t dstByteEndOff_bitsRemain = getBitCountToByteBound( dstBitLastIdx );

    size_t dst_bititer = dstBitOff;

    size_t cpybitidx = 0;

    while ( cpybitidx < bitCount )
    {
        // Get the amount of bytes than can be copied into the bit.
        uint8_t cpybyte_fronttrimcnt = 0;
        uint8_t cpybyte_backtrimcnt = 0;

        size_t dst_curbyteoff = ( dst_bititer / 8u );

        size_t dst_bytesleft = ( dstByteEndOff - dst_curbyteoff );
        size_t dst_docopybytecnt;

        if ( dst_bytesleft >= 8 )
        {
            dst_docopybytecnt = 8;
        }
        else if ( dst_bytesleft >= 4 )
        {
            dst_docopybytecnt = 4;
        }
        else if ( dst_bytesleft >= 2 )
        {
            dst_docopybytecnt = 2;
        }
        else
        {
            dst_docopybytecnt = 1;
        }

        // Prepare the trimming bitcounts.
        if ( dst_curbyteoff + dst_docopybytecnt == dstByteEndOff )
        {
            cpybyte_backtrimcnt = dstByteEndOff_bitsRemain;
        }

        // If we are to copy into an already written byte then we have to adjust the front trim to keep the data.
        uint8_t dst_bititer_intocpy = (uint8_t)( dst_bititer - dst_curbyteoff * 8u );

        cpybyte_fronttrimcnt += dst_bititer_intocpy;

        size_t toBeCopied = bitCount - cpybitidx;

        moveBitsDataContainer acquisition;

        size_t dstCanCopyCount = ( dst_docopybytecnt * 8u - cpybyte_fronttrimcnt - cpybyte_backtrimcnt );

        size_t src_bitRequestCount = std::min( dstCanCopyCount, toBeCopied );

        size_t curcopycount = cb( acquisition, dst_docopybytecnt, src_bitRequestCount, cpybyte_fronttrimcnt, cpybyte_backtrimcnt, cpybitidx );

        if ( dst_docopybytecnt == 8 )
        {
            auto& dstqword = *(endian::little_endian <uint64_t>*)( (uint8_t*)dstBuf + dst_curbyteoff );

            trimmed_write_uint( dstqword, acquisition.cpyqword, cpybyte_fronttrimcnt, cpybyte_backtrimcnt );
        }
        else if ( dst_docopybytecnt == 4 )
        {
            auto& dstdword = *(endian::little_endian <uint32_t>*)( (uint8_t*)dstBuf + dst_curbyteoff );

            trimmed_write_uint( dstdword, acquisition.cpydword, cpybyte_fronttrimcnt, cpybyte_backtrimcnt );
        }
        else if ( dst_docopybytecnt == 2 )
        {
            auto& dstword = *(endian::little_endian <uint16_t>*)( (uint8_t*)dstBuf + dst_curbyteoff );

            trimmed_write_uint( dstword, acquisition.cpyword, cpybyte_fronttrimcnt, cpybyte_backtrimcnt );
        }
        else if ( dst_docopybytecnt == 1 )
        {
            auto& dstbyte = *( (endian::little_endian <uint8_t>*)dstBuf + dst_curbyteoff );

            trimmed_write_uint( dstbyte, acquisition.cpybyte, cpybyte_fronttrimcnt, cpybyte_backtrimcnt );
        }

        cpybitidx += curcopycount;

        // Write the next byte in the next round.
        dst_bititer += curcopycount;
    }
}

// Prototyping some functionality.
AINLINE void moveBits( void *dstBuf, const void *srcBuf, size_t srcBitOff, size_t dstBitOff, size_t bitCount ) noexcept
{
    // NOTE: this algorithm does not yet support platforms that have no unaligned memory read support.
    // Adding support would be an easy extension of this algorithm that would restrict capture of write
    // and read regions based on ( srcBuf + bitoff ) / 8u or ( dstBuf + bitoff ) / 8u alignment.
    // When I get my hands on such a platform I will of course test and implement things properly.
    // For now we can consider the support of unaligned reads to be an implicit "optimization".

    genericMaskedBitsWriter( dstBuf, dstBitOff, bitCount,
        [&]( moveBitsDataContainer& acquisition, size_t dst_docopybytecnt, size_t src_bitRequestCount, uint8_t cpybyte_fronttrimcnt, uint8_t cpybyte_backtrimcnt, size_t cpybitidx ) noexcept -> size_t
        {
            // Determine which bits we want.
            size_t srcbitidx = ( cpybitidx + srcBitOff );
            size_t cpybytereadcount = calcAlignedByteCount( srcbitidx, src_bitRequestCount );
            size_t cpybytereadidx = ( srcbitidx / 8u );

            uint64_t transform_space = 0;
            uint8_t srcbitintospace = (uint8_t)( srcbitidx - cpybytereadidx * 8u );

            size_t actualrdbytecount = 0;

            if ( cpybytereadcount >= 8 )
            {
                transform_space = *(const eir::endian::little_endian <uint64_t>*)( (const uint8_t*)srcBuf + cpybytereadidx );
                actualrdbytecount = 8;
            }
            else if ( cpybytereadcount >= 4 )
            {
                transform_space = *(const eir::endian::little_endian <uint32_t>*)( (const uint8_t*)srcBuf + cpybytereadidx );
                actualrdbytecount = 4;
            }
            else if ( cpybytereadcount >= 2 )
            {
                transform_space = *(const eir::endian::little_endian <uint16_t>*)( (const uint8_t*)srcBuf + cpybytereadidx );
                actualrdbytecount = 2;
            }
            else if ( cpybytereadcount >= 1 )
            {
                transform_space = *( (const uint8_t*)srcBuf + cpybytereadidx );
                actualrdbytecount = 1;
            }

            size_t actuallyReadAndUsableBits = ( actualrdbytecount * 8u - srcbitintospace );

            if ( srcbitintospace > cpybyte_fronttrimcnt )
            {
                uint8_t moveBackCnt = ( srcbitintospace - cpybyte_fronttrimcnt );

                shiftBitsInDir( transform_space, moveBackCnt, false );
            }
            else
            {
                uint8_t moveForwardCnt = ( cpybyte_fronttrimcnt - srcbitintospace );

                shiftBitsInDir( transform_space, moveForwardCnt, true );

                actuallyReadAndUsableBits -= moveForwardCnt;
            }

            size_t curcopycount = std::min( src_bitRequestCount, actuallyReadAndUsableBits );

            if ( dst_docopybytecnt == 8 )
            {
                acquisition.cpyqword = transform_space;
            }
            else if ( dst_docopybytecnt == 4 )
            {
                acquisition.cpydword = (uint32_t)transform_space;
            }
            else if ( dst_docopybytecnt == 2 )
            {
                acquisition.cpyword = (uint16_t)transform_space;
            }
            else
            {
                acquisition.cpybyte = (uint8_t)transform_space;
            }

            return curcopycount;
        }
    );
}

AINLINE void setBits( void *dstBuf, bool bitValue, size_t dstBitOff, size_t bitCount ) noexcept
{
    genericMaskedBitsWriter(
        dstBuf, dstBitOff, bitCount,
        [&]( moveBitsDataContainer& acquisition, size_t dst_docopybytecnt, size_t src_bitRequestCount, uint8_t cpybyte_fronttrimcnt, uint8_t cpybyte_backtrimcnt, size_t cpybitidx ) noexcept -> size_t
        {
            if ( bitValue )
            {
                acquisition.cpyqword = std::numeric_limits <decltype(acquisition.cpyqword)>::max();
            }
            else
            {
                acquisition.cpyqword = 0;
            }

            return src_bitRequestCount;
        }
    );
}

} // namespace eir

#endif //_EIRREPO_DATA_UTILITIES_
