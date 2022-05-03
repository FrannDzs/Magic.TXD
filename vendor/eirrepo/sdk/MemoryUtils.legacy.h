/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MemoryUtils.legacy.h
*  PURPOSE:     Old memory utility templates
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// Here we put types that really should be removed from general usage but are still required due
// to their spread.

#ifndef _MEMORY_UTILITIES_LEGACY_HEADER_
#define _MEMORY_UTILITIES_LEGACY_HEADER_

#include <algorithm>
#include <stddef.h>

#include "eirutils.h"

// Array implementation that extends on concepts found inside GTA:SA
// NOTE: This array type is a 'trusted type'.
// -> Use it whenever necessary.
// WARNING (2016, The_GTA): this type is in need of a major overhaul due to violating modern C++ principles!
template <typename dataType, unsigned int pulseCount, unsigned int allocFlags, typename arrayMan, typename countType, typename allocatorType>
struct growableArrayEx
{
    typedef dataType dataType_t;

    allocatorType _memAllocator;

    AINLINE void* _memAlloc( size_t memSize, unsigned int flags )
    {
        return _memAllocator.Allocate( memSize, flags );
    }

    AINLINE void* _memRealloc( void *memPtr, size_t memSize, unsigned int flags )
    {
        return _memAllocator.Realloc( memPtr, memSize, flags );
    }

    AINLINE void _memFree( void *memPtr )
    {
        _memAllocator.Free( memPtr );
    }

    AINLINE growableArrayEx( void )
    {
        data = nullptr;
        numActiveEntries = 0;
        sizeCount = 0;
    }

    AINLINE growableArrayEx( allocatorType allocNode ) : _memAllocator( std::move( allocNode ) )
    {
        data = nullptr;
        numActiveEntries = 0;
        sizeCount = 0;
    }

    AINLINE growableArrayEx( growableArrayEx&& right )
    {
        this->data = right.data;
        this->numActiveEntries = right.numActiveEntries;
        this->sizeCount = right.sizeCount;

        right.data = nullptr;
        right.numActiveEntries = 0;
        right.sizeCount = 0;
    }

    AINLINE growableArrayEx( const growableArrayEx& right )
    {
        this->data = nullptr;
        this->numActiveEntries = 0;
        this->sizeCount = 0;

        operator = ( right );
    }

    AINLINE void operator = ( const growableArrayEx& right )
    {
        SetSizeCount( right.GetSizeCount() );

        // Copy all data over.
        if ( sizeCount != 0 )
        {
            std::copy( right.data, right.data + sizeCount, data );
        }

        // Set the number of active entries.
        numActiveEntries = right.numActiveEntries;
    }

    AINLINE void operator = ( growableArrayEx&& right )
    {
        SetSizeCount( 0 );

        this->data = right.data;
        this->numActiveEntries = right.numActiveEntries;
        this->sizeCount = right.sizeCount;

        right.data = nullptr;
        right.numActiveEntries = 0;
        right.sizeCount = 0;
    }

    AINLINE void SetArrayCachedTo( growableArrayEx& target )
    {
        countType targetSizeCount = GetSizeCount();
        countType oldTargetSizeCount = target.GetSizeCount();

        target.AllocateToIndex( targetSizeCount );

        if ( targetSizeCount != 0 )
        {
            std::copy( data, data + targetSizeCount, target.data );

            // Anything that is above the new target size count must be reset.
            for ( countType n = targetSizeCount; n < oldTargetSizeCount; n++ )
            {
                dataType *theField = &target.data[ n ];

                // Reset it.
                theField->~dataType();

                new (theField) dataType;

                // Tell it to the manager.
                manager.InitField( *theField );
            }
        }

        // Set the number of active entries.
        target.numActiveEntries = numActiveEntries;
    }

    AINLINE ~growableArrayEx( void )
    {
        Shutdown();
    }

    AINLINE void Init( void )
    { }

    AINLINE void Shutdown( void )
    {
        if ( data )
            SetSizeCount( 0 );

        numActiveEntries = 0;
        sizeCount = 0;
    }

    AINLINE void SetSizeCount( countType index )
    {
        if ( index != sizeCount )
        {
            countType oldCount = sizeCount;

            sizeCount = index;

            if ( data )
            {
                // Destroy any structures that got removed.
                for ( countType n = index; n < oldCount; n++ )
                {
                    data[n].~dataType();
                }
            }

            if ( index == 0 )
            {
                // Handle clearance requests.
                if ( data )
                {
                    _memFree( data );

                    data = nullptr;
                }
            }
            else
            {
                size_t newArraySize = sizeCount * sizeof( dataType );

                if ( !data )
                    data = (dataType*)_memAlloc( newArraySize, allocFlags );
                else
                    data = (dataType*)_memRealloc( data, newArraySize, allocFlags );
            }

            if ( data )
            {
                // FIXME: here is a FATAL ERROR.
                // Pointers to items inside of growableArray cannot be PRESERVED.
                // Hence growableArray is NO LONGER C++ SAFE since the introduction
                // of move semantics!

                // Fill the gap.
                for ( countType n = oldCount; n < index; n++ )
                {
                    new (&data[n]) dataType;

                    manager.InitField( data[n] );
                }
            }
            else
                sizeCount = 0;
        }
    }

    AINLINE void AllocateToIndex( countType index )
    {
        if ( index >= sizeCount )
        {
            SetSizeCount( index + ( pulseCount + 1 ) );
        }
    }

    AINLINE void SetItem( const dataType& dataField, countType index )
    {
        AllocateToIndex( index );

        data[index] = dataField;
    }

    AINLINE void SetItem( dataType&& dataField, countType index )
    {
        AllocateToIndex( index );

        data[index] = std::move( dataField );
    }

    AINLINE void SetFast( const dataType& dataField, countType index )
    {
        // God mercy the coder knows why and how he is using this.
        // We might introduce a hyper-paranoid assertion that even checks this...
        data[index] = dataField;
    }

    AINLINE void SetFast( dataType&& dataField, countType index )
    {
        // :(
        data[index] = std::move( dataField );
    }

    AINLINE dataType& GetFast( countType index ) const
    {
        // and that.
        return data[index];
    }

    AINLINE void AddItem( const dataType& data )
    {
        SetItem( data, numActiveEntries );

        numActiveEntries++;
    }

    AINLINE void AddItem( dataType&& data )
    {
        SetItem( std::move( data ), numActiveEntries );

        numActiveEntries++;
    }

private:
    template <typename insertCB>
    AINLINE void InsertItemPrepare( insertCB& cb, size_t insertIndex )
    {
        // Make sure we have enough space allocated.
        size_t numItems = this->numActiveEntries;

        // Input argument has to be in range.
        insertIndex = std::min( numItems, insertIndex );

        this->AllocateToIndex( numItems );

        // Move items one up.
        size_t moveUpStart = numItems;

        while ( moveUpStart > insertIndex )
        {
            this->data[ moveUpStart ] = std::move( this->data[ moveUpStart - 1 ] );

            moveUpStart--;
        }

        // Put in the new item.
        cb.Put( this->data[ moveUpStart ] );

        this->numActiveEntries++;
    }

public:
    AINLINE void InsertItem( dataType&& data, size_t insertIndex )
    {
        struct itemMovePutCB
        {
            AINLINE itemMovePutCB( dataType&& data ) : data( std::move( data ) )
            {
                return;
            }

            AINLINE void Put( dataType& dataOut )
            {
                dataOut = std::move( data );
            }

        private:
            dataType&& data;
        };

        itemMovePutCB cb( std::move( data ) );

        InsertItemPrepare( cb, insertIndex );
    }

    AINLINE void InsertItem( const dataType& data, size_t insertIndex )
    {
        struct insertCopyPutCB
        {
            AINLINE insertCopyPutCB( const dataType& data ) : data( data )
            {
                return;
            }

            AINLINE void Put( dataType& data )
            {
                data = this->data;
            }

        private:
            const dataType& data;
        };

        insertCopyPutCB cb( data );

        InsertItemPrepare( cb, insertIndex );
    }

    AINLINE dataType& ObtainItem( countType obtainIndex )
    {
        AllocateToIndex( obtainIndex );

        return data[obtainIndex];
    }

    AINLINE dataType& ObtainItem( void )
    {
        return ObtainItem( numActiveEntries++ );
    }

