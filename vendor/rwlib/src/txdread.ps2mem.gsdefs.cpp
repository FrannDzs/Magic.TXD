/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.ps2mem.gsdefs.cpp
*  PURPOSE:     PlayStation2 internal GS memory definitions.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2

#include "txdread.ps2.hxx"

namespace rw
{

// The PS2 memory is a rectangular device. Basically its a set of pages that can be used for allocating image chunks.
// This class is supposed to emulate the texture allocation behavior.
namespace ps2GSMemoryLayoutArrangements
{
    // These tables define the linear arrangement of block coordinates in a page.
    // Pages are the ultimate unit of linear arrangement on PS2 GS memory.
    // * same for PSMT8H, PSMT4HH, PSMTtHL
    const uint32 block_psmct32[4*8] =
    {
         0u,  1u,  4u,  5u, 16u, 17u, 20u, 21u,
         2u,  3u,  6u,  7u, 18u, 19u, 22u, 23u,
         8u,  9u, 12u, 13u, 24u, 25u, 28u, 29u,
        10u, 11u, 14u, 15u, 26u, 27u, 30u, 31u
    };
    const uint32 block_psmz32[4*8] =
    {
        24u, 25u, 28u, 29u,  8u,  9u, 12u, 13u,
        26u, 27u, 30u, 31u, 10u, 11u, 14u, 15u,
        16u, 17u, 20u, 21u,  0u,  1u,  4u,  5u,
        18u, 19u, 22u, 23u,  2u,  3u,  6u,  7u
    };
    const uint32 block_psmct16[8*4] =
    {
         0u,  2u,  8u, 10u,
         1u,  3u,  9u, 11u,
         4u,  6u, 12u, 14u,
         5u,  7u, 13u, 15u,
        16u, 18u, 24u, 26u,
        17u, 19u, 25u, 27u,
        20u, 22u, 28u, 30u,
        21u, 23u, 29u, 31u
    };
    const uint32 block_psmz16[8*4] =
    {
        24u, 26u, 16u, 18u,
        25u, 27u, 17u, 19u,
        28u, 30u, 20u, 22u,
        29u, 31u, 21u, 23u,
         8u, 10u,  0u,  2u,
         9u, 11u,  1u,  3u,
        12u, 14u,  4u,  6u,
        13u, 15u,  5u,  7u
    };
    const uint32 block_psmct16s[8*4] =
    {
         0u,  2u, 16u, 18u,
         1u,  3u, 17u, 19u,
         8u, 10u, 24u, 26u,
         9u, 11u, 25u, 27u,
         4u,  6u, 20u, 22u,
         5u,  7u, 21u, 23u,
        12u, 14u, 28u, 30u,
        13u, 15u, 29u, 31u
    };
    const uint32 block_psmz16s[8*4] =
    {
        24u, 26u, 8u, 10u,
        25u, 27u, 9u, 11u,
        16u, 18u, 0u, 2u,
        17u, 19u, 1u, 3u,
        28u, 30u, 12u, 14u,
        29u, 31u, 13u, 15u,
        20u, 22u, 4u, 6u,
        21u, 23u, 5u, 7u
    };
    const uint32 block_psmt8[4*8] =
    {
         0u,  1u,  4u,  5u, 16u, 17u, 20u, 21u,
         2u,  3u,  6u,  7u, 18u, 19u, 22u, 23u,
         8u,  9u, 12u, 13u, 24u, 25u, 28u, 29u,
        10u, 11u, 14u, 15u, 26u, 27u, 30u, 31u
    };
    const uint32 block_psmt4[8*4] =
    {
         0u,  2u,  8u, 10u,
         1u,  3u,  9u, 11u,
         4u,  6u, 12u, 14u,
         5u,  7u, 13u, 15u,
        16u, 18u, 24u, 26u,
        17u, 19u, 25u, 27u,
        20u, 22u, 28u, 30u,
        21u, 23u, 29u, 31u
    };

    // Arrangement of texels in a columns.
    // The two existing swizzle patterns are shown here:
    const uint32 swizzlePattern_column_even[2*8] =
    {
        0u, 1u, 4u, 5u,  8u,  9u, 12u, 13u,
        2u, 3u, 6u, 7u, 10u, 11u, 14u, 15u
    };
   
    const uint32 swizzlePattern_column_odd[2*8] =
    {
         8u,  9u, 12u, 13u, 0u, 1u, 4u, 5u,
        10u, 11u, 14u, 15u, 2u, 3u, 6u, 7u
    };

    // Column configuration for packing pixels.
    const columnCoordinate column_32bit[1*1] =
    {
        { 0, swizzlePattern_column_even }
    };

    const columnCoordinate column_16bit[1*2] =
    {
        { 0, swizzlePattern_column_even }, { 1, swizzlePattern_column_even }
    };

    const columnCoordinate column_8bit_even[2*2] =
    {
        { 0, swizzlePattern_column_even }, { 2, swizzlePattern_column_even },
        { 1, swizzlePattern_column_odd },  { 3, swizzlePattern_column_odd }
    };

    const columnCoordinate column_8bit_odd[2*2] =
    {
        { 0, swizzlePattern_column_odd },  { 2, swizzlePattern_column_odd },
        { 1, swizzlePattern_column_even }, { 3, swizzlePattern_column_even }
    };

    const columnCoordinate column_4bit_even[2*4] =
    {
        { 0, swizzlePattern_column_even }, { 2, swizzlePattern_column_even }, { 4, swizzlePattern_column_even }, { 6, swizzlePattern_column_even },
        { 1, swizzlePattern_column_odd },  { 3, swizzlePattern_column_odd },  { 5, swizzlePattern_column_odd },  { 7, swizzlePattern_column_odd }
    };

    const columnCoordinate column_4bit_odd[2*4] =
    {
        { 0, swizzlePattern_column_odd },  { 2, swizzlePattern_column_odd },  { 4, swizzlePattern_column_odd },  { 6, swizzlePattern_column_odd },
        { 1, swizzlePattern_column_even }, { 3, swizzlePattern_column_even }, { 5, swizzlePattern_column_even }, { 7, swizzlePattern_column_even }
    };
};

} // namespace rw

#endif //RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2