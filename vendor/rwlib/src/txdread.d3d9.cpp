/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.d3d9.cpp
*  PURPOSE:     Direct3D 9 native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9

#include "txdread.d3d9.hxx"

#include "pixelformat.hxx"

#include "txdread.d3d.dxt.hxx"

#include "pluginutil.hxx"

#include <sdk/NumericFormat.h>

namespace rw
{

void d3d9NativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    {
        BlockProvider texNativeImageStruct( &inputProvider, CHUNK_STRUCT );

        d3d9::textureMetaHeaderStructGeneric metaHeader;
        texNativeImageStruct.read( &metaHeader, sizeof(metaHeader) );

	    uint32 platform = metaHeader.platformDescriptor;

	    if (platform != PLATFORM_D3D9)
        {
            throw NativeTextureRwObjectsStructuralErrorException( "Direct3D9", AcquireObject( theTexture ), L"D3D9_STRUCTERR_PLATFORMID" );
        }

        // Recast the texture to our native type.
        NativeTextureD3D9 *platformTex = (NativeTextureD3D9*)nativeTex;

        int engineWarningLevel = engineInterface->GetWarningLevel();

        bool engineIgnoreSecureWarnings = engineInterface->GetIgnoreSecureWarnings();

        // Read the texture names.
        {
            char tmpbuf[ sizeof( metaHeader.name ) + 1 ];

            // Make sure the name buffer is zero terminted.
            tmpbuf[ sizeof( metaHeader.name ) ] = '\0';

            // Move over the texture name.
            memcpy( tmpbuf, metaHeader.name, sizeof( metaHeader.name ) );

            theTexture->SetName( tmpbuf );

            // Move over the texture mask name.
            memcpy( tmpbuf, metaHeader.maskName, sizeof( metaHeader.maskName ) );

            theTexture->SetMaskName( tmpbuf );
        }

        // Read texture format.
        texFormatInfo texFormat = metaHeader.texFormat;

        texFormat.parse( *theTexture );

        // Deconstruct the format flags.
        bool hasMipmaps = false;    // TODO: actually use this flag.
        ePaletteType texPaletteType;

        readRasterFormatFlags( metaHeader.rasterFormat, platformTex->rasterFormat, texPaletteType, hasMipmaps, platformTex->autoMipmaps );

        platformTex->paletteType = texPaletteType;  // that the texture has a palette is a constant property, it can throw exceptions tho.
        platformTex->hasAlpha = false;

        // Read the D3DFORMAT field.
        D3DFORMAT d3dFormat = metaHeader.d3dFormat; // can be really anything.

        platformTex->d3dFormat = d3dFormat;

        bool doTrustDepthValue = false;

        uint32 header_depth = metaHeader.depth;         // We do not trust this depth value at all, so we have to verify it first.
        uint32 maybeMipmapCount = metaHeader.mipmapCount;

        if ( maybeMipmapCount == 0 )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "Direct3D9", AcquireObject( theTexture ), L"D3D9_STRUCTERR_NOMIPMAPS" );
        }

        platformTex->rasterType = metaHeader.rasterType;

        bool hasExpandedFormatRegion = ( metaHeader.isNotRwCompatible == true );    // LEGACY: "compression flag"

        {
            // Here we decide about alpha.
	        platformTex->hasAlpha = metaHeader.hasAlpha;
            platformTex->isCubeTexture = metaHeader.isCubeTexture;
            platformTex->autoMipmaps = metaHeader.autoMipMaps;

	        if ( hasExpandedFormatRegion )
            {
                // If we are a texture with expanded format region, we can map to much more than original RW textures.
                // We can be a compressed texture, or something entirely different that we do not know about.

                // A really bad thing is that we cannot check the D3DFORMAT field for validity.
                // There are countless undefined formats out there that we must be able to just "pass on".

		        // Detect FOUR-CC versions for compression method.
                eCompressionType comprType = getFrameworkCompressionTypeFromD3DFORMAT( d3dFormat );

                platformTex->colorComprType = comprType;
            }
	        else
            {
                // There is never compression in original RW.
		        platformTex->colorComprType = RWCOMPRESS_NONE;
            }
        }

        // Verify raster properties and attempt to fix broken textures.
        // Broken textures travel with mods like San Andreas Retextured.
        // - Verify compression.
        {
            eCompressionType actualCompression = getFrameworkCompressionTypeFromD3DFORMAT( d3dFormat );

            if (actualCompression != platformTex->colorComprType)
            {
                engineInterface->PushWarningObject( theTexture, L"D3D9_WARN_INVCOMPRPARAMS" );

                platformTex->colorComprType = actualCompression;
            }
        }
        // - Verify that there is no conflict.
        eCompressionType colorComprType = platformTex->colorComprType;

        uint32 dxtType = 0;
        bool isDXTCompressed = IsDXTCompressionType( colorComprType, dxtType );
        {
            bool hasPalette = ( texPaletteType != PALETTE_NONE );
            bool hasTextureCompression = ( colorComprType != RWCOMPRESS_NONE );

            if ( hasPalette && hasTextureCompression )
            {
                // TODO: actually analyze what the engine does prefer (either palette or compression) and then turn this into a warning only.
                throw NativeTextureRwObjectsStructuralErrorException( "Direct3D9", AcquireObject( theTexture ), L"D3D9_STRUCTERR_AMBIGUOUS_PALCOMPR" );
            }
        }
        // - Verify raster format.
        bool d3dRasterFormatLink = false;
        d3dpublic::nativeTextureFormatHandler *usedFormatHandler = nullptr;
        {
            eColorOrdering colorOrder = COLOR_BGRA;

            bool isValidFormat = false;
            bool isRasterFormatRequired = true;

            eRasterFormat d3dRasterFormat;

            bool isD3DFORMATImportant = true;
            (void)isD3DFORMATImportant;

            bool hasActualD3DFormat = false;
            D3DFORMAT actualD3DFormat;

            bool hasReportedStrongWarning = false;

            // Do special logic for palettized textures.
            // (thank you DK22Pac)
            if (texPaletteType != PALETTE_NONE)
            {
                // Verify that we have a valid bit depth.
                bool hasValidPaletteBitDepth = false;

                if (texPaletteType == PALETTE_4BIT)
                {
                    if (header_depth == 4 || header_depth == 8)
                    {
                        hasValidPaletteBitDepth = true;
                    }
                }
                else if (texPaletteType == PALETTE_8BIT)
                {
                    if (header_depth == 8)
                    {
                        hasValidPaletteBitDepth = true;
                    }
                }

                if ( !hasValidPaletteBitDepth )
                {
                    throw NativeTextureRwObjectsStructuralErrorException( "Direct3D9", AcquireObject( theTexture ), L"D3D9_STRUCTERR_INVPALDEPTH" );
                }

                // We now do trust this depth value.
                platformTex->depth = header_depth;

                doTrustDepthValue = true;

                // This overrides the D3DFORMAT field.
                // We are forced to use the eRasterFormat property.
                isD3DFORMATImportant = false;

                colorOrder = COLOR_RGBA;

                d3dRasterFormat = platformTex->rasterFormat;

                hasActualD3DFormat = true;
                actualD3DFormat = D3DFMT_P8;

                isValidFormat = ( d3dFormat == D3DFMT_P8 );

                // Basically, we have to tell the user that it should have had a palette D3DFORMAT.
                if ( isValidFormat == false )
                {
                    if ( engineIgnoreSecureWarnings == false )
                    {
                        engineInterface->PushWarningObject( theTexture, L"D3D9_WARN_PALFMTMISMATCH" );

                        hasReportedStrongWarning = true;
                    }
                }
            }
            else
            {
                // Set it for clarity sake.
                // We do not load entirely compliant to GTA:SA, because we give higher priority to the D3DFORMAT field.
                // Even though we do that, it is preferable, since the driver implementation is more powerful than the RW original types.
                // TODO: add an interface property to enable GTA:SA-compliant loading behavior.
                isD3DFORMATImportant = true;

                // Get a valid D3DFORMAT to RW original types mapping.
                bool isVirtualFormat = false;

                isValidFormat = getRasterFormatFromD3DFormat(
                    d3dFormat, platformTex->hasAlpha,
                    d3dRasterFormat, colorOrder, isVirtualFormat
                );

                // If we have a direct format link, we can verify the bit depth!
                // This means we actually do have a bit depth anyway, for anything else
                // its a very shallow bet.
                if ( isValidFormat )
                {
                    uint32 d3dFormatDepth;

                    bool hasBitDepth;

                    // Legacy compatibility.
                    if ( isVirtualFormat )
                    {
                        // TODO: actually verify that textures have alpha!

                        d3dFormatDepth = Bitmap::getRasterFormatDepth( d3dRasterFormat );

                        hasBitDepth = true;
                    }
                    else
                    {
                        hasBitDepth = getD3DFORMATBitDepth( d3dFormat, d3dFormatDepth );
                    }

                    if ( !hasBitDepth )
                    {
                        throw NativeTextureRwObjectsInternalErrorException( "Direct3D9", AcquireObject( theTexture ), L"D3D9_INTERNERR_BITDEPTHRESOLVE" );
                    }

                    // If the bit depth is incorrect, we could notify the user, because the tool has not done a great job.
                    // But since the engine does not care about bit depth anyway, we spare this useless warning, at least for Magic.TXD :p
                    if ( header_depth != d3dFormatDepth )
                    {
                        if ( engineWarningLevel >= 5 )
                        {
                            engineInterface->PushWarningObject( theTexture, L"D3D9_WARN_INVCOLORDEPTHVALUE" );
                        }

                        // We do want to fix it.
                        header_depth = d3dFormatDepth;
                    }

                    // We can finally trust the depth value ;)
                    platformTex->depth = header_depth;

                    doTrustDepthValue = true;
                }
                else
                {
                    // There is no depth value that we can determine.
                    // Maybe we add plugin support for depth values in the future.
                    platformTex->depth = 0;
                }

                if ( isVirtualFormat )
                {
                    isRasterFormatRequired = false;
                }

                // Is the D3DFORMAT known by this implementation?
                // This is equivalent to the notion that we are a valid format.
                if ( isValidFormat == false )
                {
                    // We actually only do this if we have the extended format range enabled.
                    // TODO: make sure DK gets his converter weirdness sorted out.
                    //if ( hasExpandedFormatRegion )
                    {
                        // We could have a native format handler that has been registered to us as plugin.
                        // Try to look one up.
                        // If we have one, then we are known anyway!
                        d3dpublic::nativeTextureFormatHandler *formatHandler = this->GetFormatHandler( d3dFormat );

                        if ( formatHandler )
                        {
                            // Lets just use this.
                            usedFormatHandler = formatHandler;

                            // Just use default raster format.
                            d3dRasterFormat = RASTER_DEFAULT;

                            colorOrder = COLOR_BGRA;

                            // No required raster format.
                            isRasterFormatRequired = false;

                            isValidFormat = true;
                        }
                    }
                }

                // If everything else fails...
                if ( isValidFormat == false )
                {
                    // If the user wants to know about such things, notify him.
                    if ( engineIgnoreSecureWarnings == false )
                    {
                        auto fmtstr = eir::to_string <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( (uint32)d3dFormat );

                        engineInterface->PushWarningObjectSingleTemplate(
                            theTexture,
                            L"D3D9_WARN_D3DFORMAT_UNK",
                            L"d3dfmt",
                            fmtstr.GetConstString()
                        );

                        hasReportedStrongWarning = true;
                    }
                }
            }

            if ( isValidFormat == false )
            {
                // Fix it (if possible).
                if ( hasActualD3DFormat )
                {
                    d3dFormat = actualD3DFormat;

                    platformTex->d3dFormat = actualD3DFormat;

                    // We rescued ourselves into valid territory.
                    isValidFormat = true;
                }
            }

            // If we are a valid format, we are actually known, which means we have a D3DFORMAT -> eRasterFormat link.
            // This allows us to be handled by the Direct3D 9 RW implementation.
            bool isSupportedByRW3 = false;

            if ( isValidFormat )
            {
                // If the raster format is not required though, then it means that it actually has no link.
                if ( isRasterFormatRequired )
                {
                    d3dRasterFormatLink = true;

                    assert( doTrustDepthValue == true );

                    // Decide whether the raster format is supported by the RW3 specification of D3D9.
                    isSupportedByRW3 = isRasterFormatOriginalRWCompatible( d3dRasterFormat, colorOrder, header_depth, texPaletteType );
                }
            }
            else
            {
                // If we are a valid format that we know, we also have a d3dRasterFormat that we want to enforce.
                // Otherwise we are not entirely sure, so we should keep it as RASTER_DEFAULT.
                d3dRasterFormat = RASTER_DEFAULT;
            }

            // Check whether the raster format that we calculated matches the raster format that was given by the serialized native texture.
            eRasterFormat rasterFormat = platformTex->rasterFormat;

            if ( rasterFormat != d3dRasterFormat )
            {
                // This is a complicated check.
                // We actually allow specialized raster formats that the original RW implementation did not support.
                // Warnings to the user should still only be about the RW3 implementation.
                if ( !isValidFormat || !isRasterFormatRequired || isSupportedByRW3 )
                {
                    // We should only warn about a mismatching format if we kinda know what we are doing.
                    // Otherwise we have already warned the user about the invalid D3DFORMAT entry, that we base upon anyway.
                    if ( hasReportedStrongWarning == false )
                    {
                        if ( isRasterFormatRequired || !engineIgnoreSecureWarnings )
                        {
                            // Decide about the minimum warning level.
                            int minWarningLevel;

                            if ( isSupportedByRW3 )
                            {
                                minWarningLevel = 3;
                            }
                            else
                            {
                                minWarningLevel = 4;
                            }

                            if ( engineWarningLevel >= minWarningLevel )
                            {
                                engineInterface->PushWarningObject( theTexture, L"D3D9_WARN_INVRASTERFMT" );
                            }
                        }
                    }
                }

                // Fix it.
                platformTex->rasterFormat = d3dRasterFormat;
            }

            // Anything that maps directly to RW sample types is compatible by original RW.
            // The specification says that each texture should act as an array of samples.
            // If we are not compatible to original RW, we can take a more liberal approach and
            // see a compressed texture as an array of samples, because it decompresses to it.
            // Either way, this flag must be correct, so let's verify it.
            if ( !hasExpandedFormatRegion && !isSupportedByRW3 )
            {
                // We kinda have a broken texture here.
                // This may not load on the GTA:SA engine.
                engineInterface->PushWarningObject( theTexture, L"D3D9_WARN_EXTENDEDFMT_MISMATCH" );
            }

            // Store the color ordering.
            platformTex->colorOrdering = colorOrder;

            // Store whether we have the D3D raster format link.
            platformTex->d3dRasterFormatLink = d3dRasterFormatLink;

            // Store whether the raster format is actually compatible with RW3.
            platformTex->isOriginalRWCompatible = isSupportedByRW3;

            // Maybe we have a format handler.
            platformTex->anonymousFormatLink = usedFormatHandler;
        }

        if (texPaletteType != PALETTE_NONE)
        {
            // We kind of assume we have a valid D3D raster format link here.
            if ( d3dRasterFormatLink == false )
            {
                // If we do things correctly, this should never be triggered.
                throw NativeTextureRwObjectsInternalErrorException( "Direct3D9", AcquireObject( theTexture ), L"D3D9_STRUCTERR_PALTEXNOFORMAT" );
            }

            uint32 reqPalItemCount = getD3DPaletteCount( texPaletteType );

            uint32 palDepth = Bitmap::getRasterFormatDepth( platformTex->rasterFormat );

            assert( palDepth != 0 );

            size_t paletteDataSize = getPaletteDataSize( reqPalItemCount, palDepth );

            // Check whether we have palette data in the stream.
            texNativeImageStruct.check_read_ahead( paletteDataSize );

            void *palData = engineInterface->PixelAllocate( paletteDataSize );

            try
            {
	            texNativeImageStruct.read( palData, paletteDataSize );
            }
            catch( ... )
            {
                engineInterface->PixelFree( palData );

                throw;
            }

            // Store the palette.
            platformTex->palette = palData;
            platformTex->paletteSize = reqPalItemCount;
        }

        mipGenLevelGenerator mipLevelGen( metaHeader.width, metaHeader.height );

        if ( !mipLevelGen.isValidLevel() )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "Direct3D9", AcquireObject( theTexture ), L"D3D9_STRUCTERR_INVMIPDIMMS" );
        }

        uint32 mipmapCount = 0;

        uint32 processedMipmapCount = 0;

        // Decide if we have a "known" format.
        // We must verify data sizes of such known formats.
        bool isKnownFormat = ( d3dRasterFormatLink || isDXTCompressed );

        bool hasDamagedMipmaps = false;

        for (uint32 i = 0; i < maybeMipmapCount; i++)
        {
            bool couldEstablishLevel = true;

	        if (i > 0)
            {
                couldEstablishLevel = mipLevelGen.incrementLevel();
            }

            if (!couldEstablishLevel)
            {
                break;
            }

            // Create a new mipmap layer.
            NativeTextureD3D9::mipmapLayer newLayer;

            newLayer.layerWidth = mipLevelGen.getLevelWidth();
            newLayer.layerHeight = mipLevelGen.getLevelHeight();

            // Process dimensions.
            uint32 texWidth = newLayer.layerWidth;
            uint32 texHeight = newLayer.layerHeight;
            {
		        // DXT compression works on 4x4 blocks,
		        // no smaller values allowed
		        if (isDXTCompressed)
                {
			        texWidth = ALIGN_SIZE( texWidth, 4u );
                    texHeight = ALIGN_SIZE( texHeight, 4u );
		        }
            }

            newLayer.width = texWidth;
            newLayer.height = texHeight;

	        uint32 texDataSize = texNativeImageStruct.readUInt32();

            // We started processing this mipmap.
            processedMipmapCount++;

            // Verify the data size.
            // We can only do that if we know about its format.
            bool isValidMipmap = true;

            if ( isKnownFormat )
            {
                uint32 actualDataSize = 0;

                if (isDXTCompressed)
                {
                    uint32 texItemCount = ( texWidth * texHeight );

                    actualDataSize = getDXTRasterDataSize(dxtType, texItemCount);
                }
                else
                {
                    assert( doTrustDepthValue == true );

                    rasterRowSize rowSize = getD3DRasterDataRowSize( texWidth, header_depth );

                    actualDataSize = getRasterDataSizeByRowSize(rowSize, texHeight);
                }

                if (actualDataSize != texDataSize)
                {
                    isValidMipmap = false;
                }
            }
            else
            {
                // Check some general stuff.
                if ( texDataSize == 0 )
                {
                    isValidMipmap = false;
                }

                if ( isValidMipmap )
                {
                    // If we have a format plugin, make sure we match its size.
                    if ( usedFormatHandler != nullptr )
                    {
                        size_t shouldBeSize = usedFormatHandler->GetFormatTextureDataSize( texWidth, texHeight );

                        if ( texDataSize != shouldBeSize )
                        {
                            isValidMipmap = false;
                        }
                    }
                }
            }

            if ( !isValidMipmap )
            {
                // Even the Rockstar games texture generator appeared to have problems with mipmap generation.
                // This is why textures appear to have the size of zero.

                if (texDataSize != 0)
                {
                    if ( !engineIgnoreSecureWarnings )
                    {
                        engineInterface->PushWarningObject( theTexture, L"D3D9_WARN_MIPDAMAGE" );
                    }

                    hasDamagedMipmaps = true;
                }

                // Skip the damaged bytes.
                if (texDataSize != 0)
                {
                    texNativeImageStruct.skip( texDataSize );
                }
                break;
            }

            // We first have to check whether there is enough data in the stream.
            // Otherwise we would just flood the memory in case of an error;
            // that could be abused by exploiters.
            texNativeImageStruct.check_read_ahead( texDataSize );

            void *texelData = engineInterface->PixelAllocate( texDataSize );

            try
            {
	            texNativeImageStruct.read( texelData, texDataSize );
            }
            catch( ... )
            {
                engineInterface->PixelFree( texelData );

                throw;
            }

            // Store mipmap properties.
	        newLayer.dataSize = texDataSize;

            // Store the image data pointer.
	        newLayer.texels = texelData;

            // Put the layer.
            platformTex->mipmaps.AddToBack( std::move( newLayer ) );

            mipmapCount++;
        }

        if ( mipmapCount == 0 )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "Direct3D9", AcquireObject( theTexture ), L"D3D9_STRUCTERR_EMPTY" );
        }

        // mipmapCount can only be smaller than maybeMipmapCount.
        // This is logically true and would be absurd to assert here.

        if ( processedMipmapCount < maybeMipmapCount )
        {
            // Skip the remaining mipmaps (most likely zero-sized).
            bool hasSkippedNonZeroSized = false;

            for ( uint32 n = processedMipmapCount; n < maybeMipmapCount; n++ )
            {
                uint32 mipSize = texNativeImageStruct.readUInt32();

                if ( mipSize != 0 )
                {
                    hasSkippedNonZeroSized = true;

                    // Skip the section.
                    texNativeImageStruct.skip( mipSize );
                }
            }

            if ( !engineIgnoreSecureWarnings && !hasDamagedMipmaps )
            {
                // Print the debug message.
                if ( !hasSkippedNonZeroSized )
                {
                    engineInterface->PushWarningObject( theTexture, L"D3D9_WARN_ZEROSIZEDMIPMAPS" );
                }
                else
                {
                    engineInterface->PushWarningObject( theTexture, L"D3D9_WARN_MIPRULEVIOLATION" );
                }
            }
        }

        // Fix filtering mode.
        fixFilteringMode( *theTexture, mipmapCount );

        // - Verify auto mipmap
        {
            bool hasAutoMipmaps = platformTex->autoMipmaps;

            if ( hasAutoMipmaps )
            {
                bool canHaveAutoMipmaps = ( mipmapCount == 1 );

                if ( !canHaveAutoMipmaps )
                {
                    engineInterface->PushWarningObject( theTexture, L"D3D9_WARN_INVAUTOMIPMAP" );

                    platformTex->autoMipmaps = false;
                }
            }
        }
    }

    // Read extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

static optional_struct_space <PluginDependantStructRegister <d3d9NativeTextureTypeProvider, RwInterfaceFactory_t>> d3dNativeTexturePluginRegister;

void registerD3D9NativePlugin( void )
{
    d3dNativeTexturePluginRegister.Construct( engineFactory );
}

void unregisterD3D9NativePlugin( void )
{
    d3dNativeTexturePluginRegister.Destroy();
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_D3D9
