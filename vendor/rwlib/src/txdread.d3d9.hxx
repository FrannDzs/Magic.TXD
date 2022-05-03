/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.d3d9.hxx
*  PURPOSE:     Direct3D 9 native texture internal header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_D3D9_NATIVETEX_MAIN_HEADER_
#define _RENDERWARE_D3D9_NATIVETEX_MAIN_HEADER_

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9

#include "txdread.d3d.hxx"

#include "txdread.d3d.dxt.hxx"

#include "txdread.common.hxx"

#define PLATFORM_D3D9   9

namespace rw
{

inline bool getD3DFormatFromRasterType(eRasterFormat rasterFormat, ePaletteType paletteType, eColorOrdering colorOrder, uint32 itemDepth, D3DFORMAT& d3dFormat)
{
    bool hasFormat = false;

    if ( paletteType != PALETTE_NONE )
    {
        if ( itemDepth == 8 )
        {
            if (colorOrder == COLOR_RGBA)
            {
                d3dFormat = D3DFMT_P8;

                hasFormat = true;
            }
        }
    }
    else
    {
        if ( rasterFormat == RASTER_1555 )
        {
            if ( itemDepth == 16 )
            {
                if (colorOrder == COLOR_BGRA)
                {
                    d3dFormat = D3DFMT_A1R5G5B5;

                    hasFormat = true;
                }
            }
        }
        else if ( rasterFormat == RASTER_565 )
        {
            if ( itemDepth == 16 )
            {
                if (colorOrder == COLOR_BGRA)
                {
                    d3dFormat = D3DFMT_R5G6B5;

                    hasFormat = true;
                }
            }
        }
        else if ( rasterFormat == RASTER_4444 )
        {
            if ( itemDepth == 16 )
            {
                if (colorOrder == COLOR_BGRA)
                {
                    d3dFormat = D3DFMT_A4R4G4B4;

                    hasFormat = true;
                }
            }
        }
        else if ( rasterFormat == RASTER_LUM )
        {
            if ( itemDepth == 8 )
            {
                d3dFormat = D3DFMT_L8;

                hasFormat = true;
            }
            // there is also 4bit LUM, but as you see it is not supported by D3D.
        }
        else if ( rasterFormat == RASTER_8888 )
        {
            if ( itemDepth == 32 )
            {
                if (colorOrder == COLOR_BGRA)
                {
                    d3dFormat = D3DFMT_A8R8G8B8;

                    hasFormat = true;
                }
                else if (colorOrder == COLOR_RGBA)
                {
                    d3dFormat = D3DFMT_A8B8G8R8;

                    hasFormat = true;
                }
            }
        }
        else if ( rasterFormat == RASTER_888 )
        {
            if (colorOrder == COLOR_BGRA)
            {
                if ( itemDepth == 32 )
                {
                    d3dFormat = D3DFMT_X8R8G8B8;

                    hasFormat = true;
                }
                else if ( itemDepth == 24 )
                {
                    d3dFormat = D3DFMT_R8G8B8;

                    hasFormat = true;
                }
            }
            else if (colorOrder == COLOR_RGBA)
            {
                if ( itemDepth == 32 )
                {
                    d3dFormat = D3DFMT_X8B8G8R8;

                    hasFormat = true;
                }
            }
        }
        else if ( rasterFormat == RASTER_555 )
        {
            if ( itemDepth == 16 )
            {
                if (colorOrder == COLOR_BGRA)
                {
                    d3dFormat = D3DFMT_X1R5G5B5;

                    hasFormat = true;
                }
            }
        }
        // NEW formats :3
        else if ( rasterFormat == RASTER_LUM_ALPHA )
        {
            if ( itemDepth == 8 )
            {
                d3dFormat = D3DFMT_A4L4;

                hasFormat = true;
            }
            else if ( itemDepth == 16 )
            {
                d3dFormat = D3DFMT_A8L8;

                hasFormat = true;
            }
        }
    }

    return hasFormat;
}

inline bool isRasterFormatOriginalRWCompatible(
    eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth, ePaletteType paletteType
)
{
    // This function assumes that the format that it gets is compatible with the Direct3D 9 native texture.
    // For anything else the result is undefined behavior.

    if ( paletteType != PALETTE_NONE )
    {
        return true;
    }
    else
    {
        if ( rasterFormat == RASTER_1555 && depth == 16 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_565 && depth == 16 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_4444 && depth == 16 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_LUM && depth == 8 )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_8888 && depth == 32 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_888 && depth == 32 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_16 && depth == 16 )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_24 && depth == 24 )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_32 && depth == 32 )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_555 && depth == 16 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
    }

    return false;
}

