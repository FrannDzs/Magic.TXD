/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MemoryUtils.h
*  PURPOSE:     Memory management templates
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _GLOBAL_MEMORY_UTILITIES_
#define _GLOBAL_MEMORY_UTILITIES_

#include "rwlist.hpp"
#include "MemoryRaw.h"
#include "MetaHelpers.h"
#include <atomic>
#include <type_traits>

// TODO: the classes in this header need proper unit-tests; we need to even still finish some of them;
//  also we should add C++20 concepts where they are long due.

template <typename numberType, typename managerType>
struct FirstPassAllocationSemantics
{
    // This class exposes algorithms-only for allocating chunks of memory
    // in an address-sorted container.
    // REQUIREMENTS FOR managerType CLASS INTERFACE:
    // * REQUIREMENTS sub-type blockIter_t CLASS INTERFACE:
    //   - standard constructor (can do nothing/POD)
    //   - copy-assignable/constructible
    //   - move-assignable/constructible
    //   - void Increment( void )
    //   - memSlice_t GetMemorySlice( void )
    //   - void* GetNativePointer( void )
    // * blockIter_t GetFirstMemoryBlock( void )
    // * blockIter_t GetLastMemoryBlock( void )
    // * bool HasMemoryBlocks( void )
    // * blockIter_t GetRootNode( void )
    // * blockIter_t GetAppendNode( blockIter_t iter )
    // * bool IsEndMemoryBlock( const blockIter_t& iter ) const
    // * bool IsInAllocationRange( const memSlice_t& memRegion )
    // The programmer is encouraged to write their own storage mechanism
    // based on the output. The default storage system is linked list.

    typedef typename managerType::blockIter_t blockIter_t;

    typedef sliceOfData <numberType> memSlice_t;

    struct allocInfo
    {
        memSlice_t slice;
        numberType alignment;
        blockIter_t blockToAppendAt;
    };

    // The dynamic variant of the FindSpace method allows you to calculate a size for your memory block depending on the actual offset into the memory.
    // This behaviour is especially important in memory allocators that allow aligned memory allocations without bullshitting the user by allocating
    // more memory than necessary and then disecting the proper start offset.
    template <typename posDispatcherType>
    AINLINE static bool FindSpaceDynamic( managerType& manager, posDispatcherType& posDispatch, allocInfo& infoOut, numberType allocStart = 0 )
    {
        // Try to allocate memory at the first position we find.
        memSlice_t finalAllocSlice;
        {
            numberType realAllocStart = allocStart;
            numberType realAllocSize;

            posDispatch.ScanNextBlock( realAllocStart, realAllocSize );

            finalAllocSlice = memSlice_t( realAllocStart, realAllocSize );
        }

        blockIter_t appendNode = manager.GetRootNode();

        for ( blockIter_t iter( manager.GetFirstMemoryBlock() ); manager.IsEndMemoryBlock( iter ) == false; iter.Increment() )
        {
            // Intersect the current memory slice with the ones on our list.
            memSlice_t blockSlice = iter.GetMemorySlice();

            typename eir::eIntersectionResult intResult = finalAllocSlice.intersectWith( blockSlice );

            if ( !eir::isFloatingIntersect( intResult ) )
            {
                // Advance the try memory offset.
                numberType tryMemPosition = ( blockSlice.GetSliceEndPoint() + 1 );
                numberType memOffsetSize;

                // Fetch next memory slice.
                posDispatch.ScanNextBlock( tryMemPosition, memOffsetSize );

                // Construct the new alloc slice.
                finalAllocSlice = memSlice_t( tryMemPosition, memOffsetSize );
            }
            else if ( intResult == eir::INTERSECT_FLOATING_END )
            {
                // We kinda encounter this when the alignment makes us skip (!) over
                // data units.
                // For that we skip memory blocks until the floating end is not true anymore
                // or we are out of blocks.
            }
            else
            {
                // This means we are floating before a next-block.
                // Perfect scenario because we choose to "append" at the previous block we met.
                break;
            }

            // Set the append node further.
            appendNode = manager.GetAppendNode( iter );
        }

        // The result is - by definition of this algorithm - the earliest allocation spot
        // for our request. Since there cannot be any earlier, if our request happens to be
        // outside of the allowed allocation range, all other prior requests are invalid too.
        if ( !manager.IsInAllocationRange( finalAllocSlice ) )
            return false;

        infoOut.slice = std::move( finalAllocSlice );
        infoOut.alignment = posDispatch.GetAlignment();
        infoOut.blockToAppendAt = std::move( appendNode );
        return true;
    }

