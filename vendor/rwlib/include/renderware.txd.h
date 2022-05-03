/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.txd.h
*  PURPOSE:     TextureBase and TexDictionary object definitions.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

namespace rw
{

// Old object, do not use.
struct Texture : public RwObject
{
    inline Texture( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
    {
        this->filterFlags = 0;
        this->hasSkyMipmap = false;
    }

	uint32 filterFlags;
	std::string name;
	std::string maskName;

	/* Extensions */

	/* sky mipmap */
	bool hasSkyMipmap;

	/* functions */
	void read(std::istream &dff);
	uint32 write(std::ostream &dff);
	void readExtension(std::istream &dff);
	void dump(std::string ind = "");
};

enum eRasterStageFilterMode
{
    RWFILTER_DISABLE,
    RWFILTER_POINT,
    RWFILTER_LINEAR,
    RWFILTER_POINT_POINT,
    RWFILTER_LINEAR_POINT,
    RWFILTER_POINT_LINEAR,
    RWFILTER_LINEAR_LINEAR,
    RWFILTER_ANISOTROPY
};
enum eRasterStageAddressMode
{
    RWTEXADDRESS_WRAP = 1,
    RWTEXADDRESS_MIRROR,
    RWTEXADDRESS_CLAMP,
    RWTEXADDRESS_BORDER
};

struct rasterSizeRules
{
    inline rasterSizeRules( void )
    {
        this->powerOfTwo = false;
        this->squared = false;
        this->multipleOf = false;
        this->multipleOfValue = 0;
        this->maximum = false;
        this->maxVal = 0;
    }

    bool powerOfTwo;
    bool squared;
    bool multipleOf;
    uint32 multipleOfValue;
    bool maximum;
    uint32 maxVal;

    bool verifyDimensions( uint32 width, uint32 height ) const;
    void adjustDimensions( uint32 width, uint32 height, uint32& newWidth, uint32& newHeight ) const;
};

// This is our library-wide supported sample format enumeration.
// Feel free to extend this. You must not serialize this anywhere,
// since this type is version-agnostic.
enum eRasterFormat
{
    RASTER_DEFAULT,
    RASTER_1555,
    RASTER_565,
    RASTER_4444,
    RASTER_LUM,         // D3DFMT_L8 (can be 4 or 8 bit)
    RASTER_8888,
    RASTER_888,
    RASTER_16,          // D3DFMT_D16
    RASTER_24,          // D3DFMT_D24X8
    RASTER_32,          // D3DFMT_D32
    RASTER_555,         // D3DFMT_X1R5G5B5

    // New types. :)
    RASTER_LUM_ALPHA
};

// Remember to update this function whenever a new format emerges!
inline bool canRasterFormatHaveAlpha(eRasterFormat rasterFormat)
{
    if (rasterFormat == RASTER_1555 ||
        rasterFormat == RASTER_4444 ||
        rasterFormat == RASTER_8888 ||
        // NEW FORMATS.
        rasterFormat == RASTER_LUM_ALPHA )
    {
        return true;
    }
    
    return false;
}

enum ePaletteType
{
    PALETTE_NONE,
    PALETTE_4BIT,
    PALETTE_8BIT,
    PALETTE_4BIT_LSB    // same as 4BIT, but different addressing order
};

enum eColorOrdering
{
    COLOR_RGBA,
    COLOR_BGRA,
    COLOR_ABGR,
    COLOR_ARGB,
    COLOR_BARG
};

// utility to calculate palette item count.
inline uint32 getPaletteItemCount( ePaletteType paletteType )
{
    uint32 count = 0;

    if ( paletteType == PALETTE_4BIT ||
         paletteType == PALETTE_4BIT_LSB )
    {
        count = 16;
    }
    else if ( paletteType == PALETTE_8BIT )
    {
        count = 256;
    }
    else if ( paletteType == PALETTE_NONE )
    {
        count = 0;
    }
    else
    {
        assert( 0 );
    }

    return count;
}

} // namespace rw

#include "renderware.bmp.h"

namespace rw
{

// Texture container per platform for specialized color data.
// You must not make any assumptions backed by nothing
// about the data that is pointed at by this.
typedef void* PlatformTexture;

// Native GTA:SA feature map:
// no RASTER_PAL4 support at all.
// if RASTER_PAL8, then only RASTER_8888 and RASTER_888
// else 

enum eMipmapGenerationMode
{
    MIPMAPGEN_DEFAULT,
    MIPMAPGEN_CONTRAST,
    MIPMAPGEN_BRIGHTEN,
    MIPMAPGEN_DARKEN,
    MIPMAPGEN_SELECTCLOSE
};

enum eRasterType
{
    RASTERTYPE_DEFAULT,     // same as 4
    RASTERTYPE_BITMAP = 4
};

enum eCompressionType
{
    RWCOMPRESS_NONE,
    RWCOMPRESS_DXT1,
    RWCOMPRESS_DXT2,
    RWCOMPRESS_DXT3,
    RWCOMPRESS_DXT4,
    RWCOMPRESS_DXT5,
    RWCOMPRESS_NUM
};

struct Raster
{
    inline Raster( Interface *engineInterface, void *construction_params )
    {
        this->engineInterface = engineInterface;
        this->platformData = nullptr;
        this->refCount = 1;
        this->constRefCount = 0;
    }

    Raster( const Raster& right );
    ~Raster( void );

    void SetEngineVersion( LibraryVersion version );
    LibraryVersion GetEngineVersion( void ) const;

    Interface* getEngine( void );

    bool supportsImageMethod(const char *method) const;
    void writeImage(rw::Stream *outputStream, const char *method);
    void readImage(rw::Stream *inputStream);

    Bitmap getBitmap(void) const;
    void setImageData(const Bitmap& srcImage);

    void resize(uint32 width, uint32 height, const char *downsampleMode = nullptr, const char *upscaleMode = nullptr);

    void getSize(uint32& width, uint32& height) const;

    void newNativeData( const char *typeName );
    void clearNativeData( void );

    // Reference count for immutability across function calls.
    // Can be used to depend on the native data across function calls.
    void addConstRef( void );
    void remConstRef( void );
    bool isImmutable( void ) const;

    bool hasNativeDataOfType( const char *typeName ) const;
    const char* getNativeDataTypeName( void ) const;

    void* getNativeInterface( void );
    void* getDriverNativeInterface( void );

    void getSizeRules( rasterSizeRules& rulesOut ) const;

    void getFormatString( char *buf, size_t bufSize, size_t& lengthOut ) const;

	void convertToFormat(eRasterFormat format);
    void convertToPalette(ePaletteType paletteType, eRasterFormat newRasterFormat = RASTER_DEFAULT);

    eRasterFormat getRasterFormat( void ) const;
    ePaletteType getPaletteType( void ) const;

    // Optimization routines.
    void optimizeForLowEnd(float quality);
    void compress(float quality);
    void compressCustom(eCompressionType format);

    bool isCompressed( void ) const;
    eCompressionType getCompressionFormat( void ) const;

    // Mipmap utilities.
    uint32 getMipmapCount( void ) const;

    void clearMipmaps( void );
    void generateMipmaps( uint32 maxMipmapCount, eMipmapGenerationMode mipGenMode = MIPMAPGEN_DEFAULT );

    RW_NOT_DIRECTLY_CONSTRUCTIBLE;

    // GENERAL REMINDER: only the framework is allowed to access those fields directly!
    // They are only this open because of code-management reasons!

    // Data members.
    Interface *engineInterface;

    PlatformTexture *platformData;

    std::atomic <uint32> refCount;          // general life-time reference count

    std::atomic <uint32> constRefCount;     // if != 0, the native data is immutable
};

struct TexDictionary;
struct rwlock;

struct TextureBase : public RwObject
{
    friend struct TexDictionary;

