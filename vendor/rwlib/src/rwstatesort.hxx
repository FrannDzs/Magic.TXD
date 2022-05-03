/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwstatesort.hxx
*  PURPOSE:     Template of state sorting manager
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_STATE_MANAGER_
#define _RENDERWARE_STATE_MANAGER_

#include "rwcommon.hxx"

// Temporarily we depend on the windows internals.
#include "native.win32.hxx"

// Define this to enable thread-safety for captured states of RwStateManager.
//#define RENDERWARE_STATEMAN_THREADING_SUPPORT

#ifdef _MSC_VER
#define _MSVC_VTABLE_OPTIM  __declspec(novtable)
#else
#define _MSVC_VTABLE_OPTIM
#endif

namespace rw
{

template <typename dataType>
struct IterativeSwitchedList
{
    typedef rwStaticVector <dataType> dataArray_t;
    typedef rwStaticVector <bool> switchArray_t;

    inline IterativeSwitchedList( void )
    {
        lowestFreeSlot = 0;
    }

    inline ~IterativeSwitchedList( void )
    {
        return;
    }

    inline void PutItem( const dataType& theItem )
    {
        unsigned int n = lowestFreeSlot;

        while ( true )
        {
            bool& slotActivator = allocatedSlots.ObtainItem( n );

            if ( slotActivator == false )
            {
                slotActivator = true;

                lowestFreeSlot = n + 1;
                break;
            }

            n = n + 1;
        }

        data.SetItem( n, theItem );
    }

    inline void RemoveItem( const dataType& theItem )
    {
        for ( size_t n = 0; n < data.GetCount(); n++ )
        {
            bool& slotActivator = allocatedSlots[ n ];

            if ( slotActivator && data[ n ] == theItem )
            {
                slotActivator = false;

                if ( n < lowestFreeSlot )
                {
                    lowestFreeSlot = n;
                }
                break;
            }
        }
    }

    inline void Clear( void )
    {
        for ( size_t n = 0; n < allocatedSlots.GetCount(); n++ )
        {
            allocatedSlots[ n ] = false;
        }

        lowestFreeSlot = 0;
    }

    struct itemIterator
    {
        AINLINE itemIterator( IterativeSwitchedList& manager ) : manager( manager )
        {
            iter = 0;

            FindValidSlot();
        }

        AINLINE const dataType& Resolve( void )
        {
            return manager.data[ iter ];
        }

        AINLINE void Increment( void )
        {
            iter++;

            FindValidSlot();
        }

        AINLINE void FindValidSlot( void )
        {
            while ( !IsEnd() && manager.allocatedSlots[ iter ] == false )
            {
                iter++;
            }
        }

        AINLINE bool IsEnd( void ) const
        {
            return ( iter >= manager.data.GetCount() );
        }

        size_t iter;
        IterativeSwitchedList& manager;
    };

    AINLINE itemIterator GetIterator( void )
    {
        return itemIterator( *this );
    }

    size_t lowestFreeSlot;
    dataArray_t data;
    switchArray_t allocatedSlots;
};

template <typename stateGenericType>
struct RwStateManager
{
    // Required type exports from the generic template.
    typedef typename stateGenericType::valueType stateValueType;
    typedef typename stateGenericType::stateAddress stateAddress;

    typedef scoped_rwlock_writer <> objectOrrientedLock;

    struct stateManagedType
    {
        stateValueType value;
        bool isRequired;

        AINLINE stateManagedType( void )
        {
            isRequired = false;
        }

        operator stateValueType& ( void )
        {
            return value;
        }

        operator const stateValueType& ( void ) const
        {
            return value;
        }
    };

    typedef rwStaticVector <stateManagedType> stateManagedArray;
    typedef rwStaticVector <stateAddress> stateAddressArray;
    typedef rwStaticVector <stateValueType> stateDeviceArray;

    // Forward declaration.
    struct changeSet;

    struct _MSVC_VTABLE_OPTIM deviceStateReferenceDevice abstract
    {
        virtual ~deviceStateReferenceDevice( void )     {}

        virtual bool GetDeviceState( const stateAddress& address, stateValueType& value ) const = 0;

        // Returns the parent changeSet.
        // That change set is used to update the super changeSet.
        virtual changeSet* GetUpdateChangeSet( void ) = 0;
    };

    struct localStateReferenceDevice : deviceStateReferenceDevice
    {
        RwStateManager& manager;

        AINLINE localStateReferenceDevice( RwStateManager& man ) : manager( man )
        {
            return;
        }

        AINLINE bool GetDeviceState( const stateAddress& address, stateValueType& value ) const
        {
            size_t arrayIndex = address.GetArrayIndex();

            bool success = false;

            if ( arrayIndex < manager.localStates.GetCount() )
            {
                value = manager.localStates[ arrayIndex ];

                success = true;
            }

            return success;
        }