struct NativeTextureD3D9 : public d3dpublic::d3dNativeTextureInterface
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureD3D9( Interface *engineInterface )
    {
        // Initialize the texture object.
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->palette = nullptr;
        this->paletteSize = 0;
        this->paletteType = PALETTE_NONE;
        this->rasterFormat = RASTER_8888;
        this->depth = 0;
        this->isCubeTexture = false;
        this->autoMipmaps = false;
        this->d3dFormat = D3DFMT_A8R8G8B8;
        this->d3dRasterFormatLink = false;
        this->isOriginalRWCompatible = true;
        this->anonymousFormatLink = nullptr;
        this->colorComprType = RWCOMPRESS_NONE;
        this->rasterType = 4;
        this->hasAlpha = true;
        this->colorOrdering = COLOR_BGRA;
    }

    inline NativeTextureD3D9( const NativeTextureD3D9& right )
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

        this->isCubeTexture =           right.isCubeTexture;
        this->autoMipmaps =             right.autoMipmaps;
        this->d3dFormat =               right.d3dFormat;
        this->d3dRasterFormatLink =     right.d3dRasterFormatLink;
        this->isOriginalRWCompatible =  right.isOriginalRWCompatible;
        this->anonymousFormatLink =     right.anonymousFormatLink;
        this->colorComprType =          right.colorComprType;
        this->rasterType =              right.rasterType;
        this->hasAlpha =                right.hasAlpha;
        this->colorOrdering =           right.colorOrdering;
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

    inline ~NativeTextureD3D9( void )
    {
        this->clearTexelData();
    }

    inline static void getSizeRules( eCompressionType comprType, nativeTextureSizeRules& rulesOut )
    {
        uint32 dxtType; // unused.

        bool isDXTCompressed = IsDXTCompressionType( comprType, dxtType );

        rulesOut.powerOfTwo = false;
        rulesOut.squared = false;
        rulesOut.multipleOf = isDXTCompressed;
        rulesOut.multipleOfValue = ( isDXTCompressed ? 4u : 0u );
        rulesOut.maximum = true;
        rulesOut.maxVal = 4096;
    }

    // Implement the public API.

    void GetD3DFormat( uint32& d3dFormat ) const override
    {
        d3dFormat = (uint32)this->d3dFormat;
    }

    // PUBLIC API END

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
    bool isCubeTexture;
    bool autoMipmaps;

    D3DFORMAT d3dFormat;
    eCompressionType colorComprType;
    uint32 rasterType;

    bool d3dRasterFormatLink;
    bool isOriginalRWCompatible;

    d3dpublic::nativeTextureFormatHandler *anonymousFormatLink;

    inline bool IsRWCompatible( void ) const
    {
        // This function returns whether we can push our data to the RW implementation.
        // We cannot push anything to RW that we have no idea about how it actually looks like.
        return ( this->d3dRasterFormatLink == true || this->colorComprType != RWCOMPRESS_NONE );
    }

    bool hasAlpha;

    eColorOrdering colorOrdering;
};

RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( NativeTextureD3D9, mipRedirAlloc, mipmaps, engineInterface )

inline eCompressionType getD3DCompressionType( const NativeTextureD3D9 *nativeTex )
{
    return nativeTex->colorComprType;
}

