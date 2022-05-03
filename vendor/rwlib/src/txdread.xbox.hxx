/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.xbox.hxx
*  PURPOSE:     XBOX native texture internal header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_XBOX_NATIVETEX_MAIN_INCLUDE_
#define _RENDERWARE_XBOX_NATIVETEX_MAIN_INCLUDE_

#ifdef RWLIB_INCLUDE_NATIVETEX_XBOX

#include "txdread.nativetex.hxx"

#include "txdread.d3d.genmip.hxx"

#include "txdread.common.hxx"

#define NATIVE_TEXTURE_XBOX     5

namespace rw
{

inline uint32 getXBOXTextureDataRowAlignment( void )
{
    return sizeof( uint32 );
}

inline uint32 getXBOXCompressionTypeFromRW( eCompressionType rwCompressionType )
{
    uint32 xboxCompressionType = 0;

    if ( rwCompressionType != RWCOMPRESS_NONE )
    {
        if ( rwCompressionType == RWCOMPRESS_DXT1 )
        {
            xboxCompressionType = 0xC;
        }
        else if ( rwCompressionType == RWCOMPRESS_DXT2 )
        {
            xboxCompressionType = 0xD;
        }
        else if ( rwCompressionType == RWCOMPRESS_DXT3 )
        {
            xboxCompressionType = 0xE;
        }
        else if ( rwCompressionType == RWCOMPRESS_DXT5 )
        {
            xboxCompressionType = 0xF;
        }
        else if ( rwCompressionType == RWCOMPRESS_DXT4 )
        {
            xboxCompressionType = 0x10;
        }
        else
        {
            assert( 0 );
        }
    }

    return xboxCompressionType;
}

inline bool isXBOXTextureSwizzled(
    eRasterFormat rasterFormat, uint32 depth, ePaletteType paletteType, uint32 xboxCompressionType
)
{
    // TODO: verify if this swizzling is indeed like this.

    return ( xboxCompressionType == 0 );
}

struct NativeTextureXBOX
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureXBOX( Interface *engineInterface )
    {
        // Initialize the texture object.
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->palette = nullptr;
        this->paletteSize = 0;
        this->paletteType = PALETTE_NONE;
        this->rasterFormat = RASTER_DEFAULT;
        this->depth = 0;
        this->dxtCompression = 0;
        this->hasAlpha = false;
        this->isCubeMap = false;
        this->colorOrder = COLOR_BGRA;
        this->rasterType = 4;   // by default it is a texture raster.
        this->autoMipmaps = false;
    }

    inline NativeTextureXBOX( const NativeTextureXBOX& right )
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = right.engineInterface;
        this->texVersion = right.texVersion;

        // Copy palette information.
        {
	        if (right.palette)
            {
                uint32 palRasterDepth = Bitmap::getRasterFormatDepth(right.rasterFormat);

                size_t wholeDataSize = getPaletteDataSize( right.paletteSize, palRasterDepth );

		        this->palette = engineInterface->PixelAllocate( wholeDataSize );

		        memcpy(this->palette, right.palette, wholeDataSize);
	        }
            else
            {
		        this->palette = nullptr;
	        }

            this->paletteSize = right.paletteSize;
            this->paletteType = right.paletteType;
        }

        // Copy image texel information.
        {
            copyMipmapLayers( engineInterface, right.mipmaps, this->mipmaps );

            // Copy over attributes.
            this->rasterFormat = right.rasterFormat;
            this->depth = right.depth;
        }

        this->dxtCompression = right.dxtCompression;
        this->hasAlpha = right.hasAlpha;
        this->isCubeMap = right.isCubeMap;

        this->colorOrder = right.colorOrder;
        this->rasterType = right.rasterType;
        this->autoMipmaps = right.autoMipmaps;
    }

    inline void clearTexelData( void )
    {
        if ( this->palette )
        {
	        this->engineInterface->PixelFree( palette );

	        palette = nullptr;
        }

	    deleteMipmapLayers( this->engineInterface, this->mipmaps );

        this->mipmaps.Clear();
    }

    inline ~NativeTextureXBOX( void )
    {
        this->clearTexelData();
    }

    eRasterFormat rasterFormat;

    uint32 depth;

    typedef genmip::mipmapLayer mipmapLayer;

    DEFINE_HEAP_REDIR_ALLOC_IMPL( mipRedirAlloc );

    eir::Vector <mipmapLayer, mipRedirAlloc> mipmaps;

	void *palette;
	uint32 paletteSize;

    ePaletteType paletteType;

    uint8 rasterType;

    eColorOrdering colorOrder;

    bool hasAlpha;
    bool isCubeMap;

    bool autoMipmaps;

	// PC/XBOX
	uint32 dxtCompression;

    struct swizzleMipmapTraversal
    {
        // Input.
        uint32 mipWidth, mipHeight;
        uint32 depth;
        uint32 rowAlignment;
        void *texels;
        uint32 dataSize;

        // Output.
        uint32 newWidth, newHeight;
        void *newtexels;
        uint32 newDataSize;
    };

    static void swizzleMipmap( Interface *engineInterface, swizzleMipmapTraversal& pixelData );
    static void unswizzleMipmap( Interface *engineInterface, swizzleMipmapTraversal& pixelData );
};

RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( NativeTextureXBOX, mipRedirAlloc, mipmaps, engineInterface )

inline bool getDXTCompressionTypeFromXBOX( uint32 xboxCompressionType, eCompressionType& typeOut )
{
    eCompressionType rwCompressionType = RWCOMPRESS_NONE;

    if ( xboxCompressionType != 0 )
    {
        if ( xboxCompressionType == 0xc )
        {
            rwCompressionType = RWCOMPRESS_DXT1;
        }
        else if ( xboxCompressionType == 0xd )
        {
            rwCompressionType = RWCOMPRESS_DXT2;
        }
        else if ( xboxCompressionType == 0xe )
        {
            rwCompressionType = RWCOMPRESS_DXT3;
        }
        else if ( xboxCompressionType == 0xf )
        {
            rwCompressionType = RWCOMPRESS_DXT5;
        }
        else if ( xboxCompressionType == 0x10 )
        {
            rwCompressionType = RWCOMPRESS_DXT4;
        }
        else
        {
            // We have no idea.
            return false;
        }
    }

    typeOut = rwCompressionType;
    return true;
}

inline void getXBOXNativeTextureSizeRules( nativeTextureSizeRules& rulesOut )
{
    rulesOut.powerOfTwo = true;
    rulesOut.squared = false;
    rulesOut.maximum = true;
    rulesOut.maxVal = 1024;
}

struct xboxNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        new (objMem) NativeTextureXBOX( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize )
    {
        new (objMem) NativeTextureXBOX( *(const NativeTextureXBOX*)srcObjMem );
    }

    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        ( *(NativeTextureXBOX*)objMem ).~NativeTextureXBOX();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const
    {
        capsOut.supportsDXT1 = true;
        capsOut.supportsDXT2 = true;
        capsOut.supportsDXT3 = true;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = true;
        capsOut.supportsPalette = true;
    }

    void GetStorageCapabilities( storageCapabilities& storeCaps ) const
    {
        storeCaps.pixelCaps.supportsDXT1 = true;
        storeCaps.pixelCaps.supportsDXT2 = true;
        storeCaps.pixelCaps.supportsDXT3 = true;
        storeCaps.pixelCaps.supportsDXT4 = false;
        storeCaps.pixelCaps.supportsDXT5 = true;
        storeCaps.pixelCaps.supportsPalette = true;

        storeCaps.isCompressedFormat = false;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut );
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut );
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate );

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version )
    {
        NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem )
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return nativeTex->rasterFormat;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return nativeTex->paletteType;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return ( nativeTex->dxtCompression != 0 );
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        eCompressionType actualType;

        bool gotType = getDXTCompressionTypeFromXBOX( nativeTex->dxtCompression, actualType );

        if ( !gotType )
        {
            throw NativeTextureInternalErrorException( "XBOX", L"XBOX_INTERNERR_UNKCOMPRTYPE" );
        }

        return actualType;
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // The XBOX is assumed to have the same row alignment like Direct3D 8.
        // This means it is aligned to DWORD.
        return getXBOXTextureDataRowAlignment();
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        getXBOXNativeTextureSizeRules( rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // The XBOX native texture is a very old format, derived from very limited hardware.
        // So we restrict it very firmly, in comparison to Direct3D 8 and 9.
        getXBOXNativeTextureSizeRules( rulesOut );
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // Always the generic XBOX driver.
        return 8;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "XBOX", this, sizeof( NativeTextureXBOX ), alignof( NativeTextureXBOX ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "XBOX" );
    }
};

