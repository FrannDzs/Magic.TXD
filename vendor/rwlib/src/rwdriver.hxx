/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.hxx
*  PURPOSE:     RenderWare graphics driver environment
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_DRIVER_FRAMEWORK_
#define _RENDERWARE_DRIVER_FRAMEWORK_

#include "rwdriver.immbuf.hxx"

namespace rw
{

// Driver properties that have to be passed during driver registration.
struct driverConstructionProps
{
    size_t rasterMemSize;
    size_t rasterMemAlignment;
    size_t geomMemSize;
    size_t geomMemAlignment;
    size_t matMemSize;
    size_t matMemAlignment;
    size_t swapChainMemSize;
    size_t swapChainMemAlignment;
    size_t graphicsStateMemSize;
    size_t graphicsStateMemAlignment;
    size_t cmdAllocMemSize;
    size_t cmdAllocMemAlignment;
    size_t cmdBufMemSize;
    size_t cmdBufMemAlignment;
    size_t cmdQueueMemSize;
    size_t cmdQueueMemAlignment;
    size_t fenceMemSize;
    size_t fenceMemAlignment;
    size_t gpumemMemSize;
    size_t gpumemMemAlignment;
};

/*
    This is the driver interface that every render device has to implement.
    It is a specialization of the original Criterion pipeline design
*/
struct nativeDriverImplementation abstract
{
    // Driver object creation.
    virtual void OnDriverConstruct( Interface *engineInterface, void *driverObjMem, size_t driverMemSize ) = 0;
    virtual void OnDriverCopyConstruct( Interface *engineInterface, void *driverObjMem, const void *srcDriverObjMem, size_t driverMemSize ) = 0;
    virtual void OnDriverDestroy( Interface *engineInterface, void *driverObjMem, size_t driverMemSize ) = 0;

    // Driver API.
    virtual void* DriverGetNativeHandle( void *driverMem ) = 0;

    // Helper macros.
#define NATIVE_DRIVER_OBJ_CONSTRUCT( canonicalName ) \
    void On##canonicalName##Construct( Interface *engineInterface, void *driverObjMem, void *objMem, size_t memSize )
#define NATIVE_DRIVER_OBJ_COPYCONSTRUCT( canonicalName ) \
    void On##canonicalName##CopyConstruct( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize )
#define NATIVE_DRIVER_OBJ_DESTROY( canonicalName ) \
    void On##canonicalName##Destroy( Interface *engineInterface, void *objMem, size_t memSize )

#define NATIVE_DRIVER_DEFINE_CONSTRUCT_VIRTUAL( canonicalName ) \
    virtual NATIVE_DRIVER_OBJ_CONSTRUCT( canonicalName ) = 0; \
    virtual NATIVE_DRIVER_OBJ_COPYCONSTRUCT( canonicalName ) = 0; \
    virtual NATIVE_DRIVER_OBJ_DESTROY( canonicalName ) = 0;

#define NATIVE_DRIVER_DEFINE_CONSTRUCT_FORWARD( canonicalName ) \
    NATIVE_DRIVER_OBJ_CONSTRUCT( canonicalName ); \
    NATIVE_DRIVER_OBJ_COPYCONSTRUCT( canonicalName ); \
    NATIVE_DRIVER_OBJ_DESTROY( canonicalName );

    // Instanced type creation.
    NATIVE_DRIVER_DEFINE_CONSTRUCT_VIRTUAL( Raster );
    NATIVE_DRIVER_DEFINE_CONSTRUCT_VIRTUAL( Geometry );
    NATIVE_DRIVER_DEFINE_CONSTRUCT_VIRTUAL( Material );

    // Default implementation macros.
#define NATIVE_DRIVER_OBJ_CONSTRUCT_IMPL( canonicalName, className, driverClassName ) \
    NATIVE_DRIVER_OBJ_CONSTRUCT( canonicalName ) override \
    { \
        new (objMem) className( this, engineInterface, (driverClassName*)driverObjMem ); \
    } \
    NATIVE_DRIVER_OBJ_COPYCONSTRUCT( canonicalName ) override \
    { \
        new (objMem) className( *(const className*)srcObjMem ); \
    } \
    NATIVE_DRIVER_OBJ_DESTROY( canonicalName ) override \
    { \
        ((className*)objMem)->~className(); \
    }

