/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.common.hxx
*  PURPOSE:     Common TXD reading definitions and structures
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_COMMON_UTILITIES_PRIVATE_
#define _RENDERWARE_COMMON_UTILITIES_PRIVATE_

#include "rwserialize.hxx"

#include "pluginutil.hxx"

namespace rw
{

struct texDictionaryStreamPlugin : public serializationProvider
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        txdTypeInfo = engineInterface->typeSystem.RegisterStructType <TexDictionary> ( "texture_dictionary", engineInterface->rwobjTypeInfo );

        if ( txdTypeInfo )
        {
            // Register ourselves.
            RegisterSerialization( engineInterface, CHUNK_TEXDICTIONARY, L"TEXDICT_FRIENDLYNAME", txdTypeInfo, this, RWSERIALIZE_ISOF );
        }
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        if ( RwTypeSystem::typeInfoBase *txdTypeInfo = this->txdTypeInfo )
        {
            // Unregister us again.
            UnregisterSerialization( engineInterface, CHUNK_TEXDICTIONARY );

            engineInterface->typeSystem.DeleteType( txdTypeInfo );
        }
    }

    // Creation functions.
    TexDictionary*  CreateTexDictionary( EngineInterface *engineInterface ) const;
    TexDictionary*  ToTexDictionary( EngineInterface *engineInterface, RwObject *rwObj );
    const TexDictionary* ToConstTexDictionary( EngineInterface *engineInterface, const RwObject *rwObj );

    // Serialization functions.
    void        Serialize( Interface *engineInterface, BlockProvider& outputProvider, void *objectToSerialize ) const override;
    void        Deserialize( Interface *engineInterface, BlockProvider& inputProvider, void *objectToDeserialize ) const override;

    RwTypeSystem::typeInfoBase *txdTypeInfo;
};

typedef PluginDependantStructRegister <texDictionaryStreamPlugin, RwInterfaceFactory_t> texDictionaryStreamPluginRegister_t;

extern optional_struct_space <texDictionaryStreamPluginRegister_t> texDictionaryStreamStore;


inline void fixFilteringMode(TextureBase& inTex, uint32 mipmapCount)
{
    eRasterStageFilterMode currentFilterMode = inTex.GetFilterMode();

    eRasterStageFilterMode newFilterMode = currentFilterMode;

    if ( mipmapCount > 1 )
    {
        if ( currentFilterMode == RWFILTER_POINT )
        {
            newFilterMode = RWFILTER_POINT_POINT;
        }
        else if ( currentFilterMode == RWFILTER_LINEAR )
        {
            newFilterMode = RWFILTER_LINEAR_LINEAR;
        }
    }
    else
    {
        if ( currentFilterMode == RWFILTER_POINT_POINT ||
             currentFilterMode == RWFILTER_POINT_LINEAR )
        {
            newFilterMode = RWFILTER_POINT;
        }
        else if ( currentFilterMode == RWFILTER_LINEAR_POINT ||
                  currentFilterMode == RWFILTER_LINEAR_LINEAR )
        {
            newFilterMode = RWFILTER_LINEAR;
        }
    }

    // If the texture requires a different filter mode, set it.
    if ( currentFilterMode != newFilterMode )
    {
        Interface *engineInterface = inTex.engineInterface;

        bool ignoreSecureWarnings = engineInterface->GetIgnoreSecureWarnings();

        int warningLevel = engineInterface->GetWarningLevel();

        if ( ignoreSecureWarnings == false )
        {
            // Since this is a really annoying message, put it on warning level 4.
            if ( warningLevel >= 4 )
            {
                engineInterface->PushWarningObject( &inTex, L"TEXTURE_WARN_INVFILTERINGMODEFIX" );
            }
        }

        // Fix it.
        inTex.SetFilterMode( newFilterMode );
    }
}

// Processor of mipmap levels.
struct mipGenLevelGenerator
{
private:
    uint32 mipWidth, mipHeight;
    bool generateToMinimum;

    bool hasIncrementedWidth;
    bool hasIncrementedHeight;

public:
    inline mipGenLevelGenerator( uint32 baseWidth, uint32 baseHeight )
    {
        this->mipWidth = baseWidth;
        this->mipHeight = baseHeight;

        this->generateToMinimum = true;//false;

        this->hasIncrementedWidth = false;
        this->hasIncrementedHeight = false;
    }

    inline void setGenerateToMinimum( bool doGenMin )
    {
        this->generateToMinimum = doGenMin;
    }

    inline uint32 getLevelWidth( void ) const
    {
        return this->mipWidth;
    }

    inline uint32 getLevelHeight( void ) const
    {
        return this->mipHeight;
    }

    inline bool canIncrementDimm( uint32 dimm )
    {
        return ( dimm != 1 );
    }

