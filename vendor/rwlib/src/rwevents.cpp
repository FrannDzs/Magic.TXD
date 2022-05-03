/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwevents.cpp
*  PURPOSE:     Event system implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "pluginutil.hxx"

namespace rw
{

// We want to hold a list of all event handlers in any RwObject.
struct eventSystemManager
{
    struct objectEvents
    {
        struct eventEntry
        {
            struct handler_t
            {
                EventHandler_t cb;
                void *ud;

                inline static eir::eCompResult compare_values( const handler_t& left, const handler_t& right ) noexcept
                {
                    return eir::DefaultValueCompare( (void*)left.cb, (void*)right.cb );
                }
            };

            typedef rwStaticSet <handler_t, handler_t> handlers_t;

            handlers_t eventHandlers;
        };

    private:
        static Interface* GetEngineFromGenericObj( GenericRTTI *rtObj )
        {
            RwObject *rwObj = (RwObject*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

            return rwObj->engineInterface;
        }

    public:
        inline objectEvents( GenericRTTI *rtObj ) : events( eir::constr_with_alloc::DEFAULT, GetEngineFromGenericObj( rtObj ) )
        {
            return;            
        }

        inline void Initialize( GenericRTTI *rtObj )
        {
            RwObject *rwObj = (RwObject*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

            // We start out with an empty map of events.
            this->evtLock = CreateReadWriteLock( rwObj->engineInterface );
        }

        inline void Shutdown( GenericRTTI *rtObj )
        {
            RwObject *rwObj = (RwObject*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

            // Memory is cleared automatically.
            if ( rwlock *lock = this->evtLock )
            {
                CloseReadWriteLock( rwObj->engineInterface, lock );
            }
        }

        inline void operator = ( const objectEvents& right )
        {
            // Assigning object event handlers makes no sense.
        }

        inline void RegisterEventHandler( event_t eventID, EventHandler_t handler, void *ud )
        {
            scoped_rwlock_writer <rwlock> lock( this->evtLock );

            eventEntry& info = events[ eventID ];

            // Only register if not already registered.
            eventEntry::handler_t item;
            item.cb = handler;
            item.ud = ud;

            if ( info.eventHandlers.Find( item ) == nullptr )
            {
                info.eventHandlers.Insert( std::move( item ) );
            }
        }

        inline void UnregisterEventHandler( event_t eventID, EventHandler_t handler )
        {
            scoped_rwlock_writer <rwlock> lock( this->evtLock );

            eventMap_t::Node *foundNode = events.Find( eventID );

            if ( foundNode == nullptr )
                return; // not found.

            bool shouldRemoveEventEntryIter = false;
            {
                eventEntry& evtEntry = foundNode->GetValue();

                eventEntry::handler_t findHandler;
                findHandler.cb = handler;
                findHandler.ud = nullptr;

                auto *node = evtEntry.eventHandlers.Find( findHandler );

                if ( node != nullptr )
                {
                    evtEntry.eventHandlers.RemoveNode( node );

                    if ( evtEntry.eventHandlers.IsEmpty() )
                    {
                        // Remove this entry.
                        shouldRemoveEventEntryIter = true;
                    }
                }
            }

            if ( shouldRemoveEventEntryIter )
            {
                events.RemoveNode( foundNode );
            }
        }

        inline bool TriggerEvent( RwObject *obj, event_t eventID, void *ud )
        {
            scoped_rwlock_reader <rwlock> lock( this->evtLock );

            eventMap_t::Node *eventNode = this->events.Find( eventID );

            if ( eventNode == nullptr )
                return false; // not found.

            eventEntry& evtEntry = eventNode->GetValue();

            bool wasHandled = false;

            for ( eventEntry::handlers_t::const_iterator iter( evtEntry.eventHandlers ); !iter.IsEnd(); iter.Increment() )
            {
                // Call it.
                const eventEntry::handler_t& curHandler = iter.Resolve()->GetValue();

                curHandler.cb( obj, eventID, ud, curHandler.ud );

                wasHandled = true;
            }

            return wasHandled;
        }

        typedef rwMap <event_t, eventEntry> eventMap_t;

        eventMap_t events;

        rwlock *evtLock;
    };

    inline void Initialize( EngineInterface *engineInterface )
    {
        this->pluginOffset =
            engineInterface->typeSystem.RegisterDependantStructPlugin <objectEvents> ( engineInterface->rwobjTypeInfo, RwTypeSystem::ANONYMOUS_PLUGIN_ID );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        engineInterface->typeSystem.UnregisterPlugin( engineInterface->rwobjTypeInfo, this->pluginOffset );
    }

    inline objectEvents* GetPluginStruct( EngineInterface *engineInterface, RwObject *obj )
    {
        GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( obj );

        return RwTypeSystem::RESOLVE_STRUCT <objectEvents> ( engineInterface, rtObj, engineInterface->rwobjTypeInfo, this->pluginOffset );
    }

    RwTypeSystem::pluginOffset_t pluginOffset;
};

static optional_struct_space <PluginDependantStructRegister <eventSystemManager, RwInterfaceFactory_t>> eventSysRegister;

void RegisterEventHandler( RwObject *obj, event_t eventID, EventHandler_t theHandler, void *ud )
{
    EngineInterface *engineInterface = (EngineInterface*)obj->engineInterface;

    if ( eventSystemManager *eventSys = eventSysRegister.get().GetPluginStruct( engineInterface ) )
    {
        // Put the event handler into the holder plugin.
        if ( eventSystemManager::objectEvents *objReg = eventSys->GetPluginStruct( engineInterface, obj ) )
        {
            objReg->RegisterEventHandler( eventID, theHandler, ud );
        }
    }
}

void UnregisterEventHandler( RwObject *obj, event_t eventID, EventHandler_t theHandler )
{
    EngineInterface *engineInterface = (EngineInterface*)obj->engineInterface;

    if ( eventSystemManager *eventSys = eventSysRegister.get().GetPluginStruct( engineInterface ) )
    {
        // Put the event handler into the holder plugin.
        if ( eventSystemManager::objectEvents *objReg = eventSys->GetPluginStruct( engineInterface, obj ) )
        {
            objReg->UnregisterEventHandler( eventID, theHandler );
        }
    }
}

bool TriggerEvent( RwObject *obj, event_t eventID, void *ud )
{
    if ( !AcquireObject( obj ) )
    {
        throw InternalErrorException( eSubsystemType::EVENTSYS, L"EVENTSYS_REFCNTFAIL" );
    }

    EngineInterface *engineInterface = (EngineInterface*)obj->engineInterface;

    bool wasHandled = false;

    // Trigger all event handlers that count for this eventID.
    if ( eventSystemManager *eventSys = eventSysRegister.get().GetPluginStruct( engineInterface ) )
    {
        if ( eventSystemManager::objectEvents *objReg = eventSys->GetPluginStruct( engineInterface, obj ) )
        {
            // TODO: when we create object hierarchies, we want to trigger
            // events for the parents aswell!

            wasHandled = objReg->TriggerEvent( obj, eventID, ud );
        }
    }

    // Release the object reference.
    // This may destroy the object, so be careful!
    ReleaseObject( obj );

    return wasHandled;
}

void registerEventSystem( void )
{
    eventSysRegister.Construct( engineFactory );
}

void unregisterEventSystem( void )
{
    eventSysRegister.Destroy();
}

};