    // Instancing method helpers.
#define NATIVE_DRIVER_OBJ_INSTANCE( canonicalName ) \
    void canonicalName##Instance( Interface *engineInterface, void *driverObjMem, void *objMem, canonicalName *sysObj )
#define NATIVE_DRIVER_OBJ_UNINSTANCE( canonicalName ) \
    void canonicalName##Uninstance( Interface *engineInterface, void *driverObjMem, void *objMem )

#define NATIVE_DRIVER_DEFINE_INSTANCING_VIRTUAL( canonicalName ) \
    virtual NATIVE_DRIVER_OBJ_INSTANCE( canonicalName ) = 0; \
    virtual NATIVE_DRIVER_OBJ_UNINSTANCE( canonicalName ) = 0;

#define NATIVE_DRIVER_DEFINE_INSTANCING_FORWARD( canonicalName ) \
    NATIVE_DRIVER_OBJ_INSTANCE( canonicalName ); \
    NATIVE_DRIVER_OBJ_UNINSTANCE( canonicalName );

    // Instancing declarations.
    NATIVE_DRIVER_DEFINE_INSTANCING_VIRTUAL( Raster );
    NATIVE_DRIVER_DEFINE_INSTANCING_VIRTUAL( Geometry );
    NATIVE_DRIVER_DEFINE_INSTANCING_VIRTUAL( Material );

    // Drivers need swap chains for windowing system output.
#define NATIVE_DRIVER_SWAPCHAIN_CONSTRUCT() \
    void SwapChainConstruct( Interface *engineInterface, void *driverObjMem, void *objMem, Window *sysWnd, uint32 frameCount, void *cmdQueueMem )
#define NATIVE_DRIVER_SWAPCHAIN_DESTROY() \
    void SwapChainDestroy( Interface *engineInterface, void *objMem )

    virtual NATIVE_DRIVER_SWAPCHAIN_CONSTRUCT() = 0;
    virtual NATIVE_DRIVER_SWAPCHAIN_DESTROY() = 0;

    // Swap Chain API.
    virtual rwlock* SwapChainGetUsageLock( void *swapMem ) = 0;
    virtual void SwapChainPresent( void *swapMem, bool vsync ) = 0;
    virtual unsigned int SwapChainGetBackBufferIndex( void *swapMem ) = 0;
    virtual DriverPtr SwapChainGetBackBufferViewPointer( void *swapMem, unsigned int bufIdx ) = 0;

    // Graphics state API.
    // Those objects are immutable pipeline configurations.
    // They have to be used by the pipeline to render things using hardware acceleration.
    // It is best advised to sort their usage as efficiently as possible.
#define NATIVE_DRIVER_GRAPHICS_STATE_CONSTRUCT() \
    void GraphicsStateConstruct( Interface *engineInterface, void *driverObjMem, void *objMem, const gfxGraphicsState& gfxState )
#define NATIVE_DRIVER_GRAPHICS_STATE_DESTROY() \
    void GraphicsStateDestroy( Interface *engineInterface, void *objMem )

    virtual NATIVE_DRIVER_GRAPHICS_STATE_CONSTRUCT() = 0;
    virtual NATIVE_DRIVER_GRAPHICS_STATE_DESTROY() = 0;

    virtual void* GraphicsStateGetNativeHandle( void *psoMem ) = 0;

    // Command allocator API.
    // Memory is a very important factor in modern graphics APIs because
    // it is the most challenged resources. Smart usage of multiple allocators
    // can improve performance.
#define NATIVE_DRIVER_CMDALLOC_CONSTRUCT() \
    void CmdAllocConstruct( Interface *engineInterface, void *driverObjMem, void *objMem, eCmdAllocType allocType )
#define NATIVE_DRIVER_CMDALLOC_DESTROY() \
    void CmdAllocDestroy( Interface *engineInterface, void *objMem )

    virtual NATIVE_DRIVER_CMDALLOC_CONSTRUCT() = 0;
    virtual NATIVE_DRIVER_CMDALLOC_DESTROY() = 0;

    virtual bool CmdAllocReset( void *objMem ) = 0;

