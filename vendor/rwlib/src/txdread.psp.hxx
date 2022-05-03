/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.psp.hxx
*  PURPOSE:     Main include file of the PlayStation Portable native texture.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifdef RWLIB_INCLUDE_NATIVETEX_PSP

#include "txdread.nativetex.hxx"

#include "txdread.d3d.hxx"

#include "txdread.ps2shared.hxx"

#include "txdread.common.hxx"

#define PSP_FOURCC  0x00505350      // 'PSP\0'

namespace rw
{

static inline uint32 getPSPTextureDataRowAlignment( void )
{
    // For compatibility reasons, we say that swizzled mipmap data has a row alignment of 1.
    // It should not matter for any of the operations we do.
    return 1;
}

static inline uint32 getPSPExportTextureDataRowAlignment( void )
{
    // This row alignment should be a framework friendly size.
    // To make things most-compatible with Direct3D, a size of 4 is recommended.
    return 4;
}

inline rasterRowSize getPSPRasterDataRowSize( uint32 mipWidth, uint32 depth )
{
    return getRasterDataRowSize( mipWidth, depth, getPSPTextureDataRowAlignment() );
}

static inline bool isPSPSwizzlingRequired( uint32 baseWidth, uint32 baseHeight, uint32 depth )
{
    // TODO: not sure about this at all.

    if ( baseWidth > 256 || baseHeight > 256 )
    {
        return false;
    }

    if ( depth == 4 )
    {
        if ( baseWidth < 32 )
        {
            return false;
        }
    }
    else if ( depth == 8 )
    {
        if ( baseWidth < 16 )
        {
            return false;
        }
    }
    else if ( depth == 32 )
    {
        if ( baseWidth < 8 || baseHeight < 8 )
        {
            return false;
        }
    }

    // The PSP native texture uses a very narrow swizzling convention.
    // It is the first native texture I encountered that uses per-level swizzling flags.
    if ( baseWidth > baseHeight * 2 )
    {
        return false;
    }

    if ( baseWidth < 256 && baseHeight < 256 )
    {
        if ( baseHeight > baseWidth )
        {
            return false;
        }
    }

    return true;
}

static inline eMemoryLayoutType getPSPHardwareBITBLTFormat( uint32 depth )
{
    if ( depth == 4 )
    {
        return PSMCT16;
    }
    else if ( depth == 8 )
    {
        return PSMCT32;
    }
    else if ( depth == 16 )
    {
        return PSMCT16;
    }
    else if ( depth == 32 )
    {
        return PSMCT32;
    }

    throw NativeTextureInternalErrorException( "PSP", nullptr );
}

static inline bool getPSPBrokenPackedFormatDimensions(
    eMemoryLayoutType rawFormat, eMemoryLayoutType bitbltFormat,
    uint32 layerWidth, uint32 layerHeight,
    uint32& packedWidthOut, uint32& packedHeightOut
)
{
    // This routine is broken because it does not honor the memory scheme of the GPU.
    // The PSP GPU stores data in memory columns.

    bool gotProposedDimms = false;

    uint32 prop_packedWidth, prop_packedHeight;

    if ( bitbltFormat == PSMCT32 )
    {
        if ( rawFormat == PSMT8 )
        {
            prop_packedWidth = ( layerWidth / 2 );
            prop_packedHeight = ( layerHeight / 2 );
            
            gotProposedDimms = true;
        }
        else if ( rawFormat == PSMT4 )
        {
            prop_packedWidth = ( layerWidth / 4 );
            prop_packedHeight = ( layerHeight / 2 );
            
            gotProposedDimms = true;
        }
    }
    else if ( bitbltFormat == PSMCT16 )
    {
        if ( rawFormat == PSMT4 )
        {
            prop_packedWidth = ( layerWidth / 2 );
            prop_packedHeight = ( layerHeight / 2 );

            gotProposedDimms = true;
        }
    }

    if ( gotProposedDimms )
    {
        if ( prop_packedWidth != 0 && prop_packedHeight != 0 )
        {
            packedWidthOut = prop_packedWidth;
            packedHeightOut = prop_packedHeight;
            return true;
        }
    }

    return false;
}

struct NativeTexturePSP
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTexturePSP( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->rawColorMemoryLayout = PSMCT32;
        this->bitbltMemoryLayout = PSMCT32;
        this->palette = nullptr;
        this->unk = 0;
    }

    inline NativeTexturePSP( const NativeTexturePSP& right ) : mipmaps()
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = engineInterface;
        this->texVersion = right.texVersion;
        this->rawColorMemoryLayout = right.rawColorMemoryLayout;
        this->bitbltMemoryLayout = right.bitbltMemoryLayout;

        // Clone color buffers.
        size_t mipmapCount = right.mipmaps.GetCount();

        this->mipmaps.Resize( mipmapCount );

