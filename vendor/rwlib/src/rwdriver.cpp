/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.cpp
*  PURPOSE:     Graphics Driver management implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwdriver.hxx"

#include "pluginutil.hxx"

namespace rw
{

inline void SafeDeleteType( EngineInterface *engineInterface, RwTypeSystem::typeInfoBase *theType )
{
    if ( theType )
    {
        engineInterface->typeSystem.DeleteType( theType );
    }
}

struct nativeDriverTypeInterface final : public RwTypeSystem::typeInterface
{
    inline nativeDriverTypeInterface( EngineInterface *engineInterface, nativeDriverImplementation *driverIntf, size_t driverObjSize, const driverConstructionProps& props )
    {
        this->engineInterface = engineInterface;
        this->isRegistered = false;

        // Set up the type interface.
        this->driverImpl = driverIntf;
        this->driverMemSize = driverObjSize;

        // Initialize the sub types.
        this->_driverTypeRaster.driverType = this;
        this->_driverTypeRaster.objMemSize = props.rasterMemSize;
        this->_driverTypeRaster.objMemAlignment = props.rasterMemAlignment;

        this->_driverTypeGeometry.driverType = this;
        this->_driverTypeGeometry.objMemSize = props.geomMemSize;
        this->_driverTypeGeometry.objMemAlignment = props.geomMemAlignment;

        this->_driverTypeMaterial.driverType = this;
        this->_driverTypeMaterial.objMemSize = props.matMemSize;
        this->_driverTypeMaterial.objMemAlignment = props.matMemAlignment;

        this->_driverSwapChainType.driverType = this;
        this->_driverSwapChainType.objMemSize = props.swapChainMemSize;
        this->_driverSwapChainType.objMemAlignment = props.swapChainMemAlignment;

        this->_driverGraphicsStateType.driverType = this;
        this->_driverGraphicsStateType.objMemSize = props.graphicsStateMemSize;
        this->_driverGraphicsStateType.objMemAlignment = props.graphicsStateMemAlignment;

        this->_driverCmdAllocatorType.driverType = this;
        this->_driverCmdAllocatorType.objMemSize = props.cmdAllocMemSize;
        this->_driverCmdAllocatorType.objMemAlignment = props.cmdAllocMemAlignment;

        this->_driverCmdBufferType.driverType = this;
        this->_driverCmdBufferType.objMemSize = props.cmdBufMemSize;
        this->_driverCmdBufferType.objMemAlignment = props.cmdBufMemAlignment;

        this->_driverCmdQueueType.driverType = this;
        this->_driverCmdQueueType.objMemSize = props.cmdQueueMemSize;
        this->_driverCmdQueueType.objMemAlignment = props.cmdQueueMemAlignment;

        this->_driverFenceType.driverType = this;
        this->_driverFenceType.objMemSize = props.fenceMemSize;
        this->_driverFenceType.objMemAlignment = props.fenceMemAlignment;

        this->_driverGPUMemType.driverType = this;
        this->_driverGPUMemType.objMemSize = props.gpumemMemSize;
        this->_driverGPUMemType.objMemAlignment = props.gpumemMemAlignment;

        // Every driver has to export functionality for instanced objects.
        this->rasterType = nullptr;
        this->geometryType = nullptr;
        this->materialType = nullptr;
        this->swapChainType = nullptr;
        this->graphicsStateType = nullptr;
        this->cmdAllocType = nullptr;
        this->cmdBufType = nullptr;
        this->cmdQueueType = nullptr;
        this->fenceType = nullptr;
        this->gpumemType = nullptr;

        // Reflexion.
        this->driverType = nullptr;
    }

    inline ~nativeDriverTypeInterface( void )
    {
        // Delete our driver types.
        SafeDeleteType( engineInterface, this->rasterType );
        SafeDeleteType( engineInterface, this->geometryType );
        SafeDeleteType( engineInterface, this->materialType );
        SafeDeleteType( engineInterface, this->swapChainType );
        SafeDeleteType( engineInterface, this->graphicsStateType );
        SafeDeleteType( engineInterface, this->cmdAllocType );
        SafeDeleteType( engineInterface, this->cmdBufType );
        SafeDeleteType( engineInterface, this->cmdQueueType );
        SafeDeleteType( engineInterface, this->fenceType );
        SafeDeleteType( engineInterface, this->gpumemType );

        // The driverType is deleted first.

        if ( this->isRegistered )
        {
            LIST_REMOVE( this->node );

            this->isRegistered = false;
        }
    }

    // Helper macro for defining driver object type interfaces (Geometry, etc)
    struct driver_obj_constr_params
    {
        void *driverObj;
    };

#define NATIVE_DRIVER_OBJ_TYPEINTERFACE( canonicalName ) \
    struct nativeDriver##canonicalName##TypeInterface : public RwTypeSystem::typeInterface \
    { \
        void Construct( void *mem, EngineInterface *engineInterface, void *construction_params ) const override \
        { \
            driver_obj_constr_params *params = (driver_obj_constr_params*)construction_params; \
            driverType->driverImpl->On##canonicalName##Construct( engineInterface, params->driverObj, mem, this->objMemSize ); \
        } \
        void CopyConstruct( void *mem, const void *srcMem ) const override \
        { \
            driverType->driverImpl->On##canonicalName##CopyConstruct( driverType->engineInterface, mem, srcMem, this->objMemSize ); \
        } \
        void Destruct( void *mem ) const override \
        { \
            driverType->driverImpl->On##canonicalName##Destroy( driverType->engineInterface, mem, this->objMemSize ); \
        } \
        size_t GetTypeSize( EngineInterface *engineInterface, void *construction_params ) const override \
        { \
            return this->objMemSize; \
        } \
        size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *objMem ) const override \
        { \
            return this->objMemSize; \
        } \
        size_t GetTypeAlignment( EngineInterface *engineInterface, void *cosntruction_params ) const override \
        { \
            return this->objMemAlignment; \
        } \
        size_t GetTypeAlignmentByObject( EngineInterface *engineInterface, const void *objMem ) const override \
        { \
            return this->objMemAlignment; \
        } \
        size_t objMemSize; \
        size_t objMemAlignment; \
        nativeDriverTypeInterface *driverType; \
    }; \
    nativeDriver##canonicalName##TypeInterface _driverType##canonicalName;

    NATIVE_DRIVER_OBJ_TYPEINTERFACE( Raster );
    NATIVE_DRIVER_OBJ_TYPEINTERFACE( Geometry );
    NATIVE_DRIVER_OBJ_TYPEINTERFACE( Material );

    struct swapchain_const_params
    {
        Driver *driverObj;
        Window *sysWnd;
        uint32 frameCount;
        DriverCmdQueue *cmdQueue;
    };

    template <typename headerStructType>
    struct customTypeInterfaceWithHeader : public RwTypeSystem::typeInterface
    {
        size_t GetTypeSize( EngineInterface *engineInterface, void *construct_params ) const override final
        {
            return CalculateAlignedTypeSizeWithHeader( sizeof(headerStructType), this->objMemSize, this->objMemAlignment );
        }

        size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *objMem ) const override final
        {
            return this->GetTypeSize( engineInterface, nullptr );
        }

        size_t GetTypeAlignment( EngineInterface *engineInterface, void *construct_params ) const override final
        {
            return CalculateAlignmentForObjectWithHeader( alignof(headerStructType), this->objMemAlignment );
        }

        size_t GetTypeAlignmentByObject( EngineInterface *engineInterface, const void *objMem ) const override final
        {
            return this->GetTypeAlignment( engineInterface, nullptr );
        }

