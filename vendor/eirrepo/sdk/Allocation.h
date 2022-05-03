/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Allocation.h
*  PURPOSE:     Memory management directives and helpers.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRSDK_ALLOCATION_HEADER_
#define _EIRSDK_ALLOCATION_HEADER_

#include <utility>  // for std::forward

#include "MacroUtils.h"
#include "DataUtil.h"

namespace eir
{

// Friendship resistant replacement for std::constructible_from (it is buggy as shit, woof meow).
template <typename T, typename... cArgTypes>
concept constructible_from =
    requires ( cArgTypes&&... cArgs ) { new (nullptr) T( std::forward <cArgTypes> ( cArgs )... ); };

template <typename T, typename... cArgTypes>
concept nothrow_constructible_from =
    requires ( cArgTypes&&... cArgs ) { { new (nullptr) T( std::forward <cArgTypes> ( cArgs )... ) } noexcept; };

template <typename T>
concept trivially_moveable =
    std::is_trivially_move_constructible <T>::value &&
    std::is_trivially_move_assignable <T>::value;

// A very special pass-in just for creation with the allocator initialization.
enum class constr_with_alloc
{
    DEFAULT
};

// Construction with allocator-copy (sometimes required).
enum class constr_with_alloc_copy
{
    DEFAULT
};

namespace AllocatorMeta
{
    
template <typename methodType, typename... objArgs>
concept IsAllocationFunction =
    // (static) void* Allocate( void *refPtr, size_t memSize, size_t alignment ) noexcept;
    std::is_nothrow_invocable_r <void*, methodType, objArgs..., void*, size_t, size_t>::value;

template <typename methodType, typename... objArgs>
concept IsResizeFunction =
    // (static) bool Resize( void *refPtr, void *memptr, size_t newSize ) noexcept;
    std::is_nothrow_invocable_r <bool, methodType, objArgs..., void*, void*, size_t>::value;

template <typename methodType, typename... objArgs>
concept IsReallocFunction =
    // (static) void* Realloc( void *refPtr, void *memptr, size_t newSize, size_t alignment ) noexcept;
    std::is_nothrow_invocable_r <void*, methodType, objArgs..., void*, void*, size_t, size_t>::value;

template <typename methodType, typename... objArgs>
concept IsFreeFunction =
    // (static) void Free( void *refPtr, void *memPtr ) noexcept;
    std::is_nothrow_invocable_r <void, methodType, objArgs..., void*, void*>::value;

} // namespace AllocatorMeta

template <typename structType>
concept ObjectMemoryAllocator =
    AllocatorMeta::IsAllocationFunction <decltype(&structType::Allocate), structType&> &&
    AllocatorMeta::IsFreeFunction <decltype(&structType::Free), structType&>;

template <typename structType>
concept StaticMemoryAllocator =
    std::is_empty <structType>::value &&
    AllocatorMeta::IsAllocationFunction <decltype(structType::Allocate)> &&
    AllocatorMeta::IsFreeFunction <decltype(structType::Free)>;

template <typename structType>
concept MemoryAllocator = ObjectMemoryAllocator <structType> || StaticMemoryAllocator <structType>;

// *** RESIZE SEMANTICS ***
// In case of both realloc and resize semantics, resize should be preferred.
template <typename structType>
concept ResizeObjectMemoryAllocator =
    ObjectMemoryAllocator <structType> &&
    AllocatorMeta::IsResizeFunction <decltype(&structType::Resize), structType&>;

template <typename structType>
concept ResizeStaticMemoryAllocator =
    StaticMemoryAllocator <structType> &&
    AllocatorMeta::IsResizeFunction <decltype(structType::Resize)>;

template <typename structType>
concept ResizeMemoryAllocator =
    ResizeObjectMemoryAllocator <structType> || ResizeStaticMemoryAllocator <structType>;

// *** REALLOC SEMANTICS ***
template <typename structType>
concept ReallocObjectMemoryAllocator =
    ObjectMemoryAllocator <structType> &&
    AllocatorMeta::IsReallocFunction <decltype(&structType::Realloc), structType&>;

template <typename structType>
concept ReallocStaticMemoryAllocator =
    StaticMemoryAllocator <structType> &&
    AllocatorMeta::IsReallocFunction <decltype(structType::Realloc)>;

template <typename structType>
concept ReallocMemoryAllocator =
    ReallocObjectMemoryAllocator <structType> || ReallocStaticMemoryAllocator <structType>;

// Constructs an object with the help of a static memory allocator.
template <typename structType, StaticMemoryAllocator allocatorType, typename exceptMan = DefaultExceptionManager, typename... Args>
    requires constructible_from <structType, Args...>
inline structType* static_new_struct( void *refMem, Args&&... theArgs )
{
    // Attempt to allocate a block of memory for bootstrapping.
    void *mem = allocatorType::Allocate( refMem, sizeof( structType ), alignof( structType ) );

    if ( !mem )
    {
        exceptMan::throw_oom( sizeof( structType ) );
    }

    try
    {
        return new (mem) structType( std::forward <Args> ( theArgs )... );
    }
    catch( ... )
    {
        allocatorType::Free( refMem, mem );

        throw;
    }
}

// Destroys an object that was previously allocated using a specific static memory allocator.
template <typename structType, StaticMemoryAllocator allocatorType>
inline void static_del_struct( void *refMem, structType *theStruct ) noexcept(std::is_nothrow_destructible <structType>::value)
{
    theStruct->~structType();

    void *mem = theStruct;

    allocatorType::Free( refMem, mem );
}

// Constructs an object using an object memory allocator.
template <typename structType, typename exceptMan = DefaultExceptionManager, MemoryAllocator allocatorType, typename... Args>
    //requires constructible_from <structType, Args...>
// There are multiple issues of why I cannot use a requires-clause for this function template:
// 1) there is no way to directly befriend a function template without prior declaration as friend.
// 2) I tried and failed to write a befriending of a function template that comes with a requires-clause.
// Until the C++ standards or compilers team fixes it, I will just leave it out... Would be nice to
// have though.
inline structType* dyn_new_struct( allocatorType& allocMan, void *refMem, Args&&... theArgs )
{
    // Attempt to allocate a block of memory for bootstrapping.
    void *mem = allocMan.Allocate( refMem, sizeof( structType ), alignof( structType ) );

    if ( !mem )
    {
        exceptMan::throw_oom( sizeof( structType ) );
    }

    try
    {
        return new (mem) structType( std::forward <Args> ( theArgs )... );
    }
    catch( ... )
    {
        allocMan.Free( refMem, mem );

        throw;
    }
}

// Destroys an object that was allocated by an object memory allocator.
template <typename structType, MemoryAllocator allocatorType>
inline void dyn_del_struct( allocatorType& memAlloc, void *refMem, structType *theStruct ) noexcept(std::is_nothrow_destructible <structType>::value)
{
    theStruct->~structType();
    // Assumingly there is no way to call placement-delete manually so IDGAF about it.
    // If new C++ standards change it, then I will adhere to the new rules.
    // But the problem is that the placement-new does call placement-delete so why
    // should the developer not be able to call it aswell to close the circle?

    void *mem = theStruct;

    memAlloc.Free( refMem, mem );
}

// Repurposes object memory into a different object if possible. In the best case scenario there is no memory
// allocation manipulation being performed but the allocation is retaken for the new object lifetime. In the
// worst case an entirely new allocation is performed and returned. The old object is deleted, if the old
// memory is not reused then it is free'd aswell.
// * Only use this function if you definitely do not need the old struct anymore. You could use this function
// in the move-or-copy-assignment operators when the old data is not required anymore, to be replaced by
// a new struct type (essentially a seldom-used optimization).
// * The oldObj ptr can be nullptr. Then this function does always request a new object allocation.
// * The oldObj struct has to reside on memory allocated by memAlloc, if not nullptr.
// * This function can throw an out-of-memory exception if a new object allocation has to be performed. In
//   case of this exception the oldObj was released anyway.
template <typename newStructType, typename exceptMan = DefaultExceptionManager, typename oldStructType, MemoryAllocator allocatorType, typename... constrArgs>
inline newStructType* repurpose_memory( allocatorType& memAlloc, void *refMem, oldStructType *oldObj, constrArgs&&... cargs )
{
    if ( oldObj != nullptr )
    {
        // We will check whether we can reuse the memory.
        // But deleting the old object is definitely required.
        oldObj->~oldStructType();

        void *oldObj_anon = (void*)oldObj;

        size_t newStruct_align = alignof(newStructType);
        size_t oldStruct_align = alignof(oldStructType);

        size_t newStruct_size = sizeof(newStructType);
        size_t oldStruct_size = sizeof(oldStructType);

        // We assume that structs cannot have a size of zero.

        // Since the alignof function is always guaranteed to return a power-of-two value, the check for contained-as-factor can
        // be reduced to a less-than-or-equal-to check.
        // Check whether we are allowed to reuse the same memory pointer for the new object.
        if ( newStruct_align <= oldStruct_align )
        {
            try
            {
                // Check whether the new object can fit into the old space.
                // We want a tight fit in this algorithm.
                bool new_does_fit = ( newStruct_size == oldStruct_size );

                if constexpr ( ResizeMemoryAllocator <allocatorType> )
                {
                    if ( new_does_fit == false )
                    {
                        new_does_fit = memAlloc.Resize( refMem, oldObj_anon, newStruct_size );
                    }
                }
                else if constexpr ( ReallocMemoryAllocator <allocatorType> )
                {
                    if ( new_does_fit == false )
                    {
                        void *realloc_result = memAlloc.Realloc( refMem, oldObj_anon, newStruct_size, newStruct_align );

                        if ( realloc_result == nullptr )
                        {
                            exceptMan::throw_oom( newStruct_size );
                        }

                        oldObj_anon = realloc_result;
                        new_does_fit = true;
                    }
                }

                if ( new_does_fit )
                {
                    return new (oldObj_anon) newStructType( std::forward <constrArgs> ( cargs )... );
                }
            }
            catch( ... )
            {
                // Release the old memory anyway.
                // We will not try to allocate the new object anymore.
                memAlloc.Free( refMem, oldObj_anon );
                throw;
            }
        }

        // Just release the old memory because we could not use it anymore.
        memAlloc.Free( refMem, oldObj_anon );
    }

    // No special logic could be performed. Thus we default to the standard way.
    return dyn_new_struct <newStructType, exceptMan> ( memAlloc, refMem, std::forward <constrArgs> ( cargs )... );
}

} // namespace eir

