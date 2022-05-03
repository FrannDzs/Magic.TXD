/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.hazards.hxx
*  PURPOSE:     Thread hazard management internals, to prevent deadlocks
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _STACK_HAZARD_MANAGEMENT_INTERNALS_
#define _STACK_HAZARD_MANAGEMENT_INTERNALS_

#include "CExecutiveManager.fiber.hxx"

#include <sdk/Vector.h>

BEGIN_NATIVE_EXECUTIVE

// Struct that is registered at hazardous objects, basically anything that hosts CPU time.
// This cannot be a dependant struct.
struct stackObjectHazardRegistry abstract
{
    friend struct executiveHazardManagerEnv;

    inline stackObjectHazardRegistry( CExecutiveManagerNative *manager ) : hazardStack( eir::constr_with_alloc::DEFAULT, manager )
    {
        return;
    }

    inline void Initialize( CExecutiveManager *manager )
    {
        return;
    }

    inline void Shutdown( CExecutiveManager *manager )
    {
        // Must not have any non-cleaned-up hazard here.
        assert( this->hazardStack.GetCount() == 0 );
    }

    inline stackObjectHazardRegistry& operator = ( const stackObjectHazardRegistry& ) = delete;

private:
    struct hazardStackEntry
    {
        inline hazardStackEntry( void ) noexcept
        {
            this->intf = nullptr;
            this->delete_on_removal = false;
        }
        inline hazardStackEntry( const hazardStackEntry&& ) = delete;
        inline hazardStackEntry( hazardStackEntry&& right ) noexcept
        {
            this->intf = right.intf;
            this->delete_on_removal = right.delete_on_removal;

            right.intf = nullptr;
            right.delete_on_removal = false;
        }

        inline void cleanup( void ) noexcept
        {
            // To be honest it makes no sense to skip clean-up execution so we just do it.
            this->intf->TerminateHazard();

            if ( this->delete_on_removal )
            {
                eir::static_del_struct <hazardPreventionInterface, hazardPreventionInterface::dyn_alloc> ( nullptr, this->intf );
            }
        }

        inline hazardStackEntry& operator = ( hazardStackEntry&& right ) noexcept
        {
            this->~hazardStackEntry();

            return *new (this) hazardStackEntry( std::move( right ) );
        }
        inline hazardStackEntry& operator = ( const hazardStackEntry& right ) = delete;

        hazardPreventionInterface *intf;
        bool delete_on_removal;
    };

    eir::Vector <hazardStackEntry, NatExecStandardObjectAllocator> hazardStack;

    // We do not require any locks for this class, because:
    // * if the stack is terminating then it can only be accessed by the terminator (there can be only one).
    // * if the stack is running then it can only be accessed by the runtime (there can be only one).

public:
    inline void PushHazard( hazardPreventionInterface *intf, bool delete_on_removal = false )
    {
        hazardStackEntry entry;
        entry.intf = intf;
        entry.delete_on_removal = delete_on_removal;

        this->hazardStack.AddToBack( std::move( entry ) );
    }

    inline void PopHazard( bool execute_handler = false )
    {
        if ( this->hazardStack.GetCount() > 0 )
        {
            hazardStackEntry& top_entry = this->hazardStack.GetBack();

            hazardPreventionInterface *intf = top_entry.intf;

            if ( execute_handler )
            {
                intf->TerminateHazard();
            }

            if ( top_entry.delete_on_removal )
            {
                eir::static_del_struct <hazardPreventionInterface, hazardPreventionInterface::dyn_alloc> ( nullptr, intf );
            }

            this->hazardStack.RemoveFromBack();
        }
    }

private:
    // Called to execute and pop all handlers.
    // Note that the hazard registry must be in immutable state here before calling this function.
    inline void PurgeHazards( CExecutiveManager *manager ) noexcept
    {
        size_t item_count = this->hazardStack.GetCount();

        for ( size_t n = 0; n < item_count; n++ )
        {
            hazardStackEntry& entry = this->hazardStack[ item_count - n - 1 ];

            hazardPreventionInterface *intf = entry.intf;

            // Process the hazard.
            intf->TerminateHazard();

            if ( entry.delete_on_removal )
            {
                eir::static_del_struct <hazardPreventionInterface, hazardPreventionInterface::dyn_alloc> ( nullptr, intf );
            }
        }

        // Remove all items.
        this->hazardStack.Clear();
    }
};

// Then we need an environment that takes care of all hazardous objects.
struct executiveHazardManagerEnv : public cleanupInterface
{
private:
    struct stackObjectHazardRegistry_fiber : public stackObjectHazardRegistry
    {
        inline stackObjectHazardRegistry_fiber( CFiberImpl *fiber ) : stackObjectHazardRegistry( fiber->manager )
        {
            return;
        }

        inline void Initialize( CFiberImpl *fiber )
        {
            stackObjectHazardRegistry::Initialize( fiber->manager );
        }

        inline void Shutdown( CFiberImpl *fiber )
        {
            stackObjectHazardRegistry::Shutdown( fiber->manager );
        }
    };

    struct stackObjectHazardRegistry_thread : public stackObjectHazardRegistry
    {
        inline stackObjectHazardRegistry_thread( CExecThreadImpl *thread ) : stackObjectHazardRegistry( thread->manager )
        {
            return;
        }

        inline void Initialize( CExecThreadImpl *thread )
        {
            stackObjectHazardRegistry::Initialize( thread->manager );
        }

        inline void Shutdown( CExecThreadImpl *thread )
        {
            stackObjectHazardRegistry::Shutdown( thread->manager );
        }
    };

public:
    // We want to register the hazard object struct in threads and fibers.
    privateFiberEnvironment::fiberFactory_t::pluginOffset_t _fiberHazardOffset;
    privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t _threadHazardOffset;