        size_t objMemSize;
        size_t objMemAlignment;
        nativeDriverTypeInterface *driverType;
    };

    // Special swap chain type object.
    struct nativeDriverSwapChainTypeInterface : public customTypeInterfaceWithHeader <DriverSwapChain>
    {
        void Construct( void *mem, EngineInterface *engineInterface, void *construct_params ) const override
        {
            swapchain_const_params *params = (swapchain_const_params*)construct_params;

            Driver *theDriver = params->driverObj;
            Window *sysWnd = params->sysWnd;

            // First we construct the swap chain.
            DriverSwapChain *swapChain = new (mem) DriverSwapChain( engineInterface, theDriver, sysWnd );

            try
            {
                this->driverType->driverImpl->SwapChainConstruct(
                    engineInterface,
                    theDriver->GetImplementation(),
                    swapChain->GetImplementation(),
                    sysWnd, params->frameCount,
                    params->cmdQueue->GetImplementation()
                );
            }
            catch( ... )
            {
                swapChain->~DriverSwapChain();

                throw;
            }
        }

        void CopyConstruct( void *mem, const void *srcMem ) const override
        {
            throw InvalidOperationException( eSubsystemType::DRIVER, eOperationType::COPY_CONSTRUCT, L"DRIVER_SWAPCHAIN_FRIENDLYNAME" );
        }

        void Destruct( void *mem ) const override
        {
            DriverSwapChain *swapChain = (DriverSwapChain*)mem;

            // First destroy the implementation.
            this->driverType->driverImpl->SwapChainDestroy(
                this->driverType->engineInterface,
                swapChain->GetImplementation()
            );

            // Now destroy the actual thing.
            swapChain->~DriverSwapChain();
        }
    };
    nativeDriverSwapChainTypeInterface _driverSwapChainType;

    // Special PSO object for graphics.
    struct graphics_state_const_params
    {
        inline graphics_state_const_params( Driver *theDriver, const gfxGraphicsState& psoState )
            : theDriver( theDriver ), psoState( psoState )
        {
            return;
        }

        Driver *theDriver;
        const gfxGraphicsState& psoState;
    };

    struct nativeDriverGraphicsStateTypeInterface : public customTypeInterfaceWithHeader <DriverGraphicsState>
    {
        void Construct( void *mem, EngineInterface *engineInterface, void *construct_params ) const override
        {
            const graphics_state_const_params *params = (const graphics_state_const_params*)construct_params;

            Driver *theDriver = params->theDriver;

            DriverGraphicsState *psoObj = new (mem) DriverGraphicsState( theDriver );

            try
            {
                this->driverType->driverImpl->GraphicsStateConstruct(
                    engineInterface, theDriver->GetImplementation(),
                    psoObj->GetImplementation(), params->psoState
                );
            }
            catch( ... )
            {
                psoObj->~DriverGraphicsState();

                throw;
            }
        }

        void CopyConstruct( void *mem, const void *srcMem ) const override
        {
            throw InvalidOperationException( eSubsystemType::DRIVER, eOperationType::COPY_CONSTRUCT, L"DRIVER_PSO_FRIENDLYNAME" );
        }

        void Destruct( void *mem ) const override
        {
            DriverGraphicsState *psoObj = (DriverGraphicsState*)mem;

            this->driverType->driverImpl->GraphicsStateDestroy(
                this->driverType->engineInterface, psoObj->GetImplementation()
            );

            psoObj->~DriverGraphicsState();
        }
    };
    nativeDriverGraphicsStateTypeInterface _driverGraphicsStateType;

    // Command buffer object.
    struct cmdbuf_cparams
    {
        Driver *theDriver;
        DriverCmdAlloc *allocMan;
        eCmdBufType bufType;
    };

    struct nativeDriverCmdBufferTypeInterface : public customTypeInterfaceWithHeader <DriverCmdBuffer>
    {
        void Construct( void *mem, EngineInterface *engineInterface, void *constr_params ) const override
        {
            const cmdbuf_cparams *cparams = (cmdbuf_cparams*)constr_params;

            DriverCmdBuffer *cmdBuf = new (mem) DriverCmdBuffer( cparams->theDriver, cparams->bufType );

            try
            {
                this->driverType->driverImpl->CmdBufConstruct(
                    engineInterface,
                    cparams->theDriver->GetImplementation(),
                    cmdBuf->GetImplementation(),
                    cparams->allocMan->GetImplementation(),
                    cparams->bufType
                );
            }
            catch( ... )
            {
                cmdBuf->~DriverCmdBuffer();
                throw;
            }
        }

        void CopyConstruct( void *mem, const void *srcMem ) const override
        {
            throw InvalidOperationException( eSubsystemType::DRIVER, eOperationType::COPY_CONSTRUCT, L"DRIVER_CMDBUF_FRIENDLYNAME" );
        }

        void Destruct( void *mem ) const override
        {
            DriverCmdBuffer *cmdBuf = (DriverCmdBuffer*)mem;

            this->driverType->driverImpl->CmdBufDestroy(
                this->driverType->engineInterface,
                cmdBuf->GetImplementation()
            );

            cmdBuf->~DriverCmdBuffer();
        }
    };
    nativeDriverCmdBufferTypeInterface _driverCmdBufferType;

    // Command allocator type.
    struct cmdalloc_cparams
    {
        Driver *driver;
        eCmdAllocType allocType;
    };

    struct nativeDriverCmdAllocatorTypeInterface : public customTypeInterfaceWithHeader <DriverCmdAlloc>
    {
        void Construct( void *mem, EngineInterface *engineInterface, void *constr_params ) const override
        {
            const cmdalloc_cparams *cparams = (const cmdalloc_cparams*)constr_params;

            Driver *theDriver = cparams->driver;
            eCmdAllocType allocType = cparams->allocType;

            DriverCmdAlloc *cmdAlloc = new (mem) DriverCmdAlloc( theDriver, allocType );

            try
            {
                this->driverType->driverImpl->CmdAllocConstruct(
                    engineInterface,
                    theDriver->GetImplementation(),
                    cmdAlloc->GetImplementation(),
                    allocType
                );
            }
            catch( ... )
            {
                cmdAlloc->~DriverCmdAlloc();
                throw;
            }
        }

        void CopyConstruct( void *mem, const void *srcMem ) const override
        {
            throw InvalidOperationException( eSubsystemType::DRIVER, eOperationType::COPY_CONSTRUCT, L"DRIVER_CMDALLOC_FRIENDLYNAME" );
        }

        void Destruct( void *mem ) const override
        {
            DriverCmdAlloc *cmdAlloc = (DriverCmdAlloc*)mem;

            this->driverType->driverImpl->CmdAllocDestroy(
                this->driverType->engineInterface,
                cmdAlloc->GetImplementation()
            );

            cmdAlloc->~DriverCmdAlloc();
        }
    };
    nativeDriverCmdAllocatorTypeInterface _driverCmdAllocatorType;

    // Command Queue object.
    struct cmdqueue_cparams
    {
        Driver *theDriver;
        eCmdBufType queueType;
        int priority;
    };

    struct nativeDriverCmdQueueTypeInterface : public customTypeInterfaceWithHeader <DriverCmdQueue>
    {
        void Construct( void *mem, EngineInterface *engineInterface, void *constr_params ) const override
        {
            const cmdqueue_cparams *cparams = (const cmdqueue_cparams*)constr_params;

            Driver *theDriver = cparams->theDriver;
            eCmdBufType queueType = cparams->queueType;

            DriverCmdQueue *cmdQueue = new (mem) DriverCmdQueue( theDriver, queueType );

            try
            {
                this->driverType->driverImpl->CmdQueueConstruct(
                    engineInterface,
                    theDriver->GetImplementation(),
                    cmdQueue->GetImplementation(),
                    queueType, cparams->priority
                );
            }
            catch( ... )
            {
                cmdQueue->~DriverCmdQueue();
                throw;
            }
        }

        void CopyConstruct( void *mem, const void *srcMem ) const override
        {
            throw InvalidOperationException( eSubsystemType::DRIVER, eOperationType::COPY_CONSTRUCT, L"DRIVER_CMDQUEUE_FRIENDLYNAME" );
        }

        void Destruct( void *mem ) const override
        {
            DriverCmdQueue *cmdQueue = (DriverCmdQueue*)mem;

            this->driverType->driverImpl->CmdQueueDestroy(
                this->driverType->engineInterface,
                cmdQueue->GetImplementation()
            );

            cmdQueue->~DriverCmdQueue();
        }
    };
    nativeDriverCmdQueueTypeInterface _driverCmdQueueType;

    // Fence synchronization object.
    struct fence_cparams
    {
        Driver *theDriver;
        uint64 initValue;
    };

    struct nativeDriverFenceTypeInterface : public customTypeInterfaceWithHeader <DriverFence>
    {
        void Construct( void *mem, EngineInterface *engineInterface, void *constr_params ) const override
        {
            const fence_cparams *cparams = (const fence_cparams*)constr_params;

            Driver *theDriver = cparams->theDriver;

            DriverFence *fence = new (mem) DriverFence( theDriver );

            try
            {
                this->driverType->driverImpl->FenceConstruct(
                    engineInterface,
                    theDriver->GetImplementation(),
                    fence->GetImplementation(),
                    cparams->initValue
                );
            }
            catch( ... )
            {
                fence->~DriverFence();
                throw;
            }
        }

        void CopyConstruct( void *mem, const void *srcMem ) const override
        {
            throw InvalidOperationException( eSubsystemType::DRIVER, eOperationType::COPY_CONSTRUCT, L"DRIVER_FENCE_FRIENDLYNAME" );
        }

        void Destruct( void *mem ) const override
        {
            DriverFence *fence = (DriverFence*)mem;

            this->driverType->driverImpl->FenceDestroy(
                this->driverType->engineInterface,
                fence->GetImplementation()
            );

            fence->~DriverFence();
        }
    };
    nativeDriverFenceTypeInterface _driverFenceType;

    // GPU memory type info.
    struct gpumem_constr_props
    {
        Driver *theDriver;
        eDriverMemType memType;
        size_t numBytes;
        const gpuMemProperties& memProps;

        inline gpumem_constr_props( Driver *theDriver, eDriverMemType memType, size_t numBytes, const gpuMemProperties& memProps )
            : theDriver( theDriver ), memType( memType ), numBytes( numBytes ), memProps( memProps )
        {
            return;
        }
    };

    struct nativeDriverGPUMemTypeInterface : public customTypeInterfaceWithHeader <DriverMemory>
    {
        void Construct( void *mem, EngineInterface *engineInterface, void *constr_params ) const override
        {
            const gpumem_constr_props *cparams = (const gpumem_constr_props*)constr_params;

            eDriverMemType memType = cparams->memType;
            Driver *theDriver = cparams->theDriver;
            DriverMemory *memHandle = new (mem) DriverMemory( theDriver, memType, cparams->memProps );

            try
            {
                this->driverType->driverImpl->GPUMemRequest(
                    engineInterface,
                    theDriver->GetImplementation(),
                    memHandle->GetImplementation(),
                    memType, cparams->numBytes, cparams->memProps
                );
            }
            catch( ... )
            {
                memHandle->~DriverMemory();
                throw;
            }
        }

        void CopyConstruct( void *mem, const void *srcMem ) const override
        {
            throw InvalidOperationException( eSubsystemType::DRIVER, eOperationType::COPY_CONSTRUCT, L"DRIVER_MEM_FRIENDLYNAME" );
        }

        void Destruct( void *mem ) const override
        {
            DriverMemory *natMem = (DriverMemory*)mem;

            this->driverType->driverImpl->GPUMemRelease(
                this->driverType->engineInterface,
                natMem->GetImplementation()
            );

            natMem->~DriverMemory();
        }
    };
    nativeDriverGPUMemTypeInterface _driverGPUMemType;

    void Construct( void *mem, EngineInterface *engineInterface, void *construct_params ) const override
    {
        // First construct the driver.
        Driver *theDriver = new (mem) Driver( engineInterface );

        this->driverImpl->OnDriverConstruct(
            engineInterface,
            theDriver->GetImplementation(), this->driverMemSize
        );
    }

    void CopyConstruct( void *mem, const void *srcMem ) const override
    {
        const Driver *srcDriver = (const Driver*)srcMem;

        // First clone the driver abstract object.
        Driver *newDriver = new (mem) Driver( *srcDriver );

        this->driverImpl->OnDriverCopyConstruct(
            this->engineInterface,
            newDriver->GetImplementation(),
            srcDriver + 1,
            this->driverMemSize
        );
    }

    void Destruct( void *mem ) const override
    {
        // First destroy the implementation.
        Driver *theDriver = (Driver*)mem;

        this->driverImpl->OnDriverDestroy(
            this->engineInterface,
            theDriver->GetImplementation(), this->driverMemSize
        );

        // Now the abstract thing.
        theDriver->~Driver();
    }

    size_t GetTypeSize( EngineInterface *engineInterface, void *construct_params ) const override final
    {
        return CalculateAlignedTypeSizeWithHeader( sizeof(Driver), this->driverMemSize, this->driverMemAlignment );
    }

    size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *objMem ) const override final
    {
        return this->GetTypeSize( engineInterface, nullptr );
    }

    size_t GetTypeAlignment( EngineInterface *engineInterface, void *construct_params ) const override final
    {
        return CalculateAlignmentForObjectWithHeader( alignof(Driver), this->driverMemAlignment );
    }

    size_t GetTypeAlignmentByObject( EngineInterface *engineInterface, const void *objMem ) const override final
    {
        return this->GetTypeAlignment( engineInterface, nullptr );
    }

    EngineInterface *engineInterface;
    size_t driverMemSize;
    size_t driverMemAlignment;
    nativeDriverImplementation *driverImpl;

    // Private manager data.
    RwTypeSystem::typeInfoBase *driverType;         // actual driver type
    RwTypeSystem::typeInfoBase *rasterType;         // type of instanced raster
    RwTypeSystem::typeInfoBase *geometryType;       // type of instanced geometry
    RwTypeSystem::typeInfoBase *materialType;       // type of instanced material
    RwTypeSystem::typeInfoBase *swapChainType;      // type of windowing system buffer
    RwTypeSystem::typeInfoBase *graphicsStateType;  // type of graphics PSO
    RwTypeSystem::typeInfoBase *cmdAllocType;       // type of command allocator
    RwTypeSystem::typeInfoBase *cmdBufType;         // type of command buffer
    RwTypeSystem::typeInfoBase *cmdQueueType;       // type of command queue
    RwTypeSystem::typeInfoBase *fenceType;          // type of fence synchronization object
    RwTypeSystem::typeInfoBase *gpumemType;         // type of GPU memory

    bool isRegistered;
    RwListEntry <nativeDriverTypeInterface> node;
};

