/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwmem.cpp
*  PURPOSE:     General purpose memory allocation subsystem.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwthreading.hxx"

#ifndef RWLIB_ENABLE_THREADING
#include <sdk/OSUtils.memheap.h>
#endif //not RWLIB_ENABLE_THREADING

namespace rw
{

#ifdef RWLIB_ENABLE_THREADING
// If you want override the memory allocation then you should just override the memory callbacks
// inside of the NativeExecutive manager.
#else
static optional_struct_space <NativeHeapAllocator> _global_static_heap_alloc;
static bool _global_static_heap_is_initialized = false;

static NativeHeapAllocator& _get_global_static_heap( void )
{
    // This is actually not a problem because we have no threading support anyway!
    if ( _global_static_heap_is_initialized == false )
    {
        _global_static_heap_alloc.Construct();
        _global_static_heap_is_initialized = true;
    }

    return _global_static_heap_alloc.get();
}
#endif //RWLIB_ENABLE_THREADING

// General memory allocation routines.
// These should be used by the entire library.
void* Interface::MemAllocate( size_t memSize, size_t alignment )
{
    void *memptr = nullptr;

#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *natEngine = (EngineInterface*)this;

    threadingEnvironment *threadEnv = threadingEnv.get().GetPluginStruct( natEngine );

    assert( threadEnv != nullptr );

    memptr = threadEnv->nativeMan->MemAlloc( memSize, alignment );
#else
    memptr = _get_global_static_heap().Allocate( memSize, alignment );
#endif //

    if ( memptr == nullptr )
    {
        throw OutOfMemoryException( eSubsystemType::MEMORY, memSize );
    }

    return memptr;
}

void* Interface::MemAllocateP( size_t memSize, size_t alignment ) noexcept
{
#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *natEngine = (EngineInterface*)this;

    threadingEnvironment *threadEnv = threadingEnv.get().GetPluginStruct( natEngine );

    assert( threadEnv != nullptr );

    return threadEnv->nativeMan->MemAlloc( memSize, alignment );
#else
    return _get_global_static_heap().Allocate( memSize, alignment );
#endif //RWLIB_ENABLE_THREADING
}

bool Interface::MemResize( void *ptr, size_t memSize ) noexcept
{
#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *natEngine = (EngineInterface*)this;

    threadingEnvironment *threadEnv = threadingEnv.get().GetPluginStruct( natEngine );

    assert( threadEnv != nullptr );

    return threadEnv->nativeMan->MemResize( ptr, memSize );
#else
    return _get_global_static_heap().SetAllocationSize( ptr, memSize );
#endif //RWLIB_ENABLE_THREADING
}

void Interface::MemFree( void *ptr ) noexcept
{
#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *natEngine = (EngineInterface*)this;

    threadingEnvironment *threadEnv = threadingEnv.get().GetPluginStruct( natEngine );

    assert( threadEnv != nullptr );

    threadEnv->nativeMan->MemFree( ptr );
#else
    _get_global_static_heap().Free( ptr );
#endif //RWLIB_ENABLE_THREADING
}

void* Interface::PixelAllocate( size_t memSize, size_t alignment )
{
    void *memptr;

#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *natEngine = (EngineInterface*)this;

    threadingEnvironment *threadEnv = threadingEnv.get().GetPluginStruct( natEngine );

    assert( threadEnv != nullptr );

    memptr = threadEnv->nativeMan->MemAlloc( memSize, alignment );
#else
    memptr = _get_global_static_heap().Allocate( memSize, alignment );
#endif //RWLIB_ENABLE_THREADING

    if ( memptr == nullptr )
    {
        throw OutOfMemoryException( eSubsystemType::MEMORY, memSize );
    }

    return memptr;
}

void* Interface::PixelAllocateP( size_t memSize, size_t alignment ) noexcept
{
#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *natEngine = (EngineInterface*)this;

    threadingEnvironment *threadEnv = threadingEnv.get().GetPluginStruct( natEngine );

    assert( threadEnv != nullptr );

    return threadEnv->nativeMan->MemAlloc( memSize, alignment );
#else
    return _get_global_static_heap().Allocate( memSize, alignment );
#endif //RWLIB_ENABLE_THREADING
}

bool Interface::PixelResize( void *ptr, size_t memSize ) noexcept
{
#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *natEngine = (EngineInterface*)this;

    threadingEnvironment *threadEnv = threadingEnv.get().GetPluginStruct( natEngine );

    assert( threadEnv != nullptr );

    return threadEnv->nativeMan->MemResize( ptr, memSize );
#else
    return _get_global_static_heap().SetAllocationSize( ptr, memSize );
#endif //RWLIB_ENABLE_THREADING
}

void Interface::PixelFree( void *ptr ) noexcept
{
#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *natEngine = (EngineInterface*)this;

    threadingEnvironment *threadEnv = threadingEnv.get().GetPluginStruct( natEngine );

    assert( threadEnv != nullptr );

    threadEnv->nativeMan->MemFree( ptr );
#else
    _get_global_static_heap().Free( ptr );
#endif //RWLIB_ENABLE_THREADING
}

// Implement the static API.
IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN RwStaticMemAllocator::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS
{
#ifdef RWLIB_ENABLE_THREADING
    return NativeExecutive::NatExecGlobalStaticAlloc::Allocate( refMem, memSize, alignment );
#else
    return _get_global_static_heap().Allocate( memSize, alignment );
#endif //RWLIB_ENABLE_THREADING
}
IMPL_HEAP_REDIR_METH_RESIZE_RETURN RwStaticMemAllocator::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS_DIRECT
{
#ifdef RWLIB_ENABLE_THREADING
    return NativeExecutive::NatExecGlobalStaticAlloc::Resize( refMem, objMem, reqNewSize );
#else
    return _get_global_static_heap().SetAllocationSize( objMem, reqNewSize );
#endif //RWLIB_ENABLE_THREADING
}
IMPL_HEAP_REDIR_METH_FREE_RETURN RwStaticMemAllocator::Free IMPL_HEAP_REDIR_METH_FREE_ARGS
{
#ifdef RWLIB_ENABLE_THREADING
    NativeExecutive::NatExecGlobalStaticAlloc::Free( refMem, memPtr );
#else
    _get_global_static_heap().Free( memPtr );
#endif //RWLIB_ENABLE_THREADING
}

void registerMemoryEnvironment( void )
{
#ifndef RWLIB_ENABLE_THREADING
    // If not initialized yet then do it.
    _get_global_static_heap();
#endif //not RWLIB_ENABLE_THREADING
}

void unregisterMemoryEnvironment( void )
{
#ifndef RWLIB_ENABLE_THREADING
    if ( _global_static_heap_is_initialized )
    {
        // If the static heap is empty then destroy it here.
        NativeHeapAllocator::heapStats heapStats = _global_static_heap_alloc.get().GetStatistics();

        if ( heapStats.countOfAllocations == 0 )
        {
            // Destroy the heap.
            _global_static_heap_alloc.Destroy();

            _global_static_heap_is_initialized = false;
        }
    }
#endif //not RWLIB_ENABLE_THREADING
}

};
