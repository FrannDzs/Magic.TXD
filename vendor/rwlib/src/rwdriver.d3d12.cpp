/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.d3d12cpp
*  PURPOSE:     D3D12 driver implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwdriver.d3d12.hxx"

#include "pluginutil.hxx"

namespace rw
{

#ifndef _COMPILE_FOR_LEGACY

#ifdef _WIN32

d3d12DriverInterface::d3d12NativeDriver::d3d12NativeDriver( d3d12DriverInterface *env, Interface *intf )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    this->engineInterface = engineInterface;

    // Initialize the Direct3D 12 device.
    HRESULT deviceSuccess =
        env->D3D12CreateDevice(
            nullptr,
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS( &this->m_device )
        );

    if ( !SUCCEEDED(deviceSuccess) )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_DEVICEHANDLEFAIL" );
    }

    // Cache some things.
    this->m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

    // Success!
}

d3d12DriverInterface::d3d12NativeDriver::~d3d12NativeDriver( void )
{
    // We automatically release main device handles.
    // TODO: release advanced device resources (rasters, geometries, etc)
}

static optional_struct_space <PluginDependantStructRegister <d3d12DriverInterface, RwInterfaceFactory_t>> d3d12DriverReg;

#endif //_WIN32

#endif //_COMPILE_FOR_LEGACY

void registerD3D12DriverImplementation( void )
{
#ifndef _COMPILE_FOR_LEGACY
#ifdef _WIN32
    // Put the driver into the ecosystem.
    d3d12DriverReg.Construct( engineFactory );

    // TODO: register all driver plugins.
#endif //_WIN32
#endif //_COMPILE_FOR_LEGACY
}

void unregisterD3D12DriverImplementation( void )
{
#ifndef _COMPILE_FOR_LEGACY
#ifdef _WIN32
    d3d12DriverReg.Destroy();
#endif //_WIN32
#endif //_COMPILE_FOR_LEGACY
}

};
