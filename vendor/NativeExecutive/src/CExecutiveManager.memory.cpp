/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.memory.cpp
*  PURPOSE:     Straight-shot memory management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

// In NativeExecutive we pipe (nearly all) memory requests through a central
// provider. Memory requests have to be protected by a lock to be thread-safe.
// Thus we give those structures their own file.

#include "StdInc.h"

#include "CExecutiveManager.memory.internals.hxx"

#include "CExecutiveManager.eventplugin.hxx"

#include "CExecutiveManager.dbgheap.hxx"

#include <cstdlib>
#include <new>

// MUST NOT USE ASSERT IN MEMORY CRITICAL CODE!
#undef assert

BEGIN_NATIVE_EXECUTIVE

// TODO: since we are going to need many events across native executive embedded into the system structure (yes, we want
//  to remove as many calls to malloc as possible), we should create a helper struct for embedding events, just like the
//  memory event.

static constinit optional_struct_space <EventPluginRegister> _natExecMemoryEventReg;

struct natExecMemoryManager
{
    inline natExecMemoryManager( CExecutiveManagerNative *natExec ) : mtxMemLock( _natExecMemoryEventReg.get().GetEvent( natExec ) )
    {
        return;
    }

    inline void Initialize( CExecutiveManagerNative *natExec )
    {
        // TODO: allow the user to specify their own stuff.

        natExec->memoryIntf = &defaultAlloc;
    }

    inline void Shutdown( CExecutiveManagerNative *natExec )
    {
        natExec->memoryIntf = nullptr;
    }

    // Default memory allocator, in case the user does not supply us with their own.
    struct defaultMemAllocator : public MemoryInterface
    {
        void* Allocate( size_t memSize, size_t alignment ) noexcept override
        {
            return defaultMemHeap.Allocate( memSize, alignment );
        }

        bool Resize( void *memPtr, size_t reqSize ) noexcept override
        {
#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
            FATAL_ASSERT( defaultMemHeap.DoesOwnAllocation( memPtr ) == true );
#endif //_DEBUG

            return defaultMemHeap.SetAllocationSize( memPtr, reqSize );
        }

        void Free( void *memPtr ) noexcept override
        {
#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
            FATAL_ASSERT( defaultMemHeap.DoesOwnAllocation( memPtr ) == true );
#endif //_DEBUG

            defaultMemHeap.Free( memPtr );
        }

        NatExecHeapAllocator defaultMemHeap;
    };

    defaultMemAllocator defaultAlloc;

    CUnfairMutexImpl mtxMemLock;
};

static constinit optional_struct_space <PluginDependantStructRegister <natExecMemoryManager, executiveManagerFactory_t>> natExecMemoryEnv;

// Module API.
CUnfairMutex* CExecutiveManager::GetMemoryLock( void )
{
    CExecutiveManagerNative *natExec = (CExecutiveManagerNative*)this;

    natExecMemoryManager *memEnv = natExecMemoryEnv.get().GetPluginStruct( natExec );

    if ( !memEnv )
        return nullptr;

    return &memEnv->mtxMemLock;
}

void* CExecutiveManager::MemAlloc( size_t memSize, size_t alignment ) noexcept
{
    CExecutiveManagerNative *natExec = (CExecutiveManagerNative*)this;

    MemoryInterface *memIntf = natExec->memoryIntf;

    FATAL_ASSERT( memIntf != nullptr );

    // So we basically settled on the fact that memory allocation must not use locks that allocate
    // memory themselves because then a memory allocation would occur that would not be
    // protected under a lock itself, causing thread-insafety.

    CUnfairMutexContext ctxMemLock( natExec->GetMemoryLock() );

    return memIntf->Allocate( memSize, alignment );
}

bool CExecutiveManager::MemResize( void *memPtr, size_t reqSize ) noexcept
{
    CExecutiveManagerNative *natExec = (CExecutiveManagerNative*)this;

    MemoryInterface *memIntf = natExec->memoryIntf;

    FATAL_ASSERT( memIntf != nullptr );

    CUnfairMutexContext ctxMemLock( natExec->GetMemoryLock() );

    return memIntf->Resize( memPtr, reqSize );
}

void CExecutiveManager::MemFree( void *memPtr ) noexcept
{
    CExecutiveManagerNative *natExec = (CExecutiveManagerNative*)this;

    MemoryInterface *memIntf = natExec->memoryIntf;

    FATAL_ASSERT( memIntf != nullptr );

    CUnfairMutexContext ctxMemLock( natExec->GetMemoryLock() );

    memIntf->Free( memPtr );
}

// Access to the memory quota by the statistics API.
void _executive_manager_get_internal_mem_quota( CExecutiveManagerNative *nativeMan, size_t& usedBytesOut, size_t& metaBytesOut )
{
    natExecMemoryManager *memMan = natExecMemoryEnv.get().GetPluginStruct( nativeMan );

    FATAL_ASSERT( memMan != nullptr );

    NatExecHeapAllocator::heapStats stats = memMan->defaultAlloc.defaultMemHeap.GetStatistics();

    // TODO: count in the global allocator hook if it is enabled.

    usedBytesOut = stats.usedBytes;
    metaBytesOut = stats.usedMetaBytes;
}

// Sub-modules.
#ifdef NATEXEC_GLOBALMEM_OVERRIDE

void registerGlobalMemoryOverrides( void );
void unregisterGlobalMemoryOverrides( void );

#endif //NATEXEC_GLOBALMEM_OVERRIDE

// Module init.
void registerMemoryManager( void )
{
#ifdef NATEXEC_GLOBALMEM_OVERRIDE
    registerGlobalMemoryOverrides();
#endif //NATEXEC_GLOBALMEM_OVERRIDE

    // First we need the event.
    _natExecMemoryEventReg.Construct( executiveManagerFactory );

    // Register the memory environment.
    natExecMemoryEnv.Construct( executiveManagerFactory );
}

void unregisterMemoryManager( void )
{
    // Unregister the memory environment.
    natExecMemoryEnv.Destroy();

    // Unregister the memory event.
    _natExecMemoryEventReg.Destroy();

#ifdef NATEXEC_GLOBALMEM_OVERRIDE
    unregisterGlobalMemoryOverrides();
#endif //NATEXEC_GLOBALMEM_OVERRIDE
}

END_NATIVE_EXECUTIVE

#ifdef NATEXEC_GLOBALMEM_OVERRIDE

BEGIN_NATIVE_EXECUTIVE

#ifndef USE_HEAP_DEBUGGING
constinit optional_struct_space <NatExecHeapAllocator> _global_mem_alloc;
#endif //USE_HEAP_DEBUGGING
constinit NativeExecutive::mutexWithEventStatic _global_memlock;

// The event subsystem has its own refcount so this is valid.
extern void registerEventManagement( void );
extern void unregisterEventManagement( void );

END_NATIVE_EXECUTIVE

static constinit size_t _overrides_refcnt = 0;

static void initializeGlobalMemoryOverrides( void )
{
    if ( _overrides_refcnt++ == 0 )
    {
        NativeExecutive::registerEventManagement();

#ifdef USE_HEAP_DEBUGGING
        NativeExecutive::DbgHeap_Init();
#else
        NativeExecutive::_global_mem_alloc.Construct();
#endif //USE_HEAP_DEBUGGING
        NativeExecutive::_global_memlock.Initialize();
    }
}

static void shutdownGlobalMemoryOverrides( void )
{
    if ( --_overrides_refcnt == 0 )
    {
        NativeExecutive::_global_memlock.Shutdown();
#ifdef USE_HEAP_DEBUGGING
        NativeExecutive::DbgHeap_Shutdown();
#else
        NativeExecutive::_global_mem_alloc.Destroy();
#endif //USE_HEAP_DEBUGGING

        NativeExecutive::unregisterEventManagement();
    }
}

// Need to make sure that malloc is initialized no-matter-what.
// Because the event management is its own isolated subsystem we can depend on it.
// We also want the ability to initialize memory management without calling the special entry point on both Windows and Linux.

static constinit bool _malloc_has_initialized_overrides = false;

BEGIN_NATIVE_EXECUTIVE

void _prepare_overrides( void )
{
    if ( !_malloc_has_initialized_overrides )
    {
        _malloc_has_initialized_overrides = true;

        initializeGlobalMemoryOverrides();
    }
}

// Since the C++ programming language does use memory allocation in standard features such as
// exception throwing, we have to provide thread-safe memory allocation systems.
void* _malloc_impl( size_t memSize )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    void *retMem;

#ifdef USE_HEAP_DEBUGGING
    retMem = DbgMalloc( memSize, MEM_ALLOC_ALIGNMENT );
#else
    retMem = _global_mem_alloc.get().Allocate( memSize, MEM_ALLOC_ALIGNMENT );
#endif //USE_HEAP_DEBUGGING

    if ( retMem == nullptr )
    {
        errno = ENOMEM;
    }

    return retMem;
}

END_NATIVE_EXECUTIVE

// We overwrite the CRT functions every time.
MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV malloc( size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global malloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    // malloc could be spuriously called by either DLL init (on Linux) or the
    // throwing of an event. So make sure that we are prepared.
    NativeExecutive::_prepare_overrides();

    return NativeExecutive::_malloc_impl( memSize );
}

BEGIN_NATIVE_EXECUTIVE

void* _realloc_impl( void *memptr, size_t memSize )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    void *retMem;

#ifdef USE_HEAP_DEBUGGING
    retMem = DbgRealloc( memptr, memSize, MEM_ALLOC_ALIGNMENT );
#else

#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
    if ( memptr != nullptr )
    {
        FATAL_ASSERT( _global_mem_alloc.get().DoesOwnAllocation( memptr ) == true );
    }
#endif //_DEBUG

    retMem = _global_mem_alloc.get().Realloc( memptr, memSize, MEM_ALLOC_ALIGNMENT );
#endif

    if ( retMem == nullptr && memSize > 0 )
    {
        errno = ENOMEM;
    }

    return retMem;
}

END_NATIVE_EXECUTIVE

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV realloc( void *memptr, size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global realloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    return NativeExecutive::_realloc_impl( memptr, memSize );
}

BEGIN_NATIVE_EXECUTIVE

void _free_impl( void *memptr )
{
    if ( memptr != nullptr )
    {
        CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
        DbgFree( memptr );
#else

#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
        FATAL_ASSERT( _global_mem_alloc.get().DoesOwnAllocation( memptr ) == true );
#endif //_DEBUG

        _global_mem_alloc.get().Free( memptr );
#endif //USE_HEAP_DEBUGGING
    }
}

END_NATIVE_EXECUTIVE

MEM_SPEC void MEM_CALLCONV free( void *memptr )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global free detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    NativeExecutive::_free_impl( memptr );
}

BEGIN_NATIVE_EXECUTIVE

void* _calloc_impl( size_t cnt, size_t memSize )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    size_t actualSize = ( cnt * memSize );

    void *memBlock;

#ifdef USE_HEAP_DEBUGGING
    memBlock = DbgMalloc( actualSize, MEM_ALLOC_ALIGNMENT );
#else
    memBlock = _global_mem_alloc.get().Allocate( actualSize, MEM_ALLOC_ALIGNMENT );
#endif //USE_HEAP_DEBUGGING

    if ( memBlock != nullptr )
    {
        memset( memBlock, 0, actualSize );
    }
    else
    {
        errno = ENOMEM;
    }

    return memBlock;
}

END_NATIVE_EXECUTIVE

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV calloc( size_t cnt, size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global calloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    return NativeExecutive::_calloc_impl( cnt, memSize );
}

#ifdef _MSC_VER

BEGIN_NATIVE_EXECUTIVE

void* _recalloc_impl( void *oldMemBlock, size_t num, size_t size )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    size_t actualSize = ( num * size );
    size_t oldSize = 0;
    bool wantsOldSize = ( oldMemBlock != nullptr && actualSize > 0 );

    void *newMemBlock;

#ifdef USE_HEAP_DEBUGGING
    if ( oldMemBlock != nullptr )
    {
        oldSize = DbgAllocGetSize( oldMemBlock );
    }

    newMemBlock = DbgRealloc( oldMemBlock, actualSize, MEM_ALLOC_ALIGNMENT );
#else

    if ( oldMemBlock != nullptr )
    {
        oldSize = _global_mem_alloc.get().GetAllocationSize( oldMemBlock );
    }

#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
    if ( oldMemBlock != nullptr )
    {
        FATAL_ASSERT( _global_mem_alloc.get().DoesOwnAllocation( oldMemBlock ) == true );
    }
#endif //_DEBUG

    newMemBlock = _global_mem_alloc.get().Realloc( oldMemBlock, actualSize, MEM_ALLOC_ALIGNMENT );
#endif //USE_HEAP_DEBUGGING

    if ( newMemBlock == nullptr )
    {
        if ( actualSize > 0 )
        {
            errno = ENOMEM;
        }

        return nullptr;
    }

    // Reset new bytes if we have a growing allocation.
    if ( actualSize > oldSize )
    {
        memset( (char*)newMemBlock + oldSize, 0, actualSize - oldSize );
    }

    return newMemBlock;
}

void* _expand_impl( void *memptr, size_t memSize )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    bool couldResize;

#ifdef USE_HEAP_DEBUGGING
    couldResize = DbgAllocSetSize( memptr, memSize );
#else

#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
    FATAL_ASSERT( _global_mem_alloc.get().DoesOwnAllocation( memptr ) == true );
#endif //_DEBUG

    couldResize = _global_mem_alloc.get().SetAllocationSize( memptr, memSize );
#endif //USE_HEAP_DEBUGGING

    if ( couldResize == false )
    {
        errno = ENOMEM;

        return nullptr;
    }

    return memptr;
}

size_t _msize_impl( void *memptr )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return DbgAllocGetSize( memptr );
#else

#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
    FATAL_ASSERT( _global_mem_alloc.get().DoesOwnAllocation( memptr ) == true );
#endif //_DEBUG

    return _global_mem_alloc.get().GetAllocationSize( memptr );
#endif //USE_HEAP_DEBUGGING
}

void* _aligned_malloc_impl( size_t size, size_t alignment )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    void *retMem;

#ifdef USE_HEAP_DEBUGGING
    retMem = DbgMalloc( size, alignment );
#else
    retMem = _global_mem_alloc.get().Allocate( size, alignment );
#endif //USE_HEAP_DEBUGGING

    if ( retMem == nullptr )
    {
        errno = ENOMEM;
    }

    return retMem;
}

void* _aligned_calloc_impl( size_t cnt, size_t size, size_t alignment )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    size_t actualSize = ( cnt * size );

    void *memPtr;

#ifdef USE_HEAP_DEBUGGING
    memPtr = DbgMalloc( actualSize, alignment );
#else
    memPtr = _global_mem_alloc.get().Allocate( actualSize, alignment );
#endif //USE_HEAP_DEBUGGING

    if ( memPtr )
    {
        memset( memPtr, 0, actualSize );
    }
    else
    {
        errno = ENOMEM;
    }

    return memPtr;
}

void* _aligned_realloc_impl( void *memptr, size_t newSize, size_t alignment )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    void *retMem;

#ifdef USE_HEAP_DEBUGGING
    retMem = DbgRealloc( memptr, newSize, alignment );
#else

#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
    if ( memptr != nullptr )
    {
        FATAL_ASSERT( _global_mem_alloc.get().DoesOwnAllocation( memptr ) == true );
    }
#endif //_DEBUG

    retMem = _global_mem_alloc.get().Realloc( memptr, newSize, alignment );
#endif //USE_HEAP_DEBUGGING

    if ( retMem == nullptr && newSize > 0 )
    {
        errno = ENOMEM;
    }

    return retMem;
}

void* _aligned_recalloc_impl( void *memptr, size_t cnt, size_t newSize, size_t alignment )
{
    CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

    size_t actualSize = ( cnt * newSize );
    size_t oldSize = 0;
    bool wantsOldSize = ( memptr != nullptr && actualSize > 0 );

    void *newMemPtr;

#ifdef USE_HEAP_DEBUGGING
    if ( wantsOldSize )
    {
        oldSize = DbgAllocGetSize( memptr );
    }

    newMemPtr = DbgRealloc( memptr, actualSize, alignment );
#else

    if ( wantsOldSize )
    {
        oldSize = _global_mem_alloc.get().GetAllocationSize( memptr );
    }

#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
    if ( memptr != nullptr )
    {
        FATAL_ASSERT( _global_mem_alloc.get().DoesOwnAllocation( memptr ) == true );
    }
#endif //_DEBUG

    newMemPtr = _global_mem_alloc.get().Realloc( memptr, actualSize, alignment );
#endif //USE_HEAP_DEBUGGING

    if ( newMemPtr != nullptr )
    {
        // Reset new bytes if we have a growing allocation.
        if ( actualSize > oldSize )
        {
            memset( (char*)newMemPtr + oldSize, 0, actualSize - oldSize );
        }
    }
    else if ( actualSize > 0 )
    {
        errno = ENOMEM;
    }

    return newMemPtr;
}

END_NATIVE_EXECUTIVE

#endif //_MSC_VER

// Popular C11 function.
MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV aligned_alloc( size_t alignment, size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global aligned_alloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return NativeExecutive::DbgMalloc( memSize, alignment );
#else
    return NativeExecutive::_global_mem_alloc.get().Allocate( memSize, alignment );
#endif //USE_HEAP_DEBUGGING
}

#ifdef __linux__

void* memalign( size_t alignment, size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global memalign detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return NativeExecutive::DbgMalloc( memSize, alignment );
#else
    return NativeExecutive::_global_mem_alloc.get().Allocate( memSize, alignment );
#endif //USE_HEAP_DEBUGGING
}

int posix_memalign( void **ptr, size_t alignment, size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global posix_memalign detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

    void *newptr;

#ifdef USE_HEAP_DEBUGGING
    newptr = NativeExecutive::DbgMalloc( memSize, alignment );
#else
    newptr = NativeExecutive::_global_mem_alloc.get().Allocate( memSize, alignment );
#endif //USE_HEAP_DEBUGGING

    if ( newptr == nullptr )
    {
        return ENOMEM;
    }

    *ptr = newptr;

    return 0;
}

void* valloc( size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global valloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);

    NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return NativeExecutive::DbgMalloc( memSize, pagesize );
#else
    return NativeExecutive::_global_mem_alloc.get().Allocate( memSize, pagesize );
#endif //USE_HEAP_DEBUGGING
}

void* pvalloc( size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global pvalloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);

    NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return NativeExecutive::DbgMalloc( ALIGN_SIZE( memSize, pagesize ), pagesize );
#else
    return NativeExecutive::_global_mem_alloc.get().Allocate( ALIGN_SIZE( memSize, pagesize ), pagesize );
#endif //USE_HEAP_DEBUGGING
}

size_t malloc_usable_size( void *ptr )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global malloc_usable_size detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return NativeExecutive::DbgAllocGetSize( ptr );
#else
    return NativeExecutive::_global_mem_alloc.get().GetAllocationSize( ptr );
#endif //USE_HEAP_DEBUGGING
}

#endif //__linux__

// Also the new operators.
void* operator new ( size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global operator new detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    // We cannot prerequisite that the main entry point was called because there exist CRTs like from
    // Microsoft that use "operator new" inside of them.
    NativeExecutive::_prepare_overrides();

    // TODO: figure out why when debugging in Release mode the IDE decides to stop here for a short while???

    NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return NativeExecutive::DbgMalloc( memSize, __STDCPP_DEFAULT_NEW_ALIGNMENT__ );
#else
    return NativeExecutive::_global_mem_alloc.get().Allocate( memSize, __STDCPP_DEFAULT_NEW_ALIGNMENT__ );
#endif //USE_HEAP_DEBUGGING
}

void* operator new ( size_t memSize, const std::nothrow_t& nothrow_value ) noexcept
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global operator new (nothrow) detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return NativeExecutive::DbgMalloc( memSize, __STDCPP_DEFAULT_NEW_ALIGNMENT__ );
#else
    return NativeExecutive::_global_mem_alloc.get().Allocate( memSize, __STDCPP_DEFAULT_NEW_ALIGNMENT__ );
#endif //USE_HEAP_DEBUGGING
}

void* operator new ( size_t memSize, std::align_val_t alignment )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global operator new (with align) detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    // TODO: actually implement alignment guarrantees.
    return NativeExecutive::DbgMalloc( memSize, (size_t)alignment );
#else
    return NativeExecutive::_global_mem_alloc.get().Allocate( memSize, (size_t)alignment );
#endif //USE_HEAP_DEBUGGING
}

void* operator new ( size_t memSize, std::align_val_t alignment, const std::nothrow_t& ntv ) noexcept
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global operator new (nothrow, with align) detected.\n" );
#endif //NNATEXEC_LOG_GLOBAL_ALLOC

    NativeExecutive::_prepare_overrides();

    NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return NativeExecutive::DbgMalloc( memSize, (size_t)alignment );
#else
    return NativeExecutive::_global_mem_alloc.get().Allocate( memSize, (size_t)alignment );
#endif //USE_HEAP_DEBUGGING
}

void* operator new [] ( size_t memSize )
{ return operator new ( memSize ); }

void* operator new [] ( size_t memSize, const std::nothrow_t& nothrow_value ) noexcept
{ return operator new ( memSize, nothrow_value ); }

void* operator new [] ( size_t memSize, std::align_val_t alignment )
{ return operator new ( memSize, alignment ); }

void* operator new [] ( size_t memSize, std::align_val_t alignment, const std::nothrow_t& ntv ) noexcept
{ return operator new ( memSize, alignment, ntv ); }

AINLINE static void _free_common_pointer( void *ptr )
{
    if ( ptr != nullptr )
    {
        NativeExecutive::CUnfairMutexContext ctxMemLock( NativeExecutive::_global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
        NativeExecutive::DbgFree( ptr );
#else

#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
        FATAL_ASSERT( NativeExecutive::_global_mem_alloc.get().DoesOwnAllocation( ptr ) == true );
#endif //_DEBUG

        NativeExecutive::_global_mem_alloc.get().Free( ptr );
#endif
    }
}

void operator delete ( void *ptr ) noexcept
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global operator delete detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _free_common_pointer( ptr );
}

void operator delete ( void *ptr, const std::nothrow_t& ntv ) noexcept
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global operator delete (nothrow) detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _free_common_pointer( ptr );
}

void operator delete ( void *ptr, size_t memSize ) noexcept
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global operator delete (with size) detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _free_common_pointer( ptr );
}

void operator delete ( void *ptr, std::align_val_t alignment ) noexcept
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global operator delete (with alignment) detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _free_common_pointer( ptr );
}

void operator delete ( void *ptr, std::align_val_t alignment, const std::nothrow_t& ntv ) noexcept
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global operator delete (with alignment, nothrow) detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _free_common_pointer( ptr );
}

void operator delete ( void *ptr, size_t memSize, std::align_val_t alignment ) noexcept
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global operator delete (with size, with alignment) detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _free_common_pointer( ptr );
}

void operator delete [] ( void *ptr ) noexcept
{ operator delete ( ptr ); }

void operator delete [] ( void *ptr, const std::nothrow_t& ntv ) noexcept
{ operator delete ( ptr, ntv ); }

void operator delete [] ( void *ptr, size_t memSize ) noexcept
{ operator delete ( ptr, memSize ); }

void operator delete [] ( void *ptr, std::align_val_t alignment ) noexcept
{ operator delete ( ptr, alignment ); }

void operator delete [] ( void *ptr, std::align_val_t alignment, const std::nothrow_t& ntv ) noexcept
{ operator delete ( ptr, alignment, ntv ); }

void operator delete [] ( void *ptr, size_t memSize, std::align_val_t alignment ) noexcept
{ operator delete ( ptr, memSize, alignment ); }

#endif //NATEXEC_GLOBALMEM_OVERRIDE

BEGIN_NATIVE_EXECUTIVE

void* NatExecGlobalStaticAlloc::Allocate( void *refPtr, size_t memSize, size_t alignment ) noexcept
{
#ifdef NATEXEC_GLOBALMEM_OVERRIDE
    // I guess that we cannot really control the execution of this function thus we always
    // call to prepare overrides.
    _prepare_overrides();

    NativeExecutive::CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return NativeExecutive::DbgMalloc( memSize, alignment );
#else
    return NativeExecutive::_global_mem_alloc.get().Allocate( memSize, alignment );
#endif //USE_HEAP_DEBUGGING
#else
    return CRTHeapAllocator::Allocate( nullptr, memSize, alignment );
#endif //NATEXEC_GLOBALMEM_OVERRIDE
}

bool NatExecGlobalStaticAlloc::Resize( void *refPtr, void *memPtr, size_t memSize ) noexcept
{
#ifdef NATEXEC_GLOBALMEM_OVERRIDE
    NativeExecutive::CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    return NativeExecutive::DbgAllocSetSize( memPtr, memSize );
#else

#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
    FATAL_ASSERT( NativeExecutive::_global_mem_alloc.get().DoesOwnAllocation( memPtr ) == true );
#endif //_DEBUG

    return NativeExecutive::_global_mem_alloc.get().SetAllocationSize( memPtr, memSize );
#endif //USE_HEAP_DEBUGGING
#else

    // TODO: actually implement this using the Win32 flag for HeapReAlloc:
    // HEAP_REALLOC_IN_PLACE_ONLY

    return false;
#endif //NATEXEC_GLOBALMEM_OVERRIDE
}

void NatExecGlobalStaticAlloc::Free( void *refPtr, void *memPtr ) noexcept
{
#ifdef NATEXEC_GLOBALMEM_OVERRIDE
    NativeExecutive::CUnfairMutexContext ctxMemLock( _global_memlock.GetMutex() );

#ifdef USE_HEAP_DEBUGGING
    NativeExecutive::DbgFree( memPtr );
#else

#if defined(MEMDATA_DEBUG) && !defined(NATEXEC_NO_HEAPPTR_VERIFY)
    FATAL_ASSERT( NativeExecutive::_global_mem_alloc.get().DoesOwnAllocation( memPtr ) == true );
#endif //_DEBUG

    NativeExecutive::_global_mem_alloc.get().Free( memPtr );
#endif //USE_HEAP_DEBUGGING
#else
    CRTHeapAllocator::Free( nullptr, memPtr );
#endif //NATEXEC_GLOBALMEM_OVERRIDE
}

#ifdef NATEXEC_GLOBALMEM_OVERRIDE

#ifdef _MSC_VER
// We need special inclusions so that MSVC does not throw away our OBJ files.
#pragma comment(linker, "/INCLUDE:" _MEMEXP_PNAME("__natexec_include_msvc_release_alloc_overrides"))
#ifdef _DEBUG
#pragma comment(linker, "/INCLUDE:" _MEMEXP_PNAME("__natexec_include_msvc_debug_alloc_overrides"))
#endif //_DEBUG
#endif //_MSC_VER

// Global memory overrides module init.
void registerGlobalMemoryOverrides( void )
{
    // This module must be initialized before any other runtime object so that memory allocation goes directly
    // through it. This is usually achieved by overriding the application entry point symbol.

    initializeGlobalMemoryOverrides();
}

void unregisterGlobalMemoryOverrides( void )
{
    shutdownGlobalMemoryOverrides();
}

#endif //NATEXEC_GLOBALMEM_OVERRIDE

END_NATIVE_EXECUTIVE
