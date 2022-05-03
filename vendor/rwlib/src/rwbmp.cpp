/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwbmp.cpp
*  PURPOSE:     Simple bitmap software rendering library for image processing
*               from RenderWare textures.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "pixelformat.hxx"

#include "txdread.size.hxx"

namespace rw
{

Bitmap::Bitmap( Interface *engineInterface )
{
    this->engineInterface = engineInterface;

    this->width = 0;
    this->height = 0;
    this->depth = 32;
    this->rowAlignment = 4; // good measure.
    this->rasterFormat = RASTER_8888;
    this->texels = nullptr;
    this->dataSize = 0;

    this->colorOrder = COLOR_RGBA;

    this->bgRed = 0;
    this->bgGreen = 0;
    this->bgBlue = 0;
    this->bgAlpha = 1.0;
}

Bitmap::Bitmap( Interface *engineInterface, uint32 depth, eRasterFormat theFormat, eColorOrdering colorOrder )
{
    this->engineInterface = engineInterface;

    this->width = 0;
    this->height = 0;
    this->depth = depth;
    this->rowAlignment = 4;
    this->rasterFormat = theFormat;
    this->texels = nullptr;
    this->dataSize = 0;

    this->colorOrder = colorOrder;

    this->bgRed = 0;
    this->bgGreen = 0;
    this->bgBlue = 0;
    this->bgAlpha = 1.0;
}

void Bitmap::assignFrom( const Bitmap& right )
{
    Interface *engineInterface = right.engineInterface;

    this->engineInterface = engineInterface;

    this->width = right.width;
    this->height = right.height;
    this->depth = right.depth;
    this->rowAlignment = right.rowAlignment;
    this->rowSize = right.rowSize;
    this->rasterFormat = right.rasterFormat;

    // Copy texels.
    void *origTexels = right.texels;
    void *newTexels = nullptr;

    if ( origTexels )
    {
        newTexels = engineInterface->PixelAllocate( right.dataSize );

        memcpy( newTexels, origTexels, right.dataSize );
    }

    this->texels = newTexels;
    this->dataSize = right.dataSize;

    this->colorOrder = right.colorOrder;

    this->bgRed = right.bgRed;
    this->bgGreen = right.bgGreen;
    this->bgBlue = right.bgBlue;
    this->bgAlpha = right.bgAlpha;
}

void Bitmap::moveFrom( Bitmap&& right )
{
    this->engineInterface = right.engineInterface;

    this->width = right.width;
    this->height = right.height;
    this->depth = right.depth;
    this->rowAlignment = right.rowAlignment;
    this->rowSize = right.rowSize;
    this->rasterFormat = right.rasterFormat;

    // Move over texels.
    this->texels = right.texels;
    this->dataSize = right.dataSize;

    this->colorOrder = right.colorOrder;

    this->bgRed = right.bgRed;
    this->bgGreen = right.bgGreen;
    this->bgBlue = right.bgBlue;
    this->bgAlpha = right.bgAlpha;

    // Default the moved from object.
    right.texels = nullptr;
}

void Bitmap::clearTexelData( void )
{
    // If we have texel data, deallocate it.
    if ( void *ourTexels = this->texels )
    {
        Interface *engineInterface = this->engineInterface;

        engineInterface->PixelFree( ourTexels );

        this->texels = nullptr;
    }
}

void Bitmap::setImageData( void *theTexels, eRasterFormat theFormat, eColorOrdering colorOrder, uint32 depth, uint32 rowAlignment, uint32 width, uint32 height, uint32 dataSize, bool assignData )
{
    this->width = width;
    this->height = height;
    this->depth = depth;
    this->rowAlignment = rowAlignment;
    this->rowSize = getRasterDataRowSize( width, depth, rowAlignment );
    this->rasterFormat = theFormat;

    Interface *engineInterface = this->engineInterface;

    // Deallocate texels if we already have some.
    if ( void *origTexels = this->texels )
    {
        engineInterface->PixelFree( origTexels );

        this->texels = nullptr;
    }

    if ( assignData == false )
    {
        // Copy the texels.
        if ( dataSize != 0 )
        {
            void *newTexels = engineInterface->PixelAllocate( dataSize );

            memcpy( newTexels, theTexels, dataSize );

            this->texels = newTexels;
        }
    }
    else
    {
        // Just give us the data.
        this->texels = theTexels;
    }
    this->dataSize = dataSize;

    this->colorOrder = colorOrder;
}

void* Bitmap::copyPixelData( void ) const
{
    void *newPixels = nullptr;
    void *ourPixels = this->texels;

    if ( ourPixels )
    {
        Interface *engineInterface = this->engineInterface;

        newPixels = engineInterface->PixelAllocate( this->dataSize );

        memcpy( newPixels, ourPixels, this->dataSize );
    }

    return newPixels;
}

void Bitmap::setSize( uint32 width, uint32 height )
{
    Interface *engineInterface = this->engineInterface;

    uint32 oldWidth = this->width;
    uint32 oldHeight = this->height;

    // Allocate a new texel array.
    uint32 dataSize = getRasterImageDataSize( width, height, this->depth, this->rowAlignment );

    void *oldTexels = this->texels;
    void *newTexels = nullptr;

    if ( dataSize != 0 )
    {
        newTexels = engineInterface->PixelAllocate( dataSize );

        eRasterFormat rasterFormat = this->rasterFormat;
        eColorOrdering colorOrder = this->colorOrder;
        uint32 rasterDepth = this->depth;
        uint32 rowAlignment = this->rowAlignment;

        const uint8 srcBgRed = packcolor( this->bgRed );
        const uint8 srcBgGreen = packcolor( this->bgGreen );
        const uint8 srcBgBlue = packcolor( this->bgBlue );
        const uint8 srcBgAlpha = packcolor( this->bgAlpha );

        colorModelDispatcher fetchDispatch( rasterFormat, colorOrder, rasterDepth, nullptr, 0, PALETTE_NONE );
        colorModelDispatcher putDispatch( rasterFormat, colorOrder, rasterDepth, nullptr, 0, PALETTE_NONE );

        // Do an image copy.
        rasterRowSize srcRowSize = this->rowSize;
        rasterRowSize newRowSize = getRasterDataRowSize( width, rasterDepth, rowAlignment );

        for ( uint32 y = 0; y < height; y++ )
        {
            rasterRow dstRow = getTexelDataRow( newTexels, newRowSize, y );
            constRasterRow srcRow;

            if ( y < oldHeight )
            {
                srcRow = getConstTexelDataRow( oldTexels, srcRowSize, y );
            }

            for ( uint32 x = 0; x < width; x++ )
            {
                uint8 srcRed = srcBgRed;
                uint8 srcGreen = srcBgGreen;
                uint8 srcBlue = srcBgBlue;
                uint8 srcAlpha = srcBgAlpha;

                // Try to get a source color.
                if ( srcRow && x < oldWidth )
                {
                    fetchDispatch.getRGBA( srcRow, x, srcRed, srcGreen, srcBlue, srcAlpha );
                }

                // Put it into the new storage.
                putDispatch.setRGBA( dstRow, x, srcRed, srcGreen, srcBlue, srcAlpha );
            }
        }
    }

    // Delete old data.
    if ( oldTexels )
    {
        engineInterface->PixelFree( oldTexels );
    }

    // Set new data.
    this->texels = newTexels;
    this->width = width;
    this->height = height;
    this->dataSize = dataSize;
    this->rowSize = getRasterDataRowSize( width, this->depth, this->rowAlignment );
}

void Bitmap::scale(
    Interface *intf,
    uint32 width, uint32 height,
    const char *downsamplingMode, const char *upscaleMode
)
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    // This is actually scaling the bitmap, not changing the draw plane size.
    uint32 layerWidth = this->width;
    uint32 layerHeight = this->height;
    uint32 depth = this->depth;
    uint32 rowAlignment = this->rowAlignment;
    eRasterFormat rasterFormat = this->rasterFormat;
    eColorOrdering colorOrder = this->colorOrder;

