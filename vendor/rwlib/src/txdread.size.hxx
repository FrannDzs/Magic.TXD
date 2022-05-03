/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.size.hxx
*  PURPOSE:     Resizing environment structures and definitions
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_RESIZING_INTERNALS_
#define _RENDERWARE_RESIZING_INTERNALS_

#include <cstring>
#include <assert.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>

#include "pluginutil.hxx"

#include "txdread.common.hxx"

#include "pixelformat.hxx"

#include "pixelutil.hxx"

#include "txdread.nativetex.hxx"

namespace rw
{

struct resizeFilteringCaps
{
    bool supportsMagnification;
    bool supportsMinification;

    bool magnify2D;
    bool minify2D;
};

struct resizeColorPipeline abstract
{
    virtual eColorModel getColorModel( void ) const = 0;

    virtual bool fetchcolor( uint32 x, uint32 y, abstractColorItem& colorOut ) const = 0;
    virtual bool putcolor( uint32 x, uint32 y, const abstractColorItem& colorIn ) = 0;
};

struct rasterResizeFilterInterface abstract
{
    virtual void GetSupportedFiltering( resizeFilteringCaps& filterOut ) const = 0;

    virtual void MagnifyFiltering(
        const resizeColorPipeline& srcBmp, uint32 magX, uint32 magY, uint32 magScaleX, uint32 magScaleY,
        resizeColorPipeline& dstBmp, uint32 dstX, uint32 dstY
    ) const = 0;
    virtual void MinifyFiltering(
        const resizeColorPipeline& srcBmp, uint32 minX, uint32 minY, uint32 minScaleX, uint32 minScaleY,
        abstractColorItem& reducedColor
    ) const = 0;
};

// Resize filtering plugin, used to store filtering plugions.
struct resizeFilteringEnv
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        LIST_CLEAR( this->filters.root );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Unregister all filters.
        LIST_FOREACH_BEGIN( filterPluginEntry, this->filters.root, node )

            item->~filterPluginEntry();

        LIST_FOREACH_END

        LIST_CLEAR( this->filters.root );
    }

    inline resizeFilteringEnv& operator = ( const resizeFilteringEnv& right ) = delete;

    // Registration entry for a filtering plugin.
    struct filterPluginEntry
    {
        rwStaticString <char> filterName;

        rasterResizeFilterInterface *intf;

        RwListEntry <filterPluginEntry> node;
    };

    RwList <filterPluginEntry> filters;
    
    inline filterPluginEntry* FindPluginByName( const char *name ) const
    {
        LIST_FOREACH_BEGIN( filterPluginEntry, this->filters.root, node )

            if ( item->filterName == name )
            {
                return item;
            }

        LIST_FOREACH_END

        return nullptr;
    }

    inline filterPluginEntry* FindPluginByInterface( rasterResizeFilterInterface *intf ) const
    {
        LIST_FOREACH_BEGIN( filterPluginEntry, this->filters.root, node )

            if ( item->intf == intf )
            {
                return item;
            }

        LIST_FOREACH_END

        return nullptr;
    }
};

typedef PluginDependantStructRegister <resizeFilteringEnv, RwInterfaceFactory_t> resizeFilteringEnvRegister_t;

extern optional_struct_space <resizeFilteringEnvRegister_t> resizeFilteringEnvRegister;

// Filtering API interface. Used to register native filtering plugins.
bool RegisterResizeFiltering( EngineInterface *engineInterface, const char *filterName, rasterResizeFilterInterface *intf );
bool UnregisterResizeFiltering( EngineInterface *engineInterface, rasterResizeFilterInterface *intf );

enum class eSamplingType
{
    SAME,
    UPSCALING,
    DOWNSAMPLING
};

inline eSamplingType determineSamplingType( uint32 origDimm, uint32 newDimm )
{
    if ( origDimm == newDimm )
    {
        return eSamplingType::SAME;
    }
    else if ( origDimm < newDimm )
    {
        return eSamplingType::UPSCALING;
    }
    else if ( origDimm > newDimm )
    {
        return eSamplingType::DOWNSAMPLING;
    }

    return eSamplingType::SAME;
}

struct mipmapLayerResizeColorPipeline : public resizeColorPipeline
{
private:
    uint32 depth;
    uint32 rowAlignment;
    rasterRowSize rowSize;

    uint32 layerWidth, layerHeight;
    void *texelSource;

