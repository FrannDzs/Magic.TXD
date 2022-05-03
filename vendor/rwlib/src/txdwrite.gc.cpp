/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdwrite.gc.cpp
*  PURPOSE:     Gamecube native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_GAMECUBE

#include "txdread.gc.hxx"

#include "pluginutil.hxx"

#include "streamutil.hxx"

#include "txdread.gc.miptrans.hxx"

namespace rw
{

void gamecubeNativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{
    // Let's write a gamecube native texture!
    Interface *engineInterface = theTexture->engineInterface;

    {
        BlockProvider gcNativeBlock( &outputProvider );

        const NativeTextureGC *platformTex = (const NativeTextureGC*)nativeTex;

        eGCNativeTextureFormat internalFormat = platformTex->internalFormat;
        eGCPixelFormat palettePixelFormat = platformTex->palettePixelFormat;

        size_t mipmapCount = platformTex->mipmaps.GetCount();

        if ( mipmapCount == 0 )
        {
            throw NativeTextureRwObjectsInternalErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_INTERNERR_WRITEEMPTY" );
        }

        const NativeTextureGC::mipmapLayer& baseLayer = platformTex->mipmaps[ 0 ];

        // Write the platform descriptor.
        {
            endian::big_endian <uint32> platformDescriptor( PLATFORMDESC_GAMECUBE );

            gcNativeBlock.writeStruct( platformDescriptor );
        }

        // We first have to write the header.
        // This is a versioned struct, so we have to be careful.
        {
            // Write the format info.
            texFormatInfo formatInfo;
            formatInfo.set( *theTexture );

            bool hasAlpha = platformTex->hasAlpha;

            uint32 depth = getGCInternalFormatDepth( internalFormat );

            // Set up the common raster properties.
            rwGenericRasterFormatFlags rasterFormatFlags;
            rasterFormatFlags.value = 0;

            rasterFormatFlags.data.hasMipmaps = ( mipmapCount > 1 );
            rasterFormatFlags.data.autoMipmaps = platformTex->autoMipmaps;

            // We can also easily set the palette property.
            if ( internalFormat == GVRFMT_PAL_4BIT )
            {
                rasterFormatFlags.data.palType = RWSER_PALETTE_PAL4;
            }
            else if ( internalFormat == GVRFMT_PAL_8BIT )
            {
                rasterFormatFlags.data.palType = RWSER_PALETTE_PAL8;
            }

            // We continue updating the rasterFormatFlags in the version dependant stuff!

            // Set version dependant stuff.
            LibraryVersion nativeStructVer = gcNativeBlock.getBlockVersion();

            if ( nativeStructVer.isNewerThan( LibraryVersion( 3, 3, 0, 2 ) ) )
            {
                // The newest Gamecube revision that is most stable.

                gamecube::textureMetaHeaderStructGeneric35 metaHeader;

                metaHeader.texFormat = formatInfo;

                // First write the unknowns.
                metaHeader.unk1 = platformTex->unk1;
                metaHeader.unk2 = platformTex->unk2;
                metaHeader.unk3 = platformTex->unk3;
                metaHeader.unk4 = platformTex->unk4;

                // Write texture names.
                // These need to be written securely.
                writeStringIntoBufferSafe( engineInterface, theTexture->GetName(), metaHeader.name, sizeof( metaHeader.name ), theTexture, L"name" );
                writeStringIntoBufferSafe( engineInterface, theTexture->GetMaskName(), metaHeader.maskName, sizeof( metaHeader.maskName ), theTexture, L"mask name" );

                // Write a good format number.
                // Difference to the old native texture format is that we do not absolutely require a mapping here.
                if ( internalFormat != GVRFMT_CMP )
                {
                    if ( palettePixelFormat != GVRPIX_NO_PALETTE )
                    {
                        eGCPaletteFormat gcPalFmt = getGCPaletteFormatFromPalettePixelFormat( palettePixelFormat );

                        rasterFormatFlags.data.formatNum = gcPalFmt;
                    }
                    else
                    {
                        eGCRasterFormat rasterFmt = getGCRasterFormatFromInternalFormat( internalFormat );

                        rasterFormatFlags.data.formatNum = rasterFmt;
                    }
                }

                // In the newer GC native texture version we store the rasterType
                // property in the flags.
                rasterFormatFlags.data.rasterType = platformTex->rasterType;

                metaHeader.rasterFormat = rasterFormatFlags;

                metaHeader.width = baseLayer.layerWidth;
                metaHeader.height = baseLayer.layerHeight;
                metaHeader.depth = depth;
                metaHeader.mipmapCount = (uint8)mipmapCount;
                metaHeader.internalFormat = internalFormat;
                metaHeader.palettePixelFormat = palettePixelFormat;

                metaHeader.hasAlpha = ( hasAlpha ? 1 : 0 );

                // Write it.
                gcNativeBlock.writeStruct( metaHeader );
            }
            else
            {
                // Remember that the GC native texture prior to 3.3.0.2 has no support for luminance
                // formats, so these have to be converted down if the version of the native texture is downgraded.

                // We are a very old format.
                // This means that we got to calculate our old internalFormat mapping.
                bool isCompressed = ( internalFormat == GVRFMT_CMP );

                if ( !isCompressed )
                {
                    // Set a cool format number that describes us.
                    bool didSetFormatNumber = false;

                    if ( palettePixelFormat != GVRPIX_NO_PALETTE )
                    {
                        eColorOrdering _colorOrder;
                        eGCNativeTextureFormat gcPalInternalFormat;

                        bool couldGetFormat = getGVRNativeFormatFromPaletteFormat( palettePixelFormat, _colorOrder, gcPalInternalFormat );

                        if ( couldGetFormat )
                        {
                            eGCRasterFormat rasterFmt = getGCRasterFormatFromInternalFormat( gcPalInternalFormat );

                            if ( rasterFmt != GCRASTER_DEFAULT )
                            {
                                rasterFormatFlags.data.formatNum = rasterFmt;

                                didSetFormatNumber = true;
                            }
                        }
                    }
                    else
                    {
                        eGCRasterFormat gcRasterFormat = getGCRasterFormatFromInternalFormat( internalFormat );

                        if ( gcRasterFormat != GCRASTER_DEFAULT )
                        {
                            rasterFormatFlags.data.formatNum = gcRasterFormat;

                            didSetFormatNumber = true;
                        }
                        // There is actually no point in adding support for the GCRASTER_888 format mapping,
                        // because the GC hardware does not support it anyway.
                    }

                    if ( !didSetFormatNumber )
                    {
                        throw NativeTextureRwObjectsInternalErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_INTERNERR_SER_NOFMTMAP_O" );
                    }
                }

                if ( nativeStructVer.isNewerThan( LibraryVersion( 3, 3, 0, 0 ) ) )
                {
                    // Once again less advanced version, but still harvestable.

                    gamecube::textureMetaHeaderStructGeneric33 metaHeader;

                    metaHeader.texFormat = formatInfo;

                    // New thing is GPU internal registers!
                    metaHeader.unk1 = platformTex->unk1;
                    metaHeader.unk2 = platformTex->unk2;
                    metaHeader.unk3 = platformTex->unk3;
                    metaHeader.unk4 = platformTex->unk4;

                    // Write texture names.
                    // These need to be written securely.
                    writeStringIntoBufferSafe( engineInterface, theTexture->GetName(), metaHeader.name, sizeof( metaHeader.name ), theTexture, L"name" );
                    writeStringIntoBufferSafe( engineInterface, theTexture->GetMaskName(), metaHeader.maskName, sizeof( metaHeader.maskName ), theTexture, L"mask name" );

                    metaHeader.hasAlpha = ( hasAlpha ? 1 : 0 );

                    metaHeader.rasterFormat = rasterFormatFlags;

                    metaHeader.width = baseLayer.layerWidth;
                    metaHeader.height = baseLayer.layerHeight;
                    metaHeader.depth = depth;
                    metaHeader.mipmapCount = (uint8)mipmapCount;
                    metaHeader.rasterType = platformTex->rasterType;

                    // TODO: must make sure that pre 3.5 textures cannot have palette colors assigned to them!
                    // We need to investigate this properly, since palette could still be supported but be
                    // encoded in a weird way!
                    assert( palettePixelFormat == GVRPIX_NO_PALETTE );

                    metaHeader.isCompressed = isCompressed;

                    // Write it.
                    gcNativeBlock.writeStruct( metaHeader );
                }
                else
                {
                    // This is a less-advanced version but we can still save the most important stuff completely.

                    gamecube::textureMetaHeaderStructGeneric32 metaHeader;

                    metaHeader.texFormat = formatInfo;

                    // Write texture names.
                    // These need to be written securely.
                    writeStringIntoBufferSafe( engineInterface, theTexture->GetName(), metaHeader.name, sizeof( metaHeader.name ), theTexture, L"name" );
                    writeStringIntoBufferSafe( engineInterface, theTexture->GetMaskName(), metaHeader.maskName, sizeof( metaHeader.maskName ), theTexture, L"mask name" );

                    metaHeader.hasAlpha = ( hasAlpha ? 1 : 0 );

                    metaHeader.rasterFormat = rasterFormatFlags;

                    metaHeader.width = baseLayer.layerWidth;
                    metaHeader.height = baseLayer.layerHeight;
                    metaHeader.depth = depth;
                    metaHeader.mipmapCount = (uint8)mipmapCount;
                    metaHeader.rasterType = platformTex->rasterType;

                    metaHeader.isCompressed = isCompressed;

                    // Write it.
                    gcNativeBlock.writeStruct( metaHeader );
                }
            }
        }

        // Now write palette data.
        if ( palettePixelFormat != GVRPIX_NO_PALETTE )
        {
            uint32 palRasterDepth = getGVRPixelFormatDepth( palettePixelFormat );

            uint32 reqPalCount = getGCPaletteSize( internalFormat );

            uint32 paletteCount = platformTex->paletteSize;

            uint32 palDataSize = getPaletteDataSize( paletteCount, palRasterDepth );

            writePartialBlockSafe(gcNativeBlock, platformTex->palette, palDataSize, getPaletteDataSize(reqPalCount, palRasterDepth));
        }

        // We need to write the total size of image data.
        // For that we have to calculate it.
        uint32 imageDataSectionSize = 0;

        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            const NativeTextureGC::mipmapLayer& mipLayer = platformTex->mipmaps[ n ];

            imageDataSectionSize += mipLayer.dataSize;
        }

        endian::big_endian <uint32> sectionSizeBE = imageDataSectionSize;

        gcNativeBlock.writeStruct( sectionSizeBE );

        // Write all the mipmap layers now.
        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            const NativeTextureGC::mipmapLayer& mipLayer = platformTex->mipmaps[ n ];

            uint32 texDataSize = mipLayer.dataSize;

            const void *texels = mipLayer.texels;

            gcNativeBlock.write( texels, texDataSize );
        }