        AINLINE changeSet* GetUpdateChangeSet( void )
        {
            return nullptr;
        }
    };

    localStateReferenceDevice _localStateRefDevice;

    AINLINE RwStateManager( EngineInterface *engineInterface ) :
        _localStateRefDevice( *this ), changeRefDevice( *this ), bucketChangeRefDevice( *this )
    {
        this->engineInterface = engineInterface;

        // Pre-allocate some entries.
        this->allocator.SummonEntries( engineInterface, 128 );
        this->changeSetAllocator.SummonEntries( engineInterface, 16 );

        // In the beginning we cannot set device states and will get only invalidated states.
        isValid = false;

        // Allocate the main change set.
        rootChangeSet = AllocateChangeSet();

        rootChangeSet->SetReferenceDevice( &changeRefDevice );

        bucketChangeSet = nullptr;
        bucketValidationChange = nullptr;
        isInBucketPass = false;

        this->localStateLock = CreateReadWriteLock( engineInterface );
    }

    AINLINE ~RwStateManager( void )
    {
        assert( isInBucketPass == false );

        while ( !LIST_EMPTY( activeChangeSets.root ) )
        {
            FreeChangeSet( LIST_GETITEM( changeSet, activeChangeSets.root.next, node ) );
        }

        // Need to clean up any cached allocators.
        this->allocator.Shutdown( this->engineInterface );
        this->changeSetAllocator.Shutdown( this->engineInterface );

        if ( rwlock *lock = this->localStateLock )
        {
            CloseReadWriteLock( engineInterface, lock );
        }
    }

    AINLINE const stateValueType& GetDeviceState( const stateAddress& address )
    {
        // If we are not valid, we return invalid device states.
        if ( !isValid )
            return stateGeneric.GetInvalidDeviceState();

        return localStates.ObtainItem( address.GetArrayIndex() );
    }

    AINLINE void LockLocalState( void )
    {
        localStateLock->enter_write();
    }

    AINLINE void UnlockLocalState( void )
    {
        localStateLock->leave_write();
    }

    AINLINE void SetDeviceState( const stateAddress& address, const stateValueType& value )
    {
        if ( !isValid )
            return;

        // Make sure threads can lock the local state.
        LockLocalState();

        {
            size_t arrayIndex = address.GetArrayIndex();

            localStates.SetItem( arrayIndex, value );

            // Notify our root change set about our change directly.
            changeSet *theRootChangeSet = this->rootChangeSet;

            theRootChangeSet->OnDeviceStateChange( address, value );

            // Notify the bucket change set, too, if it exists.
            if ( changeSet *bucketChangeSet = this->bucketChangeSet )
            {
                bucketChangeSet->OnDeviceStateChange( address, value );
            }
        }

        UnlockLocalState();
    }

    AINLINE void SetDeviceStateChecked( const stateAddress& address, const stateValueType& value )
    {
        // Only try to set device state if it is not equal to local state.
        const stateValueType& currentValue = localStates.ObtainItem( address.GetArrayIndex() );

        if ( !CompareDeviceStates( currentValue, value ) )
        {
            SetDeviceState( address, value );
        }
    }

    AINLINE void ResetDeviceState( const stateAddress& address )
    {
        stateGeneric.ResetDeviceState( address );
    }

    AINLINE bool CompareDeviceStates( const stateValueType& left, const stateValueType& right ) const
    {
        return stateGeneric.CompareDeviceStates( left, right );
    }

    AINLINE bool IsCurrentLocalState( const stateAddress& address, const stateValueType& right )
    {
        return CompareDeviceStates( GetDeviceState( address ), right );
    }

    struct changeSetLocalReferenceDevice : public deviceStateReferenceDevice
    {
        AINLINE changeSetLocalReferenceDevice( RwStateManager& manager ) : manager( manager )
        {
            return;
        }

        AINLINE bool GetDeviceState( const stateAddress& address, stateValueType& value ) const
        {
            value = manager.deviceStates.ObtainItem( address.GetArrayIndex() );
            return true;
        }

        AINLINE changeSet* GetUpdateChangeSet( void )
        {
            // There is nothing to update the root states by.
            // It must be updated directly at the spot.
            return nullptr;
        }

        RwStateManager& manager;
    };