    colorModelDispatcher dispatch;

public:
    uint32 coord_mult_x, coord_mult_y;

    inline mipmapLayerResizeColorPipeline(
        eRasterFormat rasterFormat, uint32 depth, uint32 rowAlignment, eColorOrdering colorOrder,
        ePaletteType paletteType, const void *paletteData, uint32 paletteSize
    ) : dispatch( rasterFormat, colorOrder, depth, paletteData, paletteSize, paletteType )
    {
        this->depth = depth;
        this->rowAlignment = rowAlignment;

        this->layerWidth = 0;
        this->layerHeight = 0;
        this->texelSource = nullptr;

        this->coord_mult_x = 1;
        this->coord_mult_y = 1;
    }

    inline mipmapLayerResizeColorPipeline( const mipmapLayerResizeColorPipeline& right )
        : dispatch( right.dispatch )
    {
        this->depth = right.depth;
        this->rowAlignment = right.rowAlignment;
        this->rowSize = right.rowSize;
        this->layerWidth = right.layerWidth;
        this->layerHeight = right.layerHeight;
       
        this->layerWidth = 0;
        this->layerHeight = 0;
        this->texelSource = nullptr;
    }

    inline void SetMipmapData( void *texelSource, uint32 layerWidth, uint32 layerHeight )
    {
        this->rowSize = getRasterDataRowSize( layerWidth, depth, rowAlignment );

        this->layerWidth = layerWidth;
        this->layerHeight = layerHeight;
        this->texelSource = texelSource;
    }

    eColorModel getColorModel( void ) const override
    {
        return dispatch.getColorModel();
    }
    
    bool fetchcolor( uint32 x, uint32 y, abstractColorItem& colorOut ) const override
    {
        bool gotColor = false;

        x *= this->coord_mult_x;
        y *= this->coord_mult_y;

        uint32 layerWidth = this->layerWidth;
        uint32 layerHeight = this->layerHeight;

        if ( x < layerWidth && y < layerHeight )
        {
            rasterRow srcRow = getTexelDataRow( this->texelSource, this->rowSize, y );

            dispatch.getColor( srcRow, x, colorOut );

            gotColor = true;
        }

        return gotColor;
    }

    bool putcolor( uint32 x, uint32 y, const abstractColorItem& colorIn ) override
    {
        bool putColor = false;

        x *= this->coord_mult_x;
        y *= this->coord_mult_y;

        uint32 layerWidth = this->layerWidth;
        uint32 layerHeight = this->layerHeight;

        if ( x < layerWidth && y < layerHeight )
        {
            rasterRow dstRow = getTexelDataRow( this->texelSource, this->rowSize, y );

            dispatch.setColor( dstRow, x, colorIn );

            putColor = true;
        }

        return putColor;
    }
};

struct filterDimmProcess
{
    inline filterDimmProcess( double dimmAdvance, uint32 origDimmLimit )
    {
        this->dimmAdvance = dimmAdvance;

        this->dimmIter = 0;
        this->preciseDimmIter = 0;

        this->origDimmIter = 0;
        this->origDimmLimit = origDimmLimit;
    }

    inline bool IsEnd( void ) const
    {
        return ( origDimmIter == origDimmLimit );
    }

    static uint32 FilterIter( double iter )
    {
        return (uint32)round( iter );
    }

    inline void Increment( void )
    {
        this->preciseDimmIter += this->dimmAdvance;
        this->dimmIter = FilterIter( this->preciseDimmIter );

        this->origDimmIter++;
    }

    inline void Resolve( uint32& origDimmIter, uint32& filterStart, uint32& filterSize ) const
    {
        uint32 _filterStart = this->dimmIter;
        uint32 filterEnd = FilterIter( this->preciseDimmIter + this->dimmAdvance );

        filterStart = _filterStart;
        filterSize = filterEnd - _filterStart;

        assert( filterSize >= 1 );

        origDimmIter = this->origDimmIter;
    }

private:
    uint32 dimmIter;
    double preciseDimmIter;
    double dimmAdvance;

    uint32 origDimmIter;
    uint32 origDimmLimit;
};

