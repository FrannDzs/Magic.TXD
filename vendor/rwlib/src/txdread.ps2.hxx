/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.ps2.hxx
*  PURPOSE:     PlayStation 2 native texture internal header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifdef RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2

#include "txdread.nativetex.hxx"

#include "txdread.ps2shared.hxx"

#include "txdread.common.hxx"

#include "renderware.txd.ps2.h"

#include <type_traits>

#define PS2_FOURCC 0x00325350 /* "PS2\0" */

namespace rw
{

static inline uint32 getPS2TextureDataRowAlignment( void )
{
    // The documentation states that packed GIFtags have no padding of
    // their data thus it is a linear array with no row alignment.
    return 0;
}

AINLINE uint32 getPS2ExportTextureDataRowAlignment( void )
{
    // This row alignment should be a framework friendly size.
    // To make things most-compatible with Direct3D, a size of 4 is recommended.
    return 4;
}

AINLINE rasterRowSize getPS2RasterDataRowSize( uint32 mipWidth, uint32 depth )
{
    return getRasterDataRowSize( mipWidth, depth, getPS2TextureDataRowAlignment() );
}

struct ps2MipmapTransmissionData
{
    uint16 destX, destY;
};

struct ps2RasterFormatFlags
{
    uint16 rasterType : 4;
    uint16 privateFlags : 4;
    uint16 formatNum : 4;
    uint16 autoMipmaps : 1;
    uint16 palType : 2;
    uint16 hasMipmaps : 1;
};
static_assert( sizeof(ps2RasterFormatFlags) == sizeof(uint16) );

struct textureMetaDataHeader
{
    endian::p_little_endian <uint32> baseLayerWidth;
    endian::p_little_endian <uint32> baseLayerHeight;
    endian::p_little_endian <uint32> depth;
    endian::p_little_endian <ps2RasterFormatFlags> ps2RasterFlags;
    endian::p_little_endian <uint16> version;   // I had a hunch, but aap confirmed it. thx bro.

    endian::p_little_endian <ps2GSRegisters::ps2reg_t> tex0;
    endian::p_little_endian <uint32> clutOffset;
    endian::p_little_endian <uint32> tex1_low;

    endian::p_little_endian <ps2GSRegisters::ps2reg_t> miptbp1;
    endian::p_little_endian <ps2GSRegisters::ps2reg_t> miptbp2;

    endian::p_little_endian <uint32> gsMipmapDataSize;      // texels + GIFtags
    endian::p_little_endian <uint32> gsCLUTDataSize;        // palette + GIFtags + unknowns

    endian::p_little_endian <uint32> combinedGPUDataSize;

    // constant (Sky mipmap val)
    // see http://www.gtamodding.com/wiki/Sky_Mipmap_Val_%28RW_Section%29
    endian::p_little_endian <uint32> skyMipmapVal;
};

struct GIFtag
{
    enum class eEOP
    {
        FOLLOWING_PRIMITIVE = 0,
        END_OF_PACKET
    };

    enum class ePRE
    {
        IGNORE_PRIM = 0,
        OUTPUT_PRIM_VALUE_TO_PRIM_REG
    };

    enum class eFLG
    {
        PACKED_MODE,
        REGLIST_MODE,
        IMAGE_MODE,
        DISABLED
    };

    struct RegisterList
    {
        static constexpr uint32 max_num_registers = 16u;

        inline RegisterList( void )
        {
            this->regs = 0;
        }
        inline RegisterList( const RegisterList& ) = default;
        inline RegisterList( std::uint64_t value ) noexcept
        {
            this->regs = value;
        }

        inline operator uint64 ( void ) const noexcept
        {
            return this->regs;
        }

        inline RegisterList& operator = ( std::uint64_t value ) noexcept
        {
            this->regs = value;

            return *this;
        }

        inline eGIFTagRegister getRegisterID( uint32 i ) const noexcept
        {
#ifdef _DEBUG
            assert(i < max_num_registers);
#endif //_DEBUG

            std::uint64_t shiftPos = i * 4;

            return (eGIFTagRegister)( ( this->regs & ( 0xF << shiftPos ) ) >> shiftPos );
        }

        inline void setRegisterID( uint32 i, eGIFTagRegister regContent ) noexcept
        {
#ifdef _DEBUG
            assert(i < max_num_registers);
#endif //_DEBUG

            std::uint64_t shiftPos = i * 4;

            this->regs &= ~( 0xF << shiftPos );

            this->regs |= (uint32)regContent >> shiftPos;
        }

        std::uint64_t regs;
    };

    union
    {
        struct
        {
            std::uint64_t nloop : 15;
            std::uint64_t eop : 1;
            std::uint64_t pad1 : 30;
            std::uint64_t pre : 1;
            std::uint64_t prim : 11;
            std::uint64_t flg : 2;
            std::uint64_t nreg : 4;
        };
        std::uint64_t props;
    };

    RegisterList regs;
};

struct GIFtag_serialized
{
    endian::p_little_endian <std::uint64_t> props;
    endian::p_little_endian <std::uint64_t> regs;

    inline operator GIFtag ( void ) const
    {
        GIFtag result;

        result.props = this->props;
        result.regs = this->regs;

        return result;
    }

    inline GIFtag_serialized& operator = ( const GIFtag& right )
    {
        this->props = right.props;
        this->regs = right.regs;

        return *this;
    }
};

// Used in eGIFTagRegister::A_PLUS_D for specifying the targer register for data.
// This is mean to extend GIFtag's register range to all available
// registers.
struct GIFtag_aplusd
{
    std::uint64_t regID : 8;
    std::uint64_t pad1 : 56;
    std::uint64_t gsregvalue;

    inline eGSRegister getRegisterType( void ) const
    {
        return (eGSRegister)this->regID;
    }
};

struct NativeTexturePS2
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTexturePS2( Interface *engineInterface )
    {
        // Initialize the texture object.
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->rasterFormat = eRasterFormatPS2::RASTER_DEFAULT;
        this->depth = 0;
        this->paletteType = ePaletteTypePS2::NONE;
        this->autoMipmaps = false;
        this->encodingVersion = 2;
        this->skyMipMapVal = 4032;
        this->recommendedBufferBasePointer = 0;
        this->pixelStorageMode = PSMCT32;
        this->clutStorageFmt = eCLUTMemoryLayoutType::PSMCT32;
        this->clutEntryOffset = 0;
        this->clutStorageMode = eCLUTStorageMode::CSM1;
        this->clutLoadControl = 0;
        this->bitblt_targetFmt = PSMCT32;
        this->texMemSize = 0;
        this->rasterType = 4;
        this->texColorComponent = eTextureColorComponent::RGB;

        // Set default values for PS2 GS parameters.
        this->textureFunction = eTextureFunction::MODULATE;
        this->lodCalculationModel = 0;      // LOD using formula
        this->clutOffset = 0;

        // Texture raster by default.
        this->rasterType = 4;
        this->privateFlags = 0;
    }

    inline NativeTexturePS2( const NativeTexturePS2& right )
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = engineInterface;
        this->texVersion = right.texVersion;

        // Copy palette information.
        this->paletteTex.CopyTexture( engineInterface, right.paletteTex );
        this->paletteType = right.paletteType;

        // Copy image texel information.
        this->rasterFormat = right.rasterFormat;
        this->depth = right.depth;

        {
            size_t mipmapCount = right.mipmaps.GetCount();

            this->mipmaps.Resize( mipmapCount );

            for ( size_t n = 0; n < mipmapCount; n++ )
            {
                GSTexture& thisLayer = this->mipmaps[ n ];

                const GSTexture& srcLayer = right.mipmaps[ n ];

                thisLayer.CopyTexture( engineInterface, srcLayer );
            }
        }

        // Copy PS2 data.
        this->pixelStorageMode = right.pixelStorageMode;
        this->texColorComponent = right.texColorComponent;
        this->clutStorageFmt = right.clutStorageFmt;
        this->clutStorageMode = right.clutStorageMode;
        this->clutEntryOffset = right.clutEntryOffset;
        this->clutLoadControl = right.clutLoadControl;
        this->autoMipmaps = right.autoMipmaps;
        this->encodingVersion = right.encodingVersion;
        this->skyMipMapVal = right.skyMipMapVal;
        this->textureFunction = right.textureFunction;
        this->lodCalculationModel = right.lodCalculationModel;
        this->clutOffset = right.clutOffset;
        this->recommendedBufferBasePointer = right.recommendedBufferBasePointer;
        this->bitblt_targetFmt = right.bitblt_targetFmt;
        this->texMemSize = right.texMemSize;

