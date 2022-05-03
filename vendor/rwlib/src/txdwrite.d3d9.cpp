/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdwrite.d3d9.cpp
*  PURPOSE:     Direct3D 9 native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9

#include "pixelformat.hxx"

#include "txdread.d3d9.hxx"
#include "txdread.d3d9.layerpipe.hxx"

#include "streamutil.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

eTexNativeCompatibility d3d9NativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility texCompat = RWTEXCOMPAT_NONE;

    BlockProvider texNativeImageBlock( &inputProvider, CHUNK_STRUCT );

    // For now, simply check the platform descriptor.
    // It can either be Direct3D 8 or Direct3D 9.
    uint32 platformDescriptor = texNativeImageBlock.readUInt32();

    if ( platformDescriptor == PLATFORM_D3D9 )
    {
        // Since Wardrum Studios has broken the platform descriptor rules, we can only say "maybe".
        texCompat = RWTEXCOMPAT_MAYBE;
    }

    return texCompat;
}

void d3d9NativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    NativeTextureD3D9 *platformTex = (NativeTextureD3D9*)nativeTex;

    size_t mipmapCount = platformTex->mipmaps.GetCount();

    if ( mipmapCount == 0 )
    {
        throw NativeTextureRwObjectsInternalErrorException( "Direct3D9", AcquireObject( theTexture ), L"D3D9_INTERNERR_WRITEEMPTY" );
    }

    // Make sure the texture has some qualities before it can even be written.
    ePaletteType paletteType = platformTex->paletteType;

    // Luckily, we figured out the problem that textures existed with no valid representation in Direct3D 9.
    // This means that finally each texture has a valid D3DFORMAT field!

	// Struct
	{
		BlockProvider texNativeImageStruct( &outputProvider );

        d3d9::textureMetaHeaderStructGeneric metaHeader;
        metaHeader.platformDescriptor = PLATFORM_D3D9;

        texFormatInfo texFormat;
        texFormat.set( *theTexture );

        metaHeader.texFormat = texFormat;

        // Correctly write the name strings (for safety).
        // Even though we can read those name fields with zero-termination safety,
        // the engines are not guarranteed to do so.
        // Also, print a warning if the name is changed this way.
        writeStringIntoBufferSafe( engineInterface, theTexture->GetName(), metaHeader.name, sizeof( metaHeader.name ), theTexture, L"name" );
        writeStringIntoBufferSafe( engineInterface, theTexture->GetMaskName(), metaHeader.maskName, sizeof( metaHeader.maskName ), theTexture, L"mask name" );

        // Construct raster flags.
        metaHeader.rasterFormat = generateRasterFormatFlags( platformTex->rasterFormat, paletteType, mipmapCount > 1, platformTex->autoMipmaps );

		metaHeader.d3dFormat = platformTex->d3dFormat;
        metaHeader.width = platformTex->mipmaps[ 0 ].layerWidth;
        metaHeader.height = platformTex->mipmaps[ 0 ].layerHeight;
        metaHeader.depth = platformTex->depth;
        metaHeader.mipmapCount = (uint8)mipmapCount;
        metaHeader.rasterType = platformTex->rasterType;
        metaHeader.pad1 = 0;

        metaHeader.hasAlpha = platformTex->hasAlpha;
        metaHeader.isCubeTexture = platformTex->isCubeTexture;
        metaHeader.autoMipMaps = platformTex->autoMipmaps;
        metaHeader.isNotRwCompatible = ( platformTex->isOriginalRWCompatible == false );   // no valid link to RW original types.
        metaHeader.pad2 = 0;

        texNativeImageStruct.write( &metaHeader, sizeof(metaHeader) );

		/* Palette */
		if (paletteType != PALETTE_NONE)
        {
            // Make sure we write as much data as the system expects.
            uint32 reqPalCount = getD3DPaletteCount(paletteType);

            uint32 palItemCount = platformTex->paletteSize;

            // Get the real data size of the palette.
            uint32 palRasterDepth = Bitmap::getRasterFormatDepth(platformTex->rasterFormat);

            uint32 paletteDataSize = getPaletteDataSize( palItemCount, palRasterDepth );

            uint32 palByteWriteCount = writePartialBlockSafe(texNativeImageStruct, platformTex->palette, paletteDataSize, getPaletteDataSize(reqPalCount, palRasterDepth));
        
            assert( palByteWriteCount * 8 / palRasterDepth == reqPalCount );
		}

		/* Texels */
		for (uint32 i = 0; i < mipmapCount; i++)
        {
            const NativeTextureD3D9::mipmapLayer& mipLayer = platformTex->mipmaps[ i ];

			uint32 texDataSize = mipLayer.dataSize;

			texNativeImageStruct.writeUInt32( texDataSize );

            const void *texelData = mipLayer.texels;

			texNativeImageStruct.write( texelData, texDataSize );
		}
	}

	// Extension data.
	engineInterface->SerializeExtensions( theTexture, outputProvider );
}

// Pixel movement functions.
void d3d9NativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    // Cast to our native texture type.
    NativeTextureD3D9 *platformTex = (NativeTextureD3D9*)objMem;

    d3d9FetchPixelDataFromTexture <pixelDataTraversal::mipmapResource> (
        engineInterface,
        platformTex,
        pixelsOut.mipmaps,
        pixelsOut.rasterFormat, pixelsOut.depth, pixelsOut.rowAlignment, pixelsOut.colorOrder,
        pixelsOut.paletteType, pixelsOut.paletteData, pixelsOut.paletteSize, pixelsOut.compressionType,
        pixelsOut.rasterType, pixelsOut.cubeTexture, pixelsOut.autoMipmaps, pixelsOut.hasAlpha,
        pixelsOut.isNewlyAllocated
    );
}

void d3d9NativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    // We want to remove our current texels and put the new ones into us instead.
    // By chance, the runtime makes sure the texture is already empty.
    // So optimize your routine to that.

    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    // We want to simply assign stuff to this native texture.

    bool hasDirectlyAcquired;

    d3d9AcquirePixelDataToTexture <pixelDataTraversal::mipmapResource> (
        engineInterface,
        nativeTex,
        pixelsIn.mipmaps,
        pixelsIn.rasterFormat, pixelsIn.depth, pixelsIn.rowAlignment, pixelsIn.colorOrder,
        pixelsIn.paletteType, pixelsIn.paletteData, pixelsIn.paletteSize, pixelsIn.compressionType,
        pixelsIn.rasterType, pixelsIn.cubeTexture, pixelsIn.autoMipmaps, pixelsIn.hasAlpha,
        hasDirectlyAcquired
    );

    feedbackOut.hasDirectlyAcquired = hasDirectlyAcquired;
}

void d3d9NativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    // Just remove the pixels from us.
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    if ( deallocate )
    {
        // Delete all pixels.
        deleteMipmapLayers( engineInterface, nativeTex->mipmaps );

        // Delete palette.
        if ( nativeTex->paletteType != PALETTE_NONE )
        {
            if ( void *paletteData = nativeTex->palette )
            {
                engineInterface->PixelFree( paletteData );

                nativeTex->palette = nullptr;
            }

            nativeTex->paletteType = PALETTE_NONE;
        }
    }

    // Unset the pixels.
    nativeTex->mipmaps.Clear();

    nativeTex->palette = nullptr;
    nativeTex->paletteType = PALETTE_NONE;
    nativeTex->paletteSize = 0;

    // Reset general properties for cleanliness.
    nativeTex->rasterFormat = RASTER_DEFAULT;
    nativeTex->depth = 0;
    nativeTex->d3dFormat = D3DFMT_UNKNOWN;
    nativeTex->colorComprType = RWCOMPRESS_NONE;
    nativeTex->hasAlpha = false;
    nativeTex->colorOrdering = COLOR_BGRA;
}

struct d3d9MipmapManager
{
    NativeTextureD3D9 *nativeTex;

    inline d3d9MipmapManager( NativeTextureD3D9 *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureD3D9::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void GetSizeRules( nativeTextureSizeRules& rulesOut ) const
    {
        NativeTextureD3D9::getSizeRules( nativeTex->colorComprType, rulesOut );
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureD3D9::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        uint32& dstRowAlignment,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocated
    )
    {
        // If we are a texture that does not directly represent rw compatible types, 
        // we need to have a native format plugin that maps us to original RW types.
        bool isNewlyAllocated = false;

        if ( nativeTex->IsRWCompatible() == false )
        {
            d3dpublic::nativeTextureFormatHandler *formatHandler = nativeTex->anonymousFormatLink;

            if ( !formatHandler )
            {
                throw NativeTextureInvalidConfigurationException( "Direct3D9", L"D3D9_INVALIDCFG_FETCHPIXUNKFMT" );
            }

            // Do stuff.
            uint32 mipWidth = mipLayer.width;
            uint32 mipHeight = mipLayer.height;

            eRasterFormat rasterFormat;
            uint32 depth;
            eColorOrdering colorOrder;

            formatHandler->GetTextureRWFormat( rasterFormat, depth, colorOrder );

            // Calculate the data size.
            rasterRowSize dstRowSize = getD3DRasterDataRowSize( mipWidth, depth );

#ifdef _DEBUG
            assert( dstRowSize.mode == eRasterDataRowMode::ALIGNED );
#endif //_DEBUG

            uint32 texDataSize = getRasterDataSizeByRowSize( dstRowSize, mipHeight );

            // Allocate new texels.
            void *newtexels = engineInterface->PixelAllocate( texDataSize );
            
            try
            {
                formatHandler->ConvertToRW(
                    mipLayer.texels, mipWidth, mipHeight, dstRowSize.byteSize, mipLayer.dataSize,
                    newtexels
                );
            }
            catch( ... )
            {
                engineInterface->PixelFree( newtexels );

                throw;
            }

            // Now return that new stuff.
            widthOut = mipWidth;
            heightOut = mipHeight;

            // Since we are returning a raw raster, plane dimm == layer dimm.
            layerWidthOut = mipWidth;
            layerHeightOut = mipHeight;

            dstRasterFormat = rasterFormat;
            dstColorOrder = colorOrder;
            dstDepth = depth;

            dstPaletteType = PALETTE_NONE;
            dstPaletteData = nullptr;
            dstPaletteSize = 0;

            dstCompressionType = RWCOMPRESS_NONE;

            dstTexelsOut = newtexels;
            dstDataSizeOut = texDataSize;

            isNewlyAllocated = true;
        }
        else
        {
            // We just return stuff.
            widthOut = mipLayer.width;
            heightOut = mipLayer.height;

            layerWidthOut = mipLayer.layerWidth;
            layerHeightOut = mipLayer.layerHeight;
        
            dstRasterFormat = nativeTex->rasterFormat;
            dstColorOrder = nativeTex->colorOrdering;
            dstDepth = nativeTex->depth;

            dstPaletteType = nativeTex->paletteType;
            dstPaletteData = nativeTex->palette;
            dstPaletteSize = nativeTex->paletteSize;

            dstCompressionType = getD3DCompressionType( nativeTex );

            dstTexelsOut = mipLayer.texels;
            dstDataSizeOut = mipLayer.dataSize;
        }

        dstRowAlignment = getD3DTextureDataRowAlignment();

        hasAlpha = nativeTex->hasAlpha;

        isNewlyAllocatedOut = isNewlyAllocated;
        isPaletteNewlyAllocated = false;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureD3D9::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        uint32 rowAlignment,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        if ( nativeTex->IsRWCompatible() == false )
        {
            // If we have a native format plugin, we can ask it to create the encoded pixel data
            // by feeding it orignal RW type data.
            d3dpublic::nativeTextureFormatHandler *formatHandler = nativeTex->anonymousFormatLink;

            if ( !formatHandler )
            {
                throw NativeTextureInvalidConfigurationException( "Direct3D9", L"D3D9_INVALIDCFG_ADDPIXUNKFMT" );
            }

            // We create an encoding that is expected to be raw data.
            uint32 texDataSize = (uint32)formatHandler->GetFormatTextureDataSize( width, height );

            void *newtexels = engineInterface->PixelAllocate( texDataSize );

            try
            {
                // Request the plugin to create data for us.
                formatHandler->ConvertFromRW(
                    width, height, rowAlignment,
                    srcTexels, rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize,
                    newtexels
                );
            }
            catch( ... )
            {
                engineInterface->PixelFree( newtexels );

                throw;
            }

            // Write stuff.
            mipLayer.width = width;
            mipLayer.height = height;
            
            mipLayer.layerWidth = layerWidth;
            mipLayer.layerHeight = layerHeight;

            mipLayer.texels = newtexels;
            mipLayer.dataSize = texDataSize;

            // We always newly allocate to store pixels here, because we want to
            // keep plugin architecture as simple as possible.
            hasDirectlyAcquiredOut = false;
        }
        else
        {
            // Convert to our format.
            bool hasChanged =
                ConvertMipmapLayerNative(
                    engineInterface,
                    width, height, layerWidth, layerHeight, srcTexels, dataSize,
                    rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                    nativeTex->rasterFormat, nativeTex->depth, getD3DTextureDataRowAlignment(), nativeTex->colorOrdering,
                    nativeTex->paletteType, nativeTex->palette, nativeTex->paletteSize,
                    getD3DCompressionType( nativeTex ),
                    false,
                    width, height,
                    srcTexels, dataSize
                );

            // We have no more auto mipmaps.
            nativeTex->autoMipmaps = false;

            // Store the data.
            mipLayer.width = width;
            mipLayer.height = height;

            mipLayer.layerWidth = layerWidth;
            mipLayer.layerHeight = layerHeight;

            mipLayer.texels = srcTexels;
            mipLayer.dataSize = dataSize;

            hasDirectlyAcquiredOut = ( hasChanged == false );
        }
    }
};

bool d3d9NativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    d3d9MipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureD3D9::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool d3d9NativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    d3d9MipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureD3D9::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void d3d9NativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    virtualClearMipmaps <NativeTextureD3D9::mipmapLayer> ( engineInterface, nativeTex->mipmaps );
}

