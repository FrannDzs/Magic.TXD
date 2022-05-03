/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/OSUtils.memheap.h
*  PURPOSE:     Virtual-memory-based memory heap
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _NATIVE_VIRTUAL_MEMORY_HEAP_ALLOCATOR_
#define _NATIVE_VIRTUAL_MEMORY_HEAP_ALLOCATOR_

#include "OSUtils.h"
#include "MacroUtils.h"
#include "rwlist.hpp"
#include "AVLTree.h"

// For std::max_align_t.
#include <cstddef>

// Heap allocator class that provides sized memory chunks from OS-provided virtual memory.
// Version 3.
// * using AVL trees in free-bytes lists to optimize allocation performance
// * merging allocations together for optimization
// * one AVLTree for all island-free-regions for near-logarithmic performance
#ifdef __linux__
template <typename syscallDispatcherType>
#endif //__linux__
struct PlatformHeapAllocator
{
    // Allocations are made on virtual memory islands that bundle many together. Each vmem island
    // has a list of allocations residing on it. It can potentially grow infinitely but if it
    // cannot then another island is created. Each island dies if there are no more allocations
    // on it.
    // Advantage of using this class is that you have FULL CONTROL over memory allocation. You
    // can even design the features that your memory allocator should have ;)

    static constexpr size_t MIN_PAGES_FOR_ISLAND = 4;       // minimum amount of pages to reserve for an island.
    static constexpr size_t MAX_ISLAND_BURST_COUNT = 3;     // number of islands that will be grown-first.
    static constexpr size_t DEFAULT_ALIGNMENT = sizeof(std::max_align_t);

private:
    typedef PlatformPageAllocator
#ifdef __linux__
        <syscallDispatcherType>
#endif //__linux__
        PageAllocatorType;

public:
    inline PlatformHeapAllocator( void ) noexcept
    {
        return;
    }

    inline PlatformHeapAllocator( const PlatformHeapAllocator& right ) = delete;
    inline PlatformHeapAllocator( PlatformHeapAllocator&& right ) noexcept
    {
        // Move fields over, simply.
        this->nativeMemProv = std::move( right.nativeMemProv );
        this->avlFreeBlockSortedBySize = std::move( right.avlFreeBlockSortedBySize );
        this->listIslands = std::move( right.listIslands );
        this->sortedByAddrIslands = std::move( right.sortedByAddrIslands );

        // The items on the right have automatically been cleared.
    }

    inline ~PlatformHeapAllocator( void )
    {
        // Release all memory.
        LIST_FOREACH_BEGIN( VMemIsland, this->listIslands.root, managerNode )

            typename PageAllocatorType::pageHandle *islandHandle = item->allocHandle;

            item->~VMemIsland();

            this->nativeMemProv.Free( islandHandle );

        LIST_FOREACH_END
    }

    // Assignments.
    inline PlatformHeapAllocator& operator = ( const PlatformHeapAllocator& right ) = delete;
    inline PlatformHeapAllocator& operator = ( PlatformHeapAllocator&& right ) noexcept
    {
        this->~PlatformHeapAllocator();

        return *new (this) PlatformHeapAllocator( std::move( right ) );
    }

private:
    typedef sliceOfData <size_t> memBlockSlice_t;

    // YOU MUST NOT USE assert IN THIS CODE.
    // Instead use FATAL_ASSERT.

    // *** Important helpers.

    // Returns true if the given free region is not available for allocation anymore.
    // Then it can be safely faded out of activity.
    static AINLINE bool is_free_region_empty( const memBlockSlice_t& freeRegion ) noexcept
    {
        // A good definition of empty is if the slice is actually empty.
        // But an even better definition is if the slice does not even fit the VMemAllocation header struct
        // as well as at least one byte.

        size_t byteCount = freeRegion.GetSliceSize();

        return ( byteCount < ( sizeof(VMemAllocation) + 1 ) );
    }

    // Forward declaration for the methods here.
    struct VMemAllocation;
    struct VMemIsland;

    enum class eIslandMergeResult
    {
        NO_MERGE,
        MERGE_TO_FIRST,
        MERGE_TO_SECOND
    };

    AINLINE eIslandMergeResult MergeIslands( VMemIsland *firstIsland, VMemIsland *secondIsland ) noexcept
    {
        // Nothing to do if same.
        if ( firstIsland == secondIsland )
        {
            return eIslandMergeResult::NO_MERGE;
        }

        typename PageAllocatorType::pageHandle *firstIslandHandle = firstIsland->allocHandle;
        typename PageAllocatorType::pageHandle *secondIslandHandle = secondIsland->allocHandle;

        // Depends on the order of islands.
        bool isFirstIslandPriorToSecond = ( firstIsland < secondIsland );

        if ( isFirstIslandPriorToSecond )
        {
            // First merge the handles.
            bool couldMergeRegions = this->nativeMemProv.MergePageHandles( firstIslandHandle, secondIslandHandle );

            if ( !couldMergeRegions )
            {
                return eIslandMergeResult::NO_MERGE;
            }

            firstIsland->move_append_island_references( this, secondIsland );

            // Get rid of the reference of the second island.
            this->_RemoveEmptyHeapIsland( secondIsland, false );

            return eIslandMergeResult::MERGE_TO_FIRST;
        }
        else
        {
            // First merge the handles.
            bool couldMergeRegions = this->nativeMemProv.MergePageHandles( secondIslandHandle, firstIslandHandle );

            if ( !couldMergeRegions )
            {
                return eIslandMergeResult::NO_MERGE;
            }

            // Logically we place the island on the info-struct of secondIsland.

            secondIsland->move_append_island_references( this, firstIsland );

            // Get rid of the reference to the first island.
            this->_RemoveEmptyHeapIsland( firstIsland, false );

            return eIslandMergeResult::MERGE_TO_SECOND;
        }
    }

    // Each memory island can maybe allocate new data.
    // If an island cannot allocate anymore, maybe it can later, but we
    // (almost) always can create another island!
    inline VMemAllocation* AllocateOnFreeSpace( size_t dataSize, size_t alignedBy ) noexcept
    {
        // It only makes sense to pick alignedBy as power-of-two value.
        // But we allow other values too I guess? Maybe we will do unit tests for those aswell to stress test things?

        void *allocPtr;
        memBlockSlice_t allocSlice;
        VMemFreeBlock *freeBlockToAllocateInto;

        typename VMemIsland::alignedObjSizeByOffset posDispatch( dataSize, alignedBy );

        // Try to find a spot between existing data.
        {
            // Scan for the free block whose size is equal or just above the data size with the meta-data block.
            // This is the best-estimate beginning of the allocatable free regions.
            // The scan is logarithmic time so a really great idea.
            AVLNode *firstAllocatable = this->avlFreeBlockSortedBySize.GetJustAboveOrEqualNode( dataSize + sizeof(VMemAllocation) );

            typename VMemFreeBlockAVLTree::diff_node_iterator iter( firstAllocatable );

            while ( !iter.IsEnd() )
            {
                // We have to check each member of the nodestack of the current best-fit node because allocation could
                // fail due to misalignment. But since we have the best-fit node going for good alignment usage
                // is something the user wants: do not worry!

                typename VMemFreeBlockAVLTree::nodestack_iterator nodestack_iter( iter.Resolve() );

                while ( !nodestack_iter.IsEnd() )
                {
                    VMemFreeBlock *smallFreeBlock = AVL_GETITEM( VMemFreeBlock, nodestack_iter.Resolve(), sortedBySizeNode );

                    // Try to allocate into it.
                    // It succeeds if the allocation does fit into the free region.
                    size_t reqSize;
                    size_t allocOff = smallFreeBlock->freeRegion.GetSliceStartPoint();

                    posDispatch.ScanNextBlock( allocOff, reqSize );

                    memBlockSlice_t requiredMemRegion( allocOff, reqSize );

                    eir::eIntersectionResult intResult = requiredMemRegion.intersectWith( smallFreeBlock->freeRegion );

                    if ( intResult == eir::INTERSECT_INSIDE ||
                         intResult == eir::INTERSECT_EQUAL )
                    {
                        // We found a valid allocation slot!
                        // So return it.
                        allocPtr = (void*)allocOff;
                        allocSlice = requiredMemRegion;
                        freeBlockToAllocateInto = smallFreeBlock;
                        goto foundAllocationSpot;
                    }

                    // Try the next same-size freeblock.
                    nodestack_iter.Increment();
                }

                // Try the next bigger block.
                iter.Increment();
            }
        }

        // Could not allocate on this island, at least.
        // Maybe try another island or a new one.
        return nullptr;

    foundAllocationSpot:
        // Since allocation succeeded we can fetch meta-data from the position dispatcher.
        size_t allocDataOff = posDispatch.allocDataOff;
        VMemIsland *theIsland = freeBlockToAllocateInto->manager;

        return theIsland->InplaceNewAllocation( this, freeBlockToAllocateInto, allocPtr, allocDataOff, dataSize, std::move( allocSlice ) );
    }