    AINLINE static bool FindSpace( managerType& manager, numberType sizeOfBlock, allocInfo& infoOut, const numberType alignmentWidth = sizeof( void* ), numberType allocStart = 0 )
    {
        struct constantSizeDispatcher
        {
            AINLINE constantSizeDispatcher( numberType sizeOfBlock, numberType alignmentWidth )
            {
                this->sizeOfBlock = sizeOfBlock;
                this->alignmentWidth = alignmentWidth;
            }

            AINLINE void ScanNextBlock( numberType& offsetInOut, numberType& sizeOut )
            {
                // This is a simple alignment-next constant-size memory allocator.
                offsetInOut = ALIGN( offsetInOut, this->alignmentWidth, this->alignmentWidth );
                sizeOut = this->sizeOfBlock;
            }

            AINLINE numberType GetAlignment( void )
            {
                return this->alignmentWidth;
            }

            numberType sizeOfBlock;
            numberType alignmentWidth;
        };

        constantSizeDispatcher dispatch( sizeOfBlock, alignmentWidth );

        return FindSpaceDynamic( manager, dispatch, infoOut, allocStart );
    }

    static AINLINE bool ObtainSpaceAt( managerType& manager, numberType offsetAt, numberType sizeOfBlock, allocInfo& infoOut )
    {
        // Skip all blocks that are before us.
        memSlice_t newAllocSlice( offsetAt, sizeOfBlock );

        // Check if we even can fulfill that request.
        if ( !manager.IsInAllocationRange( newAllocSlice ) )
            return false;

        blockIter_t appendNode = manager.GetRootNode();

        typename eir::eIntersectionResult nextIntResult = eir::INTERSECT_UNKNOWN;
        bool hasNextIntersection = false;

        for ( blockIter_t iter( manager.GetFirstMemoryBlock() ); manager.IsEndMemoryBlock( iter ) == false; iter.Increment() )
        {
            // We are interested in how many blocks we actually have to skip.
            const memSlice_t& blockSlice = iter.GetMemorySlice();

            typename eir::eIntersectionResult intResult = blockSlice.intersectWith( newAllocSlice );

            if ( intResult != eir::INTERSECT_FLOATING_START )
            {
                nextIntResult = intResult;
                hasNextIntersection = true;
                break;
            }

            // We found something that is floating, so we have to check next block.
            appendNode = manager.GetAppendNode( iter );
        }

        // If we have any kind of next node, and it intersects violently, then we have no space :(
        if ( hasNextIntersection && nextIntResult != eir::INTERSECT_FLOATING_END )
        {
            // There is a collision, meow.
            return false;
        }

        // We are happy. :-)
        infoOut.slice = std::move( newAllocSlice );
        infoOut.alignment = 1;
        infoOut.blockToAppendAt = std::move( appendNode );
        return true;
    }

    static AINLINE bool CheckMemoryBlockExtension( managerType& manager, blockIter_t extItemIter, numberType newSize, memSlice_t& newSliceOut )
    {
        // Since we are sorted in address order, checking for block expansion is a piece of cake.
        memSlice_t newBlockSlice( extItemIter.GetMemorySlice().GetSliceStartPoint(), newSize );

        if ( manager.IsInAllocationRange( newBlockSlice ) )
            return false;

        extItemIter.Increment();

        if ( manager.IsEndMemoryBlock( extItemIter ) == false )
        {
            const memSlice_t& nextBlockSlice = extItemIter.GetMemorySlice();

            typename eir::eIntersectionResult intResult = newBlockSlice.intersectWith( nextBlockSlice );

            if ( eir::isFloatingIntersect( intResult ) == false )
                return false;
        }

        newSliceOut = std::move( newBlockSlice );
        return true;
    }

    static AINLINE numberType GetSpanSize( const managerType& manager, numberType startSpanSize = 0 )
    {
        numberType theSpanSize = std::move( startSpanSize );

        if ( manager.HasMemoryBlocks() )
        {
            blockIter_t lastItem = manager.GetLastMemoryBlock();

            // Thankfully, we depend on the sorting based on memory order.
            // Getting the span size is very easy then, since the last item is automatically at the top most memory offset.
            // Since we always start at position zero, its okay to just take the end point.
            theSpanSize = ( lastItem.GetMemorySlice().GetSliceEndPoint() + 1 );
        }

        return theSpanSize;
    }
};

