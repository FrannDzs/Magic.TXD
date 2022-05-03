/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/OSUtils.arrvmem.h
*  PURPOSE:     Virtual-memory-based array of structs
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _NATIVE_VIRTUAL_MEMORY_ARRAY_ALLOC_
#define _NATIVE_VIRTUAL_MEMORY_ARRAY_ALLOC_

#include "OSUtils.vmem.h"
#include "MemoryRaw.h"
#include "OSUtils.vmemconf.h"
#include "rwlist.hpp"
#include <string.h>

// The idea behind this class is to offer OS-backed memory storage that relies on
// no other runtime components other than itself.
// It is advised to use this in very runtime critical components that must work
// despite corruption or questionability of the program.
// When moving this class (move-constr or move-assign) you have to remember to update
// the pointer to the vmem accessor.
template <typename structType
#ifdef __linux__
    , typename syscallDispatcherType
#endif
>
struct PlatformVirtualMemoryArrayAllocator
{
    //TODO: moving this class does not safely move the vmem access struct.
    // has to be fixed by updating the reference while moving?
    // PLEASE THINK OF A VERY NATURAL WAY TO MANAGE THIS PROBLEM.

    // This class is NOT THREAD-SAFE.

private:
    typedef PlatformVirtualMemoryAccessor
#ifdef __linux__
        <syscallDispatcherType>
#endif //__linux__
        VirtualMemoryAccessorType;

public:
    inline PlatformVirtualMemoryArrayAllocator( VirtualMemoryAccessorType& vmemAccess ) noexcept : vmemAccessPtr( &vmemAccess )
    {
        // Calculate the recommended arena allocation size for virtual memory chunking.
        size_t headerSize_withoutBitmap =
            VMemArray::CalculateBitMapOffset();

        size_t reqSize_withoutBitmap =
            NativeVirtualMemoryRationalConfig::GetRecommendedArenaSize( vmemAccess, headerSize_withoutBitmap, sizeof(structType) );

        // Need to take the bitmap into account now.
        size_t bitmapSize = VMemArray::CalculateBitMapSize( reqSize_withoutBitmap );

        // Now calculate the real arena alloc size.
        size_t reqSize =
            NativeVirtualMemoryRationalConfig::GetRecommendedArenaSize( vmemAccess, headerSize_withoutBitmap + bitmapSize, sizeof(structType) );

        this->recArenaAllocSize = reqSize;
    }

    inline PlatformVirtualMemoryArrayAllocator( PlatformVirtualMemoryArrayAllocator&& right ) noexcept : vmemAccessPtr( right.vmemAccessPtr )
    {
        // Move all runtime members.
        this->vmemList = std::move( right.vmemList );
        this->recArenaAllocSize = std::move( right.recArenaAllocSize );

        // Don't forget to reset the source!
    }
    inline PlatformVirtualMemoryArrayAllocator( const PlatformVirtualMemoryArrayAllocator& right ) = delete;

private:
    AINLINE void _release_all_data( void ) noexcept
    {
        // Release all memory that has been left allocated.
        LIST_FOREACH_BEGIN( VMemArray, this->vmemList.root, managerNode )

            size_t allocSize = item->blockAllocSize;

            item->~VMemArray();

            // Release the associated memory.
            void *vmemPtr = (void*)item;

            bool gotToReleaseTerm = vmemAccessPtr->ReleaseVirtualMemory( vmemPtr, allocSize );

            FATAL_ASSERT( gotToReleaseTerm == true );

        LIST_FOREACH_END
    }

public:
    inline ~PlatformVirtualMemoryArrayAllocator( void )
    {
        _release_all_data();
    }

    // Assignment operators.
    inline PlatformVirtualMemoryArrayAllocator& operator = ( PlatformVirtualMemoryArrayAllocator&& right ) noexcept
    {
        // WARNING: this does only work under the assumption that we are not thread-safe.

        // First kill ourselves.
        this->~PlatformVirtualMemoryArrayAllocator();

        // Construct again.
        return *new (this) PlatformVirtualMemoryArrayAllocator( std::move( right ) );
    }
    inline PlatformVirtualMemoryArrayAllocator& operator = ( const PlatformVirtualMemoryArrayAllocator& right ) = delete;

    // When the vmem accessor struct moves then you have to update the internal pointer.
    inline void SetNativeVirtualMemoryAccessor( VirtualMemoryAccessorType& vmemAccess ) noexcept
    {
        this->vmemAccessPtr = &vmemAccess;
    }

    // Remember that addition can always fail due to memory shortage.
    // A very rare case if things are handled right but you should account for it anyway.
    template <typename... Args>
    inline structType* Allocate( Args&&... arguments ) noexcept(noexcept(structType( std::forward <Args> ( arguments )... )))
    {
        // Try to find a spot to allocate on.
        // If we could not find any then allocate a new array.
        LIST_FOREACH_BEGIN( VMemArray, this->vmemList.root, managerNode )

            size_t obtIdx;

            bool hasAllocatedIndex = item->ObtainAddIndex( this, obtIdx );

            if ( hasAllocatedIndex )
            {
                // Just allocate on ourselves.
                return item->PutItem( obtIdx, arguments... );
            }

        LIST_FOREACH_END

        // Allocate the new virtual memory array.
        size_t reqSize = this->recArenaAllocSize;

        void *natMem = vmemAccessPtr->RequestVirtualMemory( nullptr, reqSize );

        // We could have failed to fetch free memory, in which case this function fails.
        if ( natMem == nullptr )
        {
            return nullptr;
        }

        VirtualMemoryAccessorType& vmemAccess = GetVirtualMemoryAccess();

        VMemArray *memArray = new (natMem) VMemArray( vmemAccess, reqSize );

        // Register ourselves into the manager.
        LIST_APPEND( this->vmemList.root, memArray->managerNode );

        // Put an item on it (must not fail because there is space).
        size_t itemIndex;

        bool gotIndex = memArray->ObtainAddIndex( this, itemIndex );
        FATAL_ASSERT( gotIndex == true );

        return memArray->PutItem( itemIndex, std::forward <Args> ( arguments )... );
    }

    inline void Deallocate( structType *item ) noexcept
    {
        // See which array it has to belong to.
        size_t item_off = (size_t)item;
        memSlice_t itemSlice( item_off, sizeof(structType) );

        memSlice_t residingSlice;
        VMemArray *residingArray = nullptr;

        LIST_FOREACH_BEGIN( VMemArray, this->vmemList.root, managerNode )

            memSlice_t arrayDataSlice = item->GetDataBounds();

            eir::eIntersectionResult intResult = itemSlice.intersectWith( arrayDataSlice );

            if ( intResult == eir::INTERSECT_EQUAL ||
                 intResult == eir::INTERSECT_INSIDE )
            {
                // We found it.
                residingSlice = std::move( arrayDataSlice );
                residingArray = item;
                goto gotTheArray;
            }

        LIST_FOREACH_END

        return;

    gotTheArray:
        // We can easily determine the real item position using pointer arithmetic.
        size_t data_off = residingSlice.GetSliceStartPoint();
        size_t item_rel_off = ( item_off - data_off );
        size_t item_idx = ( item_rel_off / sizeof(structType) );

        // Release that item.
        residingArray->ReleaseItem( this, item_idx );

        // Can we remove the arena because it is empty?
        if ( residingArray->hasTakenIndex == false )
        {
            // Remove the array from activity.
            LIST_REMOVE( residingArray->managerNode );

            size_t allocSize = residingArray->blockAllocSize;

            residingArray->~VMemArray();

            // Release the memory associated with it.
            void *vmemPtr = (void*)residingArray;

            bool gotToRelease = this->vmemAccessPtr->ReleaseVirtualMemory( vmemPtr, allocSize );

            FATAL_ASSERT( gotToRelease == true );
        }
    }

