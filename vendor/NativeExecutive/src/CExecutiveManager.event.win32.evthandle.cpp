/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.event.win32.evthandle.cpp
*  PURPOSE:     Win32 event implementation using Event HANDLEs
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

// This implementation of CEvent allocates a Win32 kernel event object using
// CreateEvent and uses it for synchronization. This is pretty cumbersome because
// it relies on global kernel objects and should only be used for legacy
// support.

#include "StdInc.h"

#ifdef _WIN32

#include <Windows.h>

// Kind of a requirement here.
static_assert( INFINITE == 0xFFFFFFFF, "error in the Windows definition of INFINITE" );

BEGIN_NATIVE_EXECUTIVE

struct _event_win32_evthandle
{
    HANDLE eventObj;
};

static constinit HMODULE _kernel32Handle = nullptr;
static constinit decltype( &CreateEventA ) func_CreateEventA = nullptr;
static constinit decltype( &CloseHandle ) func_CloseHandle = nullptr;
static constinit decltype( &SetEvent ) func_SetEvent = nullptr;
static constinit decltype( &ResetEvent ) func_ResetEvent = nullptr;
static constinit decltype( &WaitForSingleObject ) func_WaitForSingleObject = nullptr;

bool _event_win32_evthandle_is_supported( void )
{
    return (
        func_CreateEventA != nullptr &&
        func_SetEvent != nullptr &&
        func_ResetEvent != nullptr &&
        func_CloseHandle != nullptr &&
        func_WaitForSingleObject != nullptr
    );
}

size_t _event_win32_evthandle_get_size( void )
{
    return sizeof(_event_win32_evthandle);
}

size_t _event_win32_evthandle_get_alignment( void )
{
    return alignof(_event_win32_evthandle);
}

void _event_win32_evthandle_constructor( void *mem, bool shouldWait )
{
    _event_win32_evthandle *obj = (_event_win32_evthandle*)mem;

    obj->eventObj = func_CreateEventA( nullptr, TRUE, ( shouldWait ? TRUE : FALSE ), nullptr );

    if ( obj->eventObj == nullptr )
    {
        throw eir::internal_error_exception();
    }
}

void _event_win32_evthandle_destructor( void *mem ) noexcept
{
    _event_win32_evthandle *obj = (_event_win32_evthandle*)mem;

    BOOL didClose = func_CloseHandle( obj->eventObj );

    assert( didClose == TRUE );
}

void _event_win32_evthandle_set( void *mem, bool shouldWait ) noexcept
{
    _event_win32_evthandle *obj = (_event_win32_evthandle*)mem;

    if ( !shouldWait )
    {
        func_SetEvent( obj->eventObj );
    }
    else
    {
        func_ResetEvent( obj->eventObj );
    }
}

void _event_win32_evthandle_wait( void *mem ) noexcept
{
    _event_win32_evthandle *obj = (_event_win32_evthandle*)mem;

    func_WaitForSingleObject( obj->eventObj, INFINITE );
}

bool _event_win32_evthandle_wait_timed( void *mem, unsigned int msTimeout ) noexcept
{
    _event_win32_evthandle *obj = (_event_win32_evthandle*)mem;

    // Bugfix: we must never allow infinite waiting here. The user would never care if he
    // does lose just one millisecond if he were to wait that long.
    if ( msTimeout == INFINITE )
    {
        msTimeout--;
    }

    DWORD waitResult = func_WaitForSingleObject( obj->eventObj, msTimeout );

    return ( waitResult == WAIT_OBJECT_0 );
}

void _event_win32_evthandle_init( void )
{
    _kernel32Handle = LoadLibraryA( "kernel32.dll" );

    if ( _kernel32Handle )
    {
        func_CreateEventA = (decltype(func_CreateEventA))GetProcAddress( _kernel32Handle, "CreateEventA" );
        func_CloseHandle = (decltype(func_CloseHandle))GetProcAddress( _kernel32Handle, "CloseHandle" );
        func_SetEvent = (decltype(func_SetEvent))GetProcAddress( _kernel32Handle, "SetEvent" );
        func_ResetEvent = (decltype(func_ResetEvent))GetProcAddress( _kernel32Handle, "ResetEvent" );
        func_WaitForSingleObject = (decltype(func_WaitForSingleObject))GetProcAddress( _kernel32Handle, "WaitForSingleObject" );
    }
}

void _event_win32_evthandle_shutdown( void )
{
    func_CreateEventA = nullptr;
    func_CloseHandle = nullptr;
    func_SetEvent = nullptr;
    func_ResetEvent = nullptr;
    func_WaitForSingleObject = nullptr;

    if ( _kernel32Handle )
    {
        FreeLibrary( _kernel32Handle );
    }
}

END_NATIVE_EXECUTIVE

#endif //_WIN32