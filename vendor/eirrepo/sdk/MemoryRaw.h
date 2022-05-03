/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MemoryRaw.h
*  PURPOSE:     Base memory management definitions for to-the-metal things
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _MEMORY_RAW_DEFS_
#define _MEMORY_RAW_DEFS_

#include "MathSlice.h"

// Mathematically correct data slice logic.
// It is one of the most important theorems in computing abstraction.
// TODO: limit this type to integral things only.
template <typename numberType>
using sliceOfData = eir::mathSlice <numberType>;

#include <type_traits>
#include <bit>  // for std::has_single_bit
#include <limits>

// Macro that defines how alignment works.
//  num: base of the number to be aligned
//  sector: aligned-offset that should be added to num
//  align: number of bytes to align to
// EXAMPLE: ALIGN( 0x1001, 4, 4 ) -> 0x1004 (equivalent of compiler structure padding alignment)
//          ALIGN( 0x1003, 1, 4 ) -> 0x1000
//          ALIGN( 0x1003, 2, 4 ) -> 0x1004
template <typename numberType> requires ( std::is_integral <numberType>::value && std::is_unsigned <numberType>::value )
AINLINE numberType _ALIGN_GP( numberType num, numberType sector, numberType align )
{
	// General purpose alignment routine.
    // Not as fast as the bitfield version.
    numberType sectorOffset = ((num) + (sector) - 1);

    return sectorOffset - ( sectorOffset % align );
}

template <typename numberType> requires ( std::is_integral <numberType>::value &&  std::is_unsigned <numberType>::value )
AINLINE numberType _ALIGN_NATIVE( numberType num, numberType sector, numberType align )
{
    // assume math based on x86 bits.
    if ( std::has_single_bit( align ) )
    {
        //bitfield version. not compatible with non-bitfield alignments.
        return (((num) + (sector) - 1) & (~((align) - 1)));
    }
    else
    {
		return _ALIGN_GP( num, sector, align );
    }
}

template <typename numberType> requires ( std::is_integral <numberType>::value &&  std::is_unsigned <numberType>::value )
AINLINE numberType ALIGN( numberType num, numberType sector, numberType align )
{
    if constexpr ( std::same_as <numberType, unsigned char> ||
                   std::same_as <numberType, unsigned short> ||
                   std::same_as <numberType, unsigned int> ||
                   std::same_as <numberType, unsigned long> )
    {
        return _ALIGN_NATIVE( num, sector, align );
    }
    else
    {
	    return _ALIGN_GP( num, sector, align );
    }
}

// Helper macro (equivalent of EXAMPLE 1)
template <typename numberType> requires ( std::is_integral <numberType>::value &&  std::is_unsigned <numberType>::value )
inline numberType ALIGN_SIZE( numberType num, numberType sector )
{
    return ( ALIGN( (num), (sector), (sector) ) );
}

// Aligning things to the boundary below.
template <typename numberType> requires ( std::is_integral <numberType>::value &&  std::is_unsigned <numberType>::value )
AINLINE numberType SCALE_DOWN( numberType value, numberType modval ) noexcept
{
    // This is faster than divide-and-multiply, plus it does exactly the same.
    numberType rem = ( value % modval );

    return ( value - rem );
}

template <typename numberType, typename divNumberType>
    requires ( std::is_integral <numberType>::value && std::is_integral <divNumberType>::value )
AINLINE numberType CEIL_DIV( numberType val, divNumberType div )
{
    return ( ( val + ( div - 1 ) ) / div );
}

// Safely casting integers between each others, with clamping to bounds.
template <typename destIntegerType, typename srcIntegerType>
    requires ( std::is_integral <destIntegerType>::value && std::is_integral <srcIntegerType>::value )
AINLINE destIntegerType TRANSFORM_INT_CLAMP( srcIntegerType value )
{
    typedef typename std::make_signed <srcIntegerType>::type srcIntegerSigned_t;
    typedef typename std::make_unsigned <srcIntegerType>::type srcIntegerUnsigned_t;

    typedef typename std::make_signed <destIntegerType>::type destIntegerSigned_t;
    typedef typename std::make_unsigned <destIntegerType>::type destIntegerUnsigned_t;

    constexpr srcIntegerUnsigned_t src_max = (srcIntegerUnsigned_t)std::numeric_limits <srcIntegerType>::max();
    constexpr srcIntegerSigned_t src_min = (srcIntegerSigned_t)std::numeric_limits <srcIntegerType>::min();

    constexpr destIntegerUnsigned_t dest_max = (destIntegerUnsigned_t)std::numeric_limits <destIntegerType>::max();
    constexpr destIntegerSigned_t dest_min = (destIntegerSigned_t)std::numeric_limits <destIntegerType>::min();

    // We assume that all unsigned integer types start at 0, all maximum values of ranges are non-negative.

    if constexpr ( dest_max < src_max )
    {
        if ( value > (srcIntegerType)dest_max )
        {
            return dest_max;
        }
    }

    if constexpr ( dest_min > src_min )
    {
        if ( value < (srcIntegerType)dest_min )
        {
            return dest_min;
        }
    }

    return (destIntegerType)value;
}

#endif //_MEMORY_RAW_DEFS_
