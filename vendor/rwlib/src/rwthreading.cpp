/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwthreading.cpp
*  PURPOSE:     Threading implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwthreading.hxx"

#ifdef RWLIB_ENABLE_THREADING

using namespace NativeExecutive;

#endif //RWLIB_ENABLE_THREADING

namespace rw
{

#ifdef RWLIB_ENABLE_THREADING

optional_struct_space <threadingEnvRegister_t> threadingEnv;

inline threadingEnvironment* GetThreadingEnv( Interface *engineInterface )
{
    return threadingEnv.get().GetPluginStruct( (EngineInterface*)engineInterface );
}

inline void* GetRWLockObject( rwlock *theLock )
{
    return ( theLock );
}

#endif //RWLIB_ENABLE_THREADING

// Read/Write lock implementation.
void rwlock::enter_read( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CReadWriteLock*)GetRWLockObject( this ))->EnterCriticalReadRegion();
#endif //RWLIB_ENABLE_THREADING
}

void rwlock::leave_read( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CReadWriteLock*)GetRWLockObject( this ))->LeaveCriticalReadRegion();
#endif //RWLIB_ENABLE_THREADING
}

void rwlock::enter_write( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CReadWriteLock*)GetRWLockObject( this ))->EnterCriticalWriteRegion();
#endif //RWLIB_ENABLE_THREADING
}

void rwlock::leave_write( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CReadWriteLock*)GetRWLockObject( this ))->LeaveCriticalWriteRegion();
#endif //RWLIB_ENABLE_THREADING
}

bool rwlock::try_enter_read( void )
{
#ifdef RWLIB_ENABLE_THREADING
    return ((CReadWriteLock*)GetRWLockObject( this ))->TryEnterCriticalReadRegion();
#else
    return true;
#endif //RWLIB_ENABLE_THREADING
}

bool rwlock::try_enter_write( void )
{
#ifdef RWLIB_ENABLE_THREADING
    return ((CReadWriteLock*)GetRWLockObject( this ))->TryEnterCriticalWriteRegion();
#else
    return true;
#endif //RWLIB_ENABLE_THREADING
}

#ifdef RWLIB_ENABLE_THREADING

inline void* GetReentrantRWLockObject( reentrant_rwlock *theLock )
{
    return ( theLock );
}

struct rwlock_implementation : public rwlock
{
    static inline size_t GetStructSize( threadingEnvironment *threadEnv )
    {
        return threadEnv->nativeMan->GetReadWriteLockStructSize();
    }

    static inline size_t GetStructAlignment( threadingEnvironment *threadEnv )
    {
        return threadEnv->nativeMan->GetReadWriteLockStructAlign();
    }

    static inline rwlock* Construct( threadingEnvironment *threadEnv, void *mem )
    {
        rwlock_implementation *rwlock = (rwlock_implementation*)threadEnv->nativeMan->CreatePlacedReadWriteLock( mem );

        return rwlock;
    }

    static inline void Destroy( threadingEnvironment *threadEnv, rwlock *theLock )
    {
        threadEnv->nativeMan->ClosePlacedReadWriteLock( (CReadWriteLock*)theLock );
    }
};

#endif //RWLIB_ENABLE_THREADING

// Reentrant Read/Write lock implementation.
void reentrant_rwlock::enter_read( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CThreadReentrantReadWriteLock*)GetReentrantRWLockObject( this ))->LockRead();
#endif //RWLIB_ENABLE_THREADING
}

void reentrant_rwlock::leave_read( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CThreadReentrantReadWriteLock*)GetReentrantRWLockObject( this ))->UnlockRead();
#endif //RWLIB_ENABLE_THREADING
}

void reentrant_rwlock::enter_write( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CThreadReentrantReadWriteLock*)GetReentrantRWLockObject( this ))->LockWrite();
#endif //RWLIB_ENABLE_THREADING
}

void reentrant_rwlock::leave_write( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CThreadReentrantReadWriteLock*)GetReentrantRWLockObject( this ))->UnlockWrite();
#endif //RWLIB_ENABLE_THREADING
}

