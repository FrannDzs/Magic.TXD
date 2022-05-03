/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdwrite.d3d8.cpp
*  PURPOSE:     Direct3D 8 native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D8

#include "pixelformat.hxx"

#include "txdread.d3d8.hxx"
#include "txdread.d3d8.layerpipe.hxx"

#include "streamutil.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

eTexNativeCompatibility d3d8NativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility texCompat = RWTEXCOMPAT_NONE;

    BlockProvider texNativeImageBlock( &inputProvider, CHUNK_STRUCT );

    // For now, simply check the platform descriptor.
    // It can either be Direct3D 8 or Direct3D 9.
    uint32 platformDescriptor = texNativeImageBlock.readUInt32();

    if ( platformDescriptor == PLATFORM_D3D8 )
    {
        // This is a sure guess.
        texCompat = RWTEXCOMPAT_ABSOLUTE;
    }

    return texCompat;
}

void d3d8NativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    NativeTextureD3D8 *platformTex = (NativeTextureD3D8*)nativeTex;

    size_t mipmapCount = platformTex->mipmaps.GetCount();

    if ( mipmapCount == 0 )
    {
        throw NativeTextureRwObjectsInternalErrorException( "Direct3D8", AcquireObject( theTexture ), L"D3D8_INTERNERR_WRITEEMPTY" );
    }

    // Make sure the texture has some qualities before it can even be written.
    ePaletteType paletteType = platformTex->paletteType;

    uint32 compressionType = platformTex->dxtCompression;

    // Check the color order if we are not compressed.
    if ( compressionType == 0 )
    {
        eColorOrdering requiredColorOrder = COLOR_BGRA;

        if ( paletteType != PALETTE_NONE )
        {
            requiredColorOrder = COLOR_RGBA;
        }

        if ( platformTex->colorOrdering != requiredColorOrder )
        {
            throw NativeTextureRwObjectsInternalErrorException( "Direct3D8", AcquireObject( theTexture ), L"D3D8_INTERNERR_WRITEINVCOLORORDER" );
        }
    }

	// Struct
	{
		BlockProvider texNativeImageStruct( &outputProvider );

        d3d8::textureMetaHeaderStructGeneric metaHeader;
        metaHeader.platformDescriptor = PLATFORM_D3D8;

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

		metaHeader.hasAlpha = platformTex->hasAlpha;
        metaHeader.width = platformTex->mipmaps[ 0 ].layerWidth;
        metaHeader.height = platformTex->mipmaps[ 0 ].layerHeight;
        metaHeader.depth = platformTex->depth;
        metaHeader.mipmapCount = (uint8)mipmapCount;
        metaHeader.rasterType = platformTex->rasterType;
        metaHeader.pad1 = 0;
        metaHeader.dxtCompression = compressionType;

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
		for (size_t i = 0; i < mipmapCount; i++)
        {
            const NativeTextureD3D8::mipmapLayer& mipLayer = platformTex->mipmaps[ i ];

			uint32 texDataSize = mipLayer.dataSize;

			texNativeImageStruct.writeUInt32( texDataSize );

            const void *texelData = mipLayer.texels;

			texNativeImageStruct.write( texelData, texDataSize );
		}
	}

	// Extension
	engineInterface->SerializeExtensions( theTexture, outputProvider );
}

// Pixel movement functions.
void d3d8NativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    // Cast to our native texture type.
    NativeTextureD3D8 *platformTex = (NativeTextureD3D8*)objMem;

    d3d8FetchPixelDataFromTexture <pixelDataTraversal::mipmapResource> (
        engineInterface,
        platformTex,
        pixelsOut.mipmaps,
        pixelsOut.rasterFormat, pixelsOut.depth, pixelsOut.rowAlignment, pixelsOut.colorOrder,
        pixelsOut.paletteType, pixelsOut.paletteData, pixelsOut.paletteSize, pixelsOut.compressionType,
        pixelsOut.rasterType, pixelsOut.autoMipmaps, pixelsOut.hasAlpha,
        pixelsOut.isNewlyAllocated
    );

    pixelsOut.cubeTexture = false;  // D3D8 does not support cubemaps.
}