    AINLINE void ApplyDeviceStates( void )
    {
        if ( !isValid )
            return;

        // Traverse local states to device states.
        changeSet *changeSet = rootChangeSet;

        for ( typename changeSet::changeSetIterator iterator = changeSet->GetIterator(); !iterator.IsEnd(); iterator.Increment() )
        {
            const stateAddress& address = iterator.Resolve();

            size_t arrayIndex = address.GetArrayIndex();

            // Unset the flag.
            changeSet->isQueried.SetItem( arrayIndex, false );

            // Update now.
            const stateValueType& newState = localStates[ arrayIndex ];
            stateValueType& currentState = deviceStates[ arrayIndex ];

            stateGeneric.SetDeviceState( address, newState );

            currentState = newState;
        }

        // Reset the change set.
        changeSet->Reset();
    }

    // Method should be used to ensure device state integrity.
    // MTA could leak some device states, so we need to prevent that.
    AINLINE void ValidateDeviceStates( void )
    {
        if ( !isValid )
            return;

        for ( stateAddress iterator; !iterator.IsEnd(); iterator.Increment() )
        {
            size_t arrayIndex = iterator.GetArrayIndex();

            stateValueType& localValue = deviceStates[ arrayIndex ];
            stateValueType actualValue;

            bool couldGet = stateGeneric.GetDeviceState( iterator, actualValue );

            if ( couldGet )
            {
                if ( !CompareDeviceStates( localValue, actualValue ) )
                {
#ifdef DEBUG_DEVICE_STATE_INTEGRITY
                    __debugbreak();
#endif //DEBUG_DEVICE_STATE_INTEGRITY

                    localValue = actualValue;
                }
            }
        }
    }

    struct changeSet
    {
        AINLINE changeSet( void )
        {
            referenceDevice = nullptr;
            manager = nullptr;

            hasBeenCommitted = false;
        }

        AINLINE void SetManager( RwStateManager *manager )
        {
            this->manager = manager;
        }

        AINLINE void SetReferenceDevice( deviceStateReferenceDevice *device )
        {
            referenceDevice = device;
        }

        AINLINE void Initialize( void )
        {
            for ( unsigned int n = 0; n < isQueried.GetCount(); n++ )
            {
                isQueried.SetItem( n, false );
            }
            changedStates.Clear();

            hasBeenCommitted = false;
        }

        // Must only be called by the manager!
        AINLINE void Invalidate( void )
        {
            hasBeenCommitted = false;
        }

        AINLINE void SetValid( void )
        {
            hasBeenCommitted = true;
        }

        AINLINE void CommitStates( void )
        {
            // If we have been committed already, do not bother.
            if ( hasBeenCommitted )
                return;

            changeSet *rootChangeSet = nullptr;

            if ( referenceDevice )
            {
                rootChangeSet = referenceDevice->GetUpdateChangeSet();
            }

            if ( rootChangeSet )
            {
                // Make sure that states are applied to the change set we want to check out.
                rootChangeSet->CommitStates();

                for ( changeSet::changeSetIterator iterator = rootChangeSet->GetIterator(); !iterator.IsEnd(); iterator.Increment() )
                {
                    const stateAddress& address = iterator.Resolve();

                    const stateValueType& currentValue = manager->localStates[ address.GetArrayIndex() ];

                    OnDeviceStateChange( address, currentValue );
                }
            }

            hasBeenCommitted = true;
        }

        AINLINE void SetChanged( const stateAddress& address, bool isChanged )
        {
            bool isInsideQuery = _IsChanged( address );

            isQueried.SetItem( address.GetArrayIndex(), isChanged );

            if ( isChanged )
            {
                if ( !isInsideQuery )
                {
                    changedStates.AddToBack( address );
                }
            }
            else
            {
                if ( isInsideQuery )
                {
                    changedStates.RemoveByValue( address );
                }
            }
        }

        AINLINE bool _IsChanged( const stateAddress& address )
        {
            size_t arrayIndex = address.GetArrayIndex();

            if ( arrayIndex < isQueried.GetCount() )
            {
                return isQueried[ arrayIndex ];
            }

            return false;
        }

        AINLINE bool IsChanged( const stateAddress& address )
        {
            // If we are not yet committed, do so now.
            CommitStates();

            return _IsChanged( address );
        }

        AINLINE void OnDeviceStateChange( const stateAddress& address, const stateValueType& value )
        {
            stateValueType refVal;

            bool isRequired = GetOriginalDeviceState( address, refVal );

            if ( isRequired )
            {
                SetChanged( address, !manager->CompareDeviceStates( refVal, value ) );
            }
        }

        AINLINE bool HasChanges( void )
        {
            CommitStates();

            return ( changedStates.GetCount() != 0 );
        }

        AINLINE void Reset( void )
        {
            Initialize();
        }

        AINLINE bool GetOriginalDeviceState( const stateAddress& address, stateValueType& refVal )
        {
            return referenceDevice->GetDeviceState( address, refVal );
        }

        struct changeSetIterator
        {
            AINLINE changeSetIterator( changeSet& manager ) : manager( manager )
            {
                iter = 0;
            }

            AINLINE changeSetIterator( changeSetIterator&& right ) : manager( right.manager )
            {
                iter = right.iter;
            }

            AINLINE changeSetIterator( const changeSetIterator& right ) : manager( right.manager )
            {
                iter = right.iter;
            }

            AINLINE void Increment( void )
            {
                iter++;
            }

            AINLINE bool IsEnd( void ) const
            {
                return ( iter == manager.changedStates.GetCount() );
            }

            AINLINE const stateAddress& Resolve( void )
            {
                return manager.changedStates[ iter ];
            }

            changeSet& manager;
            size_t iter;
        };

        AINLINE changeSetIterator GetIterator( void )
        {
            // Make sure we are committed.
            this->CommitStates();

            return changeSetIterator( *this );
        }

        struct updateFlag_t
        {
            AINLINE updateFlag_t( void ) = default;
            AINLINE updateFlag_t( bool value ) : value( value )
            {
                return;
            }
            AINLINE updateFlag_t( const updateFlag_t& ) = default;
            AINLINE updateFlag_t( updateFlag_t&& ) = default;

            AINLINE operator bool ( void ) const
            {
                return value;
            }

            AINLINE updateFlag_t& operator = ( bool value )
            {
                this->value = value;

                return *this;
            }
            AINLINE updateFlag_t& operator = ( updateFlag_t&& ) = default;
            AINLINE updateFlag_t& operator = ( const updateFlag_t& ) = default;

        private:
            bool value = false;
        };

        typedef rwStaticVector <updateFlag_t> updateFlagArray;

        stateAddressArray changedStates;
        updateFlagArray isQueried;

        RwListEntry <changeSet> node;

        deviceStateReferenceDevice *referenceDevice;

        RwStateManager *manager;

        bool hasBeenCommitted;
    };

    // List of changeSets.
    typedef rwStaticVector <changeSet*> changeSetList_t;

    typedef CachedConstructedClassAllocator <changeSet> changeSetAllocator_t;

    changeSetAllocator_t changeSetAllocator;

    AINLINE changeSet* AllocateChangeSet( void )
    {
        changeSet *set = changeSetAllocator.Allocate( this->engineInterface );

        set->Initialize();
        set->SetManager( this );

        LIST_INSERT( activeChangeSets.root, set->node );
        return set;
    }

    AINLINE void FreeChangeSet( changeSet *set )
    {
        LIST_REMOVE( set->node );

        changeSetAllocator.Free( this->engineInterface, set );
    }

    // Forward declaration.
    struct capturedState;

    typedef rwStaticVector <capturedState*> capturedStateList_t;

    // List of all allocated captured states.
    RwList <capturedState> activeCapturedStates;

    // List of capturedStates that are valid. They have acquired a state from the bucketStates device difference.
    // They need to be invalidated when the bucket states change.
    RwList <capturedState> validCapturedStates;

    // List of capturesStates that await synchronization by bucketValidationChange changeSet.
    // If they are not synchronized by the end of the bucket pass, they are removed from the caching cycle.
    RwList <capturedState> toBeSynchronizedStates;

    struct capturedState
    {
        struct capturedStateValueType
        {
            AINLINE capturedStateValueType( void )
            {
                isSet = false;
            }

            stateManagedType managedType;
            bool isSet;
        };

        typedef rwStaticVector <capturedStateValueType> capturedStateValueArray;

        AINLINE capturedState( void )
        {
            manager = nullptr;

            stateLock = nullptr;

            isValid = false;
            isAwaitingSynchronization = false;
            isAwaitingFullSynchronization = false;
        }

        AINLINE ~capturedState( void )
        {
            EngineInterface *engineInterface = this->manager->engineInterface;

            // Terminate ourselves if we have been intialized.
            if ( this->manager != nullptr )
            {
                Terminate();
            }

            CloseReadWriteLock( engineInterface, this->stateLock );
        }

        AINLINE void ClearSetDeviceStates( void )
        {
            // Reset our device states.
            size_t sizeCount = deviceStates.GetCount();

            for ( size_t n = 0; n < sizeCount; n++ )
            {
                capturedStateValueType& value = deviceStates[ n ];

                value.isSet = false;
            }

            // We have no device state set.
            setDeviceStates.Clear();
        }

        AINLINE void SetManager( RwStateManager *manager )
        {
            this->manager = manager;

            EngineInterface *engineInterface = manager->engineInterface;

            this->stateLock = CreateReadWriteLock( engineInterface );

            // Add ourselves to the active list.
            LIST_INSERT( manager->activeCapturedStates.root, activeListNode );

            isValid = false;
            isAwaitingSynchronization = false;
            isAwaitingFullSynchronization = false;

            ClearSetDeviceStates();
        }

        AINLINE void Terminate( void )
        {
            if ( isAwaitingSynchronization )
            {
                LIST_REMOVE( awaitSyncNode );

                isAwaitingSynchronization = false;
            }

            // We are not active anymore, so remove ourselves.
            LIST_REMOVE( activeListNode );

            // We belong to no more manager.
            this->manager = nullptr;

            Invalidate();
        }

        AINLINE void Validate( void ) const
        {
            if ( !isValid )
            {
                LIST_INSERT( manager->validCapturedStates.root, validListNode );

                isValid = true;
            }
        }

        AINLINE void OnDeviceStateSet( const stateAddress& theAddress, bool isSet ) const
        {
            if ( isSet )
            {
                setDeviceStates.AddToBack( theAddress );
            }
            else
            {
                setDeviceStates.RemoveByValue( theAddress );
            }
        }

        AINLINE void Capture( void )
        {
            {
                // We must be thread-safe.
                objectOrrientedLock lockState( stateLock );

                // Unset all states that were had before.
                ClearSetDeviceStates();

                for ( stateChangeIterator iterator( *manager ); !iterator.IsEnd(); iterator.Increment() )
                {
                    const stateAddress& address = iterator.Resolve();

                    size_t arrayIndex = address.GetArrayIndex();

                    capturedStateValueType value;

                    bool isRequired = manager->stateGeneric.IsDeviceStateRequired( address, manager->_localStateRefDevice );

                    value.managedType.isRequired = isRequired;  // whether it is required in the context of the device states.
                    value.managedType.value = manager->GetDeviceState( address );
                    value.isSet = true;

                    OnDeviceStateSet( address, true );

                    deviceStates.SetItem( arrayIndex, value );
                }
            }

            Validate();
        }

        AINLINE void Invalidate( void ) const
        {
            // Remove ourselves from the valid list if we are valid.
            if ( isValid )
            {
                LIST_REMOVE( validListNode );

                isValid = false;
            }
        }

        // MUST only be called by the MANAGER!
        AINLINE void _Invalidate( void )
        {
            isValid = false;
        }

        struct syncStateIterator
        {
            AINLINE syncStateIterator( const capturedState& manager )
            {
                partialSync = manager.isAwaitingSynchronization;
                fullSync = manager.isAwaitingFullSynchronization;

                if ( partialSync )
                {
                    partialChangeIterator = new (iterAlloc.Allocate()) typename changeSet::changeSetIterator( manager.synchronizeWithSet->GetIterator() );
                }
                else if ( fullSync )
                {
                    deviceStates = &manager.manager->bucketStates;
                }
            }

            AINLINE ~syncStateIterator( void )
            {
                if ( partialSync )
                {
                    partialChangeIterator->~changeSetIterator();
                }
            }

            AINLINE bool IsEnd( void ) const
            {
                bool isEnd = true;

                if ( partialSync )
                {
                    isEnd = ( partialChangeIterator->IsEnd() );
                }
                else if ( fullSync )
                {
                    isEnd = ( fullChangeIterator.IsEnd() || fullChangeIterator.GetArrayIndex() >= deviceStates->GetCount() );
                }

                return isEnd;
            }

            AINLINE const stateAddress& Resolve( void )
            {
                if ( partialSync )
                {
                    return partialChangeIterator->Resolve();
                }
                else if ( fullSync )
                {
                    return fullChangeIterator;
                }

                static stateAddress nullAddressPtr;

                return nullAddressPtr;
            }

            AINLINE void Increment( void )
            {
                if ( partialSync )
                {
                    partialChangeIterator->Increment();
                }
                else if ( fullSync )
                {
                    fullChangeIterator.Increment();
                }
            }

            StaticAllocator <typename changeSet::changeSetIterator, 1> iterAlloc;

            typename changeSet::changeSetIterator *partialChangeIterator;
            stateAddress fullChangeIterator;
            stateDeviceArray *deviceStates;
            bool partialSync;
            bool fullSync;
        };

        AINLINE void Synchronize( void ) const
        {
            {
                // Thread safety goes first.
                objectOrrientedLock lockState( stateLock );

                // Synchronize ourselves.
                for ( syncStateIterator iter( *this ); !iter.IsEnd(); iter.Increment() )
                {
                    const stateAddress& theAddress = iter.Resolve();

                    size_t arrayIndex = theAddress.GetArrayIndex();

                    capturedStateValueType& currentCapturedValue = deviceStates.ObtainItem( arrayIndex );

                    if ( currentCapturedValue.isSet )
                    {
                        const stateValueType& newStateValue = manager->bucketStates[ arrayIndex ];

                        bool isSame = manager->CompareDeviceStates(
                            currentCapturedValue.managedType.value,
                            newStateValue
                        );

                        if ( isSame )
                        {
                            currentCapturedValue.isSet = false;

                            OnDeviceStateSet( theAddress, false );
                        }

                        // Update required-ness.
                        currentCapturedValue.managedType.isRequired =
                            manager->stateGeneric.IsDeviceStateRequired( theAddress, manager->bucketChangeRefDevice );
                    }
                }
            }

            if ( isAwaitingSynchronization )
            {
                isAwaitingSynchronization = false;

                LIST_REMOVE( awaitSyncNode );
            }

            isAwaitingFullSynchronization = false;

            Validate();
        }

        // ONLY CALLED BY THE MANAGER!
        AINLINE void SynchronizeWith( changeSet *syncChange )
        {
            assert( isAwaitingSynchronization == false );

            synchronizeWithSet = syncChange;

            isAwaitingSynchronization = true;

            _Invalidate();
        }

        // MUST only be called by THE MANAGER!
        AINLINE void CancelSynchronization( void )
        {
            isAwaitingSynchronization = false;
            isAwaitingFullSynchronization = true;
        }

        AINLINE bool IsCurrent( void ) const
        {
            // Make sure we are synchronized.
            Synchronize();

            // Check for changes by the bucket change set.
            // This is done to find changes that are definately changed.
            for ( stateChangeIterator iterator( *manager ); !iterator.IsEnd(); iterator.Increment() )
            {
                const stateAddress& address = iterator.Resolve();

                // We definately changed if this captured state has no meaning for the changed state address.
                size_t arrayIndex = address.GetArrayIndex();

#ifdef _DEBUG
                // Get the local value just for debugging.
                const stateValueType& localValue = manager->localStates[ arrayIndex ];

                const stateValueType& bucketValue = manager->bucketStates[ arrayIndex ];

                (void)localValue;
                (void)bucketValue;

                //__noop();
#endif

                if ( arrayIndex < deviceStates.GetCount() )
                {
                    const capturedStateValueType& deviceCurrentValue = deviceStates[ arrayIndex ];

                    if ( deviceCurrentValue.isSet == false )
                    {
                        // We changed because a state that was not covered by us has changed.
                        return false;
                    }
                }
            }

            {
                // Thread safety!
                objectOrrientedLock lockState( stateLock );

                // Check things the old fashioned way.
                for ( size_t n = 0; n < setDeviceStates.GetCount(); n++ )
                {
                    const stateAddress& activeAddress = setDeviceStates[ n ];

                    size_t arrayIndex = activeAddress.GetArrayIndex();

                    const capturedStateValueType& capturedValue = deviceStates[ arrayIndex ];

                    if ( capturedValue.managedType.isRequired )
                    {
                        const stateValueType& localValue = manager->localStates[ arrayIndex ];

                        bool isChanged = !manager->CompareDeviceStates(
                            capturedValue.managedType.value,
                            localValue
                        );

                        if ( isChanged )
                        {
                            return false;
                        }
                    }
                }
            }

            return true;
        }

        AINLINE bool CompareWith( const capturedState *compareWith ) const
        {
            // Make sure that all states that are set at both sides are the same.
            // 1. same amount of set device states.
            if ( this->setDeviceStates.GetCount() != compareWith->setDeviceStates.GetCount() )
            {
                return false;
            }

            {
                objectOrrientedLock lockStateThis( stateLock );
                objectOrrientedLock lockStateRemote( compareWith->stateLock );

                // 2. same device states set at both.
                // 3. device states have the same values.
                for ( size_t n = 0; n < this->setDeviceStates.GetCount(); n++ )
                {
                    const stateAddress& address = this->setDeviceStates[ n ];

                    size_t arrayIndex = address.GetArrayIndex();

                    // Attempt to get the device state.
                    const capturedStateValueType *remoteState = nullptr;

                    if ( arrayIndex < compareWith->deviceStates.GetCount() )
                    {
                        remoteState = &compareWith->deviceStates[ arrayIndex ];
                    }

                    if ( remoteState == nullptr )
                    {
                        // compareWith is lacking a device state that this has.
                        return false;
                    }

                    // Check that the device state is set at compareWith.
                    {
                        bool isDeviceStateSetAtOther = remoteState->isSet;

                        if ( !isDeviceStateSetAtOther )
                        {
                            // A device state is missing that is set at this captured state.
                            return false;
                        }
                    }

                    // Check that the states have same values.
                    {
                        const stateValueType& thisValue = this->deviceStates[ arrayIndex ].managedType.value;
                        const stateValueType& compareValue = remoteState->managedType.value;

                        bool isChanged = !manager->CompareDeviceStates( thisValue, compareValue );

                        if ( isChanged )
                        {
                            // The device state values are not the same.
                            return false;
                        }
                    }
                }
            }

            // Success! We have the same states with the same values!
            return true;
        }

