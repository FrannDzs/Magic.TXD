/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwwindowing.cpp
*  PURPOSE:     Windowing subsystem implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwwindowing.hxx"

#ifdef __linux__

#include <unistd.h>

#endif //__linux__

namespace rw
{

struct window_construction_params
{
    uint32 clientWidth, clientHeight;
};

Window::Window( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
{
    window_construction_params *params = (window_construction_params*)construction_params;

    this->clientWidth = params->clientWidth;
    this->clientHeight = params->clientHeight;
}

Window::Window( const Window& right ) : RwObject( right )
{
    this->clientWidth = right.clientWidth;
    this->clientHeight = right.clientHeight;
}

Window::~Window( void )
{
    // Nothing to do here.
}

#ifdef _WIN32

optional_struct_space <PluginDependantStructRegister <windowingSystemWin32, RwInterfaceFactory_t>> windowingSystemWin32::win32Windowing;

#endif //_WIN32

// General window routines.
Window* MakeWindow( Interface *intf, uint32 clientWidth, uint32 clientHeight )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;
    (void)engineInterface;

    Window *handle = nullptr;

#ifdef _WIN32

    windowingSystemWin32 *winSys = windowingSystemWin32::GetWindowingSystem( engineInterface );

    if ( winSys )
    {
        window_construction_params params;
        params.clientWidth = clientWidth;
        params.clientHeight = clientHeight;

        GenericRTTI *winObj = engineInterface->typeSystem.Construct( engineInterface, winSys->windowTypeInfo, &params );

        if ( winObj )
        {
            windowingSystemWin32::Win32Window *win32Wnd = (windowingSystemWin32::Win32Window*)RwTypeSystem::GetObjectFromTypeStruct( winObj );

            // The window is fully created.
            // Signal the messaging thread.
            win32Wnd->KickOffMessagingThread();

            handle = win32Wnd;
        }
    }

#endif //_WIN32

    return handle;
}

void Window::SetVisible( bool vis )
{
#ifdef _WIN32
    windowingSystemWin32::Win32Window *window = (windowingSystemWin32::Win32Window*)this;

    ShowWindow( window->windowHandle, vis ? SW_SHOW : SW_HIDE );
#endif //_WIN32
}

bool Window::IsVisible( void ) const
{
#ifdef _WIN32
    windowingSystemWin32::Win32Window *window = (windowingSystemWin32::Win32Window*)this;

    BOOL isVisible = IsWindowVisible( window->windowHandle );

    return ( isVisible ? true : false );
#else
    return false;
#endif //_WIN32
}

void Window::SetClientSize( uint32 clientWidth, uint32 clientHeight )
{
#ifdef _WIN32
    windowingSystemWin32::Win32Window *window = (windowingSystemWin32::Win32Window*)this;

    scoped_rwlock_writer <rwlock> wndLock( window->wndPropertyLock );

    // Transform it into a client rect.
    RECT wndRect = { 0, 0, (LONG)clientWidth, (LONG)clientHeight };
    AdjustWindowRect( &wndRect, window->dwStyle, false );

    uint32 windowWidth = ( wndRect.right - wndRect.left );
    uint32 windowHeight = ( wndRect.bottom - wndRect.top );

    SetWindowPos( window->windowHandle, nullptr, 0, 0, windowWidth, windowHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
#endif //_WIN32
}

void PulseWindowingSystem( Interface *engineInterface )
{
#ifdef _WIN32
    windowingSystemWin32 *wndSys = windowingSystemWin32::GetWindowingSystem( engineInterface );

    if ( wndSys )
    {
        wndSys->Update();
    }
#endif //_WIN32
}

void YieldExecution( uint32 ms )
{
#ifdef _WIN32
    Sleep( ms );
#elif defined(__linux__)
    usleep( ms * 1000 );
#endif //CROSS PLATFORM CODE
}

// Windowing system registration function.
// Fow now Windows OS only.
void registerWindowingSystem( void )
{
#ifdef _WIN32
    windowingSystemWin32::win32Windowing.Construct( engineFactory );
#endif //_WIN32
}

void unregisterWindowingSystem( void )
{
#ifdef _WIN32
    windowingSystemWin32::win32Windowing.Destroy();
#endif //_WIN32
}

};
