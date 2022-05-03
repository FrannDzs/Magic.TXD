/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwserialize.cpp
*  PURPOSE:     Serialization implementation for objects.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "pluginutil.hxx"

#include "rwserialize.hxx"

namespace rw
{

// Since we do not want to pollute the Interface class, we do things privately.
struct serializationStorePlugin
{
    RwList <serializationRegistryEntry> serializers;

    inline void Initialize( EngineInterface *engineInterface )
    {
        LIST_CLEAR( serializers.root );

        // Register common POD types.
        RegisterPODSerialization( engineInterface, CHUNK_STRUCT, L"STRUCTCHUNK_FRIENDLYNAME" );
        RegisterPODSerialization( engineInterface, CHUNK_STRING, L"STRINGCHUNK_FRIENDLYNAME" );
        RegisterPODSerialization( engineInterface, CHUNK_UNICODESTRING, L"USTRINGCHUNK_FRIENDLYNAME" );
    }

    AINLINE static void CleanupSerializer( EngineInterface *rwEngine, serializationRegistryEntry *serializer )
    {
        if ( serializer->isManaged )
        {
            RwDynMemAllocator memAlloc( rwEngine );

            eir::dyn_del_struct <serializationRegistryEntry> ( memAlloc, nullptr, serializer );
        }
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Unregister all serializers.
        LIST_FOREACH_BEGIN( serializationRegistryEntry, serializers.root, managerNode )

            // Possibly invalidates this list.
            CleanupSerializer( engineInterface, item );

        LIST_FOREACH_END

        LIST_CLEAR( serializers.root );
    }

    inline serializationStorePlugin& operator = ( const serializationStorePlugin& right ) = delete;

    inline serializationRegistryEntry* FindSerializerByChunkID( uint32 chunkID )
    {
        LIST_FOREACH_BEGIN( serializationRegistryEntry, serializers.root, managerNode )

            if ( item->chunkID == chunkID )
            {
                return item;
            }

        LIST_FOREACH_END

        return nullptr;
    }

    inline serializationProvider* BrowseForSerializer( EngineInterface *rwEngine, const void *objectToStore )
    {
        const GenericRTTI *rttiObj = rwEngine->typeSystem.GetTypeStructFromConstAbstractObject( objectToStore );

        if ( rttiObj == nullptr )
            return nullptr;

        RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rttiObj );

        LIST_FOREACH_BEGIN( serializationRegistryEntry, this->serializers.root, managerNode )
            
            eSerializationClass classType = item->classOfSerialization;

            if ( classType == eSerializationClass::OBJECT )
            {
                serializationProvider *objSer = (serializationProvider*)item;

                eSerializationTypeMode typeMode = objSer->typeMode;

                bool isOkay = false;

                if ( typeInfo )
                {
                    if ( typeMode == RWSERIALIZE_INHERIT )
                    {
                        isOkay = ( rwEngine->typeSystem.IsTypeInheritingFrom( objSer->rwType, typeInfo ) );
                    }
                    else if ( typeMode == RWSERIALIZE_ISOF )
                    {
                        isOkay = ( rwEngine->typeSystem.IsSameType( objSer->rwType, typeInfo ) );
                    }
                }

                if ( isOkay )
                {
                    return objSer;
                }
            }

        LIST_FOREACH_END

        return nullptr;
    }

    bool RegisterSerialization(
        uint32 chunkID, const wchar_t *tokenChunkName, RwTypeSystem::typeInfoBase *rwType, serializationProvider *serializer,
        eSerializationTypeMode mode
    )
    {
        if ( serializer->isRegistered == false )
        {
            serializer->chunkID = chunkID;
            serializer->tokenChunkName = tokenChunkName;
            serializer->rwType = rwType;
            serializer->typeMode = mode;

            LIST_APPEND( this->serializers.root, serializer->managerNode );

            serializer->isRegistered = true;

            return true;
        }

        return false;
    }

