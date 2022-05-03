/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.memory.msvc.dbg.cpp
*  PURPOSE:     _DEBUG compatible global memory overrides.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef NATEXEC_GLOBALMEM_OVERRIDE

#include "CExecutiveManager.memory.internals.hxx"
#include "CExecutiveManager.dbgheap.hxx"

#ifdef _MSC_VER
#ifdef _DEBUG

BEGIN_NATIVE_EXECUTIVE

// TODO: maybe implement this?
static _CRT_ALLOC_HOOK _pfnAllocHook = nullptr;
static _CRT_DUMP_CLIENT _pfnDumpClient = nullptr;

END_NATIVE_EXECUTIVE

using namespace NativeExecutive;

#define _CRTDBG_ALLOC_MEM_DF        0x01  // Turn on debug allocation
#define _CRTDBG_DELAY_FREE_MEM_DF   0x02  // Don't actually free memory
#define _CRTDBG_CHECK_ALWAYS_DF     0x04  // Check heap every alloc/dealloc
#define _CRTDBG_RESERVED_DF         0x08  // Reserved - do not use
#define _CRTDBG_CHECK_CRT_DF        0x10  // Leak check/diff CRT blocks
#define _CRTDBG_LEAK_CHECK_DF       0x20  // Leak check at program exit

// Compatibility definitions.
// Assumingly used by very old code.
#undef _crtDbgFlag
#undef _crtBreakAlloc

extern "C" int _crtDbgFlag = ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_DEFAULT_DF );
extern "C" long _crtBreakAlloc = -1;

extern "C" int* MEM_CALLCONV __p__crtDbgFlag( void )
{
    return &_crtDbgFlag;
}

extern "C" long* MEM_CALLCONV __p__crtBreakAlloc( void )
{
    return &_crtBreakAlloc;
}

// Standard allocator interface for Debug CRT.
MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _malloc_dbg( size_t memSize, int blockType, const char *filename, int linenumber )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _malloc_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _malloc_impl( memSize );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _calloc_dbg( size_t num, size_t size, int blockType, const char *filename, int linenumber )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _calloc_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _calloc_impl( num, size );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _realloc_dbg( void *memPtr, size_t newSize, int blockType, const char *filename, int linenumber )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _realloc_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _realloc_impl( memPtr, newSize );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _recalloc_dbg( void *memPtr, size_t cnt, size_t newSize, int blockType, const char *filename, int linenumber )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _recalloc_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _recalloc_impl( memPtr, cnt, newSize );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _expand_dbg( void *memPtr, size_t newSize, int blockType, const char *filename, int linenumber )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _expand_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    return _expand_impl( memPtr, newSize );
}

MEM_SPEC size_t MEM_CALLCONV _msize_dbg( void *memPtr, int blockType )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _msize_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    return _msize_impl( memPtr );
}

MEM_SPEC void MEM_CALLCONV _free_dbg( void *memPtr, int blockType )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _free_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    return _free_impl( memPtr );
}

// Some weird alignment-based API.
MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_malloc_dbg( size_t size, size_t alignment, const char *file_name, int lineno )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_malloc_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _aligned_malloc_impl( size, alignment );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_offset_malloc_dbg( size_t size, size_t alignment, size_t offset, const char *file_name, int lineno )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_offset_malloc_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    // TODO.
    return nullptr;
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_realloc_dbg( void *memPtr, size_t newSize, size_t newAlignment, const char *filename, int lineno )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_realloc_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _aligned_realloc_impl( memPtr, newSize, newAlignment );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_recalloc_dbg( void *memPtr, size_t cnt, size_t size, size_t alignment, const char *filename, int lineno )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_recalloc_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _aligned_recalloc_impl( memPtr, cnt, size, alignment );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_offset_realloc_dbg( void *memPtr, size_t size, size_t alignment, size_t offset, const char *filename, int lineno )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_offset_realloc_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    // TODO.
    return nullptr;
}

MEM_SPEC size_t MEM_CALLCONV _aligned_msize_dbg( void *memPtr, size_t alignment, size_t offset )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_msize_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    return _msize_impl( memPtr );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_offset_recalloc_dbg( void *memPtr, size_t cnt, size_t size, size_t alignment, size_t offset, const char *filename, int lineno )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_offset_realloc_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    // TODO.
    return nullptr;
}

MEM_SPEC void MEM_CALLCONV _aligned_free_dbg( void *memPtr )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_free_dbg detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _free_impl( memPtr );
}

// Status API.
extern "C" long MEM_CALLCONV _CrtSetBreakAlloc( long new_break_alloc )
{
    long oldval = _crtBreakAlloc;
    _crtBreakAlloc = new_break_alloc;
    return oldval;
}

extern "C" void MEM_CALLCONV _CrtSetDbgBlockType( void *block, int usetype )
{
    // Not implemented.
}

extern "C" _CRT_ALLOC_HOOK MEM_CALLCONV _CrtGetAllocHook( void )
{
    return _pfnAllocHook;
}

extern "C" _CRT_ALLOC_HOOK MEM_CALLCONV _CrtSetAllocHook( _CRT_ALLOC_HOOK new_hook )
{
    _CRT_ALLOC_HOOK oldval = _pfnAllocHook;
    _pfnAllocHook = new_hook;
    return oldval;
}

extern "C" int MEM_CALLCONV _CrtCheckMemory( void )
{
    if ( (_crtDbgFlag & _CRTDBG_ALLOC_MEM_DF) == 0 )
        return TRUE;

    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    // TODO: maybe allow returning of a real value?

    DbgHeap_Validate();
    return TRUE;
}

extern "C" int MEM_CALLCONV _CrtSetDbgFlag( int new_bits )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    int old_bits = _crtDbgFlag;

    if ( new_bits == _CRTDBG_REPORT_FLAG )
        return old_bits;

    _crtDbgFlag = new_bits;

    return old_bits;
}

#ifdef USE_HEAP_DEBUGGING

struct _crtdbg_shim
{
    _CrtDoForAllClientObjectsCallback cb;
    void *ud;
};

static void _CrtDbg_AllocCallbackShim( void *memPtr, void *ud )
{
    _crtdbg_shim *shim = (_crtdbg_shim*)ud;

    shim->cb( memPtr, shim->ud );
}

#endif //USE_HEAP_DEBUGGING

extern "C" void MEM_CALLCONV _CrtDoForAllClientObjects( _CrtDoForAllClientObjectsCallback cb, void *ud )
{
    if ( (_crtDbgFlag & _CRTDBG_ALLOC_MEM_DF) == 0 )
        return;

    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    // Depends on whether we are using DebugHeap or the global mem allocator.
#ifdef USE_HEAP_DEBUGGING
    _crtdbg_shim shim;
    shim.cb = cb;
    shim.ud = ud;

    DbgHeap_ForAllAllocations( _CrtDbg_AllocCallbackShim, &shim );
#else
    _global_mem_alloc.get().WalkAllocations(
        [&]( void *memPtr )
        {
            cb( memPtr, ud );
        }
    );
#endif //USE_HEAP_DEBUGGING
}

// Backward compatibility definition.
extern "C" int MEM_CALLCONV _CrtIsValidPointer( const void *ptr, unsigned int size_in_bytes, int read_write )
{
    return ( ptr != nullptr ) ? TRUE : FALSE;
}

extern "C" int MEM_CALLCONV _CrtIsValidHeapPointer( const void *ptr )
{
    if ( ptr == nullptr )
        return FALSE;

    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    bool does_own;

#ifdef USE_HEAP_DEBUGGING
    does_own = DbgHeap_IsAllocation( ptr );
#else
    does_own = _global_mem_alloc.get().DoesOwnAllocation( ptr );
#endif //USE_HEAP_DEBUGGING
    
    return does_own ? TRUE : FALSE;
}

extern "C" int MEM_CALLCONV _CrtIsMemoryBlock( const void *ptr, unsigned int size, long *reqnum_out, char **filename_ptr_out, int *lineno_out )
{
    if ( reqnum_out )
        *reqnum_out = 0;

    if ( filename_ptr_out )
        *filename_ptr_out = nullptr;

    if ( lineno_out )
        *lineno_out = 0;

    if ( ptr == nullptr )
        return FALSE;

    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    if ( _CrtIsValidPointer( ptr, size, TRUE ) == FALSE )
        return FALSE;

#ifdef USE_HEAP_DEBUGGING
    if ( DbgHeap_IsAllocation( ptr ) == false )
        return FALSE;
#else
    if ( _global_mem_alloc.get().DoesOwnAllocation( ptr ) == false )
        return FALSE;
#endif //USE_HEAP_DEBUGGING
    
    size_t msize;

#ifdef USE_HEAP_DEBUGGING
    msize = DbgAllocGetSize( ptr );
#else
    msize = _global_mem_alloc.get().GetAllocationSize( ptr );
#endif //USE_HEAP_DEBUGGING

    if ( msize != size )
        return FALSE;

    // TODO: maybe fill out the stuff that the caller wants if we get to implementing
    // them.

    return TRUE;
}

extern "C" int MEM_CALLCONV _CrtReportBlockType( const void *ptr )
{
    if ( _CrtIsValidHeapPointer( ptr ) == FALSE )
        return -1;

    // TODO: maybe implement this in the future.
    
    return TRUE;
}

extern "C" _CRT_DUMP_CLIENT MEM_CALLCONV _CrtGetDumpClient( void )
{
    return _pfnDumpClient;
}

extern "C" _CRT_DUMP_CLIENT MEM_CALLCONV _CrtSetDumpClient( _CRT_DUMP_CLIENT newc )
{
    _CRT_DUMP_CLIENT oldval = _pfnDumpClient;
    _pfnDumpClient = newc;
    return oldval;
}

extern "C" void MEM_CALLCONV _CrtMemCheckpoint( _CrtMemState *memstate )
{
    FATAL_ASSERT( memstate != nullptr );

    // We do not implement this.
    memset( memstate, 0, sizeof(_CrtMemState) );
}

extern "C" int MEM_CALLCONV _CrtMemDifference( _CrtMemState *state, const _CrtMemState *old_state, const _CrtMemState *new_state )
{
    // We do not implement this.
    return FALSE;
}

extern "C" void MEM_CALLCONV _CrtMemDumpAllObjectsSince( const _CrtMemState *state )
{
    // We do not implement this.
    return;
}

extern "C" int MEM_CALLCONV _CrtDumpMemoryLeaks( void )
{
    // We do not implement this.
    return FALSE;
}

extern "C" void MEM_CALLCONV _CrtMemDumpStatistics( const _CrtMemState *state )
{
    // We do not implement this.
    return;
}

// For obj file inclusion.
extern "C" void _MEMEXP_PNAME_ATTRIB __natexec_include_msvc_debug_alloc_overrides( void )
{}

#endif //_DEBUG
#endif //_MSC_VER

#endif //NATEXEC_GLOBALMEM_OVERRIDE