        try
        {
            for ( size_t n = 0; n < mipmapCount; n++ )
            {
                const GETexture& srcLayer = right.mipmaps[ n ];

                GETexture& dstLayer = this->mipmaps[ n ];

                uint32 dataSize = srcLayer.dataSize;

                void *newbuf = engineInterface->PixelAllocate( dataSize );

                memcpy( newbuf, srcLayer.texels, dataSize );

                dstLayer.width = srcLayer.width;
                dstLayer.height = srcLayer.height;
                dstLayer.texels = newbuf;
                dstLayer.dataSize = dataSize;

                dstLayer.isSwizzled = srcLayer.isSwizzled;
            }

            ePaletteTypePS2 srcPaletteType;

            eRasterFormatPS2 rasterFormat;
            
            GetPS2RasterFormatFromMemoryLayoutType( right.rawColorMemoryLayout, eCLUTMemoryLayoutType::PSMCT32, rasterFormat, srcPaletteType );

            void *dstPalette = nullptr;

            if ( void *srcPalette = right.palette )
            {
                uint32 srcPaletteSize = getPaletteItemCount( GetGenericPaletteTypeFromPS2( srcPaletteType ) );

                uint32 palRasterDepth = Bitmap::getRasterFormatDepth( GetGenericRasterFormatFromPS2( rasterFormat ) );

                uint32 palDataSize = getPaletteDataSize( srcPaletteSize, palRasterDepth );

                dstPalette = engineInterface->PixelAllocate( palDataSize );

                memcpy( dstPalette, srcPalette, palDataSize );
            }

            this->palette = dstPalette;

            this->unk = right.unk;
        }
        catch( ... )
        {
            for ( size_t n = 0; n < mipmapCount; n++ )
            {
                this->mipmaps[ n ].Deallocate( engineInterface );
            }

            throw;
        }
    }

    inline ~NativeTexturePSP( void )
    {
        // Free all color buffer info.
        Interface *engineInterface = this->engineInterface;

        size_t mipmapCount = this->mipmaps.GetCount();

        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            this->mipmaps[n].Deallocate( engineInterface );
        }

        if ( void *palData = this->palette )
        {
            engineInterface->PixelFree( palData );

            this->palette = nullptr;
        }
    }

    void UpdateStructure( Interface *engineInterface )
    {
        // todo.
        // I don't think we will ever have to do anything over here.
    }

    struct GETexture
    {
        inline GETexture( void )
        {
            this->width = 0;
            this->height = 0;
            this->texels = nullptr;
            this->dataSize = 0;

            this->isSwizzled = false;
        }

        inline GETexture( const GETexture& right ) = delete;

        inline GETexture( GETexture&& right )
        {
            this->width = right.width;
            this->height = right.height;
            this->texels = right.texels;
            this->dataSize = right.dataSize;

            this->isSwizzled = right.isSwizzled;

            right.texels = nullptr;
        }

        inline ~GETexture( void )
        {
            assert( this->texels == nullptr );
        }

        // Required by eir::Vector for error-recovery.
        inline GETexture& operator = ( GETexture&& right )
        {
            this->width = right.width;
            this->height = right.height;
            this->texels = right.texels;
            this->dataSize = right.dataSize;

            this->isSwizzled = right.isSwizzled;

            right.texels = nullptr;

            return *this;
        }

        inline void Deallocate( Interface *engineInterface )
        {
            if ( void *texels = this->texels )
            {
                engineInterface->PixelFree( texels );

                this->texels = nullptr;
                this->dataSize = 0;
            }
        }

        uint32 width;
        uint32 height;
        void *texels;
        uint32 dataSize;

        bool isSwizzled;
    };

    // Data members.
    eMemoryLayoutType bitbltMemoryLayout;
    eMemoryLayoutType rawColorMemoryLayout;

    DEFINE_HEAP_REDIR_ALLOC_IMPL( mipRedirAlloc );

    eir::Vector <GETexture, mipRedirAlloc> mipmaps;

    // Color Look Up Table (CLUT).
    // Could be swizzled, but not a GSTexture like on the PS2.
    void *palette;

    // Unknowns.
    uint32 unk;
};

RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( NativeTexturePSP, mipRedirAlloc, mipmaps, engineInterface )

static inline void getPSPNativeTextureSizeRules( nativeTextureSizeRules& rulesOut )
{
    // Sort of the same as in the PS2 native texture.
    rulesOut.powerOfTwo = true;
    rulesOut.squared = false;
    rulesOut.maximum = true;
    rulesOut.maxVal = 1024;
}

