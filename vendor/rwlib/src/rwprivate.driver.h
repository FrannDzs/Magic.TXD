/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwprivate.driver.h
*  PURPOSE:     RenderWare driver utilities for GPU and framework interaction.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_PRIVATE_DRIVER_UTILITIES_
#define _RENDERWARE_PRIVATE_DRIVER_UTILITIES_

namespace rw
{

// Returns the depth of a gfxRasterFormat value.
inline size_t getDriverRasterFormatSampleDepth( gfxRasterFormat rasterFormat )
{
    switch( rasterFormat )
    {
    case gfxRasterFormat::UNSPECIFIED:
        return 0;
    case gfxRasterFormat::R32G32B32A32_TYPELESS:
    case gfxRasterFormat::R32G32B32A32_FLOAT:
    case gfxRasterFormat::R32G32B32A32_UINT:
    case gfxRasterFormat::R32G32B32A32_SINT:
        return ( sizeof(uint32) * 4 );
    case gfxRasterFormat::R32G32B32_TYPELESS:
    case gfxRasterFormat::R32G32B32_FLOAT:
    case gfxRasterFormat::R32G32B32_UINT:
    case gfxRasterFormat::R32G32B32_SINT:
    case gfxRasterFormat::DEPTH24:
        return ( sizeof(uint32) * 3 );
    case gfxRasterFormat::R8G8B8A8:
    case gfxRasterFormat::DEPTH32:
        return ( sizeof(uint32) );
    case gfxRasterFormat::B5G6R5:
    case gfxRasterFormat::B5G5R5A1:
    case gfxRasterFormat::DEPTH16:
        return ( sizeof(uint16) );
    case gfxRasterFormat::LUM8:
        return ( sizeof(uint8) );
    default:
        break;
    }

    return 0;
}

inline bool isDriverRasterFormatRawSample( gfxRasterFormat rasterFormat )
{
    switch( rasterFormat )
    {
    case gfxRasterFormat::R32G32B32A32_TYPELESS:
    case gfxRasterFormat::R32G32B32A32_FLOAT:
    case gfxRasterFormat::R32G32B32A32_UINT:
    case gfxRasterFormat::R32G32B32A32_SINT:
    case gfxRasterFormat::R32G32B32_TYPELESS:
    case gfxRasterFormat::R32G32B32_FLOAT:
    case gfxRasterFormat::R32G32B32_UINT:
    case gfxRasterFormat::R32G32B32_SINT:
    case gfxRasterFormat::DEPTH24:
    case gfxRasterFormat::R8G8B8A8:
    case gfxRasterFormat::DEPTH32:
    case gfxRasterFormat::B5G6R5:
    case gfxRasterFormat::B5G5R5A1:
    case gfxRasterFormat::DEPTH16:
    case gfxRasterFormat::LUM8:
        return true;
    default:
        break;
    }

    return false;
}

} // namespace rw

#endif //_RENDERWARE_PRIVATE_DRIVER_UTILITIES_
