/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.ps2mem.hxx
*  PURPOSE:     PS2 memory definitions.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_NATIVETEX_PS2_MEM_DEFS_
#define _RENDERWARE_NATIVETEX_PS2_MEM_DEFS_

#include "renderware.txd.ps2.h"

namespace rw
{

// All possible memory layouts of the PlayStation 2 Graphics Synthesizer.
enum eMemoryLayoutType
{
    PSMCT32 = 0,
    PSMCT24,
    PSMCT16,

    PSMCT16S = 10,

    PSMT8 = 19,
    PSMT4,

    PSMT8H = 27,
    PSMT4HL = 36,
    PSMT4HH = 44,

    PSMZ32 = 48,
    PSMZ24,
    PSMZ16,

    PSMZ16S = 58
};

// If the memory layout is valid, returns the string that identifies it.
AINLINE const wchar_t* GetValidMemoryLayoutTypeName( eMemoryLayoutType type )
{
    switch( type )
    {
    case PSMCT32:   return L"PSMCT32";
    case PSMCT24:   return L"PSMCT24";
    case PSMCT16:   return L"PSMCT16";
    case PSMCT16S:  return L"PSMCT16S";
    case PSMT8:     return L"PSMT8";
    case PSMT4:     return L"PSMT4";
    case PSMT8H:    return L"PSMT8H";
    case PSMT4HL:   return L"PSMT4HL";
    case PSMT4HH:   return L"PSMT4HH";
    case PSMZ32:    return L"PSMZ32";
    case PSMZ24:    return L"PSMZ24";
    case PSMZ16:    return L"PSMZ16";
    case PSMZ16S:   return L"PSMZ16S";
    }

    return nullptr;
}

// Returns the internal memory layout type that corresponds to the public one.
AINLINE eMemoryLayoutType GetInternalMemoryLayoutTypeFromPublic( ps2public::eMemoryLayoutType type )
{
    switch( type )
    {
    case ps2public::eMemoryLayoutType::PSMCT32:   return PSMCT32;
    case ps2public::eMemoryLayoutType::PSMCT24:   return PSMCT24;
    case ps2public::eMemoryLayoutType::PSMCT16:   return PSMCT16;
    case ps2public::eMemoryLayoutType::PSMCT16S:  return PSMCT16S;
    case ps2public::eMemoryLayoutType::PSMT8:     return PSMT8;
    case ps2public::eMemoryLayoutType::PSMT4:     return PSMT4;
    case ps2public::eMemoryLayoutType::PSMT8H:    return PSMT8H;
    case ps2public::eMemoryLayoutType::PSMT4HL:   return PSMT4HL;
    case ps2public::eMemoryLayoutType::PSMT4HH:   return PSMT4HH;
    case ps2public::eMemoryLayoutType::PSMZ32:    return PSMZ32;
    case ps2public::eMemoryLayoutType::PSMZ24:    return PSMZ24;
    case ps2public::eMemoryLayoutType::PSMZ16:    return PSMZ16;
    case ps2public::eMemoryLayoutType::PSMZ16S:   return PSMZ16S;
    }

    throw NativeTextureInvalidParameterException( "PlayStation2", L"PS2_INVPARAM_PUBMEMLAYOUT", nullptr );
}

// Get the memory layout item depth that is valid when the
// item is placed in GS memory (for example after unpacking by
// GIF).
AINLINE uint32 GetMemoryLayoutTypePresenceDepth( eMemoryLayoutType type )
{
    switch( type )
    {
    case PSMCT32:       return 32;
    case PSMCT24:       return 32;
    case PSMCT16:       return 16;
    case PSMCT16S:      return 16;
    case PSMT8:         return 8;
    case PSMT8H:        return 32;
    case PSMT4:         return 4;
    case PSMT4HH:       return 32;
    case PSMT4HL:       return 32;
    case PSMZ32:        return 32;
    case PSMZ24:        return 32;
    case PSMZ16:        return 16;
    case PSMZ16S:       return 16;
    }

    throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
}

// Get the depth of a memory layout type that is used for packing
// inside GS primitives.
AINLINE uint32 GetMemoryLayoutTypeGIFPackingDepth( eMemoryLayoutType type )
{
    switch( type )
    {
    case PSMCT32:       return 32;
    case PSMCT24:       return 24;
    case PSMCT16:       return 16;
    case PSMCT16S:      return 16;
    case PSMT8:         return 8;
    case PSMT8H:        return 8;
    case PSMT4:         return 4;
    case PSMT4HH:       return 4;
    case PSMT4HL:       return 4;
    case PSMZ32:        return 32;
    case PSMZ24:        return 24;
    case PSMZ16:        return 16;
    case PSMZ16S:       return 16;
    }

    throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
}

// Helper for bijective testing.
template <typename argumentType, typename... checkForArgs>
AINLINE bool BijectiveCheck( argumentType prim, argumentType second, argumentType shouldBeValue, checkForArgs... checkFor )
{
    if constexpr ( sizeof...( checkFor ) == 0 )
    {
        return ( prim == shouldBeValue || second == shouldBeValue );
    }
    else
    {
        return
            ( ( prim == shouldBeValue && ( ( second == checkFor ) || ... ) ) ||
              ( second == shouldBeValue && ( ( prim == checkFor ) || ... ) ) );
    }
}

// Returns true if two memory formats could collide upon
// placement on same memory, false otherwise. Can be used for
// optimization.
inline bool CanMemoryLayoutTypesCollide( eMemoryLayoutType first, eMemoryLayoutType second )
{
    return
        ( first == second ) ||
        BijectiveCheck( first, second, PSMCT32 ) ||
        BijectiveCheck( first, second, PSMCT24, PSMCT32, PSMCT16, PSMCT16S, PSMT8, PSMT4, PSMZ32, PSMZ24, PSMZ16, PSMZ16S ) ||
        BijectiveCheck( first, second, PSMCT16 ) ||
        BijectiveCheck( first, second, PSMCT16S ) ||
        BijectiveCheck( first, second, PSMT8 ) ||
        BijectiveCheck( first, second, PSMT4 ) ||
        BijectiveCheck( first, second, PSMT8H, PSMCT32, PSMCT16, PSMT8, PSMT4, PSMT4HL, PSMT4HH, PSMZ32, PSMZ16 ) ||
        BijectiveCheck( first, second, PSMT4HL, PSMCT32, PSMCT16, PSMT8, PSMT4, PSMT4HL, PSMZ32, PSMZ16 ) ||
        BijectiveCheck( first, second, PSMT4HH, PSMCT32, PSMCT16, PSMT8, PSMT4, PSMT4HH, PSMZ32, PSMZ16 ) ||
        BijectiveCheck( first, second, PSMZ32 ) ||
        BijectiveCheck( first, second, PSMZ24, PSMCT32, PSMCT24, PSMCT16, PSMCT16S, PSMT8, PSMT4, PSMZ32, PSMZ16, PSMZ16S ) ||
        BijectiveCheck( first, second, PSMZ16 ) ||
        BijectiveCheck( first, second, PSMZ16S );
}

// Returns true if a CLUT is attached to the given memory layout type.
inline bool DoesMemoryLayoutTypeRequireCLUT( eMemoryLayoutType type )
{
    switch( type )
    {
    case PSMT8:
    case PSMT4:
    case PSMT8H:
    case PSMT4HH:
    case PSMT4HL:
        return true;
    default:
        break;
    }

    return false;
}

// The CLUT memory addressing modes are a valid subset of the global ones.
enum class eCLUTMemoryLayoutType
{
    PSMCT32 = 0,
    PSMCT16 = 2,
    PSMCT16S = 10
};

// You can get the CLUT transfer depth by converting to regular descriptor
// type first.

// Returns the memory layout name of CLUT if valid, otherwise nullptr.
AINLINE const wchar_t* GetValidCLUTMemoryLayoutTypeName( eCLUTMemoryLayoutType type )
{
    switch( type )
    {
    case eCLUTMemoryLayoutType::PSMCT32:    return L"PSMCT32";
    case eCLUTMemoryLayoutType::PSMCT16:    return L"PSMCT16";
    case eCLUTMemoryLayoutType::PSMCT16S:   return L"PSMCT16S";
    }

    return nullptr;
}

// Returns the internal CLUT memory layout type from public.
AINLINE eCLUTMemoryLayoutType GetInternalCLUTMemoryLayoutTypeFromPublic( ps2public::eCLUTMemoryLayoutType type )
{
    switch( type )
    {
    case ps2public::eCLUTMemoryLayoutType::PSMCT32:    return eCLUTMemoryLayoutType::PSMCT32;
    case ps2public::eCLUTMemoryLayoutType::PSMCT16:    return eCLUTMemoryLayoutType::PSMCT16;
    case ps2public::eCLUTMemoryLayoutType::PSMCT16S:   return eCLUTMemoryLayoutType::PSMCT16S;
    }

    throw NativeTextureInvalidParameterException( "PlayStation2", L"PS2_INVPARAM_PUBCLUTMEMLAYOUT", nullptr );
}

// If the given memory layout is a valid CLUT layout, returns the matching global layout type.
AINLINE eMemoryLayoutType GetCommonMemoryLayoutTypeFromCLUTLayoutType( eCLUTMemoryLayoutType type )
{
#ifdef _DEBUG
    assert( GetValidCLUTMemoryLayoutTypeName( type ) != nullptr );
#endif //_DEBUG

    switch( type )
    {
    case eCLUTMemoryLayoutType::PSMCT32:    return PSMCT32;
    case eCLUTMemoryLayoutType::PSMCT16:    return PSMCT16;
    case eCLUTMemoryLayoutType::PSMCT16S:   return PSMCT16S;
    }

    throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
}

// Exports for shared runtimes.
namespace ps2GSMemoryLayoutArrangements
{
    // Blocks in a page.
    extern const uint32 block_psmct32[4*8];     // width = 8, height = 4
    extern const uint32 block_psmz32[4*8];      // width = 8, height = 4
    extern const uint32 block_psmct16[8*4];     // width = 4, height = 8
    extern const uint32 block_psmz16[8*4];      // width = 4, height = 8
    extern const uint32 block_psmct16s[8*4];    // width = 4, height = 8
    extern const uint32 block_psmz16s[8*4];     // width = 4, height = 8
    extern const uint32 block_psmt8[4*8];       // width = 8, height = 8
    extern const uint32 block_psmt4[8*4];       // width = 4, height = 8

