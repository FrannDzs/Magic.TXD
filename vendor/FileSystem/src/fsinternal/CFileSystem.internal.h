/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.internal.h
*  PURPOSE:     Master header of the internal FileSystem modules
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_INTERNAL_LOGIC_
#define _FILESYSTEM_INTERNAL_LOGIC_

#include <sdk/PluginFactory.h>

#ifdef FILESYS_MULTI_THREADING
#include <CExecutiveManager.h>
#endif //FILESYS_MULTI_THREADING

#ifndef _WIN32
#define MAX_PATH 260
#else
#include <direct.h>
#endif

#ifdef __linux__
#include <unistd.h>
#include <dirent.h>
#endif //__linux__

// FileSystem is plugin-based. Drivers register a plugin memory to build on.
#include <sdk/MemoryUtils.h>
#include <sdk/OSUtils.memheap.h>

extern struct CFileSystemNative *nativeFileSystem;

// The native class of the FileSystem library.
struct CFileSystemNative : public CFileSystem
{
public:
    inline CFileSystemNative( const fs_construction_params& params ) : CFileSystem( params )
    {
#ifdef FILESYS_MULTI_THREADING
        this->nativeMan = (NativeExecutive::CExecutiveManager*)params.nativeExecMan;
#endif //FILESYS_MULTI_THREADING

        nativeFileSystem = this;
    }

    inline ~CFileSystemNative( void )
    {
#ifdef _DEBUG
        assert( nativeFileSystem == this );
#endif //_DEBUG

        nativeFileSystem = nullptr;
    }

    // All drivers have to add their registration routines here.
    static void RegisterZIPDriver( const fs_construction_params& params );
    static void UnregisterZIPDriver( void );

    static void RegisterIMGDriver( const fs_construction_params& params );
    static void UnregisterIMGDriver( void );

    // Event when any file translator encounters an open failure.
    void OnTranslatorOpenFailure( eFileOpenFailure reason );

    // Generic things.
    template <typename charType>
    bool                GenGetSystemRootDescriptor( const charType *path, filePath& descOut ) const;
    template <typename charType>
    CFileTranslator*    GenCreateTranslator( const charType *path, eDirOpenFlags flags );
    template <typename charType>
    CFileTranslator*    GenCreateSystemMinimumAccessPoint( const charType *path, eDirOpenFlags flags = DIR_FLAG_NONE );
    template <typename charType>
    CFile*              GenCreateFileIterative( CFileTranslator *trans, const charType *prefix, const charType *suffix, unsigned int maxDupl );

#ifdef FILESYS_MULTI_THREADING
    // Members that are kind of important to all parts of CFileSystem.
    NativeExecutive::CExecutiveManager *nativeMan;
#endif //FILESYS_MULTI_THREADING
};

// The not-thread-safe (default) global memory allocator for plugin memory.
struct FSHeapAllocator
{
    static AINLINE void* Allocate( void *refMem, size_t memSize, size_t alignment ) noexcept
    {
        return heapAlloc.Allocate( memSize, alignment );
    }

    static AINLINE bool Resize( void *refMem, void *memPtr, size_t reqSize ) noexcept
    {
        return heapAlloc.SetAllocationSize( memPtr, reqSize );
    }

    static AINLINE void Free( void *refMem, void *memPtr ) noexcept
    {
        heapAlloc.Free( memPtr );
    }

    static NativeHeapAllocator heapAlloc;
};

typedef StaticPluginClassFactory <CFileSystemNative, FSHeapAllocator> fileSystemFactory_t;

extern fileSystemFactory_t _fileSysFactory;

#include "CFileSystem.internal.common.h"
#include "CFileSystem.internal.lockutil.h"
#include "CFileSystem.random.h"
#include "CFileSystem.stream.buffered.h"
#include "CFileSystem.translator.pathutil.h"
#include "CFileSystem.translator.widewrap.h"
#include "CFileSystem.internal.repo.h"
#include "CFileSystem.config.h"
#include "CFileSystem.FileDataPresence.h"

#endif //_FILESYSTEM_INTERNAL_LOGIC_
