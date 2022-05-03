/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.activityreg.cpp
*  PURPOSE:     Thread activity registration and handling.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "CExecutiveManager.rwlock.hxx"

#include "CExecutiveManager.thread.activityreg.hxx"

BEGIN_NATIVE_EXECUTIVE
   
struct threadActivityEnv
{
    AINLINE threadActivityEnv( CExecutiveManagerNative *execMan ) : items( eir::constr_with_alloc::DEFAULT, execMan )
    {
        return;
    }
    AINLINE threadActivityEnv( const threadActivityEnv& ) = delete;

    AINLINE threadActivityEnv& operator = ( const threadActivityEnv& ) = delete;

    AINLINE void Initialize( CExecutiveManagerNative *execMan )
    {
        this->suspension_ref_cnt = 0;
    }

    AINLINE void Shutdown( CExecutiveManagerNative *execMan )
    {
        assert( this->suspension_ref_cnt == 0 );
    }

    struct item
    {
        threadActivityInterface *intf;
        bool isInitialized = false;

        AINLINE item( void ) = default;
        AINLINE item( item&& ) = default;

        AINLINE bool operator == ( threadActivityInterface *intf ) const
        {
            return ( this->intf == intf );
        }

        AINLINE item& operator = ( item&& ) = default;
    };

    size_t suspension_ref_cnt;

    eir::Vector <item, NatExecStandardObjectAllocator> items;
};

static constinit optional_struct_space <PluginDependantStructRegister <threadActivityEnv, executiveManagerFactory_t>> threadActivityEnvRegister;
static constinit optional_struct_space <rwlockManagerItemRegister> lock_threadActivityEnv;

void CExecutiveManager::RegisterThreadActivity( threadActivityInterface *intf )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    CReadWriteWriteContext ctx_ta( lock_threadActivityEnv.get().GetReadWriteLock( nativeMan ) );

    auto *env = threadActivityEnvRegister.get().GetPluginStruct( nativeMan );

    // We first initialize the environment.
    intf->InitializeActivity( this );

    threadActivityEnv::item actItem;
    actItem.intf = intf;
    actItem.isInitialized = true;

    env->items.AddToBack( std::move( actItem ) );
}

void CExecutiveManager::UnregisterThreadActivity( threadActivityInterface *intf )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    CReadWriteWriteContext ctx_ta( lock_threadActivityEnv.get().GetReadWriteLock( nativeMan ) );

    auto *env = threadActivityEnvRegister.get().GetPluginStruct( nativeMan );

    size_t idx;

    if ( env->items.Find( intf, &idx ) )
    {
        bool isInitialized = ( env->items[idx].isInitialized );

        // Remove the activity item.
        env->items.RemoveByIndex( idx );

        if ( isInitialized )
        {
            // Shutdown the environment for a last time.
            intf->ShutdownActivity( this );
        }
    }
}

void suspend_thread_activities( CExecutiveManagerNative *execMan )
{
    CReadWriteWriteContext ctx_ta( lock_threadActivityEnv.get().GetReadWriteLock( execMan ) );

    auto *env = threadActivityEnvRegister.get().GetPluginStruct( execMan );

    size_t curSuspensionRefCount = env->suspension_ref_cnt;

    if ( curSuspensionRefCount == 0 )
    {
        for ( auto& item : env->items )
        {
            if ( item.isInitialized )
            {
                item.intf->ShutdownActivity( execMan );
                item.isInitialized = false;
            }
        }
    }

    env->suspension_ref_cnt = curSuspensionRefCount + 1;
}

void resume_thread_activities( CExecutiveManagerNative *execMan )
{
    CReadWriteWriteContext ctx_ta( lock_threadActivityEnv.get().GetReadWriteLock( execMan ) );

    auto *env = threadActivityEnvRegister.get().GetPluginStruct( execMan );

    size_t curSuspensionRefCount = env->suspension_ref_cnt;

    assert( curSuspensionRefCount > 0 );

    if ( curSuspensionRefCount == 1 )
    {
        for ( auto& item : env->items )
        {
            if ( item.isInitialized == false )
            {
                try
                {
                    item.intf->InitializeActivity( execMan );
                    item.isInitialized = true;
                }
                catch( ... )
                {
                    // The initialization has failed.
                    // Just keep it not-initialized then.
                }
            }
        }
    }

    env->suspension_ref_cnt = curSuspensionRefCount - 1;
}

void registerThreadActivityEnvironment( void )
{
    lock_threadActivityEnv.Construct();
    threadActivityEnvRegister.Construct( executiveManagerFactory );
}

void unregisterThreadActivityEnvironment( void )
{
    threadActivityEnvRegister.Destroy();
    lock_threadActivityEnv.Destroy();
}

END_NATIVE_EXECUTIVE