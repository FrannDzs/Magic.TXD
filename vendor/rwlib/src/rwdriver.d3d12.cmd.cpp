/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.d3d12.cmd.cpp
*  PURPOSE:     D3D12 command buffer management.
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

d3d12DriverInterface::d3d12CmdBuffer::d3d12CmdBuffer( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, d3d12CmdAllocator *allocMan, eCmdBufType bufType )
{
    this->driver = driver;

    // Map the command buffer type to the D3D12 variant.
    D3D12_COMMAND_LIST_TYPE list_type;

    if ( bufType == eCmdBufType::GRAPHICS )
    {
        list_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
    else if ( bufType == eCmdBufType::COMPUTE )
    {
        list_type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    }
    else if ( bufType == eCmdBufType::COPY )
    {
        list_type = D3D12_COMMAND_LIST_TYPE_COPY;
    }
    else
    {
        throw DriverInvalidParameterException( "Direct3D12", L"D3D12_CMDLISTTYPE_FRIENDLYNAME", nullptr );
    }

    // Just create the object.
    ID3D12Device *natDriver = driver->m_device.Get();

    HRESULT success =
        natDriver->CreateCommandList(
            0,  // TODO: allow setting special node masks for multi-GPU scenarios.
            list_type,
            allocMan->allocMan.Get(),
            nullptr,   // maybe.
            IID_PPV_ARGS(&this->cmdList)
        );

    if ( SUCCEEDED(success) == false )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_CMDLISTFAIL" );
    }
}

d3d12DriverInterface::d3d12CmdBuffer::~d3d12CmdBuffer( void )
{
    // Resources are being released automatically.
}

d3d12DriverInterface::d3d12CmdAllocator::d3d12CmdAllocator( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, eCmdAllocType allocType )
{
    this->driver = driver;

    // Map the framework allocator type to D3D12 native type.
    D3D12_COMMAND_LIST_TYPE alloc_type;

    if ( allocType == eCmdAllocType::DIRECT )
    {
        alloc_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
    else
    {
        throw DriverInvalidParameterException( "Direct3D12", L"DRIVER_CMDALLOCTYPE_FRIENDLYNAME", nullptr );
    }

    // Just create the object.
    ID3D12Device *natDriver = driver->m_device.Get();

    HRESULT success = natDriver->CreateCommandAllocator( alloc_type, IID_PPV_ARGS(&this->allocMan) );

    if ( SUCCEEDED(success) == false )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_CMDALLOCFAIL" );
    }
}

d3d12DriverInterface::d3d12CmdAllocator::~d3d12CmdAllocator( void )
{
    // Resources are being released automatically.
}

d3d12DriverInterface::d3d12CmdQueue::d3d12CmdQueue( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, eCmdBufType queueType, int priority )
{
    this->driver = driver;

    // Translate the framework queueType into a native value.
    D3D12_COMMAND_LIST_TYPE queue_type;

    if ( queueType == eCmdBufType::GRAPHICS )
    {
        queue_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
    else if ( queueType == eCmdBufType::COMPUTE )
    {
        queue_type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    }
    else if ( queueType == eCmdBufType::COPY )
    {
        queue_type = D3D12_COMMAND_LIST_TYPE_COPY;
    }
    else
    {
        throw DriverInvalidParameterException( "Direct3D12", L"DRIVER_CMDBUFTYPE_FRIENDLYNAME", nullptr );
    }

    // Create the requested queue.
    ID3D12Device *natDriver = driver->m_device.Get();

    D3D12_COMMAND_QUEUE_DESC queueDesc;
    queueDesc.Type = queue_type;
    queueDesc.NodeMask = 0;
    queueDesc.Priority = priority;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    HRESULT success =
        natDriver->CreateCommandQueue( &queueDesc, IID_PPV_ARGS(&this->natQueue) );

    if ( SUCCEEDED(success) == false )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_CMDQUEUEFAIL" );
    }
}

d3d12DriverInterface::d3d12CmdQueue::~d3d12CmdQueue( void )
{
    // Resources are being released automatically.
}

d3d12DriverInterface::d3d12Fence::d3d12Fence( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, uint64 initValue )
{
    this->driver = driver;

    // Fences are really simple objects.
    ID3D12Device *natDriver = driver->m_device.Get();

    HRESULT success = natDriver->CreateFence( initValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&this->natFence) );

    if ( SUCCEEDED(success) == false )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_FENCEFAIL" );
    }
}

d3d12DriverInterface::d3d12Fence::~d3d12Fence( void )
{
    // Resources are being released automatically.
}

};

#endif //_WIN32

#endif //_COMPILE_FOR_LEGACY
