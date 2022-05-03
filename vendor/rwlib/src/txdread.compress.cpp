/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.compress.cpp
*  PURPOSE:     Texture compression implementations
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.raster.hxx"

namespace rw
{

void Raster::optimizeForLowEnd(float quality)
{
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RasterNotInitializedException( AcquireRaster( this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    // To make good decisions, we need the storage capabilities of the texture.
    storageCapabilities storeCaps;

    texProvider->GetStorageCapabilities( storeCaps );

    // Textures that should run on low end hardware should not be too HD.
    // This routine takes the PlayStation 2 as reference hardware.

    static constexpr uint32 maxTexWidth = 256;
    static constexpr uint32 maxTexHeight = 256;

    while ( true )
    {
        // Get properties of this texture first.
        uint32 texWidth, texHeight;

        nativeTextureBatchedInfo nativeInfo;

        texProvider->GetTextureInfo( engineInterface, platformTex, nativeInfo );

        texWidth = nativeInfo.baseWidth;
        texHeight = nativeInfo.baseHeight;

        // Optimize this texture.
        bool hasTakenStep = false;

        if ( !hasTakenStep && quality < 1.0f )
        {
            // We first must make sure that the texture is not unnecessaringly huge.
            if ( texWidth > maxTexWidth || texHeight > maxTexHeight )
            {
                // Half the texture size until we are at a suitable size.
                uint32 targetWidth = texWidth;
                uint32 targetHeight = texHeight;

                do
                {
                    targetWidth /= 2;
                    targetHeight /= 2;
                }
                while ( targetWidth > maxTexWidth || targetHeight > maxTexHeight );

                // The texture dimensions are too wide.
                // We half the texture in size.
                this->resize( targetWidth, targetHeight );

                hasTakenStep = true;
            }
        }
        
        if ( !hasTakenStep )
        {
            // Can we store palette data?
            if ( storeCaps.pixelCaps.supportsPalette == true )
            {
                // We should decrease the texture size by palettization.
                ePaletteType currentPaletteType = texProvider->GetTexturePaletteType( platformTex );

                if (currentPaletteType == PALETTE_NONE)
                {
                    ePaletteType targetPalette = ( quality > 0.0f ) ? ( PALETTE_8BIT ) : ( PALETTE_4BIT );

                    // TODO: per mipmap alpha checking.

                    if (texProvider->DoesTextureHaveAlpha( platformTex ) == false)
                    {
                        // 4bit palette is only feasible for non-alpha textures (at higher quality settings).
                        // Otherwise counting the colors is too complicated.
                        
                        // TODO: still allow 8bit textures.
                        targetPalette = PALETTE_4BIT;
                    }

                    // The texture should be palettized for good measure.
                    this->convertToPalette( targetPalette );

                    // TODO: decide whether 4bit or 8bit palette.

                    hasTakenStep = true;
                }
            }
        }

        if ( !hasTakenStep )
        {
            // The texture dimension is important for renderer performance. That is why we need to scale the texture according
            // to quality settings aswell.

        }
    
        if ( !hasTakenStep )
        {
            break;
        }
    }
}

static AINLINE bool IsNativeTextureCompressed(
    Interface *engineInterface, texNativeTypeProvider *texProvider, PlatformTexture *platformTex,
    storageCapabilities *storageCapsOut = nullptr
)
{
    // The target raster may already be compressed or the target architecture does its own compression.
    // Decide about these situations.
    bool isAlreadyCompressed = texProvider->IsTextureCompressed( platformTex );

    if ( isAlreadyCompressed )
        return true; // do not recompress textures.

    storageCapabilities storeCaps;

    texProvider->GetStorageCapabilities( storeCaps );

    if ( storeCaps.isCompressedFormat )
        return true; // no point in compressing if the architecture will do it already.

    if ( storageCapsOut )
    {
        *storageCapsOut = storeCaps;
    }

    return false;
}

static AINLINE void CompressNativeTexture(
    Interface *engineInterface, texNativeTypeProvider *texProvider, PlatformTexture *platformTex,
    eCompressionType targetCompressionType
)
{
    // Since we now know about everything, we can take the pixels and perform the compression.
    pixelDataTraversal pixelData;

    texProvider->GetPixelDataFromTexture( engineInterface, platformTex, pixelData );

    bool hasDirectlyAcquired = false;

    try
    {
        // Unset the pixels from the texture and make them standalone.
        texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, pixelData.isNewlyAllocated == true );

        pixelData.SetStandalone();

        // Compress things.
        pixelFormat targetPixelFormat;
        targetPixelFormat.rasterFormat = pixelData.rasterFormat;
        targetPixelFormat.depth = pixelData.depth;
        targetPixelFormat.rowAlignment = 0;
        targetPixelFormat.colorOrder = pixelData.colorOrder;
        targetPixelFormat.paletteType = PALETTE_NONE;
        targetPixelFormat.compressionType = targetCompressionType;

        bool hasCompressed = ConvertPixelData( engineInterface, pixelData, targetPixelFormat );

        if ( !hasCompressed )
        {
            throw NativeTextureInternalErrorException( texProvider->managerData.rwTexType->name, L"NATIVETEX_INTERNALERR_COMPRFAIL" );
        }

        AdjustPixelDataDimensionsByFormat( engineInterface, texProvider, pixelData );

        // Put the new pixels into the texture.
        texNativeTypeProvider::acquireFeedback_t acquireFeedback;

        texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );

        hasDirectlyAcquired = acquireFeedback.hasDirectlyAcquired;
    }
    catch( ... )
    {
        pixelData.FreePixels( engineInterface );

        throw;
    }

    if ( hasDirectlyAcquired == false )
    {
        // Should never happen.
        pixelData.FreePixels( engineInterface );
    }
    else
    {
        pixelData.DetachPixels();
    }
}

void Raster::compress( float quality )
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