    void RegisterPODSerialization(
        EngineInterface *rwEngine,
        uint32 chunkID, const wchar_t *tokenChunkName
    )
    {
        serializationRegistryEntry newEntry( eSerializationClass::POD );
        newEntry.chunkID = chunkID;
        newEntry.tokenChunkName = tokenChunkName;
        newEntry.isManaged = true;

        RwDynMemAllocator memAlloc( rwEngine );

        serializationRegistryEntry *allocEntry = eir::dyn_new_struct <serializationRegistryEntry, rwEirExceptionManager> ( memAlloc, nullptr, std::move( newEntry ) );

        LIST_APPEND( this->serializers.root, allocEntry->managerNode );

        allocEntry->isRegistered = true;
    }

    

    bool UnregisterSerialization( EngineInterface *rwEngine, serializationRegistryEntry *serializer )
    {
        assert( serializer->isRegistered == true );

        LIST_REMOVE( serializer->managerNode );

        serializer->isRegistered = false;

        CleanupSerializer( rwEngine, serializer );

        return true;
    }
};

static optional_struct_space <PluginDependantStructRegister <serializationStorePlugin, RwInterfaceFactory_t>> serializationStoreRegister;

bool RegisterSerialization(
    Interface *engineInterface,
    uint32 chunkID, const wchar_t *tokenChunkName, RwTypeSystem::typeInfoBase *rwType, serializationProvider *serializer,
    eSerializationTypeMode mode
)
{
    serializationStorePlugin *serializeStore = serializationStoreRegister.get().GetPluginStruct( (EngineInterface*)engineInterface );

    if ( serializeStore )
    {
        // Make sure we do not have a serializer that handles this already.
        // It could also be a POD thing, still a conflict.
        serializationRegistryEntry *alreadyExisting = serializeStore->FindSerializerByChunkID( chunkID );

        if ( alreadyExisting == nullptr )
        {
            return serializeStore->RegisterSerialization( chunkID, tokenChunkName, rwType, serializer, mode );
        }
    }

    return false;
}

// Public method.
bool RegisterPODSerialization(
    Interface *engineInterface,
    uint32 chunkID, const wchar_t *tokenChunkName
)
{
    EngineInterface *rwEngine = (EngineInterface*)engineInterface;

    serializationStorePlugin *serializeStore = serializationStoreRegister.get().GetPluginStruct( rwEngine );

    if ( serializeStore )
    {
        serializationRegistryEntry *alreadyExisting = serializeStore->FindSerializerByChunkID( chunkID );

        if ( alreadyExisting == nullptr )
        {
            serializeStore->RegisterPODSerialization( rwEngine, chunkID, tokenChunkName );
            return true;
        }
    }

    return false;
}

// Public method.
bool UnregisterSerialization( Interface *engineInterface, uint32 chunkID )
{
    EngineInterface *rwEngine = (EngineInterface*)engineInterface;

    serializationStorePlugin *serializeStore = serializationStoreRegister.get().GetPluginStruct( rwEngine );

    if ( serializeStore )
    {
        serializationRegistryEntry *serializer = serializeStore->FindSerializerByChunkID( chunkID );
        
        if ( serializer )
        {
            return serializeStore->UnregisterSerialization( rwEngine, serializer );
        }
    }

    return false;
}

const wchar_t* GetTokenNameForChunkID( Interface *engineInterface, uint32 chunkID )
{
    serializationStorePlugin *serializeStore = serializationStoreRegister.get().GetPluginStruct( (EngineInterface*)engineInterface );

    if ( serializeStore )
    {
        serializationRegistryEntry *serializer = serializeStore->FindSerializerByChunkID( chunkID );

        if ( serializer )
        {
            return serializer->getTokenChunkName();
        }
    }

    return nullptr;
}

