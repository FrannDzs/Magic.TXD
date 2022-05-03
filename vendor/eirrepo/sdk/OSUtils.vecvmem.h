/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/OSUtils.vecvmem.h
*  PURPOSE:     Virtual-memory-based vector list of structs
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _NATIVE_VIRTUAL_MEMORY_VECTOR_STRUCT_LIST_
#define _NATIVE_VIRTUAL_MEMORY_VECTOR_STRUCT_LIST_

// This concept had to be created because the virtual-memory-basing function could
// never fallback to CRT malloc-or-co so that we even have to store temporary stuff
// in virtual memory. Big problem of course is that said struct had to be growable.

#include "OSUtils.vmem.h"
#include "OSUtils.vmemconf.h"
#include "rwlist.hpp"

// Template parameters:
// * structType: the C++ type to allocate inside of this vector
// * recCacheCount: the amount of pages to keep cached at all times (if possible)
template <typename structType
#ifdef __linux__
    , typename syscallDispatcherType
#endif //__linux__
    , size_t recCacheCount = 0u>
struct PlatformVirtualMemoryVector
{
    // This class is NOT THREAD-SAFE.
    // Order of items is preserved across remove-operations.

    typedef PlatformVirtualMemoryAccessor
#ifdef __linux__
        <syscallDispatcherType>
#endif //__linux__
        VirtualMemoryAccessorType;

private:
    struct VMemVector;

    AINLINE VMemVector* make_vmem_vector( void ) noexcept
    {
        VirtualMemoryAccessorType& vmemAccess = GetNativeVirtualMemoryAccessor();

        size_t reqSize = this->recArenaAllocSize;

        void *vmemPtr = vmemAccess.RequestVirtualMemory( nullptr, reqSize );

        if ( vmemPtr != nullptr )
        {
            return new (vmemPtr) VMemVector( vmemAccess, reqSize );
        }

        return nullptr;
    }

public:
    inline PlatformVirtualMemoryVector( VirtualMemoryAccessorType& vmemAccess ) noexcept : vmemAccessPtr( &vmemAccess )
    {
        // Calculate the recommended size for virtual memory allocations.
        size_t headerSize = VMemVector::GetDataOffset();

        this->recArenaAllocSize =
            NativeVirtualMemoryRationalConfig::GetRecommendedArenaSize( vmemAccess, headerSize, sizeof(structType) );

        this->itemCount = 0;

        // Initialize the cached vectors.
        for ( size_t n = 0; n < recCacheCount; n++ )
        {
            VMemVector *newVec = make_vmem_vector();

            if ( newVec != nullptr )
            {
                LIST_APPEND( this->listCachedVectors.root, newVec->managerNode );
            }
        }
    }

    inline PlatformVirtualMemoryVector( PlatformVirtualMemoryVector&& right ) noexcept : vmemAccessPtr( right.vmemAccessPtr )   // important to JUST COPY.
    {
        // Move things over, meow.
        this->itemCount = std::move( right.itemCount );
        this->recArenaAllocSize = right.recArenaAllocSize;      // Need to copy because it is a cached value.
        this->listCachedVectors = std::move( right.listCachedVectors );
        this->listVectors = std::move( right.listVectors );

        // Don't forget to reset the source!
        right.itemCount = 0;
    }
    // Cannot copy this class because it's internal functionality depends on layout of virtual memory.
    // Could actually think of removing this dependency by value-copy only. But not implementing this
    // removes a big burden.
    inline PlatformVirtualMemoryVector( const PlatformVirtualMemoryVector& right ) = delete;

private:
    AINLINE void _release_vmem_vector( VMemVector *item ) noexcept
    {
        void *vmemPtr = (void*)item;

        size_t allocSize = item->blockAllocSize;

        // Destroy the vector.
        item->~VMemVector();

        // Release the memory.
        bool gotToRelease = this->vmemAccessPtr->ReleaseVirtualMemory( vmemPtr, allocSize );

        FATAL_ASSERT( gotToRelease == true );
    }

    inline void _release_vmem_vectors( void ) noexcept
    {
        LIST_FOREACH_BEGIN( VMemVector, this->listVectors.root, managerNode )

            _release_vmem_vector( item );

        LIST_FOREACH_END
    }

public:
    inline ~PlatformVirtualMemoryVector( void )
    {
        // Destroy all active vmem vectors.
        _release_vmem_vectors();

        // Destroy all cached vmem vectors.
        LIST_FOREACH_BEGIN( VMemVector, this->listCachedVectors.root, managerNode )

            _release_vmem_vector( item );

        LIST_FOREACH_END
    }