    // Release all allocated memory from this allocator.
    inline void Reset( void ) noexcept
    {
        _release_all_data();

        // Clear the list.
        LIST_CLEAR( this->vmemList.root );
    }

private:
    // Specifies the alignment of data and bit-map.
    static constexpr size_t DATA_ALIGNED_BY = sizeof(void*);

    typedef sliceOfData <size_t> memSlice_t;

    VirtualMemoryAccessorType *vmemAccessPtr;

    // Recommended arena allocation size.
    // Since it does not depend on runtime parameters we do it best by caching it.
    // Computation of it is complicated anyway.
    size_t recArenaAllocSize;

    inline VirtualMemoryAccessorType& GetVirtualMemoryAccess( void ) noexcept
    {
        return *vmemAccessPtr;
    }

    // Each VMemArray item must only be handled by the manager that created it.
    // To ensure valid operation this class must remain private, implementation detail.
    struct VMemArray
    {
        // This struct should be embedded into the page it was allocated for.
        // So the "this pointer" is the start of the vmem page.

        inline VMemArray( VirtualMemoryAccessorType& vmemAccess, size_t arenaSize ) noexcept
        {
            void *vmemPtr = (void*)this;

            // Set up the bit-map.
            size_t bmpOff = CalculateBitMapOffset();        // must include space for VMemArray members.
            size_t bmpSize = CalculateBitMapSize( arenaSize );

            size_t pageSize = vmemAccess.GetPlatformPageSize();

            size_t reqOffTo_aligned = ALIGN( bmpOff + bmpSize, pageSize, pageSize );

            FATAL_ASSERT( reqOffTo_aligned <= arenaSize );

            // Commit that much virtual memory space to save meta-data and bit-map.
            bool couldCommit = VirtualMemoryAccessorType::CommitVirtualMemory( vmemPtr, reqOffTo_aligned );

            FATAL_ASSERT( couldCommit == true );

            // Write our maintenance (meta) data.
            this->blockAllocSize = arenaSize;
            this->blockCommitSize = reqOffTo_aligned;
            this->hasTakenIndex = false;
            this->maxTakenIndex = 0;

            // Zero out the bit-map.
            memset( (char*)vmemPtr + bmpOff, 0, bmpSize );

            // Data can be left dirty.
        }

        inline ~VMemArray( void )
        {
            void *vmemPtr = (void*)this;

            // Destroy all of our items.
            {
                size_t bmpOff = CalculateBitMapOffset();

                void *bmpPtr = ( (char*)vmemPtr + bmpOff );
                size_t maxItems = GetMaxItems();
                size_t bytesToRead = UINT_CEIL_DIV( maxItems, (size_t)8u );

                // Cache the data pointer.
                size_t bmpSize = CalculateBitMapSize( this->blockAllocSize );
                void *dataPtr = ( (char*)bmpPtr + bmpSize );

                for ( size_t n = 0; n < bytesToRead; n++ )
                {
                    // Read the current byte holding active bits.
                    unsigned char activityByte = *( (unsigned char*)bmpPtr + n );

                    for ( size_t bit_idx = 0; bit_idx < 8; bit_idx++ )
                    {
                        bool isTaken = ( ( activityByte >> bit_idx ) & 1 ) != 0;

                        if ( isTaken )
                        {
                            size_t actualIndex = ( n * 8 + bit_idx );

                            // Release that item.
                            structType *alive_item = ( (structType*)dataPtr + actualIndex );

                            alive_item->~structType();
                        }
                    }
                }

                // From now on out, ALL items are destroyed.
            }

            // Decommit all of our memory.
            size_t commitSize = this->blockCommitSize;

            bool couldDecommit = NativeVirtualMemoryAccessor::DecommitVirtualMemory( vmemPtr, commitSize );

            FATAL_ASSERT( couldDecommit == true );
        }

