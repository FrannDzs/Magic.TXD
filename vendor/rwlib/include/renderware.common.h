/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.common.h
*  PURPOSE:     RenderWare common structures used across this library.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_COMMON_STRUCTURES_HEADER_
#define _RENDERWARE_COMMON_STRUCTURES_HEADER_

#include "renderware.errsys.common.h"

#include <sdk/String.h>
#include <sdk/Vector.h>
#include <sdk/Map.h>
#include <sdk/Set.h>

#include <sdk/MetaHelpers.h>

namespace rw
{

// Main memory allocator for everything:
// Use it as allocatorType for Eir SDK types.
struct RwDynMemAllocator
{
    AINLINE RwDynMemAllocator( Interface *engineInterface ) noexcept
    {
        this->engineInterface = engineInterface;
    }

    AINLINE RwDynMemAllocator( RwDynMemAllocator&& ) = default;
    AINLINE RwDynMemAllocator( const RwDynMemAllocator& ) = default;    // IMPORTANT: has to be possible.

    AINLINE RwDynMemAllocator& operator = ( RwDynMemAllocator&& ) = default;
    AINLINE RwDynMemAllocator& operator = ( const RwDynMemAllocator& ) = default;

    // We implement them later in renderware.h
    AINLINE IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS;
    AINLINE IMPL_HEAP_REDIR_METH_RESIZE_RETURN Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS_DIRECT;
    AINLINE IMPL_HEAP_REDIR_METH_FREE_RETURN Free IMPL_HEAP_REDIR_METH_FREE_ARGS;

    struct is_object {};

private:
    Interface *engineInterface;
};

// Static allocator that is implemented inside RenderWare for usage in static contexts.
// Should be available so that usage of strings, vectors and such can be done without
// initialized RenderWare interface.
// Implemented in "rwmem.cpp".
DEFINE_HEAP_ALLOC( RwStaticMemAllocator );

// We want to rewire all Eir SDK exceptions to RW exceptions.
struct rwEirExceptionManager
{
    [[noreturn]] AINLINE static void throw_oom( size_t lastFailedRequestSize )
    {
        throw OutOfMemoryException( eSubsystemType::UTILITIES, lastFailedRequestSize );
    }
    [[noreturn]] AINLINE static void throw_internal_error( void )
    {
        throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
    }
    [[noreturn]] AINLINE static void throw_type_name_conflict( void )
    {
        throw InvalidConfigurationException( eSubsystemType::UTILITIES, nullptr );
    }
    [[noreturn]] AINLINE static void throw_abstraction_construction( void )
    {
        throw InvalidOperationException( eSubsystemType::UTILITIES, nullptr, nullptr );
    }
    [[noreturn]] AINLINE static void throw_undefined_method( eir::eMethodType type )
    {
        throw InvalidOperationException( eSubsystemType::UTILITIES, nullptr, nullptr );
    }
    [[noreturn]] AINLINE static void throw_not_initialized( void )
    {
        throw NotInitializedException( eSubsystemType::UTILITIES, nullptr );
    }
    [[noreturn]] AINLINE static void throw_math_value_out_of_valid_range( void )
    {
        throw InvalidOperationException( eSubsystemType::UTILITIES, nullptr, nullptr );
    }
    [[noreturn]] AINLINE static void throw_operation_failed( eOperationType opType )
    {
        throw InvalidOperationException( eSubsystemType::UTILITIES, nullptr, nullptr );
    }
    [[noreturn]] AINLINE static void throw_codepoint( const wchar_t *msg = nullptr )
    {
        throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
    }
    [[noreturn]] AINLINE static void throw_index_out_of_bounds( void )
    {
        throw InvalidParameterException( eSubsystemType::UTILITIES, nullptr, nullptr );
    }
    [[noreturn]] AINLINE static void throw_not_found( void )
    {
        throw NotFoundException( eSubsystemType::UTILITIES, nullptr );
    }
};

// The most used types provided using EngineInterface allocator linkage.
template <typename charType>
using rwString = eir::String <charType, RwDynMemAllocator, rwEirExceptionManager>;

template <typename structType>
using rwVector = eir::Vector <structType, RwDynMemAllocator, rwEirExceptionManager>;

template <typename keyType, typename valueType, typename comparatorType = eir::MapDefaultComparator>
using rwMap = eir::Map <keyType, valueType, RwDynMemAllocator, comparatorType, rwEirExceptionManager>;

template <typename valueType, typename comparatorType = eir::SetDefaultComparator>
using rwSet = eir::Set <valueType, comparatorType, RwDynMemAllocator, rwEirExceptionManager>;

// Used types in static contexts.
template <typename charType>
using rwStaticString = eir::String <charType, RwStaticMemAllocator, rwEirExceptionManager>;

template <typename structType>
using rwStaticVector = eir::Vector <structType, RwStaticMemAllocator, rwEirExceptionManager>;

template <typename keyType, typename valueType, eir::CompareCompatibleComparator <keyType, keyType> comparatorType = eir::MapDefaultComparator>
using rwStaticMap = eir::Map <keyType, valueType, RwStaticMemAllocator, comparatorType, rwEirExceptionManager>;

template <typename valueType, eir::CompareCompatibleComparator <valueType, valueType> comparatorType = eir::SetDefaultComparator>
using rwStaticSet = eir::Set <valueType, RwStaticMemAllocator, comparatorType, rwEirExceptionManager>;

} // namespace rw

#endif //_RENDERWARE_COMMON_STRUCTURES_HEADER_