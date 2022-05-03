/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.dbgheap.paged.cpp
*  PURPOSE:     Heap management tools for error isolation & debugging
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef USE_HEAP_DEBUGGING
#ifndef USE_FULL_PAGE_HEAP

BEGIN_NATIVE_EXECUTIVE

void _native_initHeap( void )
{
    return;
}

void* _native_allocMem( size_t memSize )
{
    return _native_allocMemPage( memSize );
}

void* _native_reallocMem( void *ptr, size_t size )
{
    return ptr;
}

void _native_freeMem( void *ptr )
{
    if ( !ptr )
        return;

    void *valid_ptr = PAGE_MEM_ADJUST( ptr );
    MEM_INTERRUPT( valid_ptr == ptr );

    _win32_freeMemPage( valid_ptr );
}

void _native_validateMemory( void )
{
    return;
}

void _native_shutdownHeap( void )
{
    return;
}

END_NATIVE_EXECUTIVE

#endif //not USE_FULL_PAGE_HEAP
#endif //USE_HEAP_DEBUGGING