/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.d3d12.gpumem.cpp
*  PURPOSE:     D3D12 GPU memory management.
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

inline D3D12_HEAP_TYPE getNativeMemoryType( eDriverMemType memType )
{
    switch( memType )
    {
    case eDriverMemType::DISCRETE: return D3D12_HEAP_TYPE_DEFAULT;
    case eDriverMemType::UPLOAD: return D3D12_HEAP_TYPE_UPLOAD;
    }

    throw DriverInvalidParameterException( "Direct3D12", L"DRIVER_MEMTYPE_FRIENDLYNAME", nullptr );
}

inline D3D12_HEAP_FLAGS getNativeHeapFlags( const gpuMemProperties& memProps )
{
    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;

    if ( !memProps.canHaveBuffers )
    {
        flags |= D3D12_HEAP_FLAG_DENY_BUFFERS;
    }

    if ( !memProps.canHaveNonRenderSurfaces )
    {
        flags |= D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
    }

    if ( !memProps.canHaveRenderSurfaces )
    {
        flags |= D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
    }

    if ( memProps.canHaveSwapchains )
    {
        flags |= D3D12_HEAP_FLAG_ALLOW_DISPLAY;
    }

    return flags;
}

d3d12DriverInterface::d3d12GPUMem::d3d12GPUMem( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, eDriverMemType memType, size_t numBytes, const gpuMemProperties& memProps )
{
    D3D12_HEAP_TYPE natMemType = getNativeMemoryType( memType );
    SIZE_T natNumBytes = (SIZE_T)numBytes;

    D3D12_HEAP_DESC natHeapInfo;
    natHeapInfo.SizeInBytes = natNumBytes;
    natHeapInfo.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    natHeapInfo.Flags = getNativeHeapFlags( memProps );
    natHeapInfo.Properties.Type = natMemType;
    natHeapInfo.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
    natHeapInfo.Properties.CreationNodeMask = 0;
    natHeapInfo.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    natHeapInfo.Properties.VisibleNodeMask = 0;

    HRESULT success = driver->m_device->CreateHeap( &natHeapInfo, IID_PPV_ARGS(&this->natHeap) );

    if ( !SUCCEEDED(success) )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_MEMFAIL" );
    }
}

d3d12DriverInterface::d3d12GPUMem::~d3d12GPUMem( void )
{
    // Nothing to do here.
    return;
}

void d3d12DriverInterface::GPUMemGetResSurfaceInfo( const ResourceSurfaceFormat& surfDesc, ResourceSurfaceInfo& infoOut ) const
{
    // Need to calculate correct resource surface properties.
    // They are used to allocate a memory block in any GPU heap.
    gfxRasterFormat surfUnitType = surfDesc.resUnitFormat;

    if ( isDriverRasterFormatRawSample( surfUnitType ) )
    {
        // Raw samples form an array of texels as texture.
        // There is a nicely defined depth per sample.
        size_t depthPerSample = getDriverRasterFormatSampleDepth( surfUnitType );

        //TODO: what is the difference between native ordering and linear ordering?
        // are the dimensions of native ordering constrained to rules?
    }
    else if ( surfUnitType == gfxRasterFormat::DXT1 ||
              surfUnitType == gfxRasterFormat::DXT3 ||
              surfUnitType == gfxRasterFormat::DXT5 )
    {

    }
}

}

#endif //_WIN32

#endif //_COMPILE_FOR_LEGACY
