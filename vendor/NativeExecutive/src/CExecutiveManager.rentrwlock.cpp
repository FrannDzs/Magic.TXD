/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.rwlock.cpp
*  PURPOSE:     Reentrant Read/Write lock synchronization object
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

// For the "dyn_anon_construct" helper.
#include "CExecutiveManager.rwlock.hxx"

#include "PluginUtils.hxx"

BEGIN_NATIVE_EXECUTIVE

// Reentrant Read Write Lock.
static size_t   _rwlock_rentReadWriteLockSize = 0;
static size_t   _rwlock_rent_alignment = 0;
static void     (*_rwlock_rent_constructor)( void *mem, CExecutiveManagerNative *manager ) = nullptr;
static void     (*_rwlock_rent_destructor)( void *mem, CExecutiveManagerNative *manager ) = nullptr;
static void     (*_rwlock_rent_enter_read)( void *mem, void *ctxMem ) = nullptr;
static void     (*_rwlock_rent_leave_read)( void *mem, void *ctxMem ) = nullptr;
static void     (*_rwlock_rent_enter_write)( void *mem, void *ctxMem ) = nullptr;
static void     (*_rwlock_rent_leave_write)( void *mem, void *ctxMem ) = nullptr;
static bool     (*_rwlock_rent_try_enter_read)( void *mem, void *ctxMem ) = nullptr;
static bool     (*_rwlock_rent_try_enter_write)( void *mem, void *ctxMem ) = nullptr;
static bool     (*_rwlock_rent_try_timed_enter_read)( void *mem, void *ctxMem, unsigned int time_ms ) = nullptr;
static bool     (*_rwlock_rent_try_timed_enter_write)( void *mem, void *ctxMem, unsigned int time_ms ) = nullptr;
static size_t   _rwlock_rent_ctx_size = 0;
static size_t   _rwlock_rent_ctx_alignment = 0;
static void     (*_rwlock_rent_ctx_constructor)( void *ctxMem, CExecutiveManagerNative *manager ) = nullptr;
static void     (*_rwlock_rent_ctx_destructor)( void *ctxMem, CExecutiveManagerNative *manager ) = nullptr;
static void     (*_rwlock_rent_ctx_move)( void *dstMem, void *srcMem ) = nullptr;

// The standard reentrant Read/Write lock implementation.
extern bool _rwlock_rent_standard_is_supported( void );
extern size_t _rwlock_rent_standard_get_size( void );
extern size_t _rwlock_rent_standard_get_alignment( void );
extern void _rwlock_rent_standard_constructor( void *mem, CExecutiveManagerNative *nativeMan );
extern void _rwlock_rent_standard_destructor( void *mem, CExecutiveManagerNative *nativeMan );
extern void _rwlock_rent_standard_enter_read( void *mem, void *ctxMem );
extern void _rwlock_rent_standard_leave_read( void *mem, void *ctxMem );
extern void _rwlock_rent_standard_enter_write( void *mem, void *ctxMem );
extern void _rwlock_rent_standard_leave_write( void *mem, void *ctxMem );
extern bool _rwlock_rent_standard_try_enter_read( void *mem, void *ctxMem );
extern bool _rwlock_rent_standard_try_enter_write( void *mem, void *ctxMem );
extern bool _rwlock_rent_standard_try_timed_enter_read( void *mem, void *ctxMem, unsigned int time_ms );
extern bool _rwlock_rent_standard_try_timed_enter_write( void *mem, void *ctxMem, unsigned int time_ms );
extern size_t _rwlock_rent_standard_ctx_get_size( void );
extern size_t _rwlock_rent_standard_ctx_get_alignment( void );
extern void _rwlock_rent_standard_ctx_constructor( void *mem, CExecutiveManagerNative *nativeMan );
extern void _rwlock_rent_standard_ctx_destructor( void *mem, CExecutiveManagerNative *nativeMan );
extern void _rwlock_rent_standard_ctx_move( void *dstMem, void *srcMem );

// Reentrant RW lock implementation.
void CReentrantReadWriteLock::LockRead( CReentrantReadWriteContext *ctx )
{
    _rwlock_rent_enter_read( this, ctx );
}

void CReentrantReadWriteLock::UnlockRead( CReentrantReadWriteContext *ctx )
{
    _rwlock_rent_leave_read( this, ctx );
}

void CReentrantReadWriteLock::LockWrite( CReentrantReadWriteContext *ctx )
{
    _rwlock_rent_enter_write( this, ctx );
}

void CReentrantReadWriteLock::UnlockWrite( CReentrantReadWriteContext *ctx )
{
    _rwlock_rent_leave_write( this, ctx );
}