    inline bool incrementLevel( void )
    {
        bool couldIncrementLevel = false;

        uint32 curMipWidth = this->mipWidth;
        uint32 curMipHeight = this->mipHeight;

        bool doGenMin = this->generateToMinimum;

        if ( doGenMin )
        {
            if ( canIncrementDimm( curMipWidth ) || canIncrementDimm( curMipHeight ) )
            {
                couldIncrementLevel = true;
            }
        }
        else
        {
            if ( canIncrementDimm( curMipWidth ) && canIncrementDimm( curMipHeight ) )
            {
                couldIncrementLevel = true;
            }
        }

        if ( couldIncrementLevel )
        {
            bool hasIncrementedWidth = false;
            bool hasIncrementedHeight = false;

            if ( canIncrementDimm( curMipWidth ) )
            {
                this->mipWidth = curMipWidth / 2;

                hasIncrementedWidth = true;
            }

            if ( canIncrementDimm( curMipHeight ) )
            {
                this->mipHeight = curMipHeight / 2;

                hasIncrementedHeight = true;
            }

            // Update feedback parameters.
            this->hasIncrementedWidth = hasIncrementedWidth;
            this->hasIncrementedHeight = hasIncrementedHeight;
        }

        return couldIncrementLevel;
    }

    inline bool isValidLevel( void ) const
    {
        return ( this->mipWidth != 0 && this->mipHeight != 0 );
    }

    inline bool didIncrementWidth( void ) const
    {
        return this->hasIncrementedWidth;
    }

    inline bool didIncrementHeight( void ) const
    {
        return this->hasIncrementedHeight;
    }
};

inline eRasterFormat getVirtualRasterFormat( bool hasAlpha, eCompressionType rwCompressionType )
{
    eRasterFormat rasterFormat = RASTER_DEFAULT;

    if ( rwCompressionType == RWCOMPRESS_DXT1 )
    {
	    if (hasAlpha)
        {
		    rasterFormat = RASTER_1555;
        }
	    else
        {
		    rasterFormat = RASTER_565;
        }
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT2 ||
              rwCompressionType == RWCOMPRESS_DXT3 )
    {
        rasterFormat = RASTER_4444;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT4 )
    {
        rasterFormat = RASTER_4444;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT5 )
    {
        rasterFormat = RASTER_4444;
    }

    return rasterFormat;
}

// Utilities used for reading native texture contents.
// Those are all sample types from RenderWare version 3.
enum rwSerializedRasterFormat_3
{
    RWFORMAT3_UNKNOWN,
    RWFORMAT3_1555,
    RWFORMAT3_565,
    RWFORMAT3_4444,
    RWFORMAT3_LUM8,
    RWFORMAT3_8888,
    RWFORMAT3_888,
    RWFORMAT3_16,
    RWFORMAT3_24,
    RWFORMAT3_32,
    RWFORMAT3_555
};

enum rwSerializedPaletteType
{
    RWSER_PALETTE_NONE,
    RWSER_PALETTE_PAL8,
    RWSER_PALETTE_PAL4
};

inline rwSerializedPaletteType genericFindPaletteTypeForFramework( ePaletteType palType )
{
    if ( palType == PALETTE_4BIT )
    {
        return RWSER_PALETTE_PAL4;
    }
    else if ( palType == PALETTE_8BIT )
    {
        return RWSER_PALETTE_PAL8;
    }

    return RWSER_PALETTE_NONE;
}

inline ePaletteType genericFindPaletteTypeForSerialized( rwSerializedPaletteType palType )
{
    if ( palType == RWSER_PALETTE_PAL8 )
    {
        return PALETTE_8BIT;
    }
    else if ( palType == RWSER_PALETTE_PAL4 )
    {
        return PALETTE_4BIT;
    }

    return PALETTE_NONE;
}

inline rwSerializedRasterFormat_3 genericFindRasterFormatForFramework( eRasterFormat rasterFormat )
{
    rwSerializedRasterFormat_3 serFormat = RWFORMAT3_UNKNOWN;

    if ( rasterFormat != RASTER_DEFAULT )
    {
        if ( rasterFormat == RASTER_1555 )
        {
            serFormat = RWFORMAT3_1555;
        }
        else if ( rasterFormat == RASTER_565 )
        {
            serFormat = RWFORMAT3_565;
        }
        else if ( rasterFormat == RASTER_4444 )
        {
            serFormat = RWFORMAT3_4444;
        }
        else if ( rasterFormat == RASTER_LUM )
        {
            serFormat = RWFORMAT3_LUM8;
        }
        else if ( rasterFormat == RASTER_8888 )
        {
            serFormat = RWFORMAT3_8888;
        }
        else if ( rasterFormat == RASTER_888 )
        {
            serFormat = RWFORMAT3_888;
        }
        else if ( rasterFormat == RASTER_16 )
        {
            serFormat = RWFORMAT3_16;
        }
        else if ( rasterFormat == RASTER_24 )
        {
            serFormat = RWFORMAT3_24;
        }
        else if ( rasterFormat == RASTER_32 )
        {
            serFormat = RWFORMAT3_32;
        }
        else if ( rasterFormat == RASTER_555 )
        {
            serFormat = RWFORMAT3_555;
        }
        // otherwise, well we failed.
        // snap, we dont have a format!
        // hopefully the implementation knows what it is doing!
    }

    return serFormat;
}

inline eRasterFormat genericFindRasterFormatForSerialized( rwSerializedRasterFormat_3 serFormat )
{
    // Map the serialized format to our raster format.
    // We should be backwards compatible to every RenderWare3 format.
    eRasterFormat formatOut = RASTER_DEFAULT;

    if ( serFormat == RWFORMAT3_1555 )
    {
        formatOut = RASTER_1555;
    }
    else if ( serFormat == RWFORMAT3_565 )
    {
        formatOut = RASTER_565;
    }
    else if ( serFormat == RWFORMAT3_4444 )
    {
        formatOut = RASTER_4444;
    }
    else if ( serFormat == RWFORMAT3_LUM8 )
    {
        formatOut = RASTER_LUM;
    }
    else if ( serFormat == RWFORMAT3_8888 )
    {
        formatOut = RASTER_8888;
    }
    else if ( serFormat == RWFORMAT3_888 )
    {
        formatOut = RASTER_888;
    }
    else if ( serFormat == RWFORMAT3_16 )
    {
        formatOut = RASTER_16;
    }
    else if ( serFormat == RWFORMAT3_24 )
    {
        formatOut = RASTER_24;
    }
    else if ( serFormat == RWFORMAT3_32 )
    {
        formatOut = RASTER_32;
    }
    else if ( serFormat == RWFORMAT3_555 )
    {
        formatOut = RASTER_555;
    }
    // anything else will be an unknown raster mapping.

    return formatOut;
}

struct rwGenericRasterFormatFlags
{
    union
    {
        struct
        {
            uint32 rasterType : 4;
            uint32 privateFlags : 4;
            uint32 formatNum : 4;
            uint32 autoMipmaps : 1;
            uint32 palType : 2;
            uint32 hasMipmaps : 1;
            uint32 pad : 16;
        } data;

        uint32 value;
    };
};
static_assert( sizeof( rwGenericRasterFormatFlags ) == sizeof( uint32 ), "rwGenericRasterFormatFlags wrong size!" );

// Useful routine to generate generic raster format flags.
inline uint32 generateRasterFormatFlags( eRasterFormat rasterFormat, ePaletteType paletteType, bool hasMipmaps, bool autoMipmaps )
{
    rwGenericRasterFormatFlags rf_data;
    rf_data.value = 0;

    // bits 0..3 can be (alternatively) used for the raster type
    // bits 4..7 are stored in the raster private flags.

    // Map the raster format for RenderWare3.
    rf_data.data.formatNum = genericFindRasterFormatForFramework( rasterFormat );

    // Decide if we have a palette.
    rf_data.data.palType = genericFindPaletteTypeForFramework( paletteType );

    rf_data.data.hasMipmaps = hasMipmaps;
    rf_data.data.autoMipmaps = autoMipmaps;

    return rf_data.value;
}

// Useful routine to read generic raster format flags.
inline void readRasterFormatFlags( uint32 rasterFormatFlags, eRasterFormat& rasterFormat, ePaletteType& paletteType, bool& hasMipmaps, bool& autoMipmaps )
{
    rwGenericRasterFormatFlags rf_data;
    rf_data.value = rasterFormatFlags;

    // Read the format region of the raster format flags.
    rwSerializedRasterFormat_3 serFormat = (rwSerializedRasterFormat_3)rf_data.data.formatNum;

    rasterFormat = genericFindRasterFormatForSerialized( serFormat );

    // Process palette.
    rwSerializedPaletteType serPalType = (rwSerializedPaletteType)rf_data.data.palType;

    paletteType = genericFindPaletteTypeForSerialized( serPalType );

    hasMipmaps = rf_data.data.hasMipmaps;
    autoMipmaps = rf_data.data.autoMipmaps;
}

struct texFormatInfo
{
private:
    uint32 filterMode : 8;
    uint32 uAddressing : 4;
    uint32 vAddressing : 4;
    uint32 pad1 : 16;

public:
    void parse(TextureBase& theTexture, bool isLikelyToFail = false) const;
    void set(const TextureBase& inTex);

    void writeToBlock(BlockProvider& outputProvider) const;
    void readFromBlock(BlockProvider& inputProvider);
};

template <typename userType = endian::p_little_endian <uint32>>
struct texFormatInfo_serialized
{
    userType info;

    inline operator texFormatInfo ( void ) const
    {
        union
        {
            texFormatInfo formatOut;
            uint32 formatOut_uint32;
        };

        formatOut_uint32 = info;

        return formatOut;
    }

    inline texFormatInfo_serialized& operator = ( const texFormatInfo& right ) noexcept
    {
        info = *(uint32*)&right;

        return *this;
    }
};

struct wardrumFormatInfo
{
private:
    endian::p_little_endian <uint32> filterMode;
    endian::p_little_endian <uint32> uAddressing;
    endian::p_little_endian <uint32> vAddressing;

public:
    void parse(TextureBase& theTexture) const;
    void set(const TextureBase& inTex );
};

};

#endif //_RENDERWARE_COMMON_UTILITIES_PRIVATE_