bool reentrant_rwlock::try_enter_read( void )
{
#ifdef RWLIB_ENABLE_THREADING
    return ((CThreadReentrantReadWriteLock*)GetReentrantRWLockObject( this ))->TryLockRead();
#else
    return true;
#endif //RWLIB_ENABLE_THREADING
}

bool reentrant_rwlock::try_enter_write( void )
{
#ifdef RWLIB_ENABLE_THREADING
    return ((CThreadReentrantReadWriteLock*)GetReentrantRWLockObject( this ))->TryLockWrite();
#else
    return true;
#endif //RWLIB_ENABLE_THREADING
}

#ifdef RWLIB_ENABLE_THREADING

struct reentrant_rwlock_implementation : public reentrant_rwlock
{
    static inline size_t GetStructSize( threadingEnvironment *threadEnv )
    {
        return threadEnv->nativeMan->GetThreadReentrantReadWriteLockStructSize();
    }

    static inline size_t GetStructAlignment( threadingEnvironment *threadEnv )
    {
        return threadEnv->nativeMan->GetThreadReentrantReadWriteLockAlignment();
    }

    static reentrant_rwlock* Construct( threadingEnvironment *threadEnv, void *mem )
    {
        reentrant_rwlock_implementation *rwlock =
            (reentrant_rwlock_implementation*)threadEnv->nativeMan->CreatePlacedThreadReentrantReadWriteLock( mem );

        return rwlock;
    }

    static void Destroy( threadingEnvironment *threadEnv, reentrant_rwlock *theLock )
    {
        threadEnv->nativeMan->ClosePlacedThreadReentrantReadWriteLock( (CThreadReentrantReadWriteLock*)theLock );
    }
};

#endif //RWLIB_ENABLE_THREADING

void unfair_mutex::enter( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CUnfairMutex*)this)->lock();
#endif //RWLIB_ENABLE_THREADING
}

void unfair_mutex::leave( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CUnfairMutex*)this)->unlock();
#endif //RWLIB_ENABLE_THREADING
}

#ifdef RWLIB_ENABLE_THREADING

struct unfair_mutex_implementation : public unfair_mutex
{
    static inline size_t GetStructSize( threadingEnvironment *threadEnv )
    {
        return threadEnv->nativeMan->GetUnfairMutexStructSize();
    }

    static inline size_t GetStructAlignment( threadingEnvironment *threadEnv )
    {
        return threadEnv->nativeMan->GetUnfairMutexAlignment();
    }

    static unfair_mutex* Construct( threadingEnvironment *threadEnv, void *mem )
    {
        return (unfair_mutex_implementation*)threadEnv->nativeMan->CreatePlacedUnfairMutex( mem );
    }

    static void Destroy( threadingEnvironment *threadEnv, unfair_mutex *mtx )
    {
        threadEnv->nativeMan->ClosePlacedUnfairMutex( (CUnfairMutex*)mtx );
    }
};

#endif //RWLIB_ENABLE_THREADING

void sync_event::set( bool doWait )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CEvent*)this)->Set( doWait );
#endif //RWLIB_ENABLE_THREADING
}

void sync_event::wait( void )
{
#ifdef RWLIB_ENABLE_THREADING
    ((CEvent*)this)->Wait();
#endif //RWLIB_ENABLE_THREADING
}

bool sync_event::wait_timed( unsigned int waitMS )
{
#ifdef RWLIB_ENABLE_THREADING
    return ((CEvent*)this)->WaitTimed( waitMS );
#else
    return false;
#endif //RWLIB_ENABLE_THREADING
}

#ifdef RWLIB_ENABLE_THREADING

struct sync_event_implementation
{
    static inline size_t GetStructSize( threadingEnvironment *threadEnv )
    {
        return threadEnv->nativeMan->GetEventStructSize();
    }

    static inline size_t GetStructAlignment( threadingEnvironment *threadEnv )
    {
        return threadEnv->nativeMan->GetEventAlignment();
    }