template <typename filteringProcessor>
AINLINE void filteringDispatcher2D(
    uint32 surfProcWidth, uint32 surfProcHeight,
    double widthProcessRatio, double heightProcessRatio,
    filteringProcessor& processor
)
{
    filterDimmProcess heightProcess( heightProcessRatio, surfProcHeight );

    while ( heightProcess.IsEnd() == false )
    {
        uint32 dstY;

        uint32 heightFilterStart, heightFilterSize;
        heightProcess.Resolve( dstY, heightFilterStart, heightFilterSize );

        filterDimmProcess widthProcess( widthProcessRatio, surfProcWidth );

        while ( widthProcess.IsEnd() == false )
        {
            uint32 dstX;

            uint32 widthFilterStart, widthFilterSize;
            widthProcess.Resolve( dstX, widthFilterStart, widthFilterSize );

            // Do the filtering.
            processor.Process(
                widthFilterStart, heightFilterStart,
                widthFilterSize, heightFilterSize,
                dstX, dstY
            );

            widthProcess.Increment();
        }

        heightProcess.Increment();
    }
}

template <typename filteringProcessor>
AINLINE void filteringDispatcherWidth1D(
    uint32 surfProcWidth, uint32 surfProcHeight,
    double widthProcessRatio,
    filteringProcessor& processor
)
{
    for ( uint32 y = 0; y < surfProcHeight; y++ )
    {
        filterDimmProcess widthProcess( widthProcessRatio, surfProcWidth );

        while ( widthProcess.IsEnd() == false )
        {
            uint32 dstX;

            uint32 widthFilterStart, widthFilterSize;
            widthProcess.Resolve( dstX, widthFilterStart, widthFilterSize );

            // Do the filtering.
            processor.Process(
                widthFilterStart, y,
                widthFilterSize, 1,
                dstX, y
            );

            widthProcess.Increment();
        }
    }
}

template <typename filteringProcessor>
AINLINE void filteringDispatcherHeight1D(
    uint32 surfProcWidth, uint32 surfProcHeight,
    double heightProcessRatio,
    filteringProcessor& processor
)
{
    filterDimmProcess heightProcess( heightProcessRatio, surfProcHeight );

    while ( heightProcess.IsEnd() == false )
    {
        uint32 dstY;

        uint32 heightFilterStart, heightFilterSize;
        heightProcess.Resolve( dstY, heightFilterStart, heightFilterSize );

        for ( uint32 x = 0; x < surfProcWidth; x++ )
        {
            // Do the filtering.
            processor.Process(
                x, heightFilterStart,
                1, heightFilterSize,
                x, dstY
            );
        }

        heightProcess.Increment();
    }
}

struct minifyFiltering2D
{
    AINLINE minifyFiltering2D(
        const mipmapLayerResizeColorPipeline& srcColorPipe, mipmapLayerResizeColorPipeline& dstColorPipe,
        rasterResizeFilterInterface *downsampleFilter
    ) : srcColorPipe( srcColorPipe ), dstColorPipe( dstColorPipe )
    {
        this->downsampleFilter = downsampleFilter;
    }

    AINLINE void Process(
        uint32 widthFilterStart, uint32 heightFilterStart,
        uint32 widthFilterSize, uint32 heightFilterSize,
        uint32 dstX, uint32 dstY
    )
    {
        abstractColorItem resultColorItem;

        downsampleFilter->MinifyFiltering(
            srcColorPipe,
            widthFilterStart, heightFilterStart,
            widthFilterSize, heightFilterSize,
            resultColorItem
        );

        // Store the result color.
        dstColorPipe.putcolor( dstX, dstY, resultColorItem );
    }

private:
    const mipmapLayerResizeColorPipeline& srcColorPipe;
    mipmapLayerResizeColorPipeline& dstColorPipe;

    rasterResizeFilterInterface *downsampleFilter;
};

struct magnifyFiltering2D
{
    AINLINE magnifyFiltering2D(
        const mipmapLayerResizeColorPipeline& srcColorPipe, mipmapLayerResizeColorPipeline& dstColorPipe,
        rasterResizeFilterInterface *upscaleFilter
    ) : srcColorPipe( srcColorPipe ), dstColorPipe( dstColorPipe )
    {
        this->upscaleFilter = upscaleFilter;
    }

    AINLINE void Process(
        uint32 widthFilterStart, uint32 heightFilterStart,
        uint32 widthFilterSize, uint32 heightFilterSize,
        uint32 srcX, uint32 srcY
    )
    {
        upscaleFilter->MagnifyFiltering(
            srcColorPipe,
            widthFilterStart, heightFilterStart,
            widthFilterSize, heightFilterSize,
            dstColorPipe,
            srcX, srcY
        );
    }

private:
    const mipmapLayerResizeColorPipeline& srcColorPipe;
    mipmapLayerResizeColorPipeline& dstColorPipe;

    rasterResizeFilterInterface *upscaleFilter;
};

AINLINE void performFiltering1D(
    EngineInterface *engineInterface,
    mipmapLayerResizeColorPipeline& dstColorPipe,   // cached thing.
    void *transMipData,                             // buffer we expect the final result in
    uint32 sampleDepth,                             // format of the (raw raster) result buffer.
    uint32 targetLayerWidth, uint32 targetLayerHeight,
    uint32 rawOrigLayerWidth, uint32 rawOrigLayerHeight, void *rawOrigTexels,
    eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth, uint32 rowAlignment,
    ePaletteType paletteType, const void *paletteData, uint32 paletteSize,
    eSamplingType mipHoriSampling, eSamplingType mipVertSampling,
    rasterResizeFilterInterface *upscaleFilter, rasterResizeFilterInterface *downsamplingFilter
)
{
    // We need to do unoptimized filtering.
    // This is splitting up a potentially 2D filtering into two 1D operations.
    uint32 currentWidth = rawOrigLayerWidth;
    uint32 currentHeight = rawOrigLayerHeight;

    void *currentTexels = rawOrigTexels;

    bool currentTexelsHasAllocated = false;

    uint32 currentDepth = depth;
    ePaletteType currentPaletteType = paletteType;
    const void *currentPaletteData = paletteData;
    uint32 currentPaletteSize = paletteSize;

    try
    {
        if ( mipHoriSampling != eSamplingType::SAME )
        {
            // We need a new target buffer.
            uint32 redirTargetWidth = targetLayerWidth;
            uint32 redirTargetHeight = currentHeight;

            void *redirTargetTexels = nullptr;

            bool redirHasAllocated = false;

            if ( redirTargetWidth == targetLayerWidth && redirTargetHeight == targetLayerHeight )
            {
                redirTargetTexels = transMipData;
            }
            else
            {
                rasterRowSize redirTargetRowSize = getRasterDataRowSize( redirTargetWidth, sampleDepth, rowAlignment );

                uint32 redirTargetDataSize = getRasterDataSizeByRowSize( redirTargetRowSize, redirTargetHeight );

                redirTargetTexels = engineInterface->PixelAllocate( redirTargetDataSize );

                redirHasAllocated = true;
            }

            try
            {
                // Set up the appropriate targets and sources.
                dstColorPipe.SetMipmapData( redirTargetTexels, redirTargetWidth, redirTargetHeight );

                mipmapLayerResizeColorPipeline srcDynamicPipe(
                    rasterFormat, currentDepth, rowAlignment, colorOrder, 
                    currentPaletteType, currentPaletteData, currentPaletteSize
                );

                srcDynamicPipe.SetMipmapData(
                    currentTexels, currentWidth, currentHeight
                );

                if ( mipHoriSampling == eSamplingType::UPSCALING )
                {
                    magnifyFiltering2D filterProc(
                        srcDynamicPipe, dstColorPipe,
                        upscaleFilter
                    );

                    double widthProcessRatio = (double)redirTargetWidth / (double)currentWidth;
                                    
                    filteringDispatcherWidth1D(
                        currentWidth, currentHeight,
                        widthProcessRatio, filterProc
                    );
                }
                else if ( mipHoriSampling == eSamplingType::DOWNSAMPLING )
                {
                    minifyFiltering2D filterProc(
                        srcDynamicPipe, dstColorPipe,
                        downsamplingFilter
                    );

                    double widthProcessRatio = (double)currentWidth / (double)redirTargetWidth;

                    filteringDispatcherWidth1D(
                        redirTargetWidth, redirTargetHeight,
                        widthProcessRatio, filterProc
                    );
                }
            }
            catch( ... )
            {
                if ( redirHasAllocated )
                {
                    engineInterface->PixelFree( redirTargetTexels );
                }

                throw;
            }

            if ( currentTexelsHasAllocated )
            {
                engineInterface->PixelFree( currentTexels );
            }

            // We use those texels as current texels now.
            currentTexels = redirTargetTexels;

            currentWidth = redirTargetWidth;
            currentHeight = redirTargetHeight;

            currentTexelsHasAllocated = redirHasAllocated;

            // meh: after resizing we definitely know that we cannot have a palettized
            // currentTexels buffer. that is why we update properties here.
            currentPaletteType = PALETTE_NONE;
            currentPaletteData = nullptr;
            currentPaletteSize = 0;
            currentDepth = sampleDepth;
        }

        if ( mipVertSampling != eSamplingType::SAME )
        {
            // We need a new target buffer.
            uint32 redirTargetWidth = currentWidth;
            uint32 redirTargetHeight = targetLayerHeight;

            void *redirTargetTexels = nullptr;

            bool redirHasAllocated = false;

            if ( redirTargetWidth == targetLayerWidth && redirTargetHeight == targetLayerHeight )
            {
                redirTargetTexels = transMipData;
            }
            else
            {
                rasterRowSize redirTargetRowSize = getRasterDataRowSize( redirTargetWidth, sampleDepth, rowAlignment );

                uint32 redirTargetDataSize = getRasterDataSizeByRowSize( redirTargetRowSize, redirTargetHeight );

                redirTargetTexels = engineInterface->PixelAllocate( redirTargetDataSize );

                redirHasAllocated = true;
            }

            try
            {
                // Set up the appropriate targets and sources.
                dstColorPipe.SetMipmapData( redirTargetTexels, redirTargetWidth, redirTargetHeight );

                mipmapLayerResizeColorPipeline srcDynamicPipe(
                    rasterFormat, currentDepth, rowAlignment, colorOrder, 
                    currentPaletteType, currentPaletteData, currentPaletteSize
                );

                srcDynamicPipe.SetMipmapData(
                    currentTexels, currentWidth, currentHeight
                );

                if ( mipVertSampling == eSamplingType::UPSCALING )
                {
                    magnifyFiltering2D filterProc(
                        srcDynamicPipe, dstColorPipe,
                        upscaleFilter
                    );

                    double heightProcessRatio = (double)redirTargetHeight / (double)currentHeight;
                                    
                    filteringDispatcherHeight1D(
                        currentWidth, currentHeight,
                        heightProcessRatio, filterProc
                    );
                }
                else if ( mipVertSampling == eSamplingType::DOWNSAMPLING )
                {
                    minifyFiltering2D filterProc(
                        srcDynamicPipe, dstColorPipe,
                        downsamplingFilter
                    );

                    double heightProcessRatio = (double)currentHeight / (double)redirTargetHeight;

                    filteringDispatcherHeight1D(
                        redirTargetWidth, redirTargetHeight,
                        heightProcessRatio, filterProc
                    );
                }
            }
            catch( ... )
            {
                if ( redirHasAllocated )
                {
                    engineInterface->PixelFree( redirTargetTexels );
                }

                throw;
            }

            if ( currentTexelsHasAllocated )
            {
                engineInterface->PixelFree( currentTexels );
            }

            // We use those texels as current texels now.
            currentTexels = redirTargetTexels;

            currentWidth = redirTargetWidth;
            currentHeight = redirTargetHeight;

            currentTexelsHasAllocated = redirHasAllocated;

            // meh: after resizing we definitely know that we cannot have a palettized
            // currentTexels buffer. that is why we update properties here.
            currentPaletteType = PALETTE_NONE;
            currentPaletteData = nullptr;
            currentPaletteSize = 0;
            currentDepth = sampleDepth;
        }
    }
    catch( ... )
    {
        if ( currentTexelsHasAllocated )
        {
            engineInterface->PixelFree( currentTexels );
        }

        throw;
    }
}

