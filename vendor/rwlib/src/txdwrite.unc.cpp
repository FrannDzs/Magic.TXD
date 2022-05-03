/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdwrite.unc.cpp
*  PURPOSE:     unc_mobile native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE

#include "pixelformat.hxx"

#include "txdread.d3d.hxx"
#include "txdread.unc.hxx"

#include "streamutil.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

void uncNativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    // Cast to our native texture format.
    NativeTextureMobileUNC *platformTex = (NativeTextureMobileUNC*)nativeTex;

    size_t mipmapCount = platformTex->mipmaps.GetCount();

    if ( mipmapCount == 0 )
    {
        throw NativeTextureRwObjectsInternalErrorException( "uncompressed_mobile", AcquireObject( theTexture ), L"UNCMOBILE_INTERNERR_WRITEEMPTY" );
    }

    // Write the texture image data block.
    {
        BlockProvider texImageDataBlock( &outputProvider );

        // Write the generic meta header.
        mobile_unc::textureNativeGenericHeader metaHeader;
        metaHeader.platformDescriptor = PLATFORMDESC_UNC_MOBILE;
        metaHeader.formatInfo.set( *theTexture );

        memset( metaHeader.pad1, 0, sizeof( metaHeader.pad1 ) );

        // Correctly write the name strings (for safety).
        // Even though we can read those name fields with zero-termination safety,
        // the engines are not guarranteed to do so.
        // Also, print a warning if the name is changed this way.
        writeStringIntoBufferSafe( engineInterface, theTexture->GetName(), metaHeader.name, sizeof( metaHeader.name ), theTexture, L"name" );
        writeStringIntoBufferSafe( engineInterface, theTexture->GetMaskName(), metaHeader.maskName, sizeof( metaHeader.maskName ), theTexture, L"mask name" );

        bool hasAlpha = platformTex->hasAlpha;

        metaHeader.mipmapCount = (uint8)mipmapCount;
        metaHeader.unk1 = false;
        metaHeader.hasAlpha = hasAlpha;
        metaHeader.pad2 = 0;

        metaHeader.width = platformTex->mipmaps[ 0 ].layerWidth;
        metaHeader.height = platformTex->mipmaps[ 0 ].layerHeight;

        metaHeader.unk2 = platformTex->unk2;
        metaHeader.unk3 = platformTex->unk3;

        // Calculate the image data section size.
        uint32 imgDataSectionSize = 0;

        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            const NativeTextureMobileUNC::mipmapLayer& mipLayer = platformTex->mipmaps[ n ];

            imgDataSectionSize += mipLayer.dataSize;
        }

        metaHeader.imageDataSectionSize = imgDataSectionSize;

        // Write the header.
        texImageDataBlock.write( &metaHeader, sizeof( metaHeader ) );

        // Now write all the mipmap layers.
        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            const NativeTextureMobileUNC::mipmapLayer& mipLayer = platformTex->mipmaps[ n ];

            uint32 mipDataSize = mipLayer.dataSize;

            const void *srcTexels = mipLayer.texels;

            texImageDataBlock.write( srcTexels, mipDataSize );
        }
    }

    // Serialize the extensions.
    engineInterface->SerializeExtensions( theTexture, outputProvider );
}

void uncNativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    // This is a pretty simple task; just give the pixels directly to the runtime.
    NativeTextureMobileUNC *nativeTex = (NativeTextureMobileUNC*)objMem;

    // Move over mipmaps.
    size_t mipmapCount = nativeTex->mipmaps.GetCount();

    pixelsOut.mipmaps.Resize( mipmapCount );

    for ( size_t n = 0; n < mipmapCount; n++ )
    {
        const NativeTextureMobileUNC::mipmapLayer& mipLayer = nativeTex->mipmaps[ n ];

        // Create a new layer.
        pixelDataTraversal::mipmapResource newLayer;

        newLayer.width = mipLayer.width;
        newLayer.height = mipLayer.height;

        newLayer.layerWidth = mipLayer.layerWidth;
        newLayer.layerHeight = mipLayer.layerHeight;

        newLayer.texels = mipLayer.texels;
        newLayer.dataSize = mipLayer.dataSize;

        // Store the new virtual layer.
        pixelsOut.mipmaps[ n ] = newLayer;
    }

    // Copy over our raster format properties.
    bool hasAlpha = nativeTex->hasAlpha;

    eRasterFormat rasterFormat;
    uint32 depth;
    eColorOrdering colorOrder;

    getUNCRasterFormat( hasAlpha, rasterFormat, colorOrder, depth );

    pixelsOut.rasterFormat = rasterFormat;
    pixelsOut.depth = depth;
    pixelsOut.rowAlignment = getUNCTextureDataRowAlignment();
    pixelsOut.colorOrder = colorOrder;

    // We cannot have a palette in uncompressed rasters.
    pixelsOut.paletteType = PALETTE_NONE;
    pixelsOut.paletteData = nullptr;
    pixelsOut.paletteSize = 0;

    // Always uncompressed.
    pixelsOut.compressionType = RWCOMPRESS_NONE;

    pixelsOut.hasAlpha = hasAlpha;
    pixelsOut.autoMipmaps = false;
    pixelsOut.cubeTexture = false;
    pixelsOut.rasterType = 4;

    // We have given the pixels directly to the runtime.
    pixelsOut.isNewlyAllocated = false;
}

void uncNativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureMobileUNC *nativeTex = (NativeTextureMobileUNC*)objMem;

    // Make sure we get only raw bitmap data.
    assert( pixelsIn.compressionType == RWCOMPRESS_NONE );

    // Verify mipmap dimension size rules.
    {
        nativeTextureSizeRules sizeRules;
        getUNCNativeTextureSizeRules( sizeRules );

        if ( !sizeRules.verifyPixelData( pixelsIn ) )
        {
            throw NativeTextureInvalidConfigurationException( "uncompressed_mobile", L"UNCMOBILE_INVALIDCFG_MIPDIMMS" );
        }
    }
    
    // Whether we can take those pixels depends on whether the pixels have alpha.
    // This uncompressed raster has a fixed raster format.
    bool hasAlpha = pixelsIn.hasAlpha;

    eRasterFormat requiredRasterFormat;
    uint32 requiredDepth;
    eColorOrdering requiredColorOrder;

    uint32 requiredRowAlignment = getUNCTextureDataRowAlignment();

    getUNCRasterFormat( hasAlpha, requiredRasterFormat, requiredColorOrder, requiredDepth );

    // We cannot have any palette.
    ePaletteType requiredPaletteType = PALETTE_NONE;

    // Check whether we can directly take the texels or need conversion.
    bool hasConvertedTexels = false;

    eRasterFormat srcRasterFormat = pixelsIn.rasterFormat;
    uint32 srcDepth = pixelsIn.depth;
    uint32 srcRowAlignment = pixelsIn.rowAlignment;
    eColorOrdering srcColorOrder = pixelsIn.colorOrder;
    ePaletteType srcPaletteType = pixelsIn.paletteType;
    void *srcPaletteData = pixelsIn.paletteData;
    uint32 srcPaletteSize = pixelsIn.paletteSize;

    if ( !doesPixelDataNeedConversion(
             pixelsIn.mipmaps,
             srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, RWCOMPRESS_NONE,
             requiredRasterFormat, requiredDepth, requiredRowAlignment, requiredColorOrder, requiredPaletteType, RWCOMPRESS_NONE
          )
        )
    {
        // We can directly take the pixels.
        hasConvertedTexels = false;
    }
    else
    {
        // Conversion is necessary.
        hasConvertedTexels = true;
    }

    // Do the mipmap conversion.
    size_t mipmapCount = pixelsIn.mipmaps.GetCount();

    nativeTex->mipmaps.Resize( mipmapCount );

    for ( size_t n = 0; n < mipmapCount; n++ )
    {
        const pixelDataTraversal::mipmapResource& mipLayer = pixelsIn.mipmaps[ n ];

        // Get the source parameters.
        uint32 surfWidth = mipLayer.width;
        uint32 surfHeight = mipLayer.height;

        uint32 layerWidth = mipLayer.layerWidth;
        uint32 layerHeight = mipLayer.layerHeight;

        void *srcTexels = mipLayer.texels;
        uint32 srcDataSize = mipLayer.dataSize;

        // Do conversion if necessary.
        void *dstTexels = srcTexels;
        uint32 dstDataSize = srcDataSize;

        if ( hasConvertedTexels )
        {
            CopyTransformRawMipmapLayer(
                engineInterface,
                surfWidth, surfHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize,
                requiredRasterFormat, requiredDepth, requiredRowAlignment, requiredColorOrder, requiredPaletteType,
                dstTexels, dstDataSize
            );
        }

        // Store this layer.
        NativeTextureMobileUNC::mipmapLayer newLayer;
        
        newLayer.width = surfWidth;
        newLayer.height = surfHeight;

        newLayer.layerWidth = layerWidth;
        newLayer.layerHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;

        // Put it into the texture.
        nativeTex->mipmaps[ n ] = newLayer;
    }

    // Store advanced parameters.
    nativeTex->hasAlpha = hasAlpha;

    // If we have converted the texels, we not directly taken them from the given ones.
    feedbackOut.hasDirectlyAcquired = ( hasConvertedTexels == false );
}

void uncNativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    NativeTextureMobileUNC *nativeTex = (NativeTextureMobileUNC*)objMem;

    if ( deallocate )
    {
        // Deallocate our mipmaps.
        deleteMipmapLayers( engineInterface, nativeTex->mipmaps );
    }

    // Unset our stuff.
    nativeTex->mipmaps.Clear();

    // For debugging purposes, reset raster format fields.
    nativeTex->hasAlpha = false;
}

struct uncMipmapManager
{
    NativeTextureMobileUNC *nativeTex;

    inline uncMipmapManager( NativeTextureMobileUNC *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureMobileUNC::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void GetSizeRules( nativeTextureSizeRules& rulesOut ) const
    {
        getUNCNativeTextureSizeRules( rulesOut );
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureMobileUNC::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        uint32& dstRowAlignment,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocated
    )
    {
        // Just pass it along.
        getUNCRasterFormat( nativeTex->hasAlpha, dstRasterFormat, dstColorOrder, dstDepth );

        dstRowAlignment = getUNCTextureDataRowAlignment();

        dstPaletteType = PALETTE_NONE;
        dstPaletteData = nullptr;
        dstPaletteSize = 0;

        dstCompressionType = RWCOMPRESS_NONE;

        widthOut = mipLayer.width;
        heightOut = mipLayer.height;

        layerWidthOut = mipLayer.layerWidth;
        layerHeightOut = mipLayer.layerHeight;

        dstTexelsOut = mipLayer.texels;
        dstDataSizeOut = mipLayer.dataSize;

        isNewlyAllocatedOut = false;
        isPaletteNewlyAllocated = false;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureMobileUNC::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        uint32 rowAlignment,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        // Convert it to our format.
        eRasterFormat texRasterFormat;
        uint32 texDepth;
        eColorOrdering texColorOrder;

        getUNCRasterFormat( nativeTex->hasAlpha, texRasterFormat, texColorOrder, texDepth );

        bool hasChanged =
            ConvertMipmapLayerNative(
                engineInterface,
                width, height, layerWidth, layerHeight, srcTexels, dataSize,
                rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                texRasterFormat, texDepth, getUNCTextureDataRowAlignment(), texColorOrder, PALETTE_NONE, nullptr, 0, RWCOMPRESS_NONE,
                false,
                width, height,
                srcTexels, dataSize
            );

        // Save the stuff.
        mipLayer.width = width;
        mipLayer.height = height;
        mipLayer.layerWidth = layerWidth;
        mipLayer.layerHeight = layerHeight;
        mipLayer.texels = srcTexels;
        mipLayer.dataSize = dataSize;

        // If we have allocated anything, it is not direct acquisition.
        hasDirectlyAcquiredOut = ( hasChanged == false );
    }
};

bool uncNativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTextureMobileUNC *nativeTex = (NativeTextureMobileUNC*)objMem;

    uncMipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureMobileUNC::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool uncNativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureMobileUNC *nativeTex = (NativeTextureMobileUNC*)objMem;

    uncMipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureMobileUNC::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void uncNativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureMobileUNC *nativeTex = (NativeTextureMobileUNC*)objMem;

    virtualClearMipmaps <NativeTextureMobileUNC::mipmapLayer> ( engineInterface, nativeTex->mipmaps );
}

void uncNativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTextureMobileUNC *nativeTex = (NativeTextureMobileUNC*)objMem;

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

void uncNativeTextureTypeProvider::GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const
{
    NativeTextureMobileUNC *nativeTex = (NativeTextureMobileUNC*)objMem;

    // Get our format.
    rwString <char> formatString( eir::constr_with_alloc::DEFAULT, engineInterface );

    eRasterFormat rasterFormat;
    uint32 depth;
    eColorOrdering colorOrder;

    getUNCRasterFormat( nativeTex->hasAlpha, rasterFormat, colorOrder, depth );

    getDefaultRasterFormatString( rasterFormat, depth, PALETTE_NONE, colorOrder, formatString );

    if ( buf )
    {
        strncpy( buf, formatString.GetConstString(), bufLen );
    }

    lengthOut = formatString.GetLength();
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE