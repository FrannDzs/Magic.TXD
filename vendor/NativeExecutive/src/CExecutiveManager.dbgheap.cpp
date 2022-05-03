/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.dbgheap.cpp
*  PURPOSE:     Heap management tools for error isolation & debugging
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "CExecutiveManager.dbgheap.hxx"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <sdk/MacroUtils.h>

/*
    DebugHeap memory debugging environment
    - NativeExecutive embedded edition.

    You can use this tool to find memory corruption and leaks in your C++ projects.
    It supports per-module heaps, so that errors can be isolated to game_sa, multiplayer_sa,
    deathmatch, core, GUI, etc. Use global defines in StdInc.h to set debugging properties...

    USE_HEAP_DEBUGGING
        Enables the heap debugger. The global new and delete operators are overloaded, so
        that the memory allocations are monitored. When the module terminates, all its
        memory is free'd. Requirement for DebugHeap to function.
    USE_FULL_PAGE_HEAP
        Enables full-page heap debugging. This option enables you to catch very crusty
        memory corruption issues (heavy out-of-bounds read/writes, buffer overflows, ...).
        For that the Windows Heap management is skipped. VirtualAlloc is used for every
        memory allocation, so that objects reside on their own pages.

        If full-page heap is disabled, the allocation defaults to the Windows Heap. It
        uses its own heap validation routines.

        Options can be used in combination...

        PAGE_HEAP_INTEGRITY_CHECK
            The memory is guarded by checksums on the object intro and outro regions and the
            remainder of the page is filled with a pattern. Once the memory is free'd or a
            validation is requested, the checksums and the pattern are checked using MEM_INTERRUPT.

            You have to enable this option if page heap memory should be free'd on termination.
        PAGE_HEAP_MEMORY_STATS
            Once the module terminates, all leaked memory is counted and free'd. Statistics
            are printed using OutputDebugString. This option only works with PAGE_HEAP_INTEGRITY_CHECK.
    USE_HEAP_STACK_TRACE
        Performs a stacktrace for every allocation made. This setting is useful to track down complicated
        memory leak situations. Use this only in very controlled scenarios, since it can use a lot of memory.

    You can define the macro MEM_INTERRUPT( bool_expr ) yourself. The most basic content is a redirect
    to assert( bool_expr ). If bool_expr is false, a memory error occured. MEM_INTERRUPT can be invoked
    during initialization, runtime and termination of your module.

    Note that debugging application memory usage in general spawns additional meta-data depending on the
    configuration. Using USE_FULL_PAGE_HEAP, the application will quickly go out of allocatable memory since
    huge chunks are allocated. Your main application may not get to properly initialize itself; test in
    a controlled environment instead!

    FEATURE SET:
        finds memory leaks,
        finds invalid (page heap) object free requests,
        detects memory corruption,
        callstack traces of memory leaks

    version 2.0
*/

#ifdef USE_HEAP_DEBUGGING
#include <sdk/rwlist.hpp>

#include "CExecutiveManager.dbgheap.impl.hxx"

#include <algorithm>
#endif //USE_HEAP_DEBUGGING

BEGIN_NATIVE_EXECUTIVE

#ifdef USE_HEAP_DEBUGGING

constinit optional_struct_space <DebugFullPageHeapAllocator> _nativeAlloc;
static pfnMemoryAllocWatch _memAllocWatchCallback;

#ifdef USE_FULL_PAGE_HEAP

void* _native_allocMemPage( size_t memSize )
{
    DebugFullPageHeapAllocator::pageHandle *handle = _nativeAlloc.get().Allocate( nullptr, memSize );

    if ( !handle )
        return nullptr;

    return handle->GetTargetPointer();
}

bool _native_resizeMemPage( void *ptr, size_t newRegionSize )
{
    DebugFullPageHeapAllocator::pageHandle *handle = _nativeAlloc.get().FindHandleByAddress( ptr );

    if ( !handle )
        return false;

    return _nativeAlloc.get().SetHandleSize( handle, newRegionSize );
}

size_t _native_getMemPageAllocationSize( void *ptr )
{
    DebugFullPageHeapAllocator::pageHandle *handle = _nativeAlloc.get().FindHandleByAddress( ptr );

    if ( handle )
    {
        return handle->GetTargetSize();
    }

    return 0;
}

void _native_walkMemory( pfnMemoryIterator cb, void *ud )
{
    _nativeAlloc.get().ForAllHandlesInMemoryOrder(
        [&]( DebugFullPageHeapAllocator::pageHandle *handle )
        {
            void *memptr = handle->GetTargetPointer();

            cb( memptr, ud );
        }
    );
}

bool _native_ownsMemory( const void *ptr )
{
    return ( _nativeAlloc.get().FindHandleByAddress( ptr ) != nullptr );
}