    // After calling this function theIsland may turn invalid. So fetch the island from the returned allocation, if required.
    AINLINE VMemAllocation* AllocateOnIslandWithExpand( VMemIsland *theIsland, size_t dataSize, size_t alignedBy ) noexcept
    {
        // It only makes sense to pick alignedBy as power-of-two value.
        // But we allow other values too I guess? Maybe we will do unit tests for those aswell to stress test things?

        void *allocPtr;
        memBlockSlice_t allocSlice;
        VMemFreeBlock *freeBlockToAllocateInto;

        typename VMemIsland::alignedObjSizeByOffset posDispatch( dataSize, alignedBy );

        // Try to make space for a new thing by growing the validity region to the right.
        {
            // We have to calculate the end offset of the data that we need.
            // The next position to allocate at is after all valid data.
            VMemFreeBlock *lastFreeBlock = theIsland->GetLastFreeBlock();
            size_t tryNewMemOffset = lastFreeBlock->freeRegion.GetSliceStartPoint();
            size_t realAllocSize;

            // Calculate the required beginning of the biggest allocation slot possibly available.
            posDispatch.ScanNextBlock( tryNewMemOffset, realAllocSize );

            size_t finalMemEndOffset = ( tryNewMemOffset + realAllocSize );

            if ( theIsland->MakeFreeSpaceToTheRight( this, lastFreeBlock, finalMemEndOffset ) )
            {
                // Just return the new spot.
                // We will insert to the end node.
                allocPtr = (void*)tryNewMemOffset;
                allocSlice = memBlockSlice_t( tryNewMemOffset, realAllocSize );
                freeBlockToAllocateInto = lastFreeBlock;
                goto foundAllocationSpot;
            }
        }

        // Try growing this island to the left.
        {
            VMemFreeBlock *firstFreeBlock = &theIsland->firstFreeSpaceBlock;
            size_t firstUsedByteOff = ( firstFreeBlock->freeRegion.GetSliceEndPoint() + 1 );
            size_t tryNewMemOffset = firstUsedByteOff;
            size_t realAllocSize;

            posDispatch.ScanPrevBlock( tryNewMemOffset, realAllocSize );

            // So the first byte we need now is located at tryNewMemOffset.
            VMemIsland *prevIsland = theIsland->get_prev_island();

            bool is_alloc_obstructed_by_prev_island = false;

            if ( prevIsland )
            {
                VMemFreeBlock *prevIslandLastFreeBlock = prevIsland->GetLastFreeBlock();

                size_t prevIslandLastAllocEndMemOffset = prevIslandLastFreeBlock->freeRegion.GetSliceStartPoint();

                is_alloc_obstructed_by_prev_island = ( prevIslandLastAllocEndMemOffset > tryNewMemOffset );
            }

            if ( is_alloc_obstructed_by_prev_island == false )
            {
                // If no other method succeeds then we have to grow the island.
                bool needToGrowIsland = true;

                VMemFreeBlock *insertionFreeBlock = firstFreeBlock;
                size_t vmemIslandEndOff = ( theIsland->allocHandle->GetTargetSlice().GetSliceEndPoint() + 1 );

                if ( prevIsland )
                {
                    size_t vmemIslandFirstByteOff = SCALE_DOWN( tryNewMemOffset, this->nativeMemProv.GetPageSize() );

                    memBlockSlice_t newIslandSlice = memBlockSlice_t::fromOffsets( vmemIslandFirstByteOff, vmemIslandEndOff );

                    eir::eCompResult cmpRes = eir::MathSliceHelpers::CompareSlicesByIntersection( newIslandSlice, prevIsland->allocHandle->GetTargetSlice(), true );

                    if ( cmpRes == eir::eCompResult::EQUAL )
                    {
                        // We must merge the islands here.
                        insertionFreeBlock = prevIsland->GetLastFreeBlock();

                        eIslandMergeResult mergeRes = this->MergeIslands( theIsland, prevIsland );

                        if ( mergeRes == eIslandMergeResult::NO_MERGE )
                        {
                            goto try_any_other_way;
                        }

                        FATAL_ASSERT( mergeRes == eIslandMergeResult::MERGE_TO_SECOND );

                        // Just work on the island that we merged to.
                        theIsland = prevIsland;

                        needToGrowIsland = false;
                    }
                }

                if ( needToGrowIsland )
                {
                    size_t vmemIslandStartOff = SCALE_DOWN( tryNewMemOffset - sizeof(VMemIsland), this->nativeMemProv.GetPageSize() );

                    size_t newReqSize = ( vmemIslandEndOff - vmemIslandStartOff );

                    VMemIsland *newIslandPtr = theIsland->grow_validity_region_to_left( this, newReqSize );

                    if ( newIslandPtr == nullptr )
                    {
                        goto try_any_other_way;
                    }

                    theIsland = newIslandPtr;
                    insertionFreeBlock = &theIsland->firstFreeSpaceBlock;
                }

                // Just return the new spot.
                allocPtr = (void*)tryNewMemOffset;
                allocSlice = memBlockSlice_t( tryNewMemOffset, realAllocSize );
                freeBlockToAllocateInto = insertionFreeBlock;
                goto foundAllocationSpot;
            }
        }

    try_any_other_way:
        // We failed.
        return nullptr;

    foundAllocationSpot:
        size_t allocDataOff = posDispatch.allocDataOff;

        return theIsland->InplaceNewAllocation( this, freeBlockToAllocateInto, allocPtr, allocDataOff, dataSize, std::move( allocSlice ) );
    }

    inline VMemIsland* ActivateMemoryRegionByHandle( typename PageAllocatorType::pageHandle *newPageHandle ) noexcept
    {
        // Handle the spurious nature of virtual memory allocation: a random island is allowed to be returned next
        // to any of our islands; then we should handle it as growth of the neighbor island and attempt allocation
        // inside of it.
        memBlockSlice_t pageHandleMemRegion = newPageHandle->GetTargetSlice();

        // Try to find an island to the left.
        AVLNode *avlNodeLeftIsland = this->sortedByAddrIslands.FindAnyNodeByCriteria(
            [&]( const AVLNode *leftNode ) noexcept
        {
            const VMemIsland *leftIsland = AVL_GETITEM( VMemIsland, leftNode, avlSortedByAddrIslandsNode );

            const memBlockSlice_t& leftIslandSlice = leftIsland->allocHandle->GetTargetSlice();

            eir::eIntersectionResult intRes = leftIslandSlice.intersectWith( pageHandleMemRegion );

            if ( intRes == eir::eIntersectionResult::INTERSECT_FLOATING_END )
            {
                return eir::eCompResult::LEFT_GREATER;
            }

#ifdef _DEBUG
            FATAL_ASSERT( intRes == eir::eIntersectionResult::INTERSECT_FLOATING_START );
#endif //_DEBUG

            if ( intRes == eir::eIntersectionResult::INTERSECT_FLOATING_START && eir::AreBoundsTight( leftIslandSlice.GetSliceEndBound(), pageHandleMemRegion.GetSliceStartBound() ) )
            {
                return eir::eCompResult::EQUAL;
            }

            return eir::eCompResult::LEFT_LESS;
        });

        // Try to find an island to the right.
        AVLNode *avlNodeRightIsland = this->sortedByAddrIslands.FindAnyNodeByCriteria(
            [&]( const AVLNode *leftNode ) noexcept
        {
            const VMemIsland *leftCompareIsland = AVL_GETITEM( VMemIsland, leftNode, avlSortedByAddrIslandsNode );

            const memBlockSlice_t& leftCompareIslandSlice = leftCompareIsland->allocHandle->GetTargetSlice();

            eir::eIntersectionResult intRes = leftCompareIslandSlice.intersectWith( pageHandleMemRegion );

            if ( intRes == eir::eIntersectionResult::INTERSECT_FLOATING_START )
            {
                return eir::eCompResult::LEFT_LESS;
            }

#ifdef _DEBUG
            FATAL_ASSERT( intRes == eir::eIntersectionResult::INTERSECT_FLOATING_END );
#endif //_DEBUG

            if ( intRes == eir::eIntersectionResult::INTERSECT_FLOATING_END && eir::AreBoundsTight( pageHandleMemRegion.GetSliceEndBound(), leftCompareIslandSlice.GetSliceStartBound() ) )
            {
                return eir::eCompResult::EQUAL;
            }

            return eir::eCompResult::LEFT_GREATER;
        });

        VMemIsland *mergingResultIsland = nullptr;

        // Detect the case if we allocated in between two islands.
        if ( avlNodeLeftIsland != nullptr && avlNodeRightIsland != nullptr )
        {
            // We just merge the three page handles together!
            VMemIsland *leftBorderIsland = AVL_GETITEM( VMemIsland, avlNodeLeftIsland, avlSortedByAddrIslandsNode );
            VMemIsland *rightBorderIsland = AVL_GETITEM( VMemIsland, avlNodeRightIsland, avlSortedByAddrIslandsNode );

            typename PageAllocatorType::pageHandle *accumHandle = leftBorderIsland->allocHandle;

            bool couldMergeLeft = this->nativeMemProv.MergePageHandles( accumHandle, newPageHandle );

            FATAL_ASSERT( couldMergeLeft == true );

            bool couldMergeRight = this->nativeMemProv.MergePageHandles( accumHandle, rightBorderIsland->allocHandle );

            FATAL_ASSERT( couldMergeRight == true );

            // Transfer the island references (free space region, memory allocations, etc).
            leftBorderIsland->move_append_island_references( this, rightBorderIsland );

            // Now that the right island is empty we can just remove it.
            this->_RemoveEmptyHeapIsland( rightBorderIsland, false );

            mergingResultIsland = leftBorderIsland;
        }
        // First we merge the island to the left.
        else if ( avlNodeLeftIsland != nullptr )
        {
            // We need to merge the left island with the current page handle allocation.
            // Basically we merge stuff to the handle of the left.
            VMemIsland *leftBorderIsland = AVL_GETITEM( VMemIsland, avlNodeLeftIsland, avlSortedByAddrIslandsNode );

            bool couldMerge = this->nativeMemProv.MergePageHandles( leftBorderIsland->allocHandle, newPageHandle );

            FATAL_ASSERT( couldMerge == true );

            // Update the last free region of our island.
            VMemFreeBlock *lastFreeBlock = leftBorderIsland->GetLastFreeBlock();

            if ( is_free_region_empty( lastFreeBlock->freeRegion ) == false )
            {
                this->avlFreeBlockSortedBySize.RemoveByNodeFast( &lastFreeBlock->sortedBySizeNode );
            }

            lastFreeBlock->freeRegion.SetSliceEndBound( pageHandleMemRegion.GetSliceEndBound() );

            if ( is_free_region_empty( lastFreeBlock->freeRegion ) == false )
            {
                this->avlFreeBlockSortedBySize.Insert( &lastFreeBlock->sortedBySizeNode );
            }

            // Now we have an island that our allocation is part of.
            // Thus the next merge should happen based on this island.
            // If we do no other merge after this then we just allocate on this island.
            mergingResultIsland = leftBorderIsland;
        }
        // Then we merge the island to the right.
        else if ( avlNodeRightIsland != nullptr )
        {
            VMemIsland *rightBorderIsland = AVL_GETITEM( VMemIsland, avlNodeRightIsland, avlSortedByAddrIslandsNode );

            bool couldMerge = this->nativeMemProv.MergePageHandles( rightBorderIsland->allocHandle, newPageHandle );

            FATAL_ASSERT( couldMerge == true );

            mergingResultIsland = rightBorderIsland->left_relocate_island_infostruct( this );
        }

        // If we have an island that we merged the page handle to, then we must have enough space on said island to allocate.
        // This is because the original handle had enough space aswell.

        // This is caused when we have no island close to us that we merged into.
        // Thus we just turn the pageHandle into a completely new island!
        if ( mergingResultIsland == nullptr )
        {
            // Create the new island.
            void *memPtr = (void*)pageHandleMemRegion.GetSliceStartPoint();

            VMemIsland *newIsland = new (memPtr) VMemIsland( this, newPageHandle );

            // We can register the island too.
            LIST_APPEND( this->listIslands.root, newIsland->managerNode );

            this->sortedByAddrIslands.Insert( &newIsland->avlSortedByAddrIslandsNode );

            mergingResultIsland = newIsland;
        }

        return mergingResultIsland;
    }

    AINLINE VMemIsland* _FindIslandContainingMemory( const void *memptr ) noexcept
    {
        // First we want to find the island that belongs to this allocation.
        AVLNode *avlIslandOfAlloc = this->sortedByAddrIslands.FindAnyNodeByCriteria(
            [&]( const AVLNode *leftCompareNode ) noexcept
        {
            const VMemIsland *leftCompareIsland = AVL_GETITEM( VMemIsland, leftCompareNode, avlSortedByAddrIslandsNode );

            const memBlockSlice_t& leftCompareSlice = leftCompareIsland->allocHandle->GetTargetSlice();

            size_t leftCompare_startOff = leftCompareSlice.GetSliceStartPoint();
            size_t leftCompare_endOff = leftCompareSlice.GetSliceEndPoint();

            size_t memptr_num = (size_t)memptr;

            if ( leftCompare_startOff <= memptr_num && leftCompare_endOff >= memptr_num )
            {
                return eir::eCompResult::EQUAL;
            }

            if ( leftCompare_startOff > memptr_num )
            {
                return eir::eCompResult::LEFT_GREATER;
            }

            // leftCompare_endOff < memptr_num.
            return eir::eCompResult::LEFT_LESS;
        });

        if ( avlIslandOfAlloc == nullptr )
        {
            // No island, topkek.
            return nullptr;
        }

        VMemIsland *islandOfAlloc = AVL_GETITEM( VMemIsland, avlIslandOfAlloc, avlSortedByAddrIslandsNode );

        return islandOfAlloc;
    }

public:
    // Useful for debugging the standard island merging scenarios.
    inline void ActivateMemoryRegion( void *memPtr, size_t memSize ) noexcept
    {
        typename PageAllocatorType::pageHandle *newPageHandle = this->nativeMemProv.Allocate( memPtr, memSize );

        if ( newPageHandle != nullptr )
        {
            this->ActivateMemoryRegionByHandle( newPageHandle );
        }
    }

    // If you want to free memory that was previously activated by ActivateMemoryRegion but nothing is allocated on it.
    inline void ReleaseUnallocatedMemory( const void *memPtr ) noexcept
    {
        VMemIsland *containingIsland = _FindIslandContainingMemory( memPtr );

        if ( containingIsland )
        {
            // Is the island empty?
            if ( containingIsland->HasNoAllocations() )
            {
                // Just release it.
                this->_RemoveEmptyHeapIsland( containingIsland, true );
            }
        }
    }

    inline void* Allocate( size_t memSize, size_t alignedBy = 0 ) noexcept
    {
        // This is not an easy system, lol.

        if ( memSize == 0 )
        {
            // Cannot allocate something that has no size.
            return nullptr;
        }

        if ( alignedBy == 0 )
        {
            // I guess the user wants the best-default.
            alignedBy = DEFAULT_ALIGNMENT;
        }

        // If the allocation succeeded we have this data.
        VMemAllocation *allocObj;

        // Try one of the existing islands for a memory allocation first.
        VMemAllocation *tryAllocObj = this->AllocateOnFreeSpace( memSize, alignedBy );

        if ( tryAllocObj != nullptr )
        {
            allocObj = tryAllocObj;
            goto gotToAllocate;
        }

        // If all existing islands showed no already-provided available memory, then we try to expand islands.
        {
            vmemIslandReverseIter_t island_iter( this->listIslands );
            size_t island_iter_cnt = 0;

            while ( !island_iter.IsEnd() )
            {
                if ( island_iter_cnt >= MAX_ISLAND_BURST_COUNT )
                {
                    break;
                }

                island_iter_cnt++;

                VMemIsland *item = island_iter.Resolve();

                VMemAllocation *tryAllocObj = this->AllocateOnIslandWithExpand( item, memSize, alignedBy );

                if ( tryAllocObj != nullptr )
                {
                    allocObj = tryAllocObj;
                    goto gotToAllocate;
                }

                island_iter.Increment();
            }

            // If all islands refused to provide memory then we have to provide an entirely new island.
            // At least we try.
            {
                // Determine the minimum memory size that we should reserve for the island.
                size_t pageSize = this->nativeMemProv.GetPageSize();

                size_t minSizeByMinPages = ( pageSize * MIN_PAGES_FOR_ISLAND );

                // Since ALIGNment is always >= than the input and offsets are equal-synonyms to sizes,
                // we can use this to have the first position of a header.
                size_t offsetToFirstHeaderTryPos = ALIGN_SIZE( sizeof(VMemIsland), VMemIsland::HEADER_ALIGNMENT );

                // It is most important that we at least can allocate one object on the new allocation.
                // Since we cannot know the virtual memory address of allocation in advance we
                // actually have to do some good estimate on the maximum required memory size.
                // But since the virtual memory pages are allocated at power-of-two offsets the
                // estimate should be very good for power-of-two alignments.
                size_t minSizeByObject = ( offsetToFirstHeaderTryPos + alignedBy + memSize + sizeof(VMemAllocation) );

                size_t actualMinSize = std::max( minSizeByMinPages, minSizeByObject );

                typename PageAllocatorType::pageHandle *newPageHandle = this->nativeMemProv.Allocate( nullptr, actualMinSize );

                if ( newPageHandle )
                {
                    VMemIsland *mergingResultIsland = this->ActivateMemoryRegionByHandle( newPageHandle );

                    VMemAllocation *newAllocObj = this->AllocateOnFreeSpace( memSize, alignedBy );

                    // TODO: just place the allocation on that island by doing the math properly; being lazy here (meh).

                    FATAL_ASSERT( newAllocObj != nullptr );
                    FATAL_ASSERT( newAllocObj->freeSpaceAfterThis.manager == mergingResultIsland );

                    // Just return the new allocation.
                    allocObj = newAllocObj;
                    goto gotToAllocate;
                }
            }

            // If there are any islands remaining that we did not try yet then do so.
            while ( !island_iter.IsEnd() )
            {
                VMemIsland *item = island_iter.Resolve();

                VMemAllocation *tryAllocObj = this->AllocateOnIslandWithExpand( item, memSize, alignedBy );

                if ( tryAllocObj != nullptr )
                {
                    allocObj = tryAllocObj;
                    goto gotToAllocate;
                }

                island_iter.Increment();
            }
        }

        // Could not allocate anything.
        // The most probable reason is that there is no more system RAM available.
        return nullptr;

    gotToAllocate:
        // Return the data portion of our allocation.
        void *dataPtr = ( (char*)allocObj + allocObj->dataOff );

        return dataPtr;
    }

private:
    AINLINE VMemAllocation* get_mem_block_from_ptr( void *memPtr ) noexcept
    {
        // TODO: add debug to this code, so that memory corruption can be combatted.
        // it could iterate over all memory pointers to verify that memPtr really belongs to us.

        size_t header_size = sizeof(VMemAllocation);

        size_t memOff = (size_t)memPtr;

        size_t memHandleOff = SCALE_DOWN( memOff - header_size, VMemIsland::HEADER_ALIGNMENT );

        return (VMemAllocation*)memHandleOff;
    }

    AINLINE const VMemAllocation* get_mem_block_from_ptr( const void *memPtr ) const noexcept
    {
        size_t header_size = sizeof(VMemAllocation);

        size_t memOff = (size_t)memPtr;

        size_t memHandleOff = SCALE_DOWN( memOff - header_size, VMemIsland::HEADER_ALIGNMENT );

        return (const VMemAllocation*)memHandleOff;
    }

    // Forward declaration.
    struct VMemIsland;

    AINLINE void _RemoveEmptyHeapIsland( VMemIsland *theIsland, bool cleanMemoryHandle ) noexcept
    {
        typename PageAllocatorType::pageHandle *islandHandle = theIsland->allocHandle;

        if ( is_free_region_empty( theIsland->firstFreeSpaceBlock.freeRegion ) == false )
        {
            this->avlFreeBlockSortedBySize.RemoveByNodeFast( &theIsland->firstFreeSpaceBlock.sortedBySizeNode );
        }

        LIST_REMOVE( theIsland->managerNode );
        LIST_REMOVE( theIsland->firstFreeSpaceBlock.sortedByAddrNode );

        FATAL_ASSERT( LIST_EMPTY( theIsland->sortedByAddrFreeBlocks.root ) == true );

        this->sortedByAddrIslands.RemoveByNodeFast( &theIsland->avlSortedByAddrIslandsNode );

        theIsland->~VMemIsland();

        if ( cleanMemoryHandle )
        {
            this->nativeMemProv.Free( islandHandle );
        }
    }

public:
    inline void Free( void *memPtr ) noexcept
    {
        // We guarrantee that this operation is O(log(h)) in Release mode with all optimizations.

        VMemAllocation *memHandle = get_mem_block_from_ptr( memPtr );

        // Release the memory.
        VMemIsland *manager = memHandle->freeSpaceAfterThis.manager;

        manager->Free( this, memHandle );

        // If the memory island is empty, we can garbage collect it.
        if ( manager->HasNoAllocations() )
        {
            _RemoveEmptyHeapIsland( manager, true );
        }
    }

    // Attempts to change the size of an allocation.
    inline bool SetAllocationSize( void *memPtr, size_t newSize ) noexcept
    {
        // We can only fail if the allocation does not fit with regards to the remaining free space.
        // Or the required data size is zero (makes no sense!)

        VMemAllocation *memHandle = get_mem_block_from_ptr( memPtr );

        VMemIsland *manager = memHandle->freeSpaceAfterThis.manager;

        return manager->ResizeAllocation( this, memHandle, memPtr, newSize );
    }

    // Returns the data size of an allocation.
    inline size_t GetAllocationSize( const void *memPtr ) const noexcept
    {
        const VMemAllocation *memHandle = get_mem_block_from_ptr( memPtr );

        return memHandle->dataSize;
    }

    // Returns the whole size of this allocation.
    // This includes the meta-data header as well as the alignment.
    inline size_t GetAllocationMetaSize( const void *memPtr ) const noexcept
    {
        const VMemAllocation *memHandle = get_mem_block_from_ptr( memPtr );

        return ( memHandle->dataOff + memHandle->dataSize );
    }

    struct heapStats
    {
        size_t usedBytes = 0;
        size_t usedMetaBytes = 0;
        size_t freeBytes = 0;
        size_t countOfAllocations = 0;
        size_t countOfIslands = 0;
        size_t islandMinFreeSpaceSize_min = 0;
        size_t islandMinFreeSpaceSize_max = 0;
        size_t islandMaxFreeSpaceSize_min = 0;
        size_t islandMaxFreeSpaceSize_max = 0;
    };

    // Returns statistics about this memory allocator.
    inline heapStats GetStatistics( void ) const noexcept
    {
        heapStats stats;

        bool first_island = true;

        LIST_FOREACH_BEGIN( VMemIsland, this->listIslands.root, managerNode )

            typename VMemIsland::usageStats islandStats = item->GetUsageStatistics();

            stats.usedBytes += islandStats.usedBytes;
            stats.usedMetaBytes += islandStats.usedMetaBytes;
            stats.freeBytes += islandStats.freeBytes;
            stats.countOfAllocations += islandStats.countOfAllocations;

            if ( first_island )
            {
                stats.islandMinFreeSpaceSize_min = islandStats.minSizeOfFreeSpace;
                stats.islandMinFreeSpaceSize_max = islandStats.minSizeOfFreeSpace;
                stats.islandMaxFreeSpaceSize_min = islandStats.maxSizeOfFreeSpace;
                stats.islandMaxFreeSpaceSize_max = islandStats.maxSizeOfFreeSpace;

                first_island = false;
            }
            else
            {
                // Bound the minimum.
                if ( stats.islandMinFreeSpaceSize_min > islandStats.minSizeOfFreeSpace )
                {
                    stats.islandMinFreeSpaceSize_min = islandStats.minSizeOfFreeSpace;
                }

                if ( stats.islandMinFreeSpaceSize_max < islandStats.minSizeOfFreeSpace )
                {
                    stats.islandMinFreeSpaceSize_max = islandStats.minSizeOfFreeSpace;
                }

                // Bound the maximum.
                if ( stats.islandMaxFreeSpaceSize_min > islandStats.maxSizeOfFreeSpace )
                {
                    stats.islandMaxFreeSpaceSize_min = islandStats.maxSizeOfFreeSpace;
                }

                if ( stats.islandMaxFreeSpaceSize_max < islandStats.maxSizeOfFreeSpace )
                {
                    stats.islandMaxFreeSpaceSize_max = islandStats.maxSizeOfFreeSpace;
                }
            }

            // One more island.
            stats.countOfIslands++;

        LIST_FOREACH_END

        return stats;
    }

    // Walks all allocations of this heap allocator.
    template <typename callbackType>
    AINLINE void WalkAllocations( callbackType&& cb ) noexcept(noexcept(cb( nullptr )))
    {
        LIST_FOREACH_BEGIN( VMemIsland, this->listIslands.root, managerNode )

            // Even if we walk allocations in memory-order for single islands, we have not ordered the islands (no point),
            // so there is no order-guarantee for this function.
            item->WalkAllocations(
                [&]( VMemAllocation *allocObj ) noexcept(noexcept(cb( nullptr )))
            {
                void *memPtr = (char*)allocObj + allocObj->dataOff;

                cb( memPtr );
            });

        LIST_FOREACH_END
    }

    // Quick helper to check if an allocation is inside this heap allocator.
    AINLINE bool DoesOwnAllocation( const void *memptr ) noexcept
    {
        VMemIsland *islandOfAlloc = _FindIslandContainingMemory( memptr );

        if ( islandOfAlloc == nullptr )
            return false;

        // Now check if the island has such an allocation entry.
        // We have such an entry if we have a free block whose end pointer is directly before the memory pointer.
        // Could be optimized in the future if we use AVLTree for it aswell, but could also be considered overkill.
        typename VMemIsland::freeBlockByAddrIter_t freeblock_iter( islandOfAlloc->sortedByAddrFreeBlocks );

        freeblock_iter.Increment();

        while ( !freeblock_iter.IsEnd() )
        {
            const VMemAllocation *theAlloc = LIST_GETITEM( VMemAllocation, freeblock_iter.Resolve(), freeSpaceAfterThis );

            const void *allocdataptr = (char*)theAlloc + theAlloc->dataOff;

            if ( allocdataptr == memptr )
            {
                return true;
            }

            freeblock_iter.Increment();
        }

        return false;
    }

    // Simple realloc helper just because it is being exposed in the CRT aswell.
    inline void* Realloc( void *memPtr, size_t newSize, size_t alignment = DEFAULT_ALIGNMENT ) noexcept
    {
        if ( memPtr == nullptr )
        {
            return Allocate( newSize, alignment );
        }

        if ( newSize == 0 )
        {
            Free( memPtr );
            return nullptr;
        }

        // Now do the tricky part.
        // If we suceeded in resizing, we leave it at that.
        // Otherwise we must allocate a new bit of memory, copy all old bytes over, free the old and return the new.
        bool couldResize = SetAllocationSize( memPtr, newSize );

        if ( couldResize )
        {
            return memPtr;
        }

        // Now we just trash the old block.
        // Did the CRT state anything about alignment tho?
        void *newMemPtr = Allocate( newSize, alignment );

        if ( newMemPtr == nullptr )
        {
            // We follow the guide as to what happens when "realloc fails"...
            // https://linux.die.net/man/3/realloc
            // You can detect this case when you passed in a positive value
            // for request size but this function returns nullptr.
            return nullptr;
        }

        // Memory copy.
        {
            char *srcPtr = (char*)memPtr;
            char *dstPtr = (char*)newMemPtr;
            size_t srcSize = GetAllocationSize( memPtr );

            for ( size_t n = 0; n < newSize; n++ )
            {
                char putByte;

                if ( n < srcSize )
                {
                    putByte = *( srcPtr + n );
                }
                else
                {
                    putByte = 0;
                }

                *( dstPtr + n ) = putByte;
            }
        }

        // Free the old.
        Free( memPtr );

        return newMemPtr;
    }

private:
    // Virtual memory manager object.
    PageAllocatorType nativeMemProv;

