/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.d3d12.hxx
*  PURPOSE:     Main internal header of the Direct3D 12 driver implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_DIRECT3D12_DRIVER_
#define _RENDERWARE_DIRECT3D12_DRIVER_

// Only include if we do not compile for legacy.
#ifndef _COMPILE_FOR_LEGACY

// Only supported under Win32.
#ifdef _WIN32

#include "rwdriver.hxx"

#include "rwthreading.hxx"

#define NOMINMAX
#include <dxgi.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dx12.h>

#include <wrl/client.h>

using namespace Microsoft::WRL;

namespace rw
{

struct d3d12DriverInterface : public nativeDriverImplementation
{
    // Thread-local environment of D3D12 drivers.
    struct threadLocalEnv
    {
        inline threadLocalEnv( void )
        {
            evtWaitForCmdBuf = CreateEventW( nullptr, FALSE, FALSE, nullptr );
        }

        inline ~threadLocalEnv( void )
        {
            CloseHandle( evtWaitForCmdBuf );
        }

        HANDLE evtWaitForCmdBuf;        // used by command buffers to wait for them.
    };
    perThreadDataRegister <threadLocalEnv, false> threadLocalEnvPlugin;

    // Driver object definitions.
    // Constructed by a factory for plugin support.
    struct d3d12NativeDriver
    {
        Interface *engineInterface;

        d3d12NativeDriver( d3d12DriverInterface *env, Interface *engineInterface );
        d3d12NativeDriver( const d3d12NativeDriver& right ) = delete;

        ~d3d12NativeDriver( void );

	    ComPtr<ID3D12Device> m_device;                      // DIRECT3D12!!!!

        // Cached values.
        UINT m_rtvDescriptorSize;
    };

    typedef StaticPluginClassFactory <d3d12NativeDriver, RwDynMemAllocator, rwEirExceptionManager> d3d12DriverFactory_t;

    d3d12DriverFactory_t d3d12DriverFactory;

    struct d3d12NativeRaster
    {
        d3d12NativeDriver *driver;

        inline d3d12NativeRaster( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver )
        {
            this->driver = driver;
        }

        inline d3d12NativeRaster( const d3d12NativeRaster& right )
        {
            this->driver = right.driver;
        }

        inline ~d3d12NativeRaster( void )
        {

        }
    };

    struct d3d12NativeGeometry
    {
        d3d12NativeDriver *driver;

        inline d3d12NativeGeometry( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver )
        {
            this->driver = driver;
        }

        inline d3d12NativeGeometry( const d3d12NativeGeometry& right )
        {
            this->driver = right.driver;
        }

        inline ~d3d12NativeGeometry( void )
        {

        }
    };

    struct d3d12NativeMaterial
    {
        d3d12NativeDriver *driver;

        inline d3d12NativeMaterial( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver )
        {
            this->driver = driver;
        }

        inline d3d12NativeMaterial( const d3d12NativeMaterial& right )
        {
            this->driver = right.driver;
        }

        inline ~d3d12NativeMaterial( void )
        {

        }
    };

    struct d3d12CmdQueue;

    struct d3d12SwapChain
    {
        d3d12NativeDriver *driver;

        d3d12SwapChain( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, Window *sysWnd, uint32 frameCount, d3d12CmdQueue *cmdQueue );
        d3d12SwapChain( const d3d12SwapChain& right ) = delete;

        ~d3d12SwapChain( void );

        void releaseFrameResources( void );
        void bindFrameResources( void );

        void updateBackBuffers( void );
        void present( bool vsync );

        Window *sysWnd;

        ComPtr <IDXGISwapChain3> m_swapChain;
	    ComPtr <ID3D12DescriptorHeap> m_rtvHeap;
        rwStaticVector <ComPtr <ID3D12Resource>> m_chainBuffers;

        // Since swap chains are event-based objects we must synchronize.
        rwlock *lockBackBufferValidity;
    };

    struct d3d12PipelineStateObject
    {
        d3d12NativeDriver *driver;

        // Makes no sense to clone an immutable object, so no copy constructor.

        d3d12PipelineStateObject( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, const gfxGraphicsState& psoState );
        ~d3d12PipelineStateObject( void );

        ComPtr <ID3D12RootSignature> rootSignature;
        ComPtr <ID3D12PipelineState> psoPtr;
    };

    struct d3d12CmdAllocator
    {
        d3d12NativeDriver *driver;

        d3d12CmdAllocator( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, eCmdAllocType allocType );
        ~d3d12CmdAllocator( void );

        ComPtr <ID3D12CommandAllocator> allocMan;
    };

    struct d3d12CmdBuffer
    {
        d3d12NativeDriver *driver;

        d3d12CmdBuffer( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, d3d12CmdAllocator *allocMan, eCmdBufType bufType );
        ~d3d12CmdBuffer( void );

        ComPtr <ID3D12GraphicsCommandList> cmdList;
    };

    struct d3d12CmdQueue
    {
        d3d12NativeDriver *driver;

        d3d12CmdQueue( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, eCmdBufType queueType, int priority );
        ~d3d12CmdQueue( void );

        ComPtr <ID3D12CommandQueue> natQueue;
    };

    struct d3d12Fence
    {
        d3d12NativeDriver *driver;

        d3d12Fence( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, uint64 initValue );
        ~d3d12Fence( void );

        ComPtr <ID3D12Fence> natFence;
    };

    struct d3d12GPUMem
    {
        d3d12NativeDriver *driver;

        d3d12GPUMem( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, eDriverMemType memType, size_t numBytes, const gpuMemProperties& memProps );
        ~d3d12GPUMem( void );

        ComPtr <ID3D12Heap> natHeap;
    };

    // Special entry points.
    typedef HRESULT (WINAPI*D3D12CreateDevice_t)(
        IUnknown *pAdapter,
        D3D_FEATURE_LEVEL MinimumFeatureLevel,
        REFIID riid,
        void **ppDevice
    );
    typedef HRESULT (WINAPI*CreateDXGIFactory1_t)(
        REFIID riid,
        void **ppFactory
    );
#ifdef _DEBUG
    typedef HRESULT (WINAPI*D3D12GetDebugInterface_t)( REFIID riif, void **ppvDebug );
#endif //_DEBUG
    typedef HRESULT (WINAPI*D3D12SerializeRootSignature_t)(
        const D3D12_ROOT_SIGNATURE_DESC *pRootSignature,
        D3D_ROOT_SIGNATURE_VERSION Version,
        ID3DBlob **ppBlob,
        ID3DBlob **ppErrorBlob
    );

    HMODULE d3d12Module;
    HMODULE dxgiModule;

    D3D12CreateDevice_t D3D12CreateDevice;
    CreateDXGIFactory1_t CreateDXGIFactory1;
#ifdef _DEBUG
    D3D12GetDebugInterface_t D3D12GetDebugInterface;
#endif //_DEBUG
    D3D12SerializeRootSignature_t D3D12SerializeRootSignature;

    struct driverFactoryConstructor
    {
        inline driverFactoryConstructor( d3d12DriverInterface *env, Interface *engineInterface )
        {
            this->env = env;
            this->engineInterface = engineInterface;
        }

        inline d3d12NativeDriver* Construct( void *mem ) const
        {
            return new (mem) d3d12NativeDriver( this->env, this->engineInterface );
        }

        d3d12DriverInterface *env;
        Interface *engineInterface;
    };

    // Driver object construction.
    void OnDriverConstruct( Interface *engineInterface, void *driverObjMem, size_t driverMemSize ) override
    {
        driverFactoryConstructor constructor( this, engineInterface );

        d3d12DriverFactory.ConstructPlacementEx( driverObjMem, constructor );
    }

    void OnDriverCopyConstruct( Interface *engineInterface, void *driverObjMem, const void *srcDriverObjMem, size_t driverMemSize ) override
    {
        d3d12DriverFactory.ClonePlacement( driverObjMem, (const d3d12NativeDriver*)srcDriverObjMem );
    }

    void OnDriverDestroy( Interface *engineInterface, void *driverObjMem, size_t driverMemSize ) override
    {
        d3d12DriverFactory.DestroyPlacement( (d3d12NativeDriver*)driverObjMem );
    }

    // Driver API.
    void* DriverGetNativeHandle( void *driverMem ) override
    {
        d3d12NativeDriver *driver = (d3d12NativeDriver*)driverMem;

        return driver->m_device.Get();
    }

