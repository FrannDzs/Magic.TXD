/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/OSUtils.h
*  PURPOSE:     Implementation dependant routines for native features
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// IMPORTANT: none of this code can use CRT functions or STL dynamic types!

#ifndef _COMMON_OS_UTILS_
#define _COMMON_OS_UTILS_

#include "MemoryRaw.h"
#include "MathSlice.algorithms.h"
#include "MemoryUtils.h"

#include "OSUtils.vmem.h"
#include "OSUtils.arrvmem.h"
#include "OSUtils.vecvmem.h"

#include <atomic>
#include <type_traits>

// Native OS memory allocation manager that marks pages on RAM to be used by the program.
// Uses the platform-dependent native virtual memory functions.
// This implementation should be used if the API exposed by the OS is not enough for you (malloc, etc).
// Due to complicated memory saving and performance reasons we do not allow intersection of page handles (anymore).
// Now with more advanced methods for expanding memory of page handles (left-expand, merge-two).
// Version 4.
#ifdef __linux__
template <typename syscallDispatcherType>
#endif //__linux__
struct PlatformPageAllocator
{
    // WARNING: this class is NOT thread-safe!

    // There is still a lot to be done for this class!
    // For the basic purpose it always guarrantees functionality under optimal situations (enough memory, etc).
    // TODO:
    // * exception safety under usage of specialized types

    struct pageHandle;

    // Amount of cached constructs ("pages" sorta) to allocate inside cached containers.
    static constexpr size_t NUM_VECTOR_PAGES_CACHED = 1;

    typedef PlatformVirtualMemoryAccessor
#ifdef __linux__
        <syscallDispatcherType>
#endif //__linux__
        VirtualMemoryAccessorType;

private:
    // Virtual memory description object.
    VirtualMemoryAccessorType vmemAccess;

    // Allocation systems that we need.
    struct pageAllocation;

    PlatformVirtualMemoryArrayAllocator <pageHandle
#ifdef __linux__
    , syscallDispatcherType
#endif
    > _allocPageHandle;
    PlatformVirtualMemoryArrayAllocator <pageAllocation
#ifdef __linux__
    , syscallDispatcherType
#endif
    > _allocPageArena;

    // It is good to keep some useful metrics.
    std::atomic <size_t> numAllocatedArenas;
    std::atomic <size_t> numAllocatedPageHandles;

public:
    inline PlatformPageAllocator( void )
        : _allocPageHandle( vmemAccess ), _allocPageArena( vmemAccess ), numAllocatedArenas( 0 ), numAllocatedPageHandles( 0 ),
          flowalloc_temp_alloc_cinfo( vmemAccess ),
          _cached_memReserveList( vmemAccess )
    {
        return;
    }

    inline ~PlatformPageAllocator( void )
    {
        // Delete all active page handles.
        while ( !LIST_EMPTY( sortedActiveHandles.root ) )
        {
            pageHandle *handle = LIST_GETITEM( pageHandle, sortedActiveHandles.root.next, managerNode );

            Free( handle );
        }

        // Now delete any active pages.
        while ( !LIST_EMPTY( activeMemoryRanges.root ) )
        {
            pageAllocation *allocation = LIST_GETITEM( pageAllocation, activeMemoryRanges.root.next, managerNode );

            DeletePageAllocation( allocation );
        }
    }

    // We are not copy-constructible because it makes no sense.
    // Instead we can be moved.
    inline PlatformPageAllocator( const PlatformPageAllocator& right ) = delete;
    inline PlatformPageAllocator( PlatformPageAllocator&& right ) noexcept
        : _allocPageHandle( std::move( right._allocPageHandle ) ),
          _allocPageArena( std::move( right._allocPageArena ) ),
          flowalloc_temp_alloc_cinfo( std::move( right.flowalloc_temp_alloc_cinfo ) ),
          _cached_memReserveList( std::move( right._cached_memReserveList ) )
    {
        // Take over all the allocations and stuff.
        this->sortedActiveHandles = std::move( right.sortedActiveHandles );
        this->activeMemoryRanges = std::move( right.activeMemoryRanges );
        this->numAllocatedArenas = right.numAllocatedArenas.load();
        this->numAllocatedPageHandles = right.numAllocatedPageHandles.load();
        this->sortedMemoryRanges = std::move( right.sortedMemoryRanges );

        // Must update the vmem accessor in the internal vectors we store.
        this->flowalloc_temp_alloc_cinfo.SetNativeVirtualMemoryAccessor( this->vmemAccess );
        this->_cached_memReserveList.SetNativeVirtualMemoryAccessor( this->vmemAccess );

        // Must update manager pointers in the allocations.
        LIST_FOREACH_BEGIN( pageAllocation, this->activeMemoryRanges.root, managerNode )

            item->manager = this;

        LIST_FOREACH_END

        // Move platform specific things.
        // TODO: moving this member away could leave the source struct invalid; will we actually use a copy here?
        this->vmemAccess = std::move( right.vmemAccess );

        // Clear the old struct somewhat.
        right.numAllocatedArenas = 0;
        right.numAllocatedPageHandles = 0;
    }

    inline PlatformPageAllocator& operator = ( const PlatformPageAllocator& right ) = delete;
    inline PlatformPageAllocator& operator = ( PlatformPageAllocator&& right ) noexcept
    {
        this->~PlatformPageAllocator();

        return *new (this) PlatformPageAllocator( std::move( right ) );
    }

    struct pageHandle;

private:
    typedef sliceOfData <size_t> memBlockSlice_t;

    struct pageAllocation;

public:
    // To associate page handles with page arenas, from version 3 on, we make use of memory address immutability guarantees.
    // This avoids having to use lists of memory handles for each arena, simplifying the memory layout A LOT.
    // It works using the following attributes:
    // * pageHandles do not intersect themselves.
    // * pageArenas do not intersect themselves.
    // * pageHandles have a valid global ordering by memory address
    // So we store the first pageHandle that is valid for each arena and list the associated handles from then on, with minimal
    // runtime overhead.
    struct pageHandle
    {
        friend struct PlatformPageAllocator;

        inline pageHandle( memBlockSlice_t spanSlice )
            : requestedMemory( std::move( spanSlice ) )
        {
            this->begResiding = nullptr;
        }

        inline ~pageHandle( void )
        {
            return;
        }

        inline void* GetTargetPointer( void ) const noexcept            { return (void*)requestedMemory.GetSliceStartPoint(); }
        inline size_t GetTargetSize( void ) const noexcept              { return (size_t)requestedMemory.GetSliceSize(); }

        inline memBlockSlice_t GetTargetSlice( void ) const noexcept    { return this->requestedMemory; }

    private:
        memBlockSlice_t requestedMemory;    // slice that represents memory that can be accessed by the application

        pageAllocation *begResiding;    // first memory arena that this handle is part of (must not be null)

        RwListEntry <pageHandle> managerNode;   // entry in the active page handle list, has to be sorted by address!
    };

private:
    // List of all active page handles in memory-order.
    // We need this feature to support listing for arenas without allocating additional memory.
    RwList <pageHandle> sortedActiveHandles;

    // An arena that spans multiple pages.
    struct pageAllocation
    {
        inline pageAllocation( PlatformPageAllocator *manager, void *arenaAddress, size_t numSlots ) noexcept
        {
            size_t arenaSpanSize = (size_t)( numSlots * manager->vmemAccess.GetPlatformPageSize() );

            pageSpan = memBlockSlice_t( (size_t)arenaAddress, arenaSpanSize );

            this->manager = manager;

            this->arenaAddress = arenaAddress;
            this->allocSize = arenaSpanSize;

            this->refCount = 0;

            this->begResideHandle = nullptr;    // does not have to be not-null.

            this->slotCount = numSlots;
        }

        inline ~pageAllocation( void )
        {
            // Make sure nobody uses us anymore.
            FATAL_ASSERT( this->refCount == 0 );

            // Release the allocated arena.
            this->manager->vmemAccess.ReleaseVirtualMemory( this->arenaAddress, this->allocSize );
        }

        // Iterator across page handles in memory-order to support cancellation points at any situation.
        struct sortedPageResidentIterator
        {
            AINLINE sortedPageResidentIterator( PlatformPageAllocator *manager, pageAllocation *arenaHandle ) noexcept
            {
                this->manager = manager;
                this->arenaHandle = arenaHandle;

                // If the arena handle has no page handles, then we just set our node to the end of list.
                pageHandle *begPageResident = arenaHandle->begResideHandle;

                if ( begPageResident == nullptr )
                {
                    this->node = &manager->sortedActiveHandles.root;
                }
                else
                {
                    this->node = &begPageResident->managerNode;
                }
            }
            AINLINE sortedPageResidentIterator( sortedPageResidentIterator&& ) = delete;
            AINLINE sortedPageResidentIterator( const sortedPageResidentIterator& ) = delete;

            AINLINE ~sortedPageResidentIterator( void )
            {
                // Nothing to do.
            }

            AINLINE sortedPageResidentIterator& operator = ( sortedPageResidentIterator&& ) = delete;
            AINLINE sortedPageResidentIterator& operator = ( const sortedPageResidentIterator& ) = delete;

            AINLINE bool IsEnd( void ) const noexcept
            {
                auto node = this->node;

                if ( node == &this->manager->sortedActiveHandles.root )
                {
                    return true;
                }

                // We are at the end if the current page handle floats after the given arena.
                pageHandle *curHandle = LIST_GETITEM( pageHandle, node, managerNode );
                pageAllocation *arenaHandle = this->arenaHandle;

                eir::eIntersectionResult intResult =
                    curHandle->requestedMemory.intersectWith( arenaHandle->pageSpan );

                FATAL_ASSERT( intResult != eir::INTERSECT_FLOATING_START );

                return ( intResult == eir::INTERSECT_FLOATING_END );
            }

            AINLINE void Increment( void ) noexcept
            {
                this->node = this->node->next;
            }

            AINLINE pageHandle* Resolve( void ) const noexcept
            {
                return LIST_GETITEM( pageHandle, this->node, managerNode );
            }

        private:
            PlatformPageAllocator *manager;
            pageAllocation *arenaHandle;
            RwListEntry <pageHandle> *node;
        };

        // Get all page handles that intersect an arena in memory-order.
        template <typename callbackType, typename... Args>
        AINLINE void ForAllPageHandlesSorted( Args... theArgs ) noexcept(noexcept(callbackType::ProcessEntry( nullptr, 0, theArgs... )))
        {
            sortedPageResidentIterator iter( this->manager, this );

            size_t sortedIndex = 0;

            while ( !iter.IsEnd() )
            {
                // Grab the handle.
                pageHandle *curHandle = iter.Resolve();

                // We have another entry in our thing.
                callbackType::ProcessEntry( curHandle, sortedIndex++, theArgs... );

                // Next one.
                iter.Increment();
            }
        }

        // DEBUG.
        inline void CheckForCollision( const RwList <pageHandle>& sortedHandles, const memBlockSlice_t& memoryRegion ) noexcept
        {
#if defined(_DEBUG) && defined(_WIN32)
            // DEBUG: check for intersection, meow.
            struct checkCollHandler
            {
                static AINLINE void ProcessEntry( pageHandle *foreignHandle, size_t sortedIndex, const memBlockSlice_t& memoryRegion )
                {
                    eir::eIntersectionResult intRes = foreignHandle->requestedMemory.intersectWith( memoryRegion );

                    if ( eir::isFloatingIntersect( intRes ) == false )
                    {
                        // Oh no :(
                        __debugbreak();
                    }
                }
            };

            ForAllPageHandlesSorted <checkCollHandler> ( memoryRegion );
#endif // DEBUG CODE.
        }

        // Each page handle that resides on an arena has to reference it.
        // We used to have a list but we cannot afford the memory associated with it anymore.
        inline void RefPageHandle( void ) noexcept
        {
            this->refCount++;
        }

        inline void DerefPageHandle( void ) noexcept
        {
            this->refCount--;
        }

        inline bool IsBlockBeingUsed( void ) const noexcept
        {
            return ( refCount != 0 );
        }

        inline void RemovePossibleFirst( PlatformPageAllocator *manager, pageHandle *theHandle ) noexcept
        {
            if ( theHandle != this->begResideHandle )
                return;

            sortedPageResidentIterator iter( manager, this );

            // Increment.
            iter.Increment();

            if ( iter.IsEnd() )
            {
                this->begResideHandle = nullptr;
            }
            else
            {
                this->begResideHandle = iter.Resolve();
            }
        }

        PlatformPageAllocator *manager;

        void *arenaAddress;         // address of the memory arena
        size_t allocSize;           // number in bytes for the allocation range

        memBlockSlice_t pageSpan;       // slice which spans the allocation range

        unsigned int refCount;          // number of handles using this page

        pageHandle *begResideHandle;    // first handle that intersects with this arena

        size_t slotCount;           // amount of slots we have space for.

        RwListEntry <pageAllocation> managerNode;   // node in the NativePageAllocator allocation list.
        RwListEntry <pageAllocation> sortedNode;
    };

    inline size_t GetAllocationArenaRange( size_t spanSize ) const noexcept
    {
        // Returns a rounded up value that determines region of RESERVE allocation.
        size_t allocGranularity = this->vmemAccess.GetPlatformAllocationGranularity();

        return ALIGN( spanSize, allocGranularity, allocGranularity );
    }

    inline size_t GetPageAllocationRange( size_t spanSize ) const noexcept
    {
        // Returns a rounded up value that determines the actual size of a page allocation.
        size_t pageSize = this->vmemAccess.GetPlatformPageSize();

        return ALIGN( spanSize, pageSize, pageSize );
    }

    RwList <pageAllocation> activeMemoryRanges;
    RwList <pageAllocation> sortedMemoryRanges;

    inline void SortedMemoryBlockInsert( pageAllocation *memBlock ) noexcept
    {
        RwListEntry <pageAllocation> *insertAfter = &sortedMemoryRanges.root;

        size_t insertMemBlockAddress = (size_t)memBlock->arenaAddress;

        LIST_FOREACH_BEGIN( pageAllocation, sortedMemoryRanges.root, sortedNode )
            // Get the address of the list item as number.
            size_t memBlockAddress = (size_t)item->arenaAddress;

            if ( memBlockAddress > insertMemBlockAddress )
            {
                insertAfter = iter;
                break;
            }
        LIST_FOREACH_END

        LIST_APPEND( *insertAfter, memBlock->sortedNode );
    }

    template <typename callbackType, typename... Args>
    AINLINE static void ProcessInclinedMemoryChunk(
        pageAllocation *arenaHandle, const memBlockSlice_t& memoryRegion, Args&&... theArgs
    ) noexcept(noexcept(callbackType::Process(memBlockSlice_t(), std::forward <Args> ( theArgs )... )))
    {
        memBlockSlice_t sharedSlice;

        bool hasPosition = memoryRegion.getSharedRegion( arenaHandle->pageSpan, sharedSlice );

        if ( hasPosition )
        {
            // Tell the meow.
            callbackType::Process( sharedSlice, std::forward <Args> ( theArgs )... );
        }
    }

    template <typename callbackType, typename... Args>
    AINLINE void ForAllPageHandleArenasSorted(
        pageHandle *arenaResident, Args&&... theArgs
    ) noexcept(noexcept(callbackType::ProcessEntry( nullptr, std::forward <Args> ( theArgs )... )))
    {
        pageAllocation *curArena = arenaResident->begResiding;

        // cannot be nullptr because page handles have to be placed on arenas.
        FATAL_ASSERT( curArena != nullptr );

        RwListEntry <pageAllocation> *node = &curArena->sortedNode;

        size_t sortedIndex = 0;

        while ( node != &this->sortedMemoryRanges.root )
        {
            // The first arena that is floating after the page handle is the end marker.
            eir::eIntersectionResult intResult = curArena->pageSpan.intersectWith( arenaResident->requestedMemory );

            FATAL_ASSERT( intResult != eir::INTERSECT_FLOATING_START );

            if ( intResult == eir::INTERSECT_FLOATING_END )
            {
                // End marker.
                break;
            }

            // Cache the next.
            RwListEntry <pageAllocation> *cachedNext = node->next;

            // Process current valid item.
            callbackType::ProcessEntry( curArena, std::forward <Args> ( theArgs )... );

            // Next.
            node = cachedNext;

            if ( node == &this->sortedMemoryRanges.root )
            {
                break;
            }

            sortedIndex++;

            curArena = LIST_GETITEM( pageAllocation, node, sortedNode );
        }
    }

    template <typename callbackType, typename... Args>
    AINLINE void SortedProcessMemoryChunks( pageHandle *arenaResident, const memBlockSlice_t& memoryRegion, Args... theArgs ) noexcept
    {
        struct sortedMemoryProcessor
        {
            static AINLINE void ProcessEntry( pageAllocation *arenaHandle, const memBlockSlice_t& memoryRegion, Args... theArgs ) noexcept
            {
                // Check what this allocation has to say.
                ProcessInclinedMemoryChunk <callbackType> ( arenaHandle, memoryRegion, theArgs... );
            }
        };

        ForAllPageHandleArenasSorted <sortedMemoryProcessor> ( arenaResident, memoryRegion, theArgs... );

        // Done.
    }