namespace xbox
{

inline void convertCompatibleRasterFormat(
    eRasterFormat& rasterFormatOut, uint32& depthOut, eColorOrdering& colorOrderOut,
    ePaletteType& paletteTypeOut
)
{
    eRasterFormat srcRasterFormat = rasterFormatOut;
    //uint32 srcDepth = depthOut;
    //eColorOrdering srcColorOrder = colorOrderOut;
    ePaletteType srcPaletteType = paletteTypeOut;

    if ( srcPaletteType != PALETTE_NONE )
    {
        if ( srcPaletteType == PALETTE_4BIT || srcPaletteType == PALETTE_8BIT )
        {
            depthOut = 8;
        }
        else if ( srcPaletteType == PALETTE_4BIT_LSB )
        {
            // We better remap it to the proper order.
            depthOut = 8;

            paletteTypeOut = PALETTE_4BIT;
        }
        else
        {
            assert( 0 );
        }

        // Make sure we have a supported palette raster format.
        // We do not care about advanced raster formats on console architecture.
        bool hasValidPaletteRasterFormat = false;

        if ( srcRasterFormat == RASTER_8888 ||
             srcRasterFormat == RASTER_888 )
        {
            hasValidPaletteRasterFormat = true;
        }

        if ( !hasValidPaletteRasterFormat )
        {
            // Anything invalid should be expanded to full color.
            rasterFormatOut = RASTER_8888;
        }
    }
    else
    {
        if ( srcRasterFormat == RASTER_1555 )
        {
            depthOut = 16;
        }
        else if ( srcRasterFormat == RASTER_565 )
        {
            depthOut = 16;
        }
        else if ( srcRasterFormat == RASTER_4444 )
        {
            depthOut = 16;
        }
        else if ( srcRasterFormat == RASTER_8888 )
        {
            depthOut = 32;
        }
        else if ( srcRasterFormat == RASTER_888 )
        {
            depthOut = 32;
        }
        else if ( srcRasterFormat == RASTER_555 )
        {
            depthOut = 16;
        }
        else if ( srcRasterFormat == RASTER_LUM )
        {
            depthOut = 8;
        }
        else
        {
            // Unsupported/unknown format, we have to convert it to something safe.
            rasterFormatOut = RASTER_8888;
            depthOut = 32;
        }
    }

    // XBOX textures always have BGRA color order, no matter if palette.
    colorOrderOut = COLOR_BGRA;
}

#pragma pack(1)
struct textureMetaHeaderStruct
{
    rw::texFormatInfo_serialized <endian::p_little_endian <uint32>> formatInfo;

    char name[32];
    char maskName[32];

    endian::p_little_endian <uint32> rasterFormat;
    endian::p_little_endian <uint16> hasAlpha;
    endian::p_little_endian <uint16> isCubeMap;
    endian::p_little_endian <uint16> width, height;

    uint8 depth;
    uint8 mipmapCount;
    uint8 rasterType;
    uint8 dxtCompression;

    endian::p_little_endian <uint32> imageDataSectionSize;
};
#pragma pack()

}

};

#endif //RWLIB_INCLUDE_NATIVETEX_XBOX

#endif //_RENDERWARE_XBOX_NATIVETEX_MAIN_INCLUDE_
