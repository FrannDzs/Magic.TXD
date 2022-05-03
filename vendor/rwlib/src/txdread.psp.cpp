/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.psp.cpp
*  PURPOSE:     PSP native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_PSP

#include "txdread.psp.hxx"

#include "txdread.psp.mem.hxx"

#include "pluginutil.hxx"

namespace rw
{

eTexNativeCompatibility pspNativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility compatOut = RWTEXCOMPAT_NONE;

    // We just check the meta block.
    {
        BlockProvider metaBlock( &inputProvider, CHUNK_STRUCT );

        // Check checksum.
        uint32 checksum = metaBlock.readUInt32();

        if ( checksum == PSP_FOURCC )
        {
            // Only the PSP native texture could have the PSP checksum.
            compatOut = RWTEXCOMPAT_ABSOLUTE;
        }
    }

    return compatOut;
}

void pspNativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    // First deserialize the top meta block.
    {
        BlockProvider metaBlock( &inputProvider, CHUNK_STRUCT );

        // First field is the checksum.
        uint32 checksum = metaBlock.readUInt32();

        if ( checksum != PSP_FOURCC )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "PSP", AcquireObject( theTexture ), L"PSP_STRUCTERR_PLATFORMID" );
        }

        // Just like the PS2 native texture there was supposed to be the filtering mode settings here.
        // Unfortunately, it never made it into production?
        texFormatInfo formatInfo;
        formatInfo.readFromBlock( metaBlock );

        formatInfo.parse( *theTexture, true );
    }

    // Now comes the texture name...
    {
        rwStaticString <char> texName;

        utils::readStringChunkANSI( engineInterface, inputProvider, texName );

        theTexture->SetName( texName.GetConstString() );
    }
    // ... and alpha mask name.
    {
        rwStaticString <char> maskName;

        utils::readStringChunkANSI( engineInterface, inputProvider, maskName );

        theTexture->SetMaskName( maskName.GetConstString() );
    }

    // Create access to the PSP native texture.
    NativeTexturePSP *pspTex = (NativeTexturePSP*)nativeTex;

    // Now comes the graphical data master block.
    // It contains the main image data.
    {
        BlockProvider colorMainBlock( &inputProvider, CHUNK_STRUCT );

        // We need meta information about the graphical data to-be-processed.
        uint32 baseWidth, baseHeight;
        uint32 depth;
        uint32 mipmapCount;
        {
            BlockProvider imageMetaBlock( &colorMainBlock, CHUNK_STRUCT );

            psp::textureMetaDataHeader metaInfo;
            imageMetaBlock.readStruct( metaInfo );

            baseWidth = metaInfo.width;
            baseHeight = metaInfo.height;
            depth = metaInfo.depth;
            mipmapCount = metaInfo.mipmapCount;

            uint32 unknown = metaInfo.unknown;

            if ( unknown != 0 )
            {
                engineInterface->PushWarningObject( theTexture, L"PSP_WARN_UNKFIELDNOTZERO" );
            }

            pspTex->unk = unknown;
        }

        // Decide on the raw format of the texture.
        eMemoryLayoutType rawColorMemoryLayout;

        if ( depth == 4 )
        {
            rawColorMemoryLayout = PSMT4;
        }
        else if ( depth == 8 )
        {
            rawColorMemoryLayout = PSMT8;
        }
        else if ( depth == 16 )
        {
            rawColorMemoryLayout = PSMCT16S;
        }
        else if ( depth == 32 )
        {
            rawColorMemoryLayout = PSMCT32;
        }
        else
        {
            // Not all depths are supported.
            throw NativeTextureRwObjectsStructuralErrorException( "PSP", AcquireObject( theTexture ), L"PSP_STRUCTERR_INVDEPTH" );
        }

        // This native texture format is pretty dumbed down to what the PSP can actually support I think.
        eMemoryLayoutType bitbltEncodingType = getPSPHardwareBITBLTFormat( depth );

        // Determine some parameters based on the meta info.
        ePaletteTypePS2 paletteType;
        eRasterFormatPS2 rasterFormat;

        GetPS2RasterFormatFromMemoryLayoutType( rawColorMemoryLayout, eCLUTMemoryLayoutType::PSMCT32, rasterFormat, paletteType );

        pspTex->bitbltMemoryLayout = bitbltEncodingType;
        pspTex->rawColorMemoryLayout = rawColorMemoryLayout;

        // GPU Data :-)
        {
            BlockProvider gpuDataBlock( &colorMainBlock, CHUNK_STRUCT );

            // There are no more GIF packets, I guess.
            // So things are not complicated.

            // Read the image data of all mipmaps first.
            uint32 mip_index = 0;

            mipGenLevelGenerator mipGen( baseWidth, baseHeight );

            if ( !mipGen.isValidLevel() )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "PSP", AcquireObject( theTexture ), L"PSP_STRUCTERR_INVMIPDIMMS" );
            }

            while ( mip_index < mipmapCount )
            {
                // Advance mipmap dimensions.
                bool establishedLevel = true;

                if ( mip_index != 0 )
                {
                    establishedLevel = mipGen.incrementLevel();
                }

                if ( !establishedLevel )
                {
                    // We abort if the mipmap generation cannot determine any more valid dimensions.
                    break;
                }

                uint32 layerWidth = mipGen.getLevelWidth();
                uint32 layerHeight = mipGen.getLevelHeight();

                // The PSP native texture has broken color buffer storage for swizzled formats
                // because the actual memory space required for PSMCT32 is not honored when writing
                // the texture data to disk.
                // Unfortunately we have to keep the broken behavior.

                // For a matter of fact, we do not handle packed dimensions ever.
                // We just deal with the raw dimensions, which is broken!
                rasterRowSize mipRowSize = getPSPRasterDataRowSize( layerWidth, depth );

                uint32 mipDataSize = getRasterDataSizeByRowSize( mipRowSize, layerHeight );

                gpuDataBlock.check_read_ahead( mipDataSize );

                void *texels = engineInterface->PixelAllocate( mipDataSize );

                try
                {
                    gpuDataBlock.read( texels, mipDataSize );
                }
                catch( ... )
                {
                    engineInterface->PixelFree( texels );

                    throw;
                }

                // Store this new layer.
                NativeTexturePSP::GETexture newLayer;
                newLayer.width = layerWidth;
                newLayer.height = layerHeight;
                newLayer.texels = texels;
                newLayer.dataSize = mipDataSize;

                // Store swizzling property.
                {
                    bool isMipSwizzled = isPSPSwizzlingRequired( layerWidth, layerHeight, depth );

                    newLayer.isSwizzled = isMipSwizzled;
                }

                pspTex->mipmaps.AddToBack( std::move( newLayer ) );

                mip_index++;
            }

            if ( mip_index == 0 )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "PSP", AcquireObject( theTexture ), L"PSP_STRUCTERR_EMPTY" );
            }

            // After the mipmap data, comes the palette, so let's read it!
            void *paletteData = nullptr;
            uint32 paletteSize = 0;

            if ( paletteType != ePaletteTypePS2::NONE )
            {
                paletteSize = getPaletteItemCount( GetGenericPaletteTypeFromPS2( paletteType ) );

                // The PSP native texture does only support 32bit RGBA palette entries (RASTER_8888 RGBA).
                uint32 palRasterDepth = Bitmap::getRasterFormatDepth( GetGenericRasterFormatFromPS2( rasterFormat ) );

                uint32 palDataSize = getPaletteDataSize( paletteSize, palRasterDepth );

                gpuDataBlock.check_read_ahead( palDataSize );

                paletteData = engineInterface->PixelAllocate( palDataSize );

                try
                {
                    gpuDataBlock.read( paletteData, palDataSize );
                }
                catch( ... )
                {
                    engineInterface->PixelFree( paletteData );

                    throw;
                }
            }

            // Store the palette information into the texture.
            pspTex->palette = paletteData;

            // Fix the filtering settings.
            fixFilteringMode( *theTexture, mip_index );

            // Sometimes there is strange padding added to the GPU data block.
            // We want to skip that and warn the user.
            {
                int64 currentBlockSeek = gpuDataBlock.tell();

                int64 leftToEnd = gpuDataBlock.getBlockLength() - currentBlockSeek;

                if ( leftToEnd > 0 )
                {
                    if ( engineInterface->GetWarningLevel() >= 3 )
                    {
                        engineInterface->PushWarningObject( theTexture, L"PSP_WARN_METADATASKIP" );
                    }

                    gpuDataBlock.skip( (size_t)leftToEnd );
                }
            }
        }
    }

    // Read extension info.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

static optional_struct_space <PluginDependantStructRegister <pspNativeTextureTypeProvider, RwInterfaceFactory_t>> pspNativeTextureTypeRegister;

void registerPSPNativeTextureType( void )
{
    pspNativeTextureTypeRegister.Construct( engineFactory );
}

void unregisterPSPNativeTextureType( void )
{
    pspNativeTextureTypeRegister.Destroy();
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_PSP
