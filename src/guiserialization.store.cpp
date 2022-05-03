/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/guiserialization.store.cpp
*  PURPOSE:     Storage for all editor serializers.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "guiserialization.hxx"

// Plugin to store serialization containers, so they can initialize before the serializer will load and save them.
struct serializationStoreEnv
{
    inline void Initialize( MainWindow *mainWnd )
    {
        return;
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        // We have to cleanly terminate. This means that all serializers have to unregister themselves.
        assert( this->serializers.size() == 0 );
    }

    inline magicSerializationProvider* FindSerializer( unsigned short id )
    {
        for ( const serializer_entry& ser : this->serializers )
        {
            if ( ser.id == id )
            {
                return ser.prov;
            }
        }

        return nullptr;
    }

    struct serializer_entry
    {
        unsigned short id;
        magicSerializationProvider *prov;

        inline bool operator == ( unsigned short id ) const
        {
            return ( this->id == id );
        }
    };

    std::vector <serializer_entry> serializers;
};

static optional_struct_space <PluginDependantStructRegister <serializationStoreEnv, mainWindowFactory_t>> serializationStoreRegister;

magicSerializationProvider* FindMainWindowSerializer( MainWindow *mainWnd, unsigned short unique_id )
{
    serializationStoreEnv *env = serializationStoreRegister.get().GetPluginStruct( mainWnd );

    if ( env )
    {
        return env->FindSerializer( unique_id );
    }

    return nullptr;
}

void ForAllMainWindowSerializers( const MainWindow *mainWnd, std::function <void ( magicSerializationProvider *prov, unsigned short id )> cb )
{
    const serializationStoreEnv *env = serializationStoreRegister.get().GetConstPluginStruct( mainWnd );

    if ( env )
    {
        for ( const serializationStoreEnv::serializer_entry& ser : env->serializers )
        {
            cb( ser.prov, ser.id );
        }
    }
}

rw::uint32 GetAmountOfMainWindowSerializers( const MainWindow *mainWnd )
{
    const serializationStoreEnv *env = serializationStoreRegister.get().GetConstPluginStruct( mainWnd );

    if ( env )
    {
        return (rw::uint32)env->serializers.size();
    }

    return 0;
}

bool RegisterMainWindowSerialization( MainWindow *mainWnd, unsigned short unique_id, magicSerializationProvider *prov )
{
    bool success = false;

    serializationStoreEnv *env = serializationStoreRegister.get().GetPluginStruct( mainWnd );

    if ( env )
    {
        // Register if the id is truly unique.
        magicSerializationProvider *alreadyExists = env->FindSerializer( unique_id );

        if ( alreadyExists == nullptr )
        {
            // We can safely register.
            serializationStoreEnv::serializer_entry entry;
            entry.id = unique_id;
            entry.prov = prov;

            env->serializers.push_back( std::move( entry ) );

            success = true;
        }
    }
    
    return success;
}

bool UnregisterMainWindowSerialization( MainWindow *mainWnd, unsigned short unique_id )
{
    bool success = false;

    serializationStoreEnv *env = serializationStoreRegister.get().GetPluginStruct( mainWnd );

    if ( env )
    {
        // Try to find the entry and delete the one that matches.
        auto found_iter = std::find( env->serializers.begin(), env->serializers.end(), unique_id );

        if ( found_iter != env->serializers.end() )
        {
            // We can safely remove this.
            env->serializers.erase( found_iter );

            success = true;
        }
    }

    return success;
}

void InitializeSerializationStorageEnv( void )
{
    serializationStoreRegister.Construct( mainWindowFactory );
}

void ShutdownSerializationStorageEnv( void )
{
    serializationStoreRegister.Destroy();
}