/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.xbox.cpp
*  PURPOSE:     XBOX native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_XBOX

#include "txdread.xbox.hxx"
#include "txdread.xbox.layerpipe.hxx"

#include "txdread.d3d.hxx"

#include "pixelformat.hxx"

#include "txdread.d3d.dxt.hxx"

#include "pluginutil.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

void xboxNativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    // Read the image data chunk.
    {
        BlockProvider texImageDataBlock( &inputProvider, CHUNK_STRUCT );

	    uint32 platform = texImageDataBlock.readUInt32();

	    if (platform != NATIVE_TEXTURE_XBOX)
        {
            throw NativeTextureRwObjectsStructuralErrorException( "XBOX", AcquireObject( theTexture ), L"XBOX_STRUCTERR_PLATFORMID" );
        }

        // Cast to our native texture format.
        NativeTextureXBOX *platformTex = (NativeTextureXBOX*)nativeTex;

        int engineWarningLevel = engineInterface->GetWarningLevel();

        bool engineIgnoreSecureWarnings = engineInterface->GetIgnoreSecureWarnings();

        // Read the main texture header.
        xbox::textureMetaHeaderStruct metaInfo;
        texImageDataBlock.read( &metaInfo, sizeof(metaInfo) );

        // Read the texture names.
        {
            char tmpbuf[ sizeof( metaInfo.name ) + 1 ];

            // Make sure the name buffer is zero terminted.
            tmpbuf[ sizeof( metaInfo.name ) ] = '\0';

            // Move over the texture name.
            memcpy( tmpbuf, metaInfo.name, sizeof( metaInfo.name ) );
                    
            theTexture->SetName( tmpbuf );

            // Move over the texture mask name.
            memcpy( tmpbuf, metaInfo.maskName, sizeof( metaInfo.maskName ) );

            theTexture->SetMaskName( tmpbuf );
        }

        // Read format info.
        texFormatInfo formatInfo = metaInfo.formatInfo;

        formatInfo.parse( *theTexture );

        // Deconstruct the rasterFormat flags.
        bool hasMipmaps = false;        // TODO: actually use this flag.
        bool autoMipmaps = false;

        readRasterFormatFlags( metaInfo.rasterFormat, platformTex->rasterFormat, platformTex->paletteType, hasMipmaps, autoMipmaps );

        platformTex->hasAlpha = ( metaInfo.hasAlpha != 0 );

        platformTex->isCubeMap = ( metaInfo.isCubeMap != 0 );

#if _DEBUG
        assert( platformTex->isCubeMap == false );
#endif

        uint32 depth = metaInfo.depth;
        uint32 maybeMipmapCount = metaInfo.mipmapCount;

        platformTex->depth = depth;

        platformTex->rasterType = metaInfo.rasterType;

        platformTex->dxtCompression = metaInfo.dxtCompression;

        platformTex->autoMipmaps = autoMipmaps;

        ePaletteType paletteType = platformTex->paletteType;

        // XBOX texture are always BGRA.
        platformTex->colorOrder = COLOR_BGRA;

        // Verify the parameters.
        eRasterFormat rasterFormat = platformTex->rasterFormat;

        // - depth
        {
            bool isValidDepth = true;

            if (paletteType == PALETTE_4BIT)
            {
                if (depth != 4 && depth != 8)
                {
                    isValidDepth = false;
                }
            }
            else if (paletteType == PALETTE_8BIT)
            {
                if (depth != 8)
                {
                    isValidDepth = false;
                }
            }
            // TODO: find more ways of verification here.

            if (isValidDepth == false)
            {
                // We cannot repair a broken depth.
                throw NativeTextureRwObjectsStructuralErrorException( "XBOX", AcquireObject( theTexture ), L"XBOX_STRUCTERR_INVDEPTH" );
            }
        }

        uint32 remainingImageSectionData = metaInfo.imageDataSectionSize;

        if (paletteType != PALETTE_NONE)
        {
            uint32 palItemCount = getD3DPaletteCount(paletteType);

            uint32 palDepth = Bitmap::getRasterFormatDepth( rasterFormat );

            uint32 paletteDataSize = getPaletteDataSize( palItemCount, palDepth );

            // Do we have palette data in the stream?
            texImageDataBlock.check_read_ahead( paletteDataSize );

	        void *palData = engineInterface->PixelAllocate( paletteDataSize );

            try
            {
	            texImageDataBlock.read( palData, paletteDataSize );
            }
            catch( ... )
            {
                engineInterface->PixelFree( palData );

                throw;
            }

            // Write the parameters.
            platformTex->palette = palData;
	        platformTex->paletteSize = palItemCount;
        }

        // Read all the textures.
        uint32 actualMipmapCount = 0;

        mipGenLevelGenerator mipLevelGen( metaInfo.width, metaInfo.height );

        if ( !mipLevelGen.isValidLevel() )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "XBOX", AcquireObject( theTexture ), L"XBOX_STRUCTERR_INVMIPDIMMS" );
        }

        uint32 dxtCompression = platformTex->dxtCompression;

        bool hasZeroSizedMipmaps = false;

        for (uint32 i = 0; i < maybeMipmapCount; i++)
        {
            if ( remainingImageSectionData == 0 )
            {
                break;
            }

            bool couldIncrementLevel = true;

	        if (i > 0)
            {
                couldIncrementLevel = mipLevelGen.incrementLevel();
            }

            if ( !couldIncrementLevel )
            {
                break;
            }

            // If any dimension is zero, ignore that mipmap.
            if ( !mipLevelGen.isValidLevel() )
            {
                hasZeroSizedMipmaps = true;
                break;
            }

            // Start a new mipmap layer.
            NativeTextureXBOX::mipmapLayer newLayer;

            newLayer.layerWidth = mipLevelGen.getLevelWidth();
            newLayer.layerHeight = mipLevelGen.getLevelHeight();

            // Process dimensions.
            uint32 texWidth = newLayer.layerWidth;
            uint32 texHeight = newLayer.layerHeight;
            {
		        // DXT compression works on 4x4 blocks,
		        // align the dimensions.
		        if (dxtCompression != 0)
                {
			        texWidth = ALIGN_SIZE( texWidth, 4u );
                    texHeight = ALIGN_SIZE( texHeight, 4u );
		        }
            }

            newLayer.width = texWidth;
            newLayer.height = texHeight;

            // Calculate the data size of this mipmap.
            uint32 texDataSize = 0;

            if (dxtCompression != 0)
            {
                uint32 texUnitCount = ( texWidth * texHeight );

                eCompressionType rwCompressionType;

                // We only support DXT (for now).
                uint32 dxtType = 0;

                bool gotType = getDXTCompressionTypeFromXBOX( dxtCompression, rwCompressionType );

                if ( gotType )
                {
	                if ( rwCompressionType == RWCOMPRESS_DXT1 )  // DXT1 (?)
                    {
                        dxtType = 1;
                    }
                    else if ( rwCompressionType == RWCOMPRESS_DXT2 )
                    {
                        dxtType = 2;
                    }
                    else if ( rwCompressionType == RWCOMPRESS_DXT3 )
                    {
                        dxtType = 3;
                    }
                    else if ( rwCompressionType == RWCOMPRESS_DXT4 )
                    {
                        dxtType = 4;
                    }
                    else if ( rwCompressionType == RWCOMPRESS_DXT5 )
                    {
                        dxtType = 5;
                    }
                }
                        
                if ( dxtType == 0 )
                {
                    throw NativeTextureRwObjectsStructuralErrorException( "XBOX", AcquireObject( theTexture ), L"XBOX_STRUCTERR_DXTTYPE" );
                }
            	        
                texDataSize = getDXTRasterDataSize(dxtType, texUnitCount);
            }
            else
            {
                // There should also be raw rasters supported.
                rasterRowSize rowSize = getD3DRasterDataRowSize( texWidth, depth );

                texDataSize = getRasterDataSizeByRowSize( rowSize, texHeight );
            }

            if ( remainingImageSectionData < texDataSize )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "XBOX", AcquireObject( theTexture ), L"XBOX_STRUCTERR_IMGDATASECTIONSIZE" );
            }

            // Decrement the overall remaining size.
            remainingImageSectionData -= texDataSize;

            // Store the texture size.
            newLayer.dataSize = texDataSize;

            // Do we have texel data in the stream?
            texImageDataBlock.check_read_ahead( texDataSize );

            // Fetch the texels.
            newLayer.texels = engineInterface->PixelAllocate( texDataSize );

            try
            {
	            texImageDataBlock.read( newLayer.texels, texDataSize );

                // Store the layer.
                platformTex->mipmaps.AddToBack( std::move( newLayer ) );
            }
            catch( ... )
            {
                engineInterface->PixelFree( newLayer.texels );

                throw;
            }

            // Increase mipmap count.
            actualMipmapCount++;
        }

        if ( actualMipmapCount == 0 )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "XBOX", AcquireObject( theTexture ), L"XBOX_STRUCTERR_EMPTY" );
        }

        if ( remainingImageSectionData != 0 )
        {
            if ( engineWarningLevel >= 2 )
            {
                engineInterface->PushWarningObject( theTexture, L"XBOX_WARN_IMGDATASECTBYTESLEFT" );
            }

            // Skip those bytes.
            texImageDataBlock.skip( remainingImageSectionData );
        }

        if ( hasZeroSizedMipmaps )
        {
            if ( !engineIgnoreSecureWarnings )
            {
                engineInterface->PushWarningObject( theTexture, L"XBOX_WARN_ZEROSIZEDMIPMAPS" );
            }
        }

        // Fix filtering mode.
        fixFilteringMode( *theTexture, actualMipmapCount );

        // Fix the auto-mipmap flag.
        {
            bool hasAutoMipmaps = platformTex->autoMipmaps;

            if ( hasAutoMipmaps )
            {
                bool canHaveAutoMipmaps = ( actualMipmapCount == 1 );

                if ( !canHaveAutoMipmaps )
                {
                    engineInterface->PushWarningObject( theTexture, L"XBOX_WARN_INVAUTOMIPMAP" );

                    platformTex->autoMipmaps = false;
                }
            }
        }
    }

    // Now the extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

static optional_struct_space <PluginDependantStructRegister <xboxNativeTextureTypeProvider, RwInterfaceFactory_t>> xboxNativeTexturePlugin;

void registerXBOXNativePlugin( void )
{
    xboxNativeTexturePlugin.Construct( engineFactory );
}

void unregisterXBOXNativePlugin( void )
{
    xboxNativeTexturePlugin.Destroy();
}

void xboxNativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    // Cast to our native format.
    NativeTextureXBOX *platformTex = (NativeTextureXBOX*)objMem;

    xboxFetchPixelDataFromTexture <pixelDataTraversal::mipmapResource> (
        engineInterface,
        platformTex,
        pixelsOut.mipmaps,
        pixelsOut.rasterFormat, pixelsOut.depth, pixelsOut.rowAlignment, pixelsOut.colorOrder,
        pixelsOut.paletteType, pixelsOut.paletteData, pixelsOut.paletteSize, pixelsOut.compressionType,
        pixelsOut.rasterType, pixelsOut.hasAlpha, pixelsOut.autoMipmaps,
        pixelsOut.isNewlyAllocated
    );
    
    pixelsOut.cubeTexture = false;  // XBOX never has cube textures.
}

void xboxNativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    // Cast to our native texture container.
    NativeTextureXBOX *xboxTex = (NativeTextureXBOX*)objMem;

    bool hasDirectlyAcquired;

    xboxAcquirePixelDataToTexture <pixelDataTraversal::mipmapResource> (
        engineInterface,
        xboxTex,
        pixelsIn.mipmaps,
        pixelsIn.rasterFormat, pixelsIn.depth, pixelsIn.rowAlignment, pixelsIn.colorOrder,
        pixelsIn.paletteType, pixelsIn.paletteData, pixelsIn.paletteSize, pixelsIn.compressionType,
        pixelsIn.rasterType, pixelsIn.hasAlpha, pixelsIn.autoMipmaps,
        hasDirectlyAcquired
    );

    feedbackOut.hasDirectlyAcquired = hasDirectlyAcquired;
}

void xboxNativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

    if ( deallocate )
    {
        // We simply deallocate everything.
        if ( void *palette = nativeTex->palette )
        {
            engineInterface->PixelFree( palette );
        }

        deleteMipmapLayers( engineInterface, nativeTex->mipmaps );
    }

    // Clear all connections.
    nativeTex->palette = nullptr;
    nativeTex->paletteSize = 0;
    nativeTex->paletteType = PALETTE_NONE;

    nativeTex->mipmaps.Clear();

    // Reset raster parameters for debugging purposes.
    nativeTex->rasterFormat = RASTER_DEFAULT;
    nativeTex->depth = 0;
    nativeTex->colorOrder = COLOR_BGRA;
    nativeTex->hasAlpha = false;
    nativeTex->autoMipmaps = false;
    nativeTex->dxtCompression = 0;
}

struct xboxMipmapManager
{
    NativeTextureXBOX *nativeTex;

    inline xboxMipmapManager( NativeTextureXBOX *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureXBOX::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void GetSizeRules( nativeTextureSizeRules& rulesOut ) const
    {
        getXBOXNativeTextureSizeRules( rulesOut );
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureXBOX::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        uint32& dstRowAlignment,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocated
    )
    {
        void *texels = mipLayer.texels;
        uint32 dataSize = mipLayer.dataSize;

        uint32 mipWidth = mipLayer.width;
        uint32 mipHeight = mipLayer.height;

        uint32 layerWidth = mipLayer.layerWidth;
        uint32 layerHeight = mipLayer.layerHeight;

        uint32 texDepth = nativeTex->depth;

        bool isNewlyAllocated = false;

        // Decide whether the texture is swizzled.
        eRasterFormat texRasterFormat = nativeTex->rasterFormat;
        ePaletteType texPaletteType = nativeTex->paletteType;
        uint32 texCompressionType = nativeTex->dxtCompression;

        bool isSwizzled = isXBOXTextureSwizzled( texRasterFormat, texDepth, texPaletteType, texCompressionType );

        // If we are swizzled, we must unswizzle.
        if ( isSwizzled )
        {
            NativeTextureXBOX::swizzleMipmapTraversal swizzleTrav;

            swizzleTrav.mipWidth = mipWidth;
            swizzleTrav.mipHeight = mipHeight;
            swizzleTrav.depth = texDepth;
            swizzleTrav.rowAlignment = getXBOXTextureDataRowAlignment();
            swizzleTrav.texels = texels;
            swizzleTrav.dataSize = dataSize;

            NativeTextureXBOX::unswizzleMipmap( engineInterface, swizzleTrav );

            mipWidth = swizzleTrav.newWidth;
            mipHeight = swizzleTrav.newHeight;

            texels = swizzleTrav.newtexels;
            dataSize = swizzleTrav.newDataSize;

            isNewlyAllocated = true;
        }

        // Return stuff.
        widthOut = mipWidth;
        heightOut = mipHeight;

        layerWidthOut = layerWidth;
        layerHeightOut = layerHeight;

        dstRasterFormat = texRasterFormat;
        dstColorOrder = nativeTex->colorOrder;
        dstDepth = texDepth;
        dstRowAlignment = getXBOXTextureDataRowAlignment();

        dstPaletteType = texPaletteType;
        dstPaletteData = nativeTex->palette;
        dstPaletteSize = nativeTex->paletteSize;

        bool gotComprType = getDXTCompressionTypeFromXBOX( texCompressionType, dstCompressionType );

        assert( gotComprType == true );

        hasAlpha = nativeTex->hasAlpha;

        dstTexelsOut = texels;
        dstDataSizeOut = dataSize;

        // We may or may not need to swizzle.
        isNewlyAllocatedOut = isNewlyAllocated;
        isPaletteNewlyAllocated = false;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureXBOX::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        uint32 rowAlignment,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        // Check whether the texture is swizzled.
        eRasterFormat texRasterFormat = nativeTex->rasterFormat;
        uint32 texDepth = nativeTex->depth;
        ePaletteType texPaletteType = nativeTex->paletteType;
        uint32 texCompressionType = nativeTex->dxtCompression;
        
        bool isSwizzled = isXBOXTextureSwizzled( texRasterFormat, texDepth, texPaletteType, texCompressionType );

        bool hasNewlyAllocated = false;

        // Convert the texels into our appropriate format.
        {
            eCompressionType rwCompressionType;
            {
                bool gotCompressionType = getDXTCompressionTypeFromXBOX( texCompressionType, rwCompressionType );

                if ( !gotCompressionType )
                {
                    throw NativeTextureInternalErrorException( "XBOX", L"XBOX_INTERNERR_UNKCOMPRTYPE" );
                }
            }

            bool hasChanged = ConvertMipmapLayerNative(
                engineInterface,
                width, height, layerWidth, layerHeight, srcTexels, dataSize,
                rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                texRasterFormat, texDepth, getXBOXTextureDataRowAlignment(), nativeTex->colorOrder,
                texPaletteType, nativeTex->palette, nativeTex->paletteSize,
                rwCompressionType,
                false,
                width, height,
                srcTexels, dataSize
            );

            hasNewlyAllocated = hasChanged;
        }

        // If we are swizzled, we also must swizzle all input textures.
        if ( isSwizzled )
        {
            NativeTextureXBOX::swizzleMipmapTraversal swizzleTrav;

            swizzleTrav.mipWidth = width;
            swizzleTrav.mipHeight = height;
            swizzleTrav.depth = texDepth;
            swizzleTrav.rowAlignment = getXBOXTextureDataRowAlignment();
            swizzleTrav.texels = srcTexels;
            swizzleTrav.dataSize = dataSize;

            NativeTextureXBOX::swizzleMipmap( engineInterface, swizzleTrav );

            if ( hasNewlyAllocated )
            {
                engineInterface->PixelFree( srcTexels );
            }

            width = swizzleTrav.newWidth;
            height = swizzleTrav.newHeight;
            srcTexels = swizzleTrav.newtexels;
            dataSize = swizzleTrav.newDataSize;

            hasNewlyAllocated = true;
        }

        // We have no more auto mipmaps.
        nativeTex->autoMipmaps = false;

        // Store the encoded mipmap data.
        mipLayer.width = width;
        mipLayer.height = height;
        mipLayer.layerWidth = layerWidth;
        mipLayer.layerHeight = layerHeight;
        mipLayer.texels = srcTexels;
        mipLayer.dataSize = dataSize;

        hasDirectlyAcquiredOut = ( hasNewlyAllocated == false );
    }
};

bool xboxNativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

    xboxMipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureXBOX::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool xboxNativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

    xboxMipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureXBOX::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void xboxNativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

    virtualClearMipmaps <NativeTextureXBOX::mipmapLayer> (
        engineInterface, nativeTex->mipmaps
    );
}

void xboxNativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

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

void xboxNativeTextureTypeProvider::GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

    // We can be DXT compressed or swizzled into a default raster format.
    rwString <char> formatString( eir::constr_with_alloc::DEFAULT, engineInterface );
    formatString = "XBOX";

    int xboxCompressionType = nativeTex->dxtCompression;

    if ( xboxCompressionType != 0 )
    {
        eCompressionType rwCompressionType;

        bool hasKnownCompression = false;
        
        bool gotType = getDXTCompressionTypeFromXBOX( xboxCompressionType, rwCompressionType );

        if ( gotType )
        {
            if ( rwCompressionType == RWCOMPRESS_DXT1 )
            {
                formatString += " DXT1";

                hasKnownCompression = true;
            }
            else if ( rwCompressionType == RWCOMPRESS_DXT2 )
            {
                formatString += " DXT2";

                hasKnownCompression = true;
            }
            else if ( rwCompressionType == RWCOMPRESS_DXT3 )
            {
                formatString += " DXT3";

                hasKnownCompression = true;
            }
            else if ( rwCompressionType == RWCOMPRESS_DXT4 )
            {
                formatString += " DXT4";

                hasKnownCompression = true;
            }
            else if ( rwCompressionType == RWCOMPRESS_DXT5 )
            {
                formatString += " DXT5";

                hasKnownCompression = true;
            }
        }
        
        if ( !hasKnownCompression )
        {
            formatString += "compressed (unk)";
        }
    }
    else
    {
        formatString += " ";

        // Just a default raster type.
        getDefaultRasterFormatString( nativeTex->rasterFormat, nativeTex->depth, nativeTex->paletteType, nativeTex->colorOrder, formatString );
    }

    if ( buf )
    {
        strncpy( buf, formatString.GetConstString(), bufLen );
    }

    lengthOut = formatString.GetLength();
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_XBOX