AINLINE void performMinifyFiltering2D(
    const mipmapLayerResizeColorPipeline& srcColorPipe, mipmapLayerResizeColorPipeline& dstColorPipe,
    uint32 rawOrigLayerWidth, uint32 rawOrigLayerHeight,
    uint32 targetLayerWidth, uint32 targetLayerHeight,
    rasterResizeFilterInterface *downsampleFilter
)
{
    minifyFiltering2D filterProc(
        srcColorPipe, dstColorPipe,
        downsampleFilter
    );

    double widthProcessRatio = (double)rawOrigLayerWidth / (double)targetLayerWidth;
    double heightProcessRatio = (double)rawOrigLayerHeight / (double)targetLayerHeight;

    filteringDispatcher2D(
        targetLayerWidth, targetLayerHeight,
        widthProcessRatio,
        heightProcessRatio,
        filterProc
    );
}

AINLINE void performMagnifyFiltering2D(
    const mipmapLayerResizeColorPipeline& srcColorPipe, mipmapLayerResizeColorPipeline& dstColorPipe,
    uint32 rawOrigLayerWidth, uint32 rawOrigLayerHeight,
    uint32 targetLayerWidth, uint32 targetLayerHeight,
    rasterResizeFilterInterface *upscaleFilter
)
{
    magnifyFiltering2D filterProc(
        srcColorPipe, dstColorPipe,
        upscaleFilter
    );

    double widthProcessRatio = (double)targetLayerWidth / (double)rawOrigLayerWidth;
    double heightProcessRatio    = (double)targetLayerHeight / (double)rawOrigLayerHeight;

    filteringDispatcher2D(
        rawOrigLayerWidth, rawOrigLayerHeight,
        widthProcessRatio,
        heightProcessRatio,
        filterProc
    );
}

