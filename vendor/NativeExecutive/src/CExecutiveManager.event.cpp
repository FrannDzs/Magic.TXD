/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.event.cpp
*  PURPOSE:     Event implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "internal/CExecutiveManager.event.internal.h"

BEGIN_NATIVE_EXECUTIVE

#ifdef _WIN32

// Legacy CreateEvent based implementation for backwards-compatibility.
extern bool _event_win32_evthandle_is_supported( void );
extern size_t _event_win32_evthandle_get_size( void );
extern size_t _event_win32_evthandle_get_alignment( void );
extern void _event_win32_evthandle_constructor( void *mem, bool shouldWait );
extern void _event_win32_evthandle_destructor( void *mem ) noexcept;
extern void _event_win32_evthandle_set( void *mem, bool shouldWait ) noexcept;
extern void _event_win32_evthandle_wait( void *mem ) noexcept;
extern bool _event_win32_evthandle_wait_timed( void *mem, unsigned int msTimeout ) noexcept;
extern void _event_win32_evthandle_init( void );
extern void _event_win32_evthandle_shutdown( void );

#ifndef _COMPILE_FOR_LEGACY

// Modern WaitOnAddress based solution (Win8+)
extern bool _event_win32_waitaddr_is_available( void );
extern size_t _event_win32_waitaddr_get_size( void );
extern size_t _event_win32_waitaddr_get_alignment( void );
extern void _event_win32_waitaddr_constructor( void *mem, bool shouldWait );
extern void _event_win32_waitaddr_destructor( void *mem ) noexcept;
extern void _event_win32_waitaddr_set( void *mem, bool shouldWait ) noexcept;
extern void _event_win32_waitaddr_wait( void *mem ) noexcept;
extern bool _event_win32_waitaddr_wait_timed( void *mem, unsigned int msTimeout ) noexcept;
extern void _event_win32_waitaddr_init( void );
extern void _event_win32_waitaddr_shutdown( void );

#endif //_COMPILE_FOR_LEGACY

#elif defined(__linux__)

extern bool _event_linux_futex_is_supported( void );
extern size_t _event_linux_futex_get_size( void );
extern size_t _event_linux_futex_get_alignment( void );
extern void _event_linux_futex_constructor( void *mem, bool shouldWait );
extern void _event_linux_futex_destructor( void *mem ) noexcept;
extern void _event_linux_futex_set( void *mem, bool shouldWait ) noexcept;
extern void _event_linux_futex_wait( void *mem ) noexcept;
extern bool _event_linux_futex_wait_timed( void *mem, unsigned int msTimeout ) noexcept;
extern void _event_linux_futex_init( void );
extern void _event_linux_futex_shutdown( void );

#endif //CROSS PLATFORM DEFINITIONS

// The chosen implementation of events.
static constinit size_t _evtMemSize = 0;
static constinit size_t _evtAlignment = 0;
static constinit void (*_event_constructor)( void *mem, bool shouldWait ) = nullptr;
static constinit void (*_event_destructor)( void *mem ) noexcept = nullptr;
static constinit void (*_event_set)( void *mem, bool shouldWait ) noexcept = nullptr;
static constinit void (*_event_wait)( void *mem ) noexcept = nullptr;
static constinit bool (*_event_wait_timed)( void *mem, unsigned int msTimeout ) noexcept = nullptr;

// Public API that links to the selected event implementation.
// Can only be used after the library was initialized.
bool pubevent_is_available( void )
{
    return ( _evtMemSize != 0 );
}

size_t pubevent_get_size( void )
{
    return _evtMemSize;
}

size_t pubevent_get_alignment( void )
{
    return _evtAlignment;
}

void pubevent_constructor( void *mem, bool shouldWait )
{
    _event_constructor( mem, shouldWait );
}

void pubevent_destructor( void *mem )
{
    _event_destructor( mem );
}

void pubevent_set( void *mem, bool shouldWait )
{
    _event_set( mem, shouldWait );
}

void pubevent_wait( void *mem )
{
    _event_wait( mem );
}

void pubevent_wait_timed( void *mem, unsigned int msTimeout )
{
    _event_wait_timed( mem, msTimeout );
}

// Library-dynamic API.
CEvent* CExecutiveManager::CreateEvent( bool shouldWait )
{
    if ( _evtMemSize == 0 )
    {
        return nullptr;
    }

    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    void *evtMem = nativeMan->MemAlloc( _evtMemSize, _evtAlignment );

    try
    {
        _event_constructor( evtMem, shouldWait );

        return (CEvent*)evtMem;
    }
    catch( ... )
    {
        nativeMan->MemFree( evtMem );

        throw;
    }
}

