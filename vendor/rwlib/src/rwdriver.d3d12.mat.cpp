/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.d3d12.mat.cpp
*  PURPOSE:     D3D12 material management.
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

void d3d12DriverInterface::MaterialInstance( Interface *engineInterface, void *driverObjMem, void *objMem, Material *sysMat )
{

}

void d3d12DriverInterface::MaterialUninstance( Interface *engineInterface, void *driverObjMem, void *objMem )
{

}

};

#endif //_WIN32

#endif //_COMPILE_FOR_LEGACY