void _native_freeMemPage( void *ptr )
{
    bool releaseSuccess = _nativeAlloc.get().FreeByAddress( ptr );

    MEM_INTERRUPT( releaseSuccess == true );    // pointer to page is invalid

    // This method assures that the pointer given to it is a real
    // pointer that has been previously returned by _win32_allocMemPage.
}

#else
static HANDLE g_privateHeap;

inline static void _native_initHeap( void )
{
    g_privateHeap = HeapCreate( 0, 0, 0 );

    unsigned int info = 0;
    HeapSetInformation( g_privateHeap, HeapCompatibilityInformation, &info, sizeof(info) );
}

inline static void* _native_allocMem( size_t memSize )
{
    return HeapAlloc( g_privateHeap, 0, memSize );
}

inline static void* _native_reallocMem( void *ptr, size_t size )
{
    return HeapReAlloc( g_privateHeap, 0, ptr, size );
}

inline static void _native_freeMem( void *ptr )
{
    if ( ptr )
    {
        MEM_INTERRUPT( HeapValidate( g_privateHeap, 0, ptr ) );
        HeapFree( g_privateHeap, 0, ptr );
    }
}

inline static void _native_validateMemory( void )
{
    MEM_INTERRUPT( HeapValidate( g_privateHeap, 0, NULL ) );
}

inline static void _native_shutdownHeap( void )
{
    MEM_INTERRUPT( HeapValidate( g_privateHeap, 0, NULL ) );
    HeapDestroy( g_privateHeap );
}
#endif //ALLOCATOR SELECTOR

inline void DbgMemAllocEvent( void *memPtr, size_t memSize )
{
    if ( _memAllocWatchCallback )
    {
        _memAllocWatchCallback( memPtr, memSize );
    }
}

// Block header for correctness.
// We sometimes _MUST_ strip the debug block header.
struct _debugMasterHeader
{
    bool hasDebugInfoHeader;
    bool isSilent;

    char pad[2];
};

// General debug block header.
struct _debugBlockHeader
{
#ifdef USE_HEAP_STACK_TRACE
    std::string callStackPrint;
#endif //USE_HEAP_STACK_TRACE
    RwListEntry <_debugBlockHeader> node;
};

static constinit bool _isInManager = false;
static constinit optional_struct_space <RwList <_debugBlockHeader>> _dbgAllocBlocks;

inline bool _doesRequireBlockHeader( void )
{
    bool doesRequire = false;

#ifdef USE_HEAP_STACK_TRACE
    doesRequire = true;
#endif //USE_HEAP_STACK_TRACE

    return doesRequire;
}

inline void _fillDebugMasterHeader( _debugMasterHeader *header, bool hasBlockHeader, bool isSilent ) noexcept
{
    new (header) _debugMasterHeader;

    header->hasDebugInfoHeader = hasBlockHeader;
    header->isSilent = isSilent;
}

inline void _fillDebugBlockHeader( _debugBlockHeader *blockHeader, bool shouldInitExpensiveExtensions )
{
    // Construct the header.
    new (blockHeader) _debugBlockHeader;

    LIST_APPEND( _dbgAllocBlocks.get().root, blockHeader->node );

    // Fill it depending on extensions.
#ifdef USE_HEAP_STACK_TRACE
    if ( shouldInitExpensiveExtensions )
    {
        DbgTrace::IEnvSnapshot *snapshot = DbgTrace::CreateEnvironmentSnapshot();

        if ( snapshot )
        {
            blockHeader->callStackPrint = snapshot->ToString();

            delete snapshot;
        }
    }
#endif //USE_HEAP_STACK_TRACE
}

inline void _killDebugMasterHeader( _debugMasterHeader *header ) noexcept
{
    header->~_debugMasterHeader();
}

inline void _killDebugBlockHeader( _debugBlockHeader *blockHeader ) noexcept
{
    // Unlist us.
    LIST_REMOVE( blockHeader->node );

    blockHeader->~_debugBlockHeader();
}

inline void* DbgMallocNative( size_t memSize, size_t alignment )
{
    void *memPtr = nullptr;

    size_t requiredMemBlockSize = memSize;

    bool requiresBlockHeader = _doesRequireBlockHeader();

    bool hasBlockHeader = false;

    if ( requiresBlockHeader )
    {
        // If we have the possibility to include any kind of headers, we need a master header.
        requiredMemBlockSize += sizeof( _debugMasterHeader );

        // Check whether we should include the debug block header.
        // This one has useful information about how a block came to be.
        if ( _isInManager == false )
        {
            requiredMemBlockSize += sizeof( _debugBlockHeader );

            hasBlockHeader = true;
        }
    }

    bool resetManagerFlag = false;

    if ( _isInManager == false )
    {
        _isInManager = true;

        resetManagerFlag = true;
    }

    // Allocate the memory.
    memPtr = _native_allocMem( requiredMemBlockSize, alignment );

    DbgMemAllocEvent( memPtr, requiredMemBlockSize );

    if ( requiresBlockHeader )
    {
        // Also fill the block header if we have it.
        bool isSilent = ( resetManagerFlag == false );

        if ( hasBlockHeader )
        {
            _debugBlockHeader *blockHeader = (_debugBlockHeader*)memPtr;

            _fillDebugBlockHeader( blockHeader, isSilent == false );

            memPtr = ( blockHeader + 1 );
        }

        // We must construct the master header last.
        _debugMasterHeader *masterHeader = (_debugMasterHeader*)memPtr;

        _fillDebugMasterHeader( masterHeader, hasBlockHeader, isSilent );

        memPtr = ( masterHeader + 1 );
    }

    if ( resetManagerFlag )
    {
        _isInManager = false;
    }

    return memPtr;
}

inline bool DbgAllocSetSizeNative( void *memPtr, size_t newSize )
{
    // If we just resize then we do not have to update any headers.

    bool requiresBlockHeader = _doesRequireBlockHeader();

    size_t extension_by_headers = 0;

    if ( requiresBlockHeader )
    {
        extension_by_headers += sizeof( _debugMasterHeader );

        _debugMasterHeader *masterHeader = (_debugMasterHeader*)memPtr - 1;

        if ( masterHeader->hasDebugInfoHeader )
        {
            extension_by_headers += sizeof( _debugBlockHeader );
        }
    }

    // Update the values.
    memPtr = (char*)memPtr - extension_by_headers;
    newSize += extension_by_headers;

    return _native_resizeMem( memPtr, newSize );
}

inline void* DbgReallocNative( void *memPtr, size_t newSize, size_t alignment )
{
    bool requiresBlockHeader = _doesRequireBlockHeader();

    void *newPtr = nullptr;

    size_t actualNewMemSize = newSize;

    bool hasBlockHeader = false;
    bool isSilent = false;

    if ( requiresBlockHeader )
    {
        actualNewMemSize += sizeof( _debugMasterHeader );

        // Check the master header.
        _debugMasterHeader *masterHeader = (_debugMasterHeader*)memPtr - 1;

        isSilent = masterHeader->isSilent;

        // We might or might not have the block header.
        if ( masterHeader->hasDebugInfoHeader )
        {
            actualNewMemSize += sizeof( _debugBlockHeader );

            // Delete the old block header.
            _debugBlockHeader *oldBlockHeader = (_debugBlockHeader*)masterHeader - 1;

            _killDebugBlockHeader( oldBlockHeader );

            memPtr = oldBlockHeader;

            hasBlockHeader = true;
        }
        else
        {
            memPtr = masterHeader;
        }
    }

    // ReAllocate the memory.
    newPtr = _native_reallocMem( memPtr, actualNewMemSize, alignment );

    if ( newPtr != nullptr )
    {
        DbgMemAllocEvent( newPtr, actualNewMemSize );

        // Resurface the structures.
        if ( requiresBlockHeader )
        {
            if ( hasBlockHeader )
            {
                _debugBlockHeader *blockHeader = (_debugBlockHeader*)newPtr;

                _fillDebugBlockHeader( blockHeader, isSilent == false );

                newPtr = ( blockHeader + 1 );
            }

            // Now the master header.
            _debugMasterHeader *masterHeader = (_debugMasterHeader*)newPtr;

            _fillDebugMasterHeader( masterHeader, hasBlockHeader, isSilent );

            newPtr = ( masterHeader + 1 );
        }
    }

    return newPtr;
}

inline void DbgFreeNative( void *memPtr ) noexcept
{
    bool requiresBlockHeader = _doesRequireBlockHeader();

    void *actualMemPtr = memPtr;

    if ( requiresBlockHeader )
    {
        // Check the master header.
        _debugMasterHeader *masterHeader = (_debugMasterHeader*)memPtr - 1;

        if ( masterHeader->hasDebugInfoHeader )
        {
            _debugBlockHeader *blockHeader = (_debugBlockHeader*)masterHeader - 1;

            // Deconstruct the block header.
            _killDebugBlockHeader( blockHeader );

            actualMemPtr = blockHeader;
        }
        else
        {
            actualMemPtr = masterHeader;
        }

        // Deconstruct the master header.
        _killDebugMasterHeader( masterHeader );
    }

    _native_freeMem( actualMemPtr );
}

inline size_t DbgAllocGetSizeNative( const void *memPtr ) noexcept
{
    size_t theSizeOut = 0;

    bool requiresHeaders = _doesRequireBlockHeader();

    bool hasBlockHeader = false;

    const void *blockPtr = memPtr;
    {
        if ( requiresHeaders )
        {
            const _debugMasterHeader *masterHeader = (const _debugMasterHeader*)memPtr - 1;

            blockPtr = masterHeader;

            if ( masterHeader->hasDebugInfoHeader )
            {
                blockPtr = (const _debugBlockHeader*)blockPtr - 1;

                hasBlockHeader = true;
            }
        }
    }

    theSizeOut = _native_getAllocSize( (void*)blockPtr );

    if ( requiresHeaders )
    {
        theSizeOut -= sizeof( _debugMasterHeader );

        if ( hasBlockHeader )
        {
            theSizeOut -= sizeof( _debugBlockHeader );
        }
    }

    return theSizeOut;
}

size_t DbgAllocGetSize( const void *ptr ) noexcept
{
    return DbgAllocGetSizeNative( ptr );
}

void* DbgMalloc( size_t size, size_t alignment )
{
    MEM_INTERRUPT( size != 0 );

    return DbgMallocNative( size, alignment );
}

void* DbgRealloc( void *ptr, size_t size, size_t alignment )
{
    return DbgReallocNative( ptr, size, alignment );
}

bool DbgAllocSetSize( void *ptr, size_t size ) noexcept
{
    MEM_INTERRUPT( size != 0 );

    return DbgAllocSetSizeNative( ptr, size );
}

void DbgFree( void *ptr ) noexcept
{
    if ( ptr != nullptr )
    {
        DbgFreeNative( ptr );
    }
}

#endif

// DebugHeap initializator routine.
// Called by NativeExecutive at first memory usage instance.
void DbgHeap_Init( void )
{
#ifdef USE_HEAP_DEBUGGING
    // Initialize watch callbacks.
    _memAllocWatchCallback = nullptr;

    _nativeAlloc.Construct();

    _native_initHeap();

    // Init meta variables.
    _isInManager = false;
    _dbgAllocBlocks.Construct();
#endif //USE_HEAP_DEBUGGING
}

// DebugHeap memory validation routine.
// Call it if you want to check for memory corruption globally.
void DbgHeap_Validate( void )
{
#ifdef USE_HEAP_DEBUGGING
    _native_validateMemory();
#endif //USE_HEAP_DEBUGGING
}

// DebugHeap client memory object walking routine.
// Iterates through every active allocation and passes it to the caller.
void DbgHeap_ForAllAllocations( pfnMemoryIterator cb, void *ud )
{
#ifdef USE_HEAP_DEBUGGING
    _native_walkMemory( cb, ud );
#endif //USE_HEAP_DEBUGGING
}

// DebugHeap pointer verification method.
// Returns true if the pointer has been allocated by DebugHeap, false otherwise.
bool DbgHeap_IsAllocation( const void *ptr )
{
#ifdef USE_HEAP_DEBUGGING
    return _native_ownsMemory( ptr );
#else
    return false;
#endif //USE_HEAP_DEBUGGING
}

// DebugHeap memory checkup routine.
// Loops through all memory blocks and tells you about their callstacks.
// Use this in combination with breakpoints.
#ifdef _MSC_VER
#pragma optimize("", off)
#endif //_MSC_VER

void DbgHeap_CheckActiveBlocks( void )
{
#ifdef USE_HEAP_DEBUGGING
    // First we must verify that our memory is in a valid state.
    _native_validateMemory();

#ifdef USE_HEAP_STACK_TRACE
    // Now loop through all blocks.
    LIST_FOREACH_BEGIN( _debugBlockHeader, _dbgAllocBlocks.root, node )

        const std::string& callstack = item->callStackPrint;

        __asm nop       // PUT BREAKPOINT HERE.

    LIST_FOREACH_END
#endif //USE_HEAP_STACK_TRACE

#endif //USE_HEAP_DEBUGGING
}

#ifdef _MSC_VER
#pragma optimize("", on)
#endif //_MSC_VER

// DebugHeap memory callback routines.
// Call these to set specific callbacks for memory watching.
void DbgHeap_SetMemoryAllocationWatch( pfnMemoryAllocWatch allocWatchCallback )
{
#ifdef USE_HEAP_DEBUGGING
    _memAllocWatchCallback = allocWatchCallback;
#endif //USE_HEAP_DEBUGGING
}

// DebugHeap termination routine.
// Probably never called because there is no safe way to do this.
void DbgHeap_Shutdown( void )
{
#ifdef USE_HEAP_DEBUGGING
    // Clear our meta variables.
    _dbgAllocBlocks.Destroy();

    _native_shutdownHeap();

    // Destroy the page manager.
    _nativeAlloc.Destroy();
#endif //USE_HEAP_DEBUGGING
}

END_NATIVE_EXECUTIVE