        this->rasterType = right.rasterType;
        this->privateFlags = right.privateFlags;
    }

    inline void clearImageData( void )
    {
        Interface *engineInterface = this->engineInterface;

        // Free all mipmaps.
        size_t mipmapCount = this->mipmaps.GetCount();

        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            GSTexture& mipLayer = this->mipmaps[ n ];

            mipLayer.Cleanup( engineInterface );
        }

        // Free the palette texture.
        this->paletteTex.Cleanup( engineInterface );
    }

    inline ~NativeTexturePS2( void )
    {
        this->clearImageData();
    }

    // Struct of dynamic size: the data of type is appended to it.
    struct GSPrimitive
    {
        uint16 nloop : 14;          // number of data attached to this primitive (at end of struct)
        // Since we are not a stream representation we do not store an end-flag here.
        // When compositioning to a stream you should set EOP=1 at the finishing entry.
        // The Sky engine does this on the fly so it can chain multiple texture uploads
        // into one giant packet.
        bool hasPrimValue;
        uint16 primValue : 10;
        GIFtag::eFLG type;
        uint8 nregs : 4;
        GIFtag::RegisterList registers;     // 16x4 bits of register types that repeat NLOOP times.

        // List into a manager to store the primitives in-order.
        RwListEntry <GSPrimitive> listNode;

        GSPrimitive* clone( Interface *rwEngine, uint16 add_cnt = 0 );
        GSPrimitive* extend_by( Interface *rwEngine, uint16 extend_cnt );

        // Allocation helpers.
        template <typename structType>
        AINLINE static void GetAllocationMetaInfo( size_t itemCount, size_t& allocSizeOut, size_t& alignOut, size_t *vecSizeOut = nullptr )
        {
            static_assert( std::is_standard_layout <structType>::value && std::is_trivial <structType>::value );

            // itemCount is nloop in IMAGE mode, otherwise nregs * nloop.

            alignOut = CalculateAlignmentForObjectWithHeader( alignof(GSPrimitive), alignof(structType) );

            size_t veclength = eir::VectorMemSize <structType> ( itemCount );

            if ( vecSizeOut )
            {
                *vecSizeOut = veclength;
            }

            allocSizeOut = CalculateAlignedTypeSizeWithHeader( sizeof(GSPrimitive), veclength, alignof(structType) );
        }

        template <typename structType>
        AINLINE structType* GetItemArray( void )
        {
            void *implPtr = GetObjectImplementationPointerForObject( this, sizeof(GSPrimitive), alignof(structType) );

            return (structType*)implPtr;
        }

        template <typename structType>
        AINLINE const structType* GetItemArray( void ) const
        {
            const void *implPtr = GetObjectImplementationPointerForObject( (void*)this, sizeof(GSPrimitive), alignof(structType) );

            return (const structType*)implPtr;
        }
    };

    struct GSPrimitivePackedItem
    {
        std::uint64_t gifregvalue_lo;
        std::uint64_t gifregvalue_hi;
    };

    struct GSPrimitiveReglistItem
    {
        std::uint64_t gsregvalue;
    };

    struct GSTexture
    {
        inline GSTexture( void )
        {
            this->width = 0;
            this->height = 0;
            this->dataSize = 0;
            this->texels = nullptr;

            this->transmissionAreaWidth = 0;
            this->transmissionAreaHeight = 0;
            this->transmissionOffsetX = 0;
            this->transmissionOffsetY = 0;

            this->textureBasePointer = 0;
            this->textureBufferWidth = 0;
        }

    private:
        AINLINE void move_from_properties( GSTexture&& right ) noexcept
        {
            this->width = right.width;
            this->height = right.height;
            this->dataSize = right.dataSize;
            this->texels = right.texels;
            this->transmissionAreaWidth = right.transmissionAreaWidth;
            this->transmissionAreaHeight = right.transmissionAreaHeight;
            this->transmissionOffsetX = right.transmissionOffsetX;
            this->transmissionOffsetY = right.transmissionOffsetY;
            this->textureBasePointer = right.textureBasePointer;
            this->textureBufferWidth = right.textureBufferWidth;
            this->list_header_primitives = std::move( right.list_header_primitives );

            // Reset the other texture.
            right.width = 0;
            right.height = 0;
            right.dataSize = 0;
            right.texels = nullptr;
            right.transmissionAreaWidth = 0;
            right.transmissionAreaHeight = 0;
            right.transmissionOffsetX = 0;
            right.transmissionOffsetY = 0;
            right.textureBasePointer = 0;
            right.textureBufferWidth = 0;
        }

    public:
        inline GSTexture( const GSTexture& ) = delete;
        inline GSTexture( GSTexture&& right ) noexcept
        {
            move_from_properties( std::move( right ) );
        }

        inline ~GSTexture( void )
        {
            // Make sure that we release all primitives.
            assert( LIST_EMPTY( this->list_header_primitives.root ) );
            assert( this->texels == nullptr );
        }

        inline GSTexture& operator = ( GSTexture&& right ) noexcept
        {
#ifdef _DEBUG
            assert( LIST_EMPTY( this->list_header_primitives.root ) );
            assert( this->texels == nullptr );
#endif //_DEBUG

            move_from_properties( std::move( right ) );

            return *this;
        }

    private:
        inline void FreeTexels( Interface *engineInterface )
        {
            if ( void *texels = this->texels )
            {
                engineInterface->PixelFree( texels );

                this->texels = nullptr;
                this->dataSize = 0;
            }
        }

    public:
        // Do this before killing a GSTexture.
        inline void Cleanup( Interface *rwEngine )
        {
            while ( LIST_EMPTY( this->list_header_primitives.root ) == false )
            {
                GSPrimitive *prim = LIST_GETITEM( GSPrimitive, this->list_header_primitives.root.next, listNode );

                LIST_REMOVE( prim->listNode );

                rwEngine->MemFree( prim );
            }

            this->FreeTexels( rwEngine );
        }

        inline void DetachTexels( void )
        {
            this->texels = nullptr;
        }

        inline void CopyTexture( Interface *engineInterface, const GSTexture& right )
        {
            // Copy over image data.
            void *newTexels = nullptr;

            uint32 dataSize = right.dataSize;

            if ( dataSize != 0 )
            {
                const void *srcTexels = right.texels;

                newTexels = engineInterface->PixelAllocate( dataSize );

                memcpy( newTexels, srcTexels, dataSize );
            }
            this->texels = newTexels;
            this->dataSize = dataSize;

            // Copy primitives (complicated task, need to check type of each primitive and then copy mem).
            LIST_FOREACH_BEGIN( GSPrimitive, this->list_header_primitives.root, listNode )

                GSPrimitive *prim = item->clone( engineInterface );

                LIST_APPEND( this->list_header_primitives.root, prim->listNode );

            LIST_FOREACH_END

            // Copy over the dimensions.
            this->width = right.width;
            this->height = right.height;

            // Copy over encoding properties.
            this->transmissionOffsetX = right.transmissionOffsetX;
            this->transmissionOffsetY = right.transmissionOffsetY;
            this->transmissionAreaWidth = right.transmissionAreaWidth;
            this->transmissionAreaHeight = right.transmissionAreaHeight;
            this->textureBasePointer = right.textureBasePointer;
            this->textureBufferWidth = right.textureBufferWidth;
        }

        uint32 getDataSize( eMemoryLayoutType type ) const
        {
            // Since the texture dimension are power of two, this is actually correct.
            // The PlayStation 2 does not use the row alignment concept anyway.
            // Instead it has a special memory pattern that must be upkept.
            uint32 encodedTexItems = ( this->transmissionAreaWidth * this->transmissionAreaHeight );

            uint32 encodingDepth = GetMemoryLayoutTypeGIFPackingDepth( type );

			return ( ALIGN_SIZE( encodedTexItems * encodingDepth, 8u ) / 8 );
        }

        // Valid for GIFtags version of reading/writing.
        uint32 readGIFPacket( Interface *engineInterface, BlockProvider& inputProvider, TextureBase *readFromTexture );
        uint32 writeGIFPacket( Interface *engineInterface, BlockProvider& outputProvider, TextureBase *writeToTexture ) const;

        // Valid for textures with GIFtags disabled (no allocation performed yet).
        uint32 readRawTexels( Interface *rwEngine, eMemoryLayoutType memLayoutType, BlockProvider& inputProvider );
        uint32 writeRawTexels( Interface *rwEngine, BlockProvider& outputProvider );

        // Writes a GS register value to this texture.
        // If GS register entries of this type already exist then
        // the last one of the requested type is overridden.
        void setGSRegister( Interface *rwEngine, eGSRegister regType, uint64 value );

        // Members.
        uint32 width, height;
        uint32 dataSize;
        void *texels;           // holds either indices or color values

        // Cached GS register values.
        // If you update these then do not forget to update the register values aswell!
        uint32 transmissionAreaWidth, transmissionAreaHeight;
        uint32 transmissionOffsetX, transmissionOffsetY;

        // Values cached from bundled registers (MIPTBP1 and MIPTBP2 namely).
        uint32 textureBasePointer;
        uint32 textureBufferWidth;

        // Primitives in order that preceed this mipmap layer.
        RwList <GSPrimitive> list_header_primitives;
    };

    DEFINE_HEAP_REDIR_ALLOC_IMPL( mipRedirAlloc );

    // mipmaps are GSTextures.
    eir::Vector <GSTexture, mipRedirAlloc> mipmaps;

    eRasterFormatPS2 rasterFormat;

    uint32 depth;

    GSTexture paletteTex;

    ePaletteTypePS2 paletteType;

    uint32 recommendedBufferBasePointer;

    // Encoding type of all mipmaps.
    eMemoryLayoutType pixelStorageMode;
    eTextureColorComponent texColorComponent;
    eCLUTMemoryLayoutType clutStorageFmt;
    eCLUTStorageMode clutStorageMode;
    uint8 clutEntryOffset;
    uint8 clutLoadControl;
    uint32 clutOffset;
    eMemoryLayoutType bitblt_targetFmt;
    uint32 texMemSize;

    eTextureFunction textureFunction;
    uint8 lodCalculationModel;

    uint16 encodingVersion;     // PS2 encoding version.
    bool autoMipmaps;

    uint32 skyMipMapVal;

    uint8 rasterType;
    uint8 privateFlags;


private:
    bool allocateTextureMemoryNative(
        uint32 mipmapBasePointer[], uint32 mipmapBufferWidth[], ps2MipmapTransmissionData mipmapTransData[], uint32 maxMipmaps,
        uint32& clutBasePointer, ps2MipmapTransmissionData& clutTransData,
        uint32& textureBufPageSizeOut
    ) const;

public:
    bool allocateTextureMemory(
        uint32 mipmapBasePointer[], uint32 mipmapBufferWidth[], ps2MipmapTransmissionData mipmapTransData[], uint32 maxMipmaps,
        uint32& clutBasePointer, ps2MipmapTransmissionData& clutTransData,
        uint32& textureBufPageSizeOut
    ) const;

    bool generatePS2GPUData(
        LibraryVersion gameVersion,
        ps2GSRegisters& gpuData,
        const uint32 mipmapBasePointer[], const uint32 mipmapBufferWidth[], uint32 maxMipmaps,
        eMemoryLayoutType memLayoutType,
        uint32 clutBasePointer
    ) const;

    // Call this function whenever special properties of the PS2 texture have changed.
    void UpdateStructure( Interface *engineInterface );

    bool getDebugBitmap( Bitmap& bmpOut ) const;
};

RW_IMPL_HEAP_REDIR_ALLOC_INTERFACE_PUSH( NativeTexturePS2, mipRedirAlloc, mipmaps, engineInterface )

