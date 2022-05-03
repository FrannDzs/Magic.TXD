/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.debugutil.cpp
*  PURPOSE:     Texture debugging helpers for the framework user.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "pixelformat.hxx"

#include "txdread.raster.hxx"

namespace rw
{

// Draws all mipmap layers onto a mipmap.
bool DebugDrawMipmaps( Interface *engineInterface, Raster *debugRaster, Bitmap& bmpOut )
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( debugRaster ) );

    // Only proceed if we have native data.
    PlatformTexture *platformTex = debugRaster->platformData;

    if ( !platformTex )
        return false;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
        return false;

    // Fetch pixel data from the texture and convert it to uncompressed data.
    pixelDataTraversal pixelData;

    texProvider->GetPixelDataFromTexture( engineInterface, platformTex, pixelData );

    // Return a debug bitmap which contains all mipmap layers.
    uint32 requiredBitmapWidth = 0;
    uint32 requiredBitmapHeight = 0;

    size_t mipmapCount = pixelData.mipmaps.GetCount();

    bool gotStuff = false;

    if ( mipmapCount > 0 )
    {
        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            const pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

            uint32 mipLayerWidth = mipLayer.layerWidth;
            uint32 mipLayerHeight = mipLayer.layerHeight;

            // We allocate all mipmaps from top left to top right.
            requiredBitmapWidth += mipLayerWidth;

            if ( requiredBitmapHeight < mipLayerHeight )
            {
                requiredBitmapHeight = mipLayerHeight;
            }
        }

        if ( requiredBitmapWidth != 0 && requiredBitmapHeight != 0 )
        {
            // Allocate bitmap space.
            bmpOut.setSize( requiredBitmapWidth, requiredBitmapHeight );

            // Cursor for the drawing operation.
            uint32 cursor_x = 0;
            uint32 cursor_y = 0;

            // Establish whether we have to convert the mipmaps.
            eRasterFormat srcRasterFormat = pixelData.rasterFormat;
            uint32 srcDepth = pixelData.depth;
            uint32 srcRowAlignment = pixelData.rowAlignment;
            eColorOrdering srcColorOrder = pixelData.colorOrder;
            ePaletteType srcPaletteType = pixelData.paletteType;
            void *srcPaletteData = pixelData.paletteData;
            uint32 srcPaletteSize = pixelData.paletteSize;
            eCompressionType srcCompressionType = pixelData.compressionType;

            eRasterFormat reqRasterFormat = srcRasterFormat;
            uint32 reqDepth = srcDepth;
            uint32 reqRowAlignment = srcRowAlignment;
            eColorOrdering reqColorOrder = srcColorOrder;
            eCompressionType reqCompressionType = RWCOMPRESS_NONE;

            bool requiresConversion = false;

            if ( srcCompressionType != reqCompressionType )
            {
                requiresConversion = true;

                if ( srcCompressionType == RWCOMPRESS_DXT1 && pixelData.hasAlpha == false )
                {
                    reqRasterFormat = RASTER_888;
                    reqDepth = 32;
                }
                else
                {
                    reqRasterFormat = RASTER_8888;
                    reqDepth = 32;
                }
                
                reqColorOrder = COLOR_BGRA;
                reqRowAlignment = 4;    // good measure.
            }

            // Draw them.
            for ( uint32 n = 0; n < mipmapCount; n++ )
            {
                const pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

                uint32 mipWidth = mipLayer.width;
                uint32 mipHeight = mipLayer.height;

                uint32 layerWidth = mipLayer.layerWidth;
                uint32 layerHeight = mipLayer.layerHeight;

                void *srcTexels = mipLayer.texels;
                uint32 srcDataSize = mipLayer.dataSize;

                bool isNewlyAllocated = false;

                if ( requiresConversion )
                {
                    bool hasConverted =
                        ConvertMipmapLayerNative(
                            engineInterface,
                            mipWidth, mipHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
                            srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize, srcCompressionType,
                            reqRasterFormat, reqDepth, reqRowAlignment, reqColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize, reqCompressionType,
                            false,
                            mipWidth, mipHeight,
                            srcTexels, srcDataSize
                        );

                    assert( hasConverted == true );

                    isNewlyAllocated = true;
                }

                // Fetch colors from this mipmap layer.
                struct mipmapColorSourcePipeline : public Bitmap::sourceColorPipeline
                {
                    const void *texelSource;
                    uint32 mipWidth, mipHeight; 
                    rasterRowSize rowSize;

                    colorModelDispatcher fetchDispatch;

                    inline mipmapColorSourcePipeline(
                        uint32 mipWidth, uint32 mipHeight, uint32 depth, uint32 rowAlignment,
                        const void *texelSource,
                        eRasterFormat rasterFormat, eColorOrdering colorOrder,
                        const void *paletteData, uint32 paletteSize, ePaletteType paletteType
                    ) :
                    fetchDispatch( rasterFormat, colorOrder, depth, paletteData, paletteSize, paletteType )
                    {
                        this->texelSource = texelSource;
                        this->mipWidth = mipWidth;
                        this->mipHeight = mipHeight;

                        this->rowSize = getRasterDataRowSize( mipWidth, depth, rowAlignment );
                    }

                    uint32 getWidth( void ) const
                    {
                        return this->mipWidth;
                    }

                    uint32 getHeight( void ) const
                    {
                        return this->mipHeight;
                    }

                    void fetchcolor( uint32 x, uint32 y, double& red, double& green, double& blue, double& alpha )
                    {
                        uint8 r, g, b, a;

                        constRasterRow srcRow = getConstTexelDataRow( this->texelSource, this->rowSize, y );

                        bool hasColor = fetchDispatch.getRGBA( srcRow, x, r, g, b, a );

                        if ( !hasColor )
                        {
                            r = 0;
                            g = 0;
                            b = 0;
                            a = 0;
                        }

                        red = unpackcolor( r );
                        green = unpackcolor( g );
                        blue = unpackcolor( b );
                        alpha = unpackcolor( a );
                    }
                };

                mipmapColorSourcePipeline colorPipe(
                    mipWidth, mipHeight, reqDepth, reqRowAlignment,
                    srcTexels,
                    reqRasterFormat, reqColorOrder,
                    srcPaletteData, srcPaletteSize, srcPaletteType
                );

                try
                {
                    // Draw it at its position.
                    bmpOut.draw(
                        colorPipe, cursor_x, cursor_y,
                        mipWidth, mipHeight,
                        Bitmap::SHADE_ZERO, Bitmap::SHADE_ONE, Bitmap::BLEND_ADDITIVE
                    );
                }
                catch( ... )
                {
                    if ( isNewlyAllocated )
                    {
                        engineInterface->PixelFree( srcTexels );
                    }

                    throw;
                }

                // Delete if necessary.
                if ( isNewlyAllocated )
                {
                    engineInterface->PixelFree( srcTexels );
                }

                // Increase cursor.
                cursor_x += mipWidth;
            }

            gotStuff = true;
        }
    }

    pixelData.FreePixels( engineInterface );

    return gotStuff;
}

}