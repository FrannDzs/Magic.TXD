/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.atc.hxx
*  PURPOSE:     AMDCompress native texture internal header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifdef RWLIB_INCLUDE_NATIVETEX_ATC_MOBILE

#include "txdread.nativetex.hxx"

#include "txdread.d3d.genmip.hxx"

#include "txdread.common.hxx"

// AMDCompress does include windows.h
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Compressonator.h>

#define PLATFORM_ATC    11

namespace rw
{

inline uint32 getATCToolTextureDataRowAlignment( void )
{
    // Once again we have no row alignment in our stored texel data.
    // But we have to pass texels to a compression tool, and this is what
    // it most likely expects.
    return 4;
}

inline uint32 getATCExportTextureDataRowAlignment( void )
{
    // Return a size that is preferred by the framework.
    return 4;
}

enum eATCInternalFormat
{
    ATC_RGB_AMD = 0x8C92,
    ATC_RGBA_EXPLICIT_ALPHA_AMD = 0x8C93,
    ATC_RGBA_INTERPOLATED_ALPHA_AMD = 0x87EE
};

inline uint32 getATCCompressionBlockSize( eATCInternalFormat internalFormat )
{
    uint32 theSize = 0;

    if ( internalFormat == ATC_RGB_AMD )
    {
        theSize = 8;
    }
    else if ( internalFormat == ATC_RGBA_EXPLICIT_ALPHA_AMD )
    {
        theSize = 16;
    }
    else if ( internalFormat == ATC_RGBA_INTERPOLATED_ALPHA_AMD )
    {
        theSize = 16;
    }

    return theSize;
}

inline void getATCMipmapSizeRules( nativeTextureSizeRules& rulesOut )
{
    rulesOut.powerOfTwo = false;
    rulesOut.squared = false;
    rulesOut.multipleOf = true;
    rulesOut.multipleOfValue = 4u;
    rulesOut.maximum = true;
    rulesOut.maxVal = 2048;
}

struct NativeTextureATC
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureATC( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();

        this->internalFormat = ATC_RGB_AMD;
        this->hasAlpha = false;
        
        // TODO.
        this->unk1 = 0;
        this->unk2 = 0;
    }

    inline NativeTextureATC( const NativeTextureATC& right )
    {
        // Copy parameters.
        this->engineInterface = right.engineInterface;
        this->texVersion = right.texVersion;
        this->internalFormat = right.internalFormat;
        this->hasAlpha = right.hasAlpha;
        this->unk1 = right.unk1;
        this->unk2 = right.unk2;

        // Copy mipmaps.
        copyMipmapLayers( this->engineInterface, right.mipmaps, this->mipmaps );
    }

    inline void clearImageData( void )
    {
        // Delete mipmap layers.
        deleteMipmapLayers( this->engineInterface, this->mipmaps );
    }

    inline ~NativeTextureATC( void )
    {
        this->clearImageData();
    }

    typedef genmip::mipmapLayer mipmapLayer;

    DEFINE_HEAP_REDIR_ALLOC_IMPL( mipRedirAlloc );

    eir::Vector <mipmapLayer, mipRedirAlloc> mipmaps;

    // Custom parameters.
    eATCInternalFormat internalFormat;

    bool hasAlpha;

    // Unknowns.
    uint8 unk1;
    uint32 unk2;
};

RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( NativeTextureATC, mipRedirAlloc, mipmaps, engineInterface )

struct atcNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        new (objMem) NativeTextureATC( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) override
    {
        new (objMem) NativeTextureATC( *(const NativeTextureATC*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        ( *(NativeTextureATC*)objMem ).~NativeTextureATC();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const override
    {
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
        storeCaps.pixelCaps.supportsPalette = false;

        storeCaps.isCompressedFormat = true;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut );
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut );
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate );

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version ) override
    {
        NativeTextureATC *nativeTex = (NativeTextureATC*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem ) override
    {
        const NativeTextureATC *nativeTex = (const NativeTextureATC*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        return RASTER_DEFAULT;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        return PALETTE_NONE;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        return true;
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        return RWCOMPRESS_NONE;
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureATC *nativeTex = (const NativeTextureATC*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // Will never be called, because we do not store raw texel data.
        return 0;
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        getATCMipmapSizeRules( rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // The size rules do not depend on the native texture.
        // This is because the format of the native texture does not change very much.
        getATCMipmapSizeRules( rulesOut );
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // This was never defined.
        return 0;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "AMDCompress", this, sizeof( NativeTextureATC ), alignof( NativeTextureATC ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "AMDCompress" );
    }
};

namespace amdtc
{
#pragma pack(push, 1)
struct textureNativeGenericHeader
{
    eir::endian::p_little_endian <uint32> platformDescriptor;

    wardrumFormatInfo formatInfo;

    uint8 pad1[0x10];

    char name[32];
    char maskName[32];

    uint8 mipmapCount;
    uint8 unk1;
    bool hasAlpha;

    uint8 pad2;

    eir::endian::p_little_endian <uint16> width, height;

    eir::endian::p_little_endian <eATCInternalFormat> internalFormat;

    eir::endian::p_little_endian <uint32> imageSectionStreamSize;
    eir::endian::p_little_endian <uint32> unk2;
};
#pragma pack(pop)
};

};

#endif //RWLIB_INCLUDE_NATIVETEX_ATC_MOBILE