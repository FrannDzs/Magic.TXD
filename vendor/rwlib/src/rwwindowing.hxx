/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwwindowing.hxx
*  PURPOSE:     RenderWare Threading shared include.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_WINDOWING_INTERNALS_
#define _RENDERWARE_WINDOWING_INTERNALS_

// TODO: work on a windowing system implementation for Linux.

#include "pluginutil.hxx"

#ifdef _WIN32

// Definately depend on windows internals here (windowing system).
#include "native.win32.hxx"

#endif //_WIN32

namespace rw
{

#ifdef _WIN32

struct windowingSystemWin32
{
    WNDCLASSEXW _windowClass;
    ATOM _windowClassHandle;

    static inline windowingSystemWin32* GetWindowingSystem( Interface *engineInterface )
    {
        return win32Windowing.get().GetPluginStruct( (EngineInterface*)engineInterface );
    }

    // We use a special window object for Windows stuff.
    struct Win32Window : public Window
    {
        struct _rwwindow_win32_initialization
        {
            Win32Window *msgWnd;
            HANDLE evtWindowCreated;
        };

        static void _rwwindow_message_handler_thread( thread_t threadHandle, Interface *intf, void *ud )
        {
            Win32Window *msgWnd;
            HANDLE evtWindowCreated;
            {
                const _rwwindow_win32_initialization *params = (const _rwwindow_win32_initialization*)ud;

                msgWnd = params->msgWnd;
                evtWindowCreated = params->evtWindowCreated;
            }

            EngineInterface *engineInterface = (EngineInterface*)intf;

            // Determine what title our window should have.
            rwStaticString <char> windowTitle = GetRunningSoftwareInformation( engineInterface, true );

            // Create a real window rect.
            RECT windowRect = { 0, 0, (LONG)msgWnd->clientWidth, (LONG)msgWnd->clientHeight };
            AdjustWindowRect( &windowRect, msgWnd->dwStyle, false );

            windowingSystemWin32 *windowSys = msgWnd->windowSys;

            HINSTANCE curHandle = windowSys->_windowClass.hInstance;

            // Create a window.
            HWND wndHandle = CreateWindowExA(
                0,
                (LPCSTR)( windowSys->_windowClassHandle ),
                windowTitle.GetConstString(),
                msgWnd->dwStyle,
                300, 300,
                windowRect.right - windowRect.left,
                windowRect.bottom - windowRect.top,
                nullptr, nullptr,
                curHandle,
                nullptr
            );

            if ( wndHandle == nullptr )
            {
                throw InternalErrorException( eSubsystemType::WINDOWING, L"WINDOWING_INTERNERR_WNDCREATE" );
            }

            // Store our class link into the window.
            SetWindowLongPtrW( wndHandle, GWLP_USERDATA, (LONG_PTR)msgWnd );

            msgWnd->windowHandle = wndHandle;

            SetEvent( evtWindowCreated );

            while ( msgWnd->isAlive )
            {
                // Dispatch messages
                if ( msgWnd->isFullyConstructed )
                {
                    MSG msg;

		            while ( true )
		            {
                        BOOL wndMsgRet = GetMessageW( &msg, nullptr, 0, 0 );

                        if ( wndMsgRet <= 0 )
                        {
                            assert( wndMsgRet == 0 );
                            break;
                        }

			            TranslateMessage( &msg );
			            DispatchMessageW( &msg );
		            }
                }
                else
                {
                    // We wait until the window is fully constructed.
                    Sleep( 1 );
                }
            }

            // WARNING: from here on the window is not an entire RenderWare object anymore!!!
            // This means that you must not use it in the ecosystem. Short out every code that
            // could use it into default handlers!

            // Remove our userdata pointer.
            SetWindowLongPtrW( wndHandle, GWLP_USERDATA, NULL );

            // Destroy our window.
            DestroyWindow( wndHandle );

            // Reset the native pointer in the window class.
            msgWnd->windowHandle = nullptr;
        }

