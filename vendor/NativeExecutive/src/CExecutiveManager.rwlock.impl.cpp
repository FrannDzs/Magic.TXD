/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.rwlock.impl.cpp
*  PURPOSE:     Read/Write lock internal implementation main
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

// Maybe we should manage NativeExecutive with a DynamicTypeSystem so we can offer all lock-types without complex
// configuration switching?

// Please be informed that because this lock uses memory allocation during lock-entering it could fail to enter and
// throw an exception (very rare case, can be omitted in not-critical applications).

#include "StdInc.h"

#include <atomic>

#include "internal/CExecutiveManager.spinlock.internal.h"

#include "CExecutiveManager.rwlock.hxx"
#include "CExecutiveManager.evtwait.hxx"

#include "PluginUtils.hxx"

BEGIN_NATIVE_EXECUTIVE

// Waiting type of threads for standard read-write locks.
enum class eLockRegType
{
    READER,
    WRITER
};

// Forward declaration.
struct _rwlock_standard_data;

// Registration unit into the read-write lock.
// Put into each thread.
struct _rwlock_standard_ptd
{
    inline void Initialize( CExecThreadImpl *thread )
    {
        this->lockWaitingOn = nullptr;
    }

    inline void Shutdown( CExecThreadImpl *thread )
    {
        // Kind of want to have no hazard-situation.
        // The hazard-resolver must have set it to nullptr anyway.
        // But the resolving must be done explicitly by user-mode code.
        assert( this->lockWaitingOn == nullptr );
    }

    // The pointer to the lock that this thread is waiting on.
    _rwlock_standard_data *lockWaitingOn;

    // The waiting-type, to consult how to treat us in a read-write lock.
    eLockRegType lockRegType;

    // Node into the lock that we are waiting on.
    RwListEntry <_rwlock_standard_ptd> lockNode;
};

static constinit optional_struct_space <PluginDependantStructRegister <PerThreadPluginRegister <_rwlock_standard_ptd>, executiveManagerFactory_t>> ptd_register;

// We provide a fall-back general purpose lock that uses OS semantics, just a little.
struct _rwlock_standard_data
{
    inline _rwlock_standard_data( CExecutiveManagerNative *nativeMan )
    {
        this->nativeMan = nativeMan;
        this->numReaders = 0;
        this->hasWriter = false;
    }

    inline ~_rwlock_standard_data( void )
    {
#ifdef _DEBUG
        assert( this->numReaders == 0 );
        assert( this->hasWriter == false );
#endif //_DEBUG
    }

    // We need the manager because we use per-thread data.
    CExecutiveManagerNative *nativeMan;

    // Having this list is important because when there is an availability when want to fairly schedule one-thread-at-a-time.
    RwList <_rwlock_standard_ptd> listWaiters;

    // The amount of users that are inside the lock.
    unsigned int numReaders;
    bool hasWriter;

    // Need to guard critical code regions using a non-fallible lock.
    CSpinLockImpl lockAtomic;
};

bool _rwlock_standard_is_supported( void )
{
    // This lock type is always available, because it uses standard features.
    return true;
}

size_t _rwlock_standard_get_size( void )
{
    return sizeof(_rwlock_standard_data);
}

size_t _rwlock_standard_get_alignment( void )
{
    return alignof(_rwlock_standard_data);
}

void _rwlock_standard_constructor( void *mem, CExecutiveManagerNative *nativeMan )
{
    new ( mem ) _rwlock_standard_data( nativeMan );
}

void _rwlock_standard_destructor( void *mem, CExecutiveManagerNative *nativeMan )
{
    _rwlock_standard_data *data = (_rwlock_standard_data*)mem;

    data->~_rwlock_standard_data();
}

static AINLINE bool _is_waiting_queue_empty( _rwlock_standard_data *lockData )
{
    return LIST_EMPTY( lockData->listWaiters.root );
}

static AINLINE bool should_reader_wait_before_entering( _rwlock_standard_data *lock )
{
    return ( lock->hasWriter || _is_waiting_queue_empty( lock ) == false );
}

