/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.cond.cpp
*  PURPOSE:     Hazard-safe conditional variable implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "CExecutiveManager.cond.h"

#include "CExecutiveManager.cond.hxx"
#include "CExecutiveManager.evtwait.hxx"

BEGIN_NATIVE_EXECUTIVE

constinit optional_struct_space <condNativeEnvRegister_t> condNativeEnvRegister;

void condVarNativeEnv::condVarThreadPlugin::Initialize( CExecThreadImpl *thread )
{
    //CExecutiveManagerNative *manager = thread->manager;

    // We initially do not wait on any condition variable.
    this->waitingOnVar = nullptr;
}

void condVarNativeEnv::condVarThreadPlugin::Shutdown( CExecThreadImpl *thread )
{
    //CExecutiveManagerNative *manager = thread->manager;

    // Make sure we are not waiting on any condVar anymore.
    // This is guaranteed by the thread logic and hazard management system.
    assert( this->waitingOnVar == nullptr );
}

void condVarNativeEnv::condVarThreadPlugin::TerminateHazard( void ) noexcept
{
    // If we get here then the hazard it correctly initialized.
    // This is made secure because we take the threadState lock in the Wait method.

    // Wake the thread.
    // The mechanism of the conditional variable will make sure he cannot get into
    // waiting state again.
    CCondVarImpl *waitingOnVar = this->waitingOnVar;

    if ( waitingOnVar != nullptr )
    {
        // It could be set to nullptr if the thread was signalled instead of running out of time.

        // The variable may never turn invalid because any thread that might have used it has to have
        // completely terminated before it can be released.

        CReadWriteWriteContextSafe <> ctx_unwait( waitingOnVar->lockAtomicCalls );

        // Could have changed if another Signal call pre-empted us.
        if ( this->waitingOnVar == waitingOnVar )
        {
            CExecutiveManagerNative *nativeMan = waitingOnVar->manager;

            condVarNativeEnv *condEnv = condNativeEnvRegister.get().GetPluginStruct( nativeMan );

            assert( condEnv != nullptr );

            // We improve our logic by just waiking ourselves up, not everyone that is waiting.
            this->condRegister.unwait( nativeMan, condEnv );

            // Do not forget to remove our node.
            LIST_REMOVE( this->condRegister.node );
        }
    }
}

CCondVarImpl::CCondVarImpl( CExecutiveManagerNative *manager )
{
    this->manager = manager;

    this->lockAtomicCalls = manager->CreateReadWriteLock();

    assert( this->lockAtomicCalls != nullptr );
}

CCondVarImpl::~CCondVarImpl( void )
{
    // TODO: wake up all threads that could be waiting at this conditional variable.

    manager->CloseReadWriteLock( this->lockAtomicCalls );
}

template <typename callbackType>
AINLINE bool CCondVarImpl::establish_wait_ctx( const callbackType& cb )
{
    CExecutiveManagerNative *nativeMan = this->manager;

    condVarNativeEnv *condEnv = condNativeEnvRegister.get().GetPluginStruct( nativeMan );

    assert( condEnv != nullptr );

    CExecThreadImpl *nativeThread = (CExecThreadImpl*)nativeMan->GetCurrentThread();

    // Could be nullptr if the environment is terminating.
    assert( nativeThread != nullptr );

    // If we are terminating then just continue that path.
    // This prevents race-conditions of the thread-local condVarThreadPlugin if the user decided to
    // intercept the threadTerminationException and still use conditional variables.
    // We also protect against thread cancellation state switches: if the thread is not cancelling
    // and not terminating then we guarrantee that no hazard-cleanup handler is running!
    nativeThread->CheckTerminationRequest();

    condVarNativeEnv::condVarThreadPlugin *threadCondEnv = condEnv->GetThreadCondEnv( nativeThread );

    assert( threadCondEnv != nullptr );

    // Get the thread waiter event.
    CEvent *evtWaiter = GetCurrentThreadWaiterEvent( manager, nativeThread );

    // Put the thread into waiting hazard mode.
    {
        CReadWriteWriteContextSafe <> ctxWaitCall( this->lockAtomicCalls );

        // We set ourselves to wait for a signal.
        // This thing can only be released by a call to Signal (and possible hazard resolver).
        evtWaiter->Set( true );

        // We need to know what conditional variable we wait on.
        threadCondEnv->waitingOnVar = this;

        // Register ourselves in the conditional variable waiter list.
        LIST_INSERT( this->listWaitingThreads.root, threadCondEnv->condRegister.node );
    }

    // Make sure that our hazard can be resolved.
    // * if the thread is terminating then we terminate here with an exception because in
    //   termination we do not want to take any new risks.
    PushHazard( nativeMan, threadCondEnv );

    cb( evtWaiter );

    // Remove our hazard again.
    PopHazard( nativeMan );

    // Remove the thread from waiting hazard mode.
    bool hasBeenWokenUpBySignal = false;
    {
        // If we are waiting on the running thread, then we terminate this relationship.
        // We either have reached this due to timeout or because an OS signal has woken ourselves up (Linux).
        // OS signals are not to be confused with CCondVar Signal method.
        CReadWriteWriteContextSafe <> ctxRemoveWaiting( this->lockAtomicCalls );

        if ( CCondVarImpl *waitingOnVar = threadCondEnv->waitingOnVar )
        {
            assert( waitingOnVar == this );

            LIST_REMOVE( threadCondEnv->condRegister.node );

            threadCondEnv->waitingOnVar = nullptr;
        }
        else
        {
            // Since we are not registered as waiting on the running thread, we must have been
            // woken up by the Signal method. Thus we return that we were not spuriously woken
            // up!
            hasBeenWokenUpBySignal = true;
        }
    }

    return hasBeenWokenUpBySignal;
}