struct driverEnvironment
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        this->baseDriverType =
            engineInterface->typeSystem.RegisterAbstractType <nativeDriverImplementation> ( "driver" );

        LIST_CLEAR( this->driverTypeList.root );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Delete all sub driver types.
        while ( !LIST_EMPTY( this->driverTypeList.root ) )
        {
            nativeDriverTypeInterface *item = LIST_GETITEM( nativeDriverTypeInterface, this->driverTypeList.root.next, node );

            engineInterface->typeSystem.DeleteType( item->driverType );
        }

        if ( RwTypeSystem::typeInfoBase *baseDriverType = this->baseDriverType )
        {
            engineInterface->typeSystem.DeleteType( baseDriverType );
        }
    }

    inline driverEnvironment& operator = ( const driverEnvironment& right ) = delete;

    RwTypeSystem::typeInfoBase *baseDriverType;

    RwList <nativeDriverTypeInterface> driverTypeList;

    inline nativeDriverTypeInterface* FindDriverTypeInterface( nativeDriverImplementation *impl ) const
    {
        LIST_FOREACH_BEGIN( nativeDriverTypeInterface, this->driverTypeList.root, node )

            if ( item->driverImpl == impl )
            {
                return item;
            }

        LIST_FOREACH_END

        return nullptr;
    }

    static inline nativeDriverTypeInterface* GetDriverTypeInterface( Driver *theDriver )
    {
        GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( theDriver );

        if ( rtObj )
        {
            RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

            if ( typeInfo )
            {
                return dynamic_cast <nativeDriverTypeInterface*> ( typeInfo->tInterface );
            }
        }

        return nullptr;
    }
};