    // Anything to do at all?
    if ( layerWidth == width && layerHeight == height )
        return;

    void *srcTexels = this->texels;

    // We want to perform default filtering, most of the time.
    rasterResizeFilterInterface *downsamplingFilter, *upscaleFilter;
    resizeFilteringCaps downsamplingCaps, upscaleCaps;

    FetchResizeFilteringFilters(
        engineInterface,
        downsamplingMode, upscaleMode,
        downsamplingFilter, upscaleFilter,
        downsamplingCaps, upscaleCaps
    );

    // The resize filtering expects a cached destination pipe.
    mipmapLayerResizeColorPipeline dstColorPipe(
        rasterFormat, depth, rowAlignment, colorOrder,
        PALETTE_NONE, nullptr, 0
    );

    // We also have to determine the sampling types that should be used beforehand.
    eSamplingType horiSampling = determineSamplingType( layerWidth, width );
    eSamplingType vertSampling = determineSamplingType( layerHeight, height );

    // Since we always are a raw raster, we can easily perform the filtering.
    // We do not have to perform _any_ source and destionation transformations, unlike for rasters.
    // We also are not bound to raster size rules that prevent certain size targets.
    void *transMipData = nullptr;
    uint32 transMipSize = 0;

    PerformRawBitmapResizeFiltering(
        engineInterface, layerWidth, layerHeight, srcTexels,
        width, height,
        rasterFormat, depth, rowAlignment, colorOrder, PALETTE_NONE, nullptr, 0,
        depth,
        dstColorPipe,
        horiSampling, vertSampling,
        upscaleFilter, downsamplingFilter,
        upscaleCaps, downsamplingCaps,
        transMipData, transMipSize
    );

