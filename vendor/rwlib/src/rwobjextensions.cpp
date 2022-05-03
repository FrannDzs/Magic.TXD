/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwobjextensions.cpp
*  PURPOSE:     Object extensions storage and handling.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "pluginutil.hxx"

namespace rw
{

struct rwObjExtensionStore
{
    EngineInterface *engineInterface;
    RwObject *rwObj;

    inline void Initialize( GenericRTTI *rtObj )
    {
        RwObject *theObject = (RwObject*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

        // Store for the exception system.
        this->rwObj = rwObj;

        EngineInterface *engineInterface = (EngineInterface*)theObject->engineInterface;

        // We store a list of raw extension blocks.

        this->engineInterface = engineInterface;    // remember for clone operations.
    }

    inline void Shutdown( GenericRTTI *rtObj )
    {
        RwObject *theObject = (RwObject*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

        Interface *engineInterface = theObject->engineInterface;

        // Deallocate all raw extension blocks.
        size_t numExtensions = this->serializeExtensions.GetCount();

        for ( size_t n = 0; n < numExtensions; n++ )
        {
            rwStoredExtension& extStored = this->serializeExtensions[ n ];

            extStored.Deallocate( engineInterface );
        }

        this->serializeExtensions.Clear();
    }

    inline rwObjExtensionStore& operator = ( const rwObjExtensionStore& right )
    {
        // Copy things over.
        size_t numExtensions = right.serializeExtensions.GetCount();

        for ( size_t n = 0; n < numExtensions; n++ )
        {
            const rwStoredExtension& ext = right.serializeExtensions[ n ];

            uint32 ext_size = ext.extensionLength;

            rwStoredExtension new_ext;
            new_ext.blockID = ext.blockID;
            new_ext.extensionVersion = ext.extensionVersion;
            new_ext.extensionLength = ext_size;

            // Clone the memory.
            new_ext.extMem = engineInterface->MemAllocate( ext_size, 1 );

            memcpy( new_ext.extMem, ext.extMem, ext_size );

            try
            {
                // Save the extension.
                this->serializeExtensions.AddToBack( std::move( new_ext ) );
            }
            catch( ... )
            {
                new_ext.Deallocate( engineInterface );
                throw;
            }
        }

        return *this;
    }

    inline void ParseExtension( Interface *engineInterface, BlockProvider& inputProvider )
    {
        // We store the block into our memory.
        rwStoredExtension storedExt;

        storedExt.extensionVersion = inputProvider.getBlockVersion();
        storedExt.blockID = inputProvider.getBlockID();

        int64 blockLength = inputProvider.getBlockLength();

        if ( blockLength > 0xFFFFFFFF )
        {
            throw RwObjectsStructuralErrorException( AcquireObject( this->rwObj ), L"OBJEXT_STRUCTERR_EXTTOOBIG" );
        }

        uint32 memSize = (uint32)blockLength;

        void *memStuff = engineInterface->MemAllocate( memSize, 1 );

        try
        {
            inputProvider.read( memStuff, memSize );
        }
        catch( ... )
        {
            engineInterface->MemFree( memStuff );

            throw;
        }

        storedExt.extMem = memStuff;
        storedExt.extensionLength = memSize;

        try
        {
            // Store it.
            this->serializeExtensions.AddToBack( std::move( storedExt ) );
        }
        catch( ... )
        {
            storedExt.Deallocate( engineInterface );
            throw;
        }
    }
    
    inline void WriteExtensions( Interface *engineInterface, BlockProvider& outputProvider ) const
    {
        size_t numExtensions = this->serializeExtensions.GetCount();

        for ( size_t n = 0; n < numExtensions; n++ )
        {
            const rwStoredExtension& storedExt = this->serializeExtensions[ n ];

            // Write a new block.
            {
                BlockProvider extBlock( &outputProvider, storedExt.blockID );

                extBlock.setBlockVersion( storedExt.extensionVersion );

                // Write stuff.
                extBlock.write( storedExt.extMem, storedExt.extensionLength );
            }
        }
    }

    // We need a store for raw extension blocks.
    struct rwStoredExtension
    {
        inline rwStoredExtension( void )
        {
            this->blockID = CHUNK_STRUCT;
            this->extMem = nullptr;
            this->extensionLength = 0;
        }

        LibraryVersion extensionVersion;
        uint32 blockID;
        uint32 extensionLength;
        void *extMem;

        inline void Deallocate( Interface *engineInterface )
        {
            if ( void *mem = this->extMem )
            {
                engineInterface->MemFree( mem );

                this->extMem = nullptr;
                this->extensionLength = 0;
            }
        }
    };

    DEFINE_HEAP_REDIR_ALLOC_IMPL( extRedirAlloc );

    eir::Vector <rwStoredExtension, extRedirAlloc> serializeExtensions;
};

RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( rwObjExtensionStore, extRedirAlloc, serializeExtensions, engineInterface )

struct rwInterfaceExtensionPlugin
{
    RwTypeSystem::pluginOffset_t rwobjExtensionStorePluginOffset;

    inline rwInterfaceExtensionPlugin( void )
    {
        this->rwobjExtensionStorePluginOffset =
            RwTypeSystem::INVALID_PLUGIN_OFFSET;
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        RwTypeSystem::typeInfoBase *rwobjTypeInfo = engineInterface->rwobjTypeInfo;

        if ( !rwobjTypeInfo )
            return;

        // Register the extension keeper plugin.
        this->rwobjExtensionStorePluginOffset =
            engineInterface->typeSystem.RegisterDependantStructPlugin <rwObjExtensionStore> ( rwobjTypeInfo, RwTypeSystem::ANONYMOUS_PLUGIN_ID );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Unregister our plugin again.
        RwTypeSystem::pluginOffset_t pluginOff = this->rwobjExtensionStorePluginOffset;

        if ( RwTypeSystem::IsOffsetValid( pluginOff ) )
        {
            engineInterface->typeSystem.UnregisterPlugin( engineInterface->rwobjTypeInfo, pluginOff );
        }
    }

    inline rwObjExtensionStore* GetObjectExtensionStore( EngineInterface *engineInterface, RwObject *rwobj )
    {
        rwObjExtensionStore *theStore = nullptr;

        RwTypeSystem::pluginOffset_t pluginOff = this->rwobjExtensionStorePluginOffset;

        if ( RwTypeSystem::IsOffsetValid( pluginOff ) )
        {
            GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( rwobj );

            RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

            RwTypeSystem::typeInfoBase *rwobjTypeInfo = engineInterface->rwobjTypeInfo;

            if ( engineInterface->typeSystem.IsTypeInheritingFrom( rwobjTypeInfo, typeInfo ) )
            {
                theStore = RwTypeSystem::RESOLVE_STRUCT <rwObjExtensionStore> ( engineInterface, rtObj, rwobjTypeInfo, pluginOff );
            }
        }

        return theStore;
    }

    inline const rwObjExtensionStore* GetConstObjectExtensionStore( EngineInterface *engineInterface, const RwObject *rwobj )
    {
        const rwObjExtensionStore *theStore = nullptr;

        RwTypeSystem::pluginOffset_t pluginOff = this->rwobjExtensionStorePluginOffset;

        if ( RwTypeSystem::IsOffsetValid( pluginOff ) )
        {
            const GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromConstObject( rwobj );

            RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

            RwTypeSystem::typeInfoBase *rwobjTypeInfo = engineInterface->rwobjTypeInfo;

            if ( engineInterface->typeSystem.IsTypeInheritingFrom( rwobjTypeInfo, typeInfo ) )
            {
                theStore = RwTypeSystem::RESOLVE_STRUCT <rwObjExtensionStore> ( engineInterface, rtObj, rwobjTypeInfo, pluginOff );
            }
        }

        return theStore;
    }
};

static optional_struct_space <PluginDependantStructRegister <rwInterfaceExtensionPlugin, RwInterfaceFactory_t>> rwExtensionsRegister;

void Interface::SerializeExtensions( const RwObject *rwObj, BlockProvider& outputProvider )
{
    EngineInterface *engineInterface = (EngineInterface*)this;
    
    BlockProvider extensionBlock( &outputProvider, CHUNK_EXTENSION );

    // Put all the extension data to the stream.
    {
        rwInterfaceExtensionPlugin *rwintExtPlugin = rwExtensionsRegister.get().GetPluginStruct( engineInterface );

        if ( rwintExtPlugin )
        {
            const rwObjExtensionStore *extStore = rwintExtPlugin->GetConstObjectExtensionStore( engineInterface, rwObj );

            if ( extStore )
            {
                // Write all sections.
                extStore->WriteExtensions( this, extensionBlock );
            }
        }
    }
}

void Interface::DeserializeExtensions( RwObject *rwObj, BlockProvider& inputProvider )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    // Attempt to fetch the object's extension store.
    rwObjExtensionStore *extStore = nullptr;
    {
        rwInterfaceExtensionPlugin *rwintExtPlugin = rwExtensionsRegister.get().GetPluginStruct( engineInterface );

        if ( rwintExtPlugin )
        {
            extStore = rwintExtPlugin->GetObjectExtensionStore( engineInterface, rwObj );
        }
    }

    // We can actually ignore reading extension data if it is not present.
    // Really.
    try
    {
        // Deserialize the blocks.
        BlockProvider extensionBlock( &inputProvider, CHUNK_EXTENSION, false );

        int64 end = extensionBlock.getBlockLength();
        end += extensionBlock.tell();
            
        while ( extensionBlock.tell() < end )
        {
            BlockProvider subExtensionBlock( &extensionBlock );

            // If we have the plugin, deserialize it.
            // Otherwise we just skip the block.
            if ( extStore != nullptr )
            {
                extStore->ParseExtension( this, subExtensionBlock );
            }
        }
    }
    catch( rw::RwException& )
    {
        // Decide whether we can ignore an extension read failure.
        // This is actually the first time where block-reading philosophy is taken.
        // If we are reading using block-lengths, we can safely ignore any error-case because the file-system is able
        // to recover. If we are not, then we allow ignoring if we are in the root-block space, because extension
        // blocks are usually at the end of everything.
        // This philosophy might need changing in the future.
        bool allowIgnore = false;

        if ( inputProvider.doesIgnoreBlockRegions() == false )
        {
            // Safe.
            allowIgnore = true;
        }
        else
        {
            // Check if we are in root block space.
            if ( inputProvider.hasParent() == false )
            {
                // We can ignore here too.
                // This is really risky, anyway.
                allowIgnore = true;
            }
        }

        if ( allowIgnore )
        {
            this->PushWarningToken( L"OBJEXT_WARN_READINGERROR" );

            // Just go ahead. Has to be safe to leave out extensions anyway.
        }
        else
        {
            // We must report this error.
            throw;
        }
    }
}

void registerObjectExtensionsPlugins( void )
{
    rwExtensionsRegister.Construct( engineFactory );
}

void unregisterObjectExtensionsPlugins( void )
{
    rwExtensionsRegister.Destroy();
}

};