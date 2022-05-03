/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.gc.cpp
*  PURPOSE:     Gamecube native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_GAMECUBE

#include "txdread.gc.hxx"

#include "txdread.gc.miptrans.hxx"

namespace rw
{

eTexNativeCompatibility gamecubeNativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility compat = RWTEXCOMPAT_NONE;

    // Read the native struct.
    BlockProvider gcNativeBlock( &inputProvider, CHUNK_STRUCT );

    // We are definately a gamecube native texture if the platform descriptor matches.
    endian::big_endian <uint32> platformDescriptor;

    gcNativeBlock.readStruct( platformDescriptor );

    if ( platformDescriptor == PLATFORMDESC_GAMECUBE )
    {
        // We should definately be a gamecube native texture.
        // Whatever errors will crop up should be handled as rule violations of the GC native texture format.
        compat = RWTEXCOMPAT_ABSOLUTE;
    }

    return compat;
}

void gamecubeNativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    {
        // Read the native struct.
        BlockProvider gcNativeBlock( &inputProvider, CHUNK_STRUCT );

        // Verify the platform descriptor.
        {
            endian::big_endian <uint32> platformDescriptor;

            gcNativeBlock.readStruct( platformDescriptor );

            if ( platformDescriptor != PLATFORMDESC_GAMECUBE )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_STRUCTERR_PLATFORMID" );
            }
        }

        // Remember to parse the raster format!
        ePaletteType paletteType;
        bool hasMipmaps;
        bool autoMipmaps;

        // Read the main header.
        // This header has actually changed depending on the RW version, so we gotta read it indirectly.
        texFormatInfo header_texFormat;
        uint32 header_unk1 = 0;        // starting from 3.3.0.1
        uint32 header_unk2 = 1;        // starting from 3.3.0.1
        uint32 header_unk3 = 1;        // starting from 3.3.0.1
        uint32 header_unk4 = 0;        // starting from 3.3.0.1
        char header_name[32];
        char header_maskName[32];
        uint8 header_rasterType = 0;
        uint16 header_width = 0;
        uint16 header_height = 0;
        uint32 header_depth = 0;
        uint32 header_mipmapCount = 0;
        eGCNativeTextureFormat header_internalFormat = GVRFMT_RGBA8888;
        eGCPixelFormat header_palettePixelFormat = GVRPIX_NO_PALETTE;  // starting from 3.3.0.2
        uint32 header_hasAlpha = 0;
        {
            // The Gamecube native texture appears to have undergone a lot of change around version 3.3.0.1

            LibraryVersion nativeDataVer = gcNativeBlock.getBlockVersion();

            if ( nativeDataVer.isNewerThan( LibraryVersion( 3, 3, 0, 2 ) ) )
            {
                gamecube::textureMetaHeaderStructGeneric35 metaHeader;

                gcNativeBlock.readStruct( metaHeader );

                // This format is the latest and greatest.
                // It basically fills out everything.

                header_texFormat = metaHeader.texFormat;
                header_unk1 = metaHeader.unk1;
                header_unk2 = metaHeader.unk2;
                header_unk3 = metaHeader.unk3;
                header_unk4 = metaHeader.unk4;
                memcpy( header_name, metaHeader.name, sizeof( header_name ) );
                memcpy( header_maskName, metaHeader.maskName, sizeof( header_maskName ) );
                header_width = metaHeader.width;
                header_height = metaHeader.height;
                header_depth = metaHeader.depth;
                header_mipmapCount = metaHeader.mipmapCount;
                header_internalFormat = metaHeader.internalFormat;
                header_palettePixelFormat = metaHeader.palettePixelFormat;
                header_hasAlpha = metaHeader.hasAlpha;

                // Read the raster format flags.
                {
                    rwGenericRasterFormatFlags rasterFormatFlags = metaHeader.rasterFormat;

                    // We take the raster type property from the flags.
                    header_rasterType = rasterFormatFlags.data.rasterType;

                    // Investigate the format number.
                    // It has changed a little in comparison to old formats.
                    if ( header_internalFormat != GVRFMT_CMP )
                    {
                        if ( header_palettePixelFormat != GVRPIX_NO_PALETTE )
                        {
                            // Check the palette format number.
                            eGCPaletteFormat readPalFmt = (eGCPaletteFormat)rasterFormatFlags.data.formatNum;

                            eGCPaletteFormat expectedPalFmt = getGCPaletteFormatFromPalettePixelFormat( header_palettePixelFormat );

                            if ( readPalFmt != expectedPalFmt )
                            {
                                engineInterface->PushWarningObject( theTexture, L"GAMECUBE_WARN_RASFMTPALETTEMISMATCH" );
                            }
                        }
                        else
                        {
                            // Check the raster format number.
                            eGCRasterFormat readRasterFmt = (eGCRasterFormat)rasterFormatFlags.data.formatNum;

                            eGCRasterFormat expectedRasterFmt = getGCRasterFormatFromInternalFormat( header_internalFormat );

                            if ( readRasterFmt != expectedRasterFmt )
                            {
                                engineInterface->PushWarningObject( theTexture, L"GAMECUBE_WARN_RASFMTINTERNMISMATCH" );
                            }
                        }
                    }

                    gcReadCommonRasterFormatFlags(
                        rasterFormatFlags,
                        paletteType, hasMipmaps, autoMipmaps
                    );

                    // Maybe we want to store those private flags aswell.
                    // It really depends.
                }
            }
            else
            {
                // We are getting into the territory of really old formats.
                // Those have old format descriptors that have to be mapped to new format descriptors.
                bool header_isCompressed;

                rwGenericRasterFormatFlags header_rasterFormatFlags;

                if ( nativeDataVer.isNewerThan( LibraryVersion( 3, 3, 0, 0 ) ) )
                {
                    // There appears to have been a very temporary version.
                    // Gotta' add support for that.

                    gamecube::textureMetaHeaderStructGeneric33 metaHeader;

                    gcNativeBlock.readStruct( metaHeader );

                    header_texFormat = metaHeader.texFormat;
                    header_unk1 = metaHeader.unk1;  // appears to be something really native.
                    header_unk2 = metaHeader.unk2;
                    header_unk3 = metaHeader.unk3;
                    header_unk4 = metaHeader.unk4;
                    memcpy( header_name, metaHeader.name, sizeof( header_name ) );
                    memcpy( header_maskName, metaHeader.maskName, sizeof( header_maskName ) );
                    header_hasAlpha = metaHeader.hasAlpha;
                    header_rasterFormatFlags = metaHeader.rasterFormat;
                    header_width = metaHeader.width;
                    header_height = metaHeader.height;
                    header_depth = metaHeader.depth;
                    header_mipmapCount = metaHeader.mipmapCount;
                    header_isCompressed = metaHeader.isCompressed;
                    header_rasterType = metaHeader.rasterType;

                    // NOT SET: palettePixelFormat.
                }
                else
                {
                    gamecube::textureMetaHeaderStructGeneric32 metaHeader;

                    gcNativeBlock.readStruct( metaHeader );

                    // This format must have been in its early stages.
                    // Not a very complicated format in comparison to PS2.

                    header_texFormat = metaHeader.texFormat;
                    memcpy( header_name, metaHeader.name, sizeof( header_name ) );
                    memcpy( header_maskName, metaHeader.maskName, sizeof( header_maskName ) );
                    header_hasAlpha = metaHeader.hasAlpha;
                    header_rasterFormatFlags = metaHeader.rasterFormat;
                    header_width = metaHeader.width;
                    header_height = metaHeader.height;
                    header_depth = metaHeader.depth;
                    header_mipmapCount = metaHeader.mipmapCount;
                    header_isCompressed = metaHeader.isCompressed;
                    header_rasterType = metaHeader.rasterType;

                    // NOT SET: unk1, unk2, unk3, unk4, palettePixelFormat.
                }

                // Have to read the raster format flags here.
                {
                    // TODO: make sure we properly process the flags here.

                    gcReadCommonRasterFormatFlags(
                        header_rasterFormatFlags,
                        paletteType, hasMipmaps, autoMipmaps
                    );
                }

                // Instead of having a palettePixelFormat field in the metaHeader,
                // the old format depends on the palette flags in the raster format.

                // Map the old internalFormat to the new internalFormat.
                bool couldMapFormat = false;

                if ( paletteType != PALETTE_NONE )
                {
                    if ( paletteType == PALETTE_4BIT_LSB )
                    {
                        header_internalFormat = GVRFMT_PAL_4BIT;
                    }
                    else if ( paletteType == PALETTE_8BIT )
                    {
                        header_internalFormat = GVRFMT_PAL_8BIT;
                    }
                    else
                    {
                        throw NativeTextureRwObjectsStructuralErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_STRUCTERR_INVPALFMT" );
                    }

                    if ( header_isCompressed )
                    {
                        // TODO: find out whether compression or palette has preference and use it to process further; then turn this
                        // message into a warning only.
                        throw NativeTextureRwObjectsStructuralErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_STRUCTERR_AMBIGUOUSPALCOMPR" );
                    }

                    // Resolve things.
                    eGCRasterFormat rasterFormat = (eGCRasterFormat)header_rasterFormatFlags.data.formatNum;

                    if ( rasterFormat == GCRASTER_565 )
                    {
                        header_palettePixelFormat = GVRPIX_RGB565;

                        couldMapFormat = true;
                    }
                    else if ( rasterFormat == GCRASTER_RGB5A3 )
                    {
                        header_palettePixelFormat = GVRPIX_RGB5A3;

                        couldMapFormat = true;
                    }
                }
                else
                {
                    if ( header_isCompressed )
                    {
                        // Actually, this is another way of saying you are DXT compressed.
                        header_internalFormat = GVRFMT_CMP;

                        couldMapFormat = true;
                    }
                    else
                    {
                        // Depends on the format number.
                        eGCRasterFormat rasterFormat = (eGCRasterFormat)header_rasterFormatFlags.data.formatNum;

                        couldMapFormat = getGCInternalFormatFromRasterFormat( rasterFormat, header_internalFormat );
                    }
                }

                if ( !couldMapFormat )
                {
                    throw NativeTextureRwObjectsStructuralErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_STRUCTERR_OLDVER_FMTMAP" );
                }
            }
        }

        NativeTextureGC *platformTex = (NativeTextureGC*)nativeTex;

        //int engineWarningLevel = engineInterface->GetWarningLevel();

        //bool engineIgnoreSecureWarnings = engineInterface->GetIgnoreSecureWarnings();

        // Parse the header.
        // Read the texture names.
        {
            char tmpbuf[ sizeof( header_name ) + 1 ];

            // Make sure the name buffer is zero terminted.
            tmpbuf[ sizeof( header_name ) ] = '\0';

            // Move over the texture name.
            memcpy( tmpbuf, header_name, sizeof( header_name ) );

            theTexture->SetName( tmpbuf );

            // Move over the texture mask name.
            memcpy( tmpbuf, header_maskName, sizeof( header_maskName ) );

            theTexture->SetMaskName( tmpbuf );
        }

        // Parse the addressing and filtering properties.
        header_texFormat.parse( *theTexture );

        // Verify that it has correct entries.
        eGCNativeTextureFormat internalFormat = header_internalFormat;

        if ( isGVRNativeFormatRawSample( internalFormat ) == false &&
                internalFormat != GVRFMT_CMP )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_STRUCTERR_UNKGCNATTEXFMT" );
        }

        // Verify the palette format.
        eGCPixelFormat palettePixelFormat = header_palettePixelFormat;

        if ( palettePixelFormat != GVRPIX_NO_PALETTE &&
                palettePixelFormat != GVRPIX_LUM_ALPHA &&
                palettePixelFormat != GVRPIX_RGB5A3 &&
                palettePixelFormat != GVRPIX_RGB565 )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_STRUCTERR_UNKGCPALFMT" );
        }

        // Check whether we have a solid palette format description.
        // We cannot have the internal format say we have a palette but the pixel format say otherwise.
        {
            bool hasPaletteByInternalFormat = ( internalFormat == GVRFMT_PAL_4BIT || internalFormat == GVRFMT_PAL_8BIT );
            bool hasPaletteByPixelFormat = ( palettePixelFormat != GVRPIX_NO_PALETTE );

            if ( hasPaletteByInternalFormat != hasPaletteByPixelFormat )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_STRUCTERR_AMBIGUOUSPAL" );
            }
        }

        // Read advanced things.
        uint8 rasterType = header_rasterType;

        uint32 depth = header_depth;

        // Verify properties.
        {
            uint32 gcDepth = getGCInternalFormatDepth( internalFormat );

            // - depth
            {
                if ( depth != gcDepth )
                {
                    // Since this warning is triggered very often for old textures,
                    // we actually want to shelter this. It ain't important anyway.
                    if ( engineInterface->GetWarningLevel() >= 4 )
                    {
                        engineInterface->PushWarningObject( theTexture, L"GAMECUBE_WARN_INVDEPTH" );
                    }

                    // Fix it.
                    depth = gcDepth;
                }
            }
            // - palette type.
            {
                ePaletteType gcPaletteType = getPaletteTypeFromGCNativeFormat( internalFormat );

                if ( paletteType != gcPaletteType )
                {
                    engineInterface->PushWarningObject( theTexture, L"GAMECUBE_WARN_INVPALTYPE" );

                    paletteType = gcPaletteType;
                }
            }
        }

        // Cache the cluster dimensions, if available.
        uint32 mip_clusterWidth = 0;
        uint32 mip_clusterHeight = 0;
        uint32 mip_clusterCount;

        // This only makes sense if we are a raw format.
        if ( isGVRNativeFormatRawSample( internalFormat ) )
        {
            bool gotClusterDimensions =
                getGVRNativeFormatClusterDimensions(
                    depth,
                    mip_clusterWidth, mip_clusterHeight,
                    mip_clusterCount
                );

            if ( !gotClusterDimensions )
            {
                throw NativeTextureRwObjectsInternalErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_INTERNERR_CLUSTERPROPSFAIL" );
            }
        }

        // Store raster properties in the native texture.
        platformTex->internalFormat = internalFormat;
        platformTex->palettePixelFormat = palettePixelFormat;
        platformTex->autoMipmaps = autoMipmaps;
        platformTex->hasAlpha = ( header_hasAlpha != 0 );
        platformTex->rasterType = rasterType;

        // Store some unknowns.
        platformTex->unk1 = header_unk1;
        platformTex->unk2 = header_unk2;
        platformTex->unk3 = header_unk3;
        platformTex->unk4 = header_unk4;

        // Read the palette.
        if ( palettePixelFormat != GVRPIX_NO_PALETTE )
        {
            uint32 palRasterDepth = getGVRPixelFormatDepth( palettePixelFormat );

            uint32 paletteSize = getGCPaletteSize( internalFormat );

            uint32 palDataSize = getPaletteDataSize( paletteSize, palRasterDepth );

            gcNativeBlock.check_read_ahead( palDataSize );

            void *palData = engineInterface->PixelAllocate( palDataSize );

            try
            {
                gcNativeBlock.read( palData, palDataSize );
            }
            catch( ... )
            {
                engineInterface->PixelFree( palData );

                throw;
            }

            platformTex->palette = palData;
            platformTex->paletteSize = paletteSize;
        }

        // Fetch the image data section size.
        uint32 imageDataSectionSize;
        {
            endian::big_endian <uint32> secSize;

            gcNativeBlock.readStruct( secSize );

            imageDataSectionSize = secSize;
        }

        // Read the image data now.
        uint32 actualImageDataLength = imageDataSectionSize;

        uint32 mipmapCount = 0;

        uint32 maybeMipmapCount = header_mipmapCount;

        mipGenLevelGenerator mipGen( header_width, header_height );

        if ( !mipGen.isValidLevel() )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_STRUCTERR_INVMIPDIMMS" );
        }

        uint32 imageDataLeft = actualImageDataLength;

        bool handledImageDataLengthError = false;

        for ( uint32 n = 0; n < maybeMipmapCount; n++ )
        {
            bool hasEstablishedLevel = true;

            if ( n != 0 )
            {
                hasEstablishedLevel = mipGen.incrementLevel();
            }

            if ( !hasEstablishedLevel )
            {
                break;
            }

            uint32 layerWidth = mipGen.getLevelWidth();
            uint32 layerHeight = mipGen.getLevelHeight();

            // Calculate the surface dimensions of this level.
            uint32 surfWidth, surfHeight;
            {
                if ( internalFormat == GVRFMT_CMP )
                {
                    // This is DXT1 compression.
                    surfWidth = ALIGN_SIZE( layerWidth, 4u * 2u );
                    surfHeight = ALIGN_SIZE( layerHeight, 4u * 2u );
                }
                else
                {
                    // A raw layer is packed into tiles/clusters.
                    // We have to respect that.
                    surfWidth = ALIGN_SIZE( layerWidth, mip_clusterWidth );
                    surfHeight = ALIGN_SIZE( layerHeight, mip_clusterHeight );
                }
            }

            // Calculate the actual mipmap data size.
            uint32 texDataSize;
            {
                if ( internalFormat == GVRFMT_CMP )
                {
                    texDataSize = getDXTRasterDataSize( 1, surfWidth * surfHeight );
                }
                else
                {
                    rasterRowSize texRowSize = getGCRasterDataRowSize( surfWidth, depth );

                    texDataSize = getRasterDataSizeByRowSize( texRowSize, surfHeight );
                }
            }

            // Check whether we can read this image data.
            if ( imageDataLeft < texDataSize )
            {
                // In this case we just halt reading and output a warning.
                engineInterface->PushWarningObject( theTexture, L"GAMECUBE_WARN_IMGSECTDATAOVERFLOW" );

                handledImageDataLengthError = true;

                break;
            }

            // Read the data.
            gcNativeBlock.check_read_ahead( texDataSize );

            void *texels = engineInterface->PixelAllocate( texDataSize );

            try
            {
                gcNativeBlock.read( texels, texDataSize );
            }
            catch( ... )
            {
                engineInterface->PixelFree( texels );

                throw;
            }

            // Update the read count of the image data section.
            imageDataLeft -= texDataSize;

            // Store this layer.
            NativeTextureGC::mipmapLayer mipLayer;
            mipLayer.width = surfWidth;
            mipLayer.height = surfHeight;

            mipLayer.layerWidth = layerWidth;
            mipLayer.layerHeight = layerHeight;

            mipLayer.texels = texels;
            mipLayer.dataSize = texDataSize;

            // Store it.
            platformTex->mipmaps.AddToBack( std::move( mipLayer ) );

            mipmapCount++;
        }

        // Make sure we cannot accept empty textures.
        if ( mipmapCount == 0 )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "Gamecube", AcquireObject( theTexture ), L"GAMECUBE_STRUCTERR_NOMIPMAPS" );
        }

        // If we have not read all the image data, skip to the end.
        if ( imageDataLeft != 0 )
        {
            if ( handledImageDataLengthError == false )
            {
                // We want to warn the user about this.
                engineInterface->PushWarningObject( theTexture, L"GAMECUBE_WARN_IMGSECTBYTESLEFT" );
            }

            gcNativeBlock.skip( imageDataLeft );
        }

        // Fix filtering modes.
        fixFilteringMode( *theTexture, mipmapCount );

        // Done.
    }

    // Read the texture extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

struct gcMipmapManager
{
    NativeTextureGC *nativeTex;

    inline gcMipmapManager( NativeTextureGC *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureGC::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void GetSizeRules( nativeTextureSizeRules& rulesOut )
    {
        bool isDXTCompressed = ( nativeTex->internalFormat == GVRFMT_CMP );

        NativeTextureGC::getSizeRules( isDXTCompressed, rulesOut );
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureGC::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormatOut, eColorOrdering& dstColorOrderOut, uint32& dstDepthOut,
        uint32& dstRowAlignmentOut,
        ePaletteType& dstPaletteTypeOut, void*& dstPaletteDataOut, uint32& dstPaletteSizeOut,
        eCompressionType& dstCompressionTypeOut, bool& hasAlphaOut,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocatedOut
    )
    {
        // Do a basic decode.
        // Not even complete yet.
        eGCNativeTextureFormat internalFormat = nativeTex->internalFormat;
        eGCPixelFormat palettePixelFormat = nativeTex->palettePixelFormat;

        // Decide the destination raster format.
        eRasterFormat dstRasterFormat;
        uint32 dstDepth;
        uint32 dstRowAlignment = 4; // for good measure.
        eColorOrdering dstColorOrder;
        ePaletteType dstPaletteType;        // PALETTE INFO MUST MATCH NATIVE TEXTURE DESCRIPTION.
        eCompressionType dstCompressionType = RWCOMPRESS_NONE;

        bool hasFormat =
            getRecommendedGCNativeTextureRasterFormat(
                internalFormat, palettePixelFormat,
                dstRasterFormat, dstDepth, dstColorOrder, dstPaletteType,
                dstCompressionType
            );

        // If we have not found a good format, decide for the best substitute.
        if ( !hasFormat )
        {
            dstRasterFormat = RASTER_8888;
            dstDepth = 32;
            dstColorOrder = COLOR_BGRA;
            dstPaletteType = PALETTE_NONE;
        }

        uint32 mipWidth = mipLayer.width;
        uint32 mipHeight = mipLayer.height;

        uint32 layerWidth = mipLayer.layerWidth;
        uint32 layerHeight = mipLayer.layerHeight;

        void *srcTexels = mipLayer.texels;
        uint32 srcDataSize = mipLayer.dataSize;

        uint32 dstSurfWidth, dstSurfHeight;
        void *dstTexels;
        uint32 dstDataSize;

        ConvertGCMipmapToRasterFormat(
            engineInterface,
            mipWidth, mipHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
            nativeTex->internalFormat, nativeTex->palettePixelFormat,
            dstPaletteType, nativeTex->palette, nativeTex->paletteSize,
            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder,
            dstCompressionType,
            dstSurfWidth, dstSurfHeight,
            dstTexels, dstDataSize
        );

        // Decode the palette and return it, if present/required.
        void *dstPaletteData = nullptr;
        uint32 dstPaletteSize = 0;

        if ( palettePixelFormat != GVRPIX_NO_PALETTE )
        {
            const void *srcPaletteData = nativeTex->palette;

            dstPaletteSize = nativeTex->paletteSize;

            dstPaletteData = GetGCPaletteData(
                engineInterface,
                srcPaletteData, dstPaletteSize,
                palettePixelFormat,
                dstRasterFormat, dstColorOrder
            );

            // Has to succeed. Error handling should be done inside.
            assert( dstPaletteData != nullptr );
        }

        // Return stuff.
        dstRasterFormatOut = dstRasterFormat;
        dstDepthOut = dstDepth;
        dstRowAlignmentOut = dstRowAlignment;
        dstColorOrderOut = dstColorOrder;
        dstPaletteTypeOut = dstPaletteType;
        dstPaletteDataOut = dstPaletteData;
        dstPaletteSizeOut = dstPaletteSize;
        dstCompressionTypeOut = dstCompressionType;

        widthOut = dstSurfWidth;
        heightOut = dstSurfHeight;
        layerWidthOut = layerWidth;
        layerHeightOut = layerHeight;

        dstTexelsOut = dstTexels;
        dstDataSizeOut = dstDataSize;

        // We have the alpha flag stored, so just return it.
        hasAlphaOut = nativeTex->hasAlpha;

        isNewlyAllocatedOut = true;
        isPaletteNewlyAllocatedOut = true;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureGC::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        uint32 rowAlignment,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        // We put the data that was given to us into native GC format.
        eGCNativeTextureFormat internalFormat = nativeTex->internalFormat;
        eGCPixelFormat palettePixelFormat = nativeTex->palettePixelFormat;

        // We must put the texel data into the appropriate format.
        // If the data is DXT2/3/4/5, we must decompress to full quality.
        // If the native texture is palettized, we must map it to the GC palette.
        // If the native texture is DXT1, we must compress things to DXT1.
        void *transSrcTexels = srcTexels;
        uint32 transDataSize = dataSize;

        uint32 transMipWidth = width;
        uint32 transMipHeight = height;

        eRasterFormat dstRasterFormat = rasterFormat;
        uint32 dstDepth = depth;
        eColorOrdering dstColorOrder = colorOrder;

        eCompressionType dstCompressionType = compressionType;
        ePaletteType dstPaletteType = paletteType;
        uint32 dstPaletteSize = paletteSize;
        {
            eCompressionType reqCompressionType;
            ePaletteType reqPaletteType;
            uint32 reqDepth;

            if ( internalFormat == GVRFMT_CMP )
            {
                reqCompressionType = RWCOMPRESS_DXT1;
                reqPaletteType = PALETTE_NONE;
                reqDepth = 4;
            }
            else
            {
                reqCompressionType = RWCOMPRESS_NONE;

                // Maybe we have to map to palette colors?
                reqPaletteType = getPaletteTypeFromGCNativeFormat( internalFormat );
                reqDepth = getGCInternalFormatDepth( internalFormat );
            }

            // We have to create a special texel representation only if we are "forced" to put into a format.
            bool isForcedToPutIntoFormat =
                ( reqCompressionType != compressionType || reqPaletteType != PALETTE_NONE );

            if ( isForcedToPutIntoFormat )
            {
                // If we palettize, then we must temporarily allocate our palette in framework format.
                void *reqPaletteData = nullptr;
                uint32 reqPaletteSize = 0;

                eRasterFormat reqRasterFormat = rasterFormat;
                eColorOrdering reqColorOrder = colorOrder;

                if ( reqPaletteType != PALETTE_NONE )
                {
                    // We want to decode the palette into the highest quality color possible.
                    reqRasterFormat = RASTER_8888;
                    reqColorOrder = COLOR_RGBA;

                    // Decode the palette.
                    reqPaletteSize = nativeTex->paletteSize;

                    reqPaletteData = GetGCPaletteData(
                        engineInterface,
                        nativeTex->palette, reqPaletteSize,
                        palettePixelFormat,
                        reqRasterFormat, reqColorOrder
                    );
                }

                try
                {
                    // Ask the framework to convert texels for us.
                    bool didChange = ConvertMipmapLayerNative(
                        engineInterface,
                        width, height, layerWidth, layerHeight, srcTexels, dataSize,
                        rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                        reqRasterFormat, reqDepth, rowAlignment, reqColorOrder, reqPaletteType, reqPaletteData, reqPaletteSize, reqCompressionType,
                        false,
                        transMipWidth, transMipHeight,
                        transSrcTexels, transDataSize
                    );

                    // We kind of always want to have changed something, because we detected the need for change.
                    assert( didChange == true );

                    if ( didChange )
                    {
                        // Update the transmission raster properties.
                        dstRasterFormat = reqRasterFormat;
                        dstDepth = reqDepth;
                        dstColorOrder = reqColorOrder;
                        dstCompressionType = reqCompressionType;
                        dstPaletteType = reqPaletteType;
                        dstPaletteSize = reqPaletteSize;
                    }
                }
                catch( ... )
                {
                    // On error, cleanup.
                    if ( reqPaletteData )
                    {
                        engineInterface->PixelFree( reqPaletteData );
                    }

                    throw;
                }

                // Free temporary resources.
                if ( reqPaletteData )
                {
                    engineInterface->PixelFree( reqPaletteData );
                }
            }
        }

        try
        {
            uint32 dstSurfWidth, dstSurfHeight;
            void *dstTexels;
            uint32 dstDataSize;

            TranscodeIntoNativeGCLayer(
                engineInterface,
                transMipWidth, transMipHeight, layerWidth, layerHeight, transSrcTexels, transDataSize,
                dstRasterFormat, dstDepth, rowAlignment, dstColorOrder,
                dstPaletteType, dstPaletteSize, dstCompressionType,
                internalFormat, palettePixelFormat,
                dstSurfWidth, dstSurfHeight,
                dstTexels, dstDataSize
            );

            // Store this as layer.
            mipLayer.width = dstSurfWidth;
            mipLayer.height = dstSurfHeight;

            mipLayer.layerWidth = layerWidth;
            mipLayer.layerHeight = layerHeight;

            mipLayer.texels = dstTexels;
            mipLayer.dataSize = dstDataSize;
        }
        catch( ... )
        {
            // If any error happened, we have no given the texels into the native texture.
            // This means that we have to free texels if we allocated any.
            if ( transSrcTexels != srcTexels )
            {
                engineInterface->PixelFree( transSrcTexels );
            }

            throw;
        }

        // Since we do not directly acquire texels anyway, we must free if we allocated any temporary ones.
        if ( transSrcTexels != srcTexels )
        {
            engineInterface->PixelFree( transSrcTexels );
        }

        // We cannot directly acquire due to swizzling.
        // This could change in the future for certain formats, but hardly worth it.
        hasDirectlyAcquiredOut = false;
    }
};

bool gamecubeNativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

    gcMipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureGC::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool gamecubeNativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, texNativeTypeProvider::acquireFeedback_t& feedbackOut )
{
    NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

    gcMipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureGC::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void gamecubeNativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

    virtualClearMipmaps <NativeTextureGC::mipmapLayer> (
        engineInterface, nativeTex->mipmaps
    );
}

void gamecubeNativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

    uint32 mipmapCount = (uint32)nativeTex->mipmaps.GetCount();

    uint32 baseWidth = 0;
    uint32 baseHeight = 0;

    if ( mipmapCount != 0 )
    {
        const NativeTextureGC::mipmapLayer& mipLayer = nativeTex->mipmaps[ 0 ];

        baseWidth = mipLayer.layerWidth;
        baseHeight = mipLayer.layerHeight;
    }

    infoOut.baseWidth = baseWidth;
    infoOut.baseHeight = baseHeight;
    infoOut.mipmapCount = mipmapCount;
}

void gamecubeNativeTextureTypeProvider::GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const
{
    const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

    rwString <char> formatName( eir::constr_with_alloc::DEFAULT, engineInterface );

    // Set information about the color format.
    eGCNativeTextureFormat internalFormat = nativeTex->internalFormat;

    if ( internalFormat == GVRFMT_LUM_4BIT )
    {
        formatName += "LUM4";
    }
    else if ( internalFormat == GVRFMT_LUM_8BIT )
    {
        formatName += "LUM8";
    }
    else if ( internalFormat == GVRFMT_LUM_4BIT_ALPHA )
    {
        formatName += "LUM_A4";
    }
    else if ( internalFormat == GVRFMT_LUM_8BIT_ALPHA )
    {
        formatName += "LUM_A8";
    }
    else if ( internalFormat == GVRFMT_RGB565 )
    {
        formatName += "565";
    }
    else if ( internalFormat == GVRFMT_RGB5A3 )
    {
        formatName += "555A3";
    }
    else if ( internalFormat == GVRFMT_RGBA8888 )
    {
        formatName += "8888";
    }
    else if ( internalFormat == GVRFMT_PAL_4BIT ||
              internalFormat == GVRFMT_PAL_8BIT )
    {
        eGCPixelFormat palettePixelFormat = nativeTex->palettePixelFormat;

        if ( palettePixelFormat == GVRPIX_LUM_ALPHA )
        {
            formatName += "LUM_A8";
        }
        else if ( palettePixelFormat == GVRPIX_RGB565 )
        {
            formatName += "565";
        }
        else if ( palettePixelFormat == GVRPIX_RGB5A3 )
        {
            formatName += "555A3";
        }
        else
        {
            formatName += "unknown";
        }

        if ( internalFormat == GVRFMT_PAL_4BIT )
        {
            formatName += " PAL4";
        }
        else if ( internalFormat == GVRFMT_PAL_8BIT )
        {
            formatName += " PAL8";
        }
    }
    else if ( internalFormat == GVRFMT_CMP )
    {
        formatName += "DXT1";
    }
    else
    {
        formatName += "unknown";
    }

    size_t formatNameLen = formatName.GetLength();

    if ( buf )
    {
        strncpy( buf, formatName.GetConstString(), formatNameLen );
    }

    lengthOut = formatNameLen;
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_GAMECUBE
