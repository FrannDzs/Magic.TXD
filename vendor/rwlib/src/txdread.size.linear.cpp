/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.size.linear.cpp
*  PURPOSE:     Linear resizing filter implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.size.hxx"

namespace rw
{

inline float linearInterpolateChannel( float left, float right, double mod )
{
    return (float)( ( right - left ) * mod + left );
}

inline abstractColorItem linearInterpolateColorItem(
    const abstractColorItem& left, const abstractColorItem& right,
    double mod
)
{
    abstractColorItem colorOut;

    assert( left.model == right.model );

    colorOut.model = left.model;

    if ( left.model == COLORMODEL_RGBA )
    {
        colorOut.rgbaColor.r = linearInterpolateChannel( left.rgbaColor.r, right.rgbaColor.r, mod );
        colorOut.rgbaColor.g = linearInterpolateChannel( left.rgbaColor.g, right.rgbaColor.g, mod );
        colorOut.rgbaColor.b = linearInterpolateChannel( left.rgbaColor.b, right.rgbaColor.b, mod );
        colorOut.rgbaColor.a = linearInterpolateChannel( left.rgbaColor.a, right.rgbaColor.a, mod );
    }
    else if ( left.model == COLORMODEL_LUMINANCE )
    {
        colorOut.luminance.lum = linearInterpolateChannel( left.luminance.lum, right.luminance.lum, mod );
        colorOut.luminance.alpha = linearInterpolateChannel( left.luminance.alpha, right.luminance.alpha, mod );
    }
    else
    {
        throw InvalidConfigurationException( eSubsystemType::RESIZING, L"RESIZING_FILTER_LINEAR_INVCOLORMODEL" );
    }
    
    return colorOut;
}

inline abstractColorItem addColorItems(
    const abstractColorItem& left, const abstractColorItem& right
)
{
    abstractColorItem colorOut;

    assert( left.model == right.model );

    colorOut.model = left.model;

    if ( left.model == COLORMODEL_RGBA )
    {
        colorOut.rgbaColor.r = ( left.rgbaColor.r + right.rgbaColor.r );
        colorOut.rgbaColor.g = ( left.rgbaColor.g + right.rgbaColor.g );
        colorOut.rgbaColor.b = ( left.rgbaColor.b + right.rgbaColor.b );
        colorOut.rgbaColor.a = ( left.rgbaColor.a + right.rgbaColor.a );
    }
    else if ( left.model == COLORMODEL_LUMINANCE )
    {
        colorOut.luminance.lum = ( left.luminance.lum + right.luminance.lum );
        colorOut.luminance.alpha = ( left.luminance.alpha + right.luminance.alpha );
    }
    else
    {
        throw InvalidConfigurationException( eSubsystemType::RESIZING, L"RESIZING_FILTER_LINEAR_INVCOLORMODEL" );
    }

    return colorOut;
}

inline abstractColorItem subColorItems(
    const abstractColorItem& left, const abstractColorItem& right
)
{
    abstractColorItem colorOut;

    assert( left.model == right.model );

    colorOut.model = left.model;

    if ( left.model == COLORMODEL_RGBA )
    {
        colorOut.rgbaColor.r = ( left.rgbaColor.r - right.rgbaColor.r );
        colorOut.rgbaColor.g = ( left.rgbaColor.g - right.rgbaColor.g );
        colorOut.rgbaColor.b = ( left.rgbaColor.b - right.rgbaColor.b );
        colorOut.rgbaColor.a = ( left.rgbaColor.a - right.rgbaColor.a );
    }
    else if ( left.model == COLORMODEL_LUMINANCE )
    {
        colorOut.luminance.lum = ( left.luminance.lum - right.luminance.lum );
        colorOut.luminance.alpha = ( left.luminance.alpha - right.luminance.alpha );
    }
    else
    {
        throw InvalidConfigurationException( eSubsystemType::RESIZING, L"RESIZING_FILTER_LINEAR_INVCOLORMODEL" );
    }

    return colorOut;
}

struct resizeFilterLinearPlugin : public rasterResizeFilterInterface
{
    void GetSupportedFiltering( resizeFilteringCaps& capsOut ) const override
    {
        capsOut.supportsMagnification = true;
        capsOut.supportsMinification = false;
        capsOut.magnify2D = false;
        capsOut.minify2D = false;
    }