template <typename numberType, typename managerType, typename collisionConditionalType>
struct ConditionalAllocationProcSemantics
{
    // TODO: we still have to test this system again!

    // Conditional allocation processing semantics.
    // managerType has the same REQUIREMENTS like FirstPassAllocationSemantics
    // ADDITIONAL REQUIREMENTS FOR blockIter_t:
    // - void Decrement( void )

    typedef typename managerType::blockIter_t blockIter_t;

    typedef sliceOfData <numberType> memSlice_t;

    // TODO: we will have to revise this conditional allocation system at some point.

    struct conditionalRegionIterator
    {
        const managerType *manager;
        collisionConditionalType *conditional;

    private:
        numberType removalByteCount;

        blockIter_t iter;

    public:
        inline conditionalRegionIterator( const managerType *manager, collisionConditionalType& conditional )
            : manager( manager ), conditional( &conditional ), iter( manager->GetFirstMemoryBlock() )
        {
            this->removalByteCount = 0;
        }

        inline conditionalRegionIterator( const conditionalRegionIterator& right ) = default;
        inline conditionalRegionIterator( conditionalRegionIterator&& right ) = default;

        inline conditionalRegionIterator& operator = ( const conditionalRegionIterator& right ) = default;
        inline conditionalRegionIterator& operator = ( conditionalRegionIterator&& right ) = default;

        inline void Increment( void )
        {
            blockIter_t curItem = this->iter;

            void *curBlock = nullptr;

            if ( !this->manager->IsEndMemoryBlock( curItem ) )
            {
                curBlock = curItem.GetNativePointer();
            }

            do
            {
                blockIter_t nextItem = curItem;
                nextItem.Increment();

                bool shouldBreak = false;

                void *nextBlock = nullptr;

                if ( !this->manager->IsEndMemoryBlock( nextItem ) )
                {
                    nextBlock = nextItem.GetNativePointer();

                    if ( curBlock == nullptr )
                    {
                        this->removalByteCount = 0;
                    }
                    else
                    {
                        if ( this->conditional->DoIgnoreBlock( curBlock ) )
                        {
                            const memSlice_t& nextBlockSlice = nextItem.GetMemorySlice();
                            const memSlice_t& curBlockSlice = curItem.GetMemorySlice();

                            numberType ignoreByteCount = ( nextBlockSlice.GetSliceStartPoint() - curBlockSlice.GetSliceStartPoint() );

                            this->removalByteCount += ignoreByteCount;
                        }
                        else
                        {
                            shouldBreak = true;
                        }
                    }
                }
                else
                {
                    shouldBreak = true;
                }

                curItem = std::move( nextItem );
                curBlock = std::move( nextBlock );

                if ( shouldBreak )
                {
                    break;
                }
            }
            while ( !this->manager->IsEndMemoryBlock( curItem ) );

            this->iter = std::move( curItem );
        }

        inline void Decrement( void )
        {
            blockIter_t curItem = this->iter;

            void *curBlock = nullptr;

            if ( !this->manager->IsEndMemoryBlock( curItem ) )
            {
                curBlock = curItem.GetNativePointer();
            }

            do
            {
                blockIter_t prevItem = curItem;
                prevItem.Decrement();

                bool shouldBreak = false;

                void *prevBlock = nullptr;

                if ( !this->manager->IsEndMemoryBlock( prevItem ) )
                {
                    prevBlock = prevItem.GetNativePointer();

                    if ( curBlock == nullptr )
                    {
                        // TODO annoying shit.
                        // basically I must restore the state as if the guy went through the list straight.
                        FATAL_ASSERT( 0 );
                    }
                    else
                    {
                        if ( this->conditional->DoIgnoreBlock( prevBlock ) )
                        {
                            const memSlice_t& curBlockSlice = curItem.GetMemorySlice();
                            const memSlice_t& prevBlockSlice = prevItem.GetMemorySlice();

                            numberType ignoreByteCount = ( curBlockSlice.GetSliceStartPoint() - prevBlockSlice.GetSliceStartPoint() );

                            this->removalByteCount -= ignoreByteCount;
                        }
                        else
                        {
                            shouldBreak = true;
                        }
                    }
                }
                else
                {
                    shouldBreak = true;
                }

                curItem = std::move( prevItem );
                curBlock = std::move( prevBlock );

                if ( shouldBreak )
                {
                    break;
                }
            }
            while ( !this->manager->IsEndMemoryBlock( curItem ) );

            this->iter = std::move( curItem );
        }