    // Replace the pixels.
    if ( srcTexels )
    {
        engineInterface->PixelFree( srcTexels );
    }

    this->width = width;
    this->height = height;
    this->texels = transMipData;
    this->dataSize = transMipSize;

    // Update the row size.
    this->rowSize = getRasterDataRowSize( width, depth, rowAlignment );
}

AINLINE void fetchpackedcolor(
    const void *texels, uint32 x, uint32 y, eRasterFormat theFormat, eColorOrdering colorOrder, uint32 itemDepth, const rasterRowSize& rowSize, double& redOut, double& greenOut, double& blueOut, double& alphaOut
)
{
    colorModelDispatcher fetchDispatch( theFormat, colorOrder, itemDepth, nullptr, 0, PALETTE_NONE );

    constRasterRow srcRow = getConstTexelDataRow( texels, rowSize, y );

    fetchDispatch.getRGBA(
        srcRow, x,
        redOut, greenOut, blueOut, alphaOut
    );
}

AINLINE void getblendfactor(
    double srcRed, double srcGreen, double srcBlue, double srcAlpha,
    double dstRed, double dstGreen, double dstBlue, double dstAlpha,
    Bitmap::eShadeMode shadeMode,
    double& blendRed, double& blendGreen, double& blendBlue, double& blendAlpha
)
{
    if ( shadeMode == Bitmap::SHADE_SRCALPHA )
    {
        blendRed = srcAlpha;
        blendGreen = srcAlpha;
        blendBlue = srcAlpha;
        blendAlpha = srcAlpha;
    }
    else if ( shadeMode == Bitmap::SHADE_INVSRCALPHA )
    {
        double invAlpha = ( 1.0 - srcAlpha );

        blendRed = invAlpha;
        blendGreen = invAlpha;
        blendBlue = invAlpha;
        blendAlpha = invAlpha;
    }
    else if ( shadeMode == Bitmap::SHADE_ONE )
    {
        blendRed = 1;
        blendGreen = 1;
        blendBlue = 1;
        blendAlpha = 1;
    }
    else if ( shadeMode == Bitmap::SHADE_ZERO )
    {
        blendRed = 0;
        blendGreen = 0;
        blendBlue = 0;
        blendAlpha = 0;
    }
    else
    {
        blendRed = 1;
        blendGreen = 1;
        blendBlue = 1;
        blendAlpha = 1;
    }
}

eColorModel Bitmap::getColorModel( void ) const
{
    return getColorModelFromRasterFormat( this->rasterFormat );
}

bool Bitmap::browsecolor(uint32 x, uint32 y, uint8& redOut, uint8& greenOut, uint8& blueOut, uint8& alphaOut) const
{
    bool hasColor = false;

    if ( x < this->width && y < this->height )
    {
        constRasterRow srcRow = getConstTexelDataRow( this->texels, this->rowSize, y );

        colorModelDispatcher fetchDispatch( this->rasterFormat, this->colorOrder, this->depth, nullptr, 0, PALETTE_NONE );

        hasColor = fetchDispatch.getRGBA(
            srcRow, x,
            redOut, greenOut, blueOut, alphaOut
        );
    }

    return hasColor;
}

bool Bitmap::browselum(uint32 x, uint32 y, uint8& lum, uint8& a) const
{
    bool hasColor = false;

    if ( x < this->width && y < this->height )
    {
        constRasterRow srcRow = getConstTexelDataRow( this->texels, this->rowSize, y );

        colorModelDispatcher fetchDispatch( this->rasterFormat, this->colorOrder, this->depth, nullptr, 0, PALETTE_NONE );

        hasColor = fetchDispatch.getLuminance(
            srcRow, x,
            lum, a
        );
    }

    return hasColor;
}