        AINLINE void SetDeviceTo( void )
        {
            //manager->RestoreToChangeSetState( stateChangeSet );
        }

        RwStateManager *manager;
        mutable capturedStateValueArray deviceStates;

        mutable stateAddressArray setDeviceStates;      // addresses of device states that are set

        RwListEntry <capturedState> activeListNode;

        typedef IterativeSwitchedList <stateAddress> fastStateAddressArray;

        mutable bool isValid;
        mutable RwListEntry <capturedState> validListNode;

        mutable bool isAwaitingSynchronization;
        mutable RwListEntry <capturedState> awaitSyncNode;
        mutable changeSet *synchronizeWithSet;

        mutable bool isAwaitingFullSynchronization;

        mutable rwlock *stateLock;
    };

    typedef CachedConstructedClassAllocator <capturedState> stateAllocator;

    stateAllocator allocator;

    AINLINE capturedState* CaptureState( void )
    {
        capturedState *state = allocator.Allocate( this->engineInterface );

        state->SetManager( this );
        state->Capture();
        return state;
    }

    AINLINE void FreeState( capturedState *state )
    {
        state->Terminate();

        allocator.Free( this->engineInterface, state );
    }

    AINLINE void Initialize( void )
    {
        // Grab the current states.
        // Also set up the local states.
        for ( stateAddress iterator; !iterator.IsEnd(); iterator.Increment() )
        {
            size_t arrayIndex = iterator.GetArrayIndex();

            stateValueType& deviceValue = deviceStates.ObtainItem( arrayIndex );

            bool couldGet = stateGeneric.GetDeviceState( iterator, deviceValue );

            if ( couldGet )
            {
                // Set up local states array.
                localStates.SetItem( arrayIndex, deviceValue );
            }
        }

        // We are now valid.
        isValid = true;

        // Reset change sets that are active.
        LIST_FOREACH_BEGIN( changeSet, activeChangeSets.root, node )
            item->Initialize();
        LIST_FOREACH_END
    }

    AINLINE void Invalidate( void )
    {
        // Invalidated.
        isValid = false;
    }

    AINLINE void ClearDevice( void )
    {
        // For all possible states, clear them.
        for ( stateAddress iterator; !iterator.IsEnd(); iterator.Increment() )
        {
            stateGeneric.ResetDeviceState( iterator );
        }
    }

    struct changeSetBucketReferenceDevice : public deviceStateReferenceDevice
    {
        AINLINE changeSetBucketReferenceDevice( RwStateManager& manager ) : manager( manager )
        {
            return;
        }

        AINLINE bool GetDeviceState( const stateAddress& address, stateValueType& value ) const
        {
            value = manager.bucketStates.ObtainItem( address.GetArrayIndex() );
            return true;
        }

        AINLINE changeSet* GetUpdateChangeSet( void )
        {
            return manager.rootChangeSet;
        }

        RwStateManager& manager;
    };

    AINLINE void BeginBucketPass( void )
    {
        assert( isInBucketPass == false );

        // Clone the current local states into the bucket reference states.
        bucketStates = localStates;

        changeSet *bucketChangeSet = this->bucketChangeSet;

        if ( !bucketChangeSet )
        {
            bucketChangeSet = AllocateChangeSet();

            bucketChangeSet->SetReferenceDevice( &bucketChangeRefDevice );

            this->bucketChangeSet = bucketChangeSet;
        }
        else
        {
            bucketChangeSet->Reset();

            // Determine the change of last time's bucket states to current.
            changeSet *validationChange = bucketValidationChange;

            if ( !validationChange )
            {
                validationChange = AllocateChangeSet();

                bucketValidationChange = validationChange;
            }
            else
            {
                validationChange->Reset();
            }

            bool hasAnyChange = false;

            for ( stateAddress address; !address.IsEnd(); address.Increment() )
            {
                size_t arrayIndex = address.GetArrayIndex();

                if ( arrayIndex < bucketStates.GetCount() )
                {
                    const stateValueType& currentBucketState = bucketStates[ arrayIndex ];
                    const stateValueType& lastBucketState = lastBucketStates[ arrayIndex ];

                    bool isChanged = !CompareDeviceStates( currentBucketState, lastBucketState );

                    if ( isChanged )
                    {
                        hasAnyChange = true;
                    }

                    validationChange->SetChanged( address, isChanged );
                }
            }

            // If there was any change at all, we need to tell our valid captured states that they must
            // validate themselves.
            if ( hasAnyChange )
            {
                // Anything that has missed validating itself to bucket states must reset
                // and recapture its context completely.
                if ( !LIST_EMPTY( toBeSynchronizedStates.root ) )
                {
                    LIST_FOREACH_BEGIN( capturedState, toBeSynchronizedStates.root, awaitSyncNode )
                        item->CancelSynchronization();
                    LIST_FOREACH_END

                    LIST_CLEAR( toBeSynchronizedStates.root );
                }

                // Valid device states have to be told to revalidate themselves.
                if ( !LIST_EMPTY( validCapturedStates.root ) )
                {
                    LIST_FOREACH_BEGIN( capturedState, validCapturedStates.root, validListNode )
                        item->SynchronizeWith( validationChange );

                        LIST_INSERT( toBeSynchronizedStates.root, item->awaitSyncNode );
                    LIST_FOREACH_END

                    // Hence we have no more valid captured states.
                    LIST_CLEAR( validCapturedStates.root );
                }
            }
        }

        // Keep track of last time's bucket states.
        bucketStates = lastBucketStates;

        isInBucketPass = true;
    }

