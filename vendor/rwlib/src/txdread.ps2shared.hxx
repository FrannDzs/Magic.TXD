/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.ps2shared.hxx
*  PURPOSE:     Shared definitions between Sony platforms that originate
*               from the design of the PlayStation 2.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef RW_SHARED_PS2_DEFINITIONS
#define RW_SHARED_PS2_DEFINITIONS

#include "pixelformat.hxx"

#include "txdread.ps2.registers.hxx"

namespace rw
{

enum class eRasterFormatPS2
{
    RASTER_DEFAULT,
    RASTER_1555,
    RASTER_8888 = 5,
    RASTER_888,
    RASTER_16,
    RASTER_24,
    RASTER_32,
    RASTER_555
};

AINLINE const wchar_t* GetValidPS2RasterFormatName( eRasterFormatPS2 fmt )
{
    switch( fmt )
    {
    case eRasterFormatPS2::RASTER_DEFAULT:  return L"none";
    case eRasterFormatPS2::RASTER_1555:     return L"1555";
    case eRasterFormatPS2::RASTER_8888:     return L"8888";
    case eRasterFormatPS2::RASTER_888:      return L"888";
    case eRasterFormatPS2::RASTER_555:      return L"555";
    case eRasterFormatPS2::RASTER_16:       return L"D16";
    case eRasterFormatPS2::RASTER_24:       return L"D24";
    case eRasterFormatPS2::RASTER_32:       return L"D32";
    }

    return nullptr;
}

AINLINE eRasterFormat GetGenericRasterFormatFromPS2( eRasterFormatPS2 fmt )
{
    switch( fmt )
    {
    case eRasterFormatPS2::RASTER_DEFAULT:  return RASTER_DEFAULT;
    case eRasterFormatPS2::RASTER_1555:     return RASTER_1555;
    case eRasterFormatPS2::RASTER_8888:     return RASTER_8888;
    case eRasterFormatPS2::RASTER_888:      return RASTER_888;
    case eRasterFormatPS2::RASTER_555:      return RASTER_555;
    case eRasterFormatPS2::RASTER_16:       return RASTER_16;
    case eRasterFormatPS2::RASTER_24:       return RASTER_24;
    case eRasterFormatPS2::RASTER_32:       return RASTER_32;
    }

    throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
}

enum class ePaletteTypePS2
{
    NONE,
    PAL8,
    PAL4
};

AINLINE void GetPS2RasterFormatFromMemoryLayoutType(
    eMemoryLayoutType pixelMemLayout, eCLUTMemoryLayoutType clutPixelMemLayout,
    eRasterFormatPS2& formatTypeOut, ePaletteTypePS2& palTypeOut
)
{
    // Use the GIF packing depth if you want to query a depth value.

    if ( pixelMemLayout == PSMCT32 )
    {
        formatTypeOut = eRasterFormatPS2::RASTER_8888;
        palTypeOut = ePaletteTypePS2::NONE;
    }
    else if ( pixelMemLayout == PSMCT24 )
    {
        formatTypeOut = eRasterFormatPS2::RASTER_888;
        palTypeOut = ePaletteTypePS2::NONE;
    }
    else if ( pixelMemLayout == PSMCT16 || pixelMemLayout == PSMCT16S )
    {
        formatTypeOut = eRasterFormatPS2::RASTER_1555;
        palTypeOut = ePaletteTypePS2::NONE;
    }
    else if ( pixelMemLayout == PSMT8 || pixelMemLayout == PSMT4 ||
              pixelMemLayout == PSMT8H || pixelMemLayout == PSMT4HL ||
              pixelMemLayout == PSMT4HH )
    {
        if ( pixelMemLayout == PSMT4 || pixelMemLayout == PSMT4HL || pixelMemLayout == PSMT4HH )
        {
            palTypeOut = ePaletteTypePS2::PAL4;
        }
        else if ( pixelMemLayout == PSMT8 || pixelMemLayout == PSMT8H )
        {
            palTypeOut = ePaletteTypePS2::PAL8;
        }
        else
        {
            throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
        }

        if ( clutPixelMemLayout == eCLUTMemoryLayoutType::PSMCT32 )
        {
            formatTypeOut = eRasterFormatPS2::RASTER_8888;
        }
        else if ( clutPixelMemLayout == eCLUTMemoryLayoutType::PSMCT16 ||
                  clutPixelMemLayout == eCLUTMemoryLayoutType::PSMCT16S )
        {
            formatTypeOut = eRasterFormatPS2::RASTER_1555;
        }
        else
        {
            throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
        }
    }
    else if ( pixelMemLayout == PSMZ32 )
    {
        formatTypeOut = eRasterFormatPS2::RASTER_32;
        palTypeOut = ePaletteTypePS2::NONE;
    }
    else if ( pixelMemLayout == PSMZ24 )
    {
        formatTypeOut = eRasterFormatPS2::RASTER_24;
        palTypeOut = ePaletteTypePS2::NONE;
    }
    else if ( pixelMemLayout == PSMZ16 || pixelMemLayout == PSMZ16S )
    {
        formatTypeOut = eRasterFormatPS2::RASTER_16;
        palTypeOut = ePaletteTypePS2::NONE;
    }
    else
    {
        throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
    }
}

AINLINE bool IsPS2RasterFormatCompatibleWithMemoryLayoutType(
    eRasterFormatPS2 formatType, ePaletteTypePS2 palType,
    eMemoryLayoutType memType, eCLUTMemoryLayoutType clutMemType
)
{
    if ( palType == ePaletteTypePS2::PAL4 )
    {
        if ( memType != PSMT4 && memType != PSMT4HH && memType != PSMT4HL )
            return false;

        if ( formatType == eRasterFormatPS2::RASTER_1555 )
        {
            return ( clutMemType == eCLUTMemoryLayoutType::PSMCT16 || clutMemType == eCLUTMemoryLayoutType::PSMCT16S );
        }
        if ( formatType == eRasterFormatPS2::RASTER_8888 )
        {
            return ( clutMemType == eCLUTMemoryLayoutType::PSMCT32 );
        }

        return false;
    }
    else if ( palType == ePaletteTypePS2::PAL8 )
    {
        return ( memType == PSMT8 || memType == PSMT8H );
    }
    else if ( palType == ePaletteTypePS2::NONE )
    {
        if ( formatType == eRasterFormatPS2::RASTER_DEFAULT )
        {
            // By not specifying any raster format we are compatible.
            return true;
        }

        if ( formatType == eRasterFormatPS2::RASTER_1555 || formatType == eRasterFormatPS2::RASTER_555 )
        {
            return ( memType == PSMCT16 || memType == PSMCT16S );
        }
        if ( formatType == eRasterFormatPS2::RASTER_8888 )
        {
            return ( memType == PSMCT32 );
        }
        if ( formatType == eRasterFormatPS2::RASTER_888 )
        {
            return ( memType == PSMCT24 );
        }
        if ( formatType == eRasterFormatPS2::RASTER_16 )
        {
            return ( memType == PSMZ16 );
        }
        if ( formatType == eRasterFormatPS2::RASTER_24 )
        {
            return ( memType == PSMZ24 );
        }
        if ( formatType == eRasterFormatPS2::RASTER_32 )
        {
            return ( memType == PSMZ32 );
        }
    }

    throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
}

AINLINE const wchar_t* GetValidPS2PaletteTypeName( ePaletteTypePS2 palType )
{
    switch( palType )
    {
    case ePaletteTypePS2::NONE:     return L"none";
    case ePaletteTypePS2::PAL4:     return L"PAL4";
    case ePaletteTypePS2::PAL8:     return L"PAL8";
    }

    return nullptr;
}

AINLINE ePaletteType GetGenericPaletteTypeFromPS2( ePaletteTypePS2 palType )
{
    switch( palType )
    {
    case ePaletteTypePS2::NONE:     return PALETTE_NONE;
    case ePaletteTypePS2::PAL4:     return PALETTE_4BIT;
    case ePaletteTypePS2::PAL8:     return PALETTE_8BIT;
    }

    throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
}

static inline eMemoryLayoutType getMemoryLayoutTypeFromRasterFormat( eRasterFormatPS2 rasterFormat, ePaletteTypePS2 paletteType )
{
    if (paletteType == ePaletteTypePS2::PAL4)
    {
        return PSMT4;
    }
    else if (paletteType == ePaletteTypePS2::PAL8)
    {
        return PSMT8;
    }
    else if (rasterFormat == eRasterFormatPS2::RASTER_888)
    {
        return PSMCT24;
    }
    else if (rasterFormat == eRasterFormatPS2::RASTER_1555 || rasterFormat == eRasterFormatPS2::RASTER_555)
    {
        return PSMCT16S;
    }
    else if (rasterFormat == eRasterFormatPS2::RASTER_8888)
    {
        return PSMCT32;
    }
    else if (rasterFormat == eRasterFormatPS2::RASTER_32)
    {
        return PSMZ32;
    }
    else if (rasterFormat == eRasterFormatPS2::RASTER_24)
    {
        return PSMZ24;
    }
    else if (rasterFormat == eRasterFormatPS2::RASTER_16)
    {
        return PSMZ16;
    }

    throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
}

// Helper function.
// TODO: is this even required?
static inline void genpalettetexeldata(
    Interface *engineInterface,
    uint32 texelWidth, uint32 texelHeight,
    void *paletteData, eRasterFormat rasterFormat, ePaletteType paletteType, uint32 itemCount,
    void*& texelData, uint32& texelDataSize
)
{
    // Allocate texture memory.
    uint32 texelItemCount = ( texelWidth * texelHeight );

    // Calculate the data size.
    uint32 palDepth = Bitmap::getRasterFormatDepth(rasterFormat);

    assert( itemCount != 0 );
    assert( texelItemCount != 0 );

    uint32 srcDataSize = getPaletteDataSize( itemCount, palDepth );
    uint32 dstDataSize = getPaletteDataSize( texelItemCount, palDepth );

    assert( srcDataSize != 0 );
    assert( dstDataSize != 0 );

    void *newTexelData = nullptr;

    if ( srcDataSize != dstDataSize )
    {
        newTexelData = engineInterface->PixelAllocate( dstDataSize );

        // Write the new memory.
        memcpy(newTexelData, paletteData, std::min(srcDataSize, dstDataSize));

        if (dstDataSize > srcDataSize)
        {
            // Zero out the rest.
            memset((char*)newTexelData + srcDataSize, 0, (dstDataSize - srcDataSize));
        }
    }
    else
    {
        newTexelData = paletteData;
    }

    // Give parameters to the runtime.
    texelData = newTexelData;
    texelDataSize = dstDataSize;
}

}

#endif //RW_SHARED_PS2_DEFINITIONS
