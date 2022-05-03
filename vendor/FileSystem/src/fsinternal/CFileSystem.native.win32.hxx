/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.native.win32.hxx
*  PURPOSE:     Win32 related library code.
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_NATIVE_WIN32_INTERNAL_HEADER_
#define _FILESYSTEM_NATIVE_WIN32_INTERNAL_HEADER_

#include "CFileSystem.platform.h"

#ifdef _WIN32

#include <sdk/eirutils.h>

#include <compare>

// We want to load certain functionality from the Win32 kernel.
struct filesysNativeWin32Environment
{
    inline filesysNativeWin32Environment( void ) noexcept
    {
        HMODULE kernel32handle = LoadLibraryA( "Kernel32.dll" );

        this->kernel32handle = kernel32handle;

        if ( kernel32handle != nullptr )
        {
            this->func_SetErrorMode = (decltype(func_SetErrorMode))GetProcAddress( kernel32handle, "SetErrorMode" );
            this->func_SetThreadErrorMode = (decltype(func_SetThreadErrorMode))GetProcAddress( kernel32handle, "SetThreadErrorMode" );
        }
        else
        {
            this->func_SetErrorMode = nullptr;
            this->func_SetThreadErrorMode = nullptr;
        }
    }

    inline ~filesysNativeWin32Environment( void )
    {
        this->func_SetErrorMode = nullptr;
        this->func_SetThreadErrorMode = nullptr;

        if ( HMODULE kernel32handle = this->kernel32handle )
        {
            FreeLibrary( kernel32handle );
            
            this->kernel32handle = nullptr;
        }
    }

    HMODULE kernel32handle;

    decltype( &SetErrorMode ) func_SetErrorMode;
    decltype( &SetThreadErrorMode ) func_SetThreadErrorMode;

    DWORD global_old_error_mode;    // used if there is no SetThreadErrorMode (like on Windows XP)
};

extern constinit optional_struct_space <filesysNativeWin32Environment> filesysNativeWin32Env;

// Helper for stack-based handle processing.
struct win32Handle
{
    AINLINE win32Handle( void ) noexcept : handle( INVALID_HANDLE_VALUE )
    {
        return;
    }
    AINLINE win32Handle( HANDLE handle ) noexcept : handle( handle )
    {
        return;
    }
    AINLINE win32Handle( const win32Handle& ) = delete;
    AINLINE win32Handle( win32Handle&& right ) noexcept
    {
        this->handle = right.handle;

        right.handle = INVALID_HANDLE_VALUE;
    }

    AINLINE void Clear( void ) noexcept
    {
        HANDLE handle = this->handle;

        if ( handle != INVALID_HANDLE_VALUE )
        {
            // Some Win32 functions deserve to be used using static imports.
            CloseHandle( handle );

            this->handle = INVALID_HANDLE_VALUE;
        }
    }

    AINLINE ~win32Handle( void )
    {
        Clear();
    }

    AINLINE win32Handle& operator = ( const win32Handle& ) = delete;
    AINLINE win32Handle& operator = ( win32Handle&& right ) noexcept
    {
        Clear();

        this->handle = right.handle;

        right.handle = INVALID_HANDLE_VALUE;
        
        return *this;
    }

    AINLINE operator HANDLE ( void ) const noexcept
    {
        return this->handle;
    }

    AINLINE operator bool ( void ) const noexcept
    {
        return ( this->handle != INVALID_HANDLE_VALUE );
    }

    AINLINE bool is_good( void ) const noexcept
    {
        return ( this->handle != INVALID_HANDLE_VALUE );
    }

    AINLINE HANDLE detach( void ) noexcept
    {
        HANDLE retHandle = this->handle;

        this->handle = INVALID_HANDLE_VALUE;

        return retHandle;
    }

private:
    HANDLE handle;
};

// Same thing for volume scanning.
struct win32FindVolumeHandle
{
    AINLINE win32FindVolumeHandle( void ) noexcept : handle( INVALID_HANDLE_VALUE )
    {
        return;
    }
    AINLINE win32FindVolumeHandle( HANDLE handle ) noexcept : handle( handle )
    {
        return;
    }
    AINLINE win32FindVolumeHandle( const win32FindVolumeHandle& ) = delete;
    AINLINE win32FindVolumeHandle( win32FindVolumeHandle&& right ) noexcept
    {
        this->handle = right.handle;

        right.handle = INVALID_HANDLE_VALUE;
    }

    AINLINE void Clear( void ) noexcept
    {
        HANDLE handle = this->handle;

        if ( handle != INVALID_HANDLE_VALUE )
        {
            // Some Win32 functions deserve to be used using static imports.
            FindVolumeClose( handle );

            this->handle = INVALID_HANDLE_VALUE;
        }
    }

    AINLINE ~win32FindVolumeHandle( void )
    {
        Clear();
    }

    AINLINE win32FindVolumeHandle& operator = ( const win32FindVolumeHandle& ) = delete;
    AINLINE win32FindVolumeHandle& operator = ( win32FindVolumeHandle&& right ) noexcept
    {
        Clear();

        this->handle = right.handle;

        right.handle = INVALID_HANDLE_VALUE;
        
        return *this;
    }

