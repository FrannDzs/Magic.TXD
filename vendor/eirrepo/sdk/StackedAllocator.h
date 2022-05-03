/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/StackedAllocator.h
*  PURPOSE:     Stacked memory allocator with forced allocation order.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRSDK_STACKED_MEM_ALLOCATOR_
#define _EIRSDK_STACKED_MEM_ALLOCATOR_

#include "eirutils.h"
#include "Allocation.h"
#include "rwlist.hpp"
#include "MemoryRaw.h"
#include "Exceptions.h"

#include <algorithm>

namespace eir
{

namespace StackedAllocatorUtils
{

// If we have no resize semantics then this makes no sense.
struct alloc_header
{
    size_t allocMetaSize;       // number of bytes for this allocation.
    size_t allocStartOff;       // offset in bytes to the start of this allocation.
    size_t prev_jump_size;      // size in bytes to jump back to previous alloc_header.
};
static_assert( std::is_trivially_destructible <alloc_header>::value == true );

// An allocation header for non-resize semantics.
struct nonresize_alloc_header
{
    nonresize_alloc_header *prev_alloc_ptr;
#ifdef _DEBUG
    size_t objMemOff;
#endif //_DEBUG
};
static_assert( std::is_trivially_destructible <alloc_header>::value == true );

// Only useful if we have resize-semantics on allocatorType.
struct block_header
{
    RwListEntry <block_header> listNode;
    alloc_header *ptrToLastAlloc;       // only set if we are not the last block.
};
static_assert( std::is_trivially_destructible <block_header>::value == true );

} // namespace StackedAllocatorUtils

// If memory allocations and their deallocations are being performed in stack-order, then
// we can use this object for efficient allocation of such spots.
template <MemoryAllocator allocatorType, typename exceptMan = DefaultExceptionManager>
struct StackedAllocator
{
    // Make templates friends of each other.
    template <MemoryAllocator, typename> friend struct StackedAllocator;

    inline StackedAllocator( void ) noexcept requires ( std::is_default_constructible <allocatorType>::value )
    {
        this->data.ptrToLastAlloc = nullptr;
    }
    inline StackedAllocator( const StackedAllocator& ) = delete;    // can never copy.

    template <MemoryAllocator otherAllocType, typename... otherAllocArgs>
        requires ( nothrow_constructible_from <allocatorType, otherAllocType&&> )
    inline StackedAllocator( StackedAllocator <otherAllocType, otherAllocArgs...>&& right ) noexcept
        : data( std::move( right.data.opt() ) )
    {
        this->data.ptrToLastAlloc = right.data.ptrToLastAlloc;
        this->data.blockList = std::move( right.data.blockList );

        right.data.ptrToLastAlloc = nullptr;
    }

    template <typename... allocArgs> requires ( constructible_from <allocatorType, allocArgs...> )
    inline StackedAllocator( eir::constr_with_alloc _, allocArgs&&... args ) : data( std::tuple(), std::tuple( std::forward <allocArgs> ( args )... ) )
    {
        this->data.ptrToLastAlloc = nullptr;
    }

    inline ~StackedAllocator( void )
    {
        if constexpr ( ResizeMemoryAllocator <allocatorType> )
        {
            // Free all remaining memory.
            while ( LIST_EMPTY( this->data.blockList.root ) == false )
            {
                block_header *blk = LIST_GETITEM( block_header, this->data.blockList.root.prev, listNode );

                LIST_REMOVE( blk->listNode );

                this->data.opt().Free( nullptr, blk );
            }
        }
        else
        {
            // Need to walk the list of allocations to release everything.
            while ( nonresize_alloc_header *alloc = (nonresize_alloc_header*)this->data.ptrToLastAlloc )
            {
                this->data.ptrToLastAlloc = alloc->prev_alloc_ptr;

                this->data.opt().Free( nullptr, alloc );
            }
        }
    }

    inline StackedAllocator& operator = ( const StackedAllocator& ) = delete;

    template <MemoryAllocator otherAllocType, typename... otherAllocArgs>
        requires ( std::is_nothrow_assignable <allocatorType, otherAllocType&&>::value )
    inline StackedAllocator& operator = ( StackedAllocator <otherAllocType, otherAllocArgs...>&& right ) noexcept
    {
        this->~StackedAllocator();

        return *new (this) StackedAllocator( std::move( right ) );
    }