bool CReentrantReadWriteLock::TryLockRead( CReentrantReadWriteContext *ctx )
{
    return _rwlock_rent_try_enter_read( this, ctx );
}

bool CReentrantReadWriteLock::TryLockWrite( CReentrantReadWriteContext *ctx )
{
    return _rwlock_rent_try_enter_write( this, ctx );
}

bool CReentrantReadWriteLock::TryTimedLockRead( CReentrantReadWriteContext *ctx, unsigned int time_ms )
{
    return _rwlock_rent_try_timed_enter_read( this, ctx, time_ms );
}

bool CReentrantReadWriteLock::TryTimedLockWrite( CReentrantReadWriteContext *ctx, unsigned int time_ms )
{
    return _rwlock_rent_try_timed_enter_write( this, ctx, time_ms );
}

// Executive manager API.
CReentrantReadWriteLock* CExecutiveManager::CreateReentrantReadWriteLock( void )
{
    CExecutiveManagerNative *execMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( execMan );

    return (CReentrantReadWriteLock*)dyn_anon_construct(
        memAlloc,
        _rwlock_rent_constructor,
        _rwlock_rentReadWriteLockSize,
        _rwlock_rent_alignment,
        execMan
    );
}

void CExecutiveManager::CloseReentrantReadWriteLock( CReentrantReadWriteLock *theLock )
{
    CExecutiveManagerNative *execMan = (CExecutiveManagerNative*)this;

    _rwlock_rent_destructor( theLock, execMan );

    void *anonMem = theLock;

    execMan->MemFree( anonMem );
}

size_t CExecutiveManager::GetReentrantReadWriteLockStructSize( void )
{
    //CExecutiveManagerNative *execMan = (CExecutiveManagerNative*)this;

    return _rwlock_rentReadWriteLockSize;
}

size_t CExecutiveManager::GetReentrantReadWriteLockAlignment( void )
{
    //CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    return _rwlock_rent_alignment;
}

CReentrantReadWriteLock* CExecutiveManager::CreatePlacedReentrantReadWriteLock( void *mem )
{
    CExecutiveManagerNative *execMan = (CExecutiveManagerNative*)this;

    // If the constructor is not present, then we assume we have no locks of this type.
    if ( !_rwlock_rent_constructor )
        return nullptr;

    _rwlock_rent_constructor( mem, execMan );

    return (CReentrantReadWriteLock*)mem;
}

void CExecutiveManager::ClosePlacedReentrantReadWriteLock( CReentrantReadWriteLock *theLock )
{
    CExecutiveManagerNative *execMan = (CExecutiveManagerNative*)this;

    _rwlock_rent_destructor( theLock, execMan );
}

size_t CExecutiveManager::GetReentrantReadWriteContextStructSize( void )
{
    //CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    return _rwlock_rent_ctx_size;
}

size_t CExecutiveManager::GetReentrantReadWriteContextAlignment( void )
{
    //CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    return _rwlock_rent_ctx_alignment;
}

CReentrantReadWriteContext* CExecutiveManager::CreateReentrantReadWriteContext( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    return (CReentrantReadWriteContext*)dyn_anon_construct(
        memAlloc,
        _rwlock_rent_ctx_constructor,
        _rwlock_rent_ctx_size,
        _rwlock_rent_ctx_alignment,
        nativeMan
    );
}

void CExecutiveManager::CloseReentrantReadWriteContext( CReentrantReadWriteContext *ctx )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    _rwlock_rent_ctx_destructor( ctx, nativeMan );

    nativeMan->MemFree( ctx );
}

CReentrantReadWriteContext* CExecutiveManager::CreatePlacedReentrantReadWriteContext( void *ctxMem )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    _rwlock_rent_ctx_constructor( ctxMem, nativeMan );

    return (CReentrantReadWriteContext*)ctxMem;
}

void CExecutiveManager::ClosePlacedReentrantReadWriteContext( CReentrantReadWriteContext *ctx )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    _rwlock_rent_ctx_destructor( ctx, nativeMan );
}

void CExecutiveManager::MoveReentrantReadWriteContext( CReentrantReadWriteContext *dstCtx, CReentrantReadWriteContext *srcCtx )
{
    //CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    _rwlock_rent_ctx_move( dstCtx, srcCtx );
}

// Each thread must have a standard context that can be used by any reentrant Read/Write lock.
// This is to simplify usage.
struct _rwlock_rent_thread_ctx_env
{
    AINLINE void Initialize( CExecThreadImpl *nativeThread )
    {
        CExecutiveManagerNative *nativeMan = nativeThread->manager;

        _rwlock_rent_ctx_constructor( this, nativeMan );
    }

    AINLINE void Shutdown( CExecThreadImpl *nativeThread )
    {
        CExecutiveManagerNative *nativeMan = nativeThread->manager;

        _rwlock_rent_ctx_destructor( this, nativeMan );
    }

    AINLINE static size_t GetStructSize( CExecutiveManagerNative *nativeMan )
    {
        return _rwlock_rent_ctx_size;
    }
};

static constinit optional_struct_space <PluginDependantStructRegister <PerThreadPluginRegister <_rwlock_rent_thread_ctx_env, true, true>, executiveManagerFactory_t>> _rwlock_rent_thread_ctx_reg;

// The actual thread-ctx helper.
struct _rent_thread_data
{
    CExecutiveManagerNative *manager;
};

static size_t _rent_thread_get_struct_offset( void )
{
    size_t rent_size = _rwlock_rentReadWriteLockSize;

    return ALIGN( rent_size, alignof(_rent_thread_data), alignof(_rent_thread_data) );
}

static _rent_thread_data* _rent_thread_get_struct( CThreadReentrantReadWriteLock *lock )
{
    size_t struct_off = _rent_thread_get_struct_offset();

    return (_rent_thread_data*)( (char*)lock + struct_off );
}

static CReentrantReadWriteContext* _rent_thread_get_context( CExecutiveManagerNative *nativeMan )
{
    CExecThreadImpl *curThread = (CExecThreadImpl*)nativeMan->GetCurrentThread();

    assert( curThread != nullptr );

    CReentrantReadWriteContext *ctx = (CReentrantReadWriteContext*)ResolveThreadPlugin( _rwlock_rent_thread_ctx_reg.get(), nativeMan, curThread );

    assert( ctx != nullptr );

    return ctx;
}

void CThreadReentrantReadWriteLock::LockRead( void )
{
    _rent_thread_data *data = _rent_thread_get_struct( this );

    CReentrantReadWriteContext *ctx = _rent_thread_get_context( data->manager );

    _rwlock_rent_enter_read( this, ctx );
}

void CThreadReentrantReadWriteLock::UnlockRead( void )
{
    _rent_thread_data *data = _rent_thread_get_struct( this );

    CReentrantReadWriteContext *ctx = _rent_thread_get_context( data->manager );

    _rwlock_rent_leave_read( this, ctx );
}

void CThreadReentrantReadWriteLock::LockWrite( void )
{
    _rent_thread_data *data = _rent_thread_get_struct( this );

    CReentrantReadWriteContext *ctx = _rent_thread_get_context( data->manager );

    _rwlock_rent_enter_write( this, ctx );
}

void CThreadReentrantReadWriteLock::UnlockWrite( void )
{
    _rent_thread_data *data = _rent_thread_get_struct( this );

    CReentrantReadWriteContext *ctx = _rent_thread_get_context( data->manager );

    _rwlock_rent_leave_write( this, ctx );
}

bool CThreadReentrantReadWriteLock::TryLockRead( void )
{
    _rent_thread_data *data = _rent_thread_get_struct( this );

    CReentrantReadWriteContext *ctx = _rent_thread_get_context( data->manager );

    return _rwlock_rent_try_enter_read( this, ctx );
}

bool CThreadReentrantReadWriteLock::TryLockWrite( void )
{
    _rent_thread_data *data = _rent_thread_get_struct( this );

    CReentrantReadWriteContext *ctx = _rent_thread_get_context( data->manager );

    return _rwlock_rent_try_enter_write( this, ctx );
}

bool CThreadReentrantReadWriteLock::TryTimedLockRead( unsigned int time_ms )
{
    _rent_thread_data *data = _rent_thread_get_struct( this );

    CReentrantReadWriteContext *ctx = _rent_thread_get_context( data->manager );

    return _rwlock_rent_try_timed_enter_read( this, ctx, time_ms );
}

bool CThreadReentrantReadWriteLock::TryTimedLockWrite( unsigned int time_ms )
{
    _rent_thread_data *data = _rent_thread_get_struct( this );

    CReentrantReadWriteContext *ctx = _rent_thread_get_context( data->manager );

    return _rwlock_rent_try_timed_enter_write( this, ctx, time_ms );
}

// Creation-API.
static void _rent_thread_constr( void *mem, CExecutiveManagerNative *nativeMan, size_t dataOff )
{
    _rwlock_rent_constructor( mem, nativeMan );

    _rent_thread_data *dataPtr = (_rent_thread_data*)( (char*)mem + dataOff );

    dataPtr->manager = nativeMan;
}

