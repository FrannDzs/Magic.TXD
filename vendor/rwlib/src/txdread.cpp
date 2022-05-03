/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.cpp
*  PURPOSE:     TexDictionary central implementations
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.natcompat.hxx"

#include "txdread.objutil.hxx"

#include "pluginutil.hxx"

#include "rwserialize.hxx"

namespace rw
{

/*
 * Texture Dictionary
 */

TexDictionary* texDictionaryStreamPlugin::CreateTexDictionary( EngineInterface *engineInterface ) const
{
    GenericRTTI *rttiObj = engineInterface->typeSystem.Construct( engineInterface, this->txdTypeInfo, nullptr );

    if ( rttiObj == nullptr )
    {
        return nullptr;
    }

    TexDictionary *txdObj = (TexDictionary*)RwTypeSystem::GetObjectFromTypeStruct( rttiObj );

    return txdObj;
}

TexDictionary* texDictionaryStreamPlugin::ToTexDictionary( EngineInterface *engineInterface, RwObject *rwObj )
{
    if ( isRwObjectInheritingFrom( engineInterface, rwObj, this->txdTypeInfo ) )
    {
        return (TexDictionary*)rwObj;
    }

    return nullptr;
}

const TexDictionary* texDictionaryStreamPlugin::ToConstTexDictionary( EngineInterface *engineInterface, const RwObject *rwObj )
{
    if ( isRwObjectInheritingFrom( engineInterface, rwObj, this->txdTypeInfo ) )
    {
        return (const TexDictionary*)rwObj;
    }

    return nullptr;
}

void texDictionaryStreamPlugin::Deserialize( Interface *intf, BlockProvider& inputProvider, void *objectToDeserialize ) const
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    // Cast our object.
    TexDictionary *txdObj = (TexDictionary*)objectToDeserialize;

    // Read the textures.
    {
        uint32 textureBlockCount = 0;
        bool requiresRecommendedPlatform = true;
        uint16 recDevicePlatID = 0;
        {
            BlockProvider texDictMetaStructBlock( &inputProvider, CHUNK_STRUCT );

            // Read the header block depending on version.
            LibraryVersion libVer = texDictMetaStructBlock.getBlockVersion();

            if ( !libVer.isNewerThan( LibraryVersion( 3, 5, 0, 0 ) ) )
            {
                textureBlockCount = texDictMetaStructBlock.readUInt32();
            }
            else
            {
                textureBlockCount = texDictMetaStructBlock.readUInt16();
                uint16 recommendedPlatform = texDictMetaStructBlock.readUInt16();

                // So if there is a recommended platform set, we will also give it one if we will write it.
                requiresRecommendedPlatform = ( recommendedPlatform != 0 );
                recDevicePlatID = recommendedPlatform;  // we want to store it aswell.
            }
        }

        txdObj->hasRecommendedPlatform = requiresRecommendedPlatform;
        txdObj->recDevicePlatID = recDevicePlatID;

        // Now follow multiple TEXTURENATIVE blocks.
        // Deserialize all of them.

        for ( uint32 n = 0; n < textureBlockCount; n++ )
        {
            BlockProvider textureNativeBlock( &inputProvider, CHUNK_TEXTURENATIVE );

            // Deserialize this block.
            RwObject *rwObj;

            try
            {
                rwObj = engineInterface->DeserializeBlock( textureNativeBlock );
            }
            catch( RwException& except )
            {
                // Catch the exception and try to continue.

                if ( textureNativeBlock.doesIgnoreBlockRegions() )
                {
                    // If we failed any texture parsing in the "ignoreBlockRegions" parse mode,
                    // there is no point in continuing, since the environment does not recover.
                    throw;
                }

                rwStaticString <wchar_t> errDebugMsg = DescribeException( intf, except );

                engineInterface->PushWarningObjectSingleTemplate( txdObj, L"TEXDICT_WARN_TEXNATIVEDECODE_TEMPLATE", L"errmsg", errDebugMsg.GetConstString() );

                continue;
            }

            // It has to be a texture that we read because we have acquired a texture native chunk
            // and it is made to deserialize into a texture only.
            TextureBase *texture = (TextureBase*)rwObj;

            texture->AddToDictionary( txdObj );
        }
    }

    // Read extensions.
    engineInterface->DeserializeExtensions( txdObj, inputProvider );
}

