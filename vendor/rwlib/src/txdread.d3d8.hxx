/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.d3d8.hxx
*  PURPOSE:     Direct3D 8 native texture internal header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_D3D8_NATIVETEX_MAIN_INCLUDE_
#define _RENDERWARE_D3D8_NATIVETEX_MAIN_INCLUDE_

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D8

#include "txdread.d3d.hxx"

#include "txdread.common.hxx"

#define PLATFORM_D3D8   8

namespace rw
{

struct NativeTextureD3D8
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureD3D8( Interface *engineInterface )
    {
        // Initialize the texture object.
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->palette = nullptr;
        this->paletteSize = 0;
        this->paletteType = PALETTE_NONE;
        this->rasterFormat = RASTER_8888;
        this->depth = 0;
        this->autoMipmaps = false;
        this->dxtCompression = 0;
        this->rasterType = 4;
        this->hasAlpha = true;
        this->colorOrdering = COLOR_BGRA;
    }

    inline NativeTextureD3D8( const NativeTextureD3D8& right )
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = engineInterface;
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

            this->rasterFormat = right.rasterFormat;
            this->depth = right.depth;
        }

        this->autoMipmaps =         right.autoMipmaps;
        this->dxtCompression =      right.dxtCompression;
        this->rasterType =          right.rasterType;
        this->hasAlpha =            right.hasAlpha;
        this->colorOrdering =       right.colorOrdering;
    }

    inline void clearTexelData( void )
    {
        if ( this->palette )
        {
	        this->engineInterface->PixelFree( palette );

	        palette = nullptr;
        }

        deleteMipmapLayers( this->engineInterface, this->mipmaps );
    }

    inline ~NativeTextureD3D8( void )
    {
        this->clearTexelData();
    }

    inline static void getSizeRules( uint32 dxtType, nativeTextureSizeRules& rulesOut )
    {
        bool isCompressed = ( dxtType != 0 );

        rulesOut.powerOfTwo = false;
        rulesOut.squared = false;
        rulesOut.multipleOf = isCompressed;
        rulesOut.multipleOfValue = ( isCompressed ? 4u : 0u );
        rulesOut.maximum = true;
        rulesOut.maxVal = 4096;
    }

public:
    typedef genmip::mipmapLayer mipmapLayer;

    eRasterFormat rasterFormat;

    uint32 depth;

    DEFINE_HEAP_REDIR_ALLOC_IMPL( mipRedirAlloc );

	eir::Vector <mipmapLayer, mipRedirAlloc> mipmaps;

	void *palette;
	uint32 paletteSize;

    ePaletteType paletteType;

	// PC/XBOX
    bool autoMipmaps;

    uint32 dxtCompression;
    uint32 rasterType;

    bool hasAlpha;

    eColorOrdering colorOrdering;
};

RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( NativeTextureD3D8, mipRedirAlloc, mipmaps, engineInterface )

inline eCompressionType getD3DCompressionType( const NativeTextureD3D8 *nativeTex )
{
    eCompressionType rwCompressionType = RWCOMPRESS_NONE;

    uint32 dxtType = nativeTex->dxtCompression;

    if ( dxtType != 0 )
    {
        if ( dxtType == 1 )
        {
            rwCompressionType = RWCOMPRESS_DXT1;
        }
        else if ( dxtType == 2 )
        {
            rwCompressionType = RWCOMPRESS_DXT2;
        }
        else if ( dxtType == 3 )
        {
            rwCompressionType = RWCOMPRESS_DXT3;
        }
        else if ( dxtType == 4 )
        {
            rwCompressionType = RWCOMPRESS_DXT4;
        }
        else if ( dxtType == 5 )
        {
            rwCompressionType = RWCOMPRESS_DXT5;
        }
        else
        {
            throw NativeTextureInternalErrorException( "Direct3D8", L"D3D8_INTERNERR_COMPRTYPE" );
        }
    }

    return rwCompressionType;
}