    inline PlatformVirtualMemoryVector& operator = ( PlatformVirtualMemoryVector&& right ) noexcept
    {
        // WARNING: only works because this class is NOT THREAD-SAFE.

        this->~PlatformVirtualMemoryVector();

        return *new (this) PlatformVirtualMemoryVector( std::move( right ) );
    }
    inline PlatformVirtualMemoryVector& operator = ( const PlatformVirtualMemoryVector& right ) = delete;

    // Regular API.
    // Could fail if there is no more virtual memory for population.
    inline bool AddItem( structType&& itemToAdd ) noexcept(noexcept(structType( std::move(itemToAdd) )))
    {
        // We need to check if the last allocated vector has a spot free for our item.
        // If it does not then we allocate a new vector and add the item to it.

        VMemVector *addToVector = nullptr;
        size_t addToVector_curCount;

        // Try to add it to the dynamic fill-em-up vectors.
        if ( !LIST_EMPTY( this->listVectors.root ) )
        {
            VMemVector *lastVec = LIST_GETITEM( VMemVector, this->listVectors.root.prev, managerNode );

            size_t curCount = lastVec->GetCount();

            if ( curCount < lastVec->GetMaxCount() )
            {
                addToVector = lastVec;
                addToVector_curCount = curCount;
                goto foundVector;
            }
        }
        else
        {
            // If dynamic vectors exist, it means that all cached vectors are full.
            // So in that case we skip the cached vectors with an easy mind.

            // Next try to add it to a cached vector.
            LIST_FOREACH_BEGIN( VMemVector, this->listCachedVectors.root, managerNode )

                size_t curCount = item->GetCount();

                if ( curCount < item->GetMaxCount() )
                {
                    addToVector = item;
                    addToVector_curCount = curCount;
                    goto foundVector;
                }

            LIST_FOREACH_END
        }

        // Try to allocate a new dynamic one.
        {
            VMemVector *newVec = make_vmem_vector();

            if ( newVec != nullptr )
            {
                // Register our new member.
                LIST_APPEND( this->listVectors.root, newVec->managerNode );

                addToVector = newVec;
                addToVector_curCount = 0;
                goto foundVector;
            }
        }

        return false;

    foundVector:
        FATAL_ASSERT( addToVector != nullptr );

        // Make sure that there is enough space committed on the vector for addition.
        size_t newCount = ( addToVector_curCount + 1 );

        VirtualMemoryAccessorType& vmemAccess = GetNativeVirtualMemoryAccessor();

        addToVector->CommitMemoryToCount( vmemAccess, newCount );

        void *dataPtr = addToVector->GetDataPointer();

        void *itemPtr = ( (structType*)dataPtr + addToVector_curCount );

        new (itemPtr) structType( std::move( itemToAdd ) );

        // We have added it successfully.
        addToVector->itemCount = newCount;

        // Keeping a global count is beneficial for debugging purposes and
        // performance. Works as long as we do not expose the internal
        // structures to the runtime.
        this->itemCount++;

        return true;
    }

    AINLINE bool AddItem( const structType& item ) noexcept(noexcept(std::move(item)))
    {
        structType copied_item = item;

        return AddItem( std::move( copied_item ) );
    }

    inline bool FindItem( const structType& itemToFind, size_t& idxOut ) const noexcept(noexcept(itemToFind == itemToFind))
    {
        // Go through each item in the vector and find the first one that
        // matches the criteria. Return the index to it.

        size_t curIdx = 0;

        LIST_FOREACH_BEGIN( VMemVector, this->listCachedVectors.root, managerNode )

            size_t subItemCount = item->GetCount();

            void *dataPtr = item->GetDataPointer();

            for ( size_t n = 0; n < subItemCount; n++, curIdx++ )
            {
                structType *someItem = ( (structType*)dataPtr + n );

                if ( *someItem == itemToFind )
                {
                    idxOut = curIdx;
                    return true;
                }
            }

        LIST_FOREACH_END

        LIST_FOREACH_BEGIN( VMemVector, this->listVectors.root, managerNode )

            size_t subItemCount = item->GetCount();

            void *dataPtr = item->GetDataPointer();

            for ( size_t n = 0; n < subItemCount; n++, curIdx++ )
            {
                structType *someItem = ( (structType*)dataPtr + n );

                if ( *someItem == itemToFind )
                {
                    idxOut = curIdx;
                    return true;
                }
            }

        LIST_FOREACH_END

        return false;
    }