    // Object construction.
    NATIVE_DRIVER_OBJ_CONSTRUCT_IMPL( Raster, d3d12NativeRaster, d3d12NativeDriver );
    NATIVE_DRIVER_OBJ_CONSTRUCT_IMPL( Geometry, d3d12NativeGeometry, d3d12NativeDriver );
    NATIVE_DRIVER_OBJ_CONSTRUCT_IMPL( Material, d3d12NativeMaterial, d3d12NativeDriver );

    // Object instancing forward declarations.
    NATIVE_DRIVER_DEFINE_INSTANCING_FORWARD( Raster );
    NATIVE_DRIVER_DEFINE_INSTANCING_FORWARD( Geometry );
    NATIVE_DRIVER_DEFINE_INSTANCING_FORWARD( Material );

    // Special swap chain object.
    NATIVE_DRIVER_SWAPCHAIN_CONSTRUCT() override
    {
        new (objMem) d3d12SwapChain( this, engineInterface, (d3d12NativeDriver*)driverObjMem, sysWnd, frameCount, (d3d12CmdQueue*)cmdQueueMem );
    }
    NATIVE_DRIVER_SWAPCHAIN_DESTROY() override
    {
        ((d3d12SwapChain*)objMem)->~d3d12SwapChain();
    }

    rwlock* SwapChainGetUsageLock( void *swapMem ) override
    {
        d3d12SwapChain *swapChain = (d3d12SwapChain*)swapMem;

        return swapChain->lockBackBufferValidity;
    }

    void SwapChainPresent( void *swapMem, bool vsync ) override
    {
        d3d12SwapChain *swapChain = (d3d12SwapChain*)swapMem;

        swapChain->present( vsync );
    }

    unsigned int SwapChainGetBackBufferIndex( void *swapMem ) override
    {
        d3d12SwapChain *swapChain = (d3d12SwapChain*)swapMem;

        return swapChain->m_swapChain->GetCurrentBackBufferIndex();
    }

    DriverPtr SwapChainGetBackBufferViewPointer( void *swapMem, unsigned int bufIdx ) override
    {
        d3d12SwapChain *swapChain = (d3d12SwapChain*)swapMem;

        d3d12NativeDriver *natDriver = swapChain->driver;

        D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc = swapChain->m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

        return ( (DriverPtr)cpuDesc.ptr + bufIdx * natDriver->m_rtvDescriptorSize );
    }

    // Taking care of the pipeline state object mappings.
    NATIVE_DRIVER_GRAPHICS_STATE_CONSTRUCT() override
    {
        new (objMem) d3d12PipelineStateObject( this, engineInterface, (d3d12NativeDriver*)driverObjMem, gfxState );
    }
    NATIVE_DRIVER_GRAPHICS_STATE_DESTROY() override
    {
        ((d3d12PipelineStateObject*)objMem)->~d3d12PipelineStateObject();
    }

    void* GraphicsStateGetNativeHandle( void *psoMem ) override
    {
        d3d12PipelineStateObject *psoObj = (d3d12PipelineStateObject*)psoMem;

        return psoObj->psoPtr.Get();
    }

    // Command allocator API.
    NATIVE_DRIVER_CMDALLOC_CONSTRUCT() override
    {
        new (objMem) d3d12CmdAllocator( this, engineInterface, (d3d12NativeDriver*)driverObjMem, allocType );
    }
    NATIVE_DRIVER_CMDALLOC_DESTROY() override
    {
        ( (d3d12CmdAllocator*)objMem )->~d3d12CmdAllocator();
    }

    bool CmdAllocReset( void *objMem ) override
    {
        d3d12CmdAllocator *allocMan = (d3d12CmdAllocator*)objMem;

        HRESULT success = allocMan->allocMan->Reset();

        return SUCCEEDED( success );
    }

    // Command buffer API.
    NATIVE_DRIVER_CMDBUF_CONSTRUCT() override
    {
        new (objMem) d3d12CmdBuffer( this, engineInterface, (d3d12NativeDriver*)driverObjMem, (d3d12CmdAllocator*)allocObjMem, bufType );
    }
    NATIVE_DRIVER_CMDBUF_DESTROY() override
    {
        ( (d3d12CmdBuffer*)objMem )->~d3d12CmdBuffer();
    }

    bool CmdBufClose( void *objMem ) override
    {
        d3d12CmdBuffer *cmdBuf = (d3d12CmdBuffer*)objMem;

        HRESULT success = cmdBuf->cmdList->Close();

        return SUCCEEDED(success);
    }

    bool CmdBufReset( void *objMem, void *cmdAllocMem, void *psoMem ) override
    {
        d3d12CmdBuffer *cmdBuf = (d3d12CmdBuffer*)objMem;
        d3d12CmdAllocator *cmdAlloc = (d3d12CmdAllocator*)cmdAllocMem;
        d3d12PipelineStateObject *psoObj = (d3d12PipelineStateObject*)psoMem;

        HRESULT success = cmdBuf->cmdList->Reset(
            cmdAlloc->allocMan.Get(),
            ( psoObj != nullptr ) ? ( psoObj->psoPtr.Get() ) : ( nullptr )
        );

        return SUCCEEDED(success);
    }

    void* CmdBufGetNativeHandle( void *objMem ) override
    {
        d3d12CmdBuffer *cmdBuf = (d3d12CmdBuffer*)objMem;

        return cmdBuf->cmdList.Get();
    }

    // Command queue API.
    NATIVE_DRIVER_CMDQUEUE_CONSTRUCT() override
    {
        new (objMem) d3d12CmdQueue( this, engineInterface, (d3d12NativeDriver*)driverObjMem, queueType, priority );
    }
    NATIVE_DRIVER_CMDQUEUE_DESTROY() override
    {
        ( (d3d12CmdQueue*)objMem )->~d3d12CmdQueue();
    }

    //TODO: more API of queue.
    bool CmdQueueSignal( void *objMem, void *fenceMem, uint64 valueToBe ) override
    {
        d3d12CmdQueue *cmdQueue = (d3d12CmdQueue*)objMem;
        d3d12Fence *fence = (d3d12Fence*)fenceMem;

        HRESULT success = cmdQueue->natQueue->Signal( fence->natFence.Get(), valueToBe );

        return SUCCEEDED(success);
    }

    bool CmdQueueExecuteCmdBuffer( void *objMem, void *cmdBufMem ) override
    {
        d3d12CmdQueue *cmdQueue = (d3d12CmdQueue*)objMem;
        d3d12CmdBuffer *cmdBuf = (d3d12CmdBuffer*)cmdBufMem;

        ID3D12CommandList *cmdListExec[] = { cmdBuf->cmdList.Get() };

        cmdQueue->natQueue->ExecuteCommandLists( _countof(cmdListExec), cmdListExec );
        return true;
    }

    NATIVE_DRIVER_FENCE_CONSTRUCT() override
    {
        new (objMem) d3d12Fence( this, engineInterface, (d3d12NativeDriver*)driverObjMem, initialValue );
    }
    NATIVE_DRIVER_FENCE_DESTROY() override
    {
        ( (d3d12Fence*)objMem )->~d3d12Fence();
    }

    // Synchronization API.
    void FenceSignal( void *objMem, uint64 newValue ) override
    {
        d3d12Fence *fence = (d3d12Fence*)objMem;

        fence->natFence->Signal( newValue );
    }

    uint64 FenceGetCurrentValue( void *objMem ) override
    {
        d3d12Fence *fence = (d3d12Fence*)objMem;

        return fence->natFence->GetCompletedValue();
    }

    void FenceSingleWaitForSignal( void *objMem, uint64 waitValue, uint32 timeout ) override
    {
        d3d12Fence *fence = (d3d12Fence*)objMem;
        
        if ( threadLocalEnv *threadEnv = threadLocalEnvPlugin.GetCurrentPluginStruct() )
        {
            HANDLE waitHandle = threadEnv->evtWaitForCmdBuf;

            fence->natFence->SetEventOnCompletion( waitValue, waitHandle );

            DWORD waitTime;

            if ( timeout >= RW_DRIVER_FENCE_NO_TIMEOUT )
            {
                waitTime = INFINITE;
            }
            else
            {
                waitTime = timeout;
            }

            WaitForSingleObject( waitHandle, waitTime );
        }
    }

    void* FenceGetNativeHandle( void *objMem ) override
    {
        d3d12Fence *fence = (d3d12Fence*)objMem;

        return fence->natFence.Get();
    }

    // GPU memory API.
    NATIVE_DRIVER_GPUMEM_CONSTRUCT() override
    {
        new (objMem) d3d12GPUMem( this, engineInterface, (d3d12NativeDriver*)driverObjMem, memType, numBytes, memProps );
    }

    NATIVE_DRIVER_GPUMEM_DESTROY() override
    {
        ( (d3d12GPUMem*)objMem )->~d3d12GPUMem();
    }

    void GPUMemGetResSurfaceInfo( const ResourceSurfaceFormat& surfProps, ResourceSurfaceInfo& infoOut ) const override;

    bool hasRegisteredDriver;

    inline void InitializeD3D12Module( void )
    {
        this->d3d12Module = LoadLibraryA( "d3d12.dll" );

        if ( HMODULE theModule = this->d3d12Module )
        {
            // Initialize the Direct3D environment.
            this->D3D12CreateDevice = (D3D12CreateDevice_t)GetProcAddress( theModule, "D3D12CreateDevice" );
#ifdef _DEBUG
            this->D3D12GetDebugInterface = (D3D12GetDebugInterface_t)GetProcAddress( theModule, "D3D12GetDebugInterface" );
#endif //_DEBUG
            this->D3D12SerializeRootSignature = (D3D12SerializeRootSignature_t)GetProcAddress( theModule, "D3D12SerializeRootSignature" );
        }
        else
        {
            this->D3D12CreateDevice = nullptr;
#ifdef _DEBUG
            this->D3D12GetDebugInterface = nullptr;
#endif //_DEBUG
            this->D3D12SerializeRootSignature = nullptr;
        }
    }

    inline void InitializeDXGIModule( void )
    {
        this->dxgiModule = LoadLibraryA( "dxgi.dll" );

        if ( HMODULE theModule = this->dxgiModule )
        {
            this->CreateDXGIFactory1 = (CreateDXGIFactory1_t)GetProcAddress( theModule, "CreateDXGIFactory1" );
        }
        else
        {
            this->CreateDXGIFactory1 = nullptr;
        }
    }

    inline d3d12DriverInterface( EngineInterface *engineInterface ) : d3d12DriverFactory( eir::constr_with_alloc::DEFAULT, engineInterface )
    {
        return;
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        bool hasRegistered = false;

        // Create the thread local environment.
        threadLocalEnvPlugin.Initialize( engineInterface );

        // Initialize modules.
        this->InitializeD3D12Module();
        this->InitializeDXGIModule();

        // We have to be able to create a Direct3D 12 device to do things.
        if ( this->D3D12CreateDevice != nullptr )
        {
#if defined(_DEBUG) || defined(RW_ENABLE_GFX_NATIVE_DEBUG)
            // Enable debugging layer, if available.
            if ( D3D12GetDebugInterface_t debugfunc = this->D3D12GetDebugInterface )
            {
                ComPtr<ID3D12Debug> debugController;

                HRESULT debugGet = debugfunc( IID_PPV_ARGS( &debugController ) );

                if ( SUCCEEDED(debugGet) )
                {
                    debugController->EnableDebugLayer();
                }
            }
#endif //_DEBUG || RW_ENABLE_GFX_NATIVE_DEBUG

            driverConstructionProps props;
            props.rasterMemSize = sizeof( d3d12NativeRaster );
            props.geomMemSize = sizeof( d3d12NativeGeometry );
            props.matMemSize = sizeof( d3d12NativeMaterial );
            props.swapChainMemSize = sizeof( d3d12SwapChain );
            props.graphicsStateMemSize = sizeof( d3d12PipelineStateObject );
            props.cmdAllocMemSize = sizeof( d3d12CmdAllocator );
            props.cmdBufMemSize = sizeof( d3d12CmdBuffer );
            props.cmdQueueMemSize = sizeof( d3d12CmdQueue );
            props.fenceMemSize = sizeof( d3d12Fence );
            props.gpumemMemSize = sizeof( d3d12GPUMem );

            hasRegistered = RegisterDriver( engineInterface, "Direct3D12", props, this, d3d12DriverFactory.GetClassSize() );
        }

        this->hasRegisteredDriver = hasRegistered;
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        if ( this->hasRegisteredDriver )
        {
            UnregisterDriver( engineInterface, this );
        }

        if ( HMODULE theModule = this->dxgiModule )
        {
            FreeLibrary( theModule );
        }

        if ( HMODULE theModule = this->d3d12Module )
        {
            FreeLibrary( theModule );
        }

        this->threadLocalEnvPlugin.Shutdown( engineInterface );
    }
};

};

#endif //_COMPILE_FOR_LEGACY

#endif //_WIN32

#endif //_RENDERWARE_DIRECT3D12_DRIVER_
