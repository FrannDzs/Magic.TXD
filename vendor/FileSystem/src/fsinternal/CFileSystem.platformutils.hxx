/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.platformutils.hxx
*  PURPOSE:     FileSystem platform dependant utilities
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_PLATFORM_UTILITIES_
#define _FILESYSTEM_PLATFORM_UTILITIES_

#include "CFileSystem.translator.system.h"

// Make sure you included platform header!

// Function for creating an OS native directory.
inline bool _File_CreateDirectory( const filePath& thePath )
{
#ifdef __linux__
    auto ansiPath = thePath.convert_ansi <FSObjectHeapAllocator> ();

    // 0777 stands for all priviledges.
    if ( mkdir( ansiPath.GetConstString(), 0777 ) == 0 )
        return true;

    switch( errno )
    {
    case EEXIST:
    case 0:
        return true;
    }

    return false;
#elif defined(_WIN32)
    // Make sure a file with that name does not exist.
    DWORD attrib = INVALID_FILE_ATTRIBUTES;

    filePath pathToMaybeFile = thePath;
    pathToMaybeFile.resize( pathToMaybeFile.size() - 1 );

    if ( const char *ansiPath = pathToMaybeFile.c_str() )
    {
        attrib = GetFileAttributesA( ansiPath );
    }
    else if ( const wchar_t *widePath = pathToMaybeFile.w_str() )
    {
        attrib = GetFileAttributesW( widePath );
    }
    else
    {
        // Unknown character type.
        pathToMaybeFile.transform_to <wchar_t> ();

        attrib = GetFileAttributesW( pathToMaybeFile.to_char <wchar_t> () );
    }

    if ( attrib != INVALID_FILE_ATTRIBUTES )
    {
        if ( !( attrib & FILE_ATTRIBUTE_DIRECTORY ) )
            return false;
    }

    BOOL dirSuccess = FALSE;

    if ( const char *ansiPath = thePath.c_str() )
    {
        dirSuccess = CreateDirectoryA( ansiPath, nullptr );
    }
    else if ( const wchar_t *widePath = thePath.w_str() )
    {
        dirSuccess = CreateDirectoryW( widePath, nullptr );
    }
    else
    {
        // Unknown char type.
        auto uniPath = thePath.convert_unicode <FileSysCommonAllocator> ();

        dirSuccess = CreateDirectoryW( uniPath.GetConstString(), nullptr );
    }

    if ( dirSuccess == FALSE )
    {
        DWORD lasterr = GetLastError();

        if ( lasterr == ERROR_ALREADY_EXISTS )
        {
            return true;
        }

        return false;
    }

    return true;
#else
#error Missing implementation
#endif //OS DEPENDANT CODE
}

inline void _FileSys_AppendApplicationRootDirectory( filePath& rootOut )
{
#ifdef _WIN32
    eir::Vector <wchar_t, FileSysCommonAllocator> appRootPath;

    static constexpr size_t INCR_AMOUNT = MAX_PATH;

    appRootPath.Resize( INCR_AMOUNT );

    size_t rootLen;

    while ( true )
    {
        SetLastError( ERROR_SUCCESS );

        DWORD copyCount = GetModuleFileNameW( nullptr, appRootPath.GetData(), (DWORD)appRootPath.GetCount() );

        if ( GetLastError() != ERROR_INSUFFICIENT_BUFFER )
        {
            rootLen = copyCount;
            break;
        }

        appRootPath.Resize( appRootPath.GetCount() + INCR_AMOUNT );
    }

    FileSystem::GetFileNameDirectory( appRootPath.GetData(), rootOut );
#elif defined(__linux__)
    eir::Vector <char, FileSysCommonAllocator> rootPathBuf;

    // TODO: find dynamic way to get read linkpath size.
    size_t linkNameLen = 1024;

    rootPathBuf.Resize( linkNameLen + 1 );

    ssize_t actualLinkSize = readlink( "/proc/self/exe", rootPathBuf.GetData(), linkNameLen );

    rootPathBuf[ (size_t)actualLinkSize ] = 0;

    FileSystem::GetFileNameDirectory( rootPathBuf.GetData(), rootOut );
#else
#error No platform definition for application root
#endif //CROSS PLATFORM CODE
}

inline void _FileSys_AppendCurrentWorkingDirectory( filePath& cwdOut )
{
#ifdef _WIN32
    wchar_t cwd[1024];
    wchar_t *gotCWD = _wgetcwd( cwd, NUMELMS(cwd)-1 );
#elif defined(__linux__)
    char cwd[1024];
    char *gotCWD = getcwd( cwd, NUMELMS(cwd)-1 );
#else
#error No platform definition for current directory
#endif

    if ( !gotCWD )
    {
        cwd[0] = 0;
    }
    else
    {
        cwd[ NUMELMS(cwd)-1 ] = 0;
    }

    cwdOut += cwd;
}

#endif //_FILESYSTEM_PLATFORM_UTILITIES_