    inline void Initialize( CExecutiveManagerNative *manager )
    {
        // Register the fiber plugin.
        privateFiberEnvironment::fiberFactory_t::pluginOffset_t fiberPluginOff = privateFiberEnvironment::fiberFactory_t::INVALID_PLUGIN_OFFSET;

        privateFiberEnvironment *fiberEnv = privateFiberEnvironmentRegister.get().GetPluginStruct( manager );

        if ( fiberEnv )
        {
            fiberPluginOff = 
                fiberEnv->fiberFact.RegisterDependantStructPlugin <stackObjectHazardRegistry_fiber> ( privateFiberEnvironment::fiberFactory_t::ANONYMOUS_PLUGIN_ID );
        }

        this->_fiberHazardOffset = fiberPluginOff;

        // Register the thread plugin.
        privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t threadPluginOff = privateThreadEnvironment::threadPluginContainer_t::INVALID_PLUGIN_OFFSET;

        privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( manager );

        if ( threadEnv )
        {
            threadPluginOff =
                threadEnv->threadPlugins.RegisterDependantStructPlugin <stackObjectHazardRegistry_thread> ( privateThreadEnvironment::threadPluginContainer_t::ANONYMOUS_PLUGIN_ID );
        }

        this->_threadHazardOffset = threadPluginOff;

        // Add ourselves to cleanup.
        manager->RegisterThreadCleanup( this );
    }

    inline void Shutdown( CExecutiveManagerNative *manager )
    {
        // Remove ourselves from cleanup.
        manager->UnregisterThreadCleanup( this );

        if ( privateThreadEnvironment::threadPluginContainer_t::IsOffsetValid( this->_threadHazardOffset ) )
        {
            privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( manager );

            if ( threadEnv )
            {
                threadEnv->threadPlugins.UnregisterPlugin( this->_threadHazardOffset );
            }
        }

        if ( privateFiberEnvironment::fiberFactory_t::IsOffsetValid( this->_fiberHazardOffset ) )
        {
            privateFiberEnvironment *fiberEnv = privateFiberEnvironmentRegister.get().GetPluginStruct( manager );

            if ( fiberEnv )
            {
                fiberEnv->fiberFact.UnregisterPlugin( this->_fiberHazardOffset );
            }
        }
    }

    inline stackObjectHazardRegistry* GetFiberHazardRegistry( CFiberImpl *fiber )
    {
        stackObjectHazardRegistry *reg = nullptr;

        if ( privateFiberEnvironment::fiberFactory_t::IsOffsetValid( this->_fiberHazardOffset ) )
        {
            reg = privateFiberEnvironment::fiberFactory_t::RESOLVE_STRUCT <stackObjectHazardRegistry_fiber> ( fiber, this->_fiberHazardOffset );
        }

        return reg;
    }

    inline stackObjectHazardRegistry* GetThreadHazardRegistry( CExecThreadImpl *thread )
    {
        stackObjectHazardRegistry *reg = nullptr;

        if ( privateThreadEnvironment::threadPluginContainer_t::IsOffsetValid( this->_threadHazardOffset ) )
        {
            reg = privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <stackObjectHazardRegistry_thread> ( thread, this->_threadHazardOffset );
        }

        return reg;
    }

    inline void PurgeThreadHazards( CExecThreadImpl *theThread ) noexcept
    {
#ifdef _DEBUG
        eThreadStatus threadStatus = theThread->GetStatus();

        assert( theThread->IsThreadCancelling() || threadStatus == THREAD_TERMINATING || threadStatus == THREAD_TERMINATED );
#endif //_DEBUG

        CExecutiveManager *execManager = theThread->manager;

        // First the thread stack.
        {
            stackObjectHazardRegistry *reg = this->GetThreadHazardRegistry( theThread );

            if ( reg )
            {
                reg->PurgeHazards( execManager );
            }
        }

        // Now the fiber stack.
        {
            threadFiberStackIterator fiberIter( theThread );

            while ( fiberIter.IsEnd() == false )
            {
                CFiberImpl *curFiber = fiberIter.Resolve();

                if ( curFiber )
                {
                    stackObjectHazardRegistry *reg = this->GetFiberHazardRegistry( curFiber );

                    if ( reg )
                    {
                        reg->PurgeHazards( execManager );
                    }
                }

                fiberIter.Increment();
            }
        }
    }

    // Only call this function on terminated fibers.
    inline void PurgeFiberHazards( CFiberImpl *theFiber ) noexcept
    {
#ifdef _DEBUG
        assert( theFiber->is_terminated() == true );
#endif //_DEBug

        CExecutiveManagerNative *nativeMan = theFiber->manager;

        stackObjectHazardRegistry *reg = this->GetFiberHazardRegistry( theFiber );

        if ( reg )
        {
            reg->PurgeHazards( nativeMan );
        }
    }

    void OnCleanup( CExecThread *thread ) noexcept override
    {
        CExecThreadImpl *nativeThread = (CExecThreadImpl*)thread;
        
        this->PurgeThreadHazards( nativeThread );
    }
};

typedef PluginDependantStructRegister <executiveHazardManagerEnv, executiveManagerFactory_t> executiveHazardManagerEnvRegister_t;

extern constinit optional_struct_space <executiveHazardManagerEnvRegister_t> executiveHazardManagerEnvRegister;

END_NATIVE_EXECUTIVE

#endif //_STACK_HAZARD_MANAGEMENT_INTERNALS_