        // These helpers are used by the manager aswell.
        static inline size_t CalculateBitMapOffset( void ) noexcept
        {
            return ALIGN( sizeof(VMemArray), DATA_ALIGNED_BY, DATA_ALIGNED_BY );
        }

        static inline size_t _CalculateBitMapSize( size_t realMaxItemCount_meow ) noexcept
        {
            // Calculate the size of the bit-map.
            size_t bitMapByteCount = UINT_CEIL_DIV( realMaxItemCount_meow, (size_t)8u );

            return ALIGN( bitMapByteCount, DATA_ALIGNED_BY, DATA_ALIGNED_BY );
        }

        static inline size_t CalculateBitMapSize( size_t allocSize ) noexcept
        {
            // See how many items would be there if we could append the bit-map.
            // WARNING: do not dig recursively deep; just be happy with this rough estimate!!!
            size_t maxRegionSize = ( allocSize );

            size_t bmpOff = CalculateBitMapOffset();

            maxRegionSize -= bmpOff;

            size_t appendItemCount = ( maxRegionSize / sizeof(structType) );

            return _CalculateBitMapSize( appendItemCount );
        }

        inline void* GetBitMapAbsolutePointer( void ) noexcept
        {
            // Here we assume that each memory page start pointer is aligned the best possible.

            return (void*)( (char*)this + CalculateBitMapOffset() );
        }

        inline size_t GetMaxItems( void ) const noexcept
        {
            // This is kind of fucking around with the end-user at this point.
            // If we were to be real, then we would need to recursively calculate
            // the real required amount of bytes for the bit-map, but we choose to
            // dig only one inch: crop some chunk and be happy with it, meow!

            size_t allocSize = this->blockAllocSize;

            // Calculate the region size where we place allocated structs.
            size_t dataRegionSize = allocSize;

            // Subtract the amount of bytes to the start of the bitmap.
            size_t bmpOff = CalculateBitMapOffset();

            dataRegionSize -= bmpOff;

            // Subtract the amount of bytes for the bit-map.
            dataRegionSize -= CalculateBitMapSize( allocSize );

            // Now what is left is the data region.
            // So to say this is the real max count.
            return ( dataRegionSize / sizeof(structType) );
        }

        inline size_t GetVirtualMemoryPage( void ) const noexcept
        {
            return (size_t)( this );
        }

        inline memSlice_t GetCommitBounds( void ) const noexcept
        {
            size_t this_off = GetVirtualMemoryPage();

            return memSlice_t( this_off, this->blockCommitSize );
        }

        inline memSlice_t GetArenaBounds( void ) const noexcept
        {
            size_t this_off = GetVirtualMemoryPage();

            return memSlice_t( this_off, this->blockAllocSize );
        }

        inline memSlice_t GetDataBounds( void ) const noexcept
        {
            void *memPtr = (void*)this;

            size_t bmpOff = CalculateBitMapOffset();
            size_t bmpSize = CalculateBitMapSize( this->blockAllocSize );

            void *bmpPtr = ( (char*)memPtr + bmpOff );
            void *dataPtr = ( (char*)bmpPtr + bmpSize );

            size_t data_off = (size_t)dataPtr;
            size_t data_size = ( this->blockCommitSize - ( bmpOff + bmpSize ) );

            return memSlice_t( data_off, data_size );
        }

        inline bool ArenaIsFullyAllocated( void ) const noexcept
        {
            return ( this->blockAllocSize == this->blockCommitSize );
        }