        inline bool IsEnd( void ) const
        {
            return ( this->manager->IsEndMemoryBlock( this->iter ) );
        }

        inline bool IsNextEnd( void ) const
        {
            blockIter_t nextItem = this->iter;
            nextItem.Increment();

            return ( this->manager->IsEndMemoryBlock( nextItem ) );
        }

        inline bool IsPrevEnd( void ) const
        {
            blockIter_t prevItem = this->iter;
            prevItem.Decrement();

            return ( this->manager->IsEndMemoryBlock( prevItem ) );
        }

        inline decltype(auto) ResolveBlock( void ) const
        {
            return this->iter.GetNativePointer();
        }

        inline const memSlice_t& ResolveMemorySlice( void ) const
        {
            return this->iter.GetMemorySlice();
        }

        inline numberType ResolveOffset( void ) const
        {
            const memSlice_t& memSlice = this->iter.GetMemorySlice();

            return ( memSlice.GetSliceStartPoint() - this->removalByteCount );
        }

        inline numberType ResolveOffsetAfter( void ) const
        {
            void *curBlock = this->iter.GetNativePointer();

            bool ignoreCurrentBlock = this->conditional->DoIgnoreBlock( curBlock );

            const memSlice_t& memSlice = this->iter.GetMemorySlice();

            if ( ignoreCurrentBlock )
            {
                return ( memSlice.GetSliceStartPoint() - this->removalByteCount );
            }

            return ( ( memSlice.GetSliceEndPoint() + 1 ) - this->removalByteCount );
        }
    };

    // Accelerated conditional look-up routines, so that you can still easily get block offsets and span size when you want to ignore
    // certain blocks.
    static inline numberType GetSpanSizeConditional( const managerType *manager, collisionConditionalType& conditional )
    {
        conditionalRegionIterator iterator( manager, conditional );

        bool hasItem = false;

        if ( iterator.IsEnd() == false )
        {
            hasItem = true;

            while ( iterator.IsNextEnd() == false )
            {
                iterator.Increment();
            }
        }

        // If the list of blocks is empty, ignore.
        if ( hasItem == false )
        {
            return 0;
        }

        return iterator.ResolveOffsetAfter();
    }

    static inline bool GetBlockOffsetConditional( const managerType *manager, const void *theBlock, numberType& outOffset, collisionConditionalType& conditional )
    {
        // If the block that we request the offset of should be ignored anyway, we will simply return false.
        if ( conditional.DoIgnoreBlock( theBlock ) )
        {
            return false;
        }

        conditionalRegionIterator iterator( &manager, conditional );

        bool hasFoundTheBlock = false;

        while ( iterator.IsEnd() == false )
        {
            // We terminate if we found our block.
            if ( iterator.ResolveBlock() == theBlock )
            {
                hasFoundTheBlock = true;
                break;
            }

            iterator.Increment();
        }

        if ( hasFoundTheBlock == false )
        {
            // This should never happen.
            return false;
        }

        // Return the actual offset that preserves alignment.
        outOffset = iterator.ResolveOffset();
        return true;
    }

    // This is a very optimized algorithm for turning a static-allocation-offset into its conditional equivalent.
    static inline bool ResolveConditionalBlockOffset( const managerType *manager, numberType staticBlockOffset, numberType& outOffset, collisionConditionalType& conditional )
    {
        // FIXME.
#if 0
        // If the block that we request the offset of should be ignored anyway, we will simply return false.
        if ( conditional.DoIgnoreBlock( theBlock ) )
        {
            return false;
        }
#endif

        conditionalRegionIterator iterator( manager, conditional );

        bool hasFoundTheBlock = false;

        while ( iterator.IsEnd() == false )
        {
            // We terminate if we found our block.
            const memSlice_t& blockSlice = iterator.ResolveMemorySlice();

            numberType thisBlockOffset = blockSlice.GetSliceStartPoint();

            if ( thisBlockOffset == staticBlockOffset )
            {
                hasFoundTheBlock = true;
                break;
            }
            else if ( thisBlockOffset > staticBlockOffset )
            {
                // We have not found it.
                // Terminate early.
                break;
            }

            iterator.Increment();
        }

        if ( hasFoundTheBlock == false )
        {
            // This should never happen.
            return false;
        }

        // Return the actual offset that preserves alignment.
        outOffset = iterator.ResolveOffset();
        return true;
    }
};