    struct arenaCommitOperator
    {
        static AINLINE void Process( const memBlockSlice_t& allocRegion ) noexcept
        {
            void *mem_ptr = (void*)allocRegion.GetSliceStartPoint();
            size_t mem_size = allocRegion.GetSliceSize();

            bool success = NativeVirtualMemoryAccessor::CommitVirtualMemory( mem_ptr, mem_size );
            (void)success;

            FATAL_ASSERT( success == true );
        }
    };

    inline void CommitMemoryOfPageHandle( pageHandle *theHandle, const memBlockSlice_t& commitRegion ) noexcept
    {
        SortedProcessMemoryChunks <arenaCommitOperator> ( theHandle, commitRegion );
    }

    struct arenaDecommitOperator
    {
        static AINLINE void Process( const memBlockSlice_t& allocRegion ) noexcept
        {
            void *mem_ptr = (void*)allocRegion.GetSliceStartPoint();
            size_t mem_size = allocRegion.GetSliceSize();

            bool success = NativeVirtualMemoryAccessor::DecommitVirtualMemory( mem_ptr, mem_size );
            (void)success;

            FATAL_ASSERT( success == true );
        }
    };

    inline void DecommitMemoryOfPageHandle( pageHandle *theHandle, const memBlockSlice_t& decommitRegion ) noexcept
    {
        SortedProcessMemoryChunks <arenaDecommitOperator> ( theHandle, decommitRegion );
    }

    inline pageAllocation* NewArenaAllocation( void *arenaAddress, size_t numSlots ) noexcept
    {
        this->numAllocatedArenas++;

        return _allocPageArena.Allocate( this, arenaAddress, numSlots );
    }

    inline void FreeArenaAllocation( pageAllocation *arenaPtr ) noexcept
    {
        this->numAllocatedArenas--;

        _allocPageArena.Deallocate( arenaPtr );
    }

    inline void DeletePageAllocation( pageAllocation *memRange ) noexcept
    {
        LIST_REMOVE( memRange->sortedNode );
        LIST_REMOVE( memRange->managerNode );

        FreeArenaAllocation( memRange );
    }

    struct memReserveAllocInfo
    {
        AINLINE memReserveAllocInfo( void ) = default;
        AINLINE memReserveAllocInfo( memReserveAllocInfo&& ) = default;
        AINLINE memReserveAllocInfo( const memReserveAllocInfo& ) = default;

        AINLINE memReserveAllocInfo& operator = ( memReserveAllocInfo&& ) = default;
        AINLINE memReserveAllocInfo& operator = ( const memReserveAllocInfo& ) = default;

        pageAllocation *hostArena;
        bool hostArenaIsFirstSpot;
        bool isHostArenaNewlyAllocated;
    };

    // Use the virtual-memory-based vector class with caching here.
    typedef PlatformVirtualMemoryVector <memReserveAllocInfo
#ifdef __linux__
        , syscallDispatcherType
#endif //__linux__
        , NUM_VECTOR_PAGES_CACHED> memCachedReserveAllocList_t;

    DEF_LIST_ITER( arenaSortedIterator_t, pageAllocation, sortedNode );
    DEF_LIST_ITER( pageHandleSortedIterator_t, pageHandle, managerNode );

    inline pageAllocation* ReserveNewMemory( void *allocStartAddr, size_t allocSize ) noexcept
    {
        FATAL_ASSERT( allocSize == GetAllocationArenaRange( allocSize ) );

        void *allocPtr = this->vmemAccess.RequestVirtualMemory( (void*)allocStartAddr, allocSize );

        if ( allocPtr == nullptr )
            return nullptr;

        pageAllocation *newHostArena = NewArenaAllocation( allocPtr, allocSize / this->vmemAccess.GetPlatformPageSize() );

        if ( !newHostArena )
        {
            this->vmemAccess.ReleaseVirtualMemory( allocPtr, allocSize );
            return nullptr;
        }

        return newHostArena;
    }

    // Temporary allocation data.
    struct temp_alloc_info_commit
    {
        AINLINE temp_alloc_info_commit( void ) = default;
        AINLINE temp_alloc_info_commit( temp_alloc_info_commit&& ) = default;
        AINLINE temp_alloc_info_commit( const temp_alloc_info_commit& ) = default;

        AINLINE temp_alloc_info_commit& operator = ( temp_alloc_info_commit&& ) = default;
        AINLINE temp_alloc_info_commit& operator = ( const temp_alloc_info_commit& ) = default;

        pageAllocation *arenaToBeCommitted;
        RwListEntry <pageAllocation> *appendAfterNode;
    };

    // The real reason we put vectors into the class itself is so that we can efficiently use the caching feature.
    typedef PlatformVirtualMemoryVector <temp_alloc_info_commit
#ifdef __linux__
        , syscallDispatcherType
#endif //__linux__
        , NUM_VECTOR_PAGES_CACHED> flowalloc_temp_alloc_cinfo_array_t;

    flowalloc_temp_alloc_cinfo_array_t flowalloc_temp_alloc_cinfo;

    // The iterator type that is used for memory arena exploration.
    struct sortedSliceIteratorType
    {
        AINLINE sortedSliceIteratorType( void ) = default;
        AINLINE sortedSliceIteratorType( arenaSortedIterator_t iter ) noexcept : iter( std::move( iter ) )
        {
            return;
        }

        AINLINE sortedSliceIteratorType( sortedSliceIteratorType&& ) = default;
        AINLINE sortedSliceIteratorType( const sortedSliceIteratorType& ) = default;

        AINLINE sortedSliceIteratorType& operator = ( sortedSliceIteratorType&& ) = default;
        AINLINE sortedSliceIteratorType& operator = ( const sortedSliceIteratorType& ) = default;

        AINLINE bool IsEnd( void ) const noexcept       { return iter.IsEnd(); }
        AINLINE void Increment( void ) noexcept         { iter.Increment(); }
        AINLINE void Decrement( void ) noexcept         { iter.Decrement(); }

        AINLINE memBlockSlice_t GetCurrentRegion( void ) const noexcept
        {
            const pageAllocation *cur_arena = this->iter.Resolve();

            return cur_arena->pageSpan;
        }

        AINLINE pageAllocation* GetCurrentArena( void ) const noexcept
        {
            return this->iter.Resolve();
        }

        AINLINE RwListEntry <pageAllocation>* GetCurrentNode( void ) const noexcept
        {
            return this->iter.GetNode();
        }

    private:
        arenaSortedIterator_t iter;
    };

    // RAII helper.
    struct temp_alloc_cinfo_context
    {
        AINLINE temp_alloc_cinfo_context( flowalloc_temp_alloc_cinfo_array_t& tmpArray ) noexcept : tmpArray( tmpArray )
        {
            return;
        }

        AINLINE ~temp_alloc_cinfo_context( void )
        {
            // Reset the array.
            // Screw this hyper retarded repulsion of the "finally" keyword :/
            tmpArray.Clear();
        }

    private:
        flowalloc_temp_alloc_cinfo_array_t& tmpArray;
    };

