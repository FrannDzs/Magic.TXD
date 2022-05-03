/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.unc.hxx
*  PURPOSE:     unc_mobile native texture internal header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifdef RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE

#include "txdread.nativetex.hxx"

// Platform descriptor, not that important.
#define PLATFORMDESC_UNC_MOBILE     12

#include "txdread.d3d.genmip.hxx"

#include "txdread.common.hxx"

namespace rw
{

inline uint32 getUNCTextureDataRowAlignment( void )
{
    // This texture format is pretty special. Since it is uncompressed it
    // indeed does have a row alignment. But since it is most likely loaded
    // by OpenGL, we cannot be sure about this row alignment.
    // We define here that loaders must adhere to 4 bytes.
    return 1;
}

inline rasterRowSize getUNCRasterDataRowSize( uint32 width, uint32 depth )
{
    return getRasterDataRowSize( width, depth, getUNCTextureDataRowAlignment() );
}

struct NativeTextureMobileUNC
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureMobileUNC( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->hasAlpha = false;
        this->unk2 = 0;
        this->unk3 = 0;
    }

    inline NativeTextureMobileUNC( const NativeTextureMobileUNC& right )
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = engineInterface;
        this->texVersion = right.texVersion;
        this->hasAlpha = right.hasAlpha;
        this->unk2 = right.unk2;
        this->unk3 = right.unk3;

        // Copy over mipmaps.
        copyMipmapLayers( engineInterface, right.mipmaps, this->mipmaps );
    }

    inline void clearTexelData( void )
    {
        deleteMipmapLayers( this->engineInterface, this->mipmaps );
    }

    inline ~NativeTextureMobileUNC( void )
    {
        // Delete all mipmaps.
        this->clearTexelData();
    }

    typedef genmip::mipmapLayer mipmapLayer;

    DEFINE_HEAP_REDIR_ALLOC_IMPL( mipRedirAlloc );

    eir::Vector <mipmapLayer, mipRedirAlloc> mipmaps;

    bool hasAlpha;

    uint32 unk2;
    uint32 unk3;
};

RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( NativeTextureMobileUNC, mipRedirAlloc, mipmaps, engineInterface )

inline void getUNCRasterFormat( bool hasAlpha, eRasterFormat& rasterFormat, eColorOrdering& colorOrder, uint32& depth )
{
    // TODO.
    if ( hasAlpha )
    {
        rasterFormat = RASTER_4444;
        depth = 16;
        colorOrder = COLOR_ABGR;
    }
    else
    {
        rasterFormat = RASTER_565;
        depth = 16;
        colorOrder = COLOR_BGRA;
    }
}

inline void getUNCNativeTextureSizeRules( nativeTextureSizeRules& rulesOut )
{
    rulesOut.powerOfTwo = false;
    rulesOut.squared = false;
    rulesOut.maximum = true;
    rulesOut.maxVal = 2048;
}

struct uncNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        new (objMem) NativeTextureMobileUNC( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize )
    {
        new (objMem) NativeTextureMobileUNC( *(const NativeTextureMobileUNC*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        ( *(NativeTextureMobileUNC*)objMem ).~NativeTextureMobileUNC();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const
    {
        capsOut.supportsDXT1 = false;
        capsOut.supportsDXT2 = false;
        capsOut.supportsDXT3 = false;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = false;
        capsOut.supportsPalette = true;
    }

    void GetStorageCapabilities( storageCapabilities& storeCaps ) const
    {
        storeCaps.pixelCaps.supportsDXT1 = false;
        storeCaps.pixelCaps.supportsDXT2 = false;
        storeCaps.pixelCaps.supportsDXT3 = false;
        storeCaps.pixelCaps.supportsDXT4 = false;
        storeCaps.pixelCaps.supportsDXT5 = false;
        storeCaps.pixelCaps.supportsPalette = false;

        storeCaps.isCompressedFormat = false;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut );
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut );
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate );

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version )
    {
        NativeTextureMobileUNC *nativeTex = (NativeTextureMobileUNC*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem )
    {
        const NativeTextureMobileUNC *nativeTex = (const NativeTextureMobileUNC*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        const NativeTextureMobileUNC *nativeTex = (const NativeTextureMobileUNC*)objMem;

        eRasterFormat rasterFormat;
        uint32 depth;
        eColorOrdering colorOrder;

        getUNCRasterFormat( nativeTex->hasAlpha, rasterFormat, colorOrder, depth );

        return rasterFormat;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        return PALETTE_NONE;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        return false;
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        return RWCOMPRESS_NONE;
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureMobileUNC *nativeTex = (const NativeTextureMobileUNC*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // Who would know something like this anyway? OpenGL can have many alignment sizes,
        // one of 1, 2, 4 or 8. So I define this here, take it or leave it!
        return 4;
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        getUNCNativeTextureSizeRules( rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // I let common sense speak here.
        getUNCNativeTextureSizeRules( rulesOut );
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // This was never defined.
        return 0;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "uncompressed_mobile", this, sizeof( NativeTextureMobileUNC ), alignof( NativeTextureMobileUNC ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "uncompressed_mobile" );
    }
};

namespace mobile_unc
{
#pragma pack(push, 1)
struct textureNativeGenericHeader
{
    endian::p_little_endian <uint32> platformDescriptor;

    wardrumFormatInfo formatInfo;

    uint8 pad1[0x10];

    char name[32];
    char maskName[32];

    uint8 mipmapCount;
    uint8 unk1;
    bool hasAlpha;

    uint8 pad2;

    endian::p_little_endian <uint16> width, height;

    endian::p_little_endian <uint32> unk2;

    endian::p_little_endian <uint32> imageDataSectionSize;
    endian::p_little_endian <uint32> unk3;
};
#pragma pack(pop)
};

};

#endif //RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE