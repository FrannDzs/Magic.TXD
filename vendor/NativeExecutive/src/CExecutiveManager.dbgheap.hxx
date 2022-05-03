/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.dbgheap.hxx
*  PURPOSE:     Heap management tools for error isolation & debugging
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef NATEXEC_HEAP_DEBUG
#define NATEXEC_HEAP_DEBUG

#include "internal/CExecutiveManager.internal.h"

BEGIN_NATIVE_EXECUTIVE

#ifdef USE_HEAP_DEBUGGING

// Malloc functions
void* DbgMalloc( size_t size, size_t alignment );
void* DbgRealloc( void *ptr, size_t size, size_t alignment );
bool DbgAllocSetSize( void *ptr, size_t newSize ) noexcept;
size_t DbgAllocGetSize( const void *ptr ) noexcept;
void DbgFree( void *ptr ) noexcept;

#endif //USE_HEAP_DEBUGGING

typedef void (*pfnMemoryAllocWatch)( void *memPtr, size_t memSize );
typedef void (*pfnMemoryIterator)( void *memPtr, void *ud );

void DbgHeap_Init( void );
void DbgHeap_Validate( void );
void DbgHeap_ForAllAllocations( pfnMemoryIterator cb, void *ud );
bool DbgHeap_IsAllocation( const void *ptr );
void DbgHeap_CheckActiveBlocks( void );
void DbgHeap_SetMemoryAllocationWatch( pfnMemoryAllocWatch allocWatchCallback );
void DbgHeap_Shutdown( void );

END_NATIVE_EXECUTIVE

#endif //NATEXEC_HEAP_DEBUG
