/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/include/CFileSystem.common.alloc.h
*  PURPOSE:     Common allocator that is used for public types.
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// Since filePath is a type that is commonly used in application code, we do not
// want to increase the usage-burden by requiring the user to specific a custom
// allocator on each construction. So we want to have an allocator that pipes
// all requests to a shared location. Of course, this allocator is open for usage
// by other future contraptions.

#ifndef _FILESYS_COMMON_ALLOCATOR_HEADER_
#define _FILESYS_COMMON_ALLOCATOR_HEADER_

#include <stdlib.h>

// Non-object allocator that pipes all requests to a custom location.
// The reference pointer is never used.
struct FileSysCommonAllocator
{
    static void* Allocate( void *refPtr, size_t memSize, size_t alignment ) noexcept;
    static bool Resize( void *refPtr, void *memPtr, size_t reqSize ) noexcept;
    static void Free( void *refPtr, void *memPtr ) noexcept;
};

#endif //_FILESYS_COMMON_ALLOCATOR_HEADER_