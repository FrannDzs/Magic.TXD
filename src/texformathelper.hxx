/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/texformathelper.hxx
*  PURPOSE:     ABI compatible mapping between framework types and virtual export types.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

inline void MagicMapToInternalRasterFormat( MAGIC_RASTER_FORMAT formatIn, rw::eRasterFormat& formatOut )
{
    if ( formatIn == MAGIC_RASTER_FORMAT::RASTER_DEFAULT )
    {
        formatOut = rw::RASTER_DEFAULT;
    }
    else if ( formatIn == MAGIC_RASTER_FORMAT::RASTER_1555 )
    {
        formatOut = rw::RASTER_1555;
    }
    else if ( formatIn == MAGIC_RASTER_FORMAT::RASTER_565 )
    {
        formatOut = rw::RASTER_4444;
    }
    else if ( formatIn == MAGIC_RASTER_FORMAT::RASTER_4444 )
    {
        formatOut = rw::RASTER_4444;
    }
    else if ( formatIn == MAGIC_RASTER_FORMAT::RASTER_LUM )
    {
        formatOut = rw::RASTER_LUM;
    }
    else if ( formatIn == MAGIC_RASTER_FORMAT::RASTER_8888 )
    {
        formatOut = rw::RASTER_8888;
    }
    else if ( formatIn == MAGIC_RASTER_FORMAT::RASTER_888 )
    {
        formatOut = rw::RASTER_888;
    }
    else if ( formatIn == MAGIC_RASTER_FORMAT::RASTER_555 )
    {
        formatOut = rw::RASTER_555;
    }
    else
    {
        formatOut = rw::RASTER_DEFAULT;
    }
}

inline void MagicMapToInternalColorOrdering( MAGIC_COLOR_ORDERING colorOrderIn, rw::eColorOrdering& colorOrderOut )
{
    if ( colorOrderIn == MAGIC_COLOR_ORDERING::COLOR_RGBA )
    {
        colorOrderOut = rw::COLOR_RGBA;
    }
    else if ( colorOrderIn == MAGIC_COLOR_ORDERING::COLOR_BGRA )
    {
        colorOrderOut = rw::COLOR_BGRA;
    }
    else if ( colorOrderIn == MAGIC_COLOR_ORDERING::COLOR_ABGR )
    {
        colorOrderOut = rw::COLOR_ABGR;
    }
    else
    {
        colorOrderOut = rw::COLOR_RGBA;
    }
}

inline void MagicMapToInternalPaletteType( MAGIC_PALETTE_TYPE paletteTypeIn, rw::ePaletteType& paletteTypeOut )
{
    if ( paletteTypeIn == MAGIC_PALETTE_TYPE::PALETTE_4BIT )
    {
        paletteTypeOut = rw::PALETTE_4BIT;
    }
    else if ( paletteTypeIn == MAGIC_PALETTE_TYPE::PALETTE_8BIT )
    {
        paletteTypeOut = rw::PALETTE_8BIT;
    }
    else if ( paletteTypeIn == MAGIC_PALETTE_TYPE::PALETTE_4BIT_LSB )
    {
        paletteTypeOut = rw::PALETTE_4BIT_LSB;
    }
    else if ( paletteTypeIn == MAGIC_PALETTE_TYPE::PALETTE_NONE )
    {
        paletteTypeOut = rw::PALETTE_NONE;
    }
    else
    {
        paletteTypeOut = rw::PALETTE_NONE;
    }
}

inline void MagicMapToVirtualRasterFormat( rw::eRasterFormat formatIn, MAGIC_RASTER_FORMAT& formatOut )
{
    if ( formatIn == rw::RASTER_DEFAULT )
    {
        formatOut = MAGIC_RASTER_FORMAT::RASTER_DEFAULT;
    }
    else if ( formatIn == rw::RASTER_1555 )
    {
        formatOut = MAGIC_RASTER_FORMAT::RASTER_1555;
    }
    else if ( formatIn == rw::RASTER_565 )
    {
        formatOut = MAGIC_RASTER_FORMAT::RASTER_565;
    }
    else if ( formatIn == rw::RASTER_4444 )
    {
        formatOut = MAGIC_RASTER_FORMAT::RASTER_4444;
    }
    else if ( formatIn == rw::RASTER_LUM )
    {
        formatOut = MAGIC_RASTER_FORMAT::RASTER_LUM;
    }
    else if ( formatIn == rw::RASTER_8888 )
    {
        formatOut = MAGIC_RASTER_FORMAT::RASTER_8888;
    }
    else if ( formatIn == rw::RASTER_888 )
    {
        formatOut = MAGIC_RASTER_FORMAT::RASTER_888;
    }
    else if ( formatIn == rw::RASTER_555 )
    {
        formatOut = MAGIC_RASTER_FORMAT::RASTER_555;
    }
    else
    {
        formatOut = MAGIC_RASTER_FORMAT::RASTER_DEFAULT;
    }
}

inline void MagicMapToVirtualColorOrdering( rw::eColorOrdering colorOrderIn, MAGIC_COLOR_ORDERING& colorOrderOut )
{
    if ( colorOrderIn == rw::COLOR_RGBA )
    {
        colorOrderOut = MAGIC_COLOR_ORDERING::COLOR_RGBA;
    }
    else if ( colorOrderIn == rw::COLOR_BGRA )
    {
        colorOrderOut = MAGIC_COLOR_ORDERING::COLOR_BGRA;
    }
    else if ( colorOrderIn == rw::COLOR_ABGR )
    {
        colorOrderOut = MAGIC_COLOR_ORDERING::COLOR_ABGR;
    }
    else
    {
        colorOrderOut = MAGIC_COLOR_ORDERING::COLOR_RGBA;
    }
}

inline void MagicMapToVirtualPaletteType( rw::ePaletteType paletteTypeIn, MAGIC_PALETTE_TYPE& paletteTypeOut )
{
    if ( paletteTypeIn == rw::PALETTE_NONE )
    {
        paletteTypeOut = MAGIC_PALETTE_TYPE::PALETTE_NONE;
    }
    else if ( paletteTypeIn == rw::PALETTE_4BIT )
    {
        paletteTypeOut = MAGIC_PALETTE_TYPE::PALETTE_4BIT;
    }
    else if ( paletteTypeIn == rw::PALETTE_8BIT )
    {
        paletteTypeOut = MAGIC_PALETTE_TYPE::PALETTE_8BIT;
    }
    else if ( paletteTypeIn == rw::PALETTE_4BIT_LSB )
    {
        paletteTypeOut = MAGIC_PALETTE_TYPE::PALETTE_4BIT_LSB;
    }
    else
    {
        paletteTypeOut = MAGIC_PALETTE_TYPE::PALETTE_NONE;
    }
}