struct d3d9NativeTextureTypeProvider : public texNativeTypeProvider, d3dpublic::d3dNativeTextureDriverInterface
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        new (objMem) NativeTextureD3D9( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) override
    {
        new (objMem) NativeTextureD3D9( *(const NativeTextureD3D9*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        ( *(NativeTextureD3D9*)objMem ).~NativeTextureD3D9();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const override;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const override;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const override;

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

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut ) override;
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut ) override;
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate ) override;

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version ) override
    {
        NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut ) override;
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut ) override;
    void ClearMipmaps( Interface *engineInterface, void *objMem ) override;

    void* GetNativeInterface( void *objMem ) override
    {
        NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

        // The native interface is part of the texture.
        d3dpublic::d3dNativeTextureInterface *nativeAPI = nativeTex;

        return (void*)nativeAPI;
    }

    void* GetDriverNativeInterface( void ) const override
    {
        d3dpublic::d3dNativeTextureDriverInterface *nativeDriver = (d3dpublic::d3dNativeTextureDriverInterface*)this;

        // We do export a public driver API.
        // It is the most direct way to address the D3D9 native texture environment.
        return nativeDriver;
    }

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut ) override;
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const override;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        // If we are not a raster format texture, we have no raster format, yea.
        if ( !nativeTex->d3dRasterFormatLink )
            return RASTER_DEFAULT;

        return nativeTex->rasterFormat;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        // If we are not a raster format texture, we have no palette type-
        if ( !nativeTex->d3dRasterFormatLink )
            return PALETTE_NONE;

        return nativeTex->paletteType;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        return ( nativeTex->colorComprType != RWCOMPRESS_NONE );
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        return getD3DCompressionType( nativeTex );
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // Direct3D 8 and 9 work with DWORD aligned texture data rows.
        // We found this out when looking at the return values of GetLevelDesc.
        // By the way, the way RenderWare reads texture data into memory is optimized and flawed.
        // https://msdn.microsoft.com/en-us/library/windows/desktop/bb206357%28v=vs.85%29.aspx
        // According to the docs, the alignment may always change, the driver has liberty to do anything.
        // We just conform to what Rockstar and Criterion have thought about here, anyway.
        // I believe that ATI and nVidia have recogized this issue and made sure the alignment is constant!
        return getD3DTextureDataRowAlignment();
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        // We want to know ahead of time before passing in our pixel data what size rules we have to obey.
        NativeTextureD3D9::getSizeRules( format.compressionType, rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // The rules here are very similar to Direct3D 8.
        // When we add support for cubemaps, we will have to respect them aswell...
        // But that is an issue for another day!
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        NativeTextureD3D9::getSizeRules( nativeTex->colorComprType, rulesOut );
    }

    void GetRecommendedRasterFormat( eRasterFormat rasterFormat, ePaletteType paletteType, uint32& recDepth, bool& hasRecDepth, eColorOrdering& recColorOrder, bool& hasRecColorOrder ) const override
    {
        // There are some pitfalls we want to avoid.
        // Basically, let us use BGRA color ordering all the time.
        hasRecColorOrder = false;
        hasRecDepth = false;

        if ( paletteType == PALETTE_NONE )
        {
            if ( rasterFormat == RASTER_1555 ||
                 rasterFormat == RASTER_565 ||
                 rasterFormat == RASTER_4444 ||
                 rasterFormat == RASTER_8888 ||
                 rasterFormat == RASTER_888 ||
                 rasterFormat == RASTER_555 )
            {
                recColorOrder = COLOR_BGRA;

                hasRecColorOrder = true;
            }

            if ( rasterFormat == RASTER_888 )
            {
                recDepth = 32;

                hasRecDepth = true;
            }
        }
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // We are the Direct3D 9 driver.
        return 2;
    }

    // PUBLIC API begin

    d3dpublic::nativeTextureFormatHandler* GetFormatHandler( D3DFORMAT format ) const
    {
        size_t numExtensions = this->formatExtensions.GetCount();

        for ( size_t n = 0; n < numExtensions; n++ )
        {
            const nativeFormatExtension& ext = this->formatExtensions[ n ];

            if ( ext.theFormat == format )
            {
                return ext.handler;
            }
        }

        return nullptr;
    }

    bool RegisterFormatHandler( uint32 format, d3dpublic::nativeTextureFormatHandler *handler ) override;
    bool UnregisterFormatHandler( uint32 format ) override;

    struct nativeFormatExtension
    {
        D3DFORMAT theFormat;
        d3dpublic::nativeTextureFormatHandler *handler;
    };

    rwVector <nativeFormatExtension> formatExtensions;

    // PUBLIC API end

    inline d3d9NativeTextureTypeProvider( EngineInterface *natEngine ) : formatExtensions( eir::constr_with_alloc::DEFAULT, natEngine )
    {
        return;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "Direct3D9", this, sizeof( NativeTextureD3D9 ), alignof( NativeTextureD3D9 ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "Direct3D9" );
    }
};

namespace d3d9
{

inline void convertCompatiblePaletteType(
    uint32& depth, ePaletteType& paletteType
)
{
    ePaletteType srcPaletteType = paletteType;

    if ( srcPaletteType == PALETTE_4BIT || srcPaletteType == PALETTE_8BIT )
    {
        // We only support 8bit depth palette.
        depth = 8;
    }
    else if ( srcPaletteType == PALETTE_4BIT_LSB )
    {
        depth = 8;

        // Make sure we reorder the palette.
        paletteType = PALETTE_4BIT;
    }
    else
    {
        assert( 0 );
    }
}

inline void convertCompatibleRasterFormat(
    bool desireWorkingFormat,
    eRasterFormat& rasterFormat, eColorOrdering& colorOrder, uint32& depth, ePaletteType& paletteType, D3DFORMAT& d3dFormatOut
)
{
    ePaletteType srcPaletteType = paletteType;

    if ( srcPaletteType != PALETTE_NONE )
    {
        convertCompatiblePaletteType( depth, paletteType );

        eRasterFormat srcRasterFormat = rasterFormat;

        // TODO: not all palette raster formats are supported; fix this.

        // All palettes have RGBA color order.
        colorOrder = COLOR_RGBA;

        d3dFormatOut = D3DFMT_P8;

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
        sharedd3dConvertRasterFormatCompatible( rasterFormat, colorOrder, depth, d3dFormatOut );
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
    endian::p_little_endian <D3DFORMAT> d3dFormat;

    endian::p_little_endian <uint16> width;
    endian::p_little_endian <uint16> height;
    uint8 depth;
    uint8 mipmapCount;
    uint8 rasterType : 3;
    uint8 pad1 : 5;

    uint8 hasAlpha : 1;
    uint8 isCubeTexture : 1;
    uint8 autoMipMaps : 1;
    uint8 isNotRwCompatible : 1;
    uint8 pad2 : 4;
};
#pragma pack()

};

}

#endif //RWLIB_INCLUDE_NATIVETEX_D3D9

#endif //_RENDERWARE_D3D9_NATIVETEX_MAIN_HEADER_