        // Finito.
    }

    // Also write it's extensions.
    engineInterface->SerializeExtensions( theTexture, outputProvider );
}

void gamecubeNativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

    eGCNativeTextureFormat internalFormat = nativeTex->internalFormat;
    eGCPixelFormat palettePixelFormat = nativeTex->palettePixelFormat;

    // We want to put the GC native texture data into a framework friendly format and return it.
    eRasterFormat dstRasterFormat;
    uint32 dstDepth;
    uint32 dstRowAlignment = 4; // for good measure.
    eColorOrdering dstColorOrder;

    ePaletteType dstPaletteType;
    eCompressionType dstCompressionType;

    bool gotFormat =
        getRecommendedGCNativeTextureRasterFormat(
            internalFormat, palettePixelFormat,
            dstRasterFormat, dstDepth, dstColorOrder, dstPaletteType,
            dstCompressionType
        );

    if ( !gotFormat )
    {
        // Since this should never happen anyway, we treat this as an error.
        throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_NOPIXFMT" );
    }

    void *paletteData = nativeTex->palette;
    uint32 paletteSize = nativeTex->paletteSize;

    try
    {
        // Convert all the mipmap layers.
        size_t mipmapCount = nativeTex->mipmaps.GetCount();

        pixelsOut.mipmaps.Resize( mipmapCount );

        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            const NativeTextureGC::mipmapLayer& srcLayer = nativeTex->mipmaps[ n ];

            // We have to unswizzle the layer.
            // TODO: sometimes we do not have to do anything; check when and just return the layer then!
            uint32 mipWidth = srcLayer.width;
            uint32 mipHeight = srcLayer.height;

            uint32 layerWidth = srcLayer.layerWidth;
            uint32 layerHeight = srcLayer.layerHeight;

            void *srcTexels = srcLayer.texels;
            uint32 srcDataSize = srcLayer.dataSize;

            // Those are the destination properties that have to be written
            // into the framework-friendly layer.
            uint32 dstSurfWidth, dstSurfHeight;
            void *dstTexels;
            uint32 dstDataSize;

            ConvertGCMipmapToRasterFormat(
                engineInterface, mipWidth, mipHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
                internalFormat, palettePixelFormat,
                dstPaletteType, paletteData, paletteSize,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstCompressionType,
                dstSurfWidth, dstSurfHeight,
                dstTexels,
                dstDataSize
            );

            // Write the properties.
            pixelDataTraversal::mipmapResource dstLayer;
            dstLayer.width = dstSurfWidth;
            dstLayer.height = dstSurfHeight;

            dstLayer.layerWidth = layerWidth;
            dstLayer.layerHeight = layerHeight;

            dstLayer.texels = dstTexels;
            dstLayer.dataSize = dstDataSize;

            pixelsOut.mipmaps[ n ] = std::move( dstLayer );
        }

        // Process the palette aswell, if required.
        void *dstPaletteData = nullptr;

        if ( palettePixelFormat != GVRPIX_NO_PALETTE )
        {
            dstPaletteData = GetGCPaletteData(
                engineInterface, paletteData, paletteSize,
                palettePixelFormat, dstRasterFormat, dstColorOrder
            );
        }

        // Return the stuff.
        pixelsOut.rasterFormat = dstRasterFormat;
        pixelsOut.depth = dstDepth;
        pixelsOut.rowAlignment = dstRowAlignment;
        pixelsOut.colorOrder = dstColorOrder;
        pixelsOut.paletteType = dstPaletteType;
        pixelsOut.paletteSize = paletteSize;
        pixelsOut.paletteData = dstPaletteData;
        pixelsOut.compressionType = dstCompressionType;

        pixelsOut.hasAlpha = nativeTex->hasAlpha;
        pixelsOut.autoMipmaps = nativeTex->autoMipmaps;
        pixelsOut.cubeTexture = false;
        pixelsOut.rasterType = nativeTex->rasterType;

        // For now we always newly allocate because the most native formats do swizzling and stuff.
        // We want to investigate things and return some formats directly!
        pixelsOut.isNewlyAllocated = true;
    }
    catch( ... )
    {
        // Since things failed, we want to clear our pixel data.
        pixelsOut.FreePixels( engineInterface );

        throw;
    }
}

void gamecubeNativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, texNativeTypeProvider::acquireFeedback_t& acquireFeedback )
{
    eCompressionType srcCompressionType = pixelsIn.compressionType;

    assert( srcCompressionType == RWCOMPRESS_NONE || srcCompressionType == RWCOMPRESS_DXT1 );

    // Make sure that the texel data obeys size rules.
    {
        nativeTextureSizeRules sizeRules;

        bool isDXTCompressed = ( srcCompressionType == RWCOMPRESS_DXT1 );

        NativeTextureGC::getSizeRules( isDXTCompressed, sizeRules );

        if ( !sizeRules.verifyPixelData( pixelsIn ) )
        {
            throw NativeTextureInvalidConfigurationException( "Gamecube", L"GAMECUBE_INVALIDCFG_MIPDIMMS" );
        }
    }

    NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

    LibraryVersion texVersion = nativeTex->texVersion;

    // Get all properties on the stack.
    eRasterFormat srcRasterFormat = pixelsIn.rasterFormat;
    uint32 srcDepth = pixelsIn.depth;
    uint32 srcRowAlignment = pixelsIn.rowAlignment;
    eColorOrdering srcColorOrder = pixelsIn.colorOrder;

    ePaletteType srcPaletteType = pixelsIn.paletteType;
    void *srcPaletteData = pixelsIn.paletteData;
    uint32 srcPaletteSize = pixelsIn.paletteSize;

    // Decide about the destination native format.
    // The problem with the destination raster format is that it depends on the
    // RW version. Prior to 3.3.0.2 this native texture did not support luminance
    // based formats.
    bool doesSupportLuminance = ( texVersion.isNewerThan( LibraryVersion( 3, 3, 0, 2 ) ) );

    eGCNativeTextureFormat dstInternalFormat;
    eGCPixelFormat dstPalettePixelFormat = GVRPIX_NO_PALETTE;

    bool gotFormat = false;

    if ( srcCompressionType == RWCOMPRESS_DXT1 )
    {
        dstInternalFormat = GVRFMT_CMP;

        gotFormat = true;
    }
    else if ( srcCompressionType == RWCOMPRESS_NONE )
    {
        if ( srcPaletteType != PALETTE_NONE )
        {
            // We want to create a palette texture, too!
            if ( srcPaletteType == PALETTE_4BIT ||
                 srcPaletteType == PALETTE_4BIT_LSB )
            {
                dstInternalFormat = GVRFMT_PAL_4BIT;
            }
            else if ( srcPaletteType == PALETTE_8BIT )
            {
                dstInternalFormat = GVRFMT_PAL_8BIT;
            }
            else
            {
                // We cannot pick a special palette format, because the index bit-width is very
                // critical to image quality.
                throw NativeTextureInvalidConfigurationException( "Gamecube", L"GAMECUBE_INVALIDCFG_PALTYPE" );
            }

            // Alright, now we gotta pick a good palette pixel format.
            // We kinda have to drop quality here for the most time.
            if ( srcRasterFormat == RASTER_565 ||
                 srcRasterFormat == RASTER_888 )
            {
                dstPalettePixelFormat = GVRPIX_RGB565;

                gotFormat = true;
            }
            else if ( srcRasterFormat == RASTER_1555 ||
                      srcRasterFormat == RASTER_4444 ||
                      srcRasterFormat == RASTER_8888 ||
                      srcRasterFormat == RASTER_555 )
            {
                dstPalettePixelFormat = GVRPIX_RGB5A3;

                gotFormat = true;
            }
            else if ( doesSupportLuminance &&
                      ( srcRasterFormat == RASTER_LUM ||
                        srcRasterFormat == RASTER_LUM_ALPHA ) )
            {
                dstPalettePixelFormat = GVRPIX_LUM_ALPHA;

                gotFormat = true;
            }
            else
            {
                // We have no idea, so lets try our best format.
                dstPalettePixelFormat = GVRPIX_RGB5A3;

                gotFormat = true;
            }
        }
        else
        {
            if ( srcRasterFormat == RASTER_1555 )
            {
                // TODO: allow fine-grained quality adjustments by RW config.
                dstInternalFormat = GVRFMT_RGB5A3;

                gotFormat = true;
            }
            else if ( srcRasterFormat == RASTER_565 )
            {
                dstInternalFormat = GVRFMT_RGB565;

                gotFormat = true;
            }
            else if ( srcRasterFormat == RASTER_4444 )
            {
                // TODO: allow fine-grained quality adjustments by RW config.
                dstInternalFormat = GVRFMT_RGB5A3;

                gotFormat = true;
            }
            else if ( doesSupportLuminance && srcRasterFormat == RASTER_LUM )
            {
                // Depends on the depth.
                if ( srcDepth == 4 )
                {
                    dstInternalFormat = GVRFMT_LUM_4BIT;

                    gotFormat = true;
                }
                else if ( srcDepth == 8 )
                {
                    dstInternalFormat = GVRFMT_LUM_8BIT;

                    gotFormat = true;
                }
                else
                {
                    // Well, we have no idea about this format.
                    // Let's just pick the best.
                    dstInternalFormat = GVRFMT_LUM_8BIT;

                    gotFormat = true;
                }
            }
            else if ( doesSupportLuminance && srcRasterFormat == RASTER_LUM_ALPHA )
            {
                // Depends on the depth.
                if ( srcDepth == 8 )
                {
                    dstInternalFormat = GVRFMT_LUM_4BIT_ALPHA;

                    gotFormat = true;
                }
                else if ( srcDepth == 16 )
                {
                    dstInternalFormat = GVRFMT_LUM_8BIT_ALPHA;

                    gotFormat = true;
                }
                else
                {
                    // No idea again. Just pick the best.
                    dstInternalFormat = GVRFMT_LUM_8BIT_ALPHA;

                    gotFormat = true;
                }
            }
            else if ( srcRasterFormat == RASTER_8888 )
            {
                dstInternalFormat = GVRFMT_RGBA8888;

                gotFormat = true;
            }
            else if ( srcRasterFormat == RASTER_888 )
            {
                dstInternalFormat = GVRFMT_RGBA8888;

                gotFormat = true;
            }
            else if ( srcRasterFormat == RASTER_555 )
            {
                dstInternalFormat = GVRFMT_RGB5A3;

                gotFormat = true;
            }
            else
            {
                // No idea about the raw sample format, so we just pick the best quality possible.
                dstInternalFormat = GVRFMT_RGBA8888;

                gotFormat = true;
            }
        }
    }
    else
    {
        throw NativeTextureInvalidConfigurationException( "Gamecube", L"GAMECUBE_INVALIDCFG_COMPRTYPE" );
    }

    if ( !gotFormat )
    {
        throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_NOFMTMAP" );
    }

    // Alright, we got our valid destination format.
    // Now we can convert the layers!
    size_t mipmapCount = pixelsIn.mipmaps.GetCount();

    for ( size_t mip_index = 0; mip_index < mipmapCount; mip_index++ )
    {
        const pixelDataTraversal::mipmapResource& srcLayer = pixelsIn.mipmaps[ mip_index ];

        uint32 mipWidth = srcLayer.width;
        uint32 mipHeight = srcLayer.height;

        uint32 layerWidth = srcLayer.layerWidth;
        uint32 layerHeight = srcLayer.layerHeight;

        void *srcTexels = srcLayer.texels;
        uint32 srcDataSize = srcLayer.dataSize;

        // Convert to native format.
        uint32 dstSurfWidth, dstSurfHeight;
        void *dstTexels = nullptr;
        uint32 dstDataSize = 0;

        TranscodeIntoNativeGCLayer(
            engineInterface,
            mipWidth, mipHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
            srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder,
            srcPaletteType, srcPaletteSize, srcCompressionType,
            dstInternalFormat, dstPalettePixelFormat,
            dstSurfWidth, dstSurfHeight,
            dstTexels, dstDataSize
        );

        try
        {
            // Put into the native texture.
            NativeTextureGC::mipmapLayer dstLayer;
            dstLayer.width = dstSurfWidth;
            dstLayer.height = dstSurfHeight;

            dstLayer.layerWidth = layerWidth;
            dstLayer.layerHeight = layerHeight;

            dstLayer.texels = dstTexels;
            dstLayer.dataSize = dstDataSize;

            nativeTex->mipmaps.AddToBack( std::move( dstLayer ) );
        }
        catch( ... )
        {
            engineInterface->PixelFree( dstTexels );

            throw;
        }
    }

    // Now convert the palette, if present/required.
    void *dstPaletteData = nullptr;

    if ( dstPalettePixelFormat != GVRPIX_NO_PALETTE )
    {
        dstPaletteData =
            TranscodeGCPaletteData(
                engineInterface,
                srcPaletteData, srcPaletteSize,
                srcRasterFormat, srcColorOrder,
                dstPalettePixelFormat
            );
    }

    // Alright! We can store all the things now.
    nativeTex->internalFormat = dstInternalFormat;
    nativeTex->palettePixelFormat = dstPalettePixelFormat;

    nativeTex->palette = dstPaletteData;
    nativeTex->paletteSize = srcPaletteSize;    // must stay the same.

    nativeTex->autoMipmaps = pixelsIn.autoMipmaps;
    nativeTex->rasterType = pixelsIn.rasterType;

    nativeTex->hasAlpha = pixelsIn.hasAlpha;

    // We ignore the unknowns. They should have good defaults already.

    // We actually never directly acquire, but this can change in the future!
    acquireFeedback.hasDirectlyAcquired = false;
}

