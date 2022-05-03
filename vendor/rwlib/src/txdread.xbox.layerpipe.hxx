/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.xbox.layerpipe.hxx
*  PURPOSE:     XBOX native texture internal pixel layer pipeline header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_XBOX_NATIVETEX_LAYER_PIPELINE_
#define _RENDERWARE_XBOX_NATIVETEX_LAYER_PIPELINE_

// RenderWare XBOX texel general routines for setting up a texture from framework color data (multi-level).
// You do not want to rewrite very complicated routines that optimize many things, etc.
// Use these routines instead of the virtual-layer-access to optimize away things like mipmap-vector allocation
// and virtual-interface indirection.

#ifdef RWLIB_INCLUDE_NATIVETEX_XBOX

#include "txdread.xbox.hxx"

namespace rw
{

inline void copyPaletteData(
    Interface *engineInterface,
    ePaletteType srcPaletteType, void *srcPaletteData, uint32 srcPaletteSize, eRasterFormat srcRasterFormat,
    bool isNewlyAllocated,
    ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize
)
{
    dstPaletteType = srcPaletteType;

    if ( dstPaletteType != PALETTE_NONE )
    {
        dstPaletteSize = srcPaletteSize;

        if ( isNewlyAllocated )
        {
            // Create a new copy.
            uint32 palRasterDepth = Bitmap::getRasterFormatDepth( srcRasterFormat );

            uint32 palDataSize = getPaletteDataSize( dstPaletteSize, palRasterDepth );

            dstPaletteData = engineInterface->PixelAllocate( palDataSize );

            memcpy( dstPaletteData, srcPaletteData, palDataSize );
        }
        else
        {
            // Just move it over.
            dstPaletteData = srcPaletteData;
        }
    }
}

template <typename defaultedMipmapLayerType, typename mipmapVectorType>
inline void xboxFetchPixelDataFromTexture(
    Interface *engineInterface,
    NativeTextureXBOX *platformTex,
    mipmapVectorType& mipmapsOut,
    eRasterFormat& rasterFormatOut, uint32& depthOut, uint32& rowAlignmentOut, eColorOrdering& colorOrderOut,
    ePaletteType& paletteTypeOut, void*& paletteDataOut, uint32& paletteSizeOut, eCompressionType& compressionTypeOut,
    uint8& rasterTypeOut, bool& hasAlphaOut, bool& autoMipmapsOut,
    bool& isNewlyAllocatedOut
)
{
    // We need to find out how to store the texels into the traversal containers.
    // If we store compressed image data, we can give the data directly to the runtime.
    uint32 xboxCompressionType = platformTex->dxtCompression;

    eCompressionType rwCompressionType;
    {
        bool gotCompressionType = getDXTCompressionTypeFromXBOX( xboxCompressionType, rwCompressionType );

        if ( !gotCompressionType )
        {
            throw NativeTextureInternalErrorException( "XBOX", L"XBOX_INTERNERR_UNKCOMPRTYPE" );
        }
    }

    uint32 srcDepth = platformTex->depth;

    ePaletteType srcPaletteType = platformTex->paletteType;

    eRasterFormat dstRasterFormat = platformTex->rasterFormat;
    uint32 dstDepth = srcDepth;

    bool hasAlpha = platformTex->hasAlpha;

    bool isSwizzledFormat = isXBOXTextureSwizzled( dstRasterFormat, dstDepth, srcPaletteType, xboxCompressionType );

    bool canHavePalette = false;

    if ( rwCompressionType == RWCOMPRESS_DXT1 ||
         rwCompressionType == RWCOMPRESS_DXT2 ||
         rwCompressionType == RWCOMPRESS_DXT3 ||
         rwCompressionType == RWCOMPRESS_DXT4 ||    // XBOX does not support this, I think.
         rwCompressionType == RWCOMPRESS_DXT5 )
    {
        dstRasterFormat = getVirtualRasterFormat( hasAlpha, rwCompressionType );

        dstDepth = Bitmap::getRasterFormatDepth( dstRasterFormat );
    }
    else if ( rwCompressionType == RWCOMPRESS_NONE )
    {
        // We can have a palette.
        canHavePalette = true;
    }
    else
    {
        throw NativeTextureInternalErrorException( "XBOX", L"XBOX_INTERNERR_UNKCOMPRTYPE" );
    }

    // If we are swizzled, then we have to create a new copy of the texels which is in linear format.
    // Otherwise we can optimize.
    bool isNewlyAllocated = ( isSwizzledFormat == true );

    size_t mipmapCount = platformTex->mipmaps.GetCount();

    // Allocate virtual mipmaps.
    mipmapsOut.Resize( mipmapCount );

    for ( size_t n = 0; n < mipmapCount; n++ )
    {
        const NativeTextureXBOX::mipmapLayer& mipLayer = platformTex->mipmaps[ n ];

        // Fetch all mipmap data onto the stack.
        uint32 mipWidth = mipLayer.width;
        uint32 mipHeight = mipLayer.height;

        uint32 layerWidth = mipLayer.layerWidth;
        uint32 layerHeight = mipLayer.layerHeight;

        void *dstTexels = nullptr;
        uint32 dstDataSize = 0;

        if ( isSwizzledFormat == false )
        {
            // We can simply move things over.
            dstTexels = mipLayer.texels;
            dstDataSize = mipLayer.dataSize;
        }
        else
        {
            // Here we have to put the pixels into a linear format.
            NativeTextureXBOX::swizzleMipmapTraversal swizzleTrav;

            swizzleTrav.mipWidth = mipWidth;
            swizzleTrav.mipHeight = mipHeight;
            swizzleTrav.depth = srcDepth;
            swizzleTrav.rowAlignment = getXBOXTextureDataRowAlignment();
            swizzleTrav.texels = mipLayer.texels;
            swizzleTrav.dataSize = mipLayer.dataSize;

            NativeTextureXBOX::unswizzleMipmap( engineInterface, swizzleTrav );

            assert( swizzleTrav.newtexels != swizzleTrav.texels );

            mipWidth = swizzleTrav.newWidth;
            mipHeight = swizzleTrav.newHeight;
            dstTexels = swizzleTrav.newtexels;
            dstDataSize = swizzleTrav.newDataSize;
        }

        // Create a new virtual mipmap layer.
        defaultedMipmapLayerType newLayer;

        newLayer.width = mipWidth;
        newLayer.height = mipHeight;

        newLayer.layerWidth = layerWidth;
        newLayer.layerHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;

        // Store this layer.
        mipmapsOut[ n ] = newLayer;
    }

    // If we can have a palette, copy it over.
    ePaletteType dstPaletteType = PALETTE_NONE;
    void *dstPaletteData = nullptr;
    uint32 dstPaletteSize = 0;

    if ( canHavePalette )
    {
        copyPaletteData(
            engineInterface,
            srcPaletteType, platformTex->palette, platformTex->paletteSize,
            dstRasterFormat, isNewlyAllocated,
            dstPaletteType, dstPaletteData, dstPaletteSize
        );
    }

    // Copy over general raster data.
    rasterFormatOut = dstRasterFormat;
    depthOut = dstDepth;
    rowAlignmentOut = getXBOXTextureDataRowAlignment();
    colorOrderOut = platformTex->colorOrder;

    compressionTypeOut = rwCompressionType;

    // Give it the palette data.
    paletteDataOut = dstPaletteData;
    paletteSizeOut = dstPaletteSize;
    paletteTypeOut = dstPaletteType;

    // Copy over more advanced parameters.
    hasAlphaOut = hasAlpha;
    autoMipmapsOut = platformTex->autoMipmaps;
    rasterTypeOut = platformTex->rasterType;

    // Tell the runtime whether we newly allocated those texels.
    isNewlyAllocatedOut = isNewlyAllocated;
}

template <typename defaultedMipLayerType, typename mipmapVectorType>
AINLINE void xboxAcquirePixelDataToTexture(
    Interface *engineInterface,
    NativeTextureXBOX *xboxTex, // the texture to acquire data to.
    const mipmapVectorType& mipmaps,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder,
    ePaletteType srcPaletteType, void *srcPaletteData, uint32 srcPaletteSize, eCompressionType rwCompressionType,
    uint32 texRasterType, bool texHasAlpha, bool texAutoMipmaps,
    bool& hasDirectlyAcquiredOut
)
{
    // Verify native texture size rules.
    {
        nativeTextureSizeRules sizeRules;
        getXBOXNativeTextureSizeRules( sizeRules );

        if ( !sizeRules.verifyMipmaps( mipmaps ) )
        {
            throw NativeTextureInvalidConfigurationException( "XBOX", L"XBOX_INVALIDCFG_MIPDIMMS" );
        }
    }

    // We need to make sure that the raster data is compatible.
    eRasterFormat dstRasterFormat = srcRasterFormat;
    uint32 dstDepth = srcDepth;
    uint32 dstRowAlignment = getXBOXTextureDataRowAlignment();
    eColorOrdering dstColorOrder = srcColorOrder;

    ePaletteType dstPaletteType = srcPaletteType;
    uint32 dstPaletteSize = srcPaletteSize;

    // Move over the texture data.
    uint32 xboxCompressionType = 0;

    bool canHavePaletteData = false;

    bool canBeConverted = false;

    if ( rwCompressionType != RWCOMPRESS_NONE )
    {
        xboxCompressionType = getXBOXCompressionTypeFromRW( rwCompressionType );

        canHavePaletteData = false;
    }
    else
    {
        // We need to verify that we have good raster properties.
        xbox::convertCompatibleRasterFormat(
            dstRasterFormat, dstDepth, dstColorOrder, dstPaletteType
        );

        canBeConverted = true;

        canHavePaletteData = true;
    }

    // Based on the destination format we can decide if the color data has to be swizzled.
    bool requiresSwizzling = isXBOXTextureSwizzled( dstRasterFormat, dstDepth, dstPaletteType, xboxCompressionType );

    // Check whether we need conversion.
    bool requiresConversion = false;

    if ( canBeConverted )
    {
        if ( doesPixelDataOrPaletteDataNeedConversion(
                 mipmaps,
                 srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteSize, rwCompressionType,
                 dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstPaletteSize, rwCompressionType
             )
           )
        {
            // If any format property is different, we need to convert.
            requiresConversion = true;
        }
    }

    // Store texel data.
    // If we require swizzling, we cannot directly acquire the data.
    bool hasDirectlyAcquired = ( requiresSwizzling == false && requiresConversion == false );

    size_t mipmapCount = mipmaps.GetCount();

    // Allocate the mipmaps on the XBOX texture.
    xboxTex->mipmaps.Resize( mipmapCount );

    for ( size_t n = 0; n < mipmapCount; n++ )
    {
        const defaultedMipLayerType& srcLayer = mipmaps[ n ];

        // Grab the pixel information onto the stack.
        uint32 mipWidth = srcLayer.width;
        uint32 mipHeight = srcLayer.height;

        uint32 layerWidth = srcLayer.layerWidth;
        uint32 layerHeight = srcLayer.layerHeight;

        void *srcTexels = srcLayer.texels;
        uint32 srcDataSize = srcLayer.dataSize;

        // The destination texels.
        void *dstTexels = srcTexels;
        uint32 dstDataSize = srcDataSize;

        bool hasNewTexels = false;

        // It could need conversion to another format.
        if ( requiresConversion )
        {
            // Call the helper function.
            CopyTransformRawMipmapLayer(
                engineInterface,
                mipWidth, mipHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType,
                dstTexels, dstDataSize
            );

            hasNewTexels = true;
        }

        // If we require swizzling, then we need to allocate a new copy of the texels
        // as destination buffer.
        if ( requiresSwizzling )
        {
            NativeTextureXBOX::swizzleMipmapTraversal swizzleTrav;

            swizzleTrav.mipWidth = mipWidth;
            swizzleTrav.mipHeight = mipHeight;
            swizzleTrav.depth = dstDepth;
            swizzleTrav.rowAlignment = dstRowAlignment;
            swizzleTrav.texels = dstTexels;
            swizzleTrav.dataSize = dstDataSize;

            NativeTextureXBOX::swizzleMipmap( engineInterface, swizzleTrav );

            assert( swizzleTrav.texels != swizzleTrav.newtexels );

            // If we had newly allocated texels, free them.
            if ( hasNewTexels == true )
            {
                engineInterface->PixelFree( dstTexels );
            }

            // Update with the swizzled parameters.
            mipWidth = swizzleTrav.newWidth;
            mipHeight = swizzleTrav.newHeight;
            dstTexels = swizzleTrav.newtexels;
            dstDataSize = swizzleTrav.newDataSize;

            hasNewTexels = true;
        }

        // Store it as mipmap.
        NativeTextureXBOX::mipmapLayer& newLayer = xboxTex->mipmaps[ n ];

        newLayer.width = mipWidth;
        newLayer.height = mipHeight;

        newLayer.layerWidth = layerWidth;
        newLayer.layerHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;
    }

    // Also copy over the palette data.
    void *dstPaletteData = nullptr;

    if ( canHavePaletteData )
    {
        if ( dstPaletteType != PALETTE_NONE )
        {
            if ( requiresConversion || hasDirectlyAcquired == false )
            {
                // Create a new copy.
                CopyConvertPaletteData(
                    engineInterface,
                    srcPaletteData,
                    srcPaletteSize, dstPaletteSize,
                    srcRasterFormat, srcColorOrder,
                    dstRasterFormat, dstColorOrder,
                    dstPaletteData
                );
            }
            else
            {
                // Just move it over.
                dstPaletteData = srcPaletteData;
            }
        }
    }

    // Copy general raster information.
    xboxTex->rasterFormat = dstRasterFormat;
    xboxTex->depth = dstDepth;
    xboxTex->colorOrder = dstColorOrder;

    // Store the palette.
    xboxTex->paletteType = dstPaletteType;
    xboxTex->palette = dstPaletteData;
    xboxTex->paletteSize = dstPaletteSize;

    // Copy more advanced properties.
    xboxTex->rasterType = texRasterType;
    xboxTex->hasAlpha = texHasAlpha;

    // Properly set the automipmaps field.
    bool autoMipmaps = texAutoMipmaps;

    if ( mipmapCount > 1 )
    {
        autoMipmaps = false;
    }

    xboxTex->autoMipmaps = autoMipmaps;

    xboxTex->dxtCompression = xboxCompressionType;

    // Since we have to swizzle the pixels, we cannot directly acquire the texels.
    // If the input was compressed though, we can directly take the pixels.
    hasDirectlyAcquiredOut = hasDirectlyAcquired;
}

}

#endif //RWLIB_INCLUDE_NATIVETEX_XBOX

#endif //_RENDERWARE_XBOX_NATIVETEX_LAYER_PIPELINE_