/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.rwlock.cpp
*  PURPOSE:     Read/Write lock synchronization object
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "CExecutiveManager.rwlock.hxx"

BEGIN_NATIVE_EXECUTIVE

// Normal Read Write Lock.
static size_t                           _rwlock_readWriteLockSize = 0;
static size_t                           _rwlock_alignment = 0;
static rwlockimpl_construct             _rwlock_constructor = nullptr;
static rwlockimpl_destroy               _rwlock_destructor = nullptr;
static rwlockimpl_enter_read            _rwlock_enter_read = nullptr;
static rwlockimpl_leave_read            _rwlock_leave_read = nullptr;
static rwlockimpl_enter_write           _rwlock_enter_write = nullptr;
static rwlockimpl_leave_write           _rwlock_leave_write = nullptr;
static rwlockimpl_try_enter_read        _rwlock_try_enter_read = nullptr;
static rwlockimpl_try_enter_write       _rwlock_try_enter_write = nullptr;
static rwlockimpl_try_timed_enter_read  _rwlock_try_timed_enter_read = nullptr;
static rwlockimpl_try_timed_enter_write _rwlock_try_timed_enter_write = nullptr;

// Fair Read Write Lock.
static size_t                           _rwlock_fair_size = 0;
static size_t                           _rwlock_fair_alignment = 0;
static rwlockimpl_construct             _rwlock_fair_constructor = nullptr;
static rwlockimpl_destroy               _rwlock_fair_destructor = nullptr;
static rwlockimpl_enter_read            _rwlock_fair_enter_read = nullptr;
static rwlockimpl_leave_read            _rwlock_fair_leave_read = nullptr;
static rwlockimpl_enter_write           _rwlock_fair_enter_write = nullptr;
static rwlockimpl_leave_write           _rwlock_fair_leave_write = nullptr;
static rwlockimpl_try_enter_read        _rwlock_fair_try_enter_read = nullptr;
static rwlockimpl_try_enter_write       _rwlock_fair_try_enter_write = nullptr;
static rwlockimpl_try_timed_enter_read  _rwlock_fair_try_timed_enter_read = nullptr;
static rwlockimpl_try_timed_enter_write _rwlock_fair_try_timed_enter_write = nullptr;

#ifdef _WIN32

// Native Win32 optimized SRW lock implementation.
extern bool _rwlock_srw_is_supported( void );
extern size_t _rwlock_srw_get_size( void );
extern size_t _rwlock_srw_get_alignment( void );
extern void _rwlock_srw_constructor( void *mem, CExecutiveManagerNative *nativeMan );
extern void _rwlock_srw_destructor( void *mem, CExecutiveManagerNative *nativeMan );
extern void _rwlock_srw_enter_read( void *mem );
extern void _rwlock_srw_leave_read( void *mem );
extern void _rwlock_srw_enter_write( void *mem );
extern void _rwlock_srw_leave_write( void *mem );
extern bool _rwlock_srw_try_enter_read( void *mem );
extern bool _rwlock_srw_try_enter_write( void *mem );
extern void _rwlock_srw_init( void );
extern void _rwlock_srw_shutdown( void );

// NOTE: this implementation does not support timed-try locking.

#endif // CROSS PLATFORM CODE

// Our own read-write lock implementation that is used if no native one is available.
extern bool _rwlock_standard_is_supported( void );
extern size_t _rwlock_standard_get_size( void );
extern size_t _rwlock_standard_get_alignment( void );
extern void _rwlock_standard_constructor( void *mem, CExecutiveManagerNative *nativeMan );
extern void _rwlock_standard_destructor( void *mem, CExecutiveManagerNative *nativeMan );
extern void _rwlock_standard_enter_read( void *mem );
extern void _rwlock_standard_leave_read( void *mem );
extern void _rwlock_standard_enter_write( void *mem );
extern void _rwlock_standard_leave_write( void *mem );
extern bool _rwlock_standard_try_enter_read( void *mem );
extern bool _rwlock_standard_try_enter_write( void *mem );
extern bool _rwlock_standard_try_timed_enter_read( void *mem, unsigned int time_ms );
extern bool _rwlock_standard_try_timed_enter_write( void *mem, unsigned int time_ms );
extern void _rwlock_standard_init( void );
extern void _rwlock_standard_shutdown( void );
extern void _rwlock_standard_init_ptd( void );
extern void _rwlock_standard_shutdown_ptd( void );