struct d3d8NativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        new (objMem) NativeTextureD3D8( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) override
    {
        new (objMem) NativeTextureD3D8( *(const NativeTextureD3D8*)srcObjMem );
    }

    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        ( *(NativeTextureD3D8*)objMem ).~NativeTextureD3D8();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const override
    {
        capsOut.supportsDXT1 = true;
        capsOut.supportsDXT2 = true;
        capsOut.supportsDXT3 = true;
        capsOut.supportsDXT4 = true;
        capsOut.supportsDXT5 = true;
        capsOut.supportsPalette = true;
    }

    void GetStorageCapabilities( storageCapabilities& storeCaps ) const override
    {
        storeCaps.pixelCaps.supportsDXT1 = true;
        storeCaps.pixelCaps.supportsDXT2 = true;
        storeCaps.pixelCaps.supportsDXT3 = true;
        storeCaps.pixelCaps.supportsDXT4 = true;
        storeCaps.pixelCaps.supportsDXT5 = true;
        storeCaps.pixelCaps.supportsPalette = true;

        storeCaps.isCompressedFormat = false;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut );
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut );
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate );

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version ) override
    {
        NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void* GetNativeInterface( void *objMem ) override
    {
        // TODO.
        return nullptr;
    }

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return nativeTex->rasterFormat;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return nativeTex->paletteType;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return ( nativeTex->dxtCompression != 0 );
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return getD3DCompressionType( nativeTex );
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // Direct3D 8 and 9 work with DWORD aligned texture data rows.
        // We found this out when looking at the return values of GetLevelDesc.
        return getD3DTextureDataRowAlignment();
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        // We want to know ahead of time before passing in our pixel data what size rules we have to obey.
        eCompressionType compressionType = format.compressionType;

        uint32 dxtType = 0;

        if ( compressionType == RWCOMPRESS_DXT1 )
        {
            dxtType = 1;
        }
        else if ( compressionType == RWCOMPRESS_DXT2 )
        {
            dxtType = 2;
        }
        else if ( compressionType == RWCOMPRESS_DXT3 )
        {
            dxtType = 3;
        }
        else if ( compressionType == RWCOMPRESS_DXT4 )
        {
            dxtType = 4;
        }
        else if ( compressionType == RWCOMPRESS_DXT5 )
        {
            dxtType = 5;
        }

        NativeTextureD3D8::getSizeRules( dxtType, rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // Even though some hardware may not support it, we are very liberal here, to prefer the future.
        // Only restriction is for instance DXT, which says it MUST be power-of-two.
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        NativeTextureD3D8::getSizeRules( nativeTex->dxtCompression, rulesOut );
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // Direct3D 8 driver.
        return 1;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "Direct3D8", this, sizeof( NativeTextureD3D8 ), alignof( NativeTextureD3D8 ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "Direct3D8" );
    }
};

namespace d3d8
{

inline void convertCompatibleRasterFormat(
    bool desireWorkingFormat,
    eRasterFormat& rasterFormat, eColorOrdering& colorOrder, uint32& depth, ePaletteType& paletteType
)
{
    eRasterFormat srcRasterFormat = rasterFormat;
    //eColorOrdering srcColorOrder = colorOrder;
    //uint32 srcDepth = depth;
    ePaletteType srcPaletteType = paletteType;

    if ( srcPaletteType != PALETTE_NONE )
    {
        if ( srcPaletteType == PALETTE_4BIT || srcPaletteType == PALETTE_8BIT )
        {
            // We only support 8bit depth palette.
            depth = 8;
        }
        else if ( srcPaletteType == PALETTE_4BIT_LSB )
        {
            depth = 8;

            // We must reorder the palette.
            paletteType = PALETTE_4BIT;
        }
        else
        {
            assert( 0 );
        }

        // All palettes have RGBA color order.
        colorOrder = COLOR_RGBA;

        // Also verify raster formats.
        // Should be fairly similar to XBOX compatibility.
        bool hasValidPaletteRasterFormat = false;

        if ( srcRasterFormat == RASTER_8888 ||
             srcRasterFormat == RASTER_888 )
        {
            hasValidPaletteRasterFormat = true;
        }

        // We can allow more complicated types if compatibility of old
        // implementations is not desired.
        if ( desireWorkingFormat == false )
        {
            if ( srcRasterFormat == RASTER_1555 ||
                 srcRasterFormat == RASTER_565 ||
                 srcRasterFormat == RASTER_4444 ||
                 srcRasterFormat == RASTER_LUM ||
                 srcRasterFormat == RASTER_8888 ||
                 srcRasterFormat == RASTER_888 ||
                 srcRasterFormat == RASTER_555 )
            {
                // Allow those more advanced palette raster formats.
                hasValidPaletteRasterFormat = true;
            }
        }

        if ( !hasValidPaletteRasterFormat )
        {
            // Anything invalid should be expanded to full color.
            rasterFormat = RASTER_8888;
        }
    }
    else
    {
        if ( srcRasterFormat == RASTER_1555 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_565 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_4444 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_8888 )
        {
            depth = 32;

            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_888 )
        {
            depth = 32;
            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_555 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_LUM )
        {
            // We only support 8bit LUM, actually.
            depth = 8;
        }
        else
        {
            // Any unknown raster formats need conversion to full quality.
            rasterFormat = RASTER_8888;
            depth = 32;
            colorOrder = COLOR_BGRA;
        }
    }
}

#pragma pack(1)
struct textureMetaHeaderStructGeneric
{
    endian::p_little_endian <uint32> platformDescriptor;

    rw::texFormatInfo_serialized <endian::p_little_endian <uint32>> texFormat;

    char name[32];
    char maskName[32];

    endian::p_little_endian <uint32> rasterFormat;
    endian::p_little_endian <uint32> hasAlpha;

    endian::p_little_endian <uint16> width;
    endian::p_little_endian <uint16> height;
    uint8 depth;
    uint8 mipmapCount;
    uint8 rasterType : 3;
    uint8 pad1 : 5;
    uint8 dxtCompression;
};
#pragma pack()

};

}

#endif //RWLIB_INCLUDE_NATIVETEX_D3D8

#endif //_RENDERWARE_D3D8_NATIVETEX_MAIN_INCLUDE_
