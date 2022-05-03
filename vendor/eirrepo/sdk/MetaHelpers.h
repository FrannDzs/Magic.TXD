/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MetaHelpers.h
*  PURPOSE:     Memory management templates and other utils.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _COMMON_META_PROGRAMMING_HELPERS_
#define _COMMON_META_PROGRAMMING_HELPERS_

// Thanks to http://stackoverflow.com/questions/87372/check-if-a-class-has-a-member-function-of-a-given-signature
#include <type_traits>

#include "rwlist.hpp"

namespace eir
{

template <typename To, typename From>
concept nothrow_move_assignable_with = requires ( To& a, From b ) { noexcept(a = std::move(b)); };

template <typename To, typename From>
concept copy_assignable_with = requires ( To& a, const From b ) { a = b; };

} // namespace eir

#define INSTANCE_METHCHECKEX( checkName, methName ) \
    template<typename, typename T> \
    struct has_##checkName { \
        static_assert( \
            std::integral_constant<T, false>::value, \
            "Second template parameter needs to be of function type."); \
    }; \
    template<typename C, typename Ret, typename... Args> \
    struct has_##checkName <C, Ret(Args...)> { \
    private: \
        template<typename T> \
        static constexpr auto check(T*) \
        -> typename \
            std::is_same< \
                decltype( std::declval<T>().methName( std::declval<Args>()... ) ), \
                Ret \
            >::type; \
        template<typename> \
        static constexpr std::false_type check(...); \
        typedef decltype(check<C>(0)) type; \
    public: \
        static constexpr bool value = type::value; \
    };

#define INSTANCE_METHCHECK( methName ) INSTANCE_METHCHECKEX( methName, methName )

#define PERFORM_METHCHECK( className, methName, funcSig )    ( has_##methName <className, funcSig>::value )

// Check if a class has a specific field.
#define INSTANCE_FIELDCHECK( fieldName ) \
    template <typename T> \
    concept hasField_##fieldName = requires( T t ) { t.fieldName; }
    
#define PERFORM_FIELDCHECK( className, fieldName ) ( hasField_##fieldName <className> )

#define INSTANCE_SUBSTRUCTCHECK( subStructName ) \
    template <typename T> \
    concept hasSubStruct_##subStructName = requires { typename T::subStructName; T::subStructName(); }

#define PERFORM_SUBSTRUCTCHECK( className, subStructName ) ( hasSubStruct_##subStructName <className> )

#define DEFINE_HEAP_REDIR_ALLOC_DEFAULTED_OBJMANIPS( allocTypeName ) \
    AINLINE allocTypeName( void ) = default; \
    AINLINE allocTypeName( allocTypeName&& ) = default; \
    AINLINE allocTypeName( const allocTypeName& ) = default; \
    AINLINE allocTypeName& operator = ( allocTypeName&& ) = default; \
    AINLINE allocTypeName& operator = ( const allocTypeName& ) = default;

// Providing the everything-purpose standard allocator pattern in the Eir SDK!
// We want a common setup where the link to the DynamicTypeSystem (DTS) is fixed to the position of the DTS.
#define DEFINE_HEAP_REDIR_ALLOC_BYSTRUCT_STDLAYOUT( allocTypeName, redirAllocTypeName ) \
    DEFINE_HEAP_REDIR_ALLOC_DEFAULTED_OBJMANIPS( allocTypeName ) \
    static inline void* Allocate( void *refMem, size_t memSize, size_t alignment ) noexcept; \
    static inline bool Resize( void *refMem, void *objMem, size_t reqNewSize ) noexcept requires ( eir::ResizeMemoryAllocator <redirAllocTypeName> ); \
    static inline void* Realloc( void *refMem, void *objMem, size_t reqNewSize, size_t alignment ) noexcept requires ( eir::ReallocMemoryAllocator <redirAllocTypeName> ); \
    static inline void Free( void *refMem, void *memPtr ) noexcept;
#define DEFINE_HEAP_REDIR_ALLOC_BYSTRUCT( allocTypeName, redirAllocTypeName ) \
    struct allocTypeName \
    { \
        DEFINE_HEAP_REDIR_ALLOC_BYSTRUCT_STDLAYOUT( allocTypeName, redirAllocTypeName ) \
    };
