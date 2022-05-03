/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwserialize.hxx
*  PURPOSE:     RenderWare serialization environment
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_SERIALIZATION_PRIVATE_
#define _RENDERWARE_SERIALIZATION_PRIVATE_

namespace rw
{

enum eSerializationTypeMode
{
    RWSERIALIZE_INHERIT,
    RWSERIALIZE_ISOF
};

enum class eSerializationClass
{
    POD,
    OBJECT
};

// Registration type node into the serialization environment.
struct serializationRegistryEntry
{
    friend struct serializationStorePlugin;

    inline serializationRegistryEntry( eSerializationClass classOfSerialization ) noexcept
    {
        this->classOfSerialization = classOfSerialization;
        this->isRegistered = false;
        this->isManaged = false;
    }
    inline serializationRegistryEntry( serializationRegistryEntry&& right ) noexcept
    {
        this->classOfSerialization = right.classOfSerialization;
        this->chunkID = right.chunkID;
        this->tokenChunkName = right.tokenChunkName;
        this->isManaged = right.isManaged;

        if ( right.isRegistered )
        {
            this->managerNode.moveFrom( std::move( right.managerNode ) );
            this->isRegistered = true;
        }
        else
        {
            this->isRegistered = false;
        }

        right.isRegistered = false;
    }
    inline serializationRegistryEntry( const serializationRegistryEntry& ) = delete;

    virtual ~serializationRegistryEntry( void )
    {
        if ( this->isRegistered )
        {
            LIST_REMOVE( this->managerNode );
        }
    }

    inline serializationRegistryEntry& operator = ( serializationRegistryEntry&& ) = delete;
    inline serializationRegistryEntry& operator = ( const serializationRegistryEntry& ) = delete;

    inline eSerializationClass getClassOfSerialization( void ) const
    {
        return this->classOfSerialization;
    }

    inline uint32 getChunkID( void ) const
    {
#ifdef _DEBUG
        assert( this->isRegistered == true );
#endif //_DEBUG

        return this->chunkID;
    }

    inline const wchar_t* getTokenChunkName( void ) const
    {
#ifdef _DEBUG
        assert( this->isRegistered == true );
#endif //_DEBUG

        return this->tokenChunkName;
    }

private:
    RwListEntry <serializationRegistryEntry> managerNode;

    eSerializationClass classOfSerialization;
    uint32 chunkID;
    const wchar_t *tokenChunkName;

    bool isRegistered;
    bool isManaged;
};

// Main chunk serialization interface.
// Allows you to store data in the RenderWare ecosystem, be officially registering it.
struct serializationProvider abstract : public serializationRegistryEntry
{
    friend struct serializationStorePlugin;

    inline serializationProvider( void ) noexcept
        : serializationRegistryEntry( eSerializationClass::OBJECT )
    {
        return;
    }

    // This interface is used to save the contents of the RenderWare engine to disk.
    virtual void            Serialize( Interface *engineInterface, BlockProvider& outputProvider, void *objectToSerialize ) const = 0;
    virtual void            Deserialize( Interface *engineInterface, BlockProvider& inputProvider, void *objectToDeserialize ) const = 0;

    inline RwTypeSystem::typeInfoBase* getObjectType( void ) const
    {
        return this->rwType;
    }

private:
    eSerializationTypeMode typeMode;
    RwTypeSystem::typeInfoBase *rwType;
};

// Internal serialization type registration API.
bool RegisterSerialization(
    Interface *engineInterface,
    uint32 chunkID, const wchar_t *tokenChunkName, RwTypeSystem::typeInfoBase *rwType, serializationProvider *serializer,
    eSerializationTypeMode mode
);
// Returns a language token that describes the request block ID.
const wchar_t* GetTokenNameForChunkID( Interface *engineInterface, uint32 chunkID );

};

#endif //_RENDERWARE_SERIALIZATION_PRIVATE_