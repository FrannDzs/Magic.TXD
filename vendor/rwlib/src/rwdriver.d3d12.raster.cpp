/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.d3d12.raster.cpp
*  PURPOSE:     D3D12 raster management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifndef _COMPILE_FOR_LEGACY

#ifdef _WIN32

#include "rwdriver.d3d12.hxx"

namespace rw
{

void d3d12DriverInterface::RasterInstance( Interface *engineInterface, void *driverObjMem, void *objMem, Raster *sysRaster )
{
    if ( sysRaster->hasNativeDataOfType( "Direct3D9" ) == false )
    {
        throw DriverRasterInvalidParameterException( "Direct3D12", AcquireRaster( sysRaster ), L"RASTER_FRIENDLYNAME", L"D3D12_REASON_UNSUPPNATTEXTYPE" );
    }

    // TODO.
}

void d3d12DriverInterface::RasterUninstance( Interface *engineInterface, void *driverObjMem, void *objMem )
{
    // TODO.
}

}

#endif //_WIN32

#endif //_COMPILE_FOR_LEGACY