    inline TextureBase( Interface *engineInterface, void *construction_params ) :
        RwObject( engineInterface, construction_params ),
        name( eir::constr_with_alloc::DEFAULT, engineInterface ),
        maskName( eir::constr_with_alloc::DEFAULT, engineInterface )
    {
        this->texRaster = nullptr;
        this->filterMode = RWFILTER_DISABLE;
        this->uAddressing = RWTEXADDRESS_WRAP;
        this->vAddressing = RWTEXADDRESS_WRAP;
        this->texDict = nullptr;
    }

    TextureBase( const TextureBase& right );
    ~TextureBase( void );

    void OnVersionChange( const LibraryVersion& oldVer, const LibraryVersion& newVer )
    {
        // If we have a raster, set its version aswell.
        if ( Raster *texRaster = this->texRaster )
        {
            texRaster->SetEngineVersion( newVer );
        }
    }

    // Make sure that you access the names under WRITE ACCESS or READ ACCESS (appr.) of the consistency lock!
    void SetName( const char *nameString )                      { this->name = nameString; }
    void SetMaskName( const char *nameString )                  { this->maskName = nameString; }

    const rwString <char>& GetName( void ) const                { return this->name; }
    const rwString <char>& GetMaskName( void ) const            { return this->maskName; }

    rwlock* GetLock( void ) const;

    void AddToDictionary( TexDictionary *dict );
    void RemoveFromDictionary( void );
    TexDictionary* GetTexDictionary( void ) const;

    void SetFilterMode( eRasterStageFilterMode filterMode );
    void SetUAddressing( eRasterStageAddressMode mode );
    void SetVAddressing( eRasterStageAddressMode mode );

    eRasterStageFilterMode GetFilterMode( void ) const;
    eRasterStageAddressMode GetUAddressing( void ) const;
    eRasterStageAddressMode GetVAddressing( void ) const;

    void improveFiltering(void);
    void fixFiltering(void);

    void SetRaster( Raster *texRaster );

    Raster* GetRaster( void ) const
    {
        return this->texRaster;
    }

private:
    // Pointer to the pixel data storage.
    Raster *texRaster;

	rwString <char> name;
	rwString <char> maskName;
	eRasterStageFilterMode filterMode;

    eRasterStageAddressMode uAddressing;
    eRasterStageAddressMode vAddressing;

    TexDictionary *texDict;

    // Must call under consistency lock WRITE ACCESS.
    // Do call only if sure that Texture is not inside TXD.
    void _LinkDictionary( TexDictionary *newDict );

    // Must call under consistency lock WRITE ACCESS.
    void _RemoveFromDictionaryNative( void );
    void _UnlinkDictionary( TexDictionary *belongingTXD );