    inline void* Allocate( size_t memSize, size_t alignment )
    {
        if constexpr ( ResizeMemoryAllocator <allocatorType> )
        {
            alloc_header *ptrToLastAlloc;

            // Fetch a new memory spot.
            block_header *use_block;
            void *use_header_memPtr;
            void *use_obj_memPtr;

            if ( LIST_EMPTY( this->data.blockList.root ) == false )
            {
                ptrToLastAlloc = (alloc_header*)this->data.ptrToLastAlloc;

                FATAL_ASSERT( ptrToLastAlloc != nullptr );

                use_block = LIST_GETITEM( block_header, this->data.blockList.root.prev, listNode );

                // Check if we can expand the block.
                void *newHeaderAllocPtr = (void*)ALIGN_SIZE( (size_t)ptrToLastAlloc + ptrToLastAlloc->allocMetaSize, alignof(alloc_header) );
                void *newObjAllocPtr = (void*)ALIGN_SIZE( (size_t)newHeaderAllocPtr + sizeof(alloc_header), alignment );

                size_t newDesiredBlockSize = ( (size_t)newObjAllocPtr - (size_t)use_block ) + memSize;

                if ( this->data.opt().Resize( nullptr, use_block, newDesiredBlockSize ) )
                {
                    // We can just use the block that we have now.
                    use_header_memPtr = newHeaderAllocPtr;
                    use_obj_memPtr = newObjAllocPtr;
                    goto allocationSuccess;
                }
            }

            // Just allocate a new block.
            {
                // We assume that each alignment is a power-of-two.

                size_t reqBlockAlignment = std::max( std::max( alignment, alignof(block_header) ), alignof(alloc_header) );

                size_t offsetToAllocHeader = ALIGN_SIZE( sizeof(block_header), alignof(alloc_header) );
                size_t offsetToObj = ALIGN_SIZE( offsetToAllocHeader + sizeof(alloc_header), alignment );

                size_t reqBlockSize = ( offsetToObj + memSize );

                void *use_block_memPtr = this->data.opt().Allocate( nullptr, reqBlockSize, reqBlockAlignment );

                if ( use_block_memPtr == nullptr )
                {
                    exceptMan::throw_oom( reqBlockSize );
                }

                // If we have a previous block, then save the allocation meta variables.
                if ( LIST_EMPTY( this->data.blockList.root ) == false )
                {
                    block_header *prev_block_header = LIST_GETITEM( block_header, this->data.blockList.root.prev, listNode );

                    prev_block_header->ptrToLastAlloc = (alloc_header*)this->data.ptrToLastAlloc;
                }

                use_block = new (use_block_memPtr) block_header;
                LIST_APPEND( this->data.blockList.root, use_block->listNode );

                use_header_memPtr = ( (char*)use_block + offsetToAllocHeader );
                use_obj_memPtr = ( (char*)use_block + offsetToObj );

                ptrToLastAlloc = nullptr;
            }

        allocationSuccess:
            size_t obj_memOff = (size_t)( (char*)use_obj_memPtr - (char*)use_header_memPtr );

            alloc_header *use_header = new (use_header_memPtr) alloc_header;
            use_header->allocMetaSize = obj_memOff + memSize;
            use_header->allocStartOff = obj_memOff;

            if ( ptrToLastAlloc )
            {
                use_header->prev_jump_size = (size_t)( (char*)use_header_memPtr - (char*)ptrToLastAlloc );
            }
            else
            {
                use_header->prev_jump_size = (size_t)( (char*)use_header_memPtr - (char*)use_block );
            }

            this->data.ptrToLastAlloc = use_header;

            return use_obj_memPtr;
        }
        else
        {
            // Create a new allocation, always.
            size_t reqAllocAlign = std::max( alignment, alignof(nonresize_alloc_header) );

            size_t offsetToObj = ALIGN_SIZE( sizeof(nonresize_alloc_header), alignment );

            size_t newAllocSize = offsetToObj + memSize;

            void *memPtr = this->data.opt().Allocate( nullptr, newAllocSize, reqAllocAlign );

            if ( memPtr == nullptr )
            {
                exceptMan::throw_oom( newAllocSize );
            }

            void *use_header_memPtr = memPtr;
            void *use_obj_memPtr = ( (char*)memPtr + offsetToObj );

            nonresize_alloc_header *header = new (use_header_memPtr) nonresize_alloc_header;
            header->prev_alloc_ptr = (nonresize_alloc_header*)this->data.ptrToLastAlloc;
#ifdef _DEBUG
            header->objMemOff = offsetToObj;
#endif //_DEBUG

            // Remember that we have an allocation.
            this->data.ptrToLastAlloc = header;

            return use_obj_memPtr;
        }
    }

