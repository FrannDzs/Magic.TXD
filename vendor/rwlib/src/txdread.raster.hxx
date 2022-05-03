/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.raster.hxx
*  PURPOSE:     RenderWare Raster object private include file.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

// We want to spread out this whole native texture management gunk.

#ifndef _RENDERWARE_RASTER_SHARED_INCLUDE_
#define _RENDERWARE_RASTER_SHARED_INCLUDE_

#include "pluginutil.hxx"

#include "txdread.nativetex.hxx"

#include "rwserialize.hxx"

#include "txdread.natcompat.hxx"

namespace rw
{

// Raster environment stuff.
struct rwMainRasterEnv_t
{
    typedef StaticPluginClassFactory <Raster, RwDynMemAllocator, rwEirExceptionManager> rasterFactory_t;

private:
    // Struct to register the raster factory as a dynamic type.
    // It allows optimized plugin registration directly by the raster object.
    struct raster_fact_pipeline : public rwFactRegPipes::rw_fact_pipeline_base
    {
        typedef rwFactRegPipes::rw_defconstr_fact_pipeline_base <Raster> base_constructor;

        AINLINE static rasterFactory_t* getFactory( EngineInterface *intf )
        {
            rwMainRasterEnv_t *rasterEnv = rwMainRasterEnv_t::pluginRegister.get().GetPluginStruct( intf );

            if ( rasterEnv )
            {
                return &rasterEnv->rasterFactory;
            }

            return nullptr;
        }

        AINLINE static const char* getTypeName( void )
        {
            return "raster";
        }
    };

public:
    TypeSystemFactoryTypeRegistration <RwTypeSystem, rasterFactory_t, raster_fact_pipeline> handler;

    inline rwMainRasterEnv_t( EngineInterface *natEngine ) : rasterFactory( eir::constr_with_alloc::DEFAULT, natEngine )
    {
        return;
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        handler.Initialize( engineInterface );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // At this point we really want that all rasters are terminated from the runtime.
        // If not we face severe consequences.
        assert( this->rasterFactory.GetNumberOfAliveClasses() == 0 );

        handler.Shutdown( engineInterface );
    }

    inline void operator =( const rwMainRasterEnv_t& right )
    {
        // Nothing to do here.
        return;
    }

    rasterFactory_t rasterFactory;

    // Register this raster environment into the main RW interface.
    typedef PluginDependantStructRegister <rwMainRasterEnv_t, RwInterfaceFactory_t> pluginRegister_t;

    static optional_struct_space <pluginRegister_t> pluginRegister;
};

// Native texture API.

static inline PlatformTexture* CreateNativeTexture( Interface *intf, RwTypeSystem::typeInfoBase *nativeTexType )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    PlatformTexture *texOut = nullptr;
    {
        GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, nativeTexType, nullptr );

        if ( rtObj )
        {
            texOut = (PlatformTexture*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
        }
    }
    return texOut;
}

static inline PlatformTexture* CloneNativeTexture( Interface *intf, const PlatformTexture *srcNativeTex )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    PlatformTexture *texOut = nullptr;
    {
        const GenericRTTI *srcRtObj = engineInterface->typeSystem.GetTypeStructFromConstAbstractObject( srcNativeTex );

        if ( srcRtObj )
        {
            GenericRTTI *dstRtObj = engineInterface->typeSystem.Clone( engineInterface, srcRtObj );

            if ( dstRtObj )
            {
                texOut = (PlatformTexture*)RwTypeSystem::GetObjectFromTypeStruct( dstRtObj );
            }
        }
    }
    return texOut;
}

static inline void DeleteNativeTexture( Interface *intf, PlatformTexture *nativeTexture )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( nativeTexture );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

struct nativeTextureStreamPlugin : public serializationProvider
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        this->platformTexType = engineInterface->typeSystem.RegisterAbstractType <PlatformTexture> ( "native_texture" );

        // Initialize the list that will keep all native texture types.
        LIST_CLEAR( this->texNativeTypes.root );

        // Register us in the serialization manager.
        RegisterSerialization( engineInterface, CHUNK_TEXTURENATIVE, L"NATIVETEX_FRIENDLYNAME", engineInterface->textureTypeInfo, this, RWSERIALIZE_INHERIT );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Unregister us again.
        UnregisterSerialization( engineInterface, CHUNK_TEXTURENATIVE );

        // Unregister all type providers.
        LIST_FOREACH_BEGIN( texNativeTypeProvider, this->texNativeTypes.root, managerData.managerNode )
            // We just set them to false.
            item->managerData.isRegistered = false;
        LIST_FOREACH_END

        LIST_CLEAR( this->texNativeTypes.root );

        if ( RwTypeSystem::typeInfoBase *platformTexType = this->platformTexType )
        {
            engineInterface->typeSystem.DeleteType( platformTexType );

            this->platformTexType = nullptr;
        }
    }

    inline nativeTextureStreamPlugin& operator = ( const nativeTextureStreamPlugin& right ) = delete;

    void Serialize( Interface *intf, BlockProvider& outputProvider, void *objectToStore ) const override
    {
        //EngineInterface *engineInterface = (EngineInterface*)intf;

        TextureBase *theTexture = (TextureBase*)objectToStore;

        // Fetch the raster, which is the virtual interface to the platform texel data.
        if ( Raster *texRaster = theTexture->GetRaster() )
        {
            // The raster also requires GPU native data, the heart of the texture.
            if ( PlatformTexture *nativeTex = texRaster->platformData )
            {
                // Get the type information of this texture and find its type provider.
                GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( nativeTex );

                RwTypeSystem::typeInfoBase *nativeTypeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

                // Attempt to cast the type interface to our native texture type provider.
                // If successful, it indeeed is a native texture.
                {
                    nativeTextureCustomTypeInterface *nativeTexTypeInterface = dynamic_cast <nativeTextureCustomTypeInterface*> ( nativeTypeInfo->tInterface );

                    if ( nativeTexTypeInterface )
                    {
                        texNativeTypeProvider *texNativeProvider = nativeTexTypeInterface->texTypeProvider;

                        // Set the version to the version of the native texture.
                        // Texture objects only have a 'virtual' version.
                        LibraryVersion nativeVersion = texNativeProvider->GetTextureVersion( nativeTex );

                        outputProvider.setBlockVersion( nativeVersion );

                        // Serialize the texture.
                        texNativeProvider->SerializeTexture( theTexture, nativeTex, outputProvider );
                    }
                    else
                    {
                        throw SerializationRasterInternalErrorException( AcquireRaster( texRaster ), L"RASTER_REASON_INNATIVEDATA" );
                    }
                }
            }
            else
            {
                throw SerializationRasterNotInitializedException( AcquireRaster( texRaster ), L"NATIVETEX_FRIENDLYNAME" );
            }
        }
        else
        {
            throw SerializationRwObjectsInternalErrorException( AcquireObject( theTexture ), L"SERIALIZATION_INTERNERR_TEXTURE_NORASTER" );
        }
    }

    struct interestedNativeType
    {
        eTexNativeCompatibility typeOfInterest;
        texNativeTypeProvider *interestedParty;
    };

    struct QueuedWarningHandler : public WarningHandler
    {
        void OnWarningMessage( rwStaticString <wchar_t>&& theMessage ) override
        {
            this->message_list.AddToBack( theMessage );
        }

        typedef rwStaticVector <rwStaticString <wchar_t>> messages_t;

        messages_t message_list;
    };

    void Deserialize( Interface *intf, BlockProvider& inputProvider, void *objectToDeserialize ) const override
    {
        EngineInterface *engineInterface = (EngineInterface*)intf;

        // This is a pretty complicated algorithm that will need revision later on, when networked streams are allowed.
        // It is required because tex native rules have been violated by War Drum Studios.
        // First, we need to analyze the given block; this is done by getting candidates from texNativeTypes that
        // have an interest in this block.
        typedef rwVector <interestedNativeType> interestList_t;

        interestList_t interestedTypeProviders( eir::constr_with_alloc::DEFAULT, intf );

        LIST_FOREACH_BEGIN( texNativeTypeProvider, this->texNativeTypes.root, managerData.managerNode )

            // Reset the input provider.
            inputProvider.seek( 0, RWSEEK_BEG );

            // Check this block's compatibility and if it is something, register it.
            eTexNativeCompatibility thisCompat = RWTEXCOMPAT_NONE;

            try
            {
                thisCompat = item->IsCompatibleTextureBlock( inputProvider );
            }
            catch( RwException& )
            {
                // If there was any exception, there is no point in selecting this provider
                // as a valid candidate.
                thisCompat = RWTEXCOMPAT_NONE;
            }

            if ( thisCompat != RWTEXCOMPAT_NONE )
            {
                interestedNativeType nativeInfo;
                nativeInfo.typeOfInterest = thisCompat;
                nativeInfo.interestedParty = item;

                interestedTypeProviders.AddToBack( nativeInfo );
            }

        LIST_FOREACH_END

        // Check whether the interest is valid.
        // There may only be one full-on interested party, but there can be multiple "maybe".
        struct providerInfo_t
        {
            inline providerInfo_t( void )
            {
                this->theProvider = nullptr;
            }

            texNativeTypeProvider *theProvider;

            QueuedWarningHandler _warningQueue;
        };

        typedef rwVector <providerInfo_t> providers_t;

        providers_t maybeProviders( eir::constr_with_alloc::DEFAULT, intf );

        texNativeTypeProvider *definiteProvider = nullptr;

        size_t interestedCount = interestedTypeProviders.GetCount();

        TextureBase *texOut = (TextureBase*)objectToDeserialize;

        for ( size_t n = 0; n < interestedCount; n++ )
        {
            const interestedNativeType& theInterest = interestedTypeProviders[ n ];

            eTexNativeCompatibility compatType = theInterest.typeOfInterest;

            if ( compatType == RWTEXCOMPAT_MAYBE )
            {
                providerInfo_t providerInfo;

                providerInfo.theProvider = theInterest.interestedParty;

                maybeProviders.AddToBack( providerInfo );
            }
            else if ( compatType == RWTEXCOMPAT_ABSOLUTE )
            {
                if ( definiteProvider == nullptr )
                {
                    definiteProvider = theInterest.interestedParty;
                }
                else
                {
                    throw SerializationRwObjectsInternalErrorException( AcquireObject( texOut ), L"SERIALIZATION_INTERNERR_TEXTURE_AMBIGUOUSPROV" );
                }
            }
        }

        // If we have no providers that recognized that texture block, we tell the runtime.
        if ( definiteProvider == nullptr && maybeProviders.GetCount() == 0 )
        {
            throw SerializationRwObjectsInternalErrorException( AcquireObject( texOut ), L"SERIALIZATION_INTERNERR_TEXTURE_NOPROV" );
        }

        // If we have only one maybe provider, we set it as definite provider.
        if ( definiteProvider == nullptr && maybeProviders.GetCount() == 1 )
        {
            definiteProvider = maybeProviders[0].theProvider;

            maybeProviders.Clear();
        }

        // If we have a definite provider, it is elected to parse the block.
        // Otherwise we try going through all "maybe" providers and select the first successful one.

        // We will need a raster which is an interface to the native GPU data.
        // It serves as a container, so serialization will not access it directly.
        Raster *texRaster = CreateRaster( engineInterface );

        if ( texRaster )
        {
            // We require to allocate a platform texture, so lets keep a pointer.
            PlatformTexture *platformData = nullptr;

            try
            {
                if ( definiteProvider != nullptr )
                {
                    // If we have a definite provider, we do not need special warning dispatching.
                    // Good for us.

                    // Create a native texture for this provider.
                    platformData = CreateNativeTexture( engineInterface, definiteProvider->managerData.rwTexType );

                    if ( platformData )
                    {
                        // Set the version of the native texture.
                        definiteProvider->SetTextureVersion( engineInterface, platformData, inputProvider.getBlockVersion() );

                        // Attempt to deserialize the native texture.
                        inputProvider.seek( 0, RWSEEK_BEG );

                        definiteProvider->DeserializeTexture( texOut, platformData, inputProvider );
                    }
                }
                else
                {
                    // Loop through all maybe providers.
                    bool hasSuccessfullyDeserialized = false;

                    providerInfo_t *successfulProvider = nullptr;

                    size_t maybeCount = maybeProviders.GetCount();

                    for ( size_t n = 0; n < maybeCount; n++ )
                    {
                        providerInfo_t& thisInfo = maybeProviders[ n ];

                        texNativeTypeProvider *theProvider = thisInfo.theProvider;

                        // Just attempt deserialization.
                        bool success = false;

                        // For any warning that has been performed during this process, we need to queue it.
                        // In case we succeeded serializing an object, we just output the warning of its runtime.
                        // On failure we warn the runtime that the deserialization was ambiguous.
                        // Then we output warnings in sections, after the native type names.
                        WarningHandler *currentWarningHandler = &thisInfo._warningQueue;

                        GlobalPushWarningHandler( engineInterface, currentWarningHandler );

                        PlatformTexture *nativeData = nullptr;

                        try
                        {
                            // We create a native texture for this provider.
                            nativeData = CreateNativeTexture( engineInterface, theProvider->managerData.rwTexType );

                            if ( nativeData )
                            {
                                try
                                {
                                    // Set the version of the native data.
                                    theProvider->SetTextureVersion( engineInterface, nativeData, inputProvider.getBlockVersion() );

                                    inputProvider.seek( 0, RWSEEK_BEG );

                                    try
                                    {
                                        theProvider->DeserializeTexture( texOut, nativeData, inputProvider );

                                        success = true;
                                    }
                                    catch( RwException& theError )
                                    {
                                        // We failed, try another deserialization.
                                        success = false;

                                        DeleteNativeTexture( engineInterface, nativeData );

                                        // We push this error as warning.
                                        auto wideError = DescribeException( intf, theError );

                                        if ( wideError.GetLength() != 0 )
                                        {
                                            engineInterface->PushWarningDynamic( std::move( wideError ) );
                                        }
                                    }
                                }
                                catch( ... )
                                {
                                    // This catch is required if we encounter any other exception that wrecks the runtime
                                    // We do not want any memory leaks.
                                    DeleteNativeTexture( engineInterface, nativeData );

                                    nativeData = nullptr;

                                    throw;
                                }
                            }
                            else
                            {
                                engineInterface->PushWarningToken( L"RASTER_WARN_NATIVETEXCREATEFAIL" );
                            }
                        }
                        catch( ... )
                        {
                            // We kinda failed somewhere, so lets unregister our warning handler.
                            GlobalPopWarningHandler( engineInterface );

                            throw;
                        }

                        GlobalPopWarningHandler( engineInterface );

                        if ( success )
                        {
                            successfulProvider = &thisInfo;

                            hasSuccessfullyDeserialized = true;

                            // Give the native data to the runtime.
                            platformData = nativeData;
                            break;
                        }
                    }

                    if ( !hasSuccessfullyDeserialized )
                    {
                        // We need to inform the user of the warnings that he might have missed.
                        size_t maybeCount = maybeProviders.GetCount();

                        if ( maybeCount > 1 )
                        {
                            engineInterface->PushWarningToken( L"RASTER_WARN_AMBIGUOUSNATTEX" );
                        }

                        // Output all warnings in sections.
                        for ( size_t n = 0; n < maybeCount; n++ )
                        {
                            const providerInfo_t& thisInfo = maybeProviders[ n ];

                            const QueuedWarningHandler& warningQueue = thisInfo._warningQueue;

                            texNativeTypeProvider *typeProvider = thisInfo.theProvider;

                            // Create a buffered warning output.
                            rwStaticString <wchar_t> typeWarnBuf;
                            typeWarnBuf += L"[";
                            CharacterUtil::TranscodeStringToZero( typeProvider->managerData.rwTexType->name, typeWarnBuf );
                            typeWarnBuf += L"]:";

                            if ( warningQueue.message_list.GetCount() == 0 )
                            {
                                typeWarnBuf += L"no warnings.";
                            }
                            else
                            {
                                typeWarnBuf += L"\n";

                                bool isFirstItem = true;

                                warningQueue.message_list.Walk(
                                    [&]( size_t idx, auto msg )
                                {
                                    if ( !isFirstItem )
                                    {
                                        typeWarnBuf += L"\n";
                                    }

                                    typeWarnBuf += msg;

                                    if ( isFirstItem )
                                    {
                                        isFirstItem = false;
                                    }
                                });
                            }

                            engineInterface->PushWarningDynamic( std::move( typeWarnBuf ) );
                        }

                        // On failure, just bail.
                    }
                    else
                    {
                        // Just output the warnings of the successful provider.
                        successfulProvider->_warningQueue.message_list.Walk(
                            [&]( size_t idx, auto& msg )
                        {
                            engineInterface->PushWarningDynamic( std::move( msg ) );
                        });
                    }
                }
            }
            catch( ... )
            {
                // If there was any exception, just pass it on.
                // We clear the texture beforehand.
                DeleteRaster( texRaster );

                texRaster = nullptr;

                if ( platformData )
                {
                    DeleteNativeTexture( engineInterface, platformData );

                    platformData = nullptr;
                }

                throw;
            }

            // Attempt to link all the data together.
            bool texLinkSuccess = false;

            if ( platformData )
            {
                // We link the raster with the texture and put the platform data into the raster.
                texRaster->platformData = platformData;

                texOut->SetRaster( texRaster );

                // We clear our reference from the raster.
                DeleteRaster( texRaster );

                // We succeeded!
                texLinkSuccess = true;
            }

            if ( texLinkSuccess == false )
            {
                // Delete platform data if we had one.
                if ( platformData )
                {
                    DeleteNativeTexture( engineInterface, platformData );

                    platformData = nullptr;
                }

                // Since we have no more texture object to store the raster into, we delete the raster.
                DeleteRaster( texRaster );

                throw SerializationRwObjectsInternalErrorException( AcquireObject( texOut ), L"SERIALIZATION_INTERNERR_TEXTURE_RASATTACHFAIL" );
            }
        }
        else
        {
            throw SerializationRwObjectsInternalErrorException( AcquireObject( texOut ), L"SERIALIZATION_INTERNERR_TEXTURE_RASCREATEFAIL" );
        }
    }

    struct nativeTextureCustomTypeInterface : public RwTypeSystem::typeInterface
    {
        AINLINE nativeTextureCustomTypeInterface( Interface *engineInterface, texNativeTypeProvider *texTypeProvider, size_t actualObjSize, size_t actualObjAlignment )
        {
            this->engineInterface = engineInterface;
            this->texTypeProvider = texTypeProvider;
            this->actualObjSize = actualObjSize;
            this->actualObjAlignment = actualObjAlignment;
        }

        void Construct( void *mem, EngineInterface *engineInterface, void *construct_params ) const override
        {
            this->texTypeProvider->ConstructTexture( this->engineInterface, mem, this->actualObjSize );
        }

        void CopyConstruct( void *mem, const void *srcMem ) const override
        {
            this->texTypeProvider->CopyConstructTexture( this->engineInterface, mem, srcMem, this->actualObjSize );
        }

        void Destruct( void *mem ) const override
        {
            this->texTypeProvider->DestroyTexture( this->engineInterface, mem, this->actualObjSize );
        }

        size_t GetTypeSize( EngineInterface *engineInterface, void *construct_params ) const override
        {
            return this->actualObjSize;
        }

        size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *objMem ) const override
        {
            return this->actualObjSize;
        }

        size_t GetTypeAlignment( EngineInterface *engineInterface, void *construct_params ) const override
        {
            return this->actualObjAlignment;
        }

        size_t GetTypeAlignmentByObject( EngineInterface *engineInterface, const void *objMem ) const override
        {
            return this->actualObjAlignment;
        }

        Interface *engineInterface;
        texNativeTypeProvider *texTypeProvider;
        size_t actualObjSize;
        size_t actualObjAlignment;
    };

    bool RegisterNativeTextureType( Interface *intf, const char *nativeName, texNativeTypeProvider *typeProvider, size_t memSize, size_t memAlignment )
    {
        EngineInterface *engineInterface = (EngineInterface*)intf;

        bool registerSuccess = false;

        if ( typeProvider->managerData.isRegistered == false )
        {
            RwTypeSystem::typeInfoBase *platformTexType = this->platformTexType;

            if ( platformTexType != nullptr )
            {
                RwTypeSystem::typeInfoBase *newType = nullptr;

                try
                {
                    newType = engineInterface->typeSystem.RegisterCommonTypeInterface <nativeTextureCustomTypeInterface> (
                        nativeName, platformTexType, engineInterface, typeProvider, memSize, memAlignment
                    );
                }
                catch( ... )
                {
                    registerSuccess = false;

                    // Fall through.
                }

                // Alright, register us.
                if ( newType )
                {
                    typeProvider->managerData.rwTexType = newType;

                    LIST_APPEND( this->texNativeTypes.root, typeProvider->managerData.managerNode );

                    typeProvider->managerData.isRegistered = true;

                    registerSuccess = true;
                }
            }
            else
            {
                engineInterface->PushWarningToken( L"NATIVETEX_WARN_REGWITHNOTYPE" );
            }
        }

        return registerSuccess;
    }

    bool UnregisterNativeTextureType( Interface *intf, const char *nativeName )
    {
        EngineInterface *engineInterface = (EngineInterface*)intf;

        bool unregisterSuccess = false;

        // Try removing the type with said name.
        {
            RwTypeSystem::typeInfoBase *nativeTypeInfo = engineInterface->typeSystem.FindTypeInfo( nativeName, this->platformTexType );

            if ( nativeTypeInfo && nativeTypeInfo->IsImmutable() == false )
            {
                // We can cast the type interface to get the type provider.
                nativeTextureCustomTypeInterface *nativeTypeInterface = dynamic_cast <nativeTextureCustomTypeInterface*> ( nativeTypeInfo->tInterface );

                if ( nativeTypeInterface )
                {
                    texNativeTypeProvider *texProvider = nativeTypeInterface->texTypeProvider;

                    // Remove it.
                    LIST_REMOVE( texProvider->managerData.managerNode );

                    texProvider->managerData.isRegistered = false;

                    // Delete the type.
                    engineInterface->typeSystem.DeleteType( nativeTypeInfo );
                }
            }
        }

        return unregisterSuccess;
    }

    RwTypeSystem::typeInfoBase *platformTexType;

    RwList <texNativeTypeProvider> texNativeTypes;
};

extern optional_struct_space <PluginDependantStructRegister <nativeTextureStreamPlugin, RwInterfaceFactory_t>> nativeTextureStreamStore;

inline RwTypeSystem::typeInfoBase* GetNativeTextureType( Interface *intf, const char *typeName )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    RwTypeSystem::typeInfoBase *typeInfo = nullptr;

    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.get().GetPluginStruct( engineInterface );

    if ( nativeTexEnv )
    {
        if ( RwTypeSystem::typeInfoBase *nativeTexType = nativeTexEnv->platformTexType )
        {
            typeInfo = engineInterface->typeSystem.FindTypeInfo( typeName, nativeTexType );
        }
    }

    return typeInfo;
}

// Immutability interface.
// Only call these functions under the raster consistency lock.
inline bool NativeIsRasterImmutable( const Raster *raster )
{
    return ( raster->constRefCount != 0 );
}

inline void NativeCheckRasterMutable( const Raster *raster )
{
    bool isImmutable = NativeIsRasterImmutable( raster );

    if ( isImmutable == true )
    {
        throw RasterInvalidConfigurationException( AcquireRaster( (Raster*)raster ), L"RASTER_INVALIDCFG_MODIFYCONST" );
    }
}

}

// Sub extensions.
#include "txdread.rasterplg.hxx"

#endif //_RENDERWARE_RASTER_SHARED_INCLUDE_