bool Bitmap::browsecolorex(uint32 x, uint32 y, rwAbstractColorItem& colorItem) const
{
    bool hasColor = false;

    if ( x < this->width && y < this->height )
    {
        constRasterRow srcRow = getConstTexelDataRow( this->texels, this->rowSize, y );

        colorModelDispatcher fetchDispatch( this->rasterFormat, this->colorOrder, this->depth, nullptr, 0, PALETTE_NONE );

        abstractColorItem sdkItem;

        fetchDispatch.getColor(
            srcRow, x,
            sdkItem
        );

        // Push the color data as framework abstract color item.
        colorItem.model = sdkItem.model;
        destscalecolorn( sdkItem.rgbaColor.r, colorItem.rgbaColor.r );
        destscalecolorn( sdkItem.rgbaColor.g, colorItem.rgbaColor.g );
        destscalecolorn( sdkItem.rgbaColor.b, colorItem.rgbaColor.b );
        destscalecolorn( sdkItem.rgbaColor.a, colorItem.rgbaColor.a );
        destscalecolorn( sdkItem.luminance.lum, colorItem.luminance.lum );
        destscalecolorn( sdkItem.luminance.alpha, colorItem.luminance.alpha );

        hasColor = true;
    }

    return hasColor;
}

void Bitmap::draw(
    sourceColorPipeline& colorSource, uint32 offX, uint32 offY, uint32 drawWidth, uint32 drawHeight,
    eShadeMode srcChannel, eShadeMode dstChannel, eBlendMode blendMode
)
{
    // Put parameters on stack.
    uint32 ourWidth = this->width;
    uint32 ourHeight = this->height;
    uint32 ourDepth = this->depth;
    rasterRowSize ourRowSize = this->rowSize;
    void *ourTexels = this->texels;
    eRasterFormat ourFormat = this->rasterFormat;
    eColorOrdering ourOrder = this->colorOrder;

    uint32 theirWidth = colorSource.getWidth();
    uint32 theirHeight = colorSource.getHeight();

    // Determine the output codec for pixel set.
    colorModelDispatcher putDispatch( ourFormat, ourOrder, ourDepth, nullptr, 0, PALETTE_NONE );

    // Calculate drawing parameters.
    double floatOffX = (double)offX;
    double floatOffY = (double)offY;
    double floatDrawWidth = (double)drawWidth;
    double floatDrawHeight = (double)drawHeight;
    double srcBitmapWidthStride = ( theirWidth / floatDrawWidth );
    double srcBitmapHeightStride = ( theirHeight / floatDrawHeight );

    // Perform the drawing.
    for ( double y = 0; y < floatDrawHeight; y++ )
    {
        for ( double x = 0; x < floatDrawWidth; x++ )
        {
            // Get the coordinates to draw on into this bitmap.
            uint32 sourceX = (uint32)( x + floatOffX );
            uint32 sourceY = (uint32)( y + floatOffY );

            // Get the coordinates from the bitmap that is being drawn from.
            uint32 targetX = (uint32)( x * srcBitmapWidthStride );
            uint32 targetY = (uint32)( y * srcBitmapHeightStride );

            // Check that all coordinates are inside of their dimensions.
            if ( targetX < theirWidth && targetY < theirHeight &&
                 sourceX < ourWidth && sourceY < ourHeight )
            {
                // Fetch the colors from both bitmaps.
                double sourceRed, sourceGreen, sourceBlue, sourceAlpha;
                double dstRed, dstGreen, dstBlue, dstAlpha;

                // Fetch from "source".
                {
                    fetchpackedcolor(
                        ourTexels, sourceX, sourceY, ourFormat, ourOrder, ourDepth, ourRowSize,
                        sourceRed, sourceGreen, sourceBlue, sourceAlpha
                    );
                }

                // Fetch from "destination".
                {
                    colorSource.fetchcolor(targetX, targetY, dstRed, dstGreen, dstBlue, dstAlpha);
                }

                // Get the blend factors.
                double srcBlendRed, srcBlendGreen, srcBlendBlue, srcBlendAlpha;
                double dstBlendRed, dstBlendGreen, dstBlendBlue, dstBlendAlpha;

                getblendfactor(
                    sourceRed, sourceGreen, sourceBlue, sourceAlpha,
                    dstRed, dstGreen, dstBlue, dstAlpha, srcChannel,
                    srcBlendRed, srcBlendGreen, srcBlendBlue, srcBlendAlpha
                );
                getblendfactor(
                    sourceRed, sourceGreen, sourceBlue, sourceAlpha,
                    dstRed, dstGreen, dstBlue, dstAlpha, dstChannel,
                    dstBlendRed, dstBlendGreen, dstBlendBlue, dstBlendAlpha
                );

                // Get the blended colors.
                double srcBlendedRed = ( sourceRed * srcBlendRed );
                double srcBlendedGreen = ( sourceGreen * srcBlendGreen );
                double srcBlendedBlue = ( sourceBlue * srcBlendBlue );
                double srcBlendedAlpha = ( sourceAlpha * srcBlendAlpha );

                double dstBlendedRed = ( dstRed * dstBlendRed );
                double dstBlendedGreen = ( dstGreen * dstBlendGreen );
                double dstBlendedBlue = ( dstBlue * dstBlendBlue );
                double dstBlendedAlpha = ( dstAlpha * dstBlendAlpha );

                // Perform the color op.
                double resRed, resGreen, resBlue, resAlpha;
                {
                    if ( blendMode == BLEND_MODULATE )
                    {
                        resRed = srcBlendedRed * dstBlendedRed;
                        resGreen = srcBlendedGreen * dstBlendedGreen;
                        resBlue = srcBlendedBlue * dstBlendedBlue;
                        resAlpha = srcBlendedAlpha * dstBlendedAlpha;
                    }
                    else if ( blendMode == BLEND_ADDITIVE )
                    {
                        resRed = srcBlendedRed + dstBlendedRed;
                        resGreen = srcBlendedGreen + dstBlendedGreen;
                        resBlue = srcBlendedBlue + dstBlendedBlue;
                        resAlpha = srcBlendedAlpha + dstBlendedAlpha;
                    }
                    else
                    {
                        resRed = sourceRed;
                        resGreen = sourceGreen;
                        resBlue = sourceBlue;
                        resAlpha = sourceAlpha;
                    }
                }

                // Clamp the colors.
                resRed = std::max( 0.0, std::min( 1.0, resRed ) );
                resGreen = std::max( 0.0, std::min( 1.0, resGreen ) );
                resBlue = std::max( 0.0, std::min( 1.0, resBlue ) );
                resAlpha = std::max( 0.0, std::min( 1.0, resAlpha ) );

                // Write back the new color.
                {
                    rasterRow dstRow = getTexelDataRow( ourTexels, ourRowSize, sourceY );

                    putDispatch.setRGBA(
                        dstRow, sourceX,
                        packcolor( resRed ), packcolor( resGreen ), packcolor( resBlue ), packcolor( resAlpha )
                    );
                }
            }
        }
    }

    // We are finished!
}