static optional_struct_space <PluginDependantStructRegister <driverEnvironment, RwInterfaceFactory_t>> driverEnvRegister;

bool RegisterDriver( EngineInterface *engineInterface, const char *typeName, const driverConstructionProps& props, nativeDriverImplementation *driverIntf, size_t driverObjSize )
{
    driverEnvironment *driverEnv = driverEnvRegister.get().GetPluginStruct( engineInterface );

    bool success = false;

    if ( driverEnv )
    {
        // First create the type interface that will manage the new driver.
        // This is required to properly register the driver into our DTS.

        // Register the driver type.
        RwTypeSystem::commonTypeInfoStruct <nativeDriverTypeInterface> *driverType =
            engineInterface->typeSystem.RegisterCommonTypeInterface <nativeDriverTypeInterface> (
                typeName, driverEnv->baseDriverType, engineInterface, driverIntf, driverObjSize, props
            );

        if ( driverType )
        {
            nativeDriverTypeInterface *driverTypeInterface = &driverType->tInterface;

            driverTypeInterface->driverType = driverType;

            // Create sub types.
            // Here we use the type hierachy to define types that belong to each other in a system.
            // Previously at RwObject we used the type hierarchy to define types that inherit from each other.
            driverTypeInterface->rasterType = engineInterface->typeSystem.RegisterType( "raster", &driverTypeInterface->_driverTypeRaster, driverType );
            driverTypeInterface->geometryType = engineInterface->typeSystem.RegisterType( "geometry", &driverTypeInterface->_driverTypeGeometry, driverType );
            driverTypeInterface->materialType = engineInterface->typeSystem.RegisterType( "material", &driverTypeInterface->_driverTypeMaterial, driverType );
            driverTypeInterface->swapChainType = engineInterface->typeSystem.RegisterType( "swapchain", &driverTypeInterface->_driverSwapChainType, driverType );
            driverTypeInterface->graphicsStateType = engineInterface->typeSystem.RegisterType( "graphics_pso", &driverTypeInterface->_driverGraphicsStateType, driverType );
            driverTypeInterface->cmdAllocType = engineInterface->typeSystem.RegisterType( "cmd_alloc", &driverTypeInterface->_driverCmdAllocatorType, driverType );
            driverTypeInterface->cmdBufType = engineInterface->typeSystem.RegisterType( "cmd_buf", &driverTypeInterface->_driverCmdBufferType, driverType );
            driverTypeInterface->cmdQueueType = engineInterface->typeSystem.RegisterType( "cmd_queue", &driverTypeInterface->_driverCmdQueueType, driverType );
            driverTypeInterface->fenceType = engineInterface->typeSystem.RegisterType( "fence", &driverTypeInterface->_driverFenceType, driverType );
            driverTypeInterface->gpumemType = engineInterface->typeSystem.RegisterType( "gpumem", &driverTypeInterface->_driverGPUMemType, driverType );

            // Register our driver.
            LIST_INSERT( driverEnv->driverTypeList.root, driverTypeInterface->node );

            driverTypeInterface->isRegistered = true;

            success = true;
        }
    }

    return success;
}