// Defines a heap redir allocator with resize-semantics.
#define DEFINE_HEAP_REDIR_ALLOC_IMPL_STDLAYOUT( allocTypeName ) \
    DEFINE_HEAP_REDIR_ALLOC_DEFAULTED_OBJMANIPS( allocTypeName ) \
    static inline void* Allocate( void *refMem, size_t memSize, size_t alignment ) noexcept; \
    static inline bool Resize( void *refMem, void *objMem, size_t reqNewSize ) noexcept; \
    static inline void Free( void *refMem, void *memPtr ) noexcept;
#define DEFINE_HEAP_REDIR_ALLOC_IMPL( allocTypeName ) \
    struct allocTypeName \
    { \
        DEFINE_HEAP_REDIR_ALLOC_IMPL_STDLAYOUT( allocTypeName ) \
    };

// Simple compatibility definitions for redir-allocators.
#define DEFINE_HEAP_REDIR_ALLOC_COMPATMETH( allocTypeName, compatAllocTypeName ) \
    AINLINE allocTypeName( struct compatAllocTypeName&& ) noexcept {} \
    AINLINE allocTypeName& operator = ( struct compatAllocTypeName&& ) noexcept { return *this; }

// Non-inline version of the heap allocator template.
#define DEFINE_HEAP_ALLOC( allocTypeName ) \
    struct allocTypeName \
    { \
        AINLINE allocTypeName( void ) = default; \
        AINLINE allocTypeName( allocTypeName&& ) = default; \
        AINLINE allocTypeName( const allocTypeName& ) = default; \
        AINLINE allocTypeName& operator = ( allocTypeName&& ) = default; \
        AINLINE allocTypeName& operator = ( const allocTypeName& ) = default; \
        static void* Allocate( void *refMem, size_t memSize, size_t alignment ) noexcept; \
        static bool Resize( void *refMem, void *objMem, size_t reqNewSize ) noexcept; \
        static void Free( void *refMem, void *memPtr ) noexcept; \
    };

// This thing assumes that the object pointed at by allocNode is of type "NativeHeapAllocator",
// but you may of course implement your own thing that has the same semantics.
#define IMPL_HEAP_REDIR_ALLOC( allocTypeName, hostStructTypeName, redirNode, allocNode ) \
    void* allocTypeName::Allocate( void *refMem, size_t memSize, size_t alignment ) noexcept \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return hostStruct->allocNode.Allocate( memSize, alignment ); \
    } \
    bool allocTypeName::Resize( void *refMem, void *objMem, size_t reqNewSize ) noexcept \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return hostStruct->allocNode.SetAllocationSize( objMem, reqNewSize ); \
    } \
    void allocTypeName::Free( void *refMem, void *memPtr ) noexcept \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        hostStruct->allocNode.Free( memPtr ); \
    }

// Default macros for the allocator templates.
#define IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS ( void *refMem, size_t memSize, size_t alignment ) noexcept
#define IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN void*

#define IMPL_HEAP_REDIR_METH_RESIZE_ARGS_DIRECT ( void *refMem, void *objMem, size_t reqNewSize ) noexcept
#define IMPL_HEAP_REDIR_METH_RESIZE_ARGS(allocatorType) IMPL_HEAP_REDIR_METH_RESIZE_ARGS_DIRECT requires ( eir::ResizeMemoryAllocator <allocatorType> )
#define IMPL_HEAP_REDIR_METH_RESIZE_RETURN bool

#define IMPL_HEAP_REDIR_METH_REALLOC_ARGS_DIRECT ( void *refMem, void *objMem, size_t reqNewSize, size_t alignment ) noexcept
#define IMPL_HEAP_REDIR_METH_REALLOC_ARGS(allocatorType) IMPL_HEAP_REDIR_METH_REALLOC_ARGS_DIRECT requires ( eir::ReallocMemoryAllocator <allocatorType> )
#define IMPL_HEAP_REDIR_METH_REALLOC_RETURN void*

#define IMPL_HEAP_REDIR_METH_FREE_ARGS ( void *refMem, void *memPtr ) noexcept
#define IMPL_HEAP_REDIR_METH_FREE_RETURN void

// Direct allocation helpers that redirect calls to another static allocator that depends on a parent struct.
#define IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_ALLOCATE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return directAllocTypeName::Allocate( hostStruct, memSize, alignment ); \
    }

#define IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_RESIZE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return directAllocTypeName::Resize( hostStruct, objMem, reqNewSize ); \
    }

#define IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_REALLOC_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return directAllocTypeName::Realloc( hostStruct, objMem, reqNewSize, alignment ); \
    }

#define IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_FREE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        directAllocTypeName::Free( hostStruct, memPtr ); \
    }