    // Columns in a block.
    static constexpr uint32 swizzlePattern_width = 8;
    static constexpr uint32 swizzlePattern_height = 2;

    extern const uint32 swizzlePattern_column_even[swizzlePattern_height * swizzlePattern_width];
    extern const uint32 swizzlePattern_column_odd[swizzlePattern_height * swizzlePattern_width];

    // Column configuration for packing columns into columns.
    struct columnCoordinate
    {
        uint32 packed_idx;
        const uint32* pixelArrangementInColumn;
    };

    // * same for PSMCT32, PSMCT24, PSMZ32, PSMZ24
    extern const columnCoordinate column_32bit[1*1];        // width = 1, height = 1

    // * same for PSMCT16, PSMCT16S, PSMZ16, PSMZ16S
    extern const columnCoordinate column_16bit[1*2];        // width = 2, height = 1

    // * PSMT8
    extern const columnCoordinate column_8bit_even[2*2];    // width = 2, height = 2
    extern const columnCoordinate column_8bit_odd[2*2];

    // * PSMT4
    extern const columnCoordinate column_4bit_even[2*4];    // width = 4, height = 2
    extern const columnCoordinate column_4bit_odd[2*4];
}

struct ps2MemoryLayoutTypeProperties
{
    uint32 pixelWidthPerColumn, pixelHeightPerColumn;
    uint32 widthBlocksPerPage, heightBlocksPerPage;

