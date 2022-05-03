/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.memory.internals.hxx
*  PURPOSE:     Memory management internal definitions header.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATIVE_EXECUTIVE_MEMORY_NATIVE_INTERNALS_HEADER_
#define _NATIVE_EXECUTIVE_MEMORY_NATIVE_INTERNALS_HEADER_

#include "internal/CExecutiveManager.event.internal.h"
#include "internal/CExecutiveManager.unfairmtx.internal.h"

#include "CExecutiveManager.memory.hxx"

// Decide whether it is a good idea to test all heap pointers for validity.
#ifdef _MSC_VER
#define MEMDATA_DEBUG
#elif defined(_DEBUG)
#define MEMDATA_DEBUG
#endif //_MSC_VER

#include "CExecutiveManager.mtxevtstatic.hxx"

BEGIN_NATIVE_EXECUTIVE

#ifdef NATEXEC_GLOBALMEM_OVERRIDE

#ifndef USE_HEAP_DEBUGGING
extern constinit optional_struct_space <NatExecHeapAllocator> _global_mem_alloc;
#endif //USE_HEAP_DEBUGGING
extern constinit mutexWithEventStatic _global_memlock;

// Call before using any of the memory functions below.
void _prepare_overrides( void );

// Standard implementation of the memory manager.
void* _malloc_impl( size_t memSize );
void* _realloc_impl( void *memptr, size_t memSize );
void _free_impl( void *memptr );
void* _calloc_impl( size_t cnt, size_t memSize );

#ifdef _MSC_VER
// Microsoft CRT only.
void* _recalloc_impl( void *oldMemBlock, size_t num, size_t size );
void* _expand_impl( void *memptr, size_t memSize );
size_t _msize_impl( void *memptr );
void* _aligned_malloc_impl( size_t size, size_t alignment );
void* _aligned_calloc_impl( size_t cnt, size_t size, size_t alignment );
void* _aligned_realloc_impl( void *memptr, size_t newSize, size_t alignment );
void* _aligned_recalloc_impl( void *memptr, size_t cnt, size_t newSize, size_t alignment );
#endif //_MSC_VER

// Calculate the alignment guarrantee that we need.
// These alignment guarantees are platform-dependent and count for the C-version of memory
// allocation functions.
#ifdef _WIN32
// We want to follow Microsoft's specification at
// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/malloc
#if defined(_M_IX86) || defined(_M_ARM)
static constexpr size_t MEM_ALLOC_ALIGNMENT = 8;
#elif defined(_M_AMD64) || defined(_M_ARM64)
static constexpr size_t MEM_ALLOC_ALIGNMENT = 16;
#else
#error Unknown alignment for platform memory allocation functions.
#endif
#else
// NativeHeapAllocator has a default alignment of std::max_align_t.
static constexpr size_t MEM_ALLOC_ALIGNMENT = NatExecHeapAllocator::DEFAULT_ALIGNMENT;
#endif

// We have to define platform-specific function signature conventions for 
// the standard memory allocator interface.
#ifdef _MSC_VER

#define MEM_SPEC extern "C" __declspec(noinline)
#define MEM_RETPTR_SPEC __declspec(restrict)
#ifdef _CRT_NOEXCEPT
#define MEM_CRT_NOEXCEPT _CRT_NOEXCEPT
#else
#define MEM_CRT_NOEXCEPT
#endif //_CRT_NOEXCEPT
#define MEM_CALLCONV __cdecl

// For lib export symbol name preservation.
#ifdef _M_IX86
#define _MEMEXP_PNAME_ATTRIB __cdecl
#define _MEMEXP_PNAME(x) "_" x
#else
#define _MEMEXP_PNAME_ATTRIB
#define _MEMEXP_PNAME(x) x
#endif //ARCHITECTURE SPECIFIC

#else

#define MEM_SPEC
#define MEM_RETPTR_SPEC
#define MEM_CRT_NOEXCEPT
#define MEM_CALLCONV

#endif //PLATFORM DEPENDANT FUNC SIGNATURE CONVENTIONS

#endif //NATEXEC_GLOBALMEM_OVERRIDE

END_NATIVE_EXECUTIVE

#endif //_NATIVE_EXECUTIVE_MEMORY_NATIVE_INTERNALS_HEADER_