CThreadReentrantReadWriteLock* CExecutiveManager::CreateThreadReentrantReadWriteLock( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    size_t ext_off = _rent_thread_get_struct_offset();

    size_t obj_size = ( ext_off + sizeof(_rent_thread_data) );

    return (CThreadReentrantReadWriteLock*)dyn_anon_construct(
        memAlloc,
        &_rent_thread_constr,
        obj_size,
        _rwlock_rent_alignment,
        nativeMan, ext_off
    );
}

void CExecutiveManager::CloseThreadReentrantReadWriteLock( CThreadReentrantReadWriteLock *lock )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    _rwlock_rent_destructor( lock, nativeMan );

    nativeMan->MemFree( lock );
}

CReentrantReadWriteContext* CExecutiveManager::GetThreadReentrantReadWriteContext( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    return _rent_thread_get_context( nativeMan );
}

size_t CExecutiveManager::GetThreadReentrantReadWriteLockStructSize( void )
{
    return ( _rent_thread_get_struct_offset() + sizeof(_rent_thread_data) );
}

size_t CExecutiveManager::GetThreadReentrantReadWriteLockAlignment( void )
{
    return ( _rwlock_rent_alignment );
}

CThreadReentrantReadWriteLock* CExecutiveManager::CreatePlacedThreadReentrantReadWriteLock( void *mem )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    size_t dataOff = _rent_thread_get_struct_offset();

    _rent_thread_constr( mem, nativeMan, dataOff );

    return (CThreadReentrantReadWriteLock*)mem;
}

void CExecutiveManager::ClosePlacedThreadReentrantReadWriteLock( CThreadReentrantReadWriteLock *lock )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    _rwlock_rent_destructor( lock, nativeMan );
}

void registerReentrantReadWriteLockEnvironment( void )
{
    // Pick a good reentrant Read/Write lock.
    if ( _rwlock_rent_standard_is_supported() )
    {
        _rwlock_rentReadWriteLockSize = _rwlock_rent_standard_get_size();
        _rwlock_rent_alignment = _rwlock_rent_standard_get_alignment();
        _rwlock_rent_constructor = _rwlock_rent_standard_constructor;
        _rwlock_rent_destructor = _rwlock_rent_standard_destructor;
        _rwlock_rent_enter_read = _rwlock_rent_standard_enter_read;
        _rwlock_rent_leave_read = _rwlock_rent_standard_leave_read;
        _rwlock_rent_enter_write = _rwlock_rent_standard_enter_write;
        _rwlock_rent_leave_write = _rwlock_rent_standard_leave_write;
        _rwlock_rent_try_enter_read = _rwlock_rent_standard_try_enter_read;
        _rwlock_rent_try_enter_write = _rwlock_rent_standard_try_enter_write;
        _rwlock_rent_try_timed_enter_read = _rwlock_rent_standard_try_timed_enter_read;
        _rwlock_rent_try_timed_enter_write = _rwlock_rent_standard_try_timed_enter_write;
        _rwlock_rent_ctx_size = _rwlock_rent_standard_ctx_get_size();
        _rwlock_rent_ctx_alignment = _rwlock_rent_standard_ctx_get_alignment();
        _rwlock_rent_ctx_constructor = _rwlock_rent_standard_ctx_constructor;
        _rwlock_rent_ctx_destructor = _rwlock_rent_standard_ctx_destructor;
        _rwlock_rent_ctx_move = _rwlock_rent_standard_ctx_move;
        goto didPickReentrantReadWriteImpl;
    }

didPickReentrantReadWriteImpl:;
    // Register the extensions/helpers.
    _rwlock_rent_thread_ctx_reg.Construct( executiveManagerFactory );
}

void unregisterReentrantReadWriteLockEnvironment( void )
{
    // Unregister the extensions/helpers.
    _rwlock_rent_thread_ctx_reg.Destroy();

    // Clean up reentrant rwlock implementation.
    _rwlock_rentReadWriteLockSize = 0;
    _rwlock_rent_alignment = 0;
    _rwlock_rent_constructor = nullptr;
    _rwlock_rent_destructor = nullptr;
    _rwlock_rent_enter_read = nullptr;
    _rwlock_rent_leave_read = nullptr;
    _rwlock_rent_enter_write = nullptr;
    _rwlock_rent_leave_write = nullptr;
    _rwlock_rent_try_enter_read = nullptr;
    _rwlock_rent_try_enter_write = nullptr;
    _rwlock_rent_try_timed_enter_read = nullptr;
    _rwlock_rent_try_timed_enter_write = nullptr;
    _rwlock_rent_ctx_size = 0;
    _rwlock_rent_ctx_alignment = 0;
    _rwlock_rent_ctx_constructor = nullptr;
    _rwlock_rent_ctx_destructor = nullptr;
    _rwlock_rent_ctx_move = nullptr;
}

END_NATIVE_EXECUTIVE