bool UnregisterDriver( EngineInterface *engineInterface, nativeDriverImplementation *driverIntf )
{
    driverEnvironment *driverEnv = driverEnvRegister.get().GetPluginStruct( engineInterface );

    bool success = false;

    if ( driverEnv )
    {
        nativeDriverTypeInterface *typeInterface = driverEnv->FindDriverTypeInterface( driverIntf );

        if ( typeInterface )
        {
            engineInterface->typeSystem.DeleteType( typeInterface->driverType );

            success = true;
        }
    }

    return success;
}

// Public driver API.
Driver* CreateDriver( Interface *intf, const char *typeName )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    Driver *driverOut = nullptr;

    driverEnvironment *driverEnv = driverEnvRegister.get().GetPluginStruct( engineInterface );

    if ( driverEnv )
    {
        // Get a the type of the driver we want to create.
        RwTypeSystem::typeInfoBase *driverType = engineInterface->typeSystem.FindTypeInfo( typeName, driverEnv->baseDriverType );

        if ( driverType )
        {
            // Construct the object and return it.
            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, driverType, nullptr );

            if ( rtObj )
            {
                // Lets return stuff.
                driverOut = (Driver*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return driverOut;
}

void DestroyDriver( Interface *intf, Driver *theDriver )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( theDriver );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

// Driver API.
void* Driver::GetNativeHandle( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this ) )
    {
        return driverInfo->driverImpl->DriverGetNativeHandle( this->GetImplementation() );
    }

    return nullptr;
}

// Driver interface.
DriverSwapChain* Driver::CreateSwapChain( Window *outputWindow, uint32 frameCount, DriverCmdQueue *cmdQueue )
{
    DriverSwapChain *swapChainOut = nullptr;

    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this ) )
    {
        // Make the friggin swap chain.
        if ( RwTypeSystem::typeInfoBase *swapChainType = driverInfo->swapChainType )
        {
            // Create it.
            nativeDriverTypeInterface::swapchain_const_params params;
            params.driverObj = this;
            params.sysWnd = outputWindow;
            params.frameCount = frameCount;
            params.cmdQueue = cmdQueue;

            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, swapChainType, &params );

            if ( rtObj )
            {
                swapChainOut = (DriverSwapChain*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return swapChainOut;
}

void Driver::DestroySwapChain( DriverSwapChain *swapChain )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    // Simply generically destroy it.
    GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( swapChain );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

rwlock* DriverSwapChain::GetUsageLock( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->driver ) )
    {
        return driverInfo->driverImpl->SwapChainGetUsageLock( this->GetImplementation() );
    }

    return nullptr;
}

void DriverSwapChain::Present( bool vsync )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->driver ) )
    {
        driverInfo->driverImpl->SwapChainPresent( this->GetImplementation(), vsync );
    }
}