        inline Win32Window( EngineInterface *engineInterface, void *construction_params ) : Window( engineInterface, construction_params )
        {
            windowingSystemWin32 *windowSys = GetWindowingSystem( engineInterface );

            if ( !windowSys )
            {
                throw NotInitializedException( eSubsystemType::WINDOWING, L"WINDOWING_WIN32_FRIENDLYNAME" );
            }

            this->windowSys = windowSys;

            // Adjust the windowing system rectangle correctly.
            DWORD dwStyle = ( WS_OVERLAPPEDWINDOW );

            this->dwStyle = dwStyle;

            this->windowHandle = nullptr;

            this->isAlive = true;
            this->isFullyConstructed = false;

            // Give the window special parameters.
            _rwwindow_win32_initialization info;
            info.msgWnd = this;
            info.evtWindowCreated = CreateEvent( nullptr, false, false, nullptr );

            // Create a thread that handles the window.
            this->windowMessageThread = MakeThread( engineInterface, _rwwindow_message_handler_thread, &info );

            if ( !this->windowMessageThread )
            {
                throw InternalErrorException( eSubsystemType::WINDOWING, L"WINDOWING_INTERNERR_THRCREATEFAIL" );
            }

            // Since threads start out suspended, we start it here.
            ResumeThread( engineInterface, this->windowMessageThread );

            // Wait until that thread has created the window.
            WaitForSingleObject( info.evtWindowCreated, INFINITE );

            // Alright. We should be signaling the thread again once the window object is fully created.
            CloseHandle( info.evtWindowCreated );

            // We need to make windows thread-safe. :)
            this->wndPropertyLock = CreateReadWriteLock( engineInterface );

            // Register our window.
            {
                scoped_rwlock_writer <rwlock> sysLock( windowSys->windowingLock );

                LIST_INSERT( windowSys->windowList.root, node );
            }
        }

        inline void KickOffMessagingThread( void )
        {
            // Notify the thread that it can process messages for real.
            this->isFullyConstructed = true;
        }

        inline Win32Window( const Win32Window& right ) : Window( right )
        {
            throw InvalidOperationException( eSubsystemType::WINDOWING, eOperationType::COPY_CONSTRUCT, L"WINDOWING_WIN32WND_FRIENDLYNAME" );
        }

        inline ~Win32Window( void )
        {
            this->isAlive = false;

            // Have to ping the window for destruction.
            PostMessageW( this->windowHandle, WM_QUIT, 0, 0 );

            Interface *engineInterface = this->engineInterface;

            // We must wait until our thread has finished.
            TerminateThread( engineInterface, this->windowMessageThread );

            // Close our handle.
            CloseThread( engineInterface, this->windowMessageThread );

            // Remove us from the list of actively managed windows.
            {
                scoped_rwlock_writer <rwlock> lock( this->windowSys->windowingLock );

                LIST_REMOVE( node );
            }

            // Delete the window property lock.
            if ( rwlock *lock = this->wndPropertyLock )
            {
                CloseReadWriteLock( engineInterface, lock );
            }
        }

        static LRESULT CALLBACK _rwwindow_message_handler_win32( HWND wndHandle, UINT uMsg, WPARAM wParam, LPARAM lParam )
        {
            // This function must be called from one thread only!

            Win32Window *wndClass = (Win32Window*)GetWindowLongPtrW( wndHandle, GWLP_USERDATA );

            if ( wndClass )
            {
                // We can only process messages if we are not destroying ourselves.
                bool isAlive = wndClass->isAlive;

                if ( isAlive )
                {
                    AcquireObject( wndClass );

                    bool hasHandled = false;

                    switch( uMsg )
                    {
                    case WM_SIZE:
                        {
                            scoped_rwlock_writer <rwlock> wndLock( wndClass->wndPropertyLock );

                            // Update our logical client area.
                            RECT clientRect;

                            BOOL gotClientRect = GetClientRect( wndHandle, &clientRect );

                            if ( gotClientRect )
                            {
                                wndClass->clientWidth = ( clientRect.right - clientRect.left );
                                wndClass->clientHeight = ( clientRect.bottom - clientRect.top );
                            }
                        }

                        // Tell the system that this window resized.
                        TriggerEvent( wndClass, event_t::WINDOW_RESIZE, nullptr );

                        hasHandled = true;
                        break;
                    case WM_CLOSE:
                        // We call an event.
                        TriggerEvent( wndClass, event_t::WINDOW_CLOSING, nullptr );

                        hasHandled = true;
                        break;
                    case WM_QUIT:
                        // We received a message for immediate termination.
                        // The application layer should get to the exit ASAP!
                        TriggerEvent( wndClass, event_t::WINDOW_QUIT, nullptr );

                        hasHandled = true;
                        break;
                    }

                    ReleaseObject( wndClass );

                    if ( hasHandled )
                    {
                        return 0;
                    }
                }
            }

            return DefWindowProcW( wndHandle, uMsg, wParam, lParam );
        }