// Export the public interface to the (initialized) read-write lock implementation.
size_t pubrwlock_get_size( void )
{
    return _rwlock_readWriteLockSize;
}

size_t pubrwlock_get_alignment( void )
{
    return _rwlock_alignment;
}

bool pubrwlock_is_supported( void )
{
    return ( _rwlock_readWriteLockSize != 0 );
}

// The actual real interface.
void CReadWriteLock::EnterCriticalReadRegion( void )
{ _rwlock_enter_read( this ); }

void CReadWriteLock::LeaveCriticalReadRegion( void )
{ _rwlock_leave_read( this ); }

void CReadWriteLock::EnterCriticalWriteRegion( void )
{ _rwlock_enter_write( this ); }

void CReadWriteLock::LeaveCriticalWriteRegion( void )
{ _rwlock_leave_write( this ); }

bool CReadWriteLock::TryEnterCriticalReadRegion( void )
{ return _rwlock_try_enter_read( this ); }

bool CReadWriteLock::TryEnterCriticalWriteRegion( void )
{ return _rwlock_try_enter_write( this ); }

// *** Support does not have to exist for timed-enter here.
bool CReadWriteLock::TryTimedEnterCriticalReadRegion( unsigned int time_ms )
{
    auto meth = _rwlock_try_timed_enter_read;

    if ( meth == nullptr )
    {
        return false;
    }

    return meth( this, time_ms );
}

bool CReadWriteLock::TryTimedEnterCriticalWriteRegion( unsigned int time_ms )
{
    auto meth = _rwlock_try_timed_enter_write;

    if ( meth == nullptr )
    {
        return false;
    }

    return meth( this, time_ms );
}

// Interface of the fair read/write lock.
void CFairReadWriteLock::EnterCriticalReadRegion( void )
{ _rwlock_fair_enter_read( this ); }

void CFairReadWriteLock::LeaveCriticalReadRegion( void )
{ _rwlock_fair_leave_read( this ); }

void CFairReadWriteLock::EnterCriticalWriteRegion( void )
{ _rwlock_fair_enter_write( this ); }

void CFairReadWriteLock::LeaveCriticalWriteRegion( void )
{ _rwlock_fair_leave_write( this ); }

bool CFairReadWriteLock::TryEnterCriticalReadRegion( void )
{ return _rwlock_fair_try_enter_read( this ); }

bool CFairReadWriteLock::TryEnterCriticalWriteRegion( void )
{ return _rwlock_fair_try_enter_write( this ); }

bool CFairReadWriteLock::TryTimedEnterCriticalReadRegion( unsigned int time_ms )
{ return _rwlock_fair_try_timed_enter_read( this, time_ms ); }

bool CFairReadWriteLock::TryTimedEnterCriticalWriteRegion( unsigned int time_ms )
{ return _rwlock_fair_try_timed_enter_write( this, time_ms ); }

// Executive manager API.
CReadWriteLock* CExecutiveManager::CreateReadWriteLock( void )
{
    CExecutiveManagerNative *execMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( execMan );

    return (CReadWriteLock*)dyn_anon_construct(
        memAlloc,
        _rwlock_constructor,
        _rwlock_readWriteLockSize,
        _rwlock_alignment,
        execMan
    );
}

void CExecutiveManager::CloseReadWriteLock( CReadWriteLock *theLock )
{
    CExecutiveManagerNative *execMan = (CExecutiveManagerNative*)this;

    _rwlock_destructor( theLock, execMan );

    void *anon_mem = theLock;

    execMan->MemFree( anon_mem );
}

size_t CExecutiveManager::GetReadWriteLockStructSize( void )
{
    //CExecutiveManagerNative *execMan = (CExecutiveManagerNative*)this;

    return _rwlock_readWriteLockSize;
}

size_t CExecutiveManager::GetReadWriteLockStructAlign( void )
{
    return _rwlock_alignment;
}

bool CExecutiveManager::DoesReadWriteLockSupportTime( void )
{
    // This check is only a problem for native implementations.
    // The library implementation will always support timed-enter.
    return ( _rwlock_try_enter_read != nullptr && _rwlock_try_enter_write != nullptr );
}

CReadWriteLock* CExecutiveManager::CreatePlacedReadWriteLock( void *mem )
{
    CExecutiveManagerNative *execMan = (CExecutiveManagerNative*)this;

    // We lack a constructor then we do not support anything.
    if ( !_rwlock_constructor )
        return nullptr;

    _rwlock_constructor( mem, execMan );

    return (CReadWriteLock*)mem;
}

void CExecutiveManager::ClosePlacedReadWriteLock( CReadWriteLock *placedLock )
{
    CExecutiveManagerNative *execMan = (CExecutiveManagerNative*)this;

    _rwlock_destructor( placedLock, execMan );
}

// Same for the fair Read/Write lock.
CFairReadWriteLock* CExecutiveManager::CreateFairReadWriteLock( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    if ( !_rwlock_fair_constructor )
        return nullptr;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    return (CFairReadWriteLock*)dyn_anon_construct(
        memAlloc,
        _rwlock_fair_constructor,
        _rwlock_fair_size,
        _rwlock_fair_alignment,
        nativeMan
    );
}

void CExecutiveManager::CloseFairReadWriteLock( CFairReadWriteLock *theLock )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    _rwlock_fair_destructor( theLock, nativeMan );

    void *anon_mem = theLock;

    nativeMan->memoryIntf->Free( anon_mem );
}

size_t CExecutiveManager::GetFairReadWriteLockStructSize( void )
{
    return _rwlock_fair_size;
}

size_t CExecutiveManager::GetFairReadWriteLockStructAlign( void )
{
    return _rwlock_fair_alignment;
}

CFairReadWriteLock* CExecutiveManager::CreatePlacedFairReadWriteLock( void *mem )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    if ( !_rwlock_fair_constructor )
        return nullptr;

    _rwlock_fair_constructor( mem, nativeMan );

    return (CFairReadWriteLock*)mem;
}

void CExecutiveManager::ClosePlacedFairReadWriteLock( CFairReadWriteLock *theLock )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    _rwlock_fair_destructor( theLock, nativeMan );
}