void CCondVarImpl::Wait( CReadWriteWriteContextSafe <>& ctxLock )
{
    this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            auto *userLock = ctxLock.GetCurrentLock();

            ctxLock.Suspend();

            // Do the wait.
            evtWaiter->Wait();

            // We have been revived by a signal, so let us continue.
            ctxLock = userLock;
        }
    );
}

void CCondVarImpl::Wait( CSpinLockContext& ctxLock )
{
    this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            auto *userLock = ctxLock.GetCurrentLock();

            ctxLock.Suspend();

            // Do the wait.
            evtWaiter->Wait();

            // We have been revived by a signal, so let us continue.
            ctxLock = userLock;
        }
    );
}

void CCondVarImpl::Wait( CUnfairMutexContext& ctxLock )
{
    this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            auto *userLock = ctxLock.release();

            // Do the wait.
            evtWaiter->Wait();

            // We have been revived by a signal, so let us continue.
            ctxLock = *userLock;
        }
    );
}

bool CCondVarImpl::WaitTimed( CReadWriteWriteContextSafe <>& ctxLock, unsigned int waitMS )
{
    return this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            auto *userLock = ctxLock.GetCurrentLock();

            ctxLock.Suspend();

            // Do the wait.
            evtWaiter->WaitTimed( waitMS );

            // We have been revived by a signal, so let us continue.
            ctxLock = userLock;
        }
    );
}

bool CCondVarImpl::WaitTimed( CSpinLockContext& ctxLock, unsigned int waitMS )
{
    return this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            auto *userLock = ctxLock.GetCurrentLock();

            ctxLock.Suspend();

            // Do the wait.
            // We must not use the result of this waiter-variable because
            // the effects of it are considered purely spurious.
            evtWaiter->WaitTimed( waitMS );

            // We have been revived by a signal, so let us continue.
            ctxLock = userLock;
        }
    );
}

bool CCondVarImpl::WaitTimed( CUnfairMutexContext& ctxLock, unsigned int waitMS )
{
    return this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            auto *userLock = ctxLock.release();

            // Do the wait.
            // We must not use the result of this waiter-variable because
            // the effects of it are considered purely spurious.
            evtWaiter->WaitTimed( waitMS );

            // We have been revived by a signal, so let us continue.
            ctxLock = *userLock;
        }
    );
}

void CCondVarImpl::WaitByLock( CReadWriteLock *lock )
{
    this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            lock->LeaveCriticalWriteRegion();

            // Do the wait.
            evtWaiter->Wait();

            // We have been revived by a signal, so let us continue.
            lock->EnterCriticalWriteRegion();
        }
    );
}

void CCondVarImpl::WaitByLock( CThreadReentrantReadWriteLock *lock )
{
    this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            lock->UnlockWrite();

            // Do the wait.
            evtWaiter->Wait();

            // We have been revived by a signal, so let us continue.
            lock->LockWrite();
        }
    );
}