    AINLINE operator HANDLE ( void ) const noexcept
    {
        return this->handle;
    }

    AINLINE operator bool ( void ) const noexcept
    {
        return ( this->handle != INVALID_HANDLE_VALUE );
    }

    AINLINE bool is_good( void ) const noexcept
    {
        return ( this->handle != INVALID_HANDLE_VALUE );
    }

private:
    HANDLE handle;
};

struct win32DevNotifyHandle
{
    inline win32DevNotifyHandle( void ) noexcept : notifyHandle( NULL )
    {
        return;
    }
    inline win32DevNotifyHandle( HDEVNOTIFY notifyHandle ) noexcept : notifyHandle( notifyHandle )
    {
        return;
    }
    inline win32DevNotifyHandle( const win32DevNotifyHandle& ) = delete;
    inline win32DevNotifyHandle( win32DevNotifyHandle&& right ) noexcept
    {
        this->notifyHandle = right.notifyHandle;

        right.notifyHandle = NULL;
    }

    inline ~win32DevNotifyHandle( void )
    {
        HDEVNOTIFY handle = this->notifyHandle;

        if ( handle != NULL )
        {
            UnregisterDeviceNotification( handle );

            this->notifyHandle = NULL;
        }
    }

    inline win32DevNotifyHandle& operator = ( const win32DevNotifyHandle& ) = delete;
    inline win32DevNotifyHandle& operator = ( win32DevNotifyHandle&& right ) noexcept
    {
        this->~win32DevNotifyHandle();

        return *new (this) win32DevNotifyHandle( std::move( right ) );
    }

    AINLINE operator HDEVNOTIFY ( void ) const noexcept
    {
        return notifyHandle;
    }

    AINLINE operator bool ( void ) const noexcept
    {
        return ( notifyHandle != NULL );
    }

    AINLINE bool is_good( void ) const noexcept
    {
        return ( notifyHandle != NULL );
    }

    AINLINE std::weak_ordering operator <=> ( HDEVNOTIFY right ) const
    {
        return ( this->notifyHandle <=> right );
    }

    AINLINE bool operator == ( HDEVNOTIFY right ) const
    {
        return ( this->notifyHandle == right );
    }

    AINLINE std::weak_ordering operator <=> ( const win32DevNotifyHandle& right ) const
    {
        return ( this->notifyHandle <=> right.notifyHandle );
    }

private:
    HDEVNOTIFY notifyHandle;
};

struct win32WindowHandle
{
    inline win32WindowHandle( void ) noexcept : wnd( NULL )
    {
        return;
    }
    inline win32WindowHandle( HWND wnd ) noexcept : wnd( wnd )
    {
        return;
    }
    inline win32WindowHandle( const win32WindowHandle& ) = delete;
    inline win32WindowHandle( win32WindowHandle&& right ) noexcept
    {
        this->wnd = right.wnd;

        right.wnd = NULL;
    }

    inline ~win32WindowHandle( void )
    {
        HWND wnd = this->wnd;

        if ( wnd != NULL )
        {
            DestroyWindow( wnd );

            this->wnd = NULL;
        }
    }

    inline win32WindowHandle& operator = ( const win32WindowHandle& ) = delete;
    inline win32WindowHandle& operator = ( win32WindowHandle&& right ) noexcept
    {
        this->~win32WindowHandle();

        return *new (this) win32WindowHandle( std::move( right ) );
    }

    inline operator HWND ( void ) const noexcept
    {
        return wnd;
    }

    inline operator bool ( void ) const noexcept
    {
        return ( wnd != NULL );
    }

    inline bool is_good( void ) const noexcept
    {
        return ( wnd != NULL );
    }

private:
    HWND wnd;
};

// Helper for threaded error mode setting.
struct win32_stacked_sysErrorMode
{
    inline win32_stacked_sysErrorMode( DWORD newErrorMode ) noexcept
    {
        if ( auto SetThreadErrorMode = filesysNativeWin32Env.get().func_SetThreadErrorMode )
        {
            BOOL couldSetErrorMode = SetThreadErrorMode( newErrorMode, &oldErrorMode );

            this->hasSetErrorMode = ( couldSetErrorMode != FALSE );
        }
    }

    inline win32_stacked_sysErrorMode( const win32_stacked_sysErrorMode& ) = delete;
    inline win32_stacked_sysErrorMode( win32_stacked_sysErrorMode&& ) = delete;

    inline ~win32_stacked_sysErrorMode( void )
    {
        if ( auto SetThreadErrorMode = filesysNativeWin32Env.get().func_SetThreadErrorMode )
        {
            if ( this->hasSetErrorMode )
            {
                SetThreadErrorMode( this->oldErrorMode, nullptr );
            }
        }
    }

    inline win32_stacked_sysErrorMode& operator = ( const win32_stacked_sysErrorMode& ) = delete;
    inline win32_stacked_sysErrorMode& operator = ( win32_stacked_sysErrorMode&& ) = delete;

private:
    DWORD oldErrorMode;
    bool hasSetErrorMode;
};

#endif //_WIN32

#endif //_FILESYSTEM_NATIVE_WIN32_INTERNAL_HEADER_