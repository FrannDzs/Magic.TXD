/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.pvr.hxx
*  PURPOSE:     PowerVR native texture internal header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE

#include "txdread.nativetex.hxx"

// The PowerVR stuff includes the Windows header.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <PVRTextureUtilities.h>

#include "txdread.d3d.genmip.hxx"

#include "txdread.common.hxx"

#include "pluginutil.hxx"

#include "pixelformat.hxx"

#define PLATFORM_PVR    10

namespace rw
{

inline uint32 getPVRToolTextureDataRowAlignment( void )
{
    // Since PowerVR is a compressed format, there is no real row alignment.
    // This row alignment is supposedly what the PowerVR encoding tool expects.
    return 4;
}

inline uint32 getPVRExportTextureDataRowAlignment( void )
{
    // We return a size here that is preferred by the runtime.
    return 4;
}

enum ePVRInternalFormat
{
    GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG = 0x8C00,
    GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG = 0x8C01,
    GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG = 0x8C02,
    GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG = 0x8C03
};

inline uint32 getDepthByPVRFormat( ePVRInternalFormat theFormat )
{
    uint32 formatDepth = 0;

    if ( theFormat == GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG ||
         theFormat == GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG )
    {
        formatDepth = 4;
    }
    else if ( theFormat == GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG ||
              theFormat == GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG )
    {
        formatDepth = 2;
    }
    else
    {
        assert( 0 );
    }

    return formatDepth;
}

inline bool getPVRCompressionBlockDimensions( uint32 formatDepth, uint32& blockWidthOut, uint32& blockHeightOut )
{
    if ( formatDepth == 2 )
    {
        blockWidthOut = 16;
        blockHeightOut = 8;
        return true;
    }
    else if ( formatDepth == 4 )
    {
        blockWidthOut = 8;
        blockHeightOut = 8;
        return true;
    }

    return false;
}

struct NativeTexturePVR
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTexturePVR( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();

        this->internalFormat = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
        this->hasAlpha = false;

        this->unk1 = 0;
        this->unk8 = 0;
    }

    inline NativeTexturePVR( const NativeTexturePVR& right )
    {
        // Copy parameters.
        this->engineInterface = right.engineInterface;
        this->texVersion = right.texVersion;
        this->internalFormat = right.internalFormat;
        this->hasAlpha = right.hasAlpha;
        this->unk1 = right.unk1;
        this->unk8 = right.unk8;

        // Copy mipmaps.
        copyMipmapLayers( this->engineInterface, right.mipmaps, this->mipmaps );
    }

    inline void clearImageData( void )
    {
        // Delete mipmap layers.
        deleteMipmapLayers( this->engineInterface, this->mipmaps );
    }

    inline ~NativeTexturePVR( void )
    {
        this->clearImageData();
    }

    typedef genmip::mipmapLayer mipmapLayer;

    DEFINE_HEAP_REDIR_ALLOC_IMPL( mipRedirAlloc );

    eir::Vector <mipmapLayer, mipRedirAlloc> mipmaps;

    ePVRInternalFormat internalFormat;

    bool hasAlpha;

    // Unknowns.
    uint8 unk1;
    uint32 unk8;
};

RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( NativeTexturePVR, mipRedirAlloc, mipmaps, engineInterface )

inline void getPVRNativeTextureSizeRules( nativeTextureSizeRules& rulesOut )
{
    rulesOut.powerOfTwo = true;
    rulesOut.squared = false;   // NOT SURE.
    rulesOut.maximum = true;
    rulesOut.maxVal = 2048;
}

