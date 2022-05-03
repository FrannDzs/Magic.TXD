/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwprivate.common.h
*  PURPOSE:     Common definitions such as strings or vectors that are used by magic-rw.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_COMMON_STRUCTURES_PRIVATE_
#define _RENDERWARE_COMMON_STRUCTURES_PRIVATE_

#include <sdk/MetaHelpers.h>
#include <sdk/rwlist.hpp>

namespace rw
{

// Allocation helper for structs that employ a member with allocator capability
// but have an EngineInterface member to pass allocations through already.
#define RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( hostStructType, redirAllocType, redirNode, interfaceFieldName ) \
    IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN hostStructType::redirAllocType::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS \
    { \
        hostStructType *hostStruct = LIST_GETITEM( hostStructType, refMem, redirNode ); \
        return hostStruct->interfaceFieldName->MemAllocateP( memSize, alignment ); \
    } \
    IMPL_HEAP_REDIR_METH_RESIZE_RETURN hostStructType::redirAllocType::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS_DIRECT \
    { \
        hostStructType *hostStruct = LIST_GETITEM( hostStructType, refMem, redirNode ); \
        return hostStruct->interfaceFieldName->MemResize( objMem, reqNewSize ); \
    } \
    IMPL_HEAP_REDIR_METH_FREE_RETURN hostStructType::redirAllocType::Free IMPL_HEAP_REDIR_METH_FREE_ARGS \
    { \
        hostStructType *hostStruct = LIST_GETITEM( hostStructType, refMem, redirNode ); \
        hostStruct->interfaceFieldName->MemFree( memPtr ); \
    }

}

#endif //_RENDERWARE_COMMON_STRUCTURES_PRIVATE_