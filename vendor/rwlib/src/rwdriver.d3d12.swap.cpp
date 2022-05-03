/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.d3d12.swap.cpp
*  PURPOSE:     D3D12 swap chain management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifndef _COMPILE_FOR_LEGACY

#ifdef _WIN32

#include "rwdriver.d3d12.hxx"

// We need to know about the system window type.
#include "rwwindowing.hxx"

namespace rw
{

static void _swapchain_window_resize_event( RwObject *evtObj, event_t eventID, void *cbData, void *ud )
{
    d3d12DriverInterface::d3d12SwapChain *swapChain = (d3d12DriverInterface::d3d12SwapChain*)ud;

    scoped_rwlock_writer <> ctxUpdateBuffers( swapChain->lockBackBufferValidity );

    // TODO: recreate the swap chain buffers, because the window has resized.

    swapChain->updateBackBuffers();
}

void d3d12DriverInterface::d3d12SwapChain::releaseFrameResources( void )
{
    // IMPORTANT: only do under write lock of lockBackBufferValidity!

    // Clear all buffer references.
    size_t numFrames = this->m_chainBuffers.GetCount();

    for ( size_t n = 0; n < numFrames; n++ )
    {
        this->m_chainBuffers[n].Reset();
    }
}

void d3d12DriverInterface::d3d12SwapChain::bindFrameResources( void )
{
    // IMPORTANT: only do under write-lock of lockBackBufferValidity!

    UINT frameCount = (UINT)this->m_chainBuffers.GetCount();

    ID3D12Device *device = this->driver->m_device.Get();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle( m_rtvHeap->GetCPUDescriptorHandleForHeapStart() );

    // Create a RTV for each frame.
    for (UINT n = 0; n < frameCount; n++)
    {
        HRESULT bufferGetSuccess = m_swapChain->GetBuffer( n, IID_PPV_ARGS(&m_chainBuffers[n]) );

        if ( !SUCCEEDED(bufferGetSuccess) )
        {
            throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_GETSWAPCHAINRTSURF" );
        }

        device->CreateRenderTargetView( m_chainBuffers[n].Get(), nullptr, rtvHandle );

        // Increment the handle.
        rtvHandle.Offset( 1, driver->m_rtvDescriptorSize );
    }
}

void d3d12DriverInterface::d3d12SwapChain::updateBackBuffers( void )
{
    UINT newWidth = (UINT)this->sysWnd->GetClientWidth();
    UINT newHeight = (UINT)this->sysWnd->GetClientHeight();

    // First release resources.
    this->releaseFrameResources();

    HRESULT success = this->m_swapChain->ResizeBuffers( 0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, 0 );

    if ( SUCCEEDED(success) == false )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_SWAPCHAINRESIZE" );
    }

    // TODO: what if the resizing fails? how do we reinstate resources?

    // Reinstate resources again.
    this->bindFrameResources();
}

void d3d12DriverInterface::d3d12SwapChain::present( bool vsync )
{
    // Lock our stuff for safety.
    scoped_rwlock_reader <> ctxPresentBuffer( this->lockBackBufferValidity );

    // Decide for how many vertical blanks to wait.
    UINT numVertBlanks = 0;

    if ( vsync )
    {
        numVertBlanks = 1;
    }

    this->m_swapChain->Present( numVertBlanks, 0 );

    // We do not care if present fails.
}

d3d12DriverInterface::d3d12SwapChain::d3d12SwapChain( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, Window *sysWnd, uint32 frameCount, d3d12CmdQueue *cmdQueue )
{
    this->driver = driver;
    this->sysWnd = sysWnd;

    CreateDXGIFactory1_t factCreate = env->CreateDXGIFactory1;

    if ( !factCreate )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_DXGIENTRYNOTFOUND" );
    }

    // We need to initialize a factory to create the swap chain on.
    ComPtr <IDXGIFactory1> dxgiFact;

    HRESULT factorySuccess = factCreate( IID_PPV_ARGS( &dxgiFact ) );

    if ( !SUCCEEDED(factorySuccess) )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_DXGIFACTCREATE" );
    }

    // Get the native interface of the window.
    windowingSystemWin32::Win32Window *wndNative = (windowingSystemWin32::Win32Window*)sysWnd;

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = frameCount;
    swapChainDesc.BufferDesc.Width = sysWnd->GetClientWidth();
    swapChainDesc.BufferDesc.Height = sysWnd->GetClientHeight();
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.OutputWindow = wndNative->windowHandle;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;

    // Create the swap chain.
    ComPtr <IDXGISwapChain> abstractSwapChain;

    HRESULT swapChainSuccess =
        dxgiFact->CreateSwapChain(
            cmdQueue->natQueue.Get(),
            &swapChainDesc,
            &abstractSwapChain
        );

    if ( !SUCCEEDED(swapChainSuccess) )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_SWAPCHAINFAIL" );
    }

    HRESULT castToNewSwapChain =
        abstractSwapChain.As( &m_swapChain );

    if ( !SUCCEEDED(castToNewSwapChain) )
    {
        throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_SWAPCHAINNEWCAST" );
    }

    // Initialize the hardware buffers.
    this->m_chainBuffers.Resize( frameCount );

    ID3D12Device *device = driver->m_device.Get();

    // Create a resource view heap just for this swap chain.
    // We could do this more efficiently in the future with a view heap manager, but we want to get something working done first!
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = frameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        HRESULT heapCreateSuccess = device->CreateDescriptorHeap( &rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap) );

        if ( !SUCCEEDED(heapCreateSuccess) )
        {
            throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_SWAPCHAIN_RTVDESCFAIL" );
        }
    }

    // Create the frame resources.
    this->bindFrameResources();

    // Need to keep back buffer valid.
    this->lockBackBufferValidity = CreateReadWriteLock( engineInterface );

    // If the window is resized, we want to know about it.
    RegisterEventHandler( sysWnd, event_t::WINDOW_RESIZE, _swapchain_window_resize_event, this );
}

d3d12DriverInterface::d3d12SwapChain::~d3d12SwapChain( void )
{
    Interface *engineInterface = this->driver->engineInterface;

    // Unregister our event handler.
    UnregisterEventHandler( this->sysWnd, event_t::WINDOW_RESIZE, _swapchain_window_resize_event );

    // Delete the lock.
    CloseReadWriteLock( engineInterface, this->lockBackBufferValidity );

    // Resources are released automatically.
}

};

#endif //_WIN32

#endif //_COMPILE_FOR_LEGACY