    static sync_event* Construct( threadingEnvironment *threadEnv, void *mem )
    {
        return (sync_event*)threadEnv->nativeMan->CreatePlacedEvent( mem );
    }

    static void Destroy( threadingEnvironment *threadEnv, sync_event *evt )
    {
        threadEnv->nativeMan->ClosePlacedEvent( (CEvent*)evt );
    }
};

#endif //RWLIB_ENABLE_THREADING

// Lock creation API.
rwlock* CreateReadWriteLock( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    size_t lockSize = rwlock_implementation::GetStructSize( threadEnv );

    if ( lockSize == 0 )
        return nullptr;

    void *lockMem = engineInterface->MemAllocateP( lockSize );

    if ( !lockMem )
    {
        return nullptr;
    }

    try
    {
        return rwlock_implementation::Construct( threadEnv, lockMem );
    }
    catch( ... )
    {
        engineInterface->MemFree( lockMem );

        throw;
    }
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

void CloseReadWriteLock( Interface *engineInterface, rwlock *theLock )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    rwlock_implementation::Destroy( threadEnv, theLock );

    engineInterface->MemFree( theLock );
#endif //RWLIB_ENABLE_THREADING
}

size_t GetReadWriteLockStructSize( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return rwlock_implementation::GetStructSize( threadEnv );
#else
    return 0;
#endif //RWLIB_ENABLE_THREADING
}

size_t GetReadWriteLockStructAlignment( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return rwlock_implementation::GetStructAlignment( threadEnv );
#else
    return 1;
#endif //RWLIB_ENABLE_THREADING
}

rwlock* CreatePlacedReadWriteLock( Interface *engineInterface, void *mem )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return rwlock_implementation::Construct( threadEnv, mem );
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

void ClosePlacedReadWriteLock( Interface *engineInterface, rwlock *theLock )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    rwlock_implementation::Destroy( threadEnv, theLock );
#endif //RWLIB_ENABLE_THREADING
}

reentrant_rwlock* CreateReentrantReadWriteLock( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    size_t lockSize = reentrant_rwlock_implementation::GetStructSize( threadEnv );

    if ( lockSize == 0 )
        return nullptr;

    size_t lockAlign = reentrant_rwlock_implementation::GetStructAlignment( threadEnv );

    void *lockMem = engineInterface->MemAllocateP( lockSize, lockAlign );

    if ( !lockMem )
    {
        return nullptr;
    }

    try
    {
        return reentrant_rwlock_implementation::Construct( threadEnv, lockMem );
    }
    catch( ... )
    {
        engineInterface->MemFree( lockMem );

        throw;
    }
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

void CloseReentrantReadWriteLock( Interface *engineInterface, reentrant_rwlock *theLock )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    reentrant_rwlock_implementation::Destroy( threadEnv, theLock );

    engineInterface->MemFree( theLock );
#endif //RWLIB_ENABLE_THREADING
}

size_t GetReeentrantReadWriteLockStructSize( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return reentrant_rwlock_implementation::GetStructSize( threadEnv );
#else
    return 0;
#endif //RWLIB_ENABLE_THREADING
}

size_t GetReentrantReadWriteLockAlignment( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return reentrant_rwlock_implementation::GetStructAlignment( threadEnv );
#else
    return 1;
#endif //RWLIB_ENABLE_THREADING
}

reentrant_rwlock* CreatePlacedReentrantReadWriteLock( Interface *engineInterface, void *mem )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return reentrant_rwlock_implementation::Construct( threadEnv, mem );
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

void ClosePlacedReentrantReadWriteLock( Interface *engineInterface, reentrant_rwlock *theLock )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    reentrant_rwlock_implementation::Destroy( threadEnv, theLock );
#endif //RWLIB_ENABLE_THREADING
}