    // We guarantee that removing a valid index will always work.
    // Order of items must be preserved.
    inline void RemoveItem( size_t index ) noexcept
    {
        // Go through all the vectors and remove the indexed item from them.
        // Then move all upper items one-down.

        structType *lastItem = nullptr;
        VMemVector *lastItem_vector = nullptr;
        bool lastItem_vectorIsCached = false;
        bool hasRemovedIndex = false;
        size_t curIndex = 0;
        size_t curIdxRangeOffset = 0;

        // Actually in the latest VS2017 this lambda nicely inlines to machine instructions without bullshit.
        // So I am happy about using lambdas!
        auto cbRemovalOnEachItem = [&]( VMemVector *item, bool isVectorCached )
        {
            size_t numEntries = item->itemCount;

            size_t idrange_start = ( curIdxRangeOffset );
            size_t idrange_end = ( idrange_start + numEntries );

            void *dataPtr = item->GetDataPointer();

            // If we had no index for removal yet, then we have to check if the requested index is
            // mapped inside of us. If true then we remove that item and start shifting items.
            if ( !hasRemovedIndex )
            {
                if ( idrange_start <= index && index < idrange_end )
                {
                    hasRemovedIndex = true;

                    // Initialize the first last-item for removal.
                    size_t local_off = ( index - idrange_start );

                    lastItem = ( (structType*)dataPtr + local_off );
                    lastItem_vector = item;
                    lastItem_vectorIsCached = isVectorCached;

                    curIndex = ( index + 1 );
                }
            }

            if ( hasRemovedIndex )
            {
                size_t local_curIndex = ( curIndex - idrange_start );

                while ( curIndex < idrange_end )
                {
                    structType *curEntry = ( (structType*)dataPtr + local_curIndex );

                    // Move item upward.
                    *lastItem = std::move( *curEntry );

                    lastItem = curEntry;
                    lastItem_vector = item;
                    lastItem_vectorIsCached = isVectorCached;

                    // Try next index.
                    local_curIndex++;
                    curIndex++;
                }
            }

            // Advance the idrange offset.
            curIdxRangeOffset = idrange_end;
        };

        // First look inside the cached vectors.
        LIST_FOREACH_BEGIN( VMemVector, this->listCachedVectors.root, managerNode )

            cbRemovalOnEachItem( item, true );

        LIST_FOREACH_END

        // Next check the dynamic vectors.
        LIST_FOREACH_BEGIN( VMemVector, this->listVectors.root, managerNode )

           cbRemovalOnEachItem( item, false );

        LIST_FOREACH_END

        if ( hasRemovedIndex )
        {
            // Decrease the valid size of the last vector.
            // If it is empty then we delete the vector aswell.
            size_t oldCount = ( lastItem_vector->itemCount );

            // Update the item count of the last vector.
            size_t newCount = ( oldCount - 1 );

            lastItem_vector->itemCount = newCount;

            // We delete the last entry.
            lastItem->~structType();

            VirtualMemoryAccessorType& vmemAccess = GetNativeVirtualMemoryAccessor();

            // Update the committed memory.
            lastItem_vector->CommitMemoryToCount( vmemAccess, newCount );

            // Garbage collect vectors.
            // If we are a cached vector, then we are not removed.
            if ( newCount == 0 && !lastItem_vectorIsCached )
            {
                void *vmemPtr = (void*)lastItem_vector;

                size_t allocSize = lastItem_vector->blockAllocSize;

                // Remove the vector registration.
                LIST_REMOVE( lastItem_vector->managerNode );

                // Delete the entire vector.
                lastItem_vector->~VMemVector();

                // Release it's memory.
                bool releaseSuccess = vmemAccess.ReleaseVirtualMemory( vmemPtr, allocSize );

                FATAL_ASSERT( releaseSuccess == true );
            }

            // We have one less item now.
            this->itemCount--;
        }
    }

    // We guarantee that all indices 0 to excluding count are valid.
    // Removing one index will decrease the count on success and keep
    // the order of items.
    inline size_t GetCount( void ) const noexcept
    {
        return this->itemCount;
    }