AINLINE void PerformRawBitmapResizeFiltering(
    EngineInterface *engineInterface,
    uint32 rawOrigLayerWidth, uint32 rawOrigLayerHeight, void *rawOrigTexels,
    uint32 targetLayerWidth, uint32 targetLayerHeight,
    eRasterFormat rasterFormat, uint32 itemDepth, uint32 rowAlignment, eColorOrdering colorOrder, ePaletteType paletteType, const void *paletteData, uint32 paletteSize,
    uint32 sampleDepth,
    mipmapLayerResizeColorPipeline& dstColorPipe,
    eSamplingType horiSampling, eSamplingType vertSampling,
    rasterResizeFilterInterface *upscaleFilter, rasterResizeFilterInterface *downsamplingFilter,
    const resizeFilteringCaps& upscaleCaps, const resizeFilteringCaps& downsamplingCaps,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    assert( horiSampling != eSamplingType::SAME || vertSampling != eSamplingType::SAME );

    // We call this temporary buffer that will be used for scaling the "transformation buffer".
    // It shall be encoded as raw raster.
    rasterRowSize transMipRowSize = getRasterDataRowSize( targetLayerWidth, sampleDepth, rowAlignment );

    uint32 transMipSize = getRasterDataSizeByRowSize( transMipRowSize, targetLayerHeight );

    void *transMipData = engineInterface->PixelAllocate( transMipSize );

    try
    {
        // Pretty much ready to do things.
        // We need to put texels into the buffer, so lets also create a destination pipe.
        dstColorPipe.SetMipmapData( transMipData, targetLayerWidth, targetLayerHeight );

        bool hasDoneOptimizedFiltering = false;

        if ( horiSampling == eSamplingType::DOWNSAMPLING && vertSampling == eSamplingType::DOWNSAMPLING )
        {
            // Check for support first.
            if ( downsamplingCaps.minify2D )
            {
                // Prepare the virtual surface pipeline.
                mipmapLayerResizeColorPipeline srcColorPipe(
                    rasterFormat, itemDepth, rowAlignment, colorOrder,
                    paletteType, paletteData, paletteSize
                );

                srcColorPipe.SetMipmapData( rawOrigTexels, rawOrigLayerWidth, rawOrigLayerHeight );

                performMinifyFiltering2D(
                    srcColorPipe, dstColorPipe,
                    rawOrigLayerWidth, rawOrigLayerHeight,
                    targetLayerWidth, targetLayerHeight,
                    downsamplingFilter
                );

                hasDoneOptimizedFiltering = true;
            }
        }
        else if ( horiSampling == eSamplingType::UPSCALING && vertSampling == eSamplingType::UPSCALING )
        {
            if ( upscaleCaps.magnify2D )
            {
                // Prepare the virtual surface pipeline.
                mipmapLayerResizeColorPipeline srcColorPipe(
                    rasterFormat, itemDepth, rowAlignment, colorOrder,
                    paletteType, paletteData, paletteSize
                );

                srcColorPipe.SetMipmapData( rawOrigTexels, rawOrigLayerWidth, rawOrigLayerHeight );

                performMagnifyFiltering2D(
                    srcColorPipe, dstColorPipe,
                    rawOrigLayerWidth, rawOrigLayerHeight,
                    targetLayerWidth, targetLayerHeight,
                    upscaleFilter
                );

                hasDoneOptimizedFiltering = true;
            }
        }
                    
        if ( !hasDoneOptimizedFiltering )
        {
            // Pretty complicated.
            // Please report any bugs if you find them.
            performFiltering1D(
                engineInterface, dstColorPipe,
                transMipData, sampleDepth,
                targetLayerWidth, targetLayerHeight,
                rawOrigLayerWidth, rawOrigLayerHeight, rawOrigTexels,
                rasterFormat, colorOrder, itemDepth, rowAlignment,
                paletteType, paletteData, paletteSize,
                horiSampling, vertSampling,
                upscaleFilter, downsamplingFilter
            );
        }
    }
    catch( ... )
    {
        // Well, something went horribly wrong.
        // Let us make sure by freeing the buffer.
        engineInterface->PixelFree( transMipData );

        throw;
    }

    // Return the texel data.
    dstTexelsOut = transMipData;
    dstDataSizeOut = transMipSize;
}