TexDictionary::TexDictionary( const TexDictionary& right ) : RwObject( right )
{
    scoped_rwlock_reader <> ctxCloneTXD( GetTXDLock( &right ) );

    // Create a new dictionary with all the textures.
    this->hasRecommendedPlatform = right.hasRecommendedPlatform;
    this->recDevicePlatID = right.recDevicePlatID;

    this->numTextures = 0;

    Interface *engineInterface = right.engineInterface;

    LIST_FOREACH_BEGIN( TextureBase, right.textures.root, texDictNode )

        TextureBase *texture = item;

        // Clone the texture and insert it into us.
        TextureBase *newTex = (TextureBase*)engineInterface->CloneRwObject( texture );

        if ( newTex )
        {
            // The_GTA: There was an interesting case of a bug once where I took a look that was not yet initialized inside of this clone constructor.
            //  It shows that locking is a really complicated task.
            newTex->_LinkDictionary( this );
        }

    LIST_FOREACH_END
}

TexDictionary::~TexDictionary( void )
{
    // Delete all textures that are part of this dictionary.
    while ( LIST_EMPTY( this->textures.root ) == false )
    {
        TextureBase *theTexture = LIST_GETITEM( TextureBase, this->textures.root.next, texDictNode );

        // Remove us from this TXD.
        // This is done because we cannot be sure that the texture is actually destroyed.
        theTexture->UnlinkDictionary();

        // Delete us.
        this->engineInterface->DeleteRwObject( theTexture );
    }
}

optional_struct_space <texDictionaryStreamPluginRegister_t> texDictionaryStreamStore;

void TexDictionary::clear(void)
{
    scoped_rwlock_writer <> ctxClearTXD( GetTXDLock( this ) );

	// We remove the links of all textures inside of us.
    while ( LIST_EMPTY( this->textures.root ) == false )
    {
        TextureBase *texture = LIST_GETITEM( TextureBase, this->textures.root.next, texDictNode );

        // Call the texture's own removal.
        texture->UnlinkDictionary();

        // TODO: should we also "kill" the texture? note that we could leak texture handles here!
        //  or we could make being-in-TXD hold a reference.
    }
}

rwlock* TexDictionary::GetLock( void ) const
{
    return GetTXDLock( this );
}

uint32 TexDictionary::GetTextureCount( void ) const
{
    scoped_rwlock_reader <> ctxGet( GetTXDLock( this ) );

    return this->numTextures;
}

const char* TexDictionary::GetRecommendedDriverPlatform( void ) const
{
    // This is already protected with a lock because of GetTexDictionaryRecommendedDriverID.

    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    texNativeTypeProvider *provider = nullptr;

    GetTexDictionaryRecommendedDriverID( engineInterface, this, &provider );

    if ( !provider )
        return nullptr;

    // HACK: reach into the type system name info directly.
    return provider->managerData.rwTexType->name;
}

TexDictionary* CreateTexDictionary( Interface *intf )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    TexDictionary *texDictOut = nullptr;

    texDictionaryStreamPlugin *txdStream = texDictionaryStreamStore.get().GetPluginStruct( engineInterface );

    if ( txdStream )
    {
        texDictOut = txdStream->CreateTexDictionary( engineInterface );
    }

    return texDictOut;
}

TexDictionary* ToTexDictionary( Interface *intf, RwObject *rwObj )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    texDictionaryStreamPlugin *txdStream = texDictionaryStreamStore.get().GetPluginStruct( engineInterface );

    if ( txdStream )
    {
        return txdStream->ToTexDictionary( engineInterface, rwObj );
    }

    return nullptr;
}

const TexDictionary* ToConstTexDictionary( Interface *intf, const RwObject *rwObj )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    texDictionaryStreamPlugin *txdStream = texDictionaryStreamStore.get().GetPluginStruct( engineInterface );

    if ( txdStream )
    {
        return txdStream->ToConstTexDictionary( engineInterface, rwObj );
    }

    return nullptr;
}

optional_struct_space <PluginDependantStructRegister <txdConsistencyLockEnv, RwInterfaceFactory_t>> txdConsistencyLockRegister;

// Main modules.
void registerTextureBasePlugins( void );
void unregisterTextureBasePlugins( void );

void registerTXDPlugins( void )
{
    // First register the main serialization plugins.
    // Those are responsible for detecting texture dictionaries and native textures in RW streams.
    // The sub module plugins depend on those.
    texDictionaryStreamStore.Construct( engineFactory );
    registerTextureBasePlugins();

    // Sub modules.
    txdConsistencyLockRegister.Construct( engineFactory );
}

void unregisterTXDPlugins( void )
{
    txdConsistencyLockRegister.Destroy();

    unregisterTextureBasePlugins();
    texDictionaryStreamStore.Destroy();
}

}