void CExecutiveManager::CloseEvent( CEvent *evtObj )
{
    if ( _evtMemSize == 0 )
    {
        return;
    }

    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    _event_destructor( evtObj );

    nativeMan->MemFree( evtObj );
}

size_t CExecutiveManager::GetEventStructSize( void )
{
    return _evtMemSize;
}

size_t CExecutiveManager::GetEventAlignment( void )
{
    return _evtAlignment;
}

CEvent* CExecutiveManager::CreatePlacedEvent( void *mem, bool shouldWait )
{
    _event_constructor( mem, shouldWait );

    return (CEvent*)mem;
}

void CExecutiveManager::ClosePlacedEvent( CEvent *evt )
{
    _event_destructor( evt );
}

// Event methods.
void CEvent::Set( bool shouldWait ) noexcept
{
    _event_set( this, shouldWait );
}

void CEvent::Wait( void ) noexcept
{
    _event_wait( this );
}

bool CEvent::WaitTimed( unsigned int msTimeout ) noexcept
{
    return _event_wait_timed( this, msTimeout );
}

// Event management requires to be an isolated submodule because of the unpredictability of malloc calls.
static constinit std::atomic <size_t> _eventman_refcnt = 0;

// Module init.
void registerEventManagement( void )
{
    size_t prevcnt = _eventman_refcnt++;

    if ( prevcnt > 0 )
        return;

    // Initialize all available event implementations.
#ifdef _WIN32
    _event_win32_evthandle_init();
#ifndef _COMPILE_FOR_LEGACY
    _event_win32_waitaddr_init();
#endif //_COMPILE_FOR_LEGACY
#elif defined(__linux__)
    _event_linux_futex_init();
#endif //CROSS PLATFORM CODE

    // Pick the best implementation.
#ifdef _WIN32

#ifndef _COMPILE_FOR_LEGACY
    if ( _event_win32_waitaddr_is_available() )
    {
        _evtMemSize = _event_win32_waitaddr_get_size();
        _evtAlignment = _event_win32_waitaddr_get_alignment();
        _event_constructor = _event_win32_waitaddr_constructor;
        _event_destructor = _event_win32_waitaddr_destructor;
        _event_set = _event_win32_waitaddr_set;
        _event_wait = _event_win32_waitaddr_wait;
        _event_wait_timed = _event_win32_waitaddr_wait_timed;
        goto pickedImplementation;
    }
#endif //_COMPILE_FOR_LEGACY

    if ( _event_win32_evthandle_is_supported() )
    {
        _evtMemSize = _event_win32_evthandle_get_size();
        _evtAlignment = _event_win32_evthandle_get_alignment();
        _event_constructor = _event_win32_evthandle_constructor;
        _event_destructor = _event_win32_evthandle_destructor;
        _event_set = _event_win32_evthandle_set;
        _event_wait = _event_win32_evthandle_wait;
        _event_wait_timed = _event_win32_evthandle_wait_timed;
        goto pickedImplementation;
    }
#elif defined(__linux__)
    if ( _event_linux_futex_is_supported() )
    {
        _evtMemSize = _event_linux_futex_get_size();
        _evtAlignment = _event_linux_futex_get_alignment();
        _event_constructor = _event_linux_futex_constructor;
        _event_destructor = _event_linux_futex_destructor;
        _event_set = _event_linux_futex_set;
        _event_wait = _event_linux_futex_wait;
        _event_wait_timed = _event_linux_futex_wait_timed;
        goto pickedImplementation;
    }
#else
#error No implementation for CEvent on this platform.
#endif //CROSS PLATFORM CODE

pickedImplementation:;
}

void unregisterEventManagement( void )
{
    size_t newcnt = --_eventman_refcnt;

    if ( newcnt > 0 )
        return;

    // Forget the chosen event implementation type.
    _evtMemSize = 0;
    _evtAlignment = 0;
    _event_constructor = nullptr;
    _event_destructor = nullptr;
    _event_set = nullptr;
    _event_wait = nullptr;
    _event_wait_timed = nullptr;

    // Shutdown all the available implementation.
#ifdef _WIN32
#ifndef _COMPILE_FOR_LEGACY
    _event_win32_waitaddr_shutdown();
#endif //_COMPILE_FOR_LEGACY
    _event_win32_evthandle_shutdown();
#elif defined(__linux__)
    _event_linux_futex_shutdown();
#endif //CROSS PLATFORM CODE
}

END_NATIVE_EXECUTIVE