void Interface::SerializeBlock( RwObject *objectToStore, BlockProvider& outputProvider )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    serializationStorePlugin *serializeStore = serializationStoreRegister.get().GetPluginStruct( engineInterface );

    if ( serializeStore == nullptr )
    {
        throw NotInitializedException( eSubsystemType::SERIALIZATION, nullptr );
    }

    // Find a serializer that can handle this object.
    serializationProvider *theSerializer = serializeStore->BrowseForSerializer( engineInterface, objectToStore );

    if ( theSerializer == nullptr )
    {
        throw NotFoundException( eSubsystemType::SERIALIZATION, L"SERIALIZATION_MANAGER_FRIENDLYNAME" );
    }

    // Serialize it!
    bool requiresBlockContext = ( outputProvider.inContext() == false );

    if ( requiresBlockContext )
    {
        outputProvider.EnterContextDirect( theSerializer->getChunkID() );

        // We only set chunk meta data if we set the context ourselves.
        outputProvider.setBlockVersion( objectToStore->GetEngineVersion() );

        // TODO: maybe handle version in a special way?
    }

    try
    {
        // Call into the serializer.
        theSerializer->Serialize( this, outputProvider, objectToStore );
    }
    catch( ... )
    {
        // If any exception was triggered during serialization, we want to cleanly leave the context
        // and rethrow it.
        if ( requiresBlockContext )
        {
            outputProvider.LeaveContext();
        }

        throw;
    }

    if ( requiresBlockContext )
    {
        outputProvider.LeaveContext();
    }
}

void Interface::Serialize( RwObject *objectToStore, Stream *outputStream )
{
    BlockProvider mainBlock( outputStream, RWBLOCKMODE_WRITE );

    this->SerializeBlock( objectToStore, mainBlock );
}

RwObject* Interface::DeserializeBlock( BlockProvider& inputProvider )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    // Try reading the block and finding a serializer that can handle it.
    serializationStorePlugin *serializeStore = serializationStoreRegister.get().GetPluginStruct( engineInterface );

    if ( serializeStore == nullptr )
    {
        throw NotInitializedException( eSubsystemType::SERIALIZATION, nullptr );
    }

    // Try entering the block.
    bool requiresBlockContext = ( inputProvider.inContext() == false );

    if ( requiresBlockContext )
    {
        // We just try to read anything that goes.
        inputProvider.EnterContextAny();
    }

    // We cannot return a nullptr anymore.
    RwObject *returnObj;

    try
    {
        // Get its chunk ID and search for a serializer.
        uint32 chunkID = inputProvider.getBlockID();

        serializationRegistryEntry *serEntry = serializeStore->FindSerializerByChunkID( chunkID );

        if ( serEntry == nullptr )
        {
            throw StructuralErrorException( eSubsystemType::SERIALIZATION, L"SERIALIZATION_STRUCTERR_UNKBLOCK" );
        }

        eSerializationClass classType = serEntry->getClassOfSerialization();

        if ( classType != eSerializationClass::OBJECT )
        {
            throw StructuralErrorException( eSubsystemType::SERIALIZATION, L"SERIALIZATION_STRUCTERR_NOPODDESER" );
        }

        serializationProvider *theSerializer = (serializationProvider*)serEntry;

        // Create an object for deserialization.
        RwTypeSystem::typeInfoBase *rwTypeInfo = theSerializer->getObjectType();

        GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, rwTypeInfo, nullptr );

        if ( rtObj == nullptr )
        {
            throw SerializationTypeSystemInternalErrorException( rwTypeInfo->name, L"SERIALIZATION_INTERNERR_CONSTRFAIL" );
        }

        // Cast to the language part, the RwObject.
        RwObject *rwObj = (RwObject*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

        // Make sure we deserialize into the object version of the block.
        rwObj->SetEngineVersion( inputProvider.getBlockVersion() );

        try
        {
            // Call into the (de-)serializer.
            theSerializer->Deserialize( this, inputProvider, rwObj );
        }
        catch( ... )
        {
            // We failed for some reason, so destroy the object again.
            engineInterface->DeleteRwObject( rwObj );

            throw;
        }

        // Return our object.
        returnObj = rwObj;
    }
    catch( ... )
    {
        // We encountered an exception.
        // For that it is important to cleanly leave the context.
        if ( requiresBlockContext )
        {
            inputProvider.LeaveContext();
        }

        throw;
    }

    // Leave the context.
    if ( requiresBlockContext )
    {
        inputProvider.LeaveContext();
    }

    return returnObj;
}

// Does not return a nullptr anymore.
RwObject* Interface::Deserialize( Stream *inputStream )
{
    BlockProvider mainBlock( inputStream, RWBLOCKMODE_READ );

    return this->DeserializeBlock( mainBlock );
}

void registerSerializationPlugins( void )
{
    serializationStoreRegister.Construct( engineFactory );
}

void unregisterSerializationPlugins( void )
{
    serializationStoreRegister.Destroy();
}

};