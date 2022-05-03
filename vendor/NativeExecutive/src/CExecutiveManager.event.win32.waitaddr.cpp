/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.event.win32.waitaddr.cpp
*  PURPOSE:     Win32 event implementation using WaitOnAddress (Win8+)
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

// Reminder that the C++ specification states that each object should have unique bytes.
// Using WaitOnAddress we can specify one byte of memory that should host a status.
// Change of that status would wake people up.
// Only available from Windows 8 and up.

#include "StdInc.h"

#ifdef _WIN32
#ifndef _COMPILE_FOR_LEGACY

#include <Windows.h>

BEGIN_NATIVE_EXECUTIVE

struct _event_win32_waitaddr
{
    bool shouldWait;
};

static constinit HMODULE _kernel32Handle = nullptr;
static constinit decltype( &WaitOnAddress ) func_WaitOnAddress = nullptr;
static constinit decltype( &WakeByAddressAll ) func_WakeByAddressAll = nullptr;

bool _event_win32_waitaddr_is_available( void )
{
    return (
        func_WaitOnAddress != nullptr &&
        func_WakeByAddressAll != nullptr
    );
}

size_t _event_win32_waitaddr_get_size( void )
{
    return sizeof(_event_win32_waitaddr);
}

size_t _event_win32_waitaddr_get_alignment( void )
{
    // No alignment necessary.
    return 1;
}

void _event_win32_waitaddr_constructor( void *mem, bool shouldWait )
{
    _event_win32_waitaddr *obj = (_event_win32_waitaddr*)mem;

    obj->shouldWait = shouldWait;
}

void _event_win32_waitaddr_destructor( void *mem ) noexcept
{
    // Nothing.
}

void _event_win32_waitaddr_set( void *mem, bool shouldWait )
{
    _event_win32_waitaddr *obj = (_event_win32_waitaddr*)mem;

    obj->shouldWait = shouldWait;

    // We only have to wake if threads can actually be woken.
    // Very sure that this is safe to ommit if otherwise.
    // At worst a wake-up causes a spin anyway.
    if ( shouldWait == false )
    {
        func_WakeByAddressAll( &obj->shouldWait );
    }
}

static const bool _shouldWaitValue = true;

void _event_win32_waitaddr_wait( void *mem ) noexcept
{
    _event_win32_waitaddr *obj = (_event_win32_waitaddr*)mem;

    while ( obj->shouldWait == true )
    {
        func_WaitOnAddress( &obj->shouldWait, (LPVOID)&_shouldWaitValue, sizeof(bool), INFINITE );
    }
}

bool _event_win32_waitaddr_wait_timed( void *mem, unsigned int msTimeout ) noexcept
{
    _event_win32_waitaddr *obj = (_event_win32_waitaddr*)mem;

    BOOL waitSuccess = func_WaitOnAddress( &obj->shouldWait, (LPVOID)&_shouldWaitValue, sizeof(bool), msTimeout );

    return ( waitSuccess == TRUE );
}

void _event_win32_waitaddr_init( void )
{
    _kernel32Handle = LoadLibraryA( "kernelbase.dll" );

    if ( _kernel32Handle != nullptr )
    {
        func_WaitOnAddress = (decltype(func_WaitOnAddress))GetProcAddress( _kernel32Handle, "WaitOnAddress" );
        func_WakeByAddressAll = (decltype(func_WakeByAddressAll))GetProcAddress( _kernel32Handle, "WakeByAddressAll" );
    }
}

void _event_win32_waitaddr_shutdown( void )
{
    func_WaitOnAddress = nullptr;
    func_WakeByAddressAll = nullptr;

    if ( _kernel32Handle != nullptr )
    {
        FreeLibrary( _kernel32Handle );
    }
}

END_NATIVE_EXECUTIVE

#endif //_COMPILE_FOR_LEGACY
#endif //_WIN32