unfair_mutex* CreateUnfairMutex( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    size_t lockSize = unfair_mutex_implementation::GetStructSize( threadEnv );

    if ( lockSize == 0 )
        return nullptr;

    size_t lockAlign = unfair_mutex_implementation::GetStructAlignment( threadEnv );

    void *lockMem = engineInterface->MemAllocateP( lockSize, lockAlign );

    if ( lockMem == nullptr )
    {
        return nullptr;
    }

    try
    {
        return unfair_mutex_implementation::Construct( threadEnv, lockMem );
    }
    catch( ... )
    {
        engineInterface->MemFree( lockMem );

        throw;
    }
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

void CloseUnfairMutex( Interface *engineInterface, unfair_mutex *mtx )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    unfair_mutex_implementation::Destroy( threadEnv, mtx );

    engineInterface->MemFree( mtx );
#endif //RWLIB_ENABLE_THREADING
}

size_t GetUnfairMutexStructSize( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return unfair_mutex_implementation::GetStructSize( threadEnv );
#else
    return 0;
#endif //RWLIB_ENABLE_THREADING
}

size_t GetUnfairMutexAlignment( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return unfair_mutex_implementation::GetStructAlignment( threadEnv );
#else
    return 1;
#endif //RWLIB_ENABLE_THREADING
}

unfair_mutex* CreatePlacedUnfairMutex( Interface *engineInterface, void *mem )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return unfair_mutex_implementation::Construct( threadEnv, mem );
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

void ClosePlacedUnfairMutex( Interface *engineInterface, unfair_mutex *mtx )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    unfair_mutex_implementation::Destroy( threadEnv, mtx );
#endif //RWLIB_ENABLE_THREADING
}

sync_event* CreateSyncEvent( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    size_t evtSize = sync_event_implementation::GetStructSize( threadEnv );

    if ( evtSize == 0 )
        return nullptr;

    size_t evtAlign = sync_event_implementation::GetStructAlignment( threadEnv );

    void *evtMem = engineInterface->MemAllocateP( evtSize, evtAlign );

    if ( evtMem == nullptr )
        return nullptr;

    try
    {
        return sync_event_implementation::Construct( threadEnv, evtMem );
    }
    catch( ... )
    {
        engineInterface->MemFree( evtMem );

        throw;
    }
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

void CloseSyncEvent( Interface *engineInterface, sync_event *evt )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    sync_event_implementation::Destroy( threadEnv, evt );

    engineInterface->MemFree( evt );
#endif //RWLIB_ENABLE_THREADING
}

size_t GetSyncEventStructSize( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return sync_event_implementation::GetStructSize( threadEnv );
#else
    return 0;
#endif //RWLIB_ENABLE_THREADING
}

size_t GetSyncEventAlignment( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return sync_event_implementation::GetStructAlignment( threadEnv );
#else
    return 1;
#endif //RWLIB_ENABLE_THREADING
}

sync_event* CreatePlacedSyncEvent( Interface *engineInterface, void *mem )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    return sync_event_implementation::Construct( threadEnv, mem );
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

void ClosePlacedSyncEvent( Interface *engineInterface, sync_event *evt )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    sync_event_implementation::Destroy( threadEnv, evt );
#endif //RWLIB_ENABLE_THREADING
}

#ifdef RWLIB_ENABLE_THREADING

// Thread API.
struct nativeexec_traverse
{
    threadEntryPoint_t ep;
    Interface *engineInterface;
    void *ud;
};

static void nativeexec_thread_entry( CExecThread *thisThread, void *ud )
{
    nativeexec_traverse *info = (nativeexec_traverse*)ud;

    threadEntryPoint_t ep = info->ep;
    Interface *engineInterface = info->engineInterface;
    void *user_ud = info->ud;

    // We do not need the userdata anymore.
    engineInterface->MemFree( ud );

    try
    {
        // Call into the usermode callback.
        ep( (thread_t)thisThread, engineInterface, user_ud );
    }
    catch( RwException& except )
    {
        // Post a warning that a thread ended in an exception.
        rwStaticString <wchar_t> errorMsg = DescribeException( engineInterface, except );

        engineInterface->PushWarningSingleTemplate( L"THREADING_WARN_FATALTHREADEXIT_TEMPLATE", L"message", errorMsg.GetConstString() );

        // We cleanly terminate.
    }
}

#endif //RWLIB_ENABLE_THREADING

