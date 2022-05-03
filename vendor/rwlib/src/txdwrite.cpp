/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdwrite.cpp
*  PURPOSE:     TexDictionary writing implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.common.hxx"

#include "txdread.raster.hxx"

#include "txdread.objutil.hxx"

namespace rw
{

struct texNativeDriverResult
{
    uint16 driverID;
    texNativeTypeProvider *driverOut;
};

static inline void FindNativeTextureTypeByDriverID_cb( texNativeTypeProvider *prov, void *ud )
{
    texNativeDriverResult *result = (texNativeDriverResult*)ud;

    if ( prov->GetDriverIdentifier( nullptr ) == result->driverID )
    {
        result->driverOut = prov;
    }
}

static inline texNativeTypeProvider* FindNativeTextureTypeByDriverID( Interface *engineInterface, uint16 driverID )
{
    texNativeDriverResult meta;
    meta.driverID = driverID;
    meta.driverOut = nullptr;

    ExploreNativeTextureTypeProviders( engineInterface, FindNativeTextureTypeByDriverID_cb, &meta );

    return meta.driverOut;
}

uint16 GetTexDictionaryRecommendedDriverID( Interface *engineInterface, const TexDictionary *txdObj, texNativeTypeProvider **driverOut )
{
    // Determine the recommended platform to give this TXD.
    // If we dont have any, we can write 0.
    uint16 recommendedPlatform = 0;

    texNativeTypeProvider *assocDriver = nullptr;
    bool wantsDriver = ( driverOut != nullptr );

    scoped_rwlock_reader <> ctxGetDriverID( GetTXDLock( txdObj ) );

    if (txdObj->hasRecommendedPlatform)
    {
        if ( txdObj->numTextures == 0 )
        {
            // Return the platform ID that we store.
            recommendedPlatform = txdObj->recDevicePlatID;

            if ( recommendedPlatform != 0 )
            {
                // Fetch the driver that is assigned this ID.
                if ( wantsDriver )
                {
                    assocDriver = FindNativeTextureTypeByDriverID( engineInterface, recommendedPlatform );
                }
            }
        }
        else
        {
            // A recommended platform can only be given if all textures are of the same platform.
            // Otherwise it is misleading.
            bool hasTexPlatform = false;
            bool hasValidPlatform = true;
            uint32 curRecommendedPlatform;

            texNativeTypeProvider *platNativeProvider = nullptr;

            LIST_FOREACH_BEGIN( TextureBase, txdObj->textures.root, texDictNode )

                TextureBase *tex = item;

                Raster *texRaster = tex->GetRaster();

                if ( texRaster )
                {
                    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( texRaster ) );

                    // We can only determine the recommended platform if we have native data.
                    void *nativeObj = texRaster->platformData;

                    if ( nativeObj )
                    {
                        texNativeTypeProvider *typeProvider = GetNativeTextureTypeProvider( engineInterface, nativeObj );

                        if ( typeProvider )
                        {
                            // Call the providers method to get the recommended driver.
                            uint32 driverId = typeProvider->GetDriverIdentifier( nativeObj );

                            if ( driverId != 0 )
                            {
                                // We want to ensure that all textures have the same recommended platform.
                                // This will mean that a texture dictionary will be loadable for certain on one specialized architecture.
                                if ( !hasTexPlatform )
                                {
                                    curRecommendedPlatform = driverId;

                                    hasTexPlatform = true;

                                    platNativeProvider = typeProvider;
                                }
                                else
                                {
                                    if ( curRecommendedPlatform != driverId )
                                    {
                                        // We found a driver conflict.
                                        // This means that we cannot recommend for any special driver.
                                        hasValidPlatform = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

            LIST_FOREACH_END

            // Set it.
            bool canHaveDriver = false;

            if ( hasTexPlatform && hasValidPlatform )
            {
                // We are valid, so pass to runtime.
                recommendedPlatform = (uint16)curRecommendedPlatform;

                canHaveDriver = true;
            }

            if ( canHaveDriver && wantsDriver )
            {
                assocDriver = platNativeProvider;
            }
        }
    }

    if ( wantsDriver )
    {
        *driverOut = assocDriver;
    }

    return recommendedPlatform;
}

void texDictionaryStreamPlugin::Serialize( Interface *intf, BlockProvider& outputProvider, void *objectToSerialize ) const
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    const TexDictionary *txdObj = (const TexDictionary*)objectToSerialize;

    // Write the TXD contents.
    {
        scoped_rwlock_reader <> ctxSerializeTXD( GetTXDLock( txdObj ) );

        LibraryVersion version = outputProvider.getBlockVersion();

        uint32 numTextures = txdObj->numTextures;

        {
	        // Write the TXD meta info struct.
            BlockProvider txdMetaInfoBlock( &outputProvider );

            // By default, the block provider is using CHUNK_STRUCT as ID.

            // Write depending on version.
            if ( !version.isNewerThan( LibraryVersion( 3, 5, 0, 0 ) ) )
            {
                txdMetaInfoBlock.writeUInt32( numTextures );
            }
            else
            {
                if ( numTextures > 0xFFFF )
                {
                    throw SerializationRwObjectsInvalidConfigurationException( AcquireObject( (TexDictionary*)txdObj ), L"SERIALIZATION_INVALIDCFG_TEXDICT_TOOMANYTEX" );
                }

                uint16 recommendedPlatform = GetTexDictionaryRecommendedDriverID( engineInterface, txdObj );

	            txdMetaInfoBlock.writeUInt16(numTextures);
	            txdMetaInfoBlock.writeUInt16(recommendedPlatform);
            }
        }

        // Serialize all textures of this TXD.
        // This is done by appending the textures after the meta block.
        LIST_FOREACH_BEGIN( TextureBase, txdObj->textures.root, texDictNode )

            TextureBase *texture = item;

            // Put it into a sub block.
            BlockProvider texNativeBlock( blockprov_constr_no_ctx::DEFAULT, &outputProvider );

            engineInterface->SerializeBlock( texture, texNativeBlock );

        LIST_FOREACH_END
    }

    // Write extensions.
    engineInterface->SerializeExtensions( txdObj, outputProvider );
}

}