void CCondVarImpl::WaitByLock( CSpinLock *lock )
{
    this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            lock->unlock();

            // Do the wait.
            evtWaiter->Wait();

            // We have been revived by a signal, so let us continue.
            lock->lock();
        }
    );
}

void CCondVarImpl::WaitByLock( CUnfairMutex *lock )
{
    this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            lock->unlock();

            // Do the wait.
            evtWaiter->Wait();

            // We have been revived by a signal, so let us continue.
            lock->lock();
        }
    );
}

bool CCondVarImpl::WaitTimedByLock( CReadWriteLock *lock, unsigned int waitMS )
{
    return this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            lock->LeaveCriticalWriteRegion();

            // Do the wait.
            evtWaiter->WaitTimed( waitMS );

            // We have been revived by a signal, so let us continue.
            lock->EnterCriticalWriteRegion();
        }
    );
}

bool CCondVarImpl::WaitTimedByLock( CThreadReentrantReadWriteLock *lock, unsigned int waitMS )
{
    return this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            lock->UnlockWrite();

            // Do the wait.
            evtWaiter->WaitTimed( waitMS );

            // We have been revived by a signal, so let us continue.
            lock->LockWrite();
        }
    );
}

bool CCondVarImpl::WaitTimedByLock( CSpinLock *lock, unsigned int waitMS )
{
    return this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            lock->unlock();

            // Do the wait.
            evtWaiter->WaitTimed( waitMS );

            // We have been revived by a signal, so let us continue.
            lock->lock();
        }
    );
}

bool CCondVarImpl::WaitTimedByLock( CUnfairMutex *lock, unsigned int waitMS )
{
    return this->establish_wait_ctx(
        [&]( CEvent *evtWaiter )
        {
            // Release all locks because we are safe.
            lock->unlock();

            // Do the wait.
            evtWaiter->WaitTimed( waitMS );

            // We have been revived by a signal, so let us continue.
            lock->lock();
        }
    );
}

void perThreadCondVarRegistration::unwait( CExecutiveManagerNative *nativeMan, condVarNativeEnv *condEnv )
{
    condVarNativeEnv::condVarThreadPlugin *threadPlugin = LIST_GETITEM( condVarNativeEnv::condVarThreadPlugin, this, condRegister );

    CExecThreadImpl *nativeThread = condEnv->BackResolveThread( threadPlugin );

    // Set the thread to not wait anymore.
    // Should open the floodgates.
    CEvent *evtWaiter = GetCurrentThreadWaiterEvent( nativeMan, nativeThread );

    evtWaiter->Set( false );

    // We are no longer waiting.
    threadPlugin->waitingOnVar = nullptr;
}

size_t CCondVarImpl::Signal( void )
{
    CExecutiveManagerNative *nativeMan = this->manager;

    condVarNativeEnv *condEnv = condNativeEnvRegister.get().GetPluginStruct( nativeMan );

    if ( !condEnv )
        return 0;

    // We need to have a sure-fire go ahead for the list of waiting threads.
    CReadWriteWriteContextSafe <> ctxSignalCall( this->lockAtomicCalls );

    size_t unwait_cnt = 0;

    // The idea is to wake all threads that reside in the waiting list.
    // To do that we first reference all threads, so we can safely use their data + plugins.
    // (the list of waiting threads will be allowed to mutate when we release em from their sleep).
    LIST_FOREACH_BEGIN( perThreadCondVarRegistration, this->listWaitingThreads.root, node )

        item->unwait( nativeMan, condEnv );

        unwait_cnt++;

    LIST_FOREACH_END

    // We have no more waiting threads.
    LIST_CLEAR( this->listWaitingThreads.root );

    return unwait_cnt;
}