template <typename numberType, typename colliderType = void>
class CollisionlessBlockAllocator
{
public:
    typedef sliceOfData <numberType> memSlice_t;

    struct block_t
    {
        RwListEntry <block_t> node;

        memSlice_t slice;
        numberType alignment;

        // Use this function ONLY IF block_t is allocated.
        inline void moveFrom( block_t&& right )
        {
            this->slice = std::move( right.slice );
            this->alignment = std::move( right.alignment );

            this->node.moveFrom( std::move( right.node ) );
        }
    };

    RwList <block_t> blockList;

    inline CollisionlessBlockAllocator( colliderType&& collider = colliderType() ) : allocSemMan( std::move( collider ) )
    {
        return;
    }

    inline CollisionlessBlockAllocator( const CollisionlessBlockAllocator& right ) = delete;
    inline CollisionlessBlockAllocator( CollisionlessBlockAllocator&& right ) = default;

    inline CollisionlessBlockAllocator& operator = ( const CollisionlessBlockAllocator& right ) = delete;
    inline CollisionlessBlockAllocator& operator = ( CollisionlessBlockAllocator&& right ) = default;

    // TODO: we do not check whether all blocks deallocated; I guess we implicitly do not care?

    struct allocSemanticsManager
    {
        colliderType collider;

        AINLINE allocSemanticsManager( colliderType&& collider ) : collider( std::move( collider ) )
        {
            return;
        }

        AINLINE CollisionlessBlockAllocator* GetManager( void )
        {
            return (CollisionlessBlockAllocator*)( this - offsetof(CollisionlessBlockAllocator, allocSemMan) );
        }

        AINLINE const CollisionlessBlockAllocator* GetManager( void ) const
        {
            return (const CollisionlessBlockAllocator*)( this - offsetof(CollisionlessBlockAllocator, allocSemMan) );
        }

        struct blockIter_t
        {
            AINLINE blockIter_t( void )
            {
                return;
            }

            AINLINE blockIter_t( RwListEntry <block_t>& node )
            {
                this->iter_node = &node;
            }

        private:
            AINLINE block_t* GetCurrentBlock( void ) const
            {
                return LIST_GETITEM( block_t, this->iter_node, node );
            }

        public:
            AINLINE const memSlice_t& GetMemorySlice( void ) const
            {
                return GetCurrentBlock()->slice;
            }

            AINLINE block_t* GetNativePointer( void ) const
            {
                return GetCurrentBlock();
            }

            AINLINE void Increment( void )
            {
                this->iter_node = this->iter_node->next;
            }

            AINLINE void Decrement( void )
            {
                this->iter_node = this->iter_node->prev;
            }

            RwListEntry <block_t> *iter_node;
        };

        AINLINE blockIter_t GetRootNode( void )
        {
            return ( GetManager()->blockList.root );
        }

        AINLINE blockIter_t GetFirstMemoryBlock( void ) const
        {
            return ( *GetManager()->blockList.root.next );
        }

        AINLINE blockIter_t GetLastMemoryBlock( void ) const
        {
            return ( *GetManager()->blockList.root.prev );
        }

        AINLINE bool HasMemoryBlocks( void ) const
        {
            return ( LIST_EMPTY( GetManager()->blockList.root ) == false );
        }

        AINLINE bool IsEndMemoryBlock( const blockIter_t& iter ) const
        {
            const CollisionlessBlockAllocator *manager = GetManager();

            return ( iter.iter_node == &manager->blockList.root );
        }

        AINLINE bool IsInAllocationRange( const memSlice_t& slice ) const
        {
            return collider.IsInAllocationRange( slice );
        }

        AINLINE blockIter_t GetAppendNode( blockIter_t node )
        {
            return node;
        }
    };

    allocSemanticsManager allocSemMan;

private:
    typedef FirstPassAllocationSemantics <numberType, allocSemanticsManager> allocSemantics;

public:
    typedef typename allocSemantics::allocInfo allocInfo;

    inline bool FindSpace( numberType sizeOfBlock, allocInfo& infoOut, const numberType alignmentWidth = sizeof( void* ), const numberType allocStart = 0 )
    {
        return allocSemantics::FindSpace( allocSemMan, sizeOfBlock, infoOut, alignmentWidth, allocStart );
    }