static AINLINE CEvent* _rwlock_standard_curthread_enter_wait_mode( CExecutiveManagerNative *manager, _rwlock_standard_data *lock, eLockRegType wait_kind, _rwlock_standard_ptd **ptd_out = nullptr )
{
    // Get the current thread data.
    // Moved inside this wait condition so that is is very unlikely to trigger in case of NativeExecutive termination.
    CExecThreadImpl *currentThread = (CExecThreadImpl*)manager->GetCurrentThread();

    assert( currentThread != nullptr );

    _rwlock_standard_ptd *threadData = ResolveThreadPlugin( ptd_register.get(), manager, currentThread );

    assert( threadData != nullptr );

    // We will only be woken up if the condition is true (we can dispatch the correct threads).
    threadData->lockRegType = wait_kind;
    threadData->lockWaitingOn = lock;

    LIST_APPEND( lock->listWaiters.root, threadData->lockNode );

    // Mark us as waiting.
    CEvent *waitEvent = GetCurrentThreadWaiterEvent( manager, currentThread );
    waitEvent->Set( true );

    if ( ptd_out )
    {
        *ptd_out = threadData;
    }

    return waitEvent;
}

void _rwlock_standard_enter_read( void *mem )
{
    _rwlock_standard_data *lock = (_rwlock_standard_data*)mem;

    CExecutiveManagerNative *manager = lock->nativeMan;

    lock->lockAtomic.lock();

    // If there is at least one writer in the queue, we wait.
    bool shouldWait = should_reader_wait_before_entering( lock );

    CEvent *waitEvent = nullptr;

    if ( shouldWait )
    {
        waitEvent = _rwlock_standard_curthread_enter_wait_mode( manager, lock, eLockRegType::READER );
    }
    else
    {
        // We have a new reader.
        lock->numReaders++;
    }

    lock->lockAtomic.unlock();

    if ( shouldWait )
    {
        waitEvent->Wait();
    }
}

static AINLINE void _release_available_waiters( CExecutiveManagerNative *nativeMan, _rwlock_standard_data *lockData )
{
    bool has_queue_release_type = false;
    eLockRegType queue_release_type;

    LIST_FOREACH_BEGIN( _rwlock_standard_ptd, lockData->listWaiters.root, lockNode )

        eLockRegType regType = item->lockRegType;

        if ( !has_queue_release_type )
        {
            queue_release_type = regType;
            
            has_queue_release_type = true;
        }
        else
        {
            if ( queue_release_type == eLockRegType::WRITER )
            {
                break;
            }

            if ( regType == eLockRegType::WRITER )
            {
                break;
            }
        }

        // Release this thread.
        // Remove us from the waiting queue (we have the spinlock).
        // This is OK because LIST_FOREACH_BEGIN is designed to allow node self-deletion.
        LIST_REMOVE( item->lockNode );

        // Not waiting on a lock anymore.
        item->lockWaitingOn = nullptr;

        if ( regType == eLockRegType::WRITER )
        {
            lockData->hasWriter = true;
        }
        else if ( regType == eLockRegType::READER )
        {
            lockData->numReaders++;
        }

        // Un-wait the thread.
        CExecThreadImpl *waitingThread = BackResolveThreadPlugin( ptd_register.get(), nativeMan, item );

        CEvent *evtWaiter = GetCurrentThreadWaiterEvent( nativeMan, waitingThread );

        evtWaiter->Set( false );

    LIST_FOREACH_END
}

void _rwlock_standard_leave_read( void *mem )
{
    _rwlock_standard_data *lock = (_rwlock_standard_data*)mem;

    CExecutiveManagerNative *manager = lock->nativeMan;

    lock->lockAtomic.lock();

    auto newNumReaders = --lock->numReaders;

    if ( newNumReaders == 0 )
    {
        // Release the waiting threads if the lock has no more active users.
        _release_available_waiters( manager, lock );
    }

    lock->lockAtomic.unlock();
}

static AINLINE bool should_writer_wait_before_entering( _rwlock_standard_data *lock )
{
    return ( lock->hasWriter || lock->numReaders > 0 || _is_waiting_queue_empty( lock ) == false );
}

void _rwlock_standard_enter_write( void *mem )
{
    _rwlock_standard_data *lock = (_rwlock_standard_data*)mem;

    CExecutiveManagerNative *manager = lock->nativeMan;

    lock->lockAtomic.lock();

    bool shouldWait = should_writer_wait_before_entering( lock );

    CEvent *waitEvent = nullptr;

    if ( shouldWait )
    {
        waitEvent = _rwlock_standard_curthread_enter_wait_mode( manager, lock, eLockRegType::WRITER );
    }
    else
    {
        // We now have a writer.
        lock->hasWriter = true;
    }

    lock->lockAtomic.unlock();

    if ( shouldWait )
    {
        waitEvent->Wait();
    }

    // We have been freed from the lock for entering for sure.
}

void _rwlock_standard_leave_write( void *mem )
{
    _rwlock_standard_data *lock = (_rwlock_standard_data*)mem;

    CExecutiveManagerNative *manager = lock->nativeMan;

    lock->lockAtomic.lock();

    // Writers are absolutely exclusive.
    lock->hasWriter = false;

    // Release the waiting threads if the lock has no more active users.
    _release_available_waiters( manager, lock );

    lock->lockAtomic.unlock();
}

bool _rwlock_standard_try_enter_read( void *mem )
{
    // Other than the forced-enter methods we just return false in the event that we would be reading.
    bool couldEnter = false;

    _rwlock_standard_data *lock = (_rwlock_standard_data*)mem;

    lock->lockAtomic.lock();

    couldEnter = ( should_reader_wait_before_entering( lock ) == false );

    if ( couldEnter )
    {
        lock->numReaders++;
    }

    lock->lockAtomic.unlock();

    return couldEnter;
}

bool _rwlock_standard_try_enter_write( void *mem )
{
    // Just like the method above.
    bool couldEnter = false;

    _rwlock_standard_data *lock = (_rwlock_standard_data*)mem;

    lock->lockAtomic.lock();

    couldEnter = ( should_writer_wait_before_entering( lock ) == false );

    if ( couldEnter )
    {
        lock->hasWriter = true;
    }

    lock->lockAtomic.unlock();

    return couldEnter;
}

// Returns true IFF we did enter the lock.
static AINLINE bool _rwlock_standard_curthread_wait_timed( CExecutiveManagerNative *manager, _rwlock_standard_data *lock, eLockRegType wait_mode, unsigned int time_ms )
{
    _rwlock_standard_ptd *threadData;

    CEvent *waitEvent = _rwlock_standard_curthread_enter_wait_mode( manager, lock, wait_mode, &threadData );

    // We can now unlock the lock because we enter waiting.
    lock->lockAtomic.unlock();

    // Do the actual waiting here.
    bool wasWaitInterrupted = waitEvent->WaitTimed( time_ms );

    // Check whether we actually did enter the lock.
    // * if the time elapsed and we are still registered as waiter then we did not enter.
    // * if the time elapsed and we are not registered as waiter then we did enter.
    // * if the time did not elapse and we are still registered as waiter then undefined behavior (cannot actually happen).
    // * if the time did not elapse and we are not registered as waiter then we did enter.
    bool has_entered = true;

    // If the wait was interrupted then we must have been entered by the lock (see the description above).
    // It is good to optimize this away due to performance considerations.
    if ( wasWaitInterrupted == false )
    {
        lock->lockAtomic.lock();

        if ( threadData->lockWaitingOn != nullptr )
        {
            assert( threadData->lockWaitingOn == lock );

            // Unregister ourselves as waiter.
            waitEvent->Set( false );

            threadData->lockWaitingOn = nullptr;

            LIST_REMOVE( threadData->lockNode );
        
            // Tell the runtime that we failed to enter.
            has_entered = false;
        }

        lock->lockAtomic.unlock();
    }
    
    return has_entered;
}

bool _rwlock_standard_try_timed_enter_read( void *mem, unsigned int time_ms )
{
    _rwlock_standard_data *lock = (_rwlock_standard_data*)mem;

    CExecutiveManagerNative *manager = lock->nativeMan;

    lock->lockAtomic.lock();

    bool shouldWait = should_reader_wait_before_entering( lock );

    if ( shouldWait == false )
    {
        // New reader.
        lock->numReaders++;

        lock->lockAtomic.unlock();
        return true;
    }

    // Do not yet release the lock because we have to do some shared logic.

    return _rwlock_standard_curthread_wait_timed( manager, lock, eLockRegType::READER, time_ms );
}

bool _rwlock_standard_try_timed_enter_write( void *mem, unsigned int time_ms )
{
    _rwlock_standard_data *lock = (_rwlock_standard_data*)mem;

    CExecutiveManagerNative *manager = lock->nativeMan;

    lock->lockAtomic.lock();

    bool shouldWait = should_writer_wait_before_entering( lock );

    if ( shouldWait == false )
    {
        // New writer.
        lock->hasWriter = true;
        
        lock->lockAtomic.unlock();
        return true;
    }

    // We release the lock inside the shared logic.

    return _rwlock_standard_curthread_wait_timed( manager, lock, eLockRegType::WRITER, time_ms );
}

// Module initialization.
void _rwlock_standard_init( void )
{
    return;
}

void _rwlock_standard_shutdown( void )
{
    return;
}

void _rwlock_standard_init_ptd( void )
{
    ptd_register.Construct( executiveManagerFactory );
}

void _rwlock_standard_shutdown_ptd( void )
{
    ptd_register.Destroy();
}

END_NATIVE_EXECUTIVE