// A simple redirector for allocators.
#define IMPL_HEAP_REDIR_DIRECT_ALLOC( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN allocTypeName::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_ALLOCATE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_RESIZE_RETURN allocTypeName::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_RESIZE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_REALLOC_RETURN allocTypeName::Realloc IMPL_HEAP_REDIR_METH_REALLOC_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_REALLOC_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_FREE_RETURN allocTypeName::Free IMPL_HEAP_REDIR_METH_FREE_ARGS \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_FREE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName )

#define IMPL_HEAP_REDIR_DIRECT_ALLOC_TEMPLATEBASE( templateBaseType, tbt_templargs, tbt_templuse, allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN templateBaseType tbt_templuse::allocTypeName::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_ALLOCATE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_RESIZE_RETURN templateBaseType tbt_templuse::allocTypeName::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_RESIZE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_REALLOC_RETURN templateBaseType tbt_templuse::allocTypeName::Realloc IMPL_HEAP_REDIR_METH_REALLOC_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_REALLOC_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_FREE_RETURN templateBaseType tbt_templuse::allocTypeName::Free IMPL_HEAP_REDIR_METH_FREE_ARGS \
    IMPL_HEAP_REDIR_DIRECT_ALLOC_METH_FREE_BODY( allocTypeName, hostStructTypeName, redirNode, directAllocTypeName )

#define IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_ALLOCATE_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return directAllocTypeName::Allocate( hostStruct->refPtrNode, memSize, alignment ); \
    }

#define IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_RESIZE_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return directAllocTypeName::Resize( hostStruct->refPtrNode, objMem, reqNewSize ); \
    }

#define IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_REALLOC_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return directAllocTypeName::Realloc( hostStruct->refPtrNode, objMem, reqNewSize, alignment ); \
    }

#define IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_FREE_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        directAllocTypeName::Free( hostStruct->refPtrNode, memPtr ); \
    }

#define IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN allocTypeName::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS \
    IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_ALLOCATE_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_RESIZE_RETURN allocTypeName::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_RESIZE_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_REALLOC_RETURN allocTypeName::Realloc IMPL_HEAP_REDIR_METH_REALLOC_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_REALLOC_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_FREE_RETURN allocTypeName::Free IMPL_HEAP_REDIR_METH_FREE_ARGS \
    IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_FREE_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName )

#define IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_TEMPLATEBASE( templateBaseType, tbt_templargs, tbt_templuse, allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN templateBaseType tbt_templuse::allocTypeName::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS \
    IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_ALLOCATE_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_RESIZE_RETURN templateBaseType tbt_templuse::allocTypeName::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_RESIZE_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_REALLOC_RETURN templateBaseType tbt_templuse::allocTypeName::Realloc IMPL_HEAP_REDIR_METH_REALLOC_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_REALLOC_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_FREE_RETURN templateBaseType tbt_templuse::allocTypeName::Free IMPL_HEAP_REDIR_METH_FREE_ARGS \
    IMPL_HEAP_REDIR_DIRECTRPTR_ALLOC_METH_FREE_BODY( allocTypeName, hostStructTypeName, redirNode, refPtrNode, directAllocTypeName )

// Similar to direct allocation but redirect calls to member allocator template instead.
#define IMPL_HEAP_REDIR_DYN_ALLOC_METH_ALLOCATE_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return hostStruct->dynAllocNode.Allocate( hostStruct, memSize, alignment ); \
    }

#define IMPL_HEAP_REDIR_DYN_ALLOC_METH_RESIZE_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return hostStruct->dynAllocNode.Resize( hostStruct, objMem, reqNewSize ); \
    }

#define IMPL_HEAP_REDIR_DYN_ALLOC_METH_REALLOC_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        return hostStruct->dynAllocNode.Realloc( hostStruct, objMem, reqNewSize, alignment ); \
    }

#define IMPL_HEAP_REDIR_DYN_ALLOC_METH_FREE_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        hostStruct->dynAllocNode.Free( hostStruct, memPtr ); \
    }