    inline bool ObtainSpaceAt( numberType offsetAt, numberType sizeOfBlock, allocInfo& infoOut )
    {
        return allocSemantics::ObtainSpaceAt( allocSemMan, offsetAt, sizeOfBlock, infoOut );
    }

    inline bool SetBlockSize( block_t *allocBlock, numberType newSize )
    {
        // Is there conflict?
        // If not we adjust the block.
        return ( allocSemantics::CheckMemoryBlockExtension( allocSemMan, allocBlock->node, newSize, allocBlock->slice ) );
    }

    inline void PutBlock( block_t *allocatedStruct, allocInfo& info )
    {
        allocatedStruct->slice = std::move( info.slice );
        allocatedStruct->alignment = std::move( info.alignment );

        LIST_INSERT( *info.blockToAppendAt.iter_node, allocatedStruct->node );
    }

    inline void RemoveBlock( block_t *theBlock )
    {
        LIST_REMOVE( theBlock->node );
    }

    inline void Clear( void )
    {
        LIST_CLEAR( blockList.root );
    }

    inline numberType GetSpanSize( void ) const
    {
        return allocSemantics::GetSpanSize( allocSemMan );
    }
};

// Standard allocator that allows infinite space of allocation.
struct InfiniteCollisionlessAllocProxy
{
    template <typename memSliceType>
    AINLINE bool IsInAllocationRange( const memSliceType& memSlice ) const
    {
        return true;
    }
};

template <typename numberType>
using InfiniteCollisionlessBlockAllocator = CollisionlessBlockAllocator <numberType, InfiniteCollisionlessAllocProxy>;

template <size_t memorySize>
class StaticMemoryAllocator
{
    typedef InfiniteCollisionlessBlockAllocator <size_t> blockAlloc_t;

    typedef blockAlloc_t::memSlice_t memSlice_t;

    blockAlloc_t blockRegions;

public:
    AINLINE StaticMemoryAllocator( void ) : validAllocationRegion( 0, memorySize )
    {
#ifdef _DEBUG
        memset( memData, 0, memorySize );
#endif
    }

    AINLINE ~StaticMemoryAllocator( void )
    {
        return;
    }

    AINLINE bool IsValidPointer( void *ptr )
    {
        return ( ptr >= memData && ptr <= ( memData + sizeof( memData ) ) );
    }

    AINLINE void* Allocate( size_t memSize )
    {
        // We cannot allocate zero size.
        if ( memSize == 0 )
            return nullptr;

        // Get the actual mem block size.
        size_t actualMemSize = ( memSize + sizeof( memoryEntry ) );

        blockAlloc_t::allocInfo allocInfo;

        bool hasSpace = blockRegions.FindSpace( actualMemSize, allocInfo );

        // The space allocation could fail if there is not enough space given by the size_t type.
        // This is very unlikely to happen, but must be taken care of.
        if ( hasSpace == false )
            return nullptr;

        // Make sure we allocate in the valid region.
        {
            eir::eIntersectionResult intResult = allocInfo.slice.intersectWith( validAllocationRegion );

            if ( intResult != eir::INTERSECT_EQUAL &&
                 intResult != eir::INTERSECT_INSIDE )
            {
                return nullptr;
            }
        }

        // Create the allocation information structure and return it.
        memoryEntry *entry = (memoryEntry*)( (char*)memData + allocInfo.slice.GetSliceStartPoint() );

        entry->blockSize = memSize;

        // Insert into the block manager.
        blockRegions.PutBlock( entry, allocInfo );

        return entry->GetData();
    }

    AINLINE void Free( void *ptr )
    {
        // Make sure this structure is a valid pointer from our heap.
        FATAL_ASSERT( IsValidPointer( ptr ) == true );

        // Remove the structure from existance.
        memoryEntry *entry = (memoryEntry*)ptr - 1;

        blockRegions.RemoveBlock( entry );
    }

private:
    blockAlloc_t::memSlice_t validAllocationRegion;

    struct memoryEntry : public blockAlloc_t::block_t
    {
        inline void* GetData( void )
        {
            return this + 1;
        }

        size_t blockSize;
    };

    char memData[ memorySize ];
};

#include "MemoryUtils.legacy.h"

#endif //_GLOBAL_MEMORY_UTILITIES_