        // Sets the new size of this container by a multiple of the native page size.
        // Since we only work on the memory we reserved this operation cannot fail.
        inline void Resize( PlatformVirtualMemoryArrayAllocator *manager, size_t newSize ) noexcept
        {
            // We can only be placed on our initial allocation.
            // This is because some platforms, namely Windows, do not support resizing of virtual memory allocations.
            FATAL_ASSERT( newSize <= this->blockAllocSize );
            FATAL_ASSERT( ( newSize % manager->GetVirtualMemoryAccess().GetPlatformPageSize() ) == 0 );

            size_t curSize = ( this->blockCommitSize );

            if ( curSize == newSize )
                return;     // nothing to do.

            void *nativePagePtr = (void*)( this );

            bool memPrepareSuccess = false;

            if ( curSize < newSize )
            {
                // Need to commit new memory.
                void *commitStartPtr = ( (char*)nativePagePtr + curSize );

                size_t commitRequestSize = ( newSize - curSize );

                memPrepareSuccess = VirtualMemoryAccessorType::CommitVirtualMemory( commitStartPtr, commitRequestSize );
            }
            else if ( curSize > newSize )
            {
                // Need to decommit memory.
                void *decommitStartPtr = ( (char*)nativePagePtr + newSize );

                size_t decommitRequestSize = ( curSize - newSize );

                memPrepareSuccess = VirtualMemoryAccessorType::DecommitVirtualMemory( decommitStartPtr, decommitRequestSize );
            }

            FATAL_ASSERT( memPrepareSuccess == true );

            // Update the commit size.
            this->blockCommitSize = newSize;

            // TODO: update the bit-map and data?
        }

    private:
        inline void CommitMemoryToCount( PlatformVirtualMemoryArrayAllocator *manager, size_t cnt_alloc ) noexcept
        {
            // Calculate the size of memory to commit.
            size_t dataSize = ( cnt_alloc * sizeof(structType) );

            size_t bmpSize = CalculateBitMapSize( this->blockAllocSize );

            size_t beforeBitmap = CalculateBitMapOffset();

            size_t memSize = ( beforeBitmap + bmpSize + dataSize );

            // Send the commit request, aligned.
            size_t pageSize = manager->GetVirtualMemoryAccess().GetPlatformPageSize();

            size_t memSize_aligned = ALIGN( memSize, pageSize, pageSize );

            Resize( manager, memSize_aligned );
        }

    public:
        inline bool ObtainAddIndex( PlatformVirtualMemoryArrayAllocator *manager, size_t& idxOut ) noexcept
        {
            // Scan the entire bit-map for an index that we can take.
            // Then mark it as taken.

            void *bmpPtr = GetBitMapAbsolutePointer();
            size_t maxItems = GetMaxItems();
            size_t bytesToRead = UINT_CEIL_DIV( maxItems, (size_t)8u );
            size_t cur_idx = 0;

            for ( size_t n = 0; n < bytesToRead; n++ )
            {
                // For each byte we can check 8 slots, because we assume that a byte consists of 8 bits.
                unsigned char& bval_ref = *( (unsigned char*)bmpPtr + n );
                unsigned char bval = bval_ref;

                for ( size_t bit_idx = 0; bit_idx < 8; bit_idx++, cur_idx++ )
                {
                    if ( cur_idx < maxItems )
                    {
                        bool isTaken = ( ( ( bval >> bit_idx ) & 1 ) != 0 );

                        if ( !isTaken )
                        {
                            // Mark as taken.
                            bval_ref = ( bval | ( 1 << bit_idx ) );

                            // Update maximum taken index.
                            if ( !this->hasTakenIndex || this->maxTakenIndex < cur_idx )
                            {
                                this->maxTakenIndex = cur_idx;
                                this->hasTakenIndex = true;

                                // Commit enough memory so we can store the new item.
                                CommitMemoryToCount( manager, cur_idx + 1 );
                            }

                            idxOut = cur_idx;
                            return true;
                        }
                    }
                }
            }

            return false;
        }

    private:
        inline void* GetDataAbsolutePointer( void ) noexcept
        {
            void *bmpPtr = GetBitMapAbsolutePointer();

            size_t bmpSize = CalculateBitMapSize( this->blockAllocSize );

            return ( (char*)bmpPtr + bmpSize );
        }

    public:
        template <typename... Args>
        inline structType* PutItem( size_t addIndex, Args&&... constrArgs ) noexcept(noexcept(structType( std::forward <Args> ( constrArgs )... )))
        {
            // We already have allocated a spot, so the point here is to put things into it.

            void *dataPtr = GetDataAbsolutePointer();

            // Put our item at the requested spot.
            void *itemPtr = ( (structType*)dataPtr + addIndex );

            return new (itemPtr) structType( std::forward <Args> ( constrArgs )... );
        }

        inline void ReleaseItem( PlatformVirtualMemoryArrayAllocator *manager, size_t idx ) noexcept
        {
            // Check if the item has indeed been taken, and if so then destruct it.
            void *bmpPtr = GetBitMapAbsolutePointer();

            size_t idxByteIndex = ( idx / 8 );
            size_t idxBitIndex = ( idx % 8 );

            unsigned char& activityByte_ref = *( (unsigned char*)bmpPtr + idxByteIndex );
            unsigned char activityByte = activityByte_ref;

            if ( ( activityByte & ( 1 << idxBitIndex ) ) == 0 )
            {
                // Not occupied. Ignore.
                return;
            }

            // Destroy the item.
            size_t bmpSize = CalculateBitMapSize( this->blockAllocSize );

            void *dataPtr = ( (char*)bmpPtr + bmpSize );

            structType *itemPtr = ( (structType*)dataPtr + idx );

            itemPtr->~structType();

            // Mark as inactive.
            activityByte_ref = ( activityByte & ~( 1 << idxBitIndex ) );

            // If we deleted the maximum, then determine the new maximum index.
            // Also calculate the new commit region to release unnecessary blocks.
            // IMPORTANT: if we delete an index, then the map is not empty, meaning there MUST be a max index!
            if ( idx == this->maxTakenIndex )
            {
                // Go backwards from the previous max index in the bit-map until we reach either the beginning
                // or an allocated index.
                size_t runIdx_byte = ( idx / 8 );
                size_t runIdx_bitIdx = ( idx % 8 );
                size_t runIdx = idx;
                bool hasIndex = false;

                while ( true )
                {
                    // Read the byte in question.
                    unsigned char valByte = *( (unsigned char*)bmpPtr + runIdx_byte );

                    while ( true )
                    {
                        // Check if this is a valid index.
                        {
                            bool isTaken = ( ( valByte >> runIdx_bitIdx ) & 1 ) != 0;

                            if ( isTaken )
                            {
                                // We found something.
                                hasIndex = true;
                                goto foundIt;
                            }
                        }

                        if ( runIdx_bitIdx == 0 )
                        {
                            break;
                        }

                        runIdx_bitIdx--;
                        runIdx--;
                    }

                    runIdx_bitIdx = 7;

                    if ( runIdx_byte == 0 )
                    {
                        break;
                    }

                    runIdx_byte--;
                }

            foundIt:
                if ( hasIndex )
                {
                    this->maxTakenIndex = runIdx;

                    // Allocate memory to the position.
                    // A position of zero is also valid.
                    CommitMemoryToCount( manager, runIdx + 1 );
                }
                else
                {
                    // Release all memory not on the header or bit-map.
                    CommitMemoryToCount( manager, 0 );
                }

                this->hasTakenIndex = hasIndex;
            }
        }

        size_t blockAllocSize;      // area that was reserved.
        size_t blockCommitSize;     // area that we currently occupy of the reservation.

        RwListEntry <VMemArray> managerNode;

        size_t maxTakenIndex;       // maximum index in the data array that is taken.
        bool hasTakenIndex;         // if the array is empty then we have none.

        // After this is the allocation bit-map and then comes the actual array of data.
        // The bitmap and the array of data are aligned by pointer size bytes.
    };

    RwList <VMemArray> vmemList;
};

template <typename structType>
using NativeVirtualMemoryArrayAllocator = PlatformVirtualMemoryArrayAllocator
    <structType
#ifdef __linux__
    , LinuxVirtualMemoryAccessorDefaultSyscalls
#endif //__linux__
    >;

#endif //_NATIVE_VIRTUAL_MEMORY_ARRAY_ALLOC_