    AINLINE static void linearFilterBetweenPixels(
        const abstractColorItem& left, const abstractColorItem& right,
        uint32 intColorDist,
        double interpStart, double interpMax,
        uint32 vecX, uint32 vecY,
        uint32 dstX, uint32 dstY,
        resizeColorPipeline& dstBmp
    )
    {
        double colorDist = (double)intColorDist;

        // Fill the destination with interpolated colors.
        for ( uint32 n = 0; n < intColorDist; n++ )
        {
            uint32 targetX = ( dstX + vecX * n );
            uint32 targetY = ( dstY + vecY * n );

            double interpMod = interpStart + ( (double)n / colorDist ) * interpMax;

            // Calculate interpolated color.
            abstractColorItem targetColor = linearInterpolateColorItem( left, right, interpMod );

            dstBmp.putcolor( targetX, targetY, targetColor );
        }
    }

    void MagnifyFiltering(
        const resizeColorPipeline& srcBmp,
        uint32 magX, uint32 magY, uint32 magScaleX, uint32 magScaleY,
        resizeColorPipeline& dstBmp, uint32 srcX, uint32 srcY
    ) const override
    {
        bool isTwoDimensional = ( magScaleX > 1 && magScaleY > 1 );

        if ( isTwoDimensional )
        {
            throw InvalidConfigurationException( eSubsystemType::RESIZING, L"RESIZING_FILTER_LINEAR_INVALIDCFG_NOTWODIMMUPSCALE" );
        }

        // This is our middle component.
        abstractColorItem interpolateSource;

        bool gotSrcColor = srcBmp.fetchcolor( srcX, srcY, interpolateSource );

        if ( !gotSrcColor )
        {
            throw InternalErrorException( eSubsystemType::RESIZING, L"RESIZING_FILTER_LINEAR_INTERNERR_SRCCOLORFAIL" );
        }

        // The first half of the area we will is an interpolation from the pixel before to the middle pixel.
        abstractColorItem interpolateLeft;

        // Then the other half is the interpolation from the middle to the pixel after.
        abstractColorItem interpolateRight;

        uint32 scaleXOffset = ( magScaleX - 1 );
        uint32 scaleYOffset = ( magScaleY - 1 );

        uint32 vecX = std::min( 1u, scaleXOffset );
        uint32 vecY = std::min( 1u, scaleYOffset );

        bool gotLeftColor = srcBmp.fetchcolor( srcX - vecX, srcY - vecY, interpolateLeft );

        if ( !gotLeftColor )
        {
            // We will do a copy operation.
            interpolateLeft = interpolateSource;
        }

        bool gotRightColor = srcBmp.fetchcolor( srcX + vecX, srcY + vecY, interpolateRight );

        if ( !gotRightColor )
        {
            interpolateRight = interpolateSource;
        }

        uint32 interpCount = ( scaleXOffset + scaleYOffset + 1 );

        uint32 leftInterpCount = ( interpCount / 2 );
        uint32 rightInterpCount = ( interpCount - leftInterpCount );

        // Do the filtering.
        linearFilterBetweenPixels(
            interpolateLeft, interpolateSource,
            leftInterpCount,
            0.5, 0.5,
            vecX, vecY,
            magX, magY,
            dstBmp
        );

        linearFilterBetweenPixels(
            interpolateSource, interpolateRight,
            rightInterpCount,
            0.0, 0.5,
            vecX, vecY,
            magX + vecX * leftInterpCount, magY + vecY * leftInterpCount,
            dstBmp
        );
    }

    void MinifyFiltering(
        const resizeColorPipeline& srcBmp, uint32 minX, uint32 minY, uint32 minScaleX, uint32 minScaleY,
        abstractColorItem& reducedColor
    ) const override
    {
        throw UnsupportedOperationException( eSubsystemType::RESIZING, nullptr, nullptr );
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        RegisterResizeFiltering( engineInterface, "linear", this );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        UnregisterResizeFiltering( engineInterface, this );
    }
};

static optional_struct_space <PluginDependantStructRegister <resizeFilterLinearPlugin, RwInterfaceFactory_t>> resizeFilterLinearPluginRegister;

void registerRasterResizeLinearPlugin( void )
{
    resizeFilterLinearPluginRegister.Construct( engineFactory );
}

void unregisterRasterResizeLinearPlugin( void )
{
    resizeFilterLinearPluginRegister.Destroy();
}

};