void registerReadWriteLockEnvironment( void )
{
    // Register all read write lock implementations.
#ifdef _WIN32
    _rwlock_srw_init();
#endif //CROSS PLATFORM CODE

    _rwlock_standard_init();

#ifndef NATEXEC_DISABLE_NATIVE_IMPL
    // Decide which rwlock implementations to pick.
#ifdef _WIN32
    if ( _rwlock_srw_is_supported() )
    {
        _rwlock_readWriteLockSize = _rwlock_srw_get_size();
        _rwlock_alignment = _rwlock_srw_get_alignment();
        _rwlock_constructor = _rwlock_srw_constructor;
        _rwlock_destructor = _rwlock_srw_destructor;
        _rwlock_enter_read = _rwlock_srw_enter_read;
        _rwlock_leave_read = _rwlock_srw_leave_read;
        _rwlock_enter_write = _rwlock_srw_enter_write;
        _rwlock_leave_write = _rwlock_srw_leave_write;
        _rwlock_try_enter_read = _rwlock_srw_try_enter_read;
        _rwlock_try_enter_write = _rwlock_srw_try_enter_write;
        _rwlock_try_timed_enter_read = nullptr;
        _rwlock_try_timed_enter_write = nullptr;
        goto didPickReadWriteImpl;
    }
#endif //_WIN32
#endif //NATEXEC_DISABLE_NATIVE_IMPL

    // Good fall-back solution in case no platform-native solution is available.
    if ( _rwlock_standard_is_supported() )
    {
        _rwlock_readWriteLockSize = _rwlock_standard_get_size();
        _rwlock_alignment = _rwlock_standard_get_alignment();
        _rwlock_constructor = _rwlock_standard_constructor;
        _rwlock_destructor = _rwlock_standard_destructor;
        _rwlock_enter_read = _rwlock_standard_enter_read;
        _rwlock_leave_read = _rwlock_standard_leave_read;
        _rwlock_enter_write = _rwlock_standard_enter_write;
        _rwlock_leave_write = _rwlock_standard_leave_write;
        _rwlock_try_enter_read = _rwlock_standard_try_enter_read;
        _rwlock_try_enter_write = _rwlock_standard_try_enter_write;
        _rwlock_try_timed_enter_read = _rwlock_standard_try_timed_enter_read;
        _rwlock_try_timed_enter_write = _rwlock_standard_try_timed_enter_write;
        goto didPickReadWriteImpl;
    }

didPickReadWriteImpl:;
    // Our standard implementation is a fair read-write lock, so pick it if available.
    if ( _rwlock_standard_is_supported() )
    {
        _rwlock_fair_size = _rwlock_standard_get_size();
        _rwlock_fair_alignment = _rwlock_standard_get_alignment();
        _rwlock_fair_constructor = _rwlock_standard_constructor;
        _rwlock_fair_destructor = _rwlock_standard_destructor;
        _rwlock_fair_enter_read = _rwlock_standard_enter_read;
        _rwlock_fair_leave_read = _rwlock_standard_leave_read;
        _rwlock_fair_enter_write = _rwlock_standard_enter_write;
        _rwlock_fair_leave_write = _rwlock_standard_leave_write;
        _rwlock_fair_try_enter_read = _rwlock_standard_try_enter_read;
        _rwlock_fair_try_enter_write = _rwlock_standard_try_enter_write;
        _rwlock_fair_try_timed_enter_read = _rwlock_standard_try_timed_enter_read;
        _rwlock_fair_try_timed_enter_write = _rwlock_standard_try_timed_enter_write;
        goto didPickFairReadWriteImpl;
    }

didPickFairReadWriteImpl:;
}

void unregisterReadWriteLockEnvironment( void )
{
    // Forget about the rwlock implementations.
    _rwlock_readWriteLockSize = 0;
    _rwlock_alignment = 0;
    _rwlock_constructor = nullptr;
    _rwlock_destructor = nullptr;
    _rwlock_enter_read = nullptr;
    _rwlock_leave_read = nullptr;
    _rwlock_enter_write = nullptr;
    _rwlock_leave_write = nullptr;
    _rwlock_try_enter_read = nullptr;
    _rwlock_try_enter_write = nullptr;
    _rwlock_try_timed_enter_read = nullptr;
    _rwlock_try_timed_enter_write = nullptr;

    _rwlock_fair_size = 0;
    _rwlock_fair_alignment = 0;
    _rwlock_fair_constructor = nullptr;
    _rwlock_fair_destructor = nullptr;
    _rwlock_fair_enter_read = nullptr;
    _rwlock_fair_leave_read = nullptr;
    _rwlock_fair_enter_write = nullptr;
    _rwlock_fair_leave_write = nullptr;
    _rwlock_fair_try_enter_read = nullptr;
    _rwlock_fair_try_enter_write = nullptr;
    _rwlock_fair_try_timed_enter_read = nullptr;
    _rwlock_fair_try_timed_enter_write = nullptr;

    // Destroy all read write lock implementations.
    _rwlock_standard_shutdown();

#ifdef _WIN32
    _rwlock_srw_shutdown();
#endif // CROSS PLATFORM CODE
}

// We need lazy-initialization for the per-thread plugins because we do not have the threads initialized
// when we initialize the implementations.
void registerReadWriteLockPTD( void )
{
    // Module PTD initialization.
    _rwlock_standard_init_ptd();
}

void unregisterReadWriteLockPTD( void )
{
    // Module PTD shutdown.
    _rwlock_standard_shutdown_ptd();
}

END_NATIVE_EXECUTIVE