static inline void getPS2NativeTextureSizeRules( nativeTextureSizeRules& rulesOut )
{
    rulesOut.powerOfTwo = true;
    rulesOut.squared = false;
    rulesOut.maximum = true;
    rulesOut.maxVal = 1024;
}

struct ps2NativeTextureTypeProvider : public texNativeTypeProvider, public ps2public::ps2NativeTextureDriverInterface
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        new (objMem) NativeTexturePS2( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize )
    {
        new (objMem) NativeTexturePS2( *(const NativeTexturePS2*)srcObjMem );
    }

    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        ( *(NativeTexturePS2*)objMem ).~NativeTexturePS2();
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
        storeCaps.pixelCaps.supportsPalette = true;

        storeCaps.isCompressedFormat = false;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut );
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut );
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate );

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version )
    {
        NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

        nativeTex->texVersion = version;

        nativeTex->UpdateStructure( engineInterface );
    }

    LibraryVersion GetTextureVersion( const void *objMem )
    {
        const NativeTexturePS2 *nativeTex = (const NativeTexturePS2*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        const NativeTexturePS2 *nativeTex = (const NativeTexturePS2*)objMem;

        return GetGenericRasterFormatFromPS2( nativeTex->rasterFormat );
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        const NativeTexturePS2 *nativeTex = (const NativeTexturePS2*)objMem;

        return GetGenericPaletteTypeFromPS2( nativeTex->paletteType );
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        return false;
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        return RWCOMPRESS_NONE;
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override;

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // This is kind of a tricky one. I believe that PlayStation 2 native textures do not use
        // any row alignment. I could be wrong tho. We are safe if we decide for 4 byte alignment.
        // Just report back to us if there is any issue. :-)
        return 4;
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        getPS2NativeTextureSizeRules( rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // The PlayStation 2 native texture does not change size rules, thankfully.
        getPS2NativeTextureSizeRules( rulesOut );
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // Always the generic PlayStation 2 driver.
        return 6;
    }

    void* GetDriverNativeInterface( void ) const override
    {
        ps2public::ps2NativeTextureDriverInterface *intf = const_cast <ps2NativeTextureTypeProvider*> ( this );

        return intf;
    }

    // Implemened in a submodule.
    bool CalculateTextureMemoryAllocation(
        // Input values:
        ps2public::eMemoryLayoutType pixelStorageFmt, ps2public::eMemoryLayoutType bitbltTransferFmt, ps2public::eCLUTMemoryLayoutType clutStorageFmt,
        const ps2public::ps2TextureMetrics *mipmaps, size_t numMipmaps, const ps2public::ps2TextureMetrics& clut,
        // Output values:
        uint32 mipmapBasePointer[], uint32 mipmapBufferWidth[], ps2public::ps2MipmapTransmissionData mipmapTransData[],
        uint32& clutBasePointerOut, ps2public::ps2MipmapTransmissionData& clutTransDataOut,
        uint32& textureBufPageSizeOut
    ) override;

    inline void Initialize( Interface *engineInterface )
    {
        // For the native texture driver API.
        this->rwEngine = engineInterface;

        RegisterNativeTextureType( engineInterface, "PlayStation2", this, sizeof( NativeTexturePS2 ), alignof( NativeTexturePS2 ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "PlayStation2" );
    }

    Interface *rwEngine;
};

static inline void getPaletteTextureDimensions(ePaletteTypePS2 paletteType, LibraryVersion version, uint32& width, uint32& height)
{
    if (paletteType == ePaletteTypePS2::PAL4)
    {
        if (version.rwLibMinor <= 1)
        {
            width = 8;
            height = 2;
        }
        else
        {
            width = 8;
            height = 3;
        }
    }
    else if (paletteType == ePaletteTypePS2::PAL8)
    {
        width = 16;
        height = 16;
    }
    else
    {
        throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
    }
}

AINLINE eMemoryLayoutType GetBITBLTMemoryLayoutType( eMemoryLayoutType basePixelStorageFormat, const LibraryVersion& libraryVersion )
{
    if ( basePixelStorageFormat == PSMT8 )
    {
        return PSMCT32;
    }
    else if ( basePixelStorageFormat == PSMT4 )
    {
        if ( libraryVersion.isNewerThan( rw::LibraryVersion( 3, 3, 0, 0 ), true ) )
        {
            return PSMCT16;
        }
        else
        {
            return PSMT4;
        }
    }
    else
    {
        return basePixelStorageFormat;
    }
}

}

#endif //RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2