void d3d8NativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    // We want to remove our current texels and put the new ones into us instead.
    // By chance, the runtime makes sure the texture is already empty.
    // So optimize your routine to that.

    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

    bool hasDirectlyAcquired;

    d3d8AcquirePixelDataToTexture <pixelDataTraversal::mipmapResource> (
        engineInterface,
        nativeTex,
        pixelsIn.mipmaps,
        pixelsIn.rasterFormat, pixelsIn.depth, pixelsIn.rowAlignment, pixelsIn.colorOrder,
        pixelsIn.paletteType, pixelsIn.paletteData, pixelsIn.paletteSize, pixelsIn.compressionType,
        pixelsIn.rasterType, pixelsIn.autoMipmaps, pixelsIn.hasAlpha,
        hasDirectlyAcquired
    );

    feedbackOut.hasDirectlyAcquired = hasDirectlyAcquired;
}

void d3d8NativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    // Just remove the pixels from us.
    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

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
    nativeTex->dxtCompression = 0;
    nativeTex->hasAlpha = false;
    nativeTex->colorOrdering = COLOR_BGRA;
}

struct d3d8MipmapManager
{
    NativeTextureD3D8 *nativeTex;

    inline d3d8MipmapManager( NativeTextureD3D8 *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureD3D8::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void GetSizeRules( nativeTextureSizeRules& rulesOut ) const
    {
        NativeTextureD3D8::getSizeRules( nativeTex->dxtCompression, rulesOut );
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureD3D8::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        uint32& dstRowAlignment,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocated
    )
    {
        // We just return stuff.
        widthOut = mipLayer.width;
        heightOut = mipLayer.height;

        layerWidthOut = mipLayer.layerWidth;
        layerHeightOut = mipLayer.layerHeight;
        
        dstRasterFormat = nativeTex->rasterFormat;
        dstColorOrder = nativeTex->colorOrdering;
        dstDepth = nativeTex->depth;
        dstRowAlignment = getD3DTextureDataRowAlignment();

        dstPaletteType = nativeTex->paletteType;
        dstPaletteData = nativeTex->palette;
        dstPaletteSize = nativeTex->paletteSize;

        dstCompressionType = getD3DCompressionType( nativeTex );

        hasAlpha = nativeTex->hasAlpha;

        dstTexelsOut = mipLayer.texels;
        dstDataSizeOut = mipLayer.dataSize;

        isNewlyAllocatedOut = false;
        isPaletteNewlyAllocated = false;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureD3D8::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        uint32 rowAlignment,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
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
};

bool d3d8NativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

    d3d8MipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureD3D8::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool d3d8NativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

    d3d8MipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureD3D8::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void d3d8NativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

    virtualClearMipmaps <NativeTextureD3D8::mipmapLayer> ( engineInterface, nativeTex->mipmaps );
}

void d3d8NativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

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

void d3d8NativeTextureTypeProvider::GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const
{
    NativeTextureD3D8 *nativeTexture = (NativeTextureD3D8*)objMem;

    rwString <char> formatString( eir::constr_with_alloc::DEFAULT, engineInterface );

    int dxtType = nativeTexture->dxtCompression;

    if ( dxtType != 0 )
    {
        if ( dxtType == 1 )
        {
            formatString = "DXT1";
        }
        else if ( dxtType == 2 )
        {
            formatString = "DXT2";
        }
        else if ( dxtType == 3 )
        {
            formatString = "DXT3";
        }
        else if ( dxtType == 4 )
        {
            formatString = "DXT4";
        }
        else if ( dxtType == 5 )
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
        // We are a default raster.
        // Share functionality here.
        getDefaultRasterFormatString( nativeTexture->rasterFormat, nativeTexture->depth, nativeTexture->paletteType, nativeTexture->colorOrdering, formatString );
    }

    if ( buf )
    {
        strncpy( buf, formatString.GetConstString(), bufLen );
    }

    lengthOut += formatString.GetLength();
}

}

#endif //RWLIB_INCLUDE_NATIVETEX_D3D8