    // Call only from TexDictionary, if absolutely sure texture
    // is owned by TXD.
    void UnlinkDictionary( void );

public:
    RwListEntry <TextureBase> texDictNode;
};

struct TexDictionary : public RwObject
{
    inline TexDictionary( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
    {
        this->hasRecommendedPlatform = false;
        this->recDevicePlatID = 0;  // zero means none.

        this->numTextures = 0;
    }

    TexDictionary( const TexDictionary& right );
	~TexDictionary( void );

    bool hasRecommendedPlatform;
    uint16 recDevicePlatID;

    uint32 numTextures;

    // List of all textures added to this TXD.
    // Be sure to lock the TXD when iterating through this list!
    RwList <TextureBase> textures;

private:
    DEF_LIST_ITER( _texIter_t, TextureBase, texDictNode );

public:
    struct texIter_t : public _texIter_t
    {
        inline texIter_t( RwList <TextureBase>& root, rwlock *theLock ) : _texIter_t( root ), ctxListIter( theLock )
        {
            return;
        }

        scoped_rwlock_reader <> ctxListIter;
    };

	/* functions */
	void clear( void );

    rwlock* GetLock( void ) const;

    inline texIter_t GetTextureIterator( void )
    {
        return texIter_t( this->textures, this->GetLock() );
    }

    uint32 GetTextureCount( void ) const;

    // Returns the recommended texture platform for this TXD archive.
    // Use this if you want to add textures in a format that the framework recommends.
    // Can be nullptr if there is no recommendation.
    const char* GetRecommendedDriverPlatform( void ) const;
};

} // namespace rw

// Include native texture extension public headers.
#include "renderware.txd.d3d.h"

namespace rw
{

// Plugin methods.
TexDictionary* CreateTexDictionary( Interface *engineInterface );
TexDictionary* ToTexDictionary( Interface *engineInterface, RwObject *rwObj );
const TexDictionary* ToConstTexDictionary( Interface *engineInterface, const RwObject *rwObj );

TextureBase* CreateTexture( Interface *engineInterface, Raster *theRaster );
TextureBase* ToTexture( Interface *engineInterface, RwObject *rwObj );
const TextureBase* ToConstTexture( Interface *engineInterface, const RwObject *rwObj );

typedef rwStaticVector <rwStaticString <char>> platformTypeNameList_t;

// Complex native texture API.
bool ConvertRasterTo( Raster *theRaster, const char *nativeName );

void* GetNativeTextureDriverInterface( Interface *engineInterface, const char *nativeName );

platformTypeNameList_t GetAvailableNativeTextureTypes( Interface *engineInterface );

struct nativeRasterFormatInfo
{
    // Basic information.
    bool isCompressedFormat;
    
    // Support flags.
    bool supportsDXT1;
    bool supportsDXT2;
    bool supportsDXT3;
    bool supportsDXT4;
    bool supportsDXT5;
    bool supportsPalette;
};

bool GetNativeTextureFormatInfo( Interface *engineInterface, const char *nativeName, nativeRasterFormatInfo& infoOut );
bool IsNativeTexture( Interface *engineInterface, const char *nativeName );

// Format info helper API.
const char* GetRasterFormatStandardName( eRasterFormat theFormat );
eRasterFormat FindRasterFormatByName( const char *name );

// Raster API.
Raster* CreateRaster( Interface *engineInterface );
Raster* CloneRaster( const Raster *rasterToClone );
Raster* AcquireRaster( Raster *theRaster );
void DeleteRaster( Raster *theRaster );

// Pixel manipulation API, exported for good compatibility.
// Use this API if you are not sure how to map the raster format stuff properly.
bool BrowseTexelRGBA(
    const void *texelSource, uint32 texelIndex,
    const rasterRowSize& rowSize, uint32 rowi,
    eRasterFormat rasterFormat, uint32 depth, eColorOrdering colorOrder, ePaletteType paletteType, const void *paletteData, uint32 paletteSize,
    uint8& redOut, uint8& greenOut, uint8& blueOut, uint8& alphaOut
);
bool BrowseTexelLuminance(
    const void *texelSource, uint32 texelIndex,
    const rasterRowSize& rowSize, uint32 rowi,
    eRasterFormat rasterFormat, uint32 depth, ePaletteType paletteType, const void *paletteData, uint32 paletteSize,
    uint8& lumOut, uint8& alphaOut
);

// Use this function to decide the functions you should use for color manipulation.
eColorModel GetRasterFormatColorModel( eRasterFormat rasterFormat );

bool PutTexelRGBA(
    void *texelSource, uint32 texelIndex,
    const rasterRowSize& rowSize, uint32 rowi,
    eRasterFormat rasterFormat, uint32 depth, eColorOrdering colorOrder,
    uint8 red, uint8 green, uint8 blue, uint8 alpha
);
bool PutTexelLuminance(
    void *texelSource, uint32 texelIndex,
    const rasterRowSize& rowSize, uint32 rowi,
    eRasterFormat rasterFormat, uint32 depth,
    uint8 lum, uint8 alpha
);

// Debug API.
bool DebugDrawMipmaps( Interface *engineInterface, Raster *debugRaster, Bitmap& drawSurface );

} // namespace rw