unsigned int DriverSwapChain::GetBackBufferIndex( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->driver ) )
    {
        return driverInfo->driverImpl->SwapChainGetBackBufferIndex( this->GetImplementation() );
    }

    return 0;
}

DriverPtr DriverSwapChain::GetBackBufferViewPointer( unsigned int bufIdx )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->driver ) )
    {
        return driverInfo->driverImpl->SwapChainGetBackBufferViewPointer( this->GetImplementation(), bufIdx );
    }

    return 0;
}

void* DriverSwapChain::GetImplementation( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->driver ) )
    {
        return GetObjectImplementationPointerForObject( this, sizeof(DriverSwapChain), driverInfo->_driverSwapChainType.objMemAlignment );
    }

    return nullptr;
}

DriverGraphicsState* Driver::CreateGraphicsState( const gfxGraphicsState& psoState )
{
    DriverGraphicsState *psoOut = nullptr;

    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this ) )
    {
        // Create the object.
        if ( RwTypeSystem::typeInfoBase *psoTypeInfo = driverInfo->graphicsStateType )
        {
            nativeDriverTypeInterface::graphics_state_const_params params( this, psoState );

            GenericRTTI *rtObj =
                engineInterface->typeSystem.Construct( engineInterface, psoTypeInfo, &params );

            if ( rtObj )
            {
                psoOut = (DriverGraphicsState*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return psoOut;
}

void Driver::DestroyGraphicsState( DriverGraphicsState *pso )
{
    // Just delete it generically.
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( pso );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

void* DriverGraphicsState::GetNativeHandle( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->driver ) )
    {
        return driverInfo->driverImpl->GraphicsStateGetNativeHandle( this->GetImplementation() );
    }

    return nullptr;
}

void* DriverGraphicsState::GetImplementation( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->driver ) )
    {
        return GetObjectImplementationPointerForObject( this, sizeof(DriverGraphicsState), driverInfo->_driverGraphicsStateType.objMemAlignment );
    }

    return nullptr;
}

// Command allocator API.
DriverCmdAlloc* Driver::CreateCommandAllocator( eCmdAllocType allocType )
{
    DriverCmdAlloc *cmdAlloc = nullptr;

    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this ) )
    {
        // Create the object.
        if ( RwTypeSystem::typeInfoBase *cmdAllocType = driverInfo->cmdAllocType )
        {
            nativeDriverTypeInterface::cmdalloc_cparams cparams;
            cparams.driver = this;
            cparams.allocType = allocType;

            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, cmdAllocType, &cparams );

            if ( rtObj )
            {
                cmdAlloc = (DriverCmdAlloc*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return cmdAlloc;
}

void Driver::DestroyCommandAllocator( DriverCmdAlloc *cmdAlloc )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( cmdAlloc );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

bool DriverCmdAlloc::Reset( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return driverInfo->driverImpl->CmdAllocReset( this->GetImplementation() );
    }

    return false;
}

void* DriverCmdAlloc::GetImplementation( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return GetObjectImplementationPointerForObject( this, sizeof(DriverCmdAlloc), driverInfo->_driverCmdAllocatorType.objMemAlignment );
    }

    return nullptr;
}

// Command buffer API.
DriverCmdBuffer* Driver::CreateCommandBuffer( eCmdBufType bufType, DriverCmdAlloc *allocMan )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this ) )
    {
        // Construct the object.
        if ( RwTypeSystem::typeInfoBase *cmdBufType = driverInfo->cmdBufType )
        {
            nativeDriverTypeInterface::cmdbuf_cparams cparams;
            cparams.theDriver = this;
            cparams.allocMan = allocMan;
            cparams.bufType = bufType;

            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, cmdBufType, &cparams );

            if ( rtObj )
            {
                return (DriverCmdBuffer*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return nullptr;
}

void Driver::DestroyCommandBuffer( DriverCmdBuffer *cmdBuf )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( cmdBuf );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

bool DriverCmdBuffer::Close( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return driverInfo->driverImpl->CmdBufClose( this->GetImplementation() );
    }

    return false;
}

bool DriverCmdBuffer::Reset( DriverCmdAlloc *cmdAlloc, DriverGraphicsState *psoObj )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return driverInfo->driverImpl->CmdBufReset(
            this->GetImplementation(),
            cmdAlloc->GetImplementation(),
            ( psoObj != nullptr ) ? ( psoObj->GetImplementation() ) : ( nullptr )
        );
    }

    return false;
}

void* DriverCmdBuffer::GetNativeHandle( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return driverInfo->driverImpl->CmdBufGetNativeHandle( this->GetImplementation() );
    }

    return nullptr;
}

void* DriverCmdBuffer::GetImplementation( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return GetObjectImplementationPointerForObject( this, sizeof(DriverCmdBuffer), driverInfo->_driverCmdBufferType.objMemAlignment );
    }

    return nullptr;
}