    // To increase allocation performance we remember all free memory regions and sort
    // this list by size of free blocks. So when we process an allocation request we
    // very quickly know where to put it into for best-fit.
    struct VMemAllocation;

    struct VMemFreeBlock
    {
        inline VMemFreeBlock( VMemIsland *manager ) noexcept : manager( manager )
        {
            return;
        }
        inline VMemFreeBlock( VMemIsland *manager, memBlockSlice_t slice ) noexcept
            : manager( manager ), freeRegion( std::move( slice ) )
        {
            return;
        }

        inline VMemFreeBlock( const VMemFreeBlock& ) = delete;
        inline VMemFreeBlock( VMemFreeBlock&& right, PlatformHeapAllocator *heapMan ) noexcept : manager( std::move( right.manager ) ), freeRegion( std::move( right.freeRegion ) )
        {
            if ( is_free_region_empty( this->freeRegion ) == false )
            {
                heapMan->avlFreeBlockSortedBySize.MoveNodeTo( this->sortedBySizeNode, std::move( right.sortedBySizeNode ) );
            }

            this->sortedByAddrNode.moveFrom( std::move( right.sortedByAddrNode ) );

            right.manager = nullptr;
            right.freeRegion = memBlockSlice_t();
        }

        inline ~VMemFreeBlock( void ) = default;

        inline VMemFreeBlock& operator = ( const VMemFreeBlock& ) = delete;
        inline VMemFreeBlock& operator = ( VMemFreeBlock&& ) = delete;

        inline void moveFrom( VMemFreeBlock&& right, PlatformHeapAllocator *heapMan ) noexcept
        {
            this->manager = std::move( right.manager );
            this->freeRegion = std::move( right.freeRegion );

            if ( is_free_region_empty( this->freeRegion ) == false )
            {
                heapMan->avlFreeBlockSortedBySize.MoveNodeTo( this->sortedBySizeNode, std::move( right.sortedBySizeNode ) );
            }

            this->sortedByAddrNode.moveFrom( std::move( right.sortedByAddrNode ) );

            right.manager = nullptr;
            right.freeRegion = memBlockSlice_t();
        }

        VMemIsland *manager;    // need this field because freeing memory/being registered in the manager itself.

        memBlockSlice_t freeRegion;                 // can be empty to display no space (0, -1).
        AVLNode sortedBySizeNode;
        RwListEntry <VMemFreeBlock> sortedByAddrNode;
    };

    // Allocation object on a VMemIsland object.
    struct VMemIsland;

    struct VMemAllocation
    {
        inline VMemAllocation( VMemIsland *allocHost, size_t dataSize, size_t dataOff ) noexcept : freeSpaceAfterThis( allocHost )
        {
            this->dataSize = dataSize;
            this->dataOff = dataOff;
        }

        inline VMemAllocation( const VMemAllocation& ) = delete;
        inline VMemAllocation( VMemAllocation&& ) = delete;

        inline ~VMemAllocation( void )
        {
            // Anything to do?
            return;
        }

        inline VMemAllocation& operator = ( const VMemAllocation& ) = delete;
        inline VMemAllocation& operator = ( VMemAllocation&& ) = delete;

        // Returns the region that this allocation has to occupy.
        inline memBlockSlice_t GetRegion( void ) const noexcept
        {
            size_t dataStart = (size_t)this;
            size_t dataSize = ( this->dataOff + this->dataSize );

            return memBlockSlice_t( dataStart, dataSize );
        }

        // We need certain meta-data per-allocation to maintain stuff.

        // Statistic fields.
        size_t dataSize;        // size in bytes of the region after this header reserved for the user application.
        size_t dataOff;         // offset after this header to the data for alignment purposes.

        // Manager meta-data.
        VMemFreeBlock freeSpaceAfterThis;   // designates any free space that could be after this allocation.

        // THERE ALWAYS IS DATA PAST THIS STRUCT, DETERMINED BY THE dataSize FIELD.
        // But it is offset by dataOff from the start of this struct.
    };

    // Sorted-by-size AVLTree dispatcher.
    struct avlFreeBlockSortedBySizeDispatcher
    {
        static eir::eCompResult CompareNodes( const AVLNode *left, const AVLNode *right ) noexcept
        {
            const VMemFreeBlock *leftBlock = AVL_GETITEM( VMemFreeBlock, left, sortedBySizeNode );
            const VMemFreeBlock *rightBlock = AVL_GETITEM( VMemFreeBlock, right, sortedBySizeNode );

            return eir::DefaultValueCompare(
                leftBlock->freeRegion.GetSliceSize(),
                rightBlock->freeRegion.GetSliceSize()
            );
        }

        static eir::eCompResult CompareNodeWithValue( const AVLNode *left, size_t right ) noexcept
        {
            const VMemFreeBlock *leftBlock = AVL_GETITEM( VMemFreeBlock, left, sortedBySizeNode );

            return eir::DefaultValueCompare(
                leftBlock->freeRegion.GetSliceSize(),
                right
            );
        }
    };
    typedef AVLTree <avlFreeBlockSortedBySizeDispatcher> VMemFreeBlockAVLTree;

    VMemFreeBlockAVLTree avlFreeBlockSortedBySize;

    // Container of many allocation objects, growing infinitely.
    struct VMemIsland
    {
        // This class is placed on top of every vmem page allocation.

        inline VMemIsland( PlatformHeapAllocator *heapMan, PageAllocatorType::pageHandle *allocHandle ) noexcept : firstFreeSpaceBlock( this )
        {
            this->allocHandle = allocHandle;

            // Initialize the free space at its entirety.
            size_t realMemStartOffset = ( (size_t)this + sizeof(VMemIsland) );

            this->firstFreeSpaceBlock.freeRegion =
                memBlockSlice_t::fromOffsets( realMemStartOffset, allocHandle->GetTargetSlice().GetSliceEndPoint() );

            // List it into the manager.
            LIST_APPEND( this->sortedByAddrFreeBlocks.root, this->firstFreeSpaceBlock.sortedByAddrNode );

            if ( is_free_region_empty( this->firstFreeSpaceBlock.freeRegion ) == false )
            {
                heapMan->avlFreeBlockSortedBySize.Insert( &this->firstFreeSpaceBlock.sortedBySizeNode );
            }
        }
        inline VMemIsland( const VMemIsland& ) = delete;
        inline VMemIsland( VMemIsland&& right, PlatformHeapAllocator *heapMan ) noexcept :
            firstFreeSpaceBlock( std::move( right.firstFreeSpaceBlock ), heapMan )
        {
            this->managerNode.moveFrom( std::move( right.managerNode ) );
            heapMan->sortedByAddrIslands.MoveNodeTo( this->avlSortedByAddrIslandsNode, std::move( right.avlSortedByAddrIslandsNode ) );
            this->allocHandle = right.allocHandle;

            LIST_REMOVE( right.firstFreeSpaceBlock.sortedByAddrNode );

            LIST_APPEND( this->sortedByAddrFreeBlocks.root, this->firstFreeSpaceBlock.sortedByAddrNode );
            this->sortedByAddrFreeBlocks.moveIntoEnd( std::move( right.sortedByAddrFreeBlocks ) );

            // Update the manager pointers in allocations.
            freeBlockByAddrIter_t fbiter( this->sortedByAddrFreeBlocks );

            while ( !fbiter.IsEnd() )
            {
                VMemFreeBlock *block = fbiter.Resolve();

                block->manager = this;

                fbiter.Increment();
            }

            // Clean up.
            right.allocHandle = nullptr;
        }

        inline ~VMemIsland( void )
        {
            // Cleaning up of memory is done by the implementation that destroys this instance.
            return;
        }

        // Append all references of the next island into our island.
        AINLINE void move_append_island_references( PlatformHeapAllocator *heapMan, VMemIsland *nextIslandMoveFrom ) noexcept
        {
#ifdef _DEBUG
            FATAL_ASSERT( this < nextIslandMoveFrom );
#endif //_DEBUG

            // Get the new to-be-comitted free space region.
            eir::upperBound <size_t> newlyAvailableFreeSpaceEndBound = nextIslandMoveFrom->firstFreeSpaceBlock.freeRegion.GetSliceEndBound();

            // Remove the free space block of the next island.
            if ( is_free_region_empty( nextIslandMoveFrom->firstFreeSpaceBlock.freeRegion ) == false )
            {
                heapMan->avlFreeBlockSortedBySize.RemoveByNodeFast( &nextIslandMoveFrom->firstFreeSpaceBlock.sortedBySizeNode );

                nextIslandMoveFrom->firstFreeSpaceBlock.freeRegion = memBlockSlice_t();
            }

            // Remove the first free block from the next island so we do not take it with us.
            LIST_REMOVE( nextIslandMoveFrom->firstFreeSpaceBlock.sortedByAddrNode );

            // First we update the free space after our island.
            VMemFreeBlock *lastFreeSpace = this->GetLastFreeBlock();

            if ( is_free_region_empty( lastFreeSpace->freeRegion ) == false )
            {
                heapMan->avlFreeBlockSortedBySize.RemoveByNodeFast( &lastFreeSpace->sortedBySizeNode );
            }

            lastFreeSpace->freeRegion.SetSliceEndBound( std::move( newlyAvailableFreeSpaceEndBound ) );

            if ( is_free_region_empty( lastFreeSpace->freeRegion ) == false )
            {
                heapMan->avlFreeBlockSortedBySize.Insert( &lastFreeSpace->sortedBySizeNode );
            }

            // Then we move over all the allocations from mergeWith to us.
            freeBlockByAddrIter_t fb_iter( nextIslandMoveFrom->sortedByAddrFreeBlocks );

            while ( !fb_iter.IsEnd() )
            {
                VMemFreeBlock *freeBlock = fb_iter.Resolve();

                freeBlock->manager = this;

                fb_iter.Increment();
            }

            // Concatenate the lists.
            this->sortedByAddrFreeBlocks.moveIntoEnd( std::move( nextIslandMoveFrom->sortedByAddrFreeBlocks ) );

            // Do a valid reset of the island.
            LIST_INSERT( nextIslandMoveFrom->sortedByAddrFreeBlocks.root, nextIslandMoveFrom->firstFreeSpaceBlock.sortedByAddrNode );
        }

        inline VMemIsland& operator = ( const VMemIsland& ) = delete;
        inline VMemIsland& operator = ( VMemIsland&& ) = delete;

        // The alignment that is required for the header struct (VMemAllocation).
        static constexpr size_t HEADER_ALIGNMENT = alignof(VMemAllocation);

        // Used by object allocation to determine the correct bounds.
        struct alignedObjSizeByOffset
        {
            AINLINE alignedObjSizeByOffset( size_t dataSize, size_t dataAlignment ) noexcept
            {
                this->dataSize = dataSize;
                this->dataAlignment = dataAlignment;
            }

            // Since recently we actually require finding a previous block, just so that we support
            // growing memory islands to the left aswell.
            AINLINE void ScanPrevBlock( size_t& offsetInOut, size_t& sizeOut ) noexcept
            {
                // We expect the offset to the byte just after free bytes.
                size_t offsetIn = offsetInOut;

                size_t dataSize = this->dataSize;

                size_t goodStartPosForData = SCALE_DOWN( offsetIn - dataSize, this->dataAlignment );

                size_t goodStartPosForHeader = SCALE_DOWN( goodStartPosForData - sizeof(VMemAllocation), HEADER_ALIGNMENT );

                size_t allocDataOff = ( goodStartPosForData - goodStartPosForHeader );

                offsetInOut = goodStartPosForHeader;
                sizeOut = ( allocDataOff + dataSize );

                this->allocDataOff = allocDataOff;
            }

            // Since this function is called every time until we found the last spot, we can
            // save state during the call that we fetch when we are done.
            AINLINE void ScanNextBlock( size_t& offsetInOut, size_t& sizeOut ) noexcept
            {
                size_t offsetIn = offsetInOut;

                // We have to at least start allocation from this offset.
                size_t minStartPosForHeader = ALIGN_SIZE( offsetIn, HEADER_ALIGNMENT );

                size_t minEndOffsetAfterHeader = ( minStartPosForHeader + sizeof(VMemAllocation) );

                // Calculate the position of our data that we should use.
                size_t goodStartPosForData = ALIGN_SIZE( minEndOffsetAfterHeader, this->dataAlignment );

                // Calculate the real header position now.
                size_t goodStartPosForHeader = SCALE_DOWN( goodStartPosForData - sizeof(VMemAllocation), HEADER_ALIGNMENT );

                // Determine the real memory size we have to allocate.
                size_t endOffsetAfterData = ( goodStartPosForData + this->dataSize );

                size_t realAllocSize = ( endOffsetAfterData - goodStartPosForHeader );

                // Return good stuff.
                offsetInOut = goodStartPosForHeader;
                sizeOut = realAllocSize;

                // Remember good meta-data.
                this->allocDataOff = ( goodStartPosForData - goodStartPosForHeader );
            }

            AINLINE size_t GetAlignment( void ) const noexcept
            {
                // Cannot really say.
                return 1;
            }

            // Meta-data for ourselves.
            size_t dataSize;
            size_t dataAlignment;

            // Data that we can fetch after successful allocation.
            size_t allocDataOff;
        };

        // Returns the size of memory actually taken by data for this island allocation.
        // This is defined by the offset of the first byte in the last free space block.
        inline size_t GetIslandUsedBytesSize( void ) const noexcept
        {
            FATAL_ASSERT( LIST_EMPTY( this->sortedByAddrFreeBlocks.root ) == false );

            VMemFreeBlock *lastFreeBlock = LIST_GETITEM( VMemFreeBlock, this->sortedByAddrFreeBlocks.root.prev, sortedByAddrNode );

            return ( lastFreeBlock->freeRegion.GetSliceStartPoint() - (size_t)this );
        }

        AINLINE bool grow_validity_region_to_right( PlatformHeapAllocator *manager, VMemFreeBlock *lastFreeBlock, size_t newReqSize ) noexcept
        {
            bool growSuccess = manager->nativeMemProv.SetHandleSize( this->allocHandle, newReqSize );

            if ( growSuccess )
            {
                if ( is_free_region_empty( lastFreeBlock->freeRegion ) == false )
                {
                    manager->avlFreeBlockSortedBySize.RemoveByNodeFast( &lastFreeBlock->sortedBySizeNode );
                }

                // Grow the available free space.
                lastFreeBlock->freeRegion.SetSliceEndBound( this->allocHandle->GetTargetSlice().GetSliceEndBound() );

                if ( is_free_region_empty( lastFreeBlock->freeRegion ) == false )
                {
                    // Since we have grown we must have some space now.
                    // But empty might not be a trivial definition anymore.
                    manager->avlFreeBlockSortedBySize.Insert( &lastFreeBlock->sortedBySizeNode );
                }
            }

            return growSuccess;
        }

        AINLINE VMemIsland* left_relocate_island_infostruct( PlatformHeapAllocator *manager ) noexcept
        {
            typename PageAllocatorType::pageHandle *islandPageHandle = this->allocHandle;

            VMemIsland tmpIsland( std::move( *this ), manager );

            void *newIslandInfoStructOff = ( islandPageHandle->GetTargetPointer() );

            VMemIsland *newIslandPtr = new (newIslandInfoStructOff) VMemIsland( std::move( tmpIsland ), manager );

            // Delete ourselves.
            this->~VMemIsland();

            if ( is_free_region_empty( newIslandPtr->firstFreeSpaceBlock.freeRegion ) == false )
            {
                manager->avlFreeBlockSortedBySize.RemoveByNodeFast( &newIslandPtr->firstFreeSpaceBlock.sortedBySizeNode );
            }

            newIslandPtr->firstFreeSpaceBlock.freeRegion.SetSliceStartBound( (size_t)( newIslandPtr + 1 ) );

            if ( is_free_region_empty( newIslandPtr->firstFreeSpaceBlock.freeRegion ) == false )
            {
                manager->avlFreeBlockSortedBySize.Insert( &newIslandPtr->firstFreeSpaceBlock.sortedBySizeNode );
            }

            return newIslandPtr;
        }

        AINLINE VMemIsland* grow_validity_region_to_left( PlatformHeapAllocator *manager, size_t newReqSize ) noexcept
        {
            typename PageAllocatorType::pageHandle *islandPageHandle = this->allocHandle;

            bool growSuccess = manager->nativeMemProv.LeftResizeHandle( islandPageHandle, newReqSize );

            if ( growSuccess )
            {
                if ( this != islandPageHandle->GetTargetPointer() )
                {
                    return left_relocate_island_infostruct( manager );
                }

                return this;
            }

            return nullptr;
        }

        AINLINE VMemIsland* get_prev_island( void ) noexcept
        {
            typename decltype(PlatformHeapAllocator::sortedByAddrIslands)::diff_node_iterator iter( &this->avlSortedByAddrIslandsNode );

            iter.Decrement();

            if ( iter.IsEnd() )
                return nullptr;

            AVLNode *prevIslandNode = iter.Resolve();

            return AVL_GETITEM( VMemIsland, prevIslandNode, avlSortedByAddrIslandsNode );
        }

        AINLINE VMemIsland* get_next_island( void ) noexcept
        {
            typename decltype(PlatformHeapAllocator::sortedByAddrIslands)::diff_node_iterator iter( &this->avlSortedByAddrIslandsNode );

            iter.Increment();

            if ( iter.IsEnd() )
                return nullptr;

            AVLNode *nextIslandNode = iter.Resolve();

            return AVL_GETITEM( VMemIsland, nextIslandNode, avlSortedByAddrIslandsNode );
        }

