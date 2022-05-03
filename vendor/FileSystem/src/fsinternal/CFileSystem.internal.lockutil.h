/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.internal.lockutil.h
*  PURPOSE:     Locking utilities for proper threading support
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_INTERNAL_LOCKING_UTILS_
#define _FILESYSTEM_INTERNAL_LOCKING_UTILS_

#ifdef FILESYS_MULTI_THREADING

inline NativeExecutive::CReadWriteLock* MakeReadWriteLock( CFileSystem *fsys )
{
    CFileSystemNative *nativeFS = (CFileSystemNative*)fsys;

    if ( NativeExecutive::CExecutiveManager *nativeMan = nativeFS->nativeMan )
    {
        return nativeMan->CreateReadWriteLock();
    }

    return nullptr;
}

inline void DeleteReadWriteLock( CFileSystem *fsys, NativeExecutive::CReadWriteLock *lock )
{
    if ( !lock )
        return;

    CFileSystemNative *nativeFS = (CFileSystemNative*)fsys;

    if ( NativeExecutive::CExecutiveManager *nativeMan = nativeFS->nativeMan )
    {
        nativeMan->CloseReadWriteLock( lock );
    }
}

#endif //FILESYS_MULTI_THREADING

#endif //_FILESYSTEM_INTERNAL_LOCKING_UTILS_