        windowingSystemWin32 *windowSys;

        bool isAlive;
        bool isFullyConstructed;

        HWND windowHandle;
        DWORD dwStyle;

        thread_t windowMessageThread;

        rwlock *wndPropertyLock;

        RwListEntry <Win32Window> node;
    };

    inline void Initialize( EngineInterface *engineInterface )
    {
        // Register the windowing system class.
	    _windowClass = { 0 };
	    _windowClass.cbSize = sizeof(_windowClass);
	    _windowClass.style = CS_HREDRAW | CS_VREDRAW;
	    _windowClass.lpfnWndProc = Win32Window::_rwwindow_message_handler_win32;
	    _windowClass.hInstance = GetModuleHandle( nullptr );
	    _windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	    _windowClass.lpszClassName = L"RwWindow_framework";

        _windowClassHandle = RegisterClassExW(&_windowClass);

#if 0
        HWND testWnd =
            CreateWindowEx(
                0,
                MAKEINTATOM( _windowClassHandle ),
                "test",
                WS_OVERLAPPEDWINDOW,
                300, 300,
                100, 100,
                nullptr, nullptr,
                _windowClass.hInstance,
                nullptr
            );
#endif

        // Register the window type.
        this->windowTypeInfo = engineInterface->typeSystem.RegisterStructType <Win32Window> ( "window", engineInterface->rwobjTypeInfo );

        if ( RwTypeSystem::typeInfoBase *windowTypeInfo = this->windowTypeInfo )
        {
            // Make sure we are an exclusive type.
            // This is because we have to use construction parameters.
            engineInterface->typeSystem.SetTypeInfoExclusive( windowTypeInfo, true );
        }

        // We need a windowing system lock to synchronize creation of windows.
        this->windowingLock = CreateReadWriteLock( engineInterface );

        LIST_CLEAR( windowList.root );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Destroy all yet active windows.
        // This will force destruction no matter what.
        // It would be best if the game destroyed the window itself.
        while ( !LIST_EMPTY( windowList.root ) )
        {
            Win32Window *wndHandle = LIST_GETITEM( Win32Window, windowList.root.next, node );

            engineInterface->DeleteRwObject( wndHandle );
        }

        // Close the windowing system lock.
        if ( rwlock *lock = this->windowingLock )
        {
            CloseReadWriteLock( engineInterface, lock );
        }

        if ( RwTypeSystem::typeInfoBase *windowTypeInfo = this->windowTypeInfo )
        {
            engineInterface->typeSystem.DeleteType( windowTypeInfo );
        }

        // Unregister our window class.
        UnregisterClassW( (LPCWSTR)this->_windowClassHandle, _windowClass.hInstance );
    }

    inline windowingSystemWin32& operator = ( const windowingSystemWin32& right ) = delete;

    inline void Update( void )
    {
        // We fetch messages for whatever.
        // I guess we want to be a good win32 application :-)
        MSG msg;

        unsigned int msgFetched = 0;

		while ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );

            msgFetched++;

            // If we fetched more than 50 messages, we should stop.
            // Let's try more next frame.
            if ( msgFetched > 50 )
            {
                break;
            }
		}
    }

    RwTypeSystem::typeInfoBase *windowTypeInfo;

    static optional_struct_space <PluginDependantStructRegister <windowingSystemWin32, RwInterfaceFactory_t>> win32Windowing;

    // List of all registered windows.
    RwList <Win32Window> windowList;

    rwlock *windowingLock;
};

#endif //_WIN32

};

#endif //_RENDERWARE_WINDOWING_INTERNALS_
