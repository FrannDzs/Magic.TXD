/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwprivate.txd.h
*  PURPOSE:     RenderWare private global inclue file about textures.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_PRIVATE_TEXDICT_
#define _RENDERWARE_PRIVATE_TEXDICT_

#include "rwprivate.txd.pixelformat.h"

namespace rw
{

// Pixel capabilities are required for transporting data properly.
struct pixelCapabilities
{
    inline pixelCapabilities( void )
    {
        this->supportsDXT1 = false;
        this->supportsDXT2 = false;
        this->supportsDXT3 = false;
        this->supportsDXT4 = false;
        this->supportsDXT5 = false;
        this->supportsPalette = false;
    }

    bool supportsDXT1;
    bool supportsDXT2;
    bool supportsDXT3;
    bool supportsDXT4;
    bool supportsDXT5;
    bool supportsPalette;
};

struct storageCapabilities
{
    inline storageCapabilities( void )
    {
        this->isCompressedFormat = false;
    }

    pixelCapabilities pixelCaps;

    bool isCompressedFormat;    // if true then this texture does not store raw texel data.
};

struct pixelFormat
{
    inline pixelFormat( void )
    {
        this->rasterFormat = RASTER_DEFAULT;
        this->depth = 0;
        this->rowAlignment = 0;
        this->colorOrder = COLOR_RGBA;
        this->paletteType = PALETTE_NONE;
        this->compressionType = RWCOMPRESS_NONE;
    }

    eRasterFormat rasterFormat;
    uint32 depth;
    uint32 rowAlignment;
    eColorOrdering colorOrder;
    ePaletteType paletteType;
    eCompressionType compressionType;
};

struct pixelDataTraversal
{
    inline pixelDataTraversal( void )
    {
        this->isNewlyAllocated = false;
        this->rasterFormat = RASTER_DEFAULT;
        this->depth = 0;
        this->rowAlignment = 0;
        this->colorOrder = COLOR_RGBA;
        this->paletteType = PALETTE_NONE;
        this->paletteData = nullptr;
        this->paletteSize = 0;
        this->compressionType = RWCOMPRESS_NONE;
        this->hasAlpha = false;
        this->autoMipmaps = false;
        this->cubeTexture = false;
        this->rasterType = 4;
    }

    void CloneFrom( Interface *engineInterface, const pixelDataTraversal& right );

    void FreePixels( Interface *engineInterface );

    inline void SetStandalone( void )
    {
        // Standalone pixels mean that they do not belong to any texture container anymore.
        // If we are the only owner, we must make sure that we free them.
        // This function was introduced to defeat a memory leak.
        this->isNewlyAllocated = true;
    }

    inline void DetachPixels( void )
    {
        if ( this->isNewlyAllocated )
        {
            this->mipmaps.Clear();
            this->paletteData = nullptr;

            this->isNewlyAllocated = false;
        }
    }

    // Mipmaps.
    struct mipmapResource
    {
        inline mipmapResource( void )
        {
            // Debug fill the mipmap struct.
            this->texels = nullptr;
            this->width = 0;
            this->height = 0;
            this->layerWidth = 0;
            this->layerHeight = 0;
            this->dataSize = 0;
        }

        void Free( Interface *engineInterface );

        void *texels;
        uint32 width, height;
        uint32 layerWidth, layerHeight; // do not update these fields.

        uint32 dataSize;
    };

    static void FreeMipmap( Interface *engineInterface, mipmapResource& mipData );

    typedef rwStaticVector <mipmapResource> mipmaps_t;

    mipmaps_t mipmaps;

    bool isNewlyAllocated;          // counts for all mipmap layers and palette data.
    eRasterFormat rasterFormat;
    uint32 depth;
    uint32 rowAlignment;
    eColorOrdering colorOrder;
    ePaletteType paletteType;
    void *paletteData;
    uint32 paletteSize;
    eCompressionType compressionType;

    // More advanced properties.
    bool hasAlpha;
    bool autoMipmaps;
    bool cubeTexture;
    uint8 rasterType;
};

enum eTexNativeCompatibility
{
    RWTEXCOMPAT_NONE,
    RWTEXCOMPAT_MAYBE,
    RWTEXCOMPAT_ABSOLUTE
};

struct rawMipmapLayer
{
    pixelDataTraversal::mipmapResource mipData;

    eRasterFormat rasterFormat;
    uint32 depth;
    uint32 rowAlignment;
    eColorOrdering colorOrder;
    ePaletteType paletteType;
    void *paletteData;
    uint32 paletteSize;
    eCompressionType compressionType;

    bool hasAlpha;

    bool isNewlyAllocated;
};

struct rawBitmapFetchResult
{
    void *texelData;
    uint32 dataSize;
    uint32 width, height;
    bool isNewlyAllocated;
    uint32 depth;
    uint32 rowAlignment;
    eRasterFormat rasterFormat;
    eColorOrdering colorOrder;
    void *paletteData;
    uint32 paletteSize;
    ePaletteType paletteType;