void d3d9NativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    size_t mipmapCount = nativeTex->mipmaps.GetCount();

    infoOut.mipmapCount = (uint32)mipmapCount;

    uint32 baseWidth = 0;
    uint32 baseHeight = 0;

    if ( mipmapCount > 0 )
    {
        baseWidth = nativeTex->mipmaps[ 0 ].layerWidth;
        baseHeight = nativeTex->mipmaps[ 0 ].layerHeight;
    }

    infoOut.baseWidth = baseWidth;
    infoOut.baseHeight = baseHeight;
}

void d3d9NativeTextureTypeProvider::GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const
{
    NativeTextureD3D9 *nativeTexture = (NativeTextureD3D9*)objMem;

    rwString <char> formatString( eir::constr_with_alloc::DEFAULT, engineInterface );

    eCompressionType colorComprType = nativeTexture->colorComprType;

    if ( colorComprType != RWCOMPRESS_NONE )
    {
        if ( colorComprType == RWCOMPRESS_DXT1 )
        {
            formatString = "DXT1";
        }
        else if ( colorComprType == RWCOMPRESS_DXT2 )
        {
            formatString = "DXT2";
        }
        else if ( colorComprType == RWCOMPRESS_DXT3 )
        {
            formatString = "DXT3";
        }
        else if ( colorComprType == RWCOMPRESS_DXT4 )
        {
            formatString = "DXT4";
        }
        else if ( colorComprType == RWCOMPRESS_DXT5 )
        {
            formatString = "DXT5";
        }
        else
        {
            formatString = "unknown";
        }
    }
    else
    {
        // Check whether we have a link to original RW types.
        // Otherwise we should have a native format extension, hopefully.
        if ( nativeTexture->d3dRasterFormatLink == false )
        {
            // Just get the string from the native format.
            d3dpublic::nativeTextureFormatHandler *formatHandler = nativeTexture->anonymousFormatLink;

            if ( formatHandler )
            {
                const char *formatName = formatHandler->GetFormatName();

                if ( formatName )
                {
                    formatString = formatName;
                }
                else
                {
                    formatString = "unspecified";
                }
            }
            else
            {
                formatString = "undefined";
            }
        }
        else
        {
            // We are a default raster.
            // Share functionality here.
            getDefaultRasterFormatString( nativeTexture->rasterFormat, nativeTexture->depth, nativeTexture->paletteType, nativeTexture->colorOrdering, formatString );
        }
    }

    if ( buf )
    {
        strncpy( buf, formatString.GetConstString(), bufLen );
    }

    lengthOut += formatString.GetLength();
}

}

#endif //RWLIB_INCLUDE_NATIVETEX_D3D9