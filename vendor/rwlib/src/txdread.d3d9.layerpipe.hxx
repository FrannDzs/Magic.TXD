/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.d3d9.layerpipe.hxx
*  PURPOSE:     Direct3D 9 native texture pixel layer pipeline
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_D3D9_NATIVETEX_LAYER_PIPELINE_
#define _RENDERWARE_D3D9_NATIVETEX_LAYER_PIPELINE_

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9

#include "txdread.d3d9.hxx"

namespace rw
{

template <typename defaultedMipmapLayerType, typename mipmapVectorType>
AINLINE void d3d9FetchPixelDataFromTexture(
    Interface *engineInterface,
    NativeTextureD3D9 *platformTex,
    mipmapVectorType& mipmapsOut,
    eRasterFormat& rasterFormatOut, uint32& depthOut, uint32& rowAlignmentOut, eColorOrdering& colorOrderOut,
    ePaletteType& paletteTypeOut, void*& paletteDataOut, uint32& paletteSizeOut, eCompressionType& compressionTypeOut,
    uint8& rasterTypeOut, bool& cubeTextureOut, bool& autoMipmapsOut, bool& hasAlphaOut,
    bool& isNewlyAllocatedOut
)
{
    // The pixel capabilities system has been mainly designed around PC texture optimization.
    // This means that we should be able to directly copy the Direct3D surface data into pixelsOut.
    // If not, we need to adjust, make a new library version.

    // We need to decide how to traverse palette runtime optimization data.

    // Determine the compression type.
    eCompressionType rwCompressionType = getD3DCompressionType( platformTex );

    // We always have a D3DFORMAT. If we have no link to original RW types, then we cannot be pushed to RW.
    // An exception case is the compression.
    d3dpublic::nativeTextureFormatHandler *useFormatHandler = nullptr;

    bool isNewlyAllocated = false;

    eRasterFormat dstRasterFormat = platformTex->rasterFormat;
    uint32 dstDepth = platformTex->depth;
    eColorOrdering dstColorOrder = platformTex->colorOrdering;
    ePaletteType dstPaletteType = platformTex->paletteType;
    void *dstPaletteData = platformTex->palette;
    uint32 dstPaletteSize = platformTex->paletteSize;

    if ( platformTex->d3dRasterFormatLink == false && rwCompressionType == RWCOMPRESS_NONE )
    {
        // We could have a native format handler that converts us into an appropriate format.
        // This would be amazing!
        useFormatHandler = platformTex->anonymousFormatLink;

        if ( useFormatHandler )
        {
            // We kinda travel in another format.
            useFormatHandler->GetTextureRWFormat( dstRasterFormat, dstDepth, dstColorOrder );

            // No more palette.
            dstPaletteType = PALETTE_NONE;
            dstPaletteData = nullptr;
            dstPaletteSize = 0;

            // We always newly allocate if there is a native format plugin.
            // For simplicity sake.
            isNewlyAllocated = true;
        }
        else
        {
            throw NativeTextureInvalidConfigurationException( "Direct3D9", L"D3D9_INVALIDCFG_NORASTERFMT" );
        }
    }

    // Put ourselves into the pixelsOut struct!
    rasterFormatOut = dstRasterFormat;
    depthOut = dstDepth;
    rowAlignmentOut = getD3DTextureDataRowAlignment();
    colorOrderOut = dstColorOrder;
    paletteTypeOut = dstPaletteType;
    paletteDataOut = dstPaletteData;
    paletteSizeOut = dstPaletteSize;
    compressionTypeOut = rwCompressionType;
    hasAlphaOut = platformTex->hasAlpha;

    autoMipmapsOut = platformTex->autoMipmaps;
    cubeTextureOut = platformTex->isCubeTexture;
    rasterTypeOut = platformTex->rasterType;

    // Now, the texels.
    size_t mipmapCount = platformTex->mipmaps.GetCount();

    mipmapsOut.Resize( mipmapCount );

    for ( size_t n = 0; n < mipmapCount; n++ )
    {
        const NativeTextureD3D9::mipmapLayer& srcLayer = platformTex->mipmaps[ n ];

        uint32 mipWidth = srcLayer.width;
        uint32 mipHeight = srcLayer.height;
        uint32 layerWidth = srcLayer.layerWidth;
        uint32 layerHeight = srcLayer.layerHeight;

        void *srcTexels = srcLayer.texels;
        uint32 texelDataSize = srcLayer.dataSize;

        if ( useFormatHandler != nullptr )
        {
            // We will always convert to native RW original types, which are raw pixels.
            // This means that layer dimensions match plane dimensions.
            mipWidth = layerWidth;
            mipHeight = layerHeight;

            // Create a new storage pointer.
            rasterRowSize dstRowSize = getD3DRasterDataRowSize( mipWidth, dstDepth );

#ifdef _DEBUG
            assert( dstRowSize.mode == eRasterDataRowMode::ALIGNED );
#endif //_DEBUG

            texelDataSize = getRasterDataSizeByRowSize( dstRowSize, mipHeight );

            void *newtexels = engineInterface->PixelAllocate( texelDataSize );

            try
            {
                // Ask the format handler to convert it to something useful.
                useFormatHandler->ConvertToRW(
                    srcTexels, mipWidth, mipHeight, dstRowSize.byteSize, texelDataSize,
                    newtexels
                );
            }
            catch( ... )
            {
                engineInterface->PixelFree( newtexels );

                throw;
            }

            srcTexels = newtexels;
        }

        defaultedMipmapLayerType newLayer;
        
        newLayer.width = mipWidth;
        newLayer.height = mipHeight;
        newLayer.layerWidth = layerWidth;
        newLayer.layerHeight = layerHeight;

        newLayer.texels = srcTexels;
        newLayer.dataSize = texelDataSize;

        // Put this layer.
        mipmapsOut[ n ] = newLayer;
    }

    // We never allocate new texels, actually.
    // Well, we do if we own a complicated D3DFORMAT.
    isNewlyAllocatedOut = isNewlyAllocated;
}

template <typename defaultedMipmapLayerType, typename mipmapVectorType>
AINLINE void d3d9AcquirePixelDataToTexture(
    Interface *engineInterface,
    NativeTextureD3D9 *nativeTex,
    mipmapVectorType& mipmaps,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder,
    ePaletteType srcPaletteType, void *srcPaletteData, uint32 srcPaletteSize, eCompressionType rwCompressionType,
    uint8 texRasterType, bool texCubeTexture, bool texAutoMipmaps, bool texHasAlpha,
    bool& hasDirectlyAcquiredOut
)
{
    // We need to ensure that the pixels we set to us are compatible.
    eRasterFormat dstRasterFormat = srcRasterFormat;
    uint32 dstDepth = srcDepth;
    uint32 dstRowAlignment = getD3DTextureDataRowAlignment();
    eColorOrdering dstColorOrder = srcColorOrder;

    ePaletteType dstPaletteType = srcPaletteType;
    uint32 dstPaletteSize = srcPaletteSize;

    // Determine the compression type.
    bool hasAlpha = texHasAlpha;

    D3DFORMAT d3dTargetFormat = D3DFMT_UNKNOWN;

    // Check whether we can directly acquire or have to allocate a new copy.
    bool canDirectlyAcquire;

    bool hasRasterFormatLink = false;

    bool hasOriginalRWCompat = false;

    if ( rwCompressionType == RWCOMPRESS_NONE )
    {
        // Actually, before we can acquire texels, we MUST make sure they are in
        // a compatible format. If they are not, then we will most likely allocate
        // new pixel information, instead in a compatible format. The same has to be
        // made for the XBOX implementation.

        bool desireWorkingFormat = engineInterface->GetFixIncompatibleRasters();

        // Make sure this texture is writable.
        // If we are on D3D, we have to avoid typical configurations that may come from
        // other hardware.
        d3d9::convertCompatibleRasterFormat(
            desireWorkingFormat,
            dstRasterFormat, dstColorOrder, dstDepth, dstPaletteType, d3dTargetFormat
        );

        canDirectlyAcquire =
            !doesPixelDataOrPaletteDataNeedConversion(
                mipmaps,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteSize, RWCOMPRESS_NONE,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstPaletteSize, RWCOMPRESS_NONE
            );

        // If we are uncompressed, then we know how to deal with the texture data.
        // Hence we have a d3dRasterFormatLink!
        hasRasterFormatLink = true;

        // We could have original RW compatibility here.
        // This is of course only possible if we are not compressed/are a raw raster.
        hasOriginalRWCompat =
            isRasterFormatOriginalRWCompatible(
                dstRasterFormat, dstColorOrder, dstDepth, dstPaletteType
            );
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT1 )
    {
        // TODO: do we properly handle DXT1 with alpha...?

        d3dTargetFormat = D3DFMT_DXT1;

        dstRasterFormat = ( hasAlpha ) ? ( RASTER_1555 ) : ( RASTER_565 );

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT2 )
    {
        d3dTargetFormat = D3DFMT_DXT2;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT3 )
    {
        d3dTargetFormat = D3DFMT_DXT3;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT4 )
    {
        d3dTargetFormat = D3DFMT_DXT4;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT5 )
    {
        d3dTargetFormat = D3DFMT_DXT5;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else
    {
        throw NativeTextureInternalErrorException( "Direct3D9", L"D3D9_INTERNERR_UNKCOMPR" );
    }

    // Verify mipmap dimension rules.
    {
        nativeTextureSizeRules sizeRules;

        NativeTextureD3D9::getSizeRules( rwCompressionType, sizeRules );

        if ( !sizeRules.verifyMipmaps( mipmaps ) )
        {
            throw NativeTextureInvalidConfigurationException( "Direct3D9", L"D3D9_INVALIDCFG_MIPDIMMS" );
        }
    }

    // If we have a palette, we must convert it aswell.
    void *dstPaletteData = srcPaletteData;

    if ( canDirectlyAcquire == false )
    {
        if ( srcPaletteType != PALETTE_NONE )
        {
            // Allocate a new palette.
            CopyConvertPaletteData(
                engineInterface,
                srcPaletteData,
                srcPaletteSize, dstPaletteSize,
                srcRasterFormat, srcColorOrder,
                dstRasterFormat, dstColorOrder,
                dstPaletteData
            );
        }
    }

    // Update our own texels.
    nativeTex->rasterFormat = dstRasterFormat;
    nativeTex->depth = dstDepth;
    nativeTex->palette = dstPaletteData;
    nativeTex->paletteSize = dstPaletteSize;
    nativeTex->paletteType = dstPaletteType;
    nativeTex->colorOrdering = dstColorOrder;
    nativeTex->hasAlpha = hasAlpha;

    size_t mipmapCount = mipmaps.GetCount();

    // Properly set the automipmaps field.
    bool autoMipmaps = texAutoMipmaps;

    if ( mipmapCount > 1 )
    {
        autoMipmaps = false;
    }

    nativeTex->autoMipmaps = autoMipmaps;
    nativeTex->isCubeTexture = texCubeTexture;
    nativeTex->rasterType = texRasterType;

    nativeTex->colorComprType = rwCompressionType;

    // Apply the pixel data.
    nativeTex->mipmaps.Resize( mipmapCount );

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        const defaultedMipmapLayerType& srcLayer = mipmaps[ n ];

        // Get the mipmap properties on the stac.
        uint32 mipWidth = srcLayer.width;
        uint32 mipHeight = srcLayer.height;

        uint32 layerWidth = srcLayer.layerWidth;
        uint32 layerHeight = srcLayer.layerHeight;

        void *srcTexels = srcLayer.texels;
        uint32 srcDataSize = srcLayer.dataSize;

        void *dstTexels = srcTexels;
        uint32 dstDataSize = srcDataSize;

        // If we cannot directly acquire, allocate a new copy and convert.
        if ( canDirectlyAcquire == false )
        {
            assert( rwCompressionType == RWCOMPRESS_NONE );

            // Call the core's helper function.
            CopyTransformRawMipmapLayer(
                engineInterface,
                mipWidth, mipHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType,
                dstTexels, dstDataSize
            );
        }

        NativeTextureD3D9::mipmapLayer newLayer;

        newLayer.width = mipWidth;
        newLayer.height = mipHeight;
        newLayer.layerWidth = layerWidth;
        newLayer.layerHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;

        // Store the layer.
        nativeTex->mipmaps[ n ] = newLayer;
    }

    // We need to set the Direct3D9 format field.
    nativeTex->d3dFormat = d3dTargetFormat;

    // Store whether we can directly deal with the data we just got.
    nativeTex->d3dRasterFormatLink = hasRasterFormatLink;

    // Store whether the pixels have a meaning in RW3 terms.
    nativeTex->isOriginalRWCompatible = hasOriginalRWCompat;

    // We cannot create a texture using an anonymous raster link this way.
    nativeTex->anonymousFormatLink = nullptr;

    // For now, we can always directly acquire pixels.
    hasDirectlyAcquiredOut = canDirectlyAcquire;
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_D3D9

#endif //_RENDERWARE_D3D9_NATIVETEX_LAYER_PIPELINE_