    // This function is NOT RECURSIVE and NOT THREAD SAFE.
    template <bool trueForwardFalseBackward, typename intervalCutIteratorType>
    AINLINE bool _FlowAllocateIntervalProcessing(
        memCachedReserveAllocList_t& areaToBeAllocatedAt_inOut,
        intervalCutIteratorType arena_iter, const memBlockSlice_t& handleAllocRegion
    ) noexcept
    {
        // Remember to reset the flowalloc temp struct.
        temp_alloc_cinfo_context ctx( this->flowalloc_temp_alloc_cinfo );

        memBlockSlice_t arena_region;
        bool isPresentArena;
        sortedSliceIteratorType arena_iter_res;

        bool allocSuccess = true;

        // Initialized first time when required.
        RwListEntry <pageAllocation> *prevArenaNode = nullptr;

        // Optimization because page handles can overlap arenas pretty often.
        pageHandle *lastCollisionCheckSorted = nullptr;
        eir::eIntersectionResult lastCollisionCheckSorted_intRes = eir::INTERSECT_UNKNOWN;  // had to do this because stupid GCC keeps complaining (warning flood in build log).

        while ( arena_iter.FetchNext( arena_region, isPresentArena, arena_iter_res ) )
        {
            memReserveAllocInfo alloc_at_info;

            if ( isPresentArena )
            {
                pageAllocation *allocate_at_arena = arena_iter_res.GetCurrentArena();

                // Check for collision against any pageHandle.
                typename pageAllocation::sortedPageResidentIterator handle_iter( this, allocate_at_arena );

                bool found_prior_handle = false;

                while ( handle_iter.IsEnd() == false )
                {
                    pageHandle *handle = handle_iter.Resolve();

                    eir::eIntersectionResult intRes;

                    if ( lastCollisionCheckSorted != nullptr && lastCollisionCheckSorted == handle )
                    {
                        intRes = lastCollisionCheckSorted_intRes;
                    }
                    else
                    {
                        intRes = handleAllocRegion.intersectWith( handle->GetTargetSlice() );

                        lastCollisionCheckSorted = handle;
                        lastCollisionCheckSorted_intRes = intRes;
                    }

                    if ( intRes == eir::INTERSECT_FLOATING_START )
                    {
                        break;
                    }

                    if ( intRes != eir::INTERSECT_FLOATING_END )
                    {
                        allocSuccess = false;
                        goto end_of_alloc_loop;
                    }

                    found_prior_handle = true;

                    handle_iter.Increment();
                }

                // We are safe to allocate on this arena.
                alloc_at_info.hostArena = allocate_at_arena;
                alloc_at_info.isHostArenaNewlyAllocated = false;
                alloc_at_info.hostArenaIsFirstSpot = ( found_prior_handle == false );
            }
            else
            {
                // Try to allocate the space that is left empty.
                pageAllocation *new_arena = ReserveNewMemory( (void*)arena_region.GetSliceStartPoint(), arena_region.GetSliceSize() );

                if ( new_arena == nullptr )
                {
                    allocSuccess = false;
                    break;
                }

                // Need to allocate at this new arena.
                alloc_at_info.hostArena = new_arena;
                alloc_at_info.isHostArenaNewlyAllocated = true;
                alloc_at_info.hostArenaIsFirstSpot = true;

                // Simple fetch if not initialized yet.
                if ( prevArenaNode == nullptr )
                {
                    if constexpr ( trueForwardFalseBackward )
                    {
                        prevArenaNode = arena_iter_res.GetCurrentNode()->prev;
                    }
                    else
                    {
                        prevArenaNode = arena_iter_res.GetCurrentNode()->next;
                    }
                }

                // Remember stuff for committing into the system on success.
                temp_alloc_info_commit commit_info;
                commit_info.arenaToBeCommitted = new_arena;
                commit_info.appendAfterNode = prevArenaNode;    // on forward this is append-after, on backward this is insert-before.

                bool couldAdd = flowalloc_temp_alloc_cinfo.AddItem( std::move( commit_info ) );

                if ( couldAdd == false )
                {
                    FreeArenaAllocation( new_arena );

                    allocSuccess = false;
                    break;
                }
            }

            pageAllocation *allocate_at_arena = alloc_at_info.hostArena;

            bool couldAdd = areaToBeAllocatedAt_inOut.AddItem( std::move( alloc_at_info ) );

            if ( couldAdd == false )
            {
                allocSuccess = false;
                break;
            }

            prevArenaNode = &allocate_at_arena->sortedNode;
        }

    end_of_alloc_loop:
        if ( !allocSuccess )
        {
            // Clean up after ourselves.
            // Those arenas never accounted to anything anyway.
            flowalloc_temp_alloc_cinfo.ForAllEntries(
                [&]( const temp_alloc_info_commit& info ) noexcept
            {
                pageAllocation *arena = info.arenaToBeCommitted;

                FreeArenaAllocation( arena );
            });

            // Have to reset the contents of the in-out array.
            areaToBeAllocatedAt_inOut.Clear();

            return false;
        }

        // Commit the change to the system.
        flowalloc_temp_alloc_cinfo.ForAllEntries(
            [&]( const temp_alloc_info_commit& info ) noexcept
        {
            pageAllocation *arena = info.arenaToBeCommitted;

            if constexpr ( trueForwardFalseBackward )
            {
                LIST_INSERT( *info.appendAfterNode, arena->sortedNode );
            }
            else
            {
                LIST_APPEND( *info.appendAfterNode, arena->sortedNode );
            }
            LIST_APPEND( this->activeMemoryRanges.root, arena->managerNode );
        });

        // Return the arena handles where the memory request should take place at
        // They are placed inside the in-out array.

        return true;
    }

    AINLINE memBlockSlice_t _CalculateArenaSliceFromPageRegion( const memBlockSlice_t& handleAllocRegion ) noexcept
    {
        // Calculate the hostAllocRegion, which is the arena-based important area for handleAllocRegion.
        size_t allocGranularity = this->vmemAccess.GetPlatformAllocationGranularity();

        const size_t arenaAllocStart = SCALE_DOWN( handleAllocRegion.GetSliceStartPoint(), allocGranularity );
        const size_t arenaAllocEnd = ALIGN_SIZE( handleAllocRegion.GetSliceEndPoint() + 1, allocGranularity );

        size_t arenaAllocSize = ( arenaAllocEnd - arenaAllocStart );

        memBlockSlice_t hostAllocRegion( arenaAllocStart, arenaAllocSize );

        return hostAllocRegion;
    }

    // This function is NOT RECURSIVE and NOT THREAD SAFE.
    inline bool FlowAllocateAfterRegion(
        memCachedReserveAllocList_t& areaToBeAllocatedAt_inOut, arenaSortedIterator_t reserveArenaIter,
        const memBlockSlice_t& handleAllocRegion ) noexcept
    {
        memBlockSlice_t hostAllocRegion = _CalculateArenaSliceFromPageRegion( handleAllocRegion );

        eir::MathSliceHelpers::intervalCutForwardIterator <sortedSliceIteratorType, size_t> arena_iter(
            std::move( reserveArenaIter ),
            std::move( hostAllocRegion ),
            true, false
        );

        return _FlowAllocateIntervalProcessing <true> ( areaToBeAllocatedAt_inOut, std::move( arena_iter ), handleAllocRegion );
    }

    // This function is NOT RECURSIVE and NOT THREAD SAFE.
    // areaToBeAllocatedAt_inOut is filled in reverse-memory-order.
    inline bool FlowAllocateAfterRegionBackward(
        memCachedReserveAllocList_t& areaToBeAllocatedAt_inOut, arenaSortedIterator_t reserveArenaIter,
        const memBlockSlice_t& handleAllocRegion ) noexcept
    {
        memBlockSlice_t hostAllocRegion = _CalculateArenaSliceFromPageRegion( handleAllocRegion );

        eir::MathSliceHelpers::intervalCutBackwardIterator <sortedSliceIteratorType, size_t> arena_iter(
            std::move( reserveArenaIter ),
            std::move( hostAllocRegion ),
            true, false
        );

        return _FlowAllocateIntervalProcessing <false> ( areaToBeAllocatedAt_inOut, std::move( arena_iter ), handleAllocRegion );
    }

    inline bool FindSortedMemoryHandleInsertionSpot( pageAllocation *arenaHandle, const memBlockSlice_t& memRegion, bool& isFirstOut ) noexcept
    {
        // In order to even allocate, the memory region must intersect with the arena's.
        // We assume that this is always the case.
#ifdef _DEBUG
        {
            eir::eIntersectionResult intResult = memRegion.intersectWith( arenaHandle->pageSpan );

            FATAL_ASSERT( eir::isFloatingIntersect( intResult ) == false );
        }
#endif //_DEBUG

        typename pageAllocation::sortedPageResidentIterator iter( this, arenaHandle );

        bool isFirst = true;

        while ( !iter.IsEnd() )
        {
            pageHandle *alloc = iter.Resolve();

            // Check what to make of this.
            eir::eIntersectionResult intResult = memRegion.intersectWith( alloc->requestedMemory );

            if ( intResult == eir::INTERSECT_FLOATING_START )
            {
                // Our requested memory does not conflict with anything and is prior to the current thing.
                isFirstOut = isFirst;
                return true;
            }
            if ( intResult == eir::INTERSECT_FLOATING_END )
            {
                // We can continue ahead.
            }
            else
            {
                // There was some sort of collision, which is bad.
                return false;
            }

            isFirst = false;

            // Next one.
            iter.Increment();
        }

        // We did not collide, so we are good.
        isFirstOut = isFirst;
        return true;
    }

    inline static bool IsAllocationObstructed( const memBlockSlice_t& handleAllocSlice, pageHandle *obstructAlloc ) noexcept
    {
        // Check if we are obstructed by the (next) resident memory.
        // This does not guarrentee allocability on its own, but it gives us a good idea.

        eir::eIntersectionResult intResult = handleAllocSlice.intersectWith( obstructAlloc->requestedMemory );

        return ( eir::isFloatingIntersect( intResult ) == false );
    }

    // Try placement of memory allocation on a specific memory address.
    inline bool PlaceMemoryRequest( memBlockSlice_t handleMemSlice, memCachedReserveAllocList_t& allocOut ) noexcept
    {
        arenaSortedIterator_t arena_iter( this->sortedMemoryRanges );

        // Just allocate stuff. It already searches for the correct position itself.
        return FlowAllocateAfterRegion( allocOut, std::move( arena_iter ), handleMemSlice );
    }

    // Find and allocate required memory, if possible.
    inline bool SearchForReservedMemory( size_t memSize, memBlockSlice_t& handleAllocSliceOut, memCachedReserveAllocList_t& allocOut ) noexcept
    {
        // We have to scan all reserved and/or committed memory for space that we can use.
        // This is so that we can reuse as most memory as possible.
        // If this fails we go ahead and ask Windows itself for new memory arenas.

        memBlockSlice_t handleAllocSlice( (size_t)0, memSize );

        // TODO: check if this is still correct over here.
        //  We really need unit tests!
        //size_t alignedMemSize = GetAllocationArenaRange( memSize );

        arenaSortedIterator_t sortedIter( this->sortedMemoryRanges );

        if ( !sortedIter.IsEnd() )
        {
            pageAllocation *currentArena = sortedIter.Resolve();
            RwListEntry <pageHandle> *sortedNextHandleIter = this->sortedActiveHandles.root.next;
            bool hasNextPageHandle = ( sortedNextHandleIter != &this->sortedActiveHandles.root );

            handleAllocSlice.SetSlicePosition( (size_t)currentArena->arenaAddress );

            do
            {
                // Note that an optimization behavior in this function is that we allocate at maximum free space.
                // When we tried and failed at maximum free space, we skip the entire space!
                // This is perfectly valid under the fact that memory allocation establishes one block of contiguous memory.

                // Check if there is an obstruction in the next-in-line item.
                // Because we are address-sorted, this is a very fast operation.
                // If obstructed we can optimize the forward-movement of the iterator.
                bool isCurrentAllocationSpotObstructed = false;

                while ( hasNextPageHandle )
                {
                    pageHandle *obstructAlloc = LIST_GETITEM( pageHandle, sortedNextHandleIter, managerNode );

                    // Is the selected allocation spot available?
                    // We skip any memory blocks entirely before the handle region.
                    eir::eIntersectionResult intResult =
                        obstructAlloc->requestedMemory.intersectWith( handleAllocSlice );

                    if ( intResult != eir::INTERSECT_FLOATING_START )
                    {
                        isCurrentAllocationSpotObstructed = ( intResult != eir::INTERSECT_FLOATING_END );
                        break;
                    }

                    // Move to the next memory block.
                    sortedNextHandleIter = sortedNextHandleIter->next;

                    hasNextPageHandle = ( sortedNextHandleIter != &this->sortedActiveHandles.root );
                }

                // If we are obstructing, then we must go on.
                // Otherwise we enter this condition.
                if ( !isCurrentAllocationSpotObstructed )
                {
                    // Try performing a normal allocation on this space.
                    arenaSortedIterator_t arena_iter( this->sortedMemoryRanges, &currentArena->sortedNode );

                    bool couldAllocate =
                        FlowAllocateAfterRegion( allocOut, arena_iter, handleAllocSlice );

                    if ( couldAllocate )
                    {
                        // We are successful, so return the allocation place.
                        handleAllocSliceOut = std::move( handleAllocSlice );
                        return true;
                    }
                }

                // Advance the current allocation attempt.
                // For that we have to check if there is a next memory location to try.
                // IMPORTANT: the next location _must_ be valid!
                {
                    // If we have no next page handle, we just advance the arena.
                    bool doAdvanceArena = false;
                    bool doAdvancePageHandle = false;
                    bool startJustAfterPageMem = true;

                    size_t nextTryPos;

                    if ( !hasNextPageHandle )
                    {
                        doAdvanceArena = true;
                        startJustAfterPageMem = false;
                    }
                    else
                    {
                        if ( isCurrentAllocationSpotObstructed )
                        {
                            // We simply skip the current allocation block.
                            doAdvancePageHandle = true;
                        }
                        else
                        {
                            doAdvanceArena = true;
                            startJustAfterPageMem = false;
                        }
                    }

                    if ( doAdvanceArena )
                    {
                        sortedIter.Increment();

                        if ( sortedIter.IsEnd() )
                        {
                            // If there is no more location to try for allocation, we simply fail out of
                            // our search for shared memory allocations. We will directly ask the OS for
                            // memory next.
                            break;
                        }

                        currentArena = sortedIter.Resolve();
                    }

                    if ( startJustAfterPageMem )
                    {
                        FATAL_ASSERT( hasNextPageHandle == true );

                        pageHandle *nextMem = LIST_GETITEM( pageHandle, sortedNextHandleIter, managerNode );

                        nextTryPos = ( nextMem->requestedMemory.GetSliceEndPoint() + 1 );
                    }
                    else
                    {
                        nextTryPos = ( currentArena->pageSpan.GetSliceStartPoint() );
                    }

                    if ( doAdvancePageHandle )
                    {
                        FATAL_ASSERT( sortedNextHandleIter != &this->sortedActiveHandles.root );

                        sortedNextHandleIter = sortedNextHandleIter->next;

                        hasNextPageHandle = ( sortedNextHandleIter != &this->sortedActiveHandles.root );
                    }

                    // Set the next try pos.
                    handleAllocSlice.SetSlicePosition( nextTryPos );
                }

                // Normalize the arena.
                // The page handle will be normalized on loop beginning.
                while ( true )
                {
                    eir::eIntersectionResult intResult =
                        currentArena->pageSpan.intersectWith( handleAllocSlice );

                    if ( intResult != eir::INTERSECT_FLOATING_START )
                    {
                        // There is an intersection, so we are ok.
                        break;
                    }

                    sortedIter.Increment();

                    if ( sortedIter.IsEnd() )
                    {
                        // No more arenas.
                        goto failure;
                    }

                    currentArena = sortedIter.Resolve();
                }

                // If the allocation start is before the arena start, we set it to the arena start.
                size_t arenaStartOff = currentArena->pageSpan.GetSliceStartPoint();

                if ( handleAllocSlice.GetSliceStartPoint() < arenaStartOff )
                {
                    handleAllocSlice.SetSlicePosition( arenaStartOff );
                }
            }
            while ( true );
        }

    failure:
        // We just failed.
        return false;
    }

    static inline bool IsValidAllocation( void *desiredAddress, size_t spanSize ) noexcept
    {
        bool isValid = true;

        if ( desiredAddress != nullptr )
        {
            // Check that there is no number overflow.
            size_t memDesiredAddress = (size_t)desiredAddress;
            size_t memSpanSize = spanSize;

            size_t memAddressBorder = ( memDesiredAddress + memSpanSize );

            // The condition I check here is that if I add those two numbers,
            // the result must be bigger than the source operand I added to.
            isValid = ( memAddressBorder > memDesiredAddress );
        }
        return isValid;
    }

    // Algorithm that looks left and right for the best insertion spot for sorted-insertion of page handles.
    // Returns the node to insert or append to.
    inline RwListEntry <pageHandle>& FindNodeForSortedInsertion( const memBlockSlice_t& desiredMemSlice, pageAllocation *scanMiddleArena, bool& trueAppendFalseInsertOut ) noexcept
    {
        // We right, left, right, left, ... and so on.
        // Until we found on the left a right-most or on the right a left-most item.

        pageAllocation *leftScan = scanMiddleArena;         // TODO: decrement this one by one (to the left).
        pageAllocation *rightScan = scanMiddleArena;

        pageHandle *prettyCloseHandle = nullptr;

        while ( true )
        {
            // Stop if left and right are none, in which case we return end of list for insertion.
            if ( leftScan == nullptr && rightScan == nullptr )
            {
                break;
            }

            // First to the right.
            if ( rightScan != nullptr )
            {
                pageHandle *begResideHandle = rightScan->begResideHandle;

                if ( begResideHandle )
                {
                    //trueAppendFalseInsertOut = false;

                    prettyCloseHandle = begResideHandle;
                    goto gotNeighborHandle;
                }

                // Increment.
                RwListEntry <pageAllocation> *nextArenaNode = rightScan->sortedNode.next;

                if ( nextArenaNode == &this->sortedMemoryRanges.root )
                {
                    rightScan = nullptr;
                }
                else
                {
                    rightScan = LIST_GETITEM( pageAllocation, nextArenaNode, sortedNode );
                }
            }

            // Next to the left.
            if ( leftScan != nullptr )
            {
                typename pageAllocation::sortedPageResidentIterator begResideIter( this, leftScan );

                if ( begResideIter.IsEnd() == false )
                {
                    trueAppendFalseInsertOut = true;

                    // Fetch the right-most item.
                    pageHandle *rightMostItem = begResideIter.Resolve();

                    while ( !begResideIter.IsEnd() )
                    {
                        rightMostItem = begResideIter.Resolve();

                        begResideIter.Increment();
                    }

                    // Return it.
                    prettyCloseHandle = rightMostItem;
                    goto gotNeighborHandle;
                }

                // Increment.
                RwListEntry <pageAllocation> *prevArenaNode = leftScan->sortedNode.prev;

                if ( prevArenaNode == &this->sortedMemoryRanges.root )
                {
                    leftScan = nullptr;
                }
                else
                {
                    leftScan = LIST_GETITEM( pageAllocation, prevArenaNode, sortedNode );
                }
            }
        }

        // The list is basically empty of page handles.
        // So just return the end.
        trueAppendFalseInsertOut = false;
        return this->sortedActiveHandles.root;

    gotNeighborHandle:
        FATAL_ASSERT( prettyCloseHandle != nullptr );

        // We need to sort it now.
        // Since we assume that the handle we found is pretty close to being sorted, we just go to
        // the neighbors until we found the correct spot.

        eir::eIntersectionResult beg_intResult =
            prettyCloseHandle->requestedMemory.intersectWith( desiredMemSlice );

        // Check if the close handle is left or right from our memory.
        // If it is left then we go right, if it is right then we go left -> until we found the collision-end.
        // IMPORTANT: we assume the new handle is not going to collide against any handles.

        bool leftTrueRightFalse = false;

        if ( beg_intResult == eir::INTERSECT_FLOATING_START )
        {
            // Close handle is left from the desired mem.
            leftTrueRightFalse = false;
        }
        else if ( beg_intResult == eir::INTERSECT_FLOATING_END )
        {
            // Close handle is right from the desired mem.
            leftTrueRightFalse = true;
        }
        else
        {
            // Should NEVER happen.
            FATAL_ASSERT( 0 );
        }

        while ( true )
        {
            // Get the next handle node.
            RwListEntry <pageHandle> *nextNode;

            if ( leftTrueRightFalse )
            {
                // Left.
                nextNode = prettyCloseHandle->managerNode.prev;
            }
            else
            {
                // Right.
                nextNode = prettyCloseHandle->managerNode.next;
            }

            bool reachedTheEnd = false;

            // We reached the end if we are the end node.
            if ( nextNode == &this->sortedActiveHandles.root )
            {
                reachedTheEnd = true;
            }

            // ... if we found another page handle that is floating just in front of our way.
            if ( !reachedTheEnd )
            {
                prettyCloseHandle = LIST_GETITEM( pageHandle, nextNode, managerNode );

                eir::eIntersectionResult intResult =
                    prettyCloseHandle->requestedMemory.intersectWith( desiredMemSlice );

                FATAL_ASSERT( eir::isFloatingIntersect( intResult ) == true );

                // We try to find the one that is our way.
                if ( leftTrueRightFalse )
                {
                    // Left.
                    if ( intResult == eir::INTERSECT_FLOATING_START )
                    {
                        // We append to nextCloseHandle.
                        reachedTheEnd = true;
                    }
                }
                else
                {
                    // Right.
                    if ( intResult == eir::INTERSECT_FLOATING_END )
                    {
                        // We insert to nextCloseHandle.
                        reachedTheEnd = true;
                    }
                }
            }

            if ( reachedTheEnd )
            {
                // Since we reached the end we can just perform the inclusion.
                // Remember that we always went one-too-far so we must place one-back.
                // If we went left we have to append, if we went right we have to insert.
                trueAppendFalseInsertOut = leftTrueRightFalse;
                return *nextNode;
            }
        }

        // SHOULD NEVER REACH THIS.
        trueAppendFalseInsertOut = false;
        return this->sortedActiveHandles.root;
    }

    // Cached vectors for usage by the main functions.
    // Can be used because functions are not reentrant and are not thread-safe.
    memCachedReserveAllocList_t _cached_memReserveList;

public:
    inline pageHandle* Allocate( void *desiredAddress, size_t spanSize ) noexcept
    {
        pageHandle *theHandle = nullptr;

        // Only proceed if the requested allocation is valid.
        if ( IsValidAllocation( desiredAddress, spanSize ) )
        {
            // Properly align the allocation request on page boundaries.
            size_t pageSize = this->vmemAccess.GetPlatformPageSize();

            size_t pageDesiredAddressStart = SCALE_DOWN( (size_t)desiredAddress, pageSize );
            size_t pageDesiredAddressEnd = ALIGN_SIZE( (size_t)desiredAddress + spanSize, pageSize );

            size_t pageSpanSize = ( pageDesiredAddressEnd - pageDesiredAddressStart );

            // Determine the pages that should host the requested memory region
            memCachedReserveAllocList_t& hostPages = this->_cached_memReserveList;
            bool validAllocation = false;

            // The actual allocation slice.
            memBlockSlice_t pageDesiredMemSlice( pageDesiredAddressStart, pageSpanSize );

            // We first have to find pages that can host our memory.
            {
                // If we know the address we should allocate on, we attempt to find regions that have already been allocated
                // so they can host our memory.
                if ( pageDesiredAddressStart != 0 )
                {
                    validAllocation = PlaceMemoryRequest( pageDesiredMemSlice, hostPages );
                }
                else
                {
                    // Otherwise we have to search for a new spot.
                    validAllocation = SearchForReservedMemory( pageSpanSize, pageDesiredMemSlice, hostPages );

                    if ( !validAllocation )
                    {
                        // As a last resort, request memory from the OS.
                        size_t arenaSpanSize = GetAllocationArenaRange( pageSpanSize );

                        pageAllocation *newArena = ReserveNewMemory( nullptr, arenaSpanSize );

                        if ( newArena )
                        {
                            // We allocate at the start of the new arena.
                            pageDesiredMemSlice.SetSlicePosition( (size_t)newArena->arenaAddress );

                            memReserveAllocInfo allocInfo;
                            allocInfo.hostArena = newArena;
                            allocInfo.hostArenaIsFirstSpot = true;
                            allocInfo.isHostArenaNewlyAllocated = true;

                            bool couldAdd = hostPages.AddItem( std::move( allocInfo ) );

                            if ( couldAdd )
                            {
                                // Register this new reserved memory.
                                LIST_INSERT( this->activeMemoryRanges.root, newArena->managerNode );
                                SortedMemoryBlockInsert( newArena );

                                validAllocation = true;
                            }
                            else
                            {
                                // Freeing without unassigning from manager (because not linked as node yet).
                                FreeArenaAllocation( newArena );
                            }
                        }
                    }
                }
            }

            if ( validAllocation )
            {
                // Create a pageHandle to it.
                pageHandle *newHandle = _allocPageHandle.Allocate( pageDesiredMemSlice );

                if ( newHandle )
                {
                    // Register it inside the host pages.
                    size_t count = hostPages.GetCount();

                    if ( count != 0 )
                    {
                        // Has to exist.
                        const memReserveAllocInfo *firstInfo = hostPages.Get( 0 );

                        // Do registration.
                        {
                            pageAllocation *firstPageAlloc = firstInfo->hostArena;

                            // Find the handle insertion spot, fast.
                            bool trueAppendFalseInsertSpotDenom;
                            RwListEntry <pageHandle>& insertOrAppendSpot = FindNodeForSortedInsertion( pageDesiredMemSlice, firstPageAlloc, trueAppendFalseInsertSpotDenom );

                            // Register the handle.
                            if ( trueAppendFalseInsertSpotDenom )
                            {
                                // Remember that the RwList operations are actually inverted-to-their-name
                                // if not used on the list root, hence...

                                // APPEND.
                                LIST_INSERT( insertOrAppendSpot, newHandle->managerNode );
                            }
                            else
                            {
                                // INSERT.
                                LIST_APPEND( insertOrAppendSpot, newHandle->managerNode );
                            }

                            // hostPages has to be address-sorted, of course.
                            newHandle->begResiding = firstPageAlloc;
                        }

                        hostPages.ForAllEntries(
                            [&]( const memReserveAllocInfo& info ) noexcept
                        {
                            pageAllocation *allocation = info.hostArena;

#ifdef _PARANOID_MEMTESTS_
                            // DEBUG.
                            allocation->CheckForCollision( newHandle->requestedMemory );
#endif //_PARANOID_MEMTESTS_

                            allocation->RefPageHandle();

                            // Set as first?
                            if ( info.hostArenaIsFirstSpot )
                            {
                                allocation->begResideHandle = newHandle;
                            }
                        });

                        this->numAllocatedPageHandles++;

                        // Put the memory active in the OS.
                        CommitMemoryOfPageHandle( newHandle, pageDesiredMemSlice );

                        theHandle = newHandle;

                        // Clear the hostPages list for another usage.
                        hostPages.Clear();
                    }
                }
            }

            if ( theHandle == nullptr )
            {
                // Delete all allocated pages.
                hostPages.ForAllEntries(
                    [&]( const memReserveAllocInfo& info )
                {
                    if ( info.isHostArenaNewlyAllocated )
                    {
                        pageAllocation *thePage = info.hostArena;

                        DeletePageAllocation( thePage );
                    }
                });

                // Clear it for another usage.
                hostPages.Clear();
            }
        }

        return theHandle;
    }

    // Returns the handle whose start pointer exactly matches pAddress.
    inline pageHandle* FindHandleByAddress( const void *pAddress ) noexcept
    {
        // Just compare addresses of every alive handle and return
        // the one that matches the query.
        LIST_FOREACH_BEGIN( pageHandle, sortedActiveHandles.root, managerNode )
            if ( item->GetTargetPointer() == pAddress )
            {
                return item;
            }
        LIST_FOREACH_END

        return nullptr;
    }

private:
    // Helper function to get a signed difference between two unsigned numbers.
    template <typename numberType>
    static inline numberType GetSignedDifference( const numberType& left, const numberType& right, bool& isSigned ) noexcept
    {
        bool _isSigned = ( left < right );

        numberType result;

        if ( _isSigned )
        {
            result = ( right - left );
        }
        else
        {
            result = ( left - right );
        }

        isSigned = _isSigned;

        return result;
    }

    inline void MemBlockGarbageCollection( pageAllocation *memBlock ) noexcept
    {
        // If the page is not being used anymore, release it.
        if ( !memBlock->IsBlockBeingUsed() )
        {
            DeletePageAllocation( memBlock );
        }
    }

    template <typename... Args>
    AINLINE void CommitArenasToPageHandle( pageHandle *theHandle, memCachedReserveAllocList_t& memReserveList, Args&&... excludingHandles ) noexcept
    {
        // Add the things together, merge them.
        memReserveList.ForAllEntries(
            [&]( const memReserveAllocInfo& info )
        {
            pageAllocation *hostArena = info.hostArena;

            // If the pageHandle was already registered with said arena (basically it shared region with said arena prior to this),
            // then we don't care to prevent double referencing.
            // This works because prior to expanding every pageHandle byte must be covered by exactly one arena.
            eir::eIntersectionResult intRes = hostArena->pageSpan.intersectWith( theHandle->GetTargetSlice() );

            if ( eir::isFloatingIntersect( intRes ) == false )
            {
                return;
            }

            // If there are any excluding page handles given to us, then we make sure that the arena does not overlap with them.
            if constexpr ( sizeof...( excludingHandles ) > 0 )
            {
                for ( pageHandle *excludingHandle : { excludingHandles... } )
                {
                    eir::eIntersectionResult exIntRes = hostArena->pageSpan.intersectWith( excludingHandle->GetTargetSlice() );

                    if ( eir::isFloatingIntersect( exIntRes ) == false )
                    {
                        return;
                    }
                }
            }

#ifdef _PARANOID_MEMTESTS_
            // DEBUG.
            hostArena->CheckForCollision( theHandle->requestedMemory );
            hostArena->CheckForCollision( requiredRegion );
#endif //_PARANOID_MEMTESTS_

            hostArena->RefPageHandle();

            // Set as first?
            if ( info.hostArenaIsFirstSpot )
            {
                hostArena->begResideHandle = theHandle;
            }
        });
    }

public:
    // Attempts to update the handle size so that either more or less memory
    // can be used.
    inline bool SetHandleSize( pageHandle *theHandle, size_t _unaligned_newReserveSize ) noexcept
    {
        // Properly align the request size.
        // This is important because we represent real memory pages.
        size_t newReserveSize = GetPageAllocationRange( _unaligned_newReserveSize );

        // Do nothing if the handle size has not changed.
        size_t oldSize = theHandle->GetTargetSize();

        if ( newReserveSize == oldSize )
            return true;

        if ( newReserveSize == 0 )
        {
            // In this case you must free the handle manually (cannot free using this function).
            return false;
        }

        bool isSigned;
        size_t memSizeDifference = GetSignedDifference( newReserveSize, oldSize, isSigned );

        bool success = false;

        if ( !isSigned )
        {
            // Make sure that this allocation is valid.
            // It can only turn invalid if the memory size is greater than before.
            if ( IsValidAllocation( theHandle->GetTargetPointer(), newReserveSize ) )
            {
                // If the new memory size is greater than the old,
                // allocate additional memory pages, on demand of course.
                memBlockSlice_t requiredRegion( (size_t)theHandle->GetTargetPointer() + (size_t)oldSize, (size_t)memSizeDifference );

                // Now we simply allocate the region(s) after the memory and
                // merge the two (or more) allocation regions into one.

                arenaSortedIterator_t sortedIter( this->sortedMemoryRanges, &theHandle->begResiding->sortedNode );

                memCachedReserveAllocList_t& expansion_inOut = this->_cached_memReserveList;

                bool flowAllocExpandSuccess =
                    FlowAllocateAfterRegion( expansion_inOut, sortedIter, requiredRegion );

                // Have we succeeded in reserving the requested memory pages?
                if ( flowAllocExpandSuccess )
                {
                    CommitArenasToPageHandle( theHandle, expansion_inOut );

                    // Set the new handle region.
                    theHandle->requestedMemory.SetSliceEndPoint( (size_t)theHandle->GetTargetPointer() + newReserveSize - (size_t)1 );

                    // Now update the OS.
                    CommitMemoryOfPageHandle( theHandle, requiredRegion );

                    success = true;

                    // Clear the list for another usage.
                    expansion_inOut.Clear();
                }
            }
        }
        else
        {
            // Otherwise the new memory size is smaller than the old.
            // We potentially have to remove pages from the residency list.

            memBlockSlice_t requiredRegion( (size_t)theHandle->GetTargetPointer() + newReserveSize, (size_t)memSizeDifference );

            // Update the OS.
            DecommitMemoryOfPageHandle( theHandle, requiredRegion );

            // Determine the amount of arenas that should be dereferenced in the course of this influence area shrinking.
            struct derefArenasOutsidePageHandleCalloid
            {
                static AINLINE void ProcessEntry( pageAllocation *oneInSortedOrder, PlatformPageAllocator *manager, pageHandle *theHandle, const memBlockSlice_t& requiredRegion ) noexcept
                {
                    // Since the start border of the requiredRegion is touching the end border of the new valid region of theHandle,
                    // each arena whose starting point is >= the starting point of requiredRegion is floating past the new valid region.
                    // The above statement directly matches the definition of floating past.
                    // Thus the check is valid.
                    bool isFloatingPast = ( oneInSortedOrder->pageSpan.GetSliceStartPoint() >= requiredRegion.GetSliceStartPoint() );

                    if ( isFloatingPast )
                    {
                        oneInSortedOrder->RemovePossibleFirst( manager, theHandle );

                        oneInSortedOrder->DerefPageHandle();

                        manager->MemBlockGarbageCollection( oneInSortedOrder );
                    }
                }
            };

            ForAllPageHandleArenasSorted <derefArenasOutsidePageHandleCalloid> ( theHandle, this, theHandle, requiredRegion );

            // Set the new handle region.
            theHandle->requestedMemory.SetSliceEndPoint( (size_t)theHandle->GetTargetPointer() + newReserveSize - (size_t)1 );

            success = true;
        }

        return success;
    }

    // Method to attempt pageHandle expansion to the left/prior, enabling more efficient memory page management on systems
    // such as Linux. Also allows for shrinking of pageHandles, like SetHandleSize.
    inline bool LeftResizeHandle( pageHandle *theHandle, size_t _unaligned_newReserveSize ) noexcept
    {
        // Properly align the request size.
        // This is important because we represent real memory pages.
        size_t newReserveSize = GetPageAllocationRange( _unaligned_newReserveSize );

        // Do nothing if the handle size has not changed.
        size_t oldSize = theHandle->GetTargetSize();

        if ( newReserveSize == oldSize )
            return true;

        if ( newReserveSize == 0 )
        {
            // In this case you must free the handle manually (cannot free using this function).
            return false;
        }

        bool isSigned;
        size_t memSizeDifference = GetSignedDifference( newReserveSize, oldSize, isSigned );

        bool success = false;

        if ( !isSigned )
        {
            // Growing the handle.
            size_t oldHandleStartOff = theHandle->GetTargetSlice().GetSliceStartPoint();
            size_t newHandleStartOff = ( oldHandleStartOff - memSizeDifference );

            if ( newHandleStartOff < oldHandleStartOff )
            {
                // Calculate the region of the pageHandle that is to be added.
                memBlockSlice_t requiredRegion( newHandleStartOff, memSizeDifference );

                arenaSortedIterator_t arena_iter( this->sortedMemoryRanges, &theHandle->begResiding->sortedNode );

                memCachedReserveAllocList_t& expansion_inOut = this->_cached_memReserveList;

                bool allocSuccess =
                    this->FlowAllocateAfterRegionBackward( expansion_inOut, std::move( arena_iter ), requiredRegion );

                if ( allocSuccess )
                {
                    // Process the new chunks.
                    CommitArenasToPageHandle( theHandle, expansion_inOut );

                    // Update the first residing arena of handle.
                    {
                        size_t expansion_inOut_count = expansion_inOut.GetCount();

#ifdef _DEBUG
                        FATAL_ASSERT( expansion_inOut_count > 0 );
#endif //_DEBUG

                        memReserveAllocInfo *allocInfo = expansion_inOut.Get( expansion_inOut_count - 1 );

                        theHandle->begResiding = allocInfo->hostArena;
                    }

                    // Set the new handle region.
                    theHandle->requestedMemory.SetSliceStartPoint( newHandleStartOff );

                    // Now update the OS.
                    CommitMemoryOfPageHandle( theHandle, requiredRegion );

                    success = true;

                    // Clear for next usage.
                    expansion_inOut.Clear();
                }
            }
        }
        else
        {
            // Shrinking the handle.
            size_t oldHandleStartOff = theHandle->GetTargetSlice().GetSliceStartPoint();
            size_t newHandleStartOff = ( oldHandleStartOff + memSizeDifference );

            memBlockSlice_t requiredRegion( oldHandleStartOff, memSizeDifference );

            // Update the OS.
            DecommitMemoryOfPageHandle( theHandle, requiredRegion );

            // Determine the amount of arenas that should be dereferenced in the course of this influence area shrinking.
            pageAllocation *first_valid_arena = nullptr;

            struct derefArenasOutsidePageHandleCalloid
            {
                static AINLINE void ProcessEntry( pageAllocation *oneInSortedOrder, PlatformPageAllocator *manager, pageHandle *theHandle, const memBlockSlice_t& requiredRegion, pageAllocation*& first_valid_arena_out )
                {
                    bool isFloatingPast = ( oneInSortedOrder->pageSpan.GetSliceEndPoint() <= requiredRegion.GetSliceEndPoint() );

                    if ( isFloatingPast )
                    {
                        oneInSortedOrder->RemovePossibleFirst( manager, theHandle );

                        oneInSortedOrder->DerefPageHandle();

                        manager->MemBlockGarbageCollection( oneInSortedOrder );
                    }
                    else
                    {
                        if ( first_valid_arena_out == nullptr )
                        {
                            first_valid_arena_out = oneInSortedOrder;
                        }
                    }
                }
            };

            ForAllPageHandleArenasSorted <derefArenasOutsidePageHandleCalloid> ( theHandle, this, theHandle, requiredRegion, first_valid_arena );

            // Update our handle.
#ifdef _DEBUG
            FATAL_ASSERT( first_valid_arena != nullptr );
#endif //_DEBUG

            theHandle->begResiding = first_valid_arena;

            // Set the new handle region.
            theHandle->requestedMemory.SetSliceStartPoint( newHandleStartOff );

            success = true;
        }

        return success;
    }

private:
    AINLINE void _FreePageHandle( pageHandle *memRange ) noexcept
    {
        this->numAllocatedPageHandles--;

        // Delete and unregister our pageHandle.
        LIST_REMOVE( memRange->managerNode );

        _allocPageHandle.Deallocate( memRange );
    }

public:
    // Merges two pageHandles by attempting allocation of the gap between them, and on success putting all
    // arena references under "one" while deleting "two".
    inline bool MergePageHandles( pageHandle *one, pageHandle *two ) noexcept
    {
        // We first sort the pageHandles by address.
        pageHandle *first, *second;

        bool isOnePriorToTwo = one->GetTargetPointer() < two->GetTargetPointer();

        if ( isOnePriorToTwo )
        {
            first = one;
            second = two;
        }
        else
        {
            first = two;
            second = one;
        }

        // If there is memory between the pageHandles, then attempt flow-allocation of it.
        // On success we complete the merging.
        eir::upperBound <size_t> firstSlice_endBound = first->GetTargetSlice().GetSliceEndBound();
        eir::lowerBound <size_t> secondSlice_startBound = second->GetTargetSlice().GetSliceStartBound();

        bool hasNewRegionInbetween = false;

        memCachedReserveAllocList_t& expansion_inOut = this->_cached_memReserveList;

        // Only set if there is memory inbetween.
        memBlockSlice_t betweenMemRegion;

        if ( eir::HasTightBound( firstSlice_endBound ) && eir::HasTightBound( secondSlice_startBound ) )
        {
            eir::lowerBound <size_t> betweenMemStart = GetTightBound( firstSlice_endBound );
            eir::upperBound <size_t> betweenMemEnd = GetTightBound( secondSlice_startBound );

            hasNewRegionInbetween = ( betweenMemStart <= betweenMemEnd );

            if ( hasNewRegionInbetween )
            {
                betweenMemRegion = memBlockSlice_t::fromBounds( std::move( betweenMemStart ), std::move( betweenMemEnd ) );

                arenaSortedIterator_t arena_iter( this->sortedMemoryRanges, &first->begResiding->sortedNode );

                bool flowAllocSuccess = FlowAllocateAfterRegion( expansion_inOut, std::move( arena_iter ), betweenMemRegion );

                if ( flowAllocSuccess == false )
                {
                    // Nothing to care about.
                    return false;
                }

                // Need to put the new memory pages to the page handle called "one".
                CommitArenasToPageHandle( one, expansion_inOut, two );
            }
        }

        struct releaseSharedArenaReferences
        {
            static AINLINE void ProcessEntry( pageAllocation *oneInSortedOrder, pageHandle *newHandle ) noexcept
            {
                // Since the oldHandle ceases to exist, we have to release the reference on any arena that both were using at the
                // same time. By definition it is any arena that has any bytes of the newHandle aswell.
                eir::eIntersectionResult intResult = oneInSortedOrder->pageSpan.intersectWith( newHandle->GetTargetSlice() );

                if ( eir::isFloatingIntersect( intResult ) == false )
                {
                    // Found a shared page, just release the reference that is too much.
                    oneInSortedOrder->DerefPageHandle();
                }
            }
        };

        ForAllPageHandleArenasSorted <releaseSharedArenaReferences> ( two, one );

        // The way to take over arenas from another page handle is just taking over its validity region.
        one->requestedMemory = memBlockSlice_t::fromBounds( first->GetTargetSlice().GetSliceStartBound(), second->GetTargetSlice().GetSliceEndBound() );

        // Also change the beginResiding of "one" if applicable.
        if ( isOnePriorToTwo == false )
        {
            one->begResiding = two->begResiding;
        }

        // Rewrite the beginResiding references from "two" to "one" because we just took over its memory validity region.
        struct rewriteArenaBegResiding
        {
            static AINLINE void ProcessEntry( pageAllocation *oneInSortedOrder, pageHandle *oldHandle, pageHandle *newHandle ) noexcept
            {
                if ( oneInSortedOrder->begResideHandle == oldHandle )
                {
                    oneInSortedOrder->begResideHandle = newHandle;
                }
            }
        };

        ForAllPageHandleArenasSorted <rewriteArenaBegResiding> ( two, two, one );

        // Delete the second page handle.
        _FreePageHandle( two );

        if ( hasNewRegionInbetween )
        {
            // Now we have to commit the memory inbetween.
            CommitMemoryOfPageHandle( one, betweenMemRegion );

            // Need to clear the list for another usage.
            expansion_inOut.Clear();
        }

        return true;
    }

    inline void Free( pageHandle *memRange ) noexcept
    {
        // Release the contents of the memory to the OS.
        DecommitMemoryOfPageHandle( memRange, memRange->requestedMemory );

        // Free the link to the allocated OS memory regions.
        struct derefAllPageHandleArenasCalloid
        {
            static AINLINE void ProcessEntry( pageAllocation *memBlock, PlatformPageAllocator *manager, pageHandle *handleBeingRemoved ) noexcept
            {
                // Make sure we are not referenced in the arena anymore.
                memBlock->RemovePossibleFirst( manager, handleBeingRemoved );

                memBlock->DerefPageHandle();

                // Clean up memory blocks that are not used anymore.
                manager->MemBlockGarbageCollection( memBlock );
            }
        };

        ForAllPageHandleArenasSorted <derefAllPageHandleArenasCalloid> ( memRange, this, memRange );

        _FreePageHandle( memRange );
    }

    inline bool FreeByAddress( void *pAddress ) noexcept
    {
        pageHandle *theHandle = FindHandleByAddress( pAddress );

        if ( !theHandle )
            return false;

        Free( theHandle );
        return true;
    }

    // Iterates through all active handles in memory-order.
    template <typename callbackType>
    inline void ForAllHandlesInMemoryOrder( callbackType&& cb )
    {
        LIST_FOREACH_BEGIN( pageHandle, this->sortedActiveHandles.root, managerNode )
            cb( item );
        LIST_FOREACH_END
    }

    // Meta-data API.
    inline size_t GetPageSize( void ) const noexcept
    {
        return vmemAccess.GetPlatformPageSize();
    }
};

using NativePageAllocator = PlatformPageAllocator
#ifdef __linux__
    <LinuxVirtualMemoryAccessorDefaultSyscalls>
#endif //__linux__
    ;

#endif //_COMMON_OS_UTILS_