    // Since fetching a data pointer from this array is not trivial, we need
    // a special function for this job.
    inline structType* Get( size_t itemIdx ) noexcept
    {
        // Get the item with the correct the index.

        size_t currentIdxOff = 0;

        LIST_FOREACH_BEGIN( VMemVector, this->listCachedVectors.root, managerNode )

            size_t idrange_start = ( currentIdxOff );
            size_t idrange_end = ( idrange_start + item->itemCount );

            if ( itemIdx >= idrange_start && itemIdx < idrange_end )
            {
                size_t local_idx = ( itemIdx - idrange_start );

                // Just return the correct item
                structType *validItem = ( (structType*)item->GetDataPointer() + local_idx );

                return validItem;
            }

            // Advance the item range.
            currentIdxOff = idrange_end;

        LIST_FOREACH_END

        LIST_FOREACH_BEGIN( VMemVector, this->listVectors.root, managerNode )

            size_t idrange_start = ( currentIdxOff );
            size_t idrange_end = ( idrange_start + item->itemCount );

            if ( itemIdx >= idrange_start && itemIdx < idrange_end )
            {
                size_t local_idx = ( itemIdx - idrange_start );

                // Just return the correct item
                structType *validItem = ( (structType*)item->GetDataPointer() + local_idx );

                return validItem;
            }

            // Advance the item range.
            currentIdxOff = idrange_end;

        LIST_FOREACH_END

        // Not found; probably index is invalid.
        return nullptr;
    }

    // It is advised to have a for-each loop function because it is much faster than fetching
    // each valid index using Get.
    // Guarantees to iterate through EVERY ITEM WITHOUT SKIPPING.
    // The vector must not be modified during iteration.
    template <typename callbackType>
    inline void ForAllEntries( const callbackType& cb ) noexcept(noexcept(cb( *(structType*)nullptr )))
    {
        size_t currentIndex = 0;

        auto cbForAllItemsLocal = [&]( VMemVector *item )
        {
            size_t itemCount = item->itemCount;

            void *dataPtr = item->GetDataPointer();

            for ( size_t n = 0; n < itemCount; n++, currentIndex++ )
            {
                structType *validItem = ( (structType*)dataPtr + n );

                cb( *validItem );
            }
        };

        LIST_FOREACH_BEGIN( VMemVector, this->listCachedVectors.root, managerNode )

            cbForAllItemsLocal( item );

        LIST_FOREACH_END

        LIST_FOREACH_BEGIN( VMemVector, this->listVectors.root, managerNode )

            cbForAllItemsLocal( item );

        LIST_FOREACH_END
    }

    // Removes all active vmem vectors, essentially clearing the entire vector contents.
    inline void Clear( void ) noexcept
    {
        _release_vmem_vectors();

        LIST_CLEAR( this->listVectors.root );

        // Delete items from the cached vectors.
        // We actually keep the cached structures allocated tho!
        LIST_FOREACH_BEGIN( VMemVector, this->listCachedVectors.root, managerNode )

            item->Clear();

        LIST_FOREACH_END

        // We are empty.
        this->itemCount = 0;
    }

    // Meta-data API for better debugging and application-awareness.
    inline size_t GetNumVirtualMemoryVectors( void ) const noexcept
    {
        size_t count = 0;

        LIST_FOREACH_BEGIN( VMemVector, this->listVectors.root, managerNode )

            count++;

        LIST_FOREACH_END

        // Add the cached ones.
        // Since allocation of the cached vectors does not have to succeed, we
        // do have to check how many we actually have.
        LIST_FOREACH_BEGIN( VMemVector, this->listCachedVectors.root, managerNode )

            count++;

        LIST_FOREACH_END

        return count;
    }

    inline size_t GetNumCachedVirtualMemoryVectors( void ) const noexcept
    {
        return recCacheCount;
    }

    // In the event that the vmem accessor moves you need to call this function to update its position in memory.
    inline void SetNativeVirtualMemoryAccessor( VirtualMemoryAccessorType& vmemAccess ) noexcept
    {
        this->vmemAccessPtr = &vmemAccess;
    }

private:
    // The total amount of items that are registered in this vector.
    size_t itemCount;

    VirtualMemoryAccessorType *vmemAccessPtr;

    inline VirtualMemoryAccessorType& GetNativeVirtualMemoryAccessor( void ) noexcept
    {
        return *vmemAccessPtr;
    }

    size_t recArenaAllocSize;       // recommended size for virtual memory allocations.

    // Need to distribute-and-grow data across the virtual memory.
    // So we have to maintain multiple virtual memory allocations.
    struct VMemVector
    {
        static constexpr size_t DATA_ALIGNED_BY = sizeof(void*);

        inline VMemVector( VirtualMemoryAccessorType& vmemAccess, size_t allocSize ) noexcept
        {
            void *vmemPtr = (void*)this;

            // Need to commit enough space for the header.
            size_t headerSize = sizeof(*this);

            size_t pageSize = vmemAccess.GetPlatformPageSize();

            size_t reqSize = ALIGN( headerSize, pageSize, pageSize );

            FATAL_ASSERT( reqSize <= allocSize );

            VirtualMemoryAccessorType::CommitVirtualMemory( vmemPtr, reqSize );

            // Store important parameters.
            this->blockAllocSize = allocSize;
            this->blockCommitSize = reqSize;
            this->itemCount = 0;
        }