#define IMPL_HEAP_REDIR_DYN_ALLOC( allocTypeName, hostStructTypeName, redirNode, dynAllocNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN allocTypeName::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS \
    IMPL_HEAP_REDIR_DYN_ALLOC_METH_ALLOCATE_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    IMPL_HEAP_REDIR_METH_RESIZE_RETURN allocTypeName::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DYN_ALLOC_METH_RESIZE_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    IMPL_HEAP_REDIR_METH_REALLOC_RETURN allocTypeName::Realloc IMPL_HEAP_REDIR_METH_REALLOC_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DYN_ALLOC_METH_REALLOC_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    IMPL_HEAP_REDIR_METH_FREE_RETURN allocTypeName::Free IMPL_HEAP_REDIR_METH_FREE_ARGS \
    IMPL_HEAP_REDIR_DYN_ALLOC_METH_FREE_BODY( hostStructTypeName, redirNode, dynAllocNode )

#define IMPL_HEAP_REDIR_DYN_ALLOC_TEMPLATEBASE( templateBaseType, tbt_templargs, tbt_templuse, allocTypeName, hostStructTypeName, redirNode, dynAllocNode, directAllocTypeName ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN templateBaseType tbt_templuse::allocTypeName::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS \
    IMPL_HEAP_REDIR_DYN_ALLOC_METH_ALLOCATE_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_RESIZE_RETURN templateBaseType tbt_templuse::allocTypeName::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DYN_ALLOC_METH_RESIZE_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_REALLOC_RETURN templateBaseType tbt_templuse::allocTypeName::Realloc IMPL_HEAP_REDIR_METH_REALLOC_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DYN_ALLOC_METH_REALLOC_BODY( hostStructTypeName, redirNode, dynAllocNode ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_FREE_RETURN templateBaseType tbt_templuse::allocTypeName::Free IMPL_HEAP_REDIR_METH_FREE_ARGS \
    IMPL_HEAP_REDIR_DYN_ALLOC_METH_FREE_BODY( hostStructTypeName, redirNode, dynAllocNode )

// Similiar to dyn-alloc but allows you to determine a separate refPtr which is on the node-way to
// the allocator itself.
#define IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_ALLOCATE_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        auto *refObj = hostStruct->refPtrNode; \
        return refObj->dynAllocRemNode.Allocate( refObj, memSize, alignment ); \
    }

#define IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_RESIZE_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        auto *refObj = hostStruct->refPtrNode; \
        return refObj->dynAllocRemNode.Resize( refObj, objMem, reqNewSize ); \
    }

#define IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_REALLOC_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        auto *refObj = hostStruct->refPtrNode; \
        return refObj->dynAllocRemNode.Realloc( refObj, objMem, reqNewSize, alignment ); \
    }

#define IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_FREE_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode ) \
    { \
        hostStructTypeName *hostStruct = LIST_GETITEM( hostStructTypeName, refMem, redirNode ); \
        auto *refObj = hostStruct->refPtrNode; \
        refObj->dynAllocRemNode.Free( refObj, memPtr ); \
    }

#define IMPL_HEAP_REDIR_DYNRPTR_ALLOC( allocTypeName, hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode, directAllocTypeName ) \
    IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN allocTypeName::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS \
    IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_ALLOCATE_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode ) \
    IMPL_HEAP_REDIR_METH_RESIZE_RETURN allocTypeName::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_RESIZE_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode ) \
    IMPL_HEAP_REDIR_METH_REALLOC_RETURN allocTypeName::Realloc IMPL_HEAP_REDIR_METH_REALLOC_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_REALLOC_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode ) \
    IMPL_HEAP_REDIR_METH_FREE_RETURN allocTypeName::Free IMPL_HEAP_REDIR_METH_FREE_ARGS \
    IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_FREE_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode )

#define IMPL_HEAP_REDIR_DYNRPTR_ALLOC_TEMPLATEBASE( templateBaseType, tbt_templargs, tbt_templuse, allocTypeName, hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode, directAllocTypeName ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN templateBaseType tbt_templuse::allocTypeName::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS \
    IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_ALLOCATE_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_RESIZE_RETURN templateBaseType tbt_templuse::allocTypeName::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_RESIZE_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_REALLOC_RETURN templateBaseType tbt_templuse::allocTypeName::Realloc IMPL_HEAP_REDIR_METH_REALLOC_ARGS(directAllocTypeName) \
    IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_REALLOC_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode ) \
    tbt_templargs \
    IMPL_HEAP_REDIR_METH_FREE_RETURN templateBaseType tbt_templuse::allocTypeName::Free IMPL_HEAP_REDIR_METH_FREE_ARGS \
    IMPL_HEAP_REDIR_DYNRPTR_ALLOC_METH_FREE_BODY( hostStructTypeName, redirNode, refPtrNode, dynAllocRemNode )

#endif //_COMMON_META_PROGRAMMING_HELPERS_
