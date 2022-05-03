/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.miputil.hxx
*  PURPOSE:     Generic mipmap layer helpers.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "txdread.common.hxx"

namespace rw
{

// Sample template for mipmap manager (in this case D3D).
#if 0
struct d3dMipmapManager
{
    NativeTextureD3D *nativeTex;

    inline d3dMipmapManager( NativeTextureD3D *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureD3D::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void GetSizeRules( nativeTextureSizeRules& rulesOut ) const
    {
        // PUT SIZE RULE CALCULATION HERE.
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureD3D::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormatOut, eColorOrdering& dstColorOrderOut, uint32& dstDepthOut,
        uint32& dstRowAlignmentOut,
        ePaletteType& dstPaletteTypeOut, void*& dstPaletteDataOut, uint32& dstPaletteSizeOut,
        eCompressionType& dstCompressionTypeOut, bool& hasAlphaOut,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocatedOut
    )
    {

    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureD3D::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        uint32 rowAlignment,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {

    }
};
#endif

template <typename mipDataType, typename mipListType, typename mipManagerType>
inline bool virtualGetMipmapLayer(
    Interface *engineInterface, mipManagerType& mipManager,
    uint32 mipIndex,
    const mipListType& mipmaps, rawMipmapLayer& layerOut
)
{
    if ( mipIndex >= mipmaps.GetCount() )
        return false;

    const mipDataType& mipLayer = mipmaps[ mipIndex ];

    uint32 mipWidth, mipHeight;
    uint32 layerWidth, layerHeight;

    eRasterFormat rasterFormat;
    eColorOrdering colorOrder;
    uint32 depth;
    uint32 rowAlignment;

    ePaletteType paletteType;
    void *paletteData;
    uint32 paletteSize;

    eCompressionType compressionType;

    bool hasAlpha;

    void *texels;
    uint32 dataSize;

    bool isNewlyAllocated;
    bool isPaletteNewlyAllocated;

    mipManager.Deinternalize(
        engineInterface,
        mipLayer,
        mipWidth, mipHeight, layerWidth, layerHeight,
        rasterFormat, colorOrder, depth,
        rowAlignment,
        paletteType, paletteData, paletteSize,
        compressionType, hasAlpha,
        texels, dataSize,
        isNewlyAllocated, isPaletteNewlyAllocated
    );

    if ( isNewlyAllocated )
    {
        // Ensure that, if required, our palette is really newly allocated.
        if ( paletteType != PALETTE_NONE && isPaletteNewlyAllocated == false )
        {
            const void *paletteSource = paletteData;

            uint32 palRasterDepth = Bitmap::getRasterFormatDepth( rasterFormat );

            uint32 palDataSize = getPaletteDataSize( paletteSize, palRasterDepth );

            paletteData = engineInterface->PixelAllocate( palDataSize );

            memcpy( paletteData, paletteSource, palDataSize );
        }
    }

    layerOut.rasterFormat = rasterFormat;
    layerOut.depth = depth;
    layerOut.rowAlignment = rowAlignment;
    layerOut.colorOrder = colorOrder;

    layerOut.paletteType = paletteType;
    layerOut.paletteData = paletteData;
    layerOut.paletteSize = paletteSize;

    layerOut.compressionType = compressionType;

    layerOut.hasAlpha = hasAlpha;

    layerOut.mipData.width = mipWidth;
    layerOut.mipData.height = mipHeight;

    layerOut.mipData.layerWidth = layerWidth;
    layerOut.mipData.layerHeight = layerHeight;

    layerOut.mipData.texels = texels;
    layerOut.mipData.dataSize = dataSize;

    layerOut.isNewlyAllocated = isNewlyAllocated;

    return true;
}

inline bool getMipmapLayerDimensions( uint32 mipLevel, uint32 baseWidth, uint32 baseHeight, uint32& layerWidth, uint32& layerHeight )
{
    mipGenLevelGenerator mipGen( baseWidth, baseHeight );

    if ( mipGen.isValidLevel() == false )
        return false;

    for ( uint32 n = 0; n < mipLevel; n++ )
    {
        bool couldIncrement = mipGen.incrementLevel();

        if ( couldIncrement == false )
        {
            return false;
        }
    }

    layerWidth = mipGen.getLevelWidth();
    layerHeight = mipGen.getLevelHeight();

    return true;
}

template <typename mipDataType, typename mipListType, typename mipManagerType>
inline bool virtualAddMipmapLayer(
    Interface *engineInterface, mipManagerType& mipManager,
    mipListType& mipmaps, const rawMipmapLayer& layerIn,
    texNativeTypeProvider::acquireFeedback_t& feedbackOut
)
{
    uint32 newMipIndex = (uint32)mipmaps.GetCount();

    uint32 layerWidth, layerHeight;

    mipDataType newLayer;

    if ( newMipIndex > 0 )
    {
        uint32 baseLayerWidth, baseLayerHeight;

        mipManager.GetLayerDimensions( mipmaps[ 0 ], baseLayerWidth, baseLayerHeight );

        bool couldFetchDimensions =
            getMipmapLayerDimensions(
                newMipIndex,
                baseLayerWidth, baseLayerHeight,
                layerWidth, layerHeight
            );

        if ( !couldFetchDimensions )
            return false;

        // Make sure the mipmap layer dimensions match.
        if ( layerWidth != layerIn.mipData.layerWidth ||
             layerHeight != layerIn.mipData.layerHeight )
        {
            return false;
        }

        // Actually, no. Mipmaps only ~inherit properties from the base layer dimensions.
        // For instance, if the base layer is power-of-two, the mipmaps are also power-of-two.
        // Does not count for every property tho, but it does not matter.
#if 0
        // Verify some more advanced rules.
        {
            nativeTextureSizeRules sizeRules;
            mipManager.GetSizeRules( sizeRules );

            if ( !sizeRules.IsMipmapSizeValid( layerWidth, layerHeight ) )
            {
                return false;
            }
        }
#endif

        bool hasDirectlyAcquired;
        //bool hasAcquiredPalette = false;    // TODO?

        try
        {
            mipManager.Internalize(
                engineInterface,
                newLayer,
                layerIn.mipData.width, layerIn.mipData.height, layerIn.mipData.layerWidth, layerIn.mipData.layerHeight, layerIn.mipData.texels, layerIn.mipData.dataSize,
                layerIn.rasterFormat, layerIn.colorOrder, layerIn.depth,
                layerIn.rowAlignment,
                layerIn.paletteType, layerIn.paletteData, layerIn.paletteSize,
                layerIn.compressionType, layerIn.hasAlpha,
                hasDirectlyAcquired
            );
        }
        catch( RwException& )
        {
            // Alright, we failed.
            return false;
        }

        feedbackOut.hasDirectlyAcquired = hasDirectlyAcquired;
    }
    else
    {
        // TODO: add support for adding mipmaps if the texture is empty.
        // we potentially have to convert the texels to a compatible format first.
        return false;
    }

    // Add this layer.
    mipmaps.AddToBack( std::move( newLayer ) );

    return true;
}

INSTANCE_METHCHECK( Cleanup );

template <typename mipDataType, typename mipListType>
inline void virtualClearMipmaps( Interface *engineInterface, mipListType& mipmaps )
{
    size_t mipmapCount = mipmaps.GetCount();

    if ( mipmapCount > 1 )
    {
        for ( size_t n = 1; n < mipmapCount; n++ )
        {
            mipDataType& mipLayer = mipmaps[ n ];

            if constexpr ( PERFORM_METHCHECK( mipDataType, Cleanup, void (Interface*) ) )
            {
                mipLayer.Cleanup( engineInterface );
            }

            if ( void *texels = mipLayer.texels )
            {
                engineInterface->PixelFree( texels );

                mipLayer.texels = nullptr;
            }
        }

        mipmaps.Resize( 1 );
    }
}

}
