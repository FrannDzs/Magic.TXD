/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.native.win32.cpp
*  PURPOSE:     Win32 related library code.
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef _WIN32

#include "CFileSystem.native.win32.hxx"

constinit optional_struct_space <filesysNativeWin32Environment> filesysNativeWin32Env;

void registerFileSystemNativeWin32Environment( void )
{
    filesysNativeWin32Env.Construct();

    if ( filesysNativeWin32Env.get().func_SetThreadErrorMode == nullptr )
    {
        if ( auto SetErrorMode = filesysNativeWin32Env.get().func_SetErrorMode )
        {
            filesysNativeWin32Env.get().global_old_error_mode = SetErrorMode( SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX );
        }
    }
}

void unregisterFileSystemNativeWin32Environment( void )
{
    if ( filesysNativeWin32Env.get().func_SetThreadErrorMode == nullptr )
    {
        if ( auto SetErrorMode = filesysNativeWin32Env.get().func_SetErrorMode )
        {
            SetErrorMode( filesysNativeWin32Env.get().global_old_error_mode );
        }
    }

    filesysNativeWin32Env.Destroy();
}

#endif //_WIN32