void Bitmap::drawBitmap(
    const Bitmap& theBitmap, uint32 offX, uint32 offY, uint32 drawWidth, uint32 drawHeight,
    eShadeMode srcChannel, eShadeMode dstChannel, eBlendMode blendMode
)
{
    struct bitmapColorSource : public sourceColorPipeline
    {
        const Bitmap& bmpSource;

        uint32 theWidth;
        uint32 theHeight;
        uint32 theDepth;
        rasterRowSize rowSize;
        eRasterFormat theFormat;
        eColorOrdering theOrder;
        void *theTexels;

        inline bitmapColorSource( const Bitmap& bmp ) : bmpSource( bmp )
        {
            bmp.getSize(this->theWidth, this->theHeight);

            this->theDepth = bmp.getDepth();
            this->theFormat = bmp.getFormat();
            this->theOrder = bmp.getColorOrder();
            this->theTexels = bmp.texels;

            this->rowSize = getRasterDataRowSize( this->theWidth, this->theDepth, bmp.getRowAlignment() );
        }

        uint32 getWidth( void ) const
        {
            return this->theWidth;
        }

        uint32 getHeight( void ) const
        {
            return this->theHeight;
        }

        void fetchcolor( uint32 x, uint32 y, double& red, double& green, double& blue, double& alpha )
        {
            fetchpackedcolor(
                this->theTexels, x, y, this->theFormat, this->theOrder, this->theDepth, this->rowSize, red, green, blue, alpha
            );
        }
    };

    bitmapColorSource bmpSource( theBitmap );

    // Draw using general-purpose pipeline.
    this->draw( bmpSource, offX, offY, drawWidth, drawHeight, srcChannel, dstChannel, blendMode );
}

};
