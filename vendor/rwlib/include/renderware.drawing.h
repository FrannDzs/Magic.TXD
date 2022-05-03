/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.drawing.h
*  PURPOSE:
*       RenderWare drawing system definitions.
*       Those are used to issue drawing commands to the environment.
*       Drawing environments execute under a specific context that keeps track of certain states.
*       There is a separate 2D context and a separate 3D context.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_DRAWING_DEFINES_
#define _RENDERWARE_DRAWING_DEFINES_

namespace rw
{

// RenderState enums.
// The implementation has to map these to internal device states.
enum eRwDeviceCmd : uint32
{
    RWSTATE_COMBINEDTEXADDRESSMODE,
    RWSTATE_UTEXADDRESSMODE,
    RWSTATE_VTEXADDRESSMODE,
    RWSTATE_WTEXADDRESSMODE,
    RWSTATE_ZTESTENABLE,
    RWSTATE_ZWRITEENABLE,
    RWSTATE_TEXFILTER,
    RWSTATE_SRCBLEND,
    RWSTATE_DSTBLEND,
    RWSTATE_ALPHABLENDENABLE,
    RWSTATE_CULLMODE,
    RWSTATE_STENCILENABLE,
    RWSTATE_STENCILFAIL,
    RWSTATE_STENCILZFAIL,
    RWSTATE_STENCILPASS,
    RWSTATE_STENCILFUNC,
    RWSTATE_STENCILREF,
    RWSTATE_STENCILMASK,
    RWSTATE_STENCILWRITEMASK,
    RWSTATE_ALPHAFUNC,
    RWSTATE_ALPHAREF
};

// Definition of a 2D drawing layer.
struct DrawingLayer2D
{
    // Basic render state setting function.
    enum eRwDeviceCmd : uint32
    {
        RWSTATE_FIRST,

        RWSTATE_UTEXADDRESSMODE,
        RWSTATE_VTEXADDRESSMODE,
        RWSTATE_ZWRITEENABLE,
        RWSTATE_TEXFILTER,
        RWSTATE_SRCBLEND,
        RWSTATE_DSTBLEND,
        RWSTATE_ALPHABLENDENABLE,
        RWSTATE_ALPHAFUNC,
        RWSTATE_ALPHAREF,

        RWSTATE_MAX
    };

    bool SetRenderState( eRwDeviceCmd cmd, rwDeviceValue_t value );
    bool GetRenderState( eRwDeviceCmd cmd, rwDeviceValue_t& value ) const;

    // Advanced resource functions.
    bool SetRaster( Raster *theRaster );
    Raster* GetRaster( void ) const;

    // Context acquisition.
    void Begin();
    void End();

    // Drawing commands.
    void DrawRect( uint32 x, uint32 y, uint32 width, uint32 height );
    void DrawLine( uint32 x, uint32 y, uint32 end_x, uint32 end_y );
    void DrawPoint( uint32 x, uint32 y );
};

// Drawing context creation API.
DrawingLayer2D* CreateDrawingLayer2D( Driver *usedDriver );
void DeleteDrawingLayer2D( DrawingLayer2D *layer );

} // namespace rw

#endif //_RENDERWARE_DRAWING_DEFINES_