// *** NOT IN THE EIR NAMESPACE BEGIN

// The basic allocator that links to the CRT.
// I was heavy against exposing this but I cannot overcome the static-initialization order in current C++17.
// This allocator does not implement the Resize function, and on some platforms it might implement the
// Realloc function instead.
struct CRTHeapAllocator
{
    static AINLINE void* Allocate( void *refPtr, size_t memSize, size_t alignment ) noexcept
    {
#ifdef _MSC_VER
        return _aligned_malloc( memSize, alignment );
#else
        // The use of aligned_alloc is braindead because it actually requires the size to be
        // an integral multiple of alignment. So you can understand why Microsoft does not want
        // it in their CRT (it has since been forced by the C standard).
        return memalign( alignment, memSize );
#endif //_MSC_VER
    }

    //static AINLINE bool Resize( void *refPtr, void *memPtr, size_t newSize ) noexcept;
    // I cannot believe that Microsoft has not yet added "_aligned_expand".

#ifdef _MSC_VER
    // The definition of realloc is purposefully rustical due to the rigidness of the CRT
    // memory interface; the recommended way to handle memory is using resize-semantics
    // anyway. We provide this functionality to ease transition through experienced-
    // convincement.
    // On Linux there is no realloc_aligned so F it. They can live with slower
    // CRT memory handling. They have to switch to the Eir SDK native memory allocation
    // to get the benefits which ultimatively is fine.
    static AINLINE void* Realloc( void *refPtr, void *memPtr, size_t newSize, size_t alignment ) noexcept
    {
        return _aligned_realloc( memPtr, newSize, alignment );
    }
#endif //_MSC_VER