    // Make sure we are mutable.
    NativeCheckRasterMutable( this );

    // A pretty complicated algorithm that can be used to optimally compress rasters.
    // Currently this only supports DXT.

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RasterNotInitializedException( AcquireRaster( this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    // We should not continue if we are compressed already.
    storageCapabilities storeCaps;

    if ( IsNativeTextureCompressed( engineInterface, texProvider, platformTex, &storeCaps ) )
        return;

    // We want to check what kind of compression the target architecture supports.
    // If we have any compression support, we proceed to next stage.
    bool supportsDXT1 = false;
    bool supportsDXT2 = false;
    bool supportsDXT3 = false;
    bool supportsDXT4 = false;
    bool supportsDXT5 = false;

    pixelCapabilities inputTransferCaps;

    texProvider->GetPixelCapabilities( inputTransferCaps );

    // Now decide upon things.
    if ( inputTransferCaps.supportsDXT1 && storeCaps.pixelCaps.supportsDXT1 )
    {
        supportsDXT1 = true;
    }

    if ( inputTransferCaps.supportsDXT2 && storeCaps.pixelCaps.supportsDXT2 )
    {
        supportsDXT2 = true;
    }

    if ( inputTransferCaps.supportsDXT3 && storeCaps.pixelCaps.supportsDXT3 )
    {
        supportsDXT3 = true;
    }

    if ( inputTransferCaps.supportsDXT4 && storeCaps.pixelCaps.supportsDXT4 )
    {
        supportsDXT4 = true;
    }

    if ( inputTransferCaps.supportsDXT5 && storeCaps.pixelCaps.supportsDXT5 )
    {
        supportsDXT5 = true;
    }

    if ( supportsDXT1 == false &&
         supportsDXT2 == false &&
         supportsDXT3 == false &&
         supportsDXT4 == false &&
         supportsDXT5 == false )
    {
        
        throw RasterInvalidConfigurationException( AcquireRaster( this ), L"RASTER_INVALIDCFG_RASTERNOTCOMPR" );
    }

    // We need to know about the alpha status of the texture to make a good decision.
    bool texHasAlpha = texProvider->DoesTextureHaveAlpha( platformTex );

    // Decide now what compression we want.
    eCompressionType targetCompressionType = RWCOMPRESS_NONE;

    bool couldDecide =
        DecideBestDXTCompressionFormat(
            engineInterface,
            texHasAlpha,
            supportsDXT1, supportsDXT2, supportsDXT3, supportsDXT4, supportsDXT5,
            quality,
            targetCompressionType
        );

    if ( !couldDecide )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_INTERNERR_UNKBESTDXT" );
    }

    CompressNativeTexture( engineInterface, texProvider, platformTex, targetCompressionType );
}

void Raster::compressCustom(eCompressionType targetCompressionType)
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

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    // Avoid recompression.
    if ( IsNativeTextureCompressed( engineInterface, texProvider, platformTex ) )
        return;

    CompressNativeTexture( engineInterface, texProvider, platformTex, targetCompressionType );
}

bool Raster::isCompressed( void ) const
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

    return IsNativeTextureCompressed( engineInterface, texProvider, platformTex );
}

eCompressionType Raster::getCompressionFormat( void ) const
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

    return texProvider->GetTextureCompressionFormat( platformTex );
}

};