    // Column arrangement in a block is always four vertical columns.

    const uint32* blockArrangementInPage;
};

// Calculator for swizzle coordinates.
struct defaultColumnCoordinateCalculator
{
    AINLINE static ps2GSMemoryLayoutArrangements::columnCoordinate calc_coord(
        uint32 x,
        uint32 y,
        const ps2MemoryLayoutTypeProperties& tex_layout,
        const ps2MemoryLayoutTypeProperties& bitblt_layout
    )
    {
        uint32 tex_pixHeight = tex_layout.pixelHeightPerColumn;

        uint32 bitblt_pixHeight = bitblt_layout.pixelHeightPerColumn;

#ifdef _DEBUG
        uint32 tex_pixWidth = tex_layout.pixelWidthPerColumn;

        uint32 bitblt_pixWidth = bitblt_layout.pixelWidthPerColumn;

        if ( tex_pixWidth < bitblt_pixWidth || tex_pixHeight < bitblt_pixHeight ||
             tex_pixWidth % bitblt_pixWidth != 0 || tex_pixHeight % bitblt_pixHeight != 0 )
        {
            throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
        }
#endif //_DEBUG

        ps2GSMemoryLayoutArrangements::columnCoordinate coord;

        //uint32 column_fit_width = tex_pixWidth / bitblt_pixWidth;
        uint32 column_fit_height = tex_pixHeight / bitblt_pixHeight;

        coord.pixelArrangementInColumn =
            ( y % 2 == 0 ? ps2GSMemoryLayoutArrangements::swizzlePattern_column_even : ps2GSMemoryLayoutArrangements::swizzlePattern_column_odd );

        coord.packed_idx = ( y + x * column_fit_height );

        return coord;
    }
};

// Returns common properties of a memory layout type.
// Use for both memory allocation and packing.
inline static ps2MemoryLayoutTypeProperties getMemoryLayoutTypeProperties( eMemoryLayoutType memLayout )
{
#ifdef _DEBUG
    assert( GetValidMemoryLayoutTypeName( memLayout ) != nullptr );
#endif //_DEBUG

    ps2MemoryLayoutTypeProperties layoutProps;

    if ( memLayout == PSMT4 )
    {
        layoutProps.widthBlocksPerPage = 4;
        layoutProps.heightBlocksPerPage = 8;
        layoutProps.blockArrangementInPage = ps2GSMemoryLayoutArrangements::block_psmt4;

        layoutProps.pixelWidthPerColumn = 32;
        layoutProps.pixelHeightPerColumn = 4;
    }
    else if ( memLayout == PSMT8 )
    {
        layoutProps.widthBlocksPerPage = 8;
        layoutProps.heightBlocksPerPage = 4;
        layoutProps.blockArrangementInPage = ps2GSMemoryLayoutArrangements::block_psmt8;

        layoutProps.pixelWidthPerColumn = 16;
        layoutProps.pixelHeightPerColumn = 4;
    }
    else if ( memLayout == PSMCT32 || memLayout == PSMCT24 ||
              memLayout == PSMZ32 || memLayout == PSMZ24 ||
              memLayout == PSMT8H || memLayout == PSMT4HH || memLayout == PSMT4HL )
    {
        layoutProps.widthBlocksPerPage = 8;
        layoutProps.heightBlocksPerPage = 4;

        if ( memLayout == PSMCT32 || memLayout == PSMCT24 )
        {
            layoutProps.blockArrangementInPage = ps2GSMemoryLayoutArrangements::block_psmct32;
        }
        else if ( memLayout == PSMZ32 || memLayout == PSMZ24 )
        {
            layoutProps.blockArrangementInPage = ps2GSMemoryLayoutArrangements::block_psmz32;
        }

        layoutProps.pixelWidthPerColumn = 8;
        layoutProps.pixelHeightPerColumn = 2;
    }
    else if ( memLayout == PSMCT16 || memLayout == PSMCT16S ||
              memLayout == PSMZ16 || memLayout == PSMZ16S )
    {
        layoutProps.widthBlocksPerPage = 4;
        layoutProps.heightBlocksPerPage = 8;

        if ( memLayout == PSMCT16 )
        {
            layoutProps.blockArrangementInPage = ps2GSMemoryLayoutArrangements::block_psmct16;
        }
        else if ( memLayout == PSMCT16S )
        {
            layoutProps.blockArrangementInPage = ps2GSMemoryLayoutArrangements::block_psmct16s;
        }
        else if ( memLayout == PSMZ16 )
        {
            layoutProps.blockArrangementInPage = ps2GSMemoryLayoutArrangements::block_psmz16;
        }
        else if ( memLayout == PSMZ16S )
        {
            layoutProps.blockArrangementInPage = ps2GSMemoryLayoutArrangements::block_psmz16s;
        }

        layoutProps.pixelWidthPerColumn = 16;
        layoutProps.pixelHeightPerColumn = 2;
    }
    else
    {
        throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
    }

    return layoutProps;
}

} // namespace rw

#endif //_RENDERWARE_NATIVETEX_PS2_MEM_DEFS_
