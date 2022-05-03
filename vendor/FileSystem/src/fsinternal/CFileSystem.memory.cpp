/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.memory.cpp
*  PURPOSE:     Memory management of this library
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

#include "CFileSystem.internal.h"

#include <sdk/eirutils.h>
#include <sdk/OSUtils.memheap.h>

#include <sdk/PluginHelpers.h>

// If NativeExecutive is available, the memory is piped through that library
// Otherwise we ought to use our own NativeHeapAllocator (good performance).
// This file is required because we treasure the optionality of NativeExecutive
// inclusion!

// Implement the global static allocator.
void* FileSysCommonAllocator::Allocate( void *refPtr, size_t memSize, size_t alignment ) noexcept
{
#ifdef FILESYS_MULTI_THREADING
    return NativeExecutive::NatExecGlobalStaticAlloc::Allocate( nullptr, memSize, alignment );
#else
    return CRTHeapAllocator::Allocate( nullptr, memSize, alignment );
#endif //FILESYS_MULTI_THREADING
}

bool FileSysCommonAllocator::Resize( void *refPtr, void *memPtr, size_t memSize ) noexcept
{
#ifdef FILESYS_MULTI_THREADING
    return NativeExecutive::NatExecGlobalStaticAlloc::Resize( nullptr, memPtr, memSize );
#else
    // There is no Resize method for the CRTHeapAllocator.
    return false;
#endif //FILESYS_MULTI_THREADING
}

void FileSysCommonAllocator::Free( void *refPtr, void *memPtr ) noexcept
{
#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::NatExecGlobalStaticAlloc::Free( nullptr, memPtr );
#else
    CRTHeapAllocator::Free( nullptr, memPtr );
#endif //FILESYS_MULTI_THREADING
}

// Implement the performant object API.
void* CFileSystem::MemAlloc( size_t memSize, size_t alignment ) noexcept
{
    CFileSystemNative *nativeMan = (CFileSystemNative*)this;

#ifdef FILESYS_MULTI_THREADING
    if ( NativeExecutive::CExecutiveManager *execMan = nativeMan->nativeMan )
    {
        return execMan->MemAlloc( memSize, alignment );
    }
#endif //FILESYS_MULTI_THREADING

    return FileSysCommonAllocator::Allocate( nullptr, memSize, alignment );
}

bool CFileSystem::MemResize( void *memPtr, size_t memSize ) noexcept
{
    CFileSystemNative *nativeMan = (CFileSystemNative*)this;

#ifdef FILESYS_MULTI_THREADING
    if ( NativeExecutive::CExecutiveManager *execMan = nativeMan->nativeMan )
    {
        return execMan->MemResize( memPtr, memSize );
    }
#endif //FILESYS_MULTI_THREADING

    return FileSysCommonAllocator::Resize( nullptr, memPtr, memSize );
}

void CFileSystem::MemFree( void *memPtr ) noexcept
{
    CFileSystemNative *nativeMan = (CFileSystemNative*)this;

#ifdef FILESYS_MULTI_THREADING
    if ( NativeExecutive::CExecutiveManager *execMan = nativeMan->nativeMan )
    {
        execMan->MemFree( memPtr );
        return;
    }
#endif //FILESYS_MULTI_THREADING

    FileSysCommonAllocator::Free( nullptr, memPtr );
}

// Module init.
void registerFileSystemMemoryManagement( void )
{
    return;
}

void unregisterFileSystemMemoryManagement( void )
{
    return;
}