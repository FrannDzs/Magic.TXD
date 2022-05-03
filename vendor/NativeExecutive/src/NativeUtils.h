#ifndef _NATIVE_EXECUTIVE_NATIVE_UTILS_
#define _NATIVE_EXECUTIVE_NATIVE_UTILS_

BEGIN_NATIVE_EXECUTIVE

// Implementation for very fast and synchronized data sheduling.
template <typename dataType>
struct SynchronizedHyperQueue
{
    AINLINE SynchronizedHyperQueue( CExecutiveManagerNative *natExec ) : queueArray( nullptr, 0, natExec )
    {
        return;
    }

    AINLINE void Initialize( CExecutiveManagerNative *natExec, unsigned int queueStartSize )
    {
        if ( queueStartSize == 0 )
        {
            queueStartSize = 256;
        }

        queueArray.Resize( queueStartSize );

        queueWrapSize = queueStartSize;

        // Initialize synchronization objects.
        this->sheduler_lock = natExec->CreateReadWriteLock();

        assert( this->sheduler_lock != nullptr );
    }

    AINLINE void Shutdown( CExecutiveManagerNative *natExec )
    {
        // Delete synchronization objects.
        natExec->CloseReadWriteLock( this->sheduler_lock );
    }

    AINLINE bool GetSheduledItem( dataType& item )
    {        
        CReadWriteReadContext <> ctxGet( this->sheduler_lock );

        return ( this->GetSheduledItemPreLock( item ) );
    }

    // REQUIREMENT: READ ACCESS on sheduler_lock
    AINLINE bool GetSheduledItemPreLock( dataType& item )
    {
        bool hasItem = false;

        unsigned int currentIndex = queueProcessorIndex;

        if ( currentIndex != queueShedulerIndex )
        {
            unsigned int nextIndex = currentIndex + 1;

            if ( nextIndex == queueWrapSize )
            {
                nextIndex = 0;
            }

            item = queueArray[ currentIndex ];

            hasItem = true;

            queueProcessorIndex = nextIndex;
        }

        return hasItem;
    }

    template <typename itemPositionerType>
    AINLINE void AddItemGeneric( itemPositionerType& cb )
    {
        unsigned int oldSize = queueWrapSize;

        unsigned int currentIndex = queueShedulerIndex;
        unsigned int nextIndex = currentIndex + 1;

        // Add our item.
        if ( nextIndex == oldSize )
        {
            nextIndex = 0;
        }

        // We must add the item here.
        cb.PutItem( queueArray[ currentIndex ] );

        queueShedulerIndex = nextIndex;

        // Ensure next task addition is safe.
        unsigned int currentProcessorIndex = queueProcessorIndex;

        if ( nextIndex == currentProcessorIndex )
        {
            // We need to expand the queue and shift items around.
            // For this a very expensive operation is required.
            // Ideally, this is not triggered often end-game.
            // Problems happen when the tasks cannot keep up with the sheduler thread.
            // Then this is triggered unnecessaringly often; the task creator is to blame.
            CReadWriteWriteContext <> ctxExpandQueue( this->sheduler_lock );

            // Skip this expensive operation if the sheduler has processed an item already.
            // This means that the next sheduling will be safe for sure.
            if ( currentProcessorIndex == queueProcessorIndex )
            {
                // Allocate new space.
                unsigned int maxSize = oldSize + 1;

                queueArray.Resize( maxSize + 1 );

                // Increase the queue wrap size.
                queueWrapSize++;

                // Copy items to next spots.
                unsigned int copyCount = oldSize - ( currentIndex + 1 );

                if ( copyCount != 0 )
                {
                    dataType *startCopy = &queueArray[ currentIndex + 1 ];
                    dataType *endCopy = startCopy + copyCount;
                    
                    std::copy_backward( startCopy, endCopy, endCopy + 1 );

                    // Initialize the new spot to empty.
                    *startCopy = dataType();
                }

                // Increase the processor index appropriately.
                queueProcessorIndex++;
            }
        }
    }

    struct DefaultItemPositioner
    {
        const dataType& theData;

        AINLINE DefaultItemPositioner( const dataType& theData ) : theData( theData )
        {
            return;
        }

        AINLINE void PutItem( dataType& theField )
        {
            theField = theData;
        }
    };

    AINLINE void AddItem( const dataType& theData )
    {
        DefaultItemPositioner positioner( theData );

        AddItemGeneric( positioner );
    }

    struct MoveItemPositioner
    {
        dataType&& theData;

        AINLINE MoveItemPositioner( dataType&& theData ) : theData( std::move( theData ) )
        {
            return;
        }

        AINLINE void PutItem( dataType& theField )
        {
            theField = std::move( theData );
        }
    };

    AINLINE void AddItem( dataType&& theData )
    {
        MoveItemPositioner positioner( std::move( theData ) );

        AddItemGeneric( positioner );
    }

    AINLINE CReadWriteLock* GetLock( void ) const
    {
        return this->sheduler_lock;
    }

    eir::Vector <dataType, NatExecStandardObjectAllocator> queueArray;

    unsigned int queueWrapSize;
    unsigned int queueProcessorIndex;
    unsigned int queueShedulerIndex;
    CReadWriteLock *sheduler_lock;
};

END_NATIVE_EXECUTIVE

#endif //_NATIVE_EXECUTIVE_NATIVE_UTILS_