/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.barrier.cpp
*  PURPOSE:     Fence/barrier sync object implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "internal/CExecutiveManager.barrier.internal.h"

#include "CExecutiveManager.evtwait.hxx"

#include "PluginUtils.hxx"

BEGIN_NATIVE_EXECUTIVE

struct _barrier_ptd
{
    inline void Initialize( CExecThreadImpl *thread )
    {
        this->waitingOnBarrier = nullptr;
    }

    inline void Shutdown( CExecThreadImpl *thread )
    {
        assert( this->waitingOnBarrier == nullptr );
    }

    CBarrierImpl *waitingOnBarrier;

    RwListEntry <_barrier_ptd> waiting_node;
};

static constinit optional_struct_space <PluginDependantStructRegister <PerThreadPluginRegister <_barrier_ptd, true, false>, executiveManagerFactory_t>> _barrier_ptd_register;

static AINLINE CEvent* _barrier_pass( CBarrierImpl *nativeBarrier, CExecutiveManagerNative *nativeMan, CExecThreadImpl **currentThreadOut )
{
    CEvent *waitingEvent = nullptr;

    CSpinLockContext ctxAtomic( nativeBarrier->lock_atomic );

    // Make sure there are not too many people waiting on this barrier.
    unsigned int cur_waiting = nativeBarrier->waiting_count;
    unsigned int req_release_count = nativeBarrier->req_release_count;

    assert( cur_waiting < req_release_count );

    // We would be one more right now.
    cur_waiting++;

    if ( cur_waiting == req_release_count )
    {
        // Just release everyone from their waiting stance.
        // This is a sort of fair release because the guy that was waiting the longest gets to execute
        // first, at least in the line of waiters. He gets to execute globally first only-if he
        // actually surpasses the thread that woke him up!
        LIST_FOREACH_BEGIN( _barrier_ptd, nativeBarrier->list_waiting.root, waiting_node )

            CExecThreadImpl *theThread = BackResolveThreadPlugin( _barrier_ptd_register.get(), nativeMan, item );

            assert( theThread != nullptr );

            CEvent *waitingEvent = GetCurrentThreadWaiterEvent( nativeMan, theThread );

            assert( waitingEvent != nullptr );

            waitingEvent->Set( false );

            // Mark us as not waiting anymore.
            item->waitingOnBarrier = nullptr;

        LIST_FOREACH_END

        // Nobody is waiting anymore.
        LIST_CLEAR( nativeBarrier->list_waiting.root );

        cur_waiting = 0;
    }
    else
    {
        CExecThreadImpl *currentThread = (CExecThreadImpl*)nativeMan->GetCurrentThread();

        assert( currentThread != nullptr );

        auto *barrierEnv = _barrier_ptd_register.get().GetPluginStruct( nativeMan );

        assert( barrierEnv != nullptr );

        _barrier_ptd *barrierData = barrierEnv->GetThreadPlugin( currentThread );

        assert( barrierData != nullptr );
        assert( barrierData->waitingOnBarrier == nullptr );

        // Register us as waiting.
        barrierData->waitingOnBarrier = nativeBarrier;
        LIST_APPEND( nativeBarrier->list_waiting.root, barrierData->waiting_node );

        if ( currentThreadOut != nullptr )
        {
            *currentThreadOut = currentThread;
        }
    
        // Mark our event as waiting.
        waitingEvent = GetCurrentThreadWaiterEvent( nativeMan, currentThread );
        waitingEvent->Set( true );
    }

    // New waiting count.
    nativeBarrier->waiting_count = cur_waiting;

    return waitingEvent;
}

void CBarrier::Wait( void )
{
    CBarrierImpl *nativeBarrier = (CBarrierImpl*)this;

    CExecutiveManagerNative *nativeMan = nativeBarrier->nativeMan;
    
    CEvent *waitingEvent = _barrier_pass( nativeBarrier, nativeMan, nullptr );

    // If we were asked to wait then just do it.
    if ( waitingEvent != nullptr )
    {
        waitingEvent->Wait();

        // Wait until the barrier has finished releasing us all.
        // This is important because we do not want to preempt it and possibly corrupt
        // it's state.
        nativeBarrier->lock_atomic.fence();
    }

    // Since we did wait indefinately we for sure have entered the barrier legit.
}

bool CBarrier::WaitTimed( unsigned int wait_ms )
{
    CBarrierImpl *nativeBarrier = (CBarrierImpl*)this;

    CExecutiveManagerNative *nativeMan = nativeBarrier->nativeMan;

    CExecThreadImpl *currentThread;

    CEvent *waitingEvent = _barrier_pass( nativeBarrier, nativeMan, &currentThread );

    if ( waitingEvent == nullptr )
    {
        // We just entered the barrier.
        return true;
    }

    // Perform the waiting logic.
    bool wasWaitInterrupted = waitingEvent->WaitTimed( wait_ms );

    // If we were interrupted then we must have entered the barrier.
    // Otherwise we _could_ have entered it due to another thread waking us up in the nick of time.
    bool barrier_satisfied = true;

    if ( wasWaitInterrupted == false )
    {
        CSpinLockContext ctxCheckWake( nativeBarrier->lock_atomic );

        auto *barrierEnv = _barrier_ptd_register.get().GetPluginStruct( nativeMan );

        _barrier_ptd *barrierData = barrierEnv->GetThreadPlugin( currentThread );

        // Even though we could still be released by wait time, the sheduler could sort the instructions
        // so we are still released by the passing of a barrier.
        if ( barrierData->waitingOnBarrier != nullptr )
        {
            assert( barrierData->waitingOnBarrier == nativeBarrier );

            // Release us from the waiting information because time elapsed with no result.
            barrierData->waitingOnBarrier = nullptr;

            LIST_REMOVE( barrierData->waiting_node );

            // One less guy waiting.
            nativeBarrier->waiting_count--;

            // Report to the runtime that we did not successfully pass the barrier.
            barrier_satisfied = false;
        }
    }
    else
    {
        // Wait until the barrier has finished releasing us all.
        // This is important because we do not want to preempt it and possibly corrupt
        // it's state.
        nativeBarrier->lock_atomic.fence();
    }

    return barrier_satisfied;
}

CBarrier* CExecutiveManager::CreateBarrier( unsigned int req_release_count )
{
    // Check arguments for invalid stuff.
    if ( req_release_count == 0 )
    {
        return nullptr;
    }

    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    CBarrier *barrier = eir::dyn_new_struct <CBarrierImpl> ( memAlloc, nullptr, nativeMan, req_release_count );

    return barrier;
}

void CExecutiveManager::CloseBarrier( CBarrier *barrier )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    eir::dyn_del_struct <CBarrierImpl> ( memAlloc, nullptr, (CBarrierImpl*)barrier );
}

void registerBarriers( void )
{
    _barrier_ptd_register.Construct( executiveManagerFactory );
}

void unregisterBarriers( void )
{
    _barrier_ptd_register.Destroy();
}

END_NATIVE_EXECUTIVE