/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.dbgheap.impl.hxx
*  PURPOSE:     Heap management tools for error isolation & debugging
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef NATEXEC_DBGHEAP_HEADER_IMPL
#define NATEXEC_DBGHEAP_HEADER_IMPL

#include "internal/CExecutiveManager.internal.h"

#include <sdk/MemoryUtils.h>
#include <sdk/OSUtils.h>

BEGIN_NATIVE_EXECUTIVE

#ifdef USE_FULL_PAGE_HEAP

#ifndef MEM_INTERRUPT
#define MEM_INTERRUPT( expr )   FATAL_ASSERT( expr )
#endif //MEM_INTERRUPT

typedef NativePageAllocator DebugFullPageHeapAllocator;

extern constinit optional_struct_space <DebugFullPageHeapAllocator> _nativeAlloc;

// Since we know that memory is allocated along pages, we can check for
// invalid pointers given to the manager.
#define _PAGE_SIZE_ACTUAL   ( _nativeAlloc.get().GetPageSize() )

//#define PAGE_MEM_ADJUST( ptr )  (void*)( (char*)ptr - ( (size_t)ptr % _PAGE_SIZE_ACTUAL ) )

void* _native_allocMemPage( size_t memSize );
bool _native_resizeMemPage( void *ptr, size_t newRegionSize );
size_t _native_getMemPageAllocationSize( void *ptr );
void _native_freeMemPage( void *ptr );

// The exposed implementation that the user picked.
void _native_initHeap( void );
void* _native_allocMem( size_t memSize, size_t alignment );
bool _native_resizeMem( void *ptr, size_t newSize );
void* _native_reallocMem( void *ptr, size_t newSize, size_t alignment );
size_t _native_getAllocSize( void *ptr );
void _native_freeMem( void *ptr );
void _native_validateMemory( void );
void _native_shutdownHeap( void );

#endif //USE_FULL_PAGE_HEAP

END_NATIVE_EXECUTIVE

#endif //NATEXEC_DBGHEAP_HEADER_IMPL