// Command queue API.
DriverCmdQueue* Driver::CreateCommandQueue( eCmdBufType queueType, int priority )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this ) )
    {
        // Create the object.
        if ( RwTypeSystem::typeInfoBase *queueTypeInfo = driverInfo->cmdQueueType )
        {
            nativeDriverTypeInterface::cmdqueue_cparams cparams;
            cparams.theDriver = this;
            cparams.queueType = queueType;
            cparams.priority = priority;

            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, queueTypeInfo, &cparams );

            if ( rtObj )
            {
                return (DriverCmdQueue*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return nullptr;
}

void Driver::DestroyCommandQueue( DriverCmdQueue *cmdQueue )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( cmdQueue );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

//TODO: add queue API.
bool DriverCmdQueue::Signal( DriverFence *fence, uint64 valueToBe )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return driverInfo->driverImpl->CmdQueueSignal(
            this->GetImplementation(),
            fence->GetImplementation(), valueToBe
        );
    }

    return false;
}

bool DriverCmdQueue::ExecuteCmdBuffer( DriverCmdBuffer *cmdBuf )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return driverInfo->driverImpl->CmdQueueExecuteCmdBuffer(
            this->GetImplementation(),
            cmdBuf->GetImplementation()
        );
    }

    return false;
}

void* DriverCmdQueue::GetImplementation( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return GetObjectImplementationPointerForObject( this, sizeof(DriverCmdQueue), driverInfo->_driverCmdQueueType.objMemAlignment );
    }

    return nullptr;
}

// Fence synchronization object API.
DriverFence* Driver::CreateFence( uint64 initValue )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this ) )
    {
        // Construct the object.
        if ( RwTypeSystem::typeInfoBase *fenceTypeInfo = driverInfo->fenceType )
        {
            nativeDriverTypeInterface::fence_cparams cparams;
            cparams.theDriver = this;
            cparams.initValue = initValue;

            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, fenceTypeInfo, &cparams );

            if ( rtObj )
            {
                return (DriverFence*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return nullptr;
}

void Driver::DestroyFence( DriverFence *fence )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( fence );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

DriverMemory* Driver::AllocateMemory( eDriverMemType memType, size_t numBytes, const gpuMemProperties& memProps )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    if ( nativeDriverTypeInterface *typeInfo = driverEnvironment::GetDriverTypeInterface( this ) )
    {
        // Construct the object.
        if ( RwTypeSystem::typeInfoBase *gpumemType = typeInfo->gpumemType )
        {
            nativeDriverTypeInterface::gpumem_constr_props cparams( this, memType, numBytes, memProps );

            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, gpumemType, &cparams );

            if ( rtObj )
            {
                return (DriverMemory*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return nullptr;
}

void Driver::ReleaseMemory( DriverMemory *gpumem )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( gpumem );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

void* Driver::GetImplementation( void )
{
    if ( nativeDriverTypeInterface *typeInfo = driverEnvironment::GetDriverTypeInterface( this ) )
    {
        return GetObjectImplementationPointerForObject( this, sizeof(Driver), typeInfo->driverMemAlignment );
    }

    return nullptr;
}

void DriverFence::Signal( uint64 newValue )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        driverInfo->driverImpl->FenceSignal( this->GetImplementation(), newValue );
    }
}

uint64 DriverFence::GetCurrentValue( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return driverInfo->driverImpl->FenceGetCurrentValue( this->GetImplementation() );
    }

    return 0;
}

void DriverFence::SingleWaitForSignal( uint64 waitValue, uint32 timeout )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        driverInfo->driverImpl->FenceSingleWaitForSignal( this->GetImplementation(), waitValue, timeout );
    }
}

void* DriverFence::GetNativeHandle( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return driverInfo->driverImpl->FenceGetNativeHandle( this->GetImplementation() );
    }

    return nullptr;
}

void* DriverFence::GetImplementation( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return GetObjectImplementationPointerForObject( this, sizeof(DriverFence), driverInfo->_driverFenceType.objMemAlignment );
    }

    return nullptr;
}

void* DriverMemory::GetImplementation( void )
{
    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this->theDriver ) )
    {
        return GetObjectImplementationPointerForObject( this, sizeof(DriverMemory), driverInfo->_driverGPUMemType.objMemAlignment );
    }

    return nullptr;
}

// Registration of driver general sub modules.
extern void registerDriverResourceEnvironment( void );
extern void registerDriverProgramManagerEnv( void );

extern void unregisterDriverResourceEnvironment( void );
extern void unregisterDriverProgramManagerEnv( void );

// Registration of driver implementations.
extern void registerD3D12DriverImplementation( void );

extern void unregisterD3D12DriverImplementation( void );

void registerDriverEnvironment( void )
{
    // Put our structure into the engine interface.
    driverEnvRegister.Construct( engineFactory );

    // Now register important sub modules.
    registerDriverResourceEnvironment();
    registerDriverProgramManagerEnv();

    // TODO: register all driver implementations.
    registerD3D12DriverImplementation();
}

void unregisterDriverEnvironment( void )
{
    unregisterD3D12DriverImplementation();

    unregisterDriverProgramManagerEnv();
    unregisterDriverResourceEnvironment();

    driverEnvRegister.Destroy();
}

};