    inline void Deallocate( void *memPtr ) noexcept
    {
        if constexpr ( ResizeMemoryAllocator <allocatorType> )
        {
            alloc_header *last_alloc = (alloc_header*)this->data.ptrToLastAlloc;

            FATAL_ASSERT( last_alloc != nullptr );

#ifdef _DEBUG
            // Check that the given pointer does match our last allocation.
            FATAL_ASSERT( (char*)last_alloc + last_alloc->allocStartOff == memPtr );
#endif //_DEBUG

            // If we are the last allocation on the block, then we kill the block and return to previous.
            // Otherwise we just shrink the block.
            block_header *cur_block = LIST_GETITEM( block_header, this->data.blockList.root.prev, listNode );

            void *prev_ptr_alloc = ( (char*)last_alloc - last_alloc->prev_jump_size );

            bool is_last_alloc_on_block = ( prev_ptr_alloc == cur_block );

            if ( is_last_alloc_on_block )
            {
                // Destroy the block.
                LIST_REMOVE( cur_block->listNode );

                // Establish the previous block as current, if available.
                if ( LIST_EMPTY( this->data.blockList.root ) == false )
                {
                    block_header *cur_block = LIST_GETITEM( block_header, this->data.blockList.root.prev, listNode );

                    this->data.ptrToLastAlloc = cur_block->ptrToLastAlloc;
                }
                else
                {
                    this->data.ptrToLastAlloc = nullptr;
                }

                // Release the memory.
                this->data.opt().Free( nullptr, cur_block );
            }
            else
            {
                alloc_header *new_last_alloc = (alloc_header*)prev_ptr_alloc;

                // Shrink block.
                size_t newReqBlockSize = ( ( (size_t)new_last_alloc + new_last_alloc->allocMetaSize ) - (size_t)cur_block );

                // Shrinking should never fail.
                this->data.opt().Resize( nullptr, cur_block, newReqBlockSize );

                this->data.ptrToLastAlloc = prev_ptr_alloc;
            }
        }
        else
        {
            nonresize_alloc_header *header = (nonresize_alloc_header*)this->data.ptrToLastAlloc;

#ifdef _DEBUG
            FATAL_ASSERT( ( (char*)header + header->objMemOff ) == memPtr );
#endif //_DEBUG

            this->data.ptrToLastAlloc = header->prev_alloc_ptr;

            // Release the memory.
            this->data.opt().Free( nullptr, header );
        }
    }

    inline bool HasAllocations( void ) const noexcept
    {
        return ( this->data.ptrToLastAlloc != nullptr );
    }

    inline size_t GetBlockCount( void ) const noexcept
    {
        size_t blkcnt = 0;

        LIST_FOREACH_BEGIN( block_header, this->data.blockList.root, listNode )
            blkcnt++;
        LIST_FOREACH_END

        return blkcnt;
    }

    // Helper function
    template <typename structType>
    inline void* AllocateFor( void )
    {
        return this->Allocate( sizeof(structType), alignof(structType) );
    }

private:
    typedef StackedAllocatorUtils::block_header block_header;
    typedef StackedAllocatorUtils::alloc_header alloc_header;
    typedef StackedAllocatorUtils::nonresize_alloc_header nonresize_alloc_header;

    struct fields
    {
        void *ptrToLastAlloc;
        RwList <block_header> blockList;
    };

    empty_opt <allocatorType, fields> data;
};

} // namespace eir

#endif //_EIRSDK_STACKED_MEM_ALLOCATOR_