struct pspNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        new (objMem) NativeTexturePSP( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) override
    {
        new (objMem) NativeTexturePSP( *(const NativeTexturePSP*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        ( *(NativeTexturePSP*)objMem ).~NativeTexturePSP();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const override;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const override;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const override;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const override
    {
        // No idea whether it actually does support DXT.
        // There is still one unknown field, but we may never know!
        capsOut.supportsDXT1 = false;
        capsOut.supportsDXT2 = false;
        capsOut.supportsDXT3 = false;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = false;
        capsOut.supportsPalette = true;
    }

    void GetStorageCapabilities( storageCapabilities& storeCaps ) const override
    {
        storeCaps.pixelCaps.supportsDXT1 = false;
        storeCaps.pixelCaps.supportsDXT2 = false;
        storeCaps.pixelCaps.supportsDXT3 = false;
        storeCaps.pixelCaps.supportsDXT4 = false;
        storeCaps.pixelCaps.supportsDXT5 = false;
        storeCaps.pixelCaps.supportsPalette = true;

        storeCaps.isCompressedFormat = false;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut ) override;
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut ) override;
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate ) override;

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version ) override
    {
        NativeTexturePSP *nativeTex = (NativeTexturePSP*)objMem;

        nativeTex->texVersion = version;

        nativeTex->UpdateStructure( engineInterface );
    }

    LibraryVersion GetTextureVersion( const void *objMem ) override
    {
        const NativeTexturePSP *nativeTex = (const NativeTexturePSP*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut ) override;
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut ) override;
    void ClearMipmaps( Interface *engineInterface, void *objMem ) override;

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut ) override
    {
        const NativeTexturePSP *pspTex = (NativeTexturePSP*)objMem;

        uint32 baseWidth = 0;
        uint32 baseHeight = 0;

        uint32 mipmapCount = (uint32)pspTex->mipmaps.GetCount();

        if ( mipmapCount > 0 )
        {
            const NativeTexturePSP::GETexture& baseLayer = pspTex->mipmaps[ 0 ];

            baseWidth = baseLayer.width;
            baseHeight = baseLayer.height;
        }

        infoOut.baseWidth = baseWidth;
        infoOut.baseHeight = baseHeight;
        infoOut.mipmapCount = mipmapCount;
    }

    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const override
    {
        NativeTexturePSP *nativeTex = (NativeTexturePSP*)objMem;

        // Very similar procedure to the PS2 native texture here.
        rwStaticString <char> formatString = "PSP ";

        eColorOrdering colorOrder = COLOR_RGBA;
        ePaletteTypePS2 paletteType;
        eRasterFormatPS2 rasterFormat;
        
        GetPS2RasterFormatFromMemoryLayoutType( nativeTex->rawColorMemoryLayout, eCLUTMemoryLayoutType::PSMCT32, rasterFormat, paletteType );

        uint32 depth = GetMemoryLayoutTypePresenceDepth( nativeTex->rawColorMemoryLayout );

        getDefaultRasterFormatString(
            GetGenericRasterFormatFromPS2( rasterFormat ),
            depth,
            GetGenericPaletteTypeFromPS2( paletteType ),
            colorOrder,
            formatString
        );

        if ( buf )
        {
            strncpy( buf, formatString.GetConstString(), bufLen );
        }

        lengthOut = formatString.GetLength();
    }

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        const NativeTexturePSP *nativeTex = (const NativeTexturePSP*)objMem;

        ePaletteTypePS2 paletteType;
        eRasterFormatPS2 rasterFormat;
        
        GetPS2RasterFormatFromMemoryLayoutType( nativeTex->rawColorMemoryLayout, eCLUTMemoryLayoutType::PSMCT32, rasterFormat, paletteType );

        return GetGenericRasterFormatFromPS2( rasterFormat );
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        const NativeTexturePSP *nativeTex = (const NativeTexturePSP*)objMem;

        ePaletteTypePS2 paletteType;
        eRasterFormatPS2 rasterFormat;
        
        GetPS2RasterFormatFromMemoryLayoutType( nativeTex->rawColorMemoryLayout, eCLUTMemoryLayoutType::PSMCT32, rasterFormat, paletteType );

        return GetGenericPaletteTypeFromPS2( paletteType );
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        // TODO: I heard PSP actually supports DXTn?
        return false;
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        // TODO: maybe there is DXT support after all?
        // Rockstar Leeds, pls.
        return RWCOMPRESS_NONE;
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override;

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        return getPSPTextureDataRowAlignment();
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        getPSPNativeTextureSizeRules( rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // TODO: I have no idea what formats are supported yet.
        // The PSP does support DXT after all, but there is no trace of this.
        getPSPNativeTextureSizeRules( rulesOut );
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // There has been no driver id assigned to the PSP native texture.
        // Because the format never advanced beyond RW 3.1.
        return 0;
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "PSP", this, sizeof( NativeTexturePSP ), alignof( NativeTexturePSP ) );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "PSP" );
    }
};

namespace psp
{

#pragma pack(push,1)
struct textureMetaDataHeader
{
    // This is the master meta data header.
    // Not fleshed out, is it?
    endian::p_little_endian <uint32> width;
    endian::p_little_endian <uint32> height;
    endian::p_little_endian <uint32> depth;
    endian::p_little_endian <uint32> mipmapCount;
    endian::p_little_endian <uint32> unknown;     // ???, always zero? reserved?
};
#pragma pack(pop)

}

};

#endif //RWLIB_INCLUDE_NATIVETEX_PSP