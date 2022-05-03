/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.memory.msvc.release.cpp
*  PURPOSE:     Common MSVC global memory overrides.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef NATEXEC_GLOBALMEM_OVERRIDE

#include "CExecutiveManager.memory.internals.hxx"

using namespace NativeExecutive;

#ifdef _MSC_VER

// *** DOUBLE DECLARATIONS BEGIN ***
// Please look at https://github.com/microsoft/STL/issues/1066 for why we need two overrides.
MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _malloc_base( size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _malloc_base detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _malloc_impl( memSize );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _realloc_base( void *memptr, size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _realloc_base detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _realloc_impl( memptr, memSize );
}

MEM_SPEC void MEM_CALLCONV _free_base( void *memptr )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _free_base detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    _free_impl( memptr );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _calloc_base( size_t cnt, size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _calloc_base detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _calloc_impl( cnt, memSize );
}
// *** DOUBLE DECLARATIONS END ***

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _recalloc_base( void *memblock, size_t num, size_t size )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _recalloc_base detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _recalloc_impl( memblock, num, size );
}

// Please look at https://github.com/microsoft/STL/issues/1066 for why we need two overrides.
MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _recalloc( void *memblock, size_t num, size_t size )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _recalloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _recalloc_impl( memblock, num, size );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _expand( void *memptr, size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _expand detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _expand_impl( memptr, memSize );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _expand_base( void *memptr, size_t memSize )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _expand_base detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _expand_impl( memptr, memSize );
}

MEM_SPEC size_t MEM_CALLCONV _msize_base( void *memptr ) MEM_CRT_NOEXCEPT
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _msize_base detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _msize_impl( memptr );
}

// Please look at https://github.com/microsoft/STL/issues/1066 for why we need two overrides.
MEM_SPEC size_t MEM_CALLCONV _msize( void *memptr )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _msize detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _msize_impl( memptr );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_malloc( size_t size, size_t alignment )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_malloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _aligned_malloc_impl( size, alignment );
}

// TODO: add the offset variants of these aligned alloc functions.
MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_offset_malloc( size_t size, size_t alignment, size_t offset )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_offset_malloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    // TODO.
    return nullptr;
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_offset_realloc( void *memptr, size_t newSize, size_t alignment, size_t offset )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_offset_realloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    // TODO.
    return nullptr;
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_offset_recalloc( void *memptr, size_t cnt, size_t newSize, size_t alignment, size_t offset )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_offset_recalloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    // TODO.
    return nullptr;
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_calloc( size_t cnt, size_t size, size_t alignment )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_calloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _aligned_calloc_impl( cnt, size, alignment );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_realloc( void *memptr, size_t newSize, size_t alignment )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_realloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _aligned_realloc_impl( memptr, newSize, alignment );
}

MEM_SPEC MEM_RETPTR_SPEC void* MEM_CALLCONV _aligned_recalloc( void *memptr, size_t cnt, size_t newSize, size_t alignment )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_recalloc detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _prepare_overrides();

    return _aligned_recalloc_impl( memptr, cnt, newSize, alignment );
}

MEM_SPEC size_t MEM_CALLCONV _aligned_msize( void *memptr, size_t alignment, size_t offset )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_msize detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    return _msize_impl( memptr );
}

MEM_SPEC void MEM_CALLCONV _aligned_free( void *memptr )
{
#ifdef NATEXEC_LOG_GLOBAL_ALLOC
    printf( "call to global _aligned_free detected.\n" );
#endif //NATEXEC_LOG_GLOBAL_ALLOC

    _free_impl( memptr );
}

// For obj file inclusion.
extern "C" void _MEMEXP_PNAME_ATTRIB __natexec_include_msvc_release_alloc_overrides( void )
{}

#endif //_MSC_VER

#endif //NATEXEC_GLOBALMEM_OVERRIDE