inline void resetNativeTexture( Interface *engineInterface, NativeTextureGC *nativeTex, bool deallocate )
{
    if ( deallocate )
    {
        deleteMipmapLayers( engineInterface, nativeTex->mipmaps );

        // Delete the palette data.
        if ( void *paletteData = nativeTex->palette )
        {
            engineInterface->PixelFree( paletteData );
        }
    }

    // Clear things.
    nativeTex->mipmaps.Clear();

    nativeTex->palette = nullptr;
    nativeTex->paletteSize = 0;

    // Reset the status of this texture.
    nativeTex->internalFormat = GVRFMT_RGBA8888;
    nativeTex->palettePixelFormat = GVRPIX_NO_PALETTE;
    nativeTex->autoMipmaps = false;
    nativeTex->rasterType = 4;
    nativeTex->hasAlpha = false;
    nativeTex->unk1 = 0;
    nativeTex->unk2 = 1;
    nativeTex->unk3 = 1;
    nativeTex->unk4 = 0;
}

void gamecubeNativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    // Clear all texture data from our GameCube texture.
    NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

    resetNativeTexture( engineInterface, nativeTex, deallocate );
}

void NativeTextureGC::UpdateStructure( void )
{
    // We check whether our native texture can support the data that it currently holds.
    // If there is any mismatch, we downgrade the data to a compatible format.

    LibraryVersion curVer = this->texVersion;

    eGCNativeTextureFormat internalFormat = this->internalFormat;
    eGCPixelFormat palettePixelFormat = this->palettePixelFormat;

    uint32 paletteSize = this->paletteSize;

    // Figure out our support capabilities.
    bool supportsLuminance = ( curVer.isNewerThan( LibraryVersion( 3, 3, 0, 2 ) ) );

    // Check if there is any conflict to our current support.
    // Also determine, if there is a conflict, what format we should map to instead.
    bool doesConflictExist = false;

    eGCNativeTextureFormat dstInternalFormat;
    eGCPixelFormat dstPalettePixelFormat = palettePixelFormat;

    if ( supportsLuminance == false )
    {
        // Check if we have a luminance based format in any way.
        if ( palettePixelFormat != GVRPIX_NO_PALETTE )
        {
            if ( palettePixelFormat == GVRPIX_LUM_ALPHA )
            {
                // Need to map to our strongest palette pixel format.
                dstInternalFormat = internalFormat;
                dstPalettePixelFormat = GVRPIX_RGB5A3;

                doesConflictExist = true;
            }
        }
        else
        {
            if ( internalFormat == GVRFMT_LUM_4BIT ||
                 internalFormat == GVRFMT_LUM_8BIT ||
                 internalFormat == GVRFMT_LUM_4BIT_ALPHA ||
                 internalFormat == GVRFMT_LUM_8BIT_ALPHA )
            {
                // We need to map to full color raw.
                dstInternalFormat = GVRFMT_RGBA8888;

                doesConflictExist = true;
            }
        }
    }

    // If there is no conflict, we do not have to worry :)
    if ( !doesConflictExist )
        return;

    // We do not support compression conversion, and do not need that anyway.
    assert( internalFormat != GVRFMT_CMP );
    assert( dstInternalFormat != GVRFMT_CMP );

    // Transform the color data into the format that we want to be put into, ugh.
    // Luckily there is no complicated packing in this native texture, so we can simply process the layers.

    // TODO: we need random-access-style accessors for internalFormat permutations, so that we can
    // convert without brainmelt!

    // Get common properties.
    uint32 srcDepth = getGCInternalFormatDepth( internalFormat );
    uint32 dstDepth = getGCInternalFormatDepth( dstInternalFormat );

    uint32 srcClusterWidth, srcClusterHeight, srcClusterCount;
    uint32 dstClusterWidth, dstClusterHeight, dstClusterCount;

    bool getSrcClusterProps = getGVRNativeFormatClusterDimensions( srcDepth, srcClusterWidth, srcClusterHeight, srcClusterCount );
    bool getDstClusterProps = getGVRNativeFormatClusterDimensions( dstDepth, dstClusterWidth, dstClusterHeight, dstClusterCount );

    if ( !getSrcClusterProps || !getDstClusterProps )
    {
        // Really, really should not happen.
        throw NativeTextureInternalErrorException( "Gamecube", L"GAMECUBE_INTERNERR_NOCLUSTERPROPS" );
    }

    bool isSrcFormatSwizzled = isGVRNativeFormatSwizzled( internalFormat );
    bool isDstFormatSwizzled = isGVRNativeFormatSwizzled( dstInternalFormat );

    try
    {
        // Process the texel layers first.
        {
            size_t mipmapCount = this->mipmaps.GetCount();

            for ( size_t n = 0; n < mipmapCount; n++ )
            {
                NativeTextureGC::mipmapLayer& mipLayer = this->mipmaps[ n ];

                uint32 srcMipWidth = mipLayer.width;
                uint32 srcMipHeight = mipLayer.height;

                uint32 layerWidth = mipLayer.layerWidth;
                uint32 layerHeight = mipLayer.layerHeight;

                void *srcTexels = mipLayer.texels;
                //uint32 srcDataSize = mipLayer.dataSize;

                // Transform this mipmap layer into another format.
                uint32 dstMipWidth, dstMipHeight;
                void *dstTexels;
                uint32 dstDataSize;

                TransformRawGCMipmapLayer(
                    engineInterface,
                    srcTexels, layerWidth, layerHeight,
                    internalFormat, dstInternalFormat, srcMipWidth, srcMipHeight,
                    srcDepth, dstDepth,
                    srcClusterWidth, srcClusterHeight, srcClusterCount,
                    dstClusterWidth, dstClusterHeight, dstClusterCount,
                    isSrcFormatSwizzled, isDstFormatSwizzled,
                    paletteSize,
                    dstMipWidth, dstMipHeight,
                    dstTexels, dstDataSize
                );

                // Delete old texels.
                if ( srcTexels != dstTexels )   // in preparation for the future ;)
                {
                    engineInterface->PixelFree( srcTexels );
                }

                // Replace the layer data.
                mipLayer.width = dstMipWidth;
                mipLayer.height = dstMipHeight;

                mipLayer.texels = dstTexels;
                mipLayer.dataSize = dstDataSize;
            }
        }

        // Process palette too, if present.
        if ( dstPalettePixelFormat != GVRPIX_NO_PALETTE )
        {
            assert( palettePixelFormat != GVRPIX_NO_PALETTE );

            void *paletteData = this->palette;

            TransformGCPaletteData(
                paletteData, paletteSize,
                palettePixelFormat, dstPalettePixelFormat
            );
        }
    }
    catch( ... )
    {
        // Unfortunately if any error occured we have to pull the safety plug and clear the native texture.
        // This is becaused we optimize for the case that no error occurs because this is a closed architecture.

        resetNativeTexture( engineInterface, this, true );

        // Pass on the error anyway.
        throw;
    }

    // Set new format properties.
    this->internalFormat = dstInternalFormat;
    this->palettePixelFormat = dstPalettePixelFormat;
}

static optional_struct_space <PluginDependantStructRegister <gamecubeNativeTextureTypeProvider, RwInterfaceFactory_t>> gcNativeTypeRegister;

#if 0
// Some tests to ensure correct functionality of Gamecube texture encoding.
inline void TestSpecificEncoding( eGCNativeTextureFormat encodingFormat )
{
    const uint32 gcTestDepth = getGCInternalFormatDepth( encodingFormat );

    const uint32 gcTestWidth = 64;
    const uint32 gcTestHeight = 32;

    uint32 clusterWidth, clusterHeight, clusterCount;
    getGVRNativeFormatClusterDimensions( gcTestDepth, clusterWidth, clusterHeight, clusterCount );

    memcodec::permutationUtilities::TestTileEncoding(
        gcTestWidth, gcTestHeight,
        clusterWidth, clusterHeight, clusterCount
    );

    // Test a tiny mipmap.
    memcodec::permutationUtilities::TestTileEncoding(
        2, 4,
        clusterWidth, clusterHeight, clusterCount
    );
}

inline void TestMemoryEncodingIntegrity( void )
{
    TestSpecificEncoding( GVRFMT_RGBA8888 );
    TestSpecificEncoding( GVRFMT_PAL_4BIT );
    TestSpecificEncoding( GVRFMT_PAL_8BIT );
    TestSpecificEncoding( GVRFMT_RGB5A3 );
}
#endif

void registerGCNativePlugin( void )
{
#if 0
    // INTEGRITY TEST FOR MEMORY ENCODING.
    TestMemoryEncodingIntegrity();
#endif

    gcNativeTypeRegister.Construct( engineFactory );
}

void unregisterGCNativePlugin( void )
{
    gcNativeTypeRegister.Destroy();
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_GAMECUBE
