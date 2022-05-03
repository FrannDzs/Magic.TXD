/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.d3d8.layerpipe.hxx
*  PURPOSE:     Direct3D 8 native texture pixel pipeline header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_D3D8_NATIVETEX_LAYER_PIPELINE_
#define _RENDERWARE_D3D8_NATIVETEX_LAYER_PIPELINE_

// RenderWare Direct3D8 native texture layer pixel setting and getting algorithms.
// Those are exported for conveniance and standardization purposes.

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D8

#include "txdread.d3d8.hxx"

namespace rw
{
  
template <typename defaultedMipmapLayerType, typename mipmapVectorType>
AINLINE void d3d8FetchPixelDataFromTexture(
    Interface *engineInterface,
    NativeTextureD3D8 *platformTex,
    mipmapVectorType& mipmaps,
    eRasterFormat& rasterFormatOut, uint32& depthOut, uint32& rowAlignmentOut, eColorOrdering& colorOrderOut,
    ePaletteType& paletteTypeOut, void*& paletteDataOut, uint32& paletteSizeOut, eCompressionType& compressionTypeOut,
    uint8& rasterTypeOut, bool& autoMipmapsOut, bool& hasAlphaOut,
    bool& isNewlyAllocatedOut
)
{
    // The pixel capabilities system has been mainly designed around PC texture optimization.
    // This means that we should be able to directly copy the Direct3D surface data into pixelsOut.
    // If not, we need to adjust, make a new library version.

    // We need to decide how to traverse palette runtime optimization data.

    // Determine the compression type.
    eCompressionType rwCompressionType = getD3DCompressionType( platformTex );

    // Put ourselves into the pixelsOut struct!
    rasterFormatOut = platformTex->rasterFormat;
    depthOut = platformTex->depth;
    rowAlignmentOut = getD3DTextureDataRowAlignment();
    colorOrderOut = platformTex->colorOrdering;
    paletteTypeOut = platformTex->paletteType;
    paletteDataOut = platformTex->palette;
    paletteSizeOut = platformTex->paletteSize;
    compressionTypeOut = rwCompressionType;
    hasAlphaOut = platformTex->hasAlpha;

    autoMipmapsOut = platformTex->autoMipmaps;
    rasterTypeOut = platformTex->rasterType;

    // Now, the texels.
    size_t mipmapCount = platformTex->mipmaps.GetCount();

    mipmaps.Resize( mipmapCount );

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        const NativeTextureD3D8::mipmapLayer& srcLayer = platformTex->mipmaps[ n ];

        defaultedMipmapLayerType newLayer;

        newLayer.width = srcLayer.width;
        newLayer.height = srcLayer.height;
        newLayer.layerWidth = srcLayer.layerWidth;
        newLayer.layerHeight = srcLayer.layerHeight;

        newLayer.texels = srcLayer.texels;
        newLayer.dataSize = srcLayer.dataSize;

        // Put this layer.
        mipmaps[ n ] = newLayer;
    }

    // We never allocate new texels, actually.
    isNewlyAllocatedOut = false;
}

template <typename defaultedMipmapLayerType, typename mipmapVectorType>
AINLINE void d3d8AcquirePixelDataToTexture(
    Interface *engineInterface,
    NativeTextureD3D8 *nativeTex,
    mipmapVectorType& mipmaps,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder,
    ePaletteType srcPaletteType, void *srcPaletteData, uint32 srcPaletteSize, eCompressionType rwCompressionType,
    uint32 texRasterType, bool texAutoMipmaps, bool texHasAlpha,
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
    uint32 dxtType;

    // Check whether we can directly acquire or have to allocate a new copy.
    bool canDirectlyAcquire;

    if ( rwCompressionType == RWCOMPRESS_NONE )
    {
        // Actually, before we can acquire texels, we MUST make sure they are in
        // a compatible format. If they are not, then we will most likely allocate
        // new pixel information, instead in a compatible format.

        bool fixIncompatibleRasters = engineInterface->GetFixIncompatibleRasters();

        // Make sure this texture is writable.
        // If we are on D3D, we have to avoid typical configurations that may come from
        // other hardware.
        d3d8::convertCompatibleRasterFormat(
            ( fixIncompatibleRasters == true ),
            dstRasterFormat, dstColorOrder, dstDepth, dstPaletteType
        );
 
        dxtType = 0;

        canDirectlyAcquire =
            !doesPixelDataOrPaletteDataNeedConversion(
                mipmaps,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteSize, RWCOMPRESS_NONE,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstPaletteSize, RWCOMPRESS_NONE
            );
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT1 )
    {
        dxtType = 1;

        dstRasterFormat = ( texHasAlpha ) ? ( RASTER_1555 ) : ( RASTER_565 );

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT2 )
    {
        dxtType = 2;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT3 )
    {
        dxtType = 3;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT4 )
    {
        dxtType = 4;
        
        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT5 )
    {
        dxtType = 5;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else
    {
        throw NativeTextureInternalErrorException( "Direct3D8", L"D3D8_INTERNERR_UNKFRAMEWORKCOMPR" );
    }

    // We have to verify some rules.
    {
        nativeTextureSizeRules sizeRules;
       
        NativeTextureD3D8::getSizeRules( dxtType, sizeRules );

        if ( !sizeRules.verifyMipmaps( mipmaps ) )
        {
            throw NativeTextureInvalidConfigurationException( "Direct3D8", L"D3D8_INVALIDCFG_MIPDIMMS" );
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
    nativeTex->hasAlpha = texHasAlpha;

    size_t mipmapCount = mipmaps.GetCount();

    // Properly set the automipmaps field.
    bool autoMipmaps = texAutoMipmaps;

    if ( mipmapCount > 1 )
    {
        autoMipmaps = false;
    }

    nativeTex->autoMipmaps = autoMipmaps;
    nativeTex->rasterType = texRasterType;

    nativeTex->dxtCompression = dxtType;

    // Apply the pixel data.
    nativeTex->mipmaps.Resize( mipmapCount );

    for ( size_t n = 0; n < mipmapCount; n++ )
    {
        const defaultedMipmapLayerType& srcLayer = mipmaps[ n ];

        // Get the mipmap properties on the stack.
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

        NativeTextureD3D8::mipmapLayer newLayer;

        newLayer.width = mipWidth;
        newLayer.height = mipHeight;
        newLayer.layerWidth = layerWidth;
        newLayer.layerHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;

        // Store the layer.
        nativeTex->mipmaps[ n ] = newLayer;
    }

    // For now, we can always directly acquire pixels.
    hasDirectlyAcquiredOut = canDirectlyAcquire;
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_D3D8

#endif //_RENDERWARE_D3D8_NATIVETEX_LAYER_PIPELINE_