    static AINLINE void Free( void *refPtr, void *memPtr ) noexcept
    {
#ifdef _MSC_VER
        _aligned_free( memPtr );
#else
        free( memPtr );
#endif //_MSC_VER
    }
};

// Not really realloc() but very similar.
template <eir::MemoryAllocator allocatorType>
AINLINE void* TemplateMemTransalloc(
    allocatorType& memAlloc, void *refPtr,
    void *memPtr, size_t oldSize, size_t newSize,
    size_t alignment = sizeof(std::max_align_t)
)
{
    if constexpr ( eir::ReallocMemoryAllocator <allocatorType> )
    {
        return memAlloc.Realloc( refPtr, memPtr, newSize, alignment );
    }
    else
    {
        if ( newSize == 0 )
        {
            if ( memPtr )
            {
                memAlloc.Free( refPtr, memPtr );
            }

            return nullptr;
        }

        if ( memPtr == nullptr )
        {
            return memAlloc.Allocate( refPtr, newSize, alignment );
        }

        if constexpr ( eir::ResizeMemoryAllocator <allocatorType> )
        {
            bool couldResize = memAlloc.Resize( refPtr, memPtr, newSize );

            if ( couldResize )
            {
                return memPtr;
            }
        }

        void *newMemPtr = memAlloc.Allocate( refPtr, newSize, alignment );

        if ( newMemPtr == nullptr )
        {
            return nullptr;
        }

        FSDataUtil::copy_impl( (const char*)memPtr, (const char*)memPtr + oldSize, (char*)newMemPtr );

        memAlloc.Free( refPtr, memPtr );

        return newMemPtr;
    }
}

// *** NOT IN THE EIR NAMESPACE END

#endif //_EIRSDK_ALLOCATION_HEADER_