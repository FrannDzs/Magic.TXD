/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.translator.system.linux.cpp
*  PURPOSE:     Linux implementation of the local system translator
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef __linux__

#define FILE_ACCESS_FLAG ( S_IRUSR | S_IWUSR )

#include <unistd.h>
#include <utime.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>
#include <errno.h>

bool _FileLinux_IsDirectoryAbsolute( const char *path )
{
    struct stat dirInfo;

    if ( stat( path, &dirInfo ) != 0 )
        return false;

    return ( dirInfo.st_mode & S_IFDIR ) != 0;
}

bool _FileLinux_DeleteDirectory( const char *path )
{
    return ( rmdir( path ) == 0 );
}

bool _FileLinux_DeleteFile( const char *path )
{
    return unlink( path ) == 0;
}

bool _FileLinux_CopyFile( const char *src, const char *dst )
{
    int iReadFile = open( src, O_RDONLY, 0 );

    if ( iReadFile == -1 )
        return false;

    int iWriteFile = open( dst, O_CREAT | O_WRONLY | O_ASYNC, FILE_ACCESS_FLAG );

    if ( iWriteFile == -1 )
        return false;

    struct stat read_info;
    if ( fstat( iReadFile, &read_info ) != 0 )
    {
        close( iReadFile );
        close( iWriteFile );
        return false;
    }

    sendfile( iWriteFile, iReadFile, NULL, read_info.st_size );

    close( iReadFile );
    close( iWriteFile );
    return true;
}

bool _FileLinux_RenameFile( const char *src, const char *dst )
{
#if 0
    // CANNOT LINK DIRECTORY WITHOUT ROOT ACCESS RIGHTS.
    if ( link( src, dst ) == 0 )
    {
        if ( unlink( src ) == 0 )
        {
            return true;
        }

        int lnkres = link( dst, src );

        (void)lnkres;
    }

    return false;
#elif 0
    // NOT IMPLEMENTED IN WSL.
    int result = renameat2( AT_FDCWD, src, AT_FDCWD, dst, RENAME_NOREPLACE );

    if ( result != 0 )
    {
        int errocode = errno;

        return false;
    }

    return true;
#else
    // Only viable option?
    // Because it uses least-complicated syscalls that are actually implementable?
    if ( access( dst, F_OK ) == 0 )
    {
        // Must not overwrite/replace.
        return false;
    }

    if ( rename( src, dst ) != 0 )
    {
        // WTF? it can fail?
        return false;
    }

    return true;
#endif //0
}

bool _FileLinux_DoesFileExist( const char *src )
{
    return ( access( src, F_OK ) == 0 );
}

AINLINE filesysAttributes _FileLinux_GetAttributesFromStat( const struct stat& linux_stats )
{
    filesysAttributes attribs;

    eFilesysItemType itemType;

    if ( S_ISREG( linux_stats.st_mode ) )
    {
        itemType = eFilesysItemType::FILE;
    }
    else if ( S_ISDIR( linux_stats.st_mode ) )
    {
        itemType = eFilesysItemType::DIRECTORY;
    }
    else
    {
        itemType = eFilesysItemType::UNKNOWN;
    }
    attribs.type = itemType;
    // TODO: improve the returned attributes by more information.
    return attribs;
}

bool _FileLinux_GetFileAttributesByPath( const char *src, filesysAttributes& attribsOut )
{
    struct stat linux_stats;

    if ( stat( src, &linux_stats ) != 0 )
    {
        return false;
    }

    attribsOut = _FileLinux_GetAttributesFromStat( linux_stats );
    return true;
}

int _FileLinux_StatFile( const char *src, filesysStats& statsOut )
{
    struct stat linux_stats;

    if ( stat( src, &linux_stats ) != 0 )
    {
        return -1;
    }

    statsOut.atime = linux_stats.st_atime;
    statsOut.ctime = linux_stats.st_ctime;
    statsOut.mtime = linux_stats.st_mtime;

    statsOut.attribs = _FileLinux_GetAttributesFromStat( linux_stats );
    return 0;
}

fsOffsetNumber_t _FileLinux_GetFileSize( const char *src )
{
    struct stat linux_stats;

    if ( stat( src, &linux_stats ) != 0 )
    {
        return 0;
    }

    return (fsOffsetNumber_t)linux_stats.st_size;
}

#endif //__linux__