    private:
        AINLINE void _release_all_data( void ) noexcept
        {
            void *vmemPtr = (void*)this;

            size_t dataOff = GetDataOffset();

            void *dataPtr = ( (char*)vmemPtr + dataOff );

            size_t itemCount = this->itemCount;

            for ( size_t n = 0; n < itemCount; n++ )
            {
                structType *aliveItem = ( (structType*)dataPtr + n );

                aliveItem->~structType();
            }
        }

    public:
        inline ~VMemVector( void )
        {
            // Destroy all active items.
            _release_all_data();

            // Release the entire memory.
            void *vmemPtr = (void*)this;

            VirtualMemoryAccessorType::DecommitVirtualMemory( vmemPtr, this->blockAllocSize );
        }

        inline void Clear( void ) noexcept
        {
            // First clear the currently presiding data.
            _release_all_data();

            // Now set as empty.
            this->itemCount = 0;
        }

        inline size_t GetCount( void ) const noexcept
        {
            return this->itemCount;
        }

        static inline size_t GetDataOffset( void ) noexcept
        {
            size_t headerSize = sizeof(VMemVector);

            return ALIGN( headerSize, DATA_ALIGNED_BY, DATA_ALIGNED_BY );
        }

        inline size_t GetMaxCount( void ) const noexcept
        {
            // TODO: maybe allow alignment of the items inbetween?

            size_t allocSize = this->blockAllocSize;

            size_t dataOff = GetDataOffset();

            size_t dataSize = ( allocSize - dataOff );

            size_t maxCount = ( dataSize / sizeof(structType) );

            return maxCount;
        }

        inline size_t IsFull( void ) const noexcept
        {
            return ( GetCount() == GetMaxCount() );
        }

        inline void CommitMemoryToCount( VirtualMemoryAccessorType& vmemAccess, size_t cnt_alloc ) noexcept
        {
            // We start off with the header.
            size_t commitSize = GetDataOffset();

            // Add to it the size of all entries.
            commitSize += ( cnt_alloc * sizeof(structType) );

            // Align it by the page size.
            size_t pageSize = vmemAccess.GetPlatformPageSize();

            size_t commitSize_aligned = ALIGN( commitSize, pageSize, pageSize );

            // Check if our committed memory has grown or shrunk.
            // Do logic depending on it.
            size_t oldCommitSize = this->blockCommitSize;

            if ( oldCommitSize == commitSize_aligned )
                return;

            void *vmemPtr = (void*)this;

            bool memUpdateSuccess;

            if ( oldCommitSize < commitSize_aligned )
            {
                size_t sizeToGrow = ( commitSize_aligned - oldCommitSize );

                void *commitStart = ( (char*)vmemPtr + oldCommitSize );

                memUpdateSuccess = VirtualMemoryAccessorType::CommitVirtualMemory( commitStart, sizeToGrow );
            }
            else
            {
                size_t sizeToShrink = ( oldCommitSize - commitSize_aligned );

                void *decommitStart = ( (char*)vmemPtr + commitSize_aligned );

                memUpdateSuccess = VirtualMemoryAccessorType::DecommitVirtualMemory( decommitStart, sizeToShrink );
            }

            FATAL_ASSERT( memUpdateSuccess == true );

            // Remember the new commit size.
            this->blockCommitSize = commitSize_aligned;
        }

        inline void* GetDataPointer( void ) noexcept
        {
            void *vmemPtr = (void*)this;

            void *dataPtr = ( (char*)vmemPtr + GetDataOffset() );

            return dataPtr;
        }

        size_t blockAllocSize;
        size_t blockCommitSize;

        RwListEntry <VMemVector> managerNode;

        size_t itemCount;   // local item count just inside of this vmem vector.
    };

    RwList <VMemVector> listCachedVectors;      // first vectors are cached.
    RwList <VMemVector> listVectors;            // sorted by order of items in vector.
};

template <typename structType, size_t reqCacheCount = 0u>
using NativeVirtualMemoryVector = PlatformVirtualMemoryVector
    <structType
#ifdef __linux__
    , LinuxVirtualMemoryAccessorDefaultSyscalls
#endif //__linux__
    , reqCacheCount>;

#endif //_NATIVE_VIRTUAL_MEMORY_VECTOR_STRUCT_LIST_
