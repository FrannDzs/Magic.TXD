/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.rwlock.win32.srw.cpp
*  PURPOSE:     Read/Write lock internal implementation main
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

// Highly-optimized Win32 SRW lock implementation for the read-write lock.

#include "StdInc.h"

#ifdef _WIN32

#include "CExecutiveManager.rwlock.hxx"

#define NOMINMAX
#include <Windows.h>

BEGIN_NATIVE_EXECUTIVE

static HMODULE kernel32_module;

static METHODDECL_GLOBAL( InitializeSRWLock );
static METHODDECL_GLOBAL( AcquireSRWLockShared );
static METHODDECL_GLOBAL( ReleaseSRWLockShared );
static METHODDECL_GLOBAL( AcquireSRWLockExclusive );
static METHODDECL_GLOBAL( ReleaseSRWLockExclusive );
static METHODDECL_GLOBAL( TryAcquireSRWLockShared );
static METHODDECL_GLOBAL( TryAcquireSRWLockExclusive );

struct _rwlock_srw
{
    SRWLOCK lock_mem;
};

bool _rwlock_srw_is_supported( void )
{
    return (
        METHODNAME_GLOBAL( InitializeSRWLock ) != nullptr &&
        METHODNAME_GLOBAL( AcquireSRWLockShared ) != nullptr &&
        METHODNAME_GLOBAL( ReleaseSRWLockShared ) != nullptr &&
        METHODNAME_GLOBAL( AcquireSRWLockExclusive ) != nullptr &&
        METHODNAME_GLOBAL( ReleaseSRWLockExclusive ) != nullptr &&
        METHODNAME_GLOBAL( TryAcquireSRWLockShared ) != nullptr &&
        METHODNAME_GLOBAL( TryAcquireSRWLockExclusive ) != nullptr
    );
}

size_t _rwlock_srw_get_size( void )
{
    return sizeof(_rwlock_srw);
}

size_t _rwlock_srw_get_alignment( void )
{
    return alignof(_rwlock_srw);
}

void _rwlock_srw_constructor( void *mem, CExecutiveManagerNative *nativeMan )
{
    _rwlock_srw *obj = (_rwlock_srw*)mem;

    obj->lock_mem = SRWLOCK_INIT;
}

void _rwlock_srw_destructor( void *mem, CExecutiveManagerNative *nativeMan )
{
    // Nothing to do.
}

void _rwlock_srw_enter_read( void *mem )
{
    _rwlock_srw *obj = (_rwlock_srw*)mem;

    METHODNAME_GLOBAL( AcquireSRWLockShared )( &obj->lock_mem );
}

void _rwlock_srw_leave_read( void *mem )
{
    _rwlock_srw *obj = (_rwlock_srw*)mem;

    METHODNAME_GLOBAL( ReleaseSRWLockShared )( &obj->lock_mem );
}

void _rwlock_srw_enter_write( void *mem )
{
    _rwlock_srw *obj = (_rwlock_srw*)mem;

    METHODNAME_GLOBAL( AcquireSRWLockExclusive )( &obj->lock_mem );
}

void _rwlock_srw_leave_write( void *mem )
{
    _rwlock_srw *obj = (_rwlock_srw*)mem;

    METHODNAME_GLOBAL( ReleaseSRWLockExclusive )( &obj->lock_mem );
}

bool _rwlock_srw_try_enter_read( void *mem )
{
    _rwlock_srw *obj = (_rwlock_srw*)mem;

    BOOL success = METHODNAME_GLOBAL( TryAcquireSRWLockShared )( &obj->lock_mem );

    return ( success == TRUE );
}

bool _rwlock_srw_try_enter_write( void *mem )
{
    _rwlock_srw *obj = (_rwlock_srw*)mem;

    BOOL success = METHODNAME_GLOBAL( TryAcquireSRWLockExclusive )( &obj->lock_mem );

    return ( success == TRUE );
}

void _rwlock_srw_init( void )
{
    kernel32_module = LoadLibraryW( L"kernel32.dll" );

    if ( kernel32_module )
    {
        METHODDECL_FETCH( kernel32_module, InitializeSRWLock );
        METHODDECL_FETCH( kernel32_module, AcquireSRWLockShared );
        METHODDECL_FETCH( kernel32_module, ReleaseSRWLockShared );
        METHODDECL_FETCH( kernel32_module, AcquireSRWLockExclusive );
        METHODDECL_FETCH( kernel32_module, ReleaseSRWLockExclusive );
        METHODDECL_FETCH( kernel32_module, TryAcquireSRWLockShared );
        METHODDECL_FETCH( kernel32_module, TryAcquireSRWLockExclusive );
    }
}

void _rwlock_srw_shutdown( void )
{
    METHODNAME_GLOBAL( InitializeSRWLock ) = nullptr;
    METHODNAME_GLOBAL( AcquireSRWLockShared ) = nullptr;
    METHODNAME_GLOBAL( ReleaseSRWLockShared ) = nullptr;
    METHODNAME_GLOBAL( AcquireSRWLockExclusive ) = nullptr;
    METHODNAME_GLOBAL( ReleaseSRWLockExclusive ) = nullptr;
    METHODNAME_GLOBAL( TryAcquireSRWLockShared ) = nullptr;
    METHODNAME_GLOBAL( TryAcquireSRWLockExclusive ) = nullptr;

    if ( kernel32_module )
    {
        FreeLibrary( kernel32_module );
    }
}

END_NATIVE_EXECUTIVE

#endif //_WIN32