size_t CCondVarImpl::SignalCount( size_t maxSignalCount )
{
    CExecutiveManagerNative *nativeMan = this->manager;

    condVarNativeEnv *condEnv = condNativeEnvRegister.get().GetPluginStruct( nativeMan );

    if ( !condEnv )
        return 0;

    // We need to have a sure-fire go ahead for the list of waiting threads.
    CReadWriteWriteContextSafe <> ctxSignalCall( this->lockAtomicCalls );

    // Just fetch the thread that has waited the longest, a couple of times.
    size_t cur_wake_count = 0;

    while ( cur_wake_count < maxSignalCount && LIST_EMPTY( this->listWaitingThreads.root ) == false )
    {
        perThreadCondVarRegistration *waiting = LIST_GETITEM( perThreadCondVarRegistration, this->listWaitingThreads.root.prev, node );

        waiting->unwait( nativeMan, condEnv );

        // It is not waiting anymore so remove it from the list.
        LIST_REMOVE( waiting->node );

        cur_wake_count++;
    }

    return cur_wake_count;
}

void CCondVar::Wait( CReadWriteWriteContextSafe <>& ctxLock )                               { ((CCondVarImpl*)this)->Wait( ctxLock ); }
void CCondVar::Wait( CSpinLockContext& ctxLock )                                            { ((CCondVarImpl*)this)->Wait( ctxLock ); }
void CCondVar::Wait( CUnfairMutexContext& ctxLock )                                         { ((CCondVarImpl*)this)->Wait( ctxLock ); }
bool CCondVar::WaitTimed( CReadWriteWriteContextSafe <>& ctxLock, unsigned int waitMS )     { return ((CCondVarImpl*)this)->WaitTimed( ctxLock, waitMS ); }
bool CCondVar::WaitTimed( CSpinLockContext& ctxLock, unsigned int waitMS )                  { return ((CCondVarImpl*)this)->WaitTimed( ctxLock, waitMS ); }
bool CCondVar::WaitTimed( CUnfairMutexContext& ctxLock, unsigned int waitMS )               { return ((CCondVarImpl*)this)->WaitTimed( ctxLock, waitMS ); }
void CCondVar::WaitByLock( CReadWriteLock *lock )                                           { ((CCondVarImpl*)this)->WaitByLock( lock ); }
void CCondVar::WaitByLock( CThreadReentrantReadWriteLock *lock )                            { ((CCondVarImpl*)this)->WaitByLock( lock ); }
void CCondVar::WaitByLock( CSpinLock *lock )                                                { ((CCondVarImpl*)this)->WaitByLock( lock ); }
void CCondVar::WaitByLock( CUnfairMutex *lock )                                             { ((CCondVarImpl*)this)->WaitByLock( lock ); }
bool CCondVar::WaitTimedByLock( CReadWriteLock *lock, unsigned int waitMS )                 { return ((CCondVarImpl*)this)->WaitTimedByLock( lock, waitMS ); }
bool CCondVar::WaitTimedByLock( CThreadReentrantReadWriteLock *lock, unsigned int waitMS )  { return ((CCondVarImpl*)this)->WaitTimedByLock( lock, waitMS ); }
bool CCondVar::WaitTimedByLock( CSpinLock *lock, unsigned int waitMS )                      { return ((CCondVarImpl*)this)->WaitTimedByLock( lock, waitMS ); }
bool CCondVar::WaitTimedByLock( CUnfairMutex *lock, unsigned int waitMS )                   { return ((CCondVarImpl*)this)->WaitTimedByLock( lock, waitMS ); }
size_t CCondVar::Signal( void )                                                             { return ((CCondVarImpl*)this)->Signal(); }
size_t CCondVar::SignalCount( size_t maxWakeUpCount )                                       { return ((CCondVarImpl*)this)->SignalCount( maxWakeUpCount ); }

CExecutiveManager* CCondVar::GetManager( void )
{
    CCondVarImpl *condNative = (CCondVarImpl*)this;

    return condNative->manager;
}

CCondVar* CExecutiveManager::CreateConditionVariable( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    CCondVarImpl *condVar = eir::dyn_new_struct <CCondVarImpl> ( memAlloc, nullptr, nativeMan );

    return condVar;
}

void CExecutiveManager::CloseConditionVariable( CCondVar *condVar )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    CCondVarImpl *nativeCondVar = (CCondVarImpl*)condVar;

    eir::dyn_del_struct <CCondVarImpl> ( memAlloc, nullptr, nativeCondVar );
}

void registerConditionalVariables( void )
{
    condNativeEnvRegister.Construct( executiveManagerFactory );
}

void unregisterConditionalVariables( void )
{
    condNativeEnvRegister.Destroy();
}

END_NATIVE_EXECUTIVE
