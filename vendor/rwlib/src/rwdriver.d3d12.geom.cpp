/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.d3d12.cmd.cpp
*  PURPOSE:     D3D12 geometry implementation
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

void d3d12DriverInterface::GeometryInstance( Interface *engineInterface, void *driverObjMem, void *objMem, Geometry *sysGeom )
{

}

void d3d12DriverInterface::GeometryUninstance( Interface *engineInterface, void *driverObjMem, void *objMem )
{

}

};

#endif //_WIN32

#endif //_COMPILE_FOR_LEGACY