    // Command buffer API.
    // Modern GPU interfaces provide access to command lists that can be fed into
    // the GPU pipeline. This is an abstraction that wants to fit best for each.
#define NATIVE_DRIVER_CMDBUF_CONSTRUCT() \
    void CmdBufConstruct( Interface *engineInterface, void *driverObjMem, void *objMem, void *allocObjMem, eCmdBufType bufType )
#define NATIVE_DRIVER_CMDBUF_DESTROY() \
    void CmdBufDestroy( Interface *engineInterface, void *objMem )

    virtual NATIVE_DRIVER_CMDBUF_CONSTRUCT() = 0;
    virtual NATIVE_DRIVER_CMDBUF_DESTROY() = 0;

    // Generic commands to be put into command buffers.

    virtual bool CmdBufClose( void *objMem ) = 0;
    virtual bool CmdBufReset( void *objMem, void *cmdAllocMem, void *psoMem ) = 0;
    virtual void* CmdBufGetNativeHandle( void *objMem ) = 0;

    // Command queue API.
    // Those queues can process an arbitrary amount of commands that are
    // pushed into them. A single GPU can process multiple of them
    // asynchronously, so it is a good idea to manage multiple.
#define NATIVE_DRIVER_CMDQUEUE_CONSTRUCT() \
    void CmdQueueConstruct( Interface *engineInterface, void *driverObjMem, void *objMem, eCmdBufType queueType, int priority )
#define NATIVE_DRIVER_CMDQUEUE_DESTROY() \
    void CmdQueueDestroy( Interface *engineInterface, void *objMem )

    virtual NATIVE_DRIVER_CMDQUEUE_CONSTRUCT() = 0;
    virtual NATIVE_DRIVER_CMDQUEUE_DESTROY() = 0;

    // TODO: add synchronization and execution API.
    virtual bool CmdQueueSignal( void *objMem, void *fenceMem, uint64 valueToBe ) = 0;
    virtual bool CmdQueueExecuteCmdBuffer( void *objMem, void *cmdBufMem ) = 0;

    // Fence API.
    // These objects are synchronized integers that are processed by both
    // the GPU and the CPU. This way you can keep track of the GPU procession status.
#define NATIVE_DRIVER_FENCE_CONSTRUCT() \
    void FenceConstruct( Interface *engineInterface, void *driverObjMem, void *objMem, uint64 initialValue )
#define NATIVE_DRIVER_FENCE_DESTROY() \
    void FenceDestroy( Interface *engineInterface, void *objMem )

    virtual NATIVE_DRIVER_FENCE_CONSTRUCT() = 0;
    virtual NATIVE_DRIVER_FENCE_DESTROY() = 0;

    // Fence management API.
    virtual void FenceSignal( void *objMem, uint64 newValue ) = 0;
    virtual uint64 FenceGetCurrentValue( void *objMem ) = 0;
    virtual void FenceSingleWaitForSignal( void *objMem, uint64 waitValue, uint32 timeout ) = 0;

    virtual void* FenceGetNativeHandle( void *objMem ) = 0;

    // Memory API.
    // We can allocate GPU memory directly. Ownership is managed by a returned handle to a struct.
#define NATIVE_DRIVER_GPUMEM_CONSTRUCT() \
    void GPUMemRequest( Interface *engineInterface, void *driverObjMem, void *objMem, eDriverMemType memType, size_t numBytes, const gpuMemProperties& memProps )
#define NATIVE_DRIVER_GPUMEM_DESTROY() \
    void GPUMemRelease( Interface *engineInterface, void *objMem )

    virtual NATIVE_DRIVER_GPUMEM_CONSTRUCT() = 0;
    virtual NATIVE_DRIVER_GPUMEM_DESTROY() = 0;

    // Memory management API.
    virtual void GPUMemGetResSurfaceInfo( const ResourceSurfaceFormat& surfProps, ResourceSurfaceInfo& infoOut ) const = 0;
};

// Driver registration API.
bool RegisterDriver( EngineInterface *engineInterface, const char *typeName, const driverConstructionProps& props, nativeDriverImplementation *driverIntf, size_t driverObjSize );
bool UnregisterDriver( EngineInterface *engineInterface, nativeDriverImplementation *driverIntf );

};

#endif //_RENDERWARE_DRIVER_FRAMEWORK_