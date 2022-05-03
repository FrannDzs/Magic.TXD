/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.raster.imaging.cpp
*  PURPOSE:     Imaging specific things about the Raster object.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.raster.hxx"

namespace rw
{

inline bool IsNativeTextureSupportedNativeImagingFormat( Interface *engineInterface, const void *nativeTex, const char *method )
{
    // Get the type name of this native texture.
    const GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromConstObject( nativeTex );

    RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

    return DoesNativeImageSupportNativeTextureFriendly( engineInterface, method, typeInfo->name );
}

bool Raster::supportsImageMethod( const char *method ) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    Interface *engineInterface = this->engineInterface;

    PlatformTexture *platformTex = this->platformData;

    if ( platformTex == nullptr )
    {
        throw RasterNotInitializedException( AcquireRaster( (Raster*)this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( (Raster*)this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    // First check native image support.
    if ( IsNativeTextureSupportedNativeImagingFormat( engineInterface, platformTex, method ) )
    {
        // The format is natively supported.
        return true;
    }

    // Other than that, we may have general support.
    if ( IsImagingFormatAvailable( engineInterface, method ) )
    {
        // Basic support is still support.
        return true;
    }

    // Nothing.
    return false;
}

void Raster::writeImage(Stream *outputStream, const char *method)
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    Interface *engineInterface = this->engineInterface;

    PlatformTexture *platformTex = this->platformData;

    if ( platformTex == nullptr )
    {
        throw RasterNotInitializedException( AcquireRaster( this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    bool hasSerialized = false;

    // First we try to serialize in a native image format.
    if ( IsNativeTextureSupportedNativeImagingFormat( engineInterface, platformTex, method ) )
    {
        // We want to serialize using this.

        if ( const char *natImgTypeName = GetNativeImageTypeNameFromFriendlyName( engineInterface, method ) )
        {
            NativeImage *natImg = CreateNativeImage( engineInterface, natImgTypeName );

            if ( natImg )
            {
                bool needsRef = false;

                try
                {
                    try
                    {
                        // Now that we have the native image handle, we push data into it,
                        // then we write using it and then we delete it.

                        // Add a const reference.
                        this->constRefCount++;

                        try
                        {
                            // We need the typeName of our native texture.
                            const char *nativeTypeName;
                            {
                                GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( platformTex );

                                RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

                                nativeTypeName = typeInfo->name;
                            }

                            NativeImageFetchFromRasterNoLock( natImg, this, nativeTypeName, needsRef );
                        }
                        catch( ... )
                        {
                            // We didnt get anything done, so clear the refence.
                            this->constRefCount--;

                            throw;
                        }

                        // Clean up the reference if not required.
                        if ( !needsRef )
                        {
                            this->constRefCount--;
                        }

                        natImg->writeToStream( outputStream );
                    }
                    catch( ... )
                    {
                        // Some sort of error happened.
                        DeleteNativeImage( natImg );

                        throw;
                    }

                    // Clean things up.
                    DeleteNativeImage( natImg );
                }
                catch( ... )
                {
                    // Remember to clean up the const reference!
                    if ( needsRef )
                    {
                        this->constRefCount--;
                    }

                    throw;
                }

                // If the raster still had a reference, clean it up.
                if ( needsRef )
                {
                    this->constRefCount--;
                }

                hasSerialized = true;
            }
            else
            {
                engineInterface->PushWarningToken( L"NATIVEIMG_WARN_CREATEFAIL_WRITEIMG" );
            }
        }
    }

    // Last resort.
    if ( !hasSerialized )
    {
        // Get the mipmap from layer 0.
        rawMipmapLayer rawLayer;

        bool gotLayer = texProvider->GetMipmapLayer( engineInterface, platformTex, 0, rawLayer );

        if ( !gotLayer )
        {
            throw RasterInvalidConfigurationException( AcquireRaster( this ), L"RASTER_INVALIDCFG_MIPLAYERZEROFAIL" );
        }

        try
        {
            // Push the mipmap to the imaging plugin.
            bool successfullyStored = SerializeMipmapLayer( outputStream, method, rawLayer );

            if ( successfullyStored == false )
            {
                throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_INTERNERR_MIPSERIALIZEFAIL" );
            }
        }
        catch( ... )
        {
            if ( rawLayer.isNewlyAllocated )
            {
                engineInterface->PixelFree( rawLayer.mipData.texels );

                rawLayer.mipData.texels = nullptr;
            }

            throw;
        }

        // Free raw bitmap resources.
        if ( rawLayer.isNewlyAllocated )
        {
            engineInterface->PixelFree( rawLayer.mipData.texels );

            rawLayer.mipData.texels = nullptr;
        }

        hasSerialized = true;
    }
}

void Raster::readImage( rw::Stream *inputStream )
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

    // Make sure we are mutable.
    NativeCheckRasterMutable( this );

    Interface *engineInterface = this->engineInterface;

    PlatformTexture *platformTex = this->platformData;

    if ( platformTex == nullptr )
    {
        throw RasterNotInitializedException( AcquireRaster( this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    bool hasDeserialized = false;

    // We could be a native image format.
    if ( const char *natImgTypeName = GetNativeImageTypeForStream( inputStream ) )
    {
        // Deserialize using that type.
        NativeImage *natImg = CreateNativeImage( engineInterface, natImgTypeName );

        if ( natImg )
        {
            try
            {
                // First read the data.
                // This will automatically clear the color data from this raster aswell.
                natImg->readFromStream( inputStream );

                // Then push the data to the Raster.
                NativeImagePutToRasterNoLock( natImg, this );
            }
            catch( ... )
            {
                DeleteNativeImage( natImg );

                throw;
            }

            // Clean up since we are finished.
            DeleteNativeImage( natImg );

            // We successfully deserialized!
            hasDeserialized = true;
        }
        else
        {
            engineInterface->PushWarningToken( L"NATIVEIMG_WARN_CREATEFAIL_READIMG" );
        }
    }

    // Last resort.
    if ( !hasDeserialized )
    {
        // Attempt to get a mipmap layer from the stream.
        rawMipmapLayer rawImagingLayer;

        bool deserializeSuccess = DeserializeMipmapLayer( inputStream, rawImagingLayer );

        if ( !deserializeSuccess )
        {
            throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_INTERNERR_DESERFAIL_IMGFMT" );
        }

        try
        {
            // Delete image data that was previously at the texture.
            texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, true );
        }
        catch( ... )
        {
            // Free the raw mipmap layer.
            engineInterface->PixelFree( rawImagingLayer.mipData.texels );

            throw;
        }

        // Put the imaging layer into the pixel traversal struct.
        pixelDataTraversal pixelData;

        pixelData.mipmaps.Resize( 1 );

        pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ 0 ];

        mipLayer.texels = rawImagingLayer.mipData.texels;
        mipLayer.width = rawImagingLayer.mipData.width;
        mipLayer.height = rawImagingLayer.mipData.height;
        mipLayer.layerWidth = rawImagingLayer.mipData.layerWidth;
        mipLayer.layerHeight = rawImagingLayer.mipData.layerHeight;
        mipLayer.dataSize = rawImagingLayer.mipData.dataSize;

        pixelData.rasterFormat = rawImagingLayer.rasterFormat;
        pixelData.depth = rawImagingLayer.depth;
        pixelData.rowAlignment = rawImagingLayer.rowAlignment;
        pixelData.colorOrder = rawImagingLayer.colorOrder;
        pixelData.paletteType = rawImagingLayer.paletteType;
        pixelData.paletteData = rawImagingLayer.paletteData;
        pixelData.paletteSize = rawImagingLayer.paletteSize;
        pixelData.compressionType = rawImagingLayer.compressionType;

        pixelData.hasAlpha = calculateHasAlpha( engineInterface, pixelData );
        pixelData.autoMipmaps = false;
        pixelData.cubeTexture = false;
        pixelData.rasterType = 4;   // bitmap raster.

        pixelData.isNewlyAllocated = true;

        texNativeTypeProvider::acquireFeedback_t acquireFeedback;

        try
        {
            // Make sure the pixel data is compatible.
            CompatibilityTransformPixelData( engineInterface, pixelData, texProvider );

            AdjustPixelDataDimensionsByFormat( engineInterface, texProvider, pixelData );

            // Set this to the texture now.
            texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );
        }
        catch( ... )
        {
            // We just free our shit.
            pixelData.FreePixels( engineInterface );

            throw;
        }

        if ( acquireFeedback.hasDirectlyAcquired == false )
        {
            // We need to release our pixels.
            pixelData.FreePixels( engineInterface );
        }
        else
        {
            pixelData.DetachPixels();
        }

        hasDeserialized = true;
    }
}

};