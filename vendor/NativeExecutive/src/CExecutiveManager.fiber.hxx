/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.fiber.hxx
*  PURPOSE:     Fiber environment internals
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_FIBER_INTERNALS_
#define _EXECUTIVE_MANAGER_FIBER_INTERNALS_

#include "CExecutiveManager.thread.hxx"
#include "internal/CExecutiveManager.fiber.internal.h"

#include <sdk/Vector.h>
#include <sdk/MetaHelpers.h>

BEGIN_NATIVE_EXECUTIVE

// Management variables for the fiber-stack-per-thread extension.
struct threadFiberStackPluginInfo
{
    inline threadFiberStackPluginInfo( CExecThreadImpl *natThread ) : fiberStack( nullptr, 0, natThread->manager )
    {
        return;
    }

    inline CFiberImpl* GetCurrentFiber( void )
    {
        size_t fiberCount = this->fiberStack.GetCount();

        return ( fiberCount != 0 ) ? ( this->fiberStack[ fiberCount - 1 ] ) : ( nullptr );
    }

    inline bool HasFiber( CFiberImpl *fiber ) const
    {
        return this->fiberStack.Find( fiber );
    }

    // Fiber stacks per thread!
    eir::Vector <CFiberImpl*, NatExecStandardObjectAllocator> fiberStack;

    // Need to have a per-thread native fiber that counts as last go-to point.
    Fiber context;
};

struct privateFiberEnvironment
{
    inline privateFiberEnvironment( CExecutiveManagerNative *manager ) : fiberFact( eir::constr_with_alloc::DEFAULT, manager )
    {
        this->threadFiberStackPluginOffset = privateThreadEnvironment::threadPluginContainer_t::INVALID_PLUGIN_OFFSET;
    }

    inline void Initialize( CExecutiveManagerNative *manager )
    {
        privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( manager );

        assert( threadEnv != nullptr );

        this->threadFiberStackPluginOffset =
            threadEnv->threadPlugins.RegisterStructPlugin <threadFiberStackPluginInfo> ( THREAD_PLUGIN_FIBER_STACK );

        this->defGroup = (CExecutiveGroupImpl*)manager->CreateGroup();
        
        assert( this->defGroup != nullptr );
    }

    inline void Shutdown( CExecutiveManagerNative *manager )
    {
        privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( manager );

        assert( threadEnv != nullptr );

        // Since the NativeExecutive environment has already destroyed all groups,
        // this should not be done.
        //manager->CloseGroup( this->defGroup );

        if ( privateThreadEnvironment::threadPluginContainer_t::IsOffsetValid( this->threadFiberStackPluginOffset ) )
        {
            threadEnv->threadPlugins.UnregisterPlugin( this->threadFiberStackPluginOffset );
        }
    }

    inline threadFiberStackPluginInfo* GetThreadFiberStackPlugin( CExecThreadImpl *thread )
    {
        auto pluginOffset = this->threadFiberStackPluginOffset;

        if ( !privateThreadEnvironment::threadPluginContainer_t::IsOffsetValid( pluginOffset ) )
        {
            return nullptr;
        }

        return privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <threadFiberStackPluginInfo> ( thread, pluginOffset );
    }

    typedef StaticPluginClassFactory <CFiberImpl, NatExecStandardObjectAllocator> fiberFactory_t;

    fiberFactory_t fiberFact;   // boys and girls, its fact!

    CExecutiveGroupImpl *defGroup;      // default group that all fibers are put into at the beginning.

    privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t threadFiberStackPluginOffset;
};

typedef PluginDependantStructRegister <privateFiberEnvironment, executiveManagerFactory_t> privateFiberEnvironmentRegister_t;

extern constinit optional_struct_space <privateFiberEnvironmentRegister_t> privateFiberEnvironmentRegister;

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_FIBER_INTERNALS_
