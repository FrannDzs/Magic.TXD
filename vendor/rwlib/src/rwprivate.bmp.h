/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwprivate.bmp.h
*  PURPOSE:     RenderWare BMP private header that includes useful
*               raster format processing utilities.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_PRIVATE_BITMAP_HEADER_
#define _RENDERWARE_PRIVATE_BITMAP_HEADER_

namespace rw
{

inline bool shouldAllocateNewRasterBuffer(
    uint32 mipWidth,
    uint32 srcDepth, uint32 srcRowAlignment,
    uint32 dstDepth, uint32 dstRowAlignment
)
{
    // If the depth changed, an item will take a different amount of space.
    // This means that items cannot be placed at the positions where they belong to (swapping) so get off.
    if ( srcDepth != dstDepth )
    {
        return true;
    }

    // Assuming the depth is the same, then the alignment may change the size of the resulting
    // texel data. If it does, then we have to reallocate.
    if ( srcRowAlignment != dstRowAlignment )
    {
        size_t srcRowBitSize = getRasterDataRowSize( mipWidth, srcDepth, srcRowAlignment ).getBitSize();
        size_t dstRowBitSize = getRasterDataRowSize( mipWidth, dstDepth, dstRowAlignment ).getBitSize();

        if ( srcRowBitSize != dstRowBitSize )
        {
            return true;
        }
    }

    return false;
}

inline bool hasConflictingAddressing(
    uint32 mipWidth,
    uint32 srcDepth, uint32 srcRowAlignment, ePaletteType srcPaletteType,
    uint32 dstDepth, uint32 dstRowAlignment, ePaletteType dstPaletteType
)
{
    // If the buffer dimensions are different, we should kinda think about it.
    if ( shouldAllocateNewRasterBuffer( mipWidth, srcDepth, srcRowAlignment, dstDepth, dstRowAlignment ) )
        return true;

    // If there is conflicting addressing, you most likely need a different destination buffer for conversion.
    // This happens sometimes.
    if ( srcPaletteType != PALETTE_NONE && dstPaletteType != PALETTE_NONE )
    {
        // Because palette types are really complicated, a change in palette type most likely
        // introduces a change in addressing. Be careful about that.
        if ( srcPaletteType != dstPaletteType )
        {
            return true;
        }
    }

    // We are good to go.
    return false;
}

} // namespace rw

#endif //_RENDERWARE_PRIVATE_BITMAP_HEADER_