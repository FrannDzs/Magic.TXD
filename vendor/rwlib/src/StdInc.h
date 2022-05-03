/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/StdInc.h
*  PURPOSE:     Global internal header file
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include <sdk/MacroUtils.h>

#include <sdk/PluginFactory.h>

#define RWCORE
#include "renderware.h"

// Include the RenderWare configuration file.
// This one should be private to the rwtools project, hence we reject including it in "renderware.h"
#include "../rwconf.h"

#include <sdk/DynamicTypeSystem.h>
#include <sdk/MetaHelpers.h>

#ifdef DEBUG
	#define READ_HEADER(x)\
	header.read(rw);\
	if (header.type != (x)) {\
		cerr << filename << " ";\
		ChunkNotFound((x), rw.tellg());\
	}
#else
	#define READ_HEADER(x)\
	header.read(rw);
#endif

#include "rwprivate.common.h"

namespace rw
{

// Type system declaration for type abstraction.
// This is where atomics, frames, geometries register to.
struct EngineInterface : public Interface
{
    friend struct Interface;

    EngineInterface( void );
    EngineInterface( const EngineInterface& ) = delete;
    ~EngineInterface( void );

    void PerformLateInit( void );

    // DO NOT ACCESS THE FIELDS DIRECTLY.
    // THEY MUST BE ACCESSED UNDER MUTUAL EXCLUSION/CONTEXT LOCKING.

    // General type system.
    struct typeSystemLockProvider
    {
        typedef unfair_mutex rwlock;

        inline rwlock* CreateLock( void )
        {
            return CreateUnfairMutex( engineInterface );
        }

        inline void CloseLock( rwlock *theLock )
        {
            CloseUnfairMutex( engineInterface, theLock );
        }

        inline void LockEnterRead( rwlock *theLock ) const
        {
            theLock->enter();
        }

        inline void LockLeaveRead( rwlock *theLock ) const
        {
            theLock->leave();
        }

        inline void LockEnterWrite( rwlock *theLock ) const
        {
            theLock->enter();
        }

        inline void LockLeaveWrite( rwlock *theLock ) const
        {
            theLock->leave();
        }

        EngineInterface *engineInterface;
    };

private:
    DEFINE_HEAP_REDIR_ALLOC_IMPL( dtsRedirAlloc );

public:
    typedef DynamicTypeSystem <dtsRedirAlloc, EngineInterface, typeSystemLockProvider, cachedMinimalStructRegistryFlavor, rwEirExceptionManager> RwTypeSystem;

    RwTypeSystem typeSystem;

    // Types that should be registered by all RenderWare implementations.
    // These can be nullptr, tho.
    RwTypeSystem::typeInfoBase *streamTypeInfo;
    RwTypeSystem::typeInfoBase *rwobjTypeInfo;
    RwTypeSystem::typeInfoBase *textureTypeInfo;

    // Information about the running application.
    // NOTE: have to be static strings because the memory manager is destroyed prior to them.
    rwStaticString <char> applicationName;
    rwStaticString <char> applicationVersion;
    rwStaticString <char> applicationDescription;
};

IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN EngineInterface::dtsRedirAlloc::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS
{
    EngineInterface *natEngine = LIST_GETITEM( EngineInterface, refMem, typeSystem );
    return natEngine->MemAllocateP( memSize, alignment );
}
IMPL_HEAP_REDIR_METH_RESIZE_RETURN EngineInterface::dtsRedirAlloc::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS_DIRECT
{
    EngineInterface *natEngine = LIST_GETITEM( EngineInterface, refMem, typeSystem );
    return natEngine->MemResize( objMem, reqNewSize );
}
IMPL_HEAP_REDIR_METH_FREE_RETURN EngineInterface::dtsRedirAlloc::Free IMPL_HEAP_REDIR_METH_FREE_ARGS
{
    EngineInterface *natEngine = LIST_GETITEM( EngineInterface, refMem, typeSystem );
    natEngine->MemFree( memPtr );
}

typedef EngineInterface::RwTypeSystem RwTypeSystem;

// Use this function if you need a string that describes the currently running RenderWare environment.
// It uses the application variables of EngineInterface.
rwStaticString <char> GetRunningSoftwareInformation( EngineInterface *engineInterface, bool outputShort = false );

// Factory for global RenderWare interfaces.
typedef StaticPluginClassFactory <EngineInterface, RwStaticMemAllocator, rwEirExceptionManager> RwInterfaceFactory_t;

typedef RwInterfaceFactory_t::pluginOffset_t RwInterfacePluginOffset_t;

extern optional_struct_space <RwInterfaceFactory_t> engineFactory;

} // namespace rw

#include "rwprivate.bmp.h"
#include "rwprivate.txd.h"
#include "rwprivate.imaging.h"
#include "rwprivate.driver.h"
#include "rwprivate.warnings.h"
#include "rwprivate.locale.h"

#include "rwprivate.utils.h"