        AINLINE bool MakeFreeSpaceToTheRight( PlatformHeapAllocator *manager, VMemFreeBlock *lastFreeBlock, size_t finalMemEndOffset ) noexcept
        {
            // Calculate the possible amount of memory available for allocation until we hit the next hard obstacle.
            // If there is no next obstacle then we just ignore this.
            VMemIsland *nextIsland = this->get_next_island();

            if ( nextIsland )
            {
                VMemFreeBlock *firstFreeBlock = &nextIsland->firstFreeSpaceBlock;

                size_t nextIslandFirstAllocMemoryOffset = ( firstFreeBlock->freeRegion.GetSliceEndPoint() + 1 );

                // If the required end offset is past the first allocation beginning of the next island, then we
                // cannot even allocate.
                bool is_alloc_obstructed_by_next_island = ( nextIslandFirstAllocMemoryOffset < finalMemEndOffset );

                if ( is_alloc_obstructed_by_next_island )
                {
                    return false;
                }
            }

            void *vmemPtr = (void*)this;
            size_t vmemOffset = (size_t)vmemPtr;

            // If we are intersecting or coming-to-border with the next island, then we must merge regions with it.
            // Since we are not obstructing with the next island this is no problem.
            bool needToGrowIsland = true;

            if ( nextIsland )
            {
                typename PageAllocatorType::pageHandle *nextHandle = nextIsland->allocHandle;

                // Must predict to what region we would grow to next.
                size_t vmemIslandFinalEndOff = ALIGN_SIZE( finalMemEndOffset, manager->nativeMemProv.GetPageSize() );

                memBlockSlice_t newIslandSlice = memBlockSlice_t::fromOffsets( vmemOffset, vmemIslandFinalEndOff );

                eir::eCompResult cmpRes = eir::MathSliceHelpers::CompareSlicesByIntersection( newIslandSlice, nextHandle->GetTargetSlice(), true );

                if ( cmpRes == eir::eCompResult::EQUAL )
                {
                    // Merge the islands.
                    eIslandMergeResult mergeRes = manager->MergeIslands( this, nextIsland );

                    if ( mergeRes == eIslandMergeResult::NO_MERGE )
                    {
                        // Cannot expand island to the right.
                        return false;
                    }

                    // Because according to our sorting nextIsland is after theIsland.
                    FATAL_ASSERT( mergeRes == eIslandMergeResult::MERGE_TO_FIRST );

                    // No need to update pointers because we stay on same island.

                    needToGrowIsland = false;
                }
            }

            if ( needToGrowIsland )
            {
                // Calculate the required new virtual memory size.
                size_t newReqSize = ( finalMemEndOffset - vmemOffset );

                bool growSuccess = this->grow_validity_region_to_right( manager, lastFreeBlock, newReqSize );

                if ( !growSuccess )
                {
                    return false;
                }
            }

            // Ready to allow something on our island.
            return true;
        }

        AINLINE VMemAllocation* InplaceNewAllocation( PlatformHeapAllocator *heapMan, VMemFreeBlock *freeBlockToAllocateInto, void *allocPtr, size_t allocDataOff, size_t dataSize, memBlockSlice_t allocSlice ) noexcept
        {
            VMemAllocation *newAlloc = new (allocPtr) VMemAllocation( this, dataSize, allocDataOff );

            // Subtract our allocation from the free region we have found and newly manage the things.
            bool hadSomethingStartFromLeft = false;
            bool hadFreeSpaceAfterNewAlloc = false;

            // Update the pointery things.
            LIST_INSERT( freeBlockToAllocateInto->sortedByAddrNode, newAlloc->freeSpaceAfterThis.sortedByAddrNode );

            // Update the region sizes.

            // It cannot be empty because something just got allocated into it.
            FATAL_ASSERT( is_free_region_empty( freeBlockToAllocateInto->freeRegion ) == false );

            // When updating AVLTree node values we must remove the nodes (temporarily).
            heapMan->avlFreeBlockSortedBySize.RemoveByNodeFast( &freeBlockToAllocateInto->sortedBySizeNode );

            freeBlockToAllocateInto->freeRegion.subtractRegion( allocSlice,
                [&]( const memBlockSlice_t& slicedRegion, bool isStartingFromLeft )
            {
                if ( isStartingFromLeft )
                {
                    hadSomethingStartFromLeft = true;

                    // Update the new free region.
                    freeBlockToAllocateInto->freeRegion = slicedRegion;

                    if ( is_free_region_empty( slicedRegion ) == false )
                    {
                        // Still we must check if that region is empty or not.
                        heapMan->avlFreeBlockSortedBySize.Insert( &freeBlockToAllocateInto->sortedBySizeNode );
                    }
                }
                else
                {
                    // It is important that we keep the pointers inside of free region intact,
                    // so even if it is empty we know where it is supposed to start.
                    hadFreeSpaceAfterNewAlloc = true;

                    // This has to be the memory that is available just after our allocation.
                    newAlloc->freeSpaceAfterThis.freeRegion = slicedRegion;

                    if ( is_free_region_empty( slicedRegion ) == false )
                    {
                        heapMan->avlFreeBlockSortedBySize.Insert( &newAlloc->freeSpaceAfterThis.sortedBySizeNode );
                    }
                }
            });

            if ( !hadSomethingStartFromLeft )
            {
                // We have subtracted the left free block entirely, so keep it removed.
                freeBlockToAllocateInto->freeRegion.collapse();
            }

            if ( !hadFreeSpaceAfterNewAlloc )
            {
                // Make proper empty space.
                newAlloc->freeSpaceAfterThis.freeRegion = memBlockSlice_t( allocSlice.GetSliceEndPoint() + 1, 0 );
            }

            return newAlloc;
        }

    private:
        AINLINE bool is_last_node( VMemAllocation *allocObj ) noexcept
        {
            return ( this->sortedByAddrFreeBlocks.root.prev == &allocObj->freeSpaceAfterThis.sortedByAddrNode );
        }

        AINLINE void truncate_to_minimum_space( PlatformHeapAllocator *manager, VMemFreeBlock *lastFreeBlock ) noexcept
        {
            // WARNING: we assume that lastFreeBlock IS NOT INSIDE THE AVL TREE.

            // Make sure we at least have the minimum size.
            size_t pageSize = manager->nativeMemProv.GetPageSize();

            size_t minSizeByPage = ( pageSize * MIN_PAGES_FOR_ISLAND );

            size_t actualReqSize = minSizeByPage;

            // Minimum size by span.
            {
                size_t vmemOff = (size_t)this;

                size_t newReqSize_local = ( lastFreeBlock->freeRegion.GetSliceStartPoint() - vmemOff );

                if ( newReqSize_local > actualReqSize )
                {
                    actualReqSize = newReqSize_local;
                }
            }

            bool gotToShrink = manager->nativeMemProv.SetHandleSize( this->allocHandle, actualReqSize );

            FATAL_ASSERT( gotToShrink == true );

            // Update the region of free space for the last block.
            lastFreeBlock->freeRegion.SetSliceEndPoint( this->allocHandle->GetTargetSlice().GetSliceEndPoint() );
        }

    public:
        inline void Free( PlatformHeapAllocator *manager, VMemAllocation *allocObj ) noexcept
        {
            bool isLastNode = is_last_node( allocObj );

            // We simply release out the memory that we are asked to free.
            VMemFreeBlock *potLastFreeBlock = nullptr;
            {
                size_t newFreeEndOffset = allocObj->freeSpaceAfterThis.freeRegion.GetSliceEndPoint();

                RwListEntry <VMemFreeBlock> *nodePrevFreeBlock = allocObj->freeSpaceAfterThis.sortedByAddrNode.prev;

                // Has to be because there is a first free block, always.
                FATAL_ASSERT( nodePrevFreeBlock != &this->sortedByAddrFreeBlocks.root );

                VMemFreeBlock *prevFreeBlock = LIST_GETITEM( VMemFreeBlock, nodePrevFreeBlock, sortedByAddrNode );

                // When updating the size we must remove from the tree.
                if ( is_free_region_empty( prevFreeBlock->freeRegion ) == false )
                {
                    manager->avlFreeBlockSortedBySize.RemoveByNodeFast( &prevFreeBlock->sortedBySizeNode );
                }

                prevFreeBlock->freeRegion.SetSliceEndPoint( newFreeEndOffset );

                // If we deleted the last block, then the previous one becomes the new last.
                potLastFreeBlock = prevFreeBlock;
            }

            // Kill the current last node, with the free block.
            if ( is_free_region_empty( allocObj->freeSpaceAfterThis.freeRegion ) == false )
            {
                manager->avlFreeBlockSortedBySize.RemoveByNodeFast( &allocObj->freeSpaceAfterThis.sortedBySizeNode );
            }

            LIST_REMOVE( allocObj->freeSpaceAfterThis.sortedByAddrNode );

            allocObj->~VMemAllocation();

            // If we got rid of the last allocation, then we should attempt to shrink
            // the required memory region to best-fit.
            if ( isLastNode )
            {
                VMemFreeBlock *lastFreeBlock = potLastFreeBlock;

                truncate_to_minimum_space( manager, lastFreeBlock );
            }

            // Kinda has to have a size now (?).
            if ( is_free_region_empty( potLastFreeBlock->freeRegion ) == false )
            {
                manager->avlFreeBlockSortedBySize.Insert( &potLastFreeBlock->sortedBySizeNode );
            }
        }

        inline bool ResizeAllocation( PlatformHeapAllocator *manager, VMemAllocation *memHandle, void *memPtr, size_t newSize ) noexcept
        {
            // Cannot delete allocations this way so bail.
            if ( newSize == 0 )
                return false;

            // We do not have to update anything, so bail.
            size_t oldDataSize = memHandle->dataSize;

            if ( oldDataSize == newSize )
                return true;

            bool isGrowingAlloc = ( oldDataSize < newSize );

            // If we are the last allocation we can either shrink or grow the allocation depending on the
            // requested size.
            bool isLastNode = is_last_node( memHandle );

            // Since we know the free space after the memory handle, we can simply grow or shrink without issue.
            // The operation takes logarithmic time though, because we update the AVL tree.

            size_t startOfDataOffset = (size_t)memPtr;

            size_t newRequestedStartOfFreeBytes = ( startOfDataOffset + newSize );

            // Get the offset to the byte that is last of the available (possible) free space.
            size_t endOfFreeSpaceOffset = memHandle->freeSpaceAfterThis.freeRegion.GetSliceEndPoint();

            // If this is not a valid offset for the free bytes, we bail.
            // We add 1 because it could become empty aswell.
            // (I guess this could only be triggered if we grow memory?)
            if ( endOfFreeSpaceOffset + 1 < newRequestedStartOfFreeBytes )
            {
                // If we are the last node we could actually try to grow the island.
                if ( !isLastNode )
                {
                    return false;
                }

                FATAL_ASSERT( isGrowingAlloc );

                bool couldGrow = this->MakeFreeSpaceToTheRight( manager, &memHandle->freeSpaceAfterThis, newRequestedStartOfFreeBytes );

                if ( !couldGrow )
                {
                    // We absolutely fail.
                    return false;
                }

                // Second wind! We got more space.
            }

            // Update the meta-data.
            if ( is_free_region_empty( memHandle->freeSpaceAfterThis.freeRegion ) == false )
            {
                manager->avlFreeBlockSortedBySize.RemoveByNodeFast( &memHandle->freeSpaceAfterThis.sortedBySizeNode );
            }

            memHandle->freeSpaceAfterThis.freeRegion.SetSliceStartPoint( newRequestedStartOfFreeBytes );
            memHandle->dataSize = newSize;

            // If we are actually shrinking the allocation, we should try to truncate the virtual memory to the minimum required.
            if ( isLastNode && !isGrowingAlloc )
            {
                truncate_to_minimum_space( manager, &memHandle->freeSpaceAfterThis );
            }

            // Insert the new thing again.
            if ( is_free_region_empty( memHandle->freeSpaceAfterThis.freeRegion ) == false )
            {
                manager->avlFreeBlockSortedBySize.Insert( &memHandle->freeSpaceAfterThis.sortedBySizeNode );
            }

            return true;
        }

        inline bool HasNoAllocations( void ) const noexcept
        {
            // If there is just the first free space block, then there cannot be any allocation either.
            return ( this->firstFreeSpaceBlock.sortedByAddrNode.next == &this->sortedByAddrFreeBlocks.root );
        }

        // Returns statistics about usage of this memory island.
        struct usageStats
        {
            size_t usedBytes = 0;
            size_t usedMetaBytes = 0;
            size_t freeBytes = 0;
            size_t countOfAllocations = 0;
            size_t minSizeOfFreeSpace = 0;
            size_t maxSizeOfFreeSpace = 0;
        };

        inline usageStats GetUsageStatistics( void ) const noexcept
        {
            usageStats stats;

            bool has_minSizeOfFreeSpace = false;
            bool has_maxSizeOfFreeSpace = false;

            // Have to take the header bytes of each island as meta-bytes into account.
            // Just saying that having too many islands is not the best idea.
            stats.usedMetaBytes += sizeof( VMemIsland );

            LIST_FOREACH_BEGIN( VMemFreeBlock, this->sortedByAddrFreeBlocks.root, sortedByAddrNode )

                // If we have an allocation associated with this free block, add up the data bytes.
                if ( item != &this->firstFreeSpaceBlock )
                {
                    VMemAllocation *allocObj = LIST_GETITEM( VMemAllocation, item, freeSpaceAfterThis );

                    size_t dataSize = allocObj->dataSize;

                    stats.usedBytes += dataSize;
                    stats.usedMetaBytes += ( dataSize + allocObj->dataOff );

                    // We have one more allocation.
                    stats.countOfAllocations++;
                }

                // Count the free bytes aswell.
                size_t freeSpaceSize = item->freeRegion.GetSliceSize();

                stats.freeBytes += freeSpaceSize;

                if ( !has_minSizeOfFreeSpace || stats.minSizeOfFreeSpace > freeSpaceSize )
                {
                    stats.minSizeOfFreeSpace = freeSpaceSize;

                    has_minSizeOfFreeSpace = true;
                }

                if ( !has_maxSizeOfFreeSpace || stats.maxSizeOfFreeSpace < freeSpaceSize )
                {
                    stats.maxSizeOfFreeSpace = freeSpaceSize;

                    has_maxSizeOfFreeSpace = true;
                }

            LIST_FOREACH_END

            return stats;
        }

        AINLINE VMemAllocation* GetFirstAllocation( void ) noexcept
        {
            RwListEntry <VMemFreeBlock> *secondFreeBlockNode = this->firstFreeSpaceBlock.sortedByAddrNode.next;

            if ( secondFreeBlockNode == &this->sortedByAddrFreeBlocks.root )
            {
                return nullptr;
            }

            return LIST_GETITEM( VMemAllocation, secondFreeBlockNode, freeSpaceAfterThis.sortedByAddrNode );
        }

        AINLINE VMemFreeBlock* GetLastFreeBlock( void ) noexcept
        {
            // While free blocks with zero size do not exist in the sortedBySize AVL tree,
            // the list of address-sorted free-blocks must be complete! Thus the first free block
            // must always be inside.
#ifdef _DEBUG
            FATAL_ASSERT( LIST_EMPTY( this->sortedByAddrFreeBlocks.root ) == false );
#endif //_DEBUG

            return LIST_GETITEM( VMemFreeBlock, this->sortedByAddrFreeBlocks.root.prev, sortedByAddrNode );
        }

        AINLINE VMemAllocation* GetLastAllocation( void ) noexcept
        {
            RwListEntry <VMemFreeBlock> *lastFreeBlockNode = this->sortedByAddrFreeBlocks.root.prev;

            if ( lastFreeBlockNode == &this->firstFreeSpaceBlock.sortedByAddrNode )
            {
                return nullptr;
            }

            return LIST_GETITEM( VMemAllocation, lastFreeBlockNode, freeSpaceAfterThis.sortedByAddrNode );
        }

        DEF_LIST_ITER( freeBlockByAddrIter_t, VMemFreeBlock, sortedByAddrNode );

        // Walks all memory allocations of this island in memory-address order.
        template <typename callbackType>
        AINLINE void WalkAllocations( const callbackType& cb ) noexcept(noexcept(cb( (VMemAllocation*)nullptr )))
        {
            freeBlockByAddrIter_t iter( this->sortedByAddrFreeBlocks );

            if ( iter.IsEnd() )
                return;

            iter.Increment();

            while ( !iter.IsEnd() )
            {
                VMemAllocation *allocObj = LIST_GETITEM( VMemAllocation, iter.Resolve(), freeSpaceAfterThis );

                cb( allocObj );

                iter.Increment();
            }
        }

        RwListEntry <VMemIsland> managerNode;
        AVLNode avlSortedByAddrIslandsNode;

        PageAllocatorType::pageHandle *allocHandle;   // handle into the NativePageAllocator for meta-info

        VMemFreeBlock firstFreeSpaceBlock;      // describes the amount of memory free before any allocation.
        RwList <VMemFreeBlock> sortedByAddrFreeBlocks;
    };

    // We kinda want to keep this.
    RwList <VMemIsland> listIslands;

    DEF_LIST_REVITER( vmemIslandReverseIter_t, VMemIsland, managerNode );

    struct memisland_addr_sort_dispatcher
    {
        static AINLINE eir::eCompResult CompareNodes( const AVLNode *leftNode, const AVLNode *rightNode ) noexcept
        {
            const VMemIsland *leftIsland = AVL_GETITEM( VMemIsland, leftNode, avlSortedByAddrIslandsNode );
            const VMemIsland *rightIsland = AVL_GETITEM( VMemIsland, rightNode, avlSortedByAddrIslandsNode );

            return eir::DefaultValueCompare( leftIsland, rightIsland );
        }

        static AINLINE eir::eCompResult CompareNodeWithValue( const AVLNode *leftNode, size_t rightValue ) noexcept
        {
            const VMemIsland *leftIsland = AVL_GETITEM( VMemIsland, leftNode, avlSortedByAddrIslandsNode );

            return eir::DefaultValueCompare( (size_t)leftIsland, rightValue );
        }
    };

    AVLTree <memisland_addr_sort_dispatcher> sortedByAddrIslands;
};

typedef PlatformHeapAllocator
#ifdef __linux__
    <LinuxVirtualMemoryAccessorDefaultSyscalls>
#endif //__linux__
    NativeHeapAllocator;

#endif //_NATIVE_VIRTUAL_MEMORY_HEAP_ALLOCATOR_
