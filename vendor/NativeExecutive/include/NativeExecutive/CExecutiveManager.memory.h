/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.memory.h
*  PURPOSE:     Memory management publics
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

// During development of FileSystem the need for a global static memory allocator
// transpired. After all, runtime objects need to be created completely decoupled
// from the manager struct. For that reason there should be an export of a global
// static allocator template.

// This allocator template works best if NativeExecutive is built with
// NATEXEC_GLOBALMEM_OVERRIDE
// enabled. Then the template is implemented using actual Eir SDK memory objects,
// increasing performance.

#ifndef _NAT_EXEC_MEMORY_HEADER_
#define _NAT_EXEC_MEMORY_HEADER_

#include <sdk/MetaHelpers.h>

BEGIN_NATIVE_EXECUTIVE

DEFINE_HEAP_ALLOC( NatExecGlobalStaticAlloc );

// Standard allocator type that uses the CExecutiveManager object for allocation.
struct NatExecStandardObjectAllocator
{
    inline NatExecStandardObjectAllocator( CExecutiveManager *natExec ) noexcept
    {
        this->manager = natExec;
    }
    inline NatExecStandardObjectAllocator( const NatExecStandardObjectAllocator& ) = default;

    inline NatExecStandardObjectAllocator& operator = ( const NatExecStandardObjectAllocator& ) = default;

    inline void* Allocate( void *refPtr, size_t memSize, size_t alignment ) noexcept
    {
        return manager->MemAlloc( memSize, alignment );
    }

    inline bool Resize( void *refPtr, void *memPtr, size_t reqSize ) noexcept
    {
        return manager->MemResize( memPtr, reqSize );
    }

    inline void Free( void *refPtr, void *memPtr ) noexcept
    {
        manager->MemFree( memPtr );
    }

    CExecutiveManager *manager;
};

END_NATIVE_EXECUTIVE

#endif //_NAT_EXEC_MEMORY_HEADER_