    AINLINE changeSet* GetBucketChangeSet( void )
    {
        return bucketChangeSet;
    }

    stateAddressArray changedStateAddresses;

    AINLINE void RestoreToChangeSetState( changeSet *set )
    {
        // Set device states to the state when the change set started from.
        for ( typename changeSet::changeSetIterator iterator = set->GetIterator(); !iterator.IsEnd(); iterator.Increment() )
        {
            changedStateAddresses.AddItem( iterator.Resolve() );
        }

        for ( size_t n = 0; n < changedStateAddresses.GetCount(); n++ )
        {
            const stateAddress& changedAddress = changedStateAddresses[ n ];

            stateValueType origValue;

            if ( set->GetOriginalDeviceState( changedAddress, origValue ) )
            {
                SetDeviceState( changedAddress, origValue );
            }
        }
        changedStateAddresses.Clear();
    }

    AINLINE void SetBucketStates( void )
    {
        RestoreToChangeSetState( GetBucketChangeSet() );
    }

    AINLINE void EndBucketPass( void )
    {
        assert( isInBucketPass == true );

        isInBucketPass = false;
    }

    struct stateChangeIterator
    {
        AINLINE stateChangeIterator( RwStateManager& manager )
        {
            changeSet *set = manager.GetBucketChangeSet();

            if ( set )
            {
                isChangeSetIterator = true;

                changeSetIterator = new (staticAlloc.Allocate()) typename changeSet::changeSetIterator( set->GetIterator() );
            }
            else
            {
                isChangeSetIterator = false;
            }
        }

        AINLINE ~stateChangeIterator( void )
        {
            if ( isChangeSetIterator )
            {
                changeSetIterator->~changeSetIterator();
            }
        }

        AINLINE void Increment( void )
        {
            if ( isChangeSetIterator )
            {
                changeSetIterator->Increment();
            }
            else
            {
                stateIterator.Increment();
            }
        }

        AINLINE bool IsEnd( void ) const
        {
            if ( isChangeSetIterator )
            {
                return changeSetIterator->IsEnd();
            }
            else
            {
                return stateIterator.IsEnd();
            }
        }

        AINLINE const stateAddress& Resolve( void )
        {
            if ( isChangeSetIterator )
            {
                return changeSetIterator->Resolve();
            }
            else
            {
                return stateIterator;
            }
        }

        StaticAllocator <typename changeSet::changeSetIterator, 1> staticAlloc;

        bool isChangeSetIterator;
        typename changeSet::changeSetIterator *changeSetIterator;
        stateAddress stateIterator;
    };

    // RenderWare environment.
    EngineInterface *engineInterface;

    // Interface to access the device states.
    stateGenericType stateGeneric;

    // Root change set used for updating to the device.
    changeSetLocalReferenceDevice changeRefDevice;
    changeSet* rootChangeSet;

    // Variables for bucket management.
    stateDeviceArray lastBucketStates;  // last bucket states
    stateDeviceArray bucketStates;  // current bucket states
    changeSet* bucketChangeSet;     // change of local states in comparison to current bucket states.
    changeSetBucketReferenceDevice bucketChangeRefDevice;
    bool isInBucketPass;
    changeSet* bucketValidationChange;  // change from last to current bucket states

    RwList <changeSet> activeChangeSets;

    stateDeviceArray localStates;
    stateDeviceArray deviceStates;

    // Boolean whether the state manager is valid.
    // It it valid if the device is valid.
    bool isValid;

    // Lock object used by state management.
    rwlock *localStateLock;
};

}

#endif //_RENDERWARE_STATE_MANAGER_