thread_t MakeThread( Interface *engineInterface, threadEntryPoint_t entryPoint, void *ud )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    nativeexec_traverse *cb_info = (nativeexec_traverse*)engineInterface->MemAllocate( sizeof( nativeexec_traverse ) );
    cb_info->ep = entryPoint;
    cb_info->engineInterface = engineInterface;
    cb_info->ud = ud;

    CExecThread *threadHandle = threadEnv->nativeMan->CreateThread( nativeexec_thread_entry, cb_info );

    if ( threadHandle == nullptr )
    {
        // Need to release data if thread cannot launch.
        engineInterface->MemFree( cb_info );
    }

    return (thread_t)threadHandle;

    // Threads are created suspended. You have to kick them off using rw::ResumeThread.

    // Remember to close thread handles.
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

void CloseThread( Interface *engineInterface, thread_t threadHandle )
{
#ifdef RWLIB_ENABLE_THREADING
    // WARNING: doing this is an unsafe operation in certain circumstances.
    // always cleanly terminate threads instead of doing this.

    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    CExecThread *theThread = (CExecThread*)threadHandle;

    threadEnv->nativeMan->CloseThread( theThread );
#endif //RWLIB_ENABLE_THREADING
}

thread_t AcquireThread( Interface *engineInterface, thread_t threadHandle )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    CExecThread *theThread = (CExecThread*)threadHandle;

    return (thread_t)threadEnv->nativeMan->AcquireThread( theThread );
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

bool ResumeThread( Interface *engineInterface, thread_t threadHandle )
{
#ifdef RWLIB_ENABLE_THREADING
    CExecThread *theThread = (CExecThread*)threadHandle;

    return theThread->Resume();
#else
    return false;
#endif //RWLIB_ENABLE_THREADING
}

bool SuspendThread( Interface *engineInterface, thread_t threadHandle )
{
#ifdef RWLIB_ENABLE_THREADING
    CExecThread *theThread = (CExecThread*)threadHandle;

    return theThread->Suspend();
#else
    return false;
#endif //RWLIB_ENABLE_THREADING
}

void JoinThread( Interface *engineInterface, thread_t threadHandle )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    CExecThread *theThread = (CExecThread*)threadHandle;

    threadEnv->nativeMan->JoinThread( theThread );
#endif //RWLIB_ENABLE_THREADING
}

void TerminateThread( Interface *engineInterface, thread_t threadHandle, bool waitOnRemote )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    CExecThread *theThread = (CExecThread*)threadHandle;

    threadEnv->nativeMan->TerminateThread( theThread, waitOnRemote );
#endif //RWLIB_ENABLE_THREADING
}

void CheckThreadHazards( Interface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    threadEnv->nativeMan->CheckHazardCondition();
#endif //RWLIB_ENABLE_THREADING
}

void ThreadingMarkAsTerminating( EngineInterface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    threadEnv->nativeMan->MarkAsTerminating();
#endif //RWLIB_ENABLE_THREADING
}

void PurgeActiveThreadingObjects( EngineInterface *engineInterface )
{
#ifdef RWLIB_ENABLE_THREADING
    // USE WITH FRIGGIN CAUTION.
    threadingEnvironment *threadEnv = GetThreadingEnv( engineInterface );

    threadEnv->nativeMan->PurgeActiveRuntimes();
    threadEnv->nativeMan->PurgeActiveThreads();
#endif //RWLIB_ENABLE_THREADING
}

void* GetThreadingNativeManager( Interface *intf )
{
#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *engineInterface = (EngineInterface*)intf;

    return GetNativeExecutive( engineInterface );
#else
    return nullptr;
#endif //RWLIB_ENABLE_THREADING
}

// Module initialization.
void registerThreadingEnvironment( void )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnv.Construct( engineFactory );
#endif //RWLIB_ENABLE_THREADING
}

void unregisterThreadingEnvironment( void )
{
#ifdef RWLIB_ENABLE_THREADING
    threadingEnv.Destroy();
#endif //RWLIB_ENABLE_THREADING
}

};
