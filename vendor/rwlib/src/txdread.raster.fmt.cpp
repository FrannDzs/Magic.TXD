/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.raster.fmt.cpp
*  PURPOSE:     Format specific things about the Raster object
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.raster.hxx"

namespace rw
{

inline bool GetNativeTextureRawBitmapData(
    Interface *engineInterface, PlatformTexture *nativeTexture, texNativeTypeProvider *texTypeProvider, uint32 mipIndex, bool supportsPalette, rawBitmapFetchResult& rawBitmapOut
)
{
    bool fetchSuccessful = false;

    // Fetch mipmap data result and convert it to raw texels.
    rawMipmapLayer mipData;

    bool gotMipLayer = texTypeProvider->GetMipmapLayer( engineInterface, nativeTexture, mipIndex, mipData );

    if ( gotMipLayer )
    {
        // Get the important data onto the stack.
        uint32 width = mipData.mipData.width;
        uint32 height = mipData.mipData.height;

        uint32 layerWidth = mipData.mipData.layerWidth;
        uint32 layerHeight = mipData.mipData.layerHeight;

        void *srcTexels = mipData.mipData.texels;
        uint32 dataSize = mipData.mipData.dataSize;

        eRasterFormat rasterFormat = mipData.rasterFormat;
        uint32 depth = mipData.depth;
        uint32 rowAlignment = mipData.rowAlignment;
        eColorOrdering colorOrder = mipData.colorOrder;
        ePaletteType paletteType = mipData.paletteType;
        void *paletteData = mipData.paletteData;
        uint32 paletteSize = mipData.paletteSize;

        eCompressionType compressionType = mipData.compressionType;

        bool isNewlyAllocated = mipData.isNewlyAllocated;

        try
        {
            // If we are not raw compressed yet, then we have to make us raw compressed.
            if ( compressionType != RWCOMPRESS_NONE )
            {
                // Compresion types do not support a valid source raster format.
                // We should determine one.
                eRasterFormat targetRasterFormat;
                uint32 targetDepth;
                uint32 targetRowAlignment = 4;  // good measure.

                if ( compressionType == RWCOMPRESS_DXT1 && mipData.hasAlpha == false )
                {
                    targetRasterFormat = RASTER_888;
                    targetDepth = 32;
                }
                else
                {
                    targetRasterFormat = RASTER_8888;
                    targetDepth = 32;
                }

                uint32 uncWidth, uncHeight;

                void *dstTexels = nullptr;
                uint32 dstDataSize = 0;

                bool doConvert =
                    ConvertMipmapLayerNative(
                        engineInterface,
                        width, height, layerWidth, layerHeight, srcTexels, dataSize,
                        rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                        targetRasterFormat, targetDepth, targetRowAlignment, colorOrder, paletteType, paletteData, paletteSize, RWCOMPRESS_NONE,
                        false,
                        uncWidth, uncHeight,
                        dstTexels, dstDataSize
                    );

                if ( doConvert == false )
                {
                    // We prefer C++ exceptions over assertions.
                    // This way, we actually handle mistakes that can always happen.
                    throw NativeTextureInternalErrorException( texTypeProvider->managerData.rwTexType->name, L"NATIVETEX_INTERNERR_MIPCONVFAIL" );
                }

                if ( isNewlyAllocated )
                {
                    engineInterface->PixelFree( srcTexels );
                }

                // Update stuff.
                width = uncWidth;
                height = uncHeight;

                srcTexels = dstTexels;
                dataSize = dstDataSize;

                rasterFormat = targetRasterFormat;
                depth = targetDepth;
                rowAlignment = targetRowAlignment;

                compressionType = RWCOMPRESS_NONE;

                isNewlyAllocated = true;
            }

            // Filter out palette if requested.
            if ( paletteType != PALETTE_NONE && supportsPalette == false )
            {
                uint32 palWidth, palHeight;

                void *dstPalTexels = nullptr;
                uint32 dstPalDataSize = 0;

                uint32 dstDepth = Bitmap::getRasterFormatDepth(rasterFormat);

                bool hasConverted =
                    ConvertMipmapLayerNative(
                        engineInterface,
                        width, height, layerWidth, layerHeight, srcTexels, dataSize,
                        rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                        rasterFormat, dstDepth, rowAlignment, colorOrder, PALETTE_NONE, nullptr, 0, compressionType,
                        false,
                        palWidth, palHeight,
                        dstPalTexels, dstPalDataSize
                    );

                (void)hasConverted;

                if ( isNewlyAllocated )
                {
                    engineInterface->PixelFree( srcTexels );
                    engineInterface->PixelFree( paletteData );
                }

                width = palWidth;
                height = palHeight;

                srcTexels = dstPalTexels;
                dataSize = dstPalDataSize;

                depth = dstDepth;

                paletteType = PALETTE_NONE;
                paletteData = nullptr;
                paletteSize = 0;

                isNewlyAllocated = true;
            }
        }
        catch( ... )
        {
            // If there was any error during conversion, we must clean up.
            if ( isNewlyAllocated )
            {
                engineInterface->PixelFree( srcTexels );

                if ( paletteData != nullptr )
                {
                    engineInterface->PixelFree( paletteData );
                }
            }

            throw;
        }

        // Put data into the output.
        rawBitmapOut.texelData = srcTexels;
        rawBitmapOut.dataSize = dataSize;

        rawBitmapOut.width = layerWidth;
        rawBitmapOut.height = layerHeight;

        rawBitmapOut.isNewlyAllocated = isNewlyAllocated;

        rawBitmapOut.depth = depth;
        rawBitmapOut.rowAlignment = rowAlignment;
        rawBitmapOut.rasterFormat = rasterFormat;
        rawBitmapOut.colorOrder = colorOrder;
        rawBitmapOut.paletteData = paletteData;
        rawBitmapOut.paletteSize = paletteSize;
        rawBitmapOut.paletteType = paletteType;

        // Success.
        fetchSuccessful = true;
    }

    return fetchSuccessful;
}

bool ConvertRasterTo( Raster *theRaster, const char *nativeName )
{
    bool conversionSuccess = false;

    EngineInterface *engineInterface = (EngineInterface*)theRaster->engineInterface;

    // First get the native texture environment.
    // This is used to browse for convertible types.
    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.get().GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        // Get the lock.
        scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( theRaster ) );

        // Make sure the raster is mutable.
        if ( NativeIsRasterImmutable( theRaster ) == false )
        {
            // Only convert if the raster has image data.
            if ( PlatformTexture *nativeTex = theRaster->platformData )
            {
                // Get the type information of the original platform data.
                GenericRTTI *origNativeRtObj = RwTypeSystem::GetTypeStructFromObject( nativeTex );

                RwTypeSystem::typeInfoBase *origTypeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( origNativeRtObj );

                // Get the type information of the destination format.
                RwTypeSystem::typeInfoBase *dstTypeInfo = GetNativeTextureType( engineInterface, nativeName );

                if ( origTypeInfo != nullptr && dstTypeInfo != nullptr )
                {
                    // If the destination type and the source type match, we are finished.
                    if ( engineInterface->typeSystem.IsSameType( origTypeInfo, dstTypeInfo ) )
                    {
                        conversionSuccess = true;
                    }
                    else
                    {
                        // Attempt to get the native texture type interface for both types.
                        nativeTextureStreamPlugin::nativeTextureCustomTypeInterface *origTypeInterface = dynamic_cast <nativeTextureStreamPlugin::nativeTextureCustomTypeInterface*> ( origTypeInfo->tInterface );
                        nativeTextureStreamPlugin::nativeTextureCustomTypeInterface *dstTypeInterface = dynamic_cast <nativeTextureStreamPlugin::nativeTextureCustomTypeInterface*> ( dstTypeInfo->tInterface );

                        // Only proceed if both could resolve.
                        if ( origTypeInterface != nullptr && dstTypeInterface != nullptr )
                        {
                            // Use the original type provider to grab pixel data from the texture.
                            // Then get the pixel capabilities of both formats and convert the pixel data into a compatible format for the destination format.
                            // Finally, apply the pixels to the destination format texture.
                            // * PERFORMANCE: at best, it can fetch pixel data (without allocation), free the original texture, allocate the new texture and put the pixels to it.
                            // * this would be a simple move operation. the actual operation depends on the complexity of both formats.
                            texNativeTypeProvider *origTypeProvider = origTypeInterface->texTypeProvider;
                            texNativeTypeProvider *dstTypeProvider = dstTypeInterface->texTypeProvider;

                            // In case of an exception, we have to deal with the pixel information, so we do not leak memory.
                            pixelDataTraversal pixelStore;

                            // 1. Fetch the pixel data.
                            origTypeProvider->GetPixelDataFromTexture( engineInterface, nativeTex, pixelStore );

                            try
                            {
                                // 2. detach the pixel data from the texture and free it.
                                //    free the pixels if we got a private copy.
                                origTypeProvider->UnsetPixelDataFromTexture( engineInterface, nativeTex, ( pixelStore.isNewlyAllocated == true ) );

                                // Since we are the only owners of pixelData now, inform it.
                                pixelStore.SetStandalone();

                                // 3. Allocate a new texture.
                                PlatformTexture *newNativeTex = CreateNativeTexture( engineInterface, dstTypeInfo );

                                if ( newNativeTex )
                                {
                                    try
                                    {
                                        // Transfer the version of the raster.
                                        {
                                            LibraryVersion srcVersion = origTypeProvider->GetTextureVersion( nativeTex );

                                            dstTypeProvider->SetTextureVersion( engineInterface, newNativeTex, srcVersion );
                                        }

                                        // 4. make pixels compatible for the target format.
                                        // *  First decide what pixel format we have to deduce from the capabilities
                                        //    and then call the "ConvertPixelData" function to do the job.
                                        CompatibilityTransformPixelData( engineInterface, pixelStore, dstTypeProvider );

                                        // The texels have to obey size rules of the destination native texture.
                                        // So let us check what size rules we need, right?
                                        AdjustPixelDataDimensionsByFormat( engineInterface, dstTypeProvider, pixelStore );

                                        // 5. Put the texels into our texture.
                                        //    Throwing an exception here means that the texture did not apply any of the pixel
                                        //    information. We can safely free pixelStore.
                                        texNativeTypeProvider::acquireFeedback_t acquireFeedback;

                                        dstTypeProvider->SetPixelDataToTexture( engineInterface, newNativeTex, pixelStore, acquireFeedback );

                                        if ( acquireFeedback.hasDirectlyAcquired == false )
                                        {
                                            // We need to release the pixels from the storage.
                                            pixelStore.FreePixels( engineInterface );
                                        }
                                        else
                                        {
                                            // Since the texture now owns the pixels, we just detach.
                                            pixelStore.DetachPixels();
                                        }

                                        // 6. Link the new native texture!
                                        //    Also delete the old one.
                                        DeleteNativeTexture( engineInterface, nativeTex );
                                    }
                                    catch( ... )
                                    {
                                        // If any exception happened, we must clean up things.
                                        DeleteNativeTexture( engineInterface, newNativeTex );

                                        throw;
                                    }

                                    theRaster->platformData = newNativeTex;

                                    // We are successful!
                                    conversionSuccess = true;
                                }
                                else
                                {
                                    conversionSuccess = false;
                                }
                            }
                            catch( ... )
                            {
                                // We do not pass on exceptions.
                                // Do not rely on this tho.
                                conversionSuccess = false;
                            }

                            if ( conversionSuccess == false )
                            {
                                // We failed at doing our task.
                                // Terminate any resource that allocated.
                                pixelStore.FreePixels( engineInterface );
                            }
                        }
                    }
                }
            }
        }
    }

    return conversionSuccess;
}

void Raster::convertToFormat(eRasterFormat newFormat)
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

    // Make sure we are mutable.
    NativeCheckRasterMutable( this );

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RasterNotInitializedException( AcquireRaster( this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    Interface *engineInterface = this->engineInterface;

    // Get the type provider of the native data.
    texNativeTypeProvider *typeProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !typeProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    // First see whether a conversion is even necessary.
    eRasterFormat texRasterFormat = typeProvider->GetTextureRasterFormat( platformTex );
    ePaletteType texPaletteType = typeProvider->GetTexturePaletteType( platformTex );
    eCompressionType texCompressionType = typeProvider->GetTextureCompressionFormat( platformTex );

    bool texIsPaletteRaster = ( texPaletteType != PALETTE_NONE );
    bool texIsCompressed = ( texCompressionType != RWCOMPRESS_NONE );

    if ( texIsCompressed || texIsPaletteRaster || newFormat != texRasterFormat )
    {
        // Fetch the entire pixel data from this texture and convert it.
        pixelDataTraversal pixelData;

        typeProvider->GetPixelDataFromTexture( engineInterface, platformTex, pixelData );

        bool werePixelsAllocated = pixelData.isNewlyAllocated;

        // We have now decided to replace the pixel data.
        // Delete any pixels that the texture had previously.
        typeProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, werePixelsAllocated == true );

        pixelData.SetStandalone();

        try
        {
            bool isCompressed = ( pixelData.compressionType != RWCOMPRESS_NONE );

            pixelFormat dstFormat;
            dstFormat.rasterFormat = newFormat;
            dstFormat.depth = Bitmap::getRasterFormatDepth( newFormat );
            dstFormat.rowAlignment = ( isCompressed ? 4 : pixelData.rowAlignment );
            dstFormat.colorOrder = pixelData.colorOrder;
            dstFormat.paletteType = PALETTE_NONE;
            dstFormat.compressionType = RWCOMPRESS_NONE;

            // Convert the pixels.
            bool hasUpdated = ConvertPixelData( engineInterface, pixelData, dstFormat );

            (void)hasUpdated;

            // Make sure dimensions are correct.
            AdjustPixelDataDimensionsByFormat( engineInterface, typeProvider, pixelData );

            // Put the texels back into the texture.
            texNativeTypeProvider::acquireFeedback_t acquireFeedback;

            typeProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );

            // Free pixel data if required.
            if ( acquireFeedback.hasDirectlyAcquired == false )
            {
                pixelData.FreePixels( engineInterface );
            }
            else
            {
                pixelData.DetachPixels();
            }
        }
        catch( ... )
        {
            pixelData.FreePixels( engineInterface );

            throw;
        }
    }
}

eRasterFormat Raster::getRasterFormat( void ) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RasterNotInitializedException( AcquireRaster( (Raster*)this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( (Raster*)this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    return texProvider->GetTextureRasterFormat( platformTex );
}

Bitmap Raster::getBitmap(void) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    Interface *engineInterface = this->engineInterface;

    Bitmap resultBitmap( engineInterface );
    {
        PlatformTexture *platformTex = this->platformData;

        if ( platformTex == nullptr )
        {
            throw RasterNotInitializedException( AcquireRaster( (Raster*)this ), L"NATIVETEX_FRIENDLYNAME" );
        }

        // If it has any mipmap at all.
        texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

        if ( texProvider == nullptr )
        {
            throw RasterInternalErrorException( AcquireRaster( (Raster*)this ), L"RASTER_REASON_INVNATIVEDATA" );
        }

        uint32 mipmapCount = GetNativeTextureMipmapCount( engineInterface, platformTex, texProvider );

        if ( mipmapCount > 0 )
        {
            uint32 width;
            uint32 height;
            uint32 depth;
            uint32 rowAlignment;
            eRasterFormat theFormat;
            eColorOrdering theOrder;

            // Get the color data.
            bool hasAllocatedNewPixelData = false;
            void *pixelData = nullptr;

            uint32 texDataSize;

            {
                rawBitmapFetchResult rawBitmap;

                bool gotPixelData = GetNativeTextureRawBitmapData( engineInterface, platformTex, texProvider, 0, false, rawBitmap );

                if ( gotPixelData )
                {
                    width = rawBitmap.width;
                    height = rawBitmap.height;
                    texDataSize = rawBitmap.dataSize;
                    depth = rawBitmap.depth;
                    rowAlignment = rawBitmap.rowAlignment;
                    theFormat = rawBitmap.rasterFormat;
                    theOrder = rawBitmap.colorOrder;
                    hasAllocatedNewPixelData = rawBitmap.isNewlyAllocated;
                    pixelData = rawBitmap.texelData;
                }
            }

            if ( pixelData != nullptr )
            {
                // Set the data into the bitmap.
                resultBitmap.setImageData(
                    pixelData, theFormat, theOrder, depth, rowAlignment, width, height, texDataSize,
                    ( hasAllocatedNewPixelData == true )
                );

                // We do not have to free the raw bitmap, because we assign it if it was stand-alone.
            }
        }
    }

    return resultBitmap;
}

void Raster::setImageData(const Bitmap& srcImage)
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

    // Make sure we are mutable.
    NativeCheckRasterMutable( this );

    PlatformTexture *platformTex = this->platformData;

    if ( platformTex == nullptr )
    {
        throw RasterNotInitializedException( AcquireRaster( this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    // Delete old image data.
    texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, true );

    if ( srcImage.getDataSize() == 0 )
    {
        // Empty shit.
        return;
    }

    // Create a copy of the bitmap pixel data and put it into a traversal struct.
    void *dstTexels = srcImage.copyPixelData();
    uint32 dstDataSize = srcImage.getDataSize();

    // Only valid if the block below has not thrown an exception.
    texNativeTypeProvider::acquireFeedback_t acquireFeedback;

    pixelDataTraversal pixelData;

    try
    {
        pixelDataTraversal::mipmapResource newLayer;

        // Set the new texel data.
        uint32 newWidth, newHeight;
        uint32 newDepth = srcImage.getDepth();
        uint32 newRowAlignment = srcImage.getRowAlignment();
        eRasterFormat newFormat = srcImage.getFormat();
        eColorOrdering newColorOrdering = srcImage.getColorOrder();

        srcImage.getSize( newWidth, newHeight );

        newLayer.width = newWidth;
        newLayer.height = newHeight;

        // Since it is raw image data, layer dimm is same as regular dimm.
        newLayer.layerWidth = newWidth;
        newLayer.layerHeight = newHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;

        pixelData.rasterFormat = newFormat;
        pixelData.depth = newDepth;
        pixelData.rowAlignment = newRowAlignment;
        pixelData.colorOrder = newColorOrdering;

        pixelData.paletteType = PALETTE_NONE;
        pixelData.paletteData = nullptr;
        pixelData.paletteSize = 0;

        pixelData.compressionType = RWCOMPRESS_NONE;

        pixelData.mipmaps.AddToBack( newLayer );

        pixelData.isNewlyAllocated = true;
        pixelData.hasAlpha = calculateHasAlpha( engineInterface, pixelData );
        pixelData.rasterType = 4;   // Bitmap.
        pixelData.autoMipmaps = false;
        pixelData.cubeTexture = false;

        // We do not have to transform for capabilities, since the input is a raw mipmap layer.
        // Raw texture data must be accepted by every native texture format.

        // Sure the dimensions are correct.
        AdjustPixelDataDimensionsByFormat( engineInterface, texProvider, pixelData );

        // Push the data to the texture.
        texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );
    }
    catch( ... )
    {
        // If the acquisition has failed, we have to bail.
        pixelData.FreePixels( engineInterface );

        throw;
    }

    if ( acquireFeedback.hasDirectlyAcquired == false )
    {
        pixelData.FreePixels( engineInterface );
    }
    else
    {
        pixelData.DetachPixels();
    }
}

};