AINLINE void MapToCompatibleResizeRasterFormat(
    eRasterFormat rasterFormat, uint32 depth, eColorOrdering colorOrder, eCompressionType compressionType,
    eRasterFormat& compRasterFormat, uint32& compDepth, eColorOrdering& compColorOrder
)
{
    if ( compressionType != RWCOMPRESS_NONE )
    {
        compRasterFormat = RASTER_8888;
        compColorOrder = COLOR_BGRA;
        compDepth = Bitmap::getRasterFormatDepth( compRasterFormat );
    }
    else
    {
        compRasterFormat = rasterFormat;
        compColorOrder = colorOrder;
        compDepth = depth;
    }
}

inline void FetchResizeFilteringFilters(
    EngineInterface *engineInterface, const char *downsampleMode, const char *upscaleMode,
    rasterResizeFilterInterface*& downsamplingFilterOut, rasterResizeFilterInterface*& upscaleFilterOut,
    resizeFilteringCaps& downsamplingCapsOut, resizeFilteringCaps& upscaleCapsOut
)
{
    // Get the filter plugin environment.
    resizeFilteringEnv *filterEnv = resizeFilteringEnvRegister.get().GetPluginStruct( engineInterface );

    if ( !filterEnv )
    {
        throw NotInitializedException( eSubsystemType::RESIZING, nullptr );
    }

    // If the user has not decided for a filtering plugin yet, choose the default.
    if ( !downsampleMode )
    {
        downsampleMode = "blur";
    }

    if ( !upscaleMode )
    {
        upscaleMode = "linear";
    }

    rasterResizeFilterInterface *downsamplingFilter = nullptr;
    rasterResizeFilterInterface *upscaleFilter = nullptr;
    {
        // Determine the sampling plugin that the runtime wants us to use.
        resizeFilteringEnv::filterPluginEntry *samplingPluginCont = filterEnv->FindPluginByName( downsampleMode );

        if ( !samplingPluginCont )
        {
            throw InvalidParameterException( eSubsystemType::RESIZING, L"RESIZING_DOWNSAMPLEFLTRNAME_FRIENDLYNAME", nullptr );
        }

        downsamplingFilter = samplingPluginCont->intf;

        resizeFilteringEnv::filterPluginEntry *upscalePluginCont = filterEnv->FindPluginByName( upscaleMode );

        if ( !upscalePluginCont )
        {
            throw InvalidParameterException( eSubsystemType::RESIZING, L"RESIZING_UPSCALEFLTRNAME_FRIENDLYNAME", nullptr );
        }

        upscaleFilter = upscalePluginCont->intf;
    }

    resizeFilteringCaps downsamplingCaps;
    resizeFilteringCaps upscaleCaps;

    downsamplingFilter->GetSupportedFiltering( downsamplingCaps );
    upscaleFilter->GetSupportedFiltering( upscaleCaps );

    if ( downsamplingCaps.supportsMinification == false )
    {
        throw InvalidConfigurationException( eSubsystemType::RESIZING, L"RESIZING_INVALIDCFG_DOWNSAMPLEFLTR_NOMINIFY" );
    }

    if ( upscaleCaps.supportsMagnification == false )
    {
        throw InvalidConfigurationException( eSubsystemType::RESIZING, L"RESIZING_INVALIDCFG_UPSCALEFLTR_NOMAGNIFY" );
    }
    
    // Return valid parameters.
    downsamplingFilterOut = downsamplingFilter;
    upscaleFilterOut = upscaleFilter;

    downsamplingCapsOut = downsamplingCaps;
    upscaleCapsOut = upscaleCaps;
}

};

#endif //_RENDERWARE_RESIZING_INTERNALS_