    void FreePixels( Interface *engineInterface )
    {
        engineInterface->PixelFree( this->texelData );

        this->texelData = nullptr;

        if ( void *paletteData = this->paletteData )
        {
            engineInterface->PixelFree( paletteData );

            this->paletteData = nullptr;
        }
    }
};

// Quick palette depth remapper.
void ConvertPaletteDepthEx(
    const void *srcTexels, void *dstTexels,
    uint32 srcTexelOffX, uint32 srcTexelOffY,
    uint32 dstTexelOffX, uint32 dstTexelOffY,
    uint32 texWidth, uint32 texHeight,
    uint32 texProcessWidth, uint32 texProcessHeight,
    ePaletteType srcPaletteType, ePaletteType dstPaletteType,
    uint32 paletteSize,
    uint32 srcDepth, uint32 dstDepth,
    uint32 srcRowAlignment, uint32 dstRowAlignment
);

void ConvertPaletteDepth(
    const void *srcTexels, void *dstTexels,
    uint32 texWidth, uint32 texHeight,
    ePaletteType srcPaletteType, ePaletteType dstPaletteType,
    uint32 paletteSize,
    uint32 srcDepth, uint32 dstDepth,
    uint32 srcRowAlignment, uint32 dstRowAlignment
);

// Private pixel manipulation API.
void TransformMipmapLayer(
    Interface *engineInterface,
    uint32 surfWidth, uint32 surfHeight, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 srcDataSize,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType,
    bool hasSurfaceRowFormatChanged,
    void*& dstTexelsOut, uint32& dstDataSizeOut
);

bool ConvertMipmapLayerNative(
    Interface *engineInterface,
    uint32 mipWidth, uint32 mipHeight, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 srcDataSize,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType, const void *dstPaletteData, uint32 dstPaletteSize, eCompressionType dstCompressionType,
    bool copyAnyway,
    uint32& dstPlaneWidthOut, uint32& dstPlaneHeightOut,
    void*& dstTexelsOut, uint32& dstDataSizeOut
);

bool ConvertMipmapLayerEx(
    Interface *engineInterface,
    const pixelDataTraversal::mipmapResource& mipLayer,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType, const void *dstPaletteData, uint32 dstPaletteSize, eCompressionType dstCompressionType,
    bool copyAnyway,
    uint32& dstPlaneWidthOut, uint32& dstPlaneHeightOut,
    void*& dstTexelsOut, uint32& dstDataSizeOut
);

// Variant of mipmap surface conversion that always allocates a new destination buffer.
void CopyTransformRawMipmapLayer(
    Interface *engineInterface,
    uint32 surfWidth, uint32 surfHeight, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 srcDataSize,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType,
    void*& dstTexelsOut, uint32& dstDataSizeOut
);

void DepthCopyTransformMipmapLayer(
    Interface *engineInterface,
    uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 srcDataSize,
    uint32 srcDepth, uint32 srcRowAlignment, eir::eByteAddressingMode srcByteAddr,
    uint32 dstDepth, uint32 dstRowAlignment, eir::eByteAddressingMode dstByteAddr,
    void*& dstTexelsOut, uint32& dstDataSizeOut
);

// Automatic optimal compression decision algorithm for RenderWare extensions.
bool DecideBestDXTCompressionFormat(
    Interface *engineInterface,
    bool srcHasAlpha,
    bool supportsDXT1, bool supportsDXT2, bool supportsDXT3, bool supportsDXT4, bool supportsDXT5,
    float quality,
    eCompressionType& dstCompressionTypeOut
);

// Palette conversion helper functions.
void ConvertPaletteData(
    const void *srcPaletteTexels, void *dstPaletteTexels,
    uint32 srcPaletteSize, uint32 dstPaletteSize,
    eRasterFormat srcRasterFormat, eColorOrdering srcColorOrder, uint32 srcPalRasterDepth,
    eRasterFormat dstRasterFormat, eColorOrdering dstColorOrder, uint32 dstPalRasterDepth
);

void TransformPaletteDataEx(
    Interface *engineInterface,
    void *srcPaletteData,
    uint32 srcPaletteSize, uint32 dstPaletteSize,
    eRasterFormat srcRasterFormat, eColorOrdering srcColorOrder, uint32 srcPalRasterDepth,
    eRasterFormat dstRasterFormat, eColorOrdering dstColorOrder, uint32 dstPalRasterDepth,
    bool canTransformIntoSource,
    void*& dstPalDataOut
);

void TransformPaletteData(
    Interface *engineInterface,
    void *srcPaletteData,
    uint32 srcPaletteSize, uint32 dstPaletteSize,
    eRasterFormat srcRasterFormat, eColorOrdering srcColorOrder,
    eRasterFormat dstRasterFormat, eColorOrdering dstColorOrder,
    bool canTransformIntoSource,
    void*& dstPalDataOut
);

void CopyConvertPaletteData(
    Interface *engineInterface,
    const void *srcPaletteData,
    uint32 srcPaletteSize, uint32 dstPaletteSize,
    eRasterFormat srcRasterFormat, eColorOrdering srcColorOrder,
    eRasterFormat dstRasterFormat, eColorOrdering dstColorOrder,
    void*& dstPaletteDataOut
);

bool ConvertPixelData( Interface *engineInterface, pixelDataTraversal& pixelsToConvert, const pixelFormat pixFormat );
bool ConvertPixelDataDeferred( Interface *engineInterface, const pixelDataTraversal& srcPixels, pixelDataTraversal& dstPixels, const pixelFormat pixFormat );

} // namespace rw

#endif //_RENDERWARE_PRIVATE_TEXDICT_