    AINLINE countType GetCount( void ) const
    {
        return numActiveEntries;
    }

    AINLINE countType GetSizeCount( void ) const
    {
        return sizeCount;
    }

    AINLINE dataType& Get( countType index )
    {
        FATAL_ASSERT( index < sizeCount );

        return data[index];
    }

    AINLINE const dataType& Get( countType index ) const
    {
        FATAL_ASSERT( index < sizeCount );

        return data[index];
    }

    AINLINE bool Front( dataType& outVal ) const
    {
        bool success = ( GetCount() != 0 );

        if ( success )
        {
            outVal = data[ 0 ];
        }

        return success;
    }

    AINLINE bool Tail( dataType& outVal ) const
    {
        countType count = GetCount();

        bool success = ( count != 0 );

        if ( success )
        {
            outVal = data[ count - 1 ];
        }

        return success;
    }

    AINLINE bool Pop( dataType& item )
    {
        if ( numActiveEntries != 0 )
        {
            item = data[--numActiveEntries];
            return true;
        }

        return false;
    }

    AINLINE bool Pop( void )
    {
        if ( numActiveEntries != 0 )
        {
            --numActiveEntries;
            return true;
        }

        return false;
    }

    AINLINE void RemoveItem( countType foundSlot )
    {
        FATAL_ASSERT( foundSlot >= 0 && foundSlot < numActiveEntries );
        FATAL_ASSERT( numActiveEntries != 0 );

        countType moveCount = numActiveEntries - ( foundSlot + 1 );

        if ( moveCount != 0 )
        {
            FSDataUtil::copy_impl( data + foundSlot + 1, data + numActiveEntries, data + foundSlot );
        }

        numActiveEntries--;
    }

    AINLINE bool RemoveItem( const dataType& item )
    {
        countType foundSlot = -1;

        if ( !Find( item, foundSlot ) )
            return false;

        RemoveItem( foundSlot );
        return true;
    }

    AINLINE bool Find( const dataType& inst, countType& indexOut ) const
    {
        for ( countType n = 0; n < numActiveEntries; n++ )
        {
            if ( data[n] == inst )
            {
                indexOut = n;
                return true;
            }
        }

        return false;
    }

    AINLINE bool Find( const dataType& inst ) const
    {
        countType trashIndex;

        return Find( inst, trashIndex );
    }

    AINLINE unsigned int Count( const dataType& inst ) const
    {
        unsigned int count = 0;

        for ( countType n = 0; n < numActiveEntries; n++ )
        {
            if ( data[n] == inst )
                count++;
        }

        return count;
    }

    AINLINE void Clear( void )
    {
        numActiveEntries = 0;
    }

    AINLINE void TrimTo( countType indexTo )
    {
        if ( numActiveEntries > indexTo )
            numActiveEntries = indexTo;
    }

    AINLINE void SwapContents( growableArrayEx& right )
    {
        dataType *myData = this->data;
        dataType *swapData = right.data;

        this->data = swapData;
        right.data = myData;

        countType myActiveCount = this->numActiveEntries;
        countType swapActiveCount = right.numActiveEntries;

        this->numActiveEntries = swapActiveCount;
        right.numActiveEntries = myActiveCount;

        countType mySizeCount = this->sizeCount;
        countType swapSizeCount = right.sizeCount;

        this->sizeCount = swapSizeCount;
        right.sizeCount = mySizeCount;
    }

    AINLINE void SetContents( growableArrayEx& right )
    {
        right.SetSizeCount( numActiveEntries );

        for ( countType n = 0; n < numActiveEntries; n++ )
            right.data[n] = data[n];

        right.numActiveEntries = numActiveEntries;
    }

    dataType* data;
    countType numActiveEntries;
    countType sizeCount;
    arrayMan manager;
};

template <typename dataType>
struct iterativeGrowableArrayExManager
{
    AINLINE void InitField( dataType& theField )
    {
        return;
    }
};

template <typename dataType, unsigned int pulseCount, unsigned int allocFlags, typename countType, typename allocatorType>
using iterativeGrowableArrayEx = growableArrayEx <dataType, pulseCount, allocFlags, iterativeGrowableArrayExManager <dataType>, countType, allocatorType>;

#endif //_MEMORY_UTILITIES_LEGACY_HEADER_