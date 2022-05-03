/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.size.cpp
*  PURPOSE:     Resizing implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.size.hxx"

#include "txdread.raster.hxx"

namespace rw
{

inline void GetNativeTextureBaseDimensions( Interface *engineInterface, PlatformTexture *nativeTexture, texNativeTypeProvider *texTypeProvider, uint32& baseWidth, uint32& baseHeight )
{
    nativeTextureBatchedInfo info;

    texTypeProvider->GetTextureInfo( engineInterface, nativeTexture, info );

    baseWidth = info.baseWidth;
    baseHeight = info.baseHeight;
}

optional_struct_space <resizeFilteringEnvRegister_t> resizeFilteringEnvRegister;

bool RegisterResizeFiltering( EngineInterface *engineInterface, const char *filterName, rasterResizeFilterInterface *intf )
{
    resizeFilteringEnv *filterEnv = resizeFilteringEnvRegister.get().GetPluginStruct( engineInterface );

    bool success = false;

    if ( filterEnv )
    {
        // Only register if a plugin by that name does not exist already.
        resizeFilteringEnv::filterPluginEntry *alreadyExists = filterEnv->FindPluginByName( filterName );

        if ( alreadyExists == nullptr )
        {
            // Add it.
            void *entryMem = engineInterface->MemAllocateP( sizeof( resizeFilteringEnv::filterPluginEntry ), alignof( resizeFilteringEnv::filterPluginEntry ) );

            if ( entryMem )
            {
                resizeFilteringEnv::filterPluginEntry *filter_entry = new (entryMem) resizeFilteringEnv::filterPluginEntry();

                filter_entry->filterName = filterName;
                filter_entry->intf = intf;

                LIST_INSERT( filterEnv->filters.root, filter_entry->node );

                success = true;
            }
        }
    }

    return success;
}

bool UnregisterResizeFiltering( EngineInterface *engineInterface, rasterResizeFilterInterface *intf )
{
    resizeFilteringEnv *filterEnv = resizeFilteringEnvRegister.get().GetPluginStruct( engineInterface );

    bool success = false;

    if ( filterEnv )
    {
        // If we find it, we remove it.
        resizeFilteringEnv::filterPluginEntry *foundPlugin = filterEnv->FindPluginByInterface( intf );

        if ( foundPlugin )
        {
            // Remove and delete it.
            LIST_REMOVE( foundPlugin->node );

            foundPlugin->~filterPluginEntry();

            // Now free the memory.
            engineInterface->MemFree( foundPlugin );

            success = true;
        }
    }

    return success;
}

void Raster::resize(uint32 newWidth, uint32 newHeight, const char *downsampleMode, const char *upscaleMode)
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

    // Make sure we are mutable.
    NativeCheckRasterMutable( this );

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RasterNotInitializedException( AcquireRaster( this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    // TODO: improve this check by doing it for every mipmap level!
    // I know this can be skipped, but better write idiot-proof code.
    // who knows, maybe we would be the idiots later on.

    // Determine whether we have to do upscaling or downsampling on either axis.
    uint32 baseWidth, baseHeight;

    GetNativeTextureBaseDimensions( engineInterface, platformTex, texProvider, baseWidth, baseHeight );

    eSamplingType horiSampling = determineSamplingType( baseWidth, newWidth );
    eSamplingType vertSampling = determineSamplingType( baseHeight, newHeight );

    // If there is nothing to do, we can quit here.
    if ( horiSampling == eSamplingType::SAME && vertSampling == eSamplingType::SAME )
        return;

    // Verify that the dimensions are even accepted by the native texture.
    // If they are, we can safely go on. Otherwise we have to tell the user to pick better dimensions!
    {
        nativeTextureSizeRules sizeRules;

        texProvider->GetTextureSizeRules( platformTex, sizeRules );

        if ( sizeRules.IsMipmapSizeValid( newWidth, newHeight ) == false )
        {
            throw ResizingRasterInvalidConfigurationException( AcquireRaster( this ), L"RESIZING_INVALIDCFG_NATTEX_MIPDIMMS" );
        }
    }

    // We need to get the filtering properties to use.
    rasterResizeFilterInterface *downsamplingFilter, *upscaleFilter;
    resizeFilteringCaps downsamplingCaps, upscaleCaps;

    FetchResizeFilteringFilters(
        engineInterface, downsampleMode, upscaleMode,
        downsamplingFilter, upscaleFilter,
        downsamplingCaps, upscaleCaps
    );

    // Get the pixel data of the original raster that we will operate on.
    pixelDataTraversal pixelData;

    texProvider->GetPixelDataFromTexture( engineInterface, platformTex, pixelData );

    // Since we will replace all mipmaps, free the data in the texture.
    texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, pixelData.isNewlyAllocated == true );

    pixelData.SetStandalone();

    // We will set the same pixel storage data to the texture.
    texNativeTypeProvider::acquireFeedback_t acquireFeedback;

    try
    {
        // Alright. We shall allocate a new target buffer for the filtering operation.
        // This buffer should have the same format as the original texels, kinda.
        eRasterFormat rasterFormat = pixelData.rasterFormat;
        uint32 depth = pixelData.depth;
        eColorOrdering colorOrder = pixelData.colorOrder;
        uint32 rowAlignment = pixelData.rowAlignment;

        // If the original texture was a palette or was compressed, we will unfortunately
        // have to recompress or remap the pixels, basically putting it into the original format.
        ePaletteType paletteType = pixelData.paletteType;
        void *paletteData = pixelData.paletteData;
        uint32 paletteSize = pixelData.paletteSize;
        eCompressionType compressionType = pixelData.compressionType;

        // If the raster data is not currently in raw raster mode, we have to use a temporary raster that is full color.
        // Otherwise we have no valid configuration.
        eRasterFormat tmpRasterFormat;
        eColorOrdering tmpColorOrder;
        uint32 tmpItemDepth;

        MapToCompatibleResizeRasterFormat(
            rasterFormat, depth, colorOrder, compressionType,
            tmpRasterFormat, tmpItemDepth, tmpColorOrder
        );

        // Since we will have a raw raster transformation buffer, we need to fetch the sample depth.
        uint32 sampleDepth = Bitmap::getRasterFormatDepth( tmpRasterFormat );

        mipmapLayerResizeColorPipeline dstColorPipe(
            tmpRasterFormat, sampleDepth, rowAlignment, tmpColorOrder,
            PALETTE_NONE, nullptr, 0
        );

        // Perform for every mipmap.
        size_t origMipmapCount = pixelData.mipmaps.GetCount();

        mipGenLevelGenerator mipGen( newWidth, newHeight );

        if ( !mipGen.isValidLevel() )
        {
            throw ResizingRasterInvalidConfigurationException( AcquireRaster( this ), L"RESIZING_INVALIDCFG_INVMIPDIMMS" );
        }

        size_t mipIter = 0;

        for ( ; mipIter < origMipmapCount; mipIter++ )
        {
            pixelDataTraversal::mipmapResource& dstMipLayer = pixelData.mipmaps[ mipIter ];

            uint32 origMipWidth = dstMipLayer.width;
            uint32 origMipHeight = dstMipLayer.height;

            uint32 origMipLayerWidth = dstMipLayer.layerWidth;
            uint32 origMipLayerHeight = dstMipLayer.layerHeight;

            void *origTexels = dstMipLayer.texels;
            uint32 origDataSize = dstMipLayer.dataSize;

            // Determine the size that this mipmap will have if resized.
            bool hasEstablishedLevel = true;

            if ( mipIter != 0 )
            {
                hasEstablishedLevel = mipGen.incrementLevel();
            }

            if ( !hasEstablishedLevel )
            {
                // We cannot generate any more mipmaps.
                // The remaining ones will be cleared, if available.
                break;
            }

            uint32 targetLayerWidth = mipGen.getLevelWidth();
            uint32 targetLayerHeight = mipGen.getLevelHeight();

            // Verify certain sampling properties.
            eSamplingType mipHoriSampling = determineSamplingType( origMipLayerWidth, targetLayerWidth );
            eSamplingType mipVertSampling = determineSamplingType( origMipLayerHeight, targetLayerHeight );

            assert( mipHoriSampling == eSamplingType::SAME || horiSampling == mipHoriSampling );
            assert( mipVertSampling == eSamplingType::SAME || vertSampling == mipVertSampling );

            // Check whether we have to do anything for this layer.
            if ( mipHoriSampling != eSamplingType::SAME || mipVertSampling != eSamplingType::SAME )
            {
                // We will have to read texels from this mip surface.
                // That is why we should make it raw.
                void *rawOrigTexels = nullptr;
                uint32 rawOrigDataSize = 0;

                uint32 rawOrigLayerWidth, rawOrigLayerHeight;

                bool rawOrigHasChanged =
                    ConvertMipmapLayerNative(
                        engineInterface, origMipWidth, origMipHeight, origMipLayerWidth, origMipLayerHeight, origTexels, origDataSize,
                        rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                        tmpRasterFormat, tmpItemDepth, rowAlignment, tmpColorOrder, paletteType, paletteData, paletteSize, RWCOMPRESS_NONE,
                        true,
                        rawOrigLayerWidth, rawOrigLayerHeight,
                        rawOrigTexels, rawOrigDataSize
                    );

                (void)rawOrigHasChanged;

                try
                {
                    // Perform the codec operations.
                    void *transMipData = nullptr;
                    uint32 transMipSize = 0;

                    PerformRawBitmapResizeFiltering(
                        engineInterface,
                        rawOrigLayerWidth, rawOrigLayerHeight, rawOrigTexels,
                        targetLayerWidth, targetLayerHeight,
                        tmpRasterFormat, tmpItemDepth, rowAlignment, tmpColorOrder, paletteType, paletteData, paletteSize,
                        sampleDepth,
                        dstColorPipe,
                        mipHoriSampling, mipVertSampling,
                        upscaleFilter, downsamplingFilter,
                        upscaleCaps, downsamplingCaps,
                        transMipData, transMipSize
                    );

                    // Encode the raw mipmap into a correct mipmap that the architecture expects.
                    void *encodedTexels = nullptr;
                    uint32 encodedDataSize = 0;

                    uint32 encodedWidth, encodedHeight;

                    try
                    {
                        bool hasChanged =
                            ConvertMipmapLayerNative(
                                engineInterface, targetLayerWidth, targetLayerHeight, targetLayerWidth, targetLayerHeight, transMipData, transMipSize,
                                tmpRasterFormat, sampleDepth, rowAlignment, tmpColorOrder, PALETTE_NONE, nullptr, 0, RWCOMPRESS_NONE,
                                rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                                true,
                                encodedWidth, encodedHeight,
                                encodedTexels, encodedDataSize
                            );

                        (void)hasChanged;
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( transMipData );
                        throw;
                    }

                    // If we have new texels now, we can actually free the old ones.
                    if ( encodedTexels != transMipData )
                    {
                        engineInterface->PixelFree( transMipData );
                    }

                    // Update the mipmap layer.
                    engineInterface->PixelFree( origTexels );

                    dstMipLayer.width = encodedWidth;
                    dstMipLayer.height = encodedHeight;
                    dstMipLayer.layerWidth = targetLayerWidth;
                    dstMipLayer.layerHeight = targetLayerHeight;
                    dstMipLayer.texels = encodedTexels;
                    dstMipLayer.dataSize = encodedDataSize;

                    // Success! On to the next.
                }
                catch( ... )
                {
                    if ( rawOrigTexels != origTexels )
                    {
                        engineInterface->PixelFree( rawOrigTexels );
                    }

                    throw;
                }

                // We do not need the temporary transformed layer anymore.
                if ( rawOrigTexels != origTexels )
                {
                    engineInterface->PixelFree( rawOrigTexels );
                }
            }
        }

        size_t actualMipmapCount = mipIter;

        if ( actualMipmapCount != origMipmapCount )
        {
            // Clear remaining mipmaps.
            for ( ; mipIter < origMipmapCount; mipIter++ )
            {
                pixelDataTraversal::FreeMipmap( engineInterface, pixelData.mipmaps[ mipIter ] );
            }

            pixelData.mipmaps.Resize( actualMipmapCount );
        }

        // Store the new mipmaps.
        texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );
    }
    catch( ... )
    {
        pixelData.FreePixels( engineInterface );

        throw;
    }

    // Free pixels if necessary.
    if ( acquireFeedback.hasDirectlyAcquired == false )
    {
        pixelData.FreePixels( engineInterface );
    }
    else
    {
        pixelData.DetachPixels();
    }
}

void Raster::getSize(uint32& width, uint32& height) const
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

    GetNativeTextureBaseDimensions( engineInterface, platformTex, texProvider, width, height );
}

// Filtering plugins.
extern void registerRasterSizeBlurPlugin( void );
extern void registerRasterResizeLinearPlugin( void );

extern void unregisterRasterSizeBlurPlugin( void );
extern void unregisterRasterResizeLinearPlugin( void );

void registerResizeFilteringEnvironment( void )
{
    resizeFilteringEnvRegister.Construct( engineFactory );

    // TODO: register all filtering plugins.
    registerRasterSizeBlurPlugin();
    registerRasterResizeLinearPlugin();
}

void unregisterResizeFilteringEnvironment( void )
{
    unregisterRasterResizeLinearPlugin();
    unregisterRasterSizeBlurPlugin();

    resizeFilteringEnvRegister.Destroy();
}

};