struct pvrNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        new (objMem) NativeTexturePVR( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize )
    {
        new (objMem) NativeTexturePVR( *(const NativeTexturePVR*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        ( *(NativeTexturePVR*)objMem ).~NativeTexturePVR();
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

        storeCaps.isCompressedFormat = true;
    }

    // Public API.
    typedef void* PVRTextureHeader;
    typedef void* PVRTexture;
    typedef void* PVRPixelType;

    // Transformation pipeline functions.
    void DecompressPVRMipmap(
        Interface *engineInterface,
        uint32 mipWidth, uint32 mipHeight, uint32 layerWidth, uint32 layerHeight, const void *srcTexels,
        eRasterFormat pvrRasterFormat, uint32 pvrDepth, eColorOrdering pvrColorOrder,
        eRasterFormat targetRasterFormat, uint32 targetDepth, uint32 targetRowAlignment, eColorOrdering targetColorOrder,
        PVRPixelType pvrSrcPixelType, PVRPixelType pvrDstPixelType,
        void*& dstTexelsOut, uint32& dstDataSizeOut
    );
    template <typename srcDispatchType>
    inline void GenericCompressMipmapToPVR(
        Interface *engineInterface,
        uint32 mipWidth, uint32 mipHeight, const void *srcTexels,
        srcDispatchType& fetchDispatch, uint32 srcDepth, uint32 srcRowAlignment,
        eRasterFormat pvrRasterFormat, uint32 pvrDepth, eColorOrdering pvrColorOrder,
        PVRPixelType pvrSrcPixelType, PVRPixelType pvrDstPixelType,
        uint32 pvrBlockWidth, uint32 pvrBlockHeight,
        uint32 pvrBlockDepth,
        uint32& widthOut, uint32& heightOut,
        void*& dstTexelsOut, uint32& dstDataSizeOut
    )
    {
        // Create a PVR texture.
        rasterRowSize srcRowSize = getRasterDataRowSize( mipWidth, srcDepth, srcRowAlignment );

        // We need to determine dimensions that the PVR texture has to use.
        uint32 pvrTexWidth = ALIGN_SIZE( mipWidth, pvrBlockWidth );
        uint32 pvrTexHeight = ALIGN_SIZE( mipHeight, pvrBlockHeight );

        rasterRowSize pvrRowSize = getRasterDataRowSize( pvrTexWidth, pvrDepth, getPVRToolTextureDataRowAlignment() );

        PVRTextureHeader pvrHeader = PVRTextureHeaderCreate( engineInterface, PVRPixelTypeGetID( pvrSrcPixelType ), pvrTexHeight, pvrTexWidth );

        if ( !pvrHeader )
        {
            throw NativeTextureInternalErrorException( "PowerVR", L"POWERVR_INTERNERR_PVRTEXHEADERFAIL" );
        }

        try
        {
            // Copy stuff into the PVR texture properly.
            PVRTexture pvrTexture = PVRTextureCreate( engineInterface, pvrHeader );

            if ( !pvrTexture )
            {
                throw NativeTextureInternalErrorException( "PowerVR", L"POWERVR_INTERNERR_PVRTEXFAIL" );
            }

            try
            {
                // Process the colors into the PowerVR texture.
                {
                    void *pvrDstBuf = PVRTextureGetDataPtr( pvrTexture );

                    colorModelDispatcher putDispatch( pvrRasterFormat, pvrColorOrder, pvrDepth, nullptr, 0, PALETTE_NONE );

                    copyTexelDataBounded(
                        srcTexels, pvrDstBuf,
                        fetchDispatch, putDispatch,
                        mipWidth, mipHeight,
                        pvrTexWidth, pvrTexHeight,
                        0, 0,
                        0, 0,
                        srcRowSize, pvrRowSize
                    );
                }

                // Transcode it.
                bool transcodeSuccess =
                    PVRTranscode( pvrTexture, pvrDstPixelType, ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB );

                if ( transcodeSuccess == false )
                {
                    throw NativeTextureInternalErrorException( "PowerVR", L"POWERVR_INTERNERR_COMPRFAIL" );
                }

                // Copy the PowerVR pixels into a local array.
                uint32 dstDataSize = getPackedRasterDataSize(pvrTexWidth * pvrTexHeight, pvrBlockDepth);

                PVRTextureHeaderCheckDataSize( pvrTexture, dstDataSize );

                void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

                memcpy( dstTexels, PVRTextureGetDataPtr( pvrTexture ), dstDataSize );

                // Give parameters to the runtime.
                widthOut = pvrTexWidth;
                heightOut = pvrTexHeight;

                dstTexelsOut = dstTexels;
                dstDataSizeOut = dstDataSize;
            }
            catch( ... )
            {
                PVRTextureDelete( engineInterface, pvrTexture );

                throw;
            }

            PVRTextureDelete( engineInterface, pvrTexture );
        }
        catch( ... )
        {
            PVRTextureHeaderDelete( engineInterface, pvrHeader );

            throw;
        }

        PVRTextureHeaderDelete( engineInterface, pvrHeader );
    }
    void CompressMipmapToPVR(
        Interface *engineInterface,
        uint32 mipWidth, uint32 mipHeight, const void *srcTexels,
        eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize,
        eRasterFormat pvrRasterFormat, uint32 pvrDepth, eColorOrdering pvrColorOrder,
        PVRPixelType pvrSrcPixelType, PVRPixelType pvrDstPixelType,
        uint32 pvrBlockWidth, uint32 pvrBlockHeight,
        uint32 pvrBlockDepth,
        uint32& widthOut, uint32& heightOut,
        void*& dstTexelsOut, uint32& dstDataSizeOut
    )
    {
        colorModelDispatcher fetchDispatch( srcRasterFormat, srcColorOrder, srcDepth, srcPaletteData, srcPaletteSize, srcPaletteType );

        GenericCompressMipmapToPVR(
            engineInterface,
            mipWidth, mipHeight, srcTexels,
            fetchDispatch, srcDepth, srcRowAlignment,
            pvrRasterFormat, pvrDepth, pvrColorOrder,
            pvrSrcPixelType, pvrDstPixelType,
            pvrBlockWidth, pvrBlockHeight,
            pvrBlockDepth,
            widthOut, heightOut,
            dstTexelsOut, dstDataSizeOut
        );
    }

    // Selector used for image compression.
    inline ePVRInternalFormat GetRecommendedPVRCompressionFormat( uint32 layerWidth, uint32 layerHeight, bool hasAlpha )
    {
        bool canBeCompressedHigh = ( layerWidth * layerHeight ) >= ( 100 * 100 );

        if ( hasAlpha )
        {
            if ( canBeCompressedHigh )
            {
                return GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
            }
            else
            {
                return GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
            }
        }
        else
        {
            if ( canBeCompressedHigh )
            {
                return GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
            }
            else
            {
                return GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
            }
        }
    }

    // Layer pipeline functions.
    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut );
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut );
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate );

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version )
    {
        NativeTexturePVR *nativeTex = (NativeTexturePVR*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem )
    {
        const NativeTexturePVR *nativeTex = (const NativeTexturePVR*)objMem;

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
        const NativeTexturePVR *nativeTex = (const NativeTexturePVR*)objMem;

        return nativeTex->hasAlpha;
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        getPVRNativeTextureSizeRules( rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // The PowerVR native texture seems to be very optimized, limited and not future proof.
        // I am uncertain about the exact rules (a throwback to the good old days!).
        getPVRNativeTextureSizeRules( rulesOut );
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // Once again, we are compressing our contents.
        // Row alignment never plays a role here.
        return 0;
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // Has not been officially defined.
        return 0;
    }

private:
    static constexpr size_t FUTURE_BUFFER_EXAPAND = 128u;

    typedef void (__thiscall* PVRTextureHeader_constructor_t)(
        void *memory,
        pvrtexture::uint64 pixelFormat,
        pvrtexture::uint32 height, pvrtexture::uint32 width,
        pvrtexture::uint32 depth,
        pvrtexture::uint32 numMipmaps,
        pvrtexture::uint32 numArrayMembers,
        pvrtexture::uint32 numFaces,
        EPVRTColourSpace colorSpace,
        EPVRTVariableType varType,
        bool preMultiplied
    );
    typedef void (__thiscall* PVRTextureHeader_destructor_t)( pvrtexture::CPVRTextureHeader *header );
    typedef pvrtexture::uint32 (__thiscall* PVRTextureHeader_getDataSize_t)( const pvrtexture::CPVRTextureHeader *header, pvrtexture::int32 iMipLevel, bool bAllSurfaces, bool bAllFaces );

    typedef void (__thiscall* PVRTexture_constructor_t)( void *memory, const pvrtexture::CPVRTextureHeader& pvrHeader, const void *pData );
    typedef void (__thiscall* PVRTexture_destructor_t)( pvrtexture::CPVRTexture *texture );
    typedef void* (__thiscall* PVRTexture_getDataPtr_t)( const pvrtexture::CPVRTexture *texture, pvrtexture::uint32 uiMIPLevel, pvrtexture::uint32 uiArrayMember, pvrtexture::uint32 uiFaceNumber );

    typedef void (__thiscall* PVRPixelType_constructor_byFormat_t)( void *memory, pvrtexture::uint64 format );
    typedef void (__thiscall* PVRPixelType_constructor_t)( void *memory, pvrtexture::uint8 C1Name, pvrtexture::uint8 C2Name, pvrtexture::uint8 C3Name, pvrtexture::uint8 C4Name, pvrtexture::uint8 C1Bits, pvrtexture::uint8 C2Bits, pvrtexture::uint8 C3Bits, pvrtexture::uint8 C4Bits );

    typedef bool (__cdecl* PVRTranscode_t)( pvrtexture::CPVRTexture& sTexture, const pvrtexture::PixelType ptFormat, const EPVRTVariableType eChannelType, const EPVRTColourSpace eColourspace, const pvrtexture::ECompressorQuality eQuality, const bool bDoDither );

    // We want to have dynamic access to PVRTexLib functions, because Imagination Technologies refuses to ship static libraries.
    PVRTextureHeader_constructor_t pvrHeaderConstructor;
    PVRTextureHeader_destructor_t pvrHeaderDestructor;
    PVRTextureHeader_getDataSize_t pvrHeaderGetDataSize;

    PVRTexture_constructor_t pvrTextureConstructor;
    PVRTexture_destructor_t pvrTextureDestructor;
    PVRTexture_getDataPtr_t pvrTextureGetDataPtr;

    PVRPixelType_constructor_byFormat_t pvrPixelTypeConstructorByFormat;
    PVRPixelType_constructor_t pvrPixelTypeConstructor;

    PVRTranscode_t pvrTranscode;

    bool wasRegistered;
    HMODULE pvrModule;
    
public:
    // Cached pixel type things.
    PVRPixelType pvrPixelType_pvrtc_2bpp_rgb;
    PVRPixelType pvrPixelType_pvrtc_2bpp_rgba;
    PVRPixelType pvrPixelType_pvrtc_4bpp_rgb;
    PVRPixelType pvrPixelType_pvrtc_4bpp_rgba;

    PVRPixelType pvrPixelType_rgba8888;

    PVRTextureHeader PVRTextureHeaderCreate(
        Interface *engineInterface,
        pvrtexture::uint64 pixelFormat,
        pvrtexture::uint32 height, pvrtexture::uint32 width,
        pvrtexture::uint32 depth = 1,
        pvrtexture::uint32 numMipmaps = 1,
        pvrtexture::uint32 numArrayMembers = 1,
        pvrtexture::uint32 numFaces = 1,
        EPVRTColourSpace colorSpace = ePVRTCSpacelRGB,
        EPVRTVariableType varType = ePVRTVarTypeUnsignedByteNorm,
        bool preMultiplied = false
    )
    {
        if ( auto pvrHeaderConstructor = this->pvrHeaderConstructor )
        {
            void *headerMem = engineInterface->MemAllocateP( sizeof( pvrtexture::CPVRTextureHeader ) + FUTURE_BUFFER_EXAPAND, alignof(pvrtexture::CPVRTextureHeader) );

            if ( headerMem )
            {
                try
                {
                    pvrHeaderConstructor( headerMem, pixelFormat, height, width, depth, numMipmaps, numArrayMembers, numFaces, colorSpace, varType, preMultiplied );

                    return (PVRTextureHeader)headerMem;
                }
                catch( ... )
                {
                    engineInterface->MemFree( headerMem );

                    throw;
                }
            }
        }

        return nullptr;
    }

    void PVRTextureHeaderDelete( Interface *engineInterface, PVRTextureHeader header )
    {
        auto pvrHeaderDestructor = this->pvrHeaderDestructor;

        assert( pvrHeaderDestructor != nullptr );
        {
            pvrtexture::CPVRTextureHeader *pvrHeader = (pvrtexture::CPVRTextureHeader*)header;

            pvrHeaderDestructor( pvrHeader );
        }

        engineInterface->MemFree( header );
    }

    void PVRTextureHeaderCheckDataSize( PVRTextureHeader header, uint32 minDataSize )
    {
        if ( auto pvrHeaderGetDataSize = this->pvrHeaderGetDataSize )
        {
            pvrtexture::CPVRTextureHeader *pvrHeader = (pvrtexture::CPVRTextureHeader*)header;

            uint32 actualDataSize = pvrHeaderGetDataSize( pvrHeader, PVRTEX_ALLMIPLEVELS, true, true );

            assert( actualDataSize >= minDataSize );
        }
    }

    PVRTexture PVRTextureCreate( Interface *engineInterface, PVRTextureHeader header, const void *dataPtr = nullptr )
    {
        if ( auto pvrTextureConstructor = this->pvrTextureConstructor )
        {
            void *texMem = engineInterface->MemAllocateP( sizeof( pvrtexture::CPVRTexture ) + FUTURE_BUFFER_EXAPAND, alignof(pvrtexture::CPVRTexture) );

            if ( texMem )
            {
                try
                {
                    pvrtexture::CPVRTextureHeader *pvrHeader = (pvrtexture::CPVRTextureHeader*)header;

                    pvrTextureConstructor( texMem, *pvrHeader, dataPtr );

                    return (PVRTexture)texMem;
                }
                catch( ... )
                {
                    engineInterface->MemFree( texMem );

                    throw;
                }
            }
        }
        
        return nullptr;
    }

    void PVRTextureDelete( Interface *engineInterface, PVRTexture texture )
    {
        auto pvrTextureDestructor = this->pvrTextureDestructor;

        assert( pvrTextureDestructor != nullptr );
        {
            pvrtexture::CPVRTexture *pvrTexture = (pvrtexture::CPVRTexture*)texture;

            pvrTextureDestructor( pvrTexture );
        }

        engineInterface->MemFree( texture );
    }

    void* PVRTextureGetDataPtr( PVRTexture texture )
    {
        if ( auto pvrTextureGetDataPtr = this->pvrTextureGetDataPtr )
        {
            pvrtexture::CPVRTexture *pvrTexture = (pvrtexture::CPVRTexture*)texture;

            return pvrTextureGetDataPtr( pvrTexture, 0, 0, 0 );
        }

        return nullptr;
    }

    PVRPixelType PVRPixelTypeCreateByFormat( Interface *engineInterface, pvrtexture::uint64 format )
    {
        if ( auto pvrPixelTypeConstructorByFormat = this->pvrPixelTypeConstructorByFormat )
        {
            void *pixelTypeMem = engineInterface->MemAllocateP( sizeof( pvrtexture::PixelType ) + FUTURE_BUFFER_EXAPAND, alignof(pvrtexture::PixelType) );

            if ( pixelTypeMem )
            {
                try
                {
                    pvrPixelTypeConstructorByFormat( pixelTypeMem, format );

                    pvrtexture::PixelType *pvrPixelType = (pvrtexture::PixelType*)pixelTypeMem;

                    return (PVRPixelType)pvrPixelType;
                }
                catch( ... )
                {
                    engineInterface->MemFree( pixelTypeMem );

                    throw;
                }
            }
        }

        return nullptr;
    }

    PVRPixelType PVRPixelTypeCreate(
        Interface *engineInterface,
        pvrtexture::uint8 C1Name, pvrtexture::uint8 C2Name, pvrtexture::uint8 C3Name, pvrtexture::uint8 C4Name,
        pvrtexture::uint8 C1Bits, pvrtexture::uint8 C2Bits, pvrtexture::uint8 C3Bits, pvrtexture::uint8 C4Bits
    )
    {
        if ( auto pvrPixelTypeConstructor = this->pvrPixelTypeConstructor )
        {
            void *pixelTypeMem = engineInterface->MemAllocateP( sizeof( pvrtexture::PixelType ) + FUTURE_BUFFER_EXAPAND, alignof(pvrtexture::PixelType) );

            if ( pixelTypeMem )
            {
                try
                {
                    pvrPixelTypeConstructor( pixelTypeMem, C1Name, C2Name, C3Name, C4Name, C1Bits, C2Bits, C3Bits, C4Bits );

                    return (PVRPixelType)pixelTypeMem;
                }
                catch( ... )
                {
                    engineInterface->MemFree( pixelTypeMem );

                    throw;
                }
            }
        }

        return nullptr;
    }

    void PVRPixelTypeDelete( Interface *engineInterface, PVRPixelType pixelType )
    {
        engineInterface->MemFree( pixelType );
    }

    pvrtexture::uint64 PVRPixelTypeGetID( PVRPixelType pixelType )
    {
        if ( pvrtexture::PixelType *pvrPixelType = (pvrtexture::PixelType*)pixelType )
        {
            return pvrPixelType->PixelTypeID;
        }

        return 0;
    }

    PVRPixelType PVRGetCachedPixelType( ePVRInternalFormat internalFormat )
    {
        if ( internalFormat == GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG )
        {
            return this->pvrPixelType_pvrtc_2bpp_rgb;
        }
        else if ( internalFormat == GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG )
        {
            return this->pvrPixelType_pvrtc_2bpp_rgba;
        }
        else if ( internalFormat == GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG )
        {
            return this->pvrPixelType_pvrtc_4bpp_rgb;
        }
        else if ( internalFormat == GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG )
        {
            return this->pvrPixelType_pvrtc_4bpp_rgba;
        }

        return nullptr;
    }

    bool PVRTranscode( PVRTexture sTexture, PVRPixelType ptFormat, const EPVRTVariableType eChannelType, const EPVRTColourSpace eColourspace, const pvrtexture::ECompressorQuality eQuality=pvrtexture::ePVRTCNormal, const bool bDoDither=false )
    {
        if ( auto pvrTranscode = this->pvrTranscode )
        {
            pvrtexture::CPVRTexture *pvrSrcTexture = (pvrtexture::CPVRTexture*)sTexture;
            pvrtexture::PixelType *pvrPtFormat = (pvrtexture::PixelType*)ptFormat;

            return pvrTranscode( *pvrSrcTexture, *pvrPtFormat, eChannelType, eColourspace, eQuality, bDoDither );
        }

        return false;
    }

    inline void Initialize( Interface *engineInterface )
    {
        bool didRegister = false;

        HMODULE pvrModule = LoadLibraryA( "PVRTexLib.dll" );

        PVRTextureHeader_constructor_t pvrHeaderConstructor = nullptr;
        PVRTextureHeader_destructor_t pvrHeaderDestructor = nullptr;
        PVRTextureHeader_getDataSize_t pvrHeaderGetDataSize = nullptr;

        PVRTexture_constructor_t pvrTextureConstructor = nullptr;
        PVRTexture_destructor_t pvrTextureDestructor = nullptr;
        PVRTexture_getDataPtr_t pvrTextureGetDataPtr = nullptr;

        PVRPixelType_constructor_byFormat_t pvrPixelTypeConstructorByFormat = nullptr;
        PVRPixelType_constructor_t pvrPixelTypeConstructor = nullptr;

        PVRTranscode_t pvrTranscode = nullptr;

        this->pvrPixelType_pvrtc_2bpp_rgb = nullptr;
        this->pvrPixelType_pvrtc_2bpp_rgba = nullptr;
        this->pvrPixelType_pvrtc_4bpp_rgb = nullptr;
        this->pvrPixelType_pvrtc_4bpp_rgba = nullptr;
        this->pvrPixelType_rgba8888 = nullptr;

        if ( pvrModule )
        {
            pvrHeaderConstructor = (PVRTextureHeader_constructor_t)GetProcAddress( pvrModule, "??0CPVRTextureHeader@pvrtexture@@QEAA@_KIIIIIIW4EPVRTColourSpace@@W4EPVRTVariableType@@_N@Z" );
            pvrHeaderDestructor = (PVRTextureHeader_destructor_t)GetProcAddress( pvrModule, "??1CPVRTextureHeader@pvrtexture@@QEAA@XZ" );
            pvrHeaderGetDataSize = (PVRTextureHeader_getDataSize_t)GetProcAddress( pvrModule, "?getDataSize@CPVRTextureHeader@pvrtexture@@QEBAIH_N0@Z" );

            pvrTextureConstructor = (PVRTexture_constructor_t)GetProcAddress( pvrModule, "??0CPVRTexture@pvrtexture@@QEAA@AEBVCPVRTextureHeader@1@PEBX@Z" );
            pvrTextureDestructor = (PVRTexture_destructor_t)GetProcAddress( pvrModule, "??1CPVRTexture@pvrtexture@@QEAA@XZ" );
            pvrTextureGetDataPtr = (PVRTexture_getDataPtr_t)GetProcAddress( pvrModule, "?getDataPtr@CPVRTexture@pvrtexture@@QEBAPEAXIII@Z" );

            pvrPixelTypeConstructorByFormat = (PVRPixelType_constructor_byFormat_t)GetProcAddress( pvrModule, "??0PixelType@pvrtexture@@QEAA@_K@Z" );
            pvrPixelTypeConstructor = (PVRPixelType_constructor_t)GetProcAddress( pvrModule, "??0PixelType@pvrtexture@@QEAA@EEEEEEEE@Z" );

            pvrTranscode = (PVRTranscode_t)GetProcAddress( pvrModule, "?Transcode@pvrtexture@@YA_NAEAVCPVRTexture@1@TPixelType@1@W4EPVRTVariableType@@W4EPVRTColourSpace@@W4ECompressorQuality@1@_N@Z" );
        }

        this->pvrHeaderConstructor = pvrHeaderConstructor;
        this->pvrHeaderDestructor = pvrHeaderDestructor;
        this->pvrHeaderGetDataSize = pvrHeaderGetDataSize;

        this->pvrTextureConstructor = pvrTextureConstructor;
        this->pvrTextureDestructor = pvrTextureDestructor;
        this->pvrTextureGetDataPtr = pvrTextureGetDataPtr;

        this->pvrPixelTypeConstructorByFormat = pvrPixelTypeConstructorByFormat;
        this->pvrPixelTypeConstructor = pvrPixelTypeConstructor;

        this->pvrTranscode = pvrTranscode;
        this->pvrModule = pvrModule;

        // Validate if we have a proper API configuration.
        if ( pvrHeaderConstructor && pvrHeaderDestructor &&
             pvrTextureConstructor && pvrTextureDestructor && pvrTextureGetDataPtr &&
             pvrTranscode &&
             pvrPixelTypeConstructorByFormat && pvrPixelTypeConstructor )
        {
            // Cache important pixel types.
            {
                this->pvrPixelType_pvrtc_2bpp_rgb = PVRPixelTypeCreateByFormat( engineInterface, ePVRTPF_PVRTCI_2bpp_RGB );
                this->pvrPixelType_pvrtc_2bpp_rgba = PVRPixelTypeCreateByFormat( engineInterface, ePVRTPF_PVRTCI_2bpp_RGBA );
                this->pvrPixelType_pvrtc_4bpp_rgb = PVRPixelTypeCreateByFormat( engineInterface, ePVRTPF_PVRTCI_4bpp_RGB );
                this->pvrPixelType_pvrtc_4bpp_rgba = PVRPixelTypeCreateByFormat( engineInterface, ePVRTPF_PVRTCI_4bpp_RGBA );

                this->pvrPixelType_rgba8888 = PVRPixelTypeCreate( engineInterface, 'r', 'g', 'b', 'a', 8, 8, 8, 8 );
            }

            didRegister = RegisterNativeTextureType( engineInterface, "PowerVR", this, sizeof( NativeTexturePVR ), alignof( NativeTexturePVR ) );
        }

        this->wasRegistered = didRegister;
    }

    inline void Shutdown( Interface *engineInterface )
    {
        if ( this->wasRegistered )
        {
            UnregisterNativeTextureType( engineInterface, "PowerVR" );

            this->wasRegistered = false;
        }

        // Clean up cached things.
        {
            if ( this->pvrPixelType_pvrtc_2bpp_rgb )
            {
                PVRPixelTypeDelete( engineInterface, this->pvrPixelType_pvrtc_2bpp_rgb );

                this->pvrPixelType_pvrtc_2bpp_rgb = nullptr;
            }

            if ( this->pvrPixelType_pvrtc_2bpp_rgba )
            {
                PVRPixelTypeDelete( engineInterface, this->pvrPixelType_pvrtc_2bpp_rgba );

                this->pvrPixelType_pvrtc_2bpp_rgba = nullptr;
            }

            if ( this->pvrPixelType_pvrtc_4bpp_rgb )
            {
                PVRPixelTypeDelete( engineInterface, this->pvrPixelType_pvrtc_4bpp_rgb );

                this->pvrPixelType_pvrtc_4bpp_rgb = nullptr;
            }

            if ( this->pvrPixelType_pvrtc_4bpp_rgba )
            {
                PVRPixelTypeDelete( engineInterface, this->pvrPixelType_pvrtc_4bpp_rgba );

                this->pvrPixelType_pvrtc_4bpp_rgba = nullptr;
            }

            if ( this->pvrPixelType_rgba8888 )
            {
                PVRPixelTypeDelete( engineInterface, this->pvrPixelType_rgba8888 );
                
                this->pvrPixelType_rgba8888 = nullptr;
            }
        }

        // Clean up function stuff.
        this->pvrHeaderConstructor = nullptr;
        this->pvrHeaderDestructor = nullptr;
        this->pvrHeaderGetDataSize = nullptr;

        this->pvrTextureConstructor = nullptr;
        this->pvrTextureDestructor = nullptr;
        this->pvrTextureGetDataPtr = nullptr;

        this->pvrPixelTypeConstructorByFormat = nullptr;
        this->pvrPixelTypeConstructor = nullptr,

        this->pvrTranscode = nullptr;

        if ( HMODULE pvrModule = this->pvrModule )
        {
            FreeLibrary( pvrModule );

            this->pvrModule = nullptr;
        }
    }

    inline void operator =( const pvrNativeTextureTypeProvider& right )
    {
        // Nothing to do.
        return;
    }
};

typedef PluginDependantStructRegister <pvrNativeTextureTypeProvider, RwInterfaceFactory_t> pvrNativeTextureTypeProviderRegister_t;

extern optional_struct_space <pvrNativeTextureTypeProviderRegister_t> pvrNativeTextureTypeProviderRegister;

namespace pvr
{

#pragma pack(1)
struct textureMetaHeaderGeneric
{
    endian::p_little_endian <uint32> platformDescriptor;

    wardrumFormatInfo formatInfo;

    uint8 pad1[ 0x10 ];

    char name[ 32 ];
    char maskName[ 32 ];

    uint8 mipmapCount;
    uint8 unk1;
    bool hasAlpha;

    uint8 pad2;

    endian::p_little_endian <uint16> width, height;

    endian::p_little_endian <ePVRInternalFormat> internalFormat;
    endian::p_little_endian <uint32> imageDataStreamSize;
    endian::p_little_endian <uint32> unk8;
};
#pragma pack()

};

};

#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE