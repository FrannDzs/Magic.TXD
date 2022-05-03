/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.size.blur.cpp
*  PURPOSE:     Blur resize filter implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.size.hxx"

namespace rw
{

struct resizeFilterBlurPlugin : public rasterResizeFilterInterface
{
    void GetSupportedFiltering( resizeFilteringCaps& capsOut ) const override
    {
        capsOut.supportsMagnification = false;
        capsOut.supportsMinification = true;
        capsOut.magnify2D = false;
        capsOut.minify2D = true;
    }

    void MagnifyFiltering(
        const resizeColorPipeline& srcBmp, uint32 magX, uint32 magY, uint32 magScaleX, uint32 magScaleY,
        resizeColorPipeline& dstBmp, uint32 dstX, uint32 dstY
    ) const override
    {
        // TODO.
        throw UnsupportedOperationException( eSubsystemType::RESIZING, nullptr, nullptr );
    }

    void MinifyFiltering(
        const resizeColorPipeline& srcBmp, uint32 minX, uint32 minY, uint32 minScaleX, uint32 minScaleY,
        abstractColorItem& colorItem
    ) const override
    {
        eColorModel model = srcBmp.getColorModel();

        if ( model == COLORMODEL_RGBA )
        {
            additive_expand <decltype( colorItem.rgbaColor.r )> redSumm = 0;
            additive_expand <decltype( colorItem.rgbaColor.g )> greenSumm = 0;
            additive_expand <decltype( colorItem.rgbaColor.b )> blueSumm = 0;
            additive_expand <decltype( colorItem.rgbaColor.a )> alphaSumm = 0;

            // Loop through the texels and calculate a blur.
            uint32 addCount = 0;

            for ( uint32 y = 0; y < minScaleY; y++ )
            {
                for ( uint32 x = 0; x < minScaleX; x++ )
                {
                    abstractColorItem srcColorItem;

                    bool hasColor = srcBmp.fetchcolor(
                        x + minX, y + minY,
                        srcColorItem
                    );

                    if ( hasColor )
                    {
                        // Add colors together.
                        redSumm += srcColorItem.rgbaColor.r;
                        greenSumm += srcColorItem.rgbaColor.g;
                        blueSumm += srcColorItem.rgbaColor.b;
                        alphaSumm += srcColorItem.rgbaColor.a;

                        addCount++;
                    }
                }
            }

            if ( addCount != 0 )
            {
                // Calculate the real color.
                // Also clamp it.
                colorItem.rgbaColor.r = std::min( redSumm / addCount, color_defaults <decltype( redSumm )>::one );
                colorItem.rgbaColor.g = std::min( greenSumm / addCount, color_defaults <decltype( greenSumm )>::one );
                colorItem.rgbaColor.b = std::min( blueSumm / addCount, color_defaults <decltype( blueSumm )>::one );
                colorItem.rgbaColor.a = std::min( alphaSumm / addCount, color_defaults <decltype( alphaSumm )>::one );

                colorItem.model = COLORMODEL_RGBA;
            }
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            additive_expand <decltype( colorItem.luminance.lum )> lumSumm = 0;
            additive_expand <decltype( colorItem.luminance.alpha )> alphaSumm = 0;

            // Loop through the texels and calculate a blur.
            uint32 addCount = 0;

            for ( uint32 y = 0; y < minScaleY; y++ )
            {
                for ( uint32 x = 0; x < minScaleX; x++ )
                {
                    abstractColorItem srcColorItem;

                    bool hasColor = srcBmp.fetchcolor(
                        x + minX, y + minY,
                        srcColorItem
                    );

                    if ( hasColor )
                    {
                        // Add colors together.
                        lumSumm += srcColorItem.luminance.lum;
                        alphaSumm += srcColorItem.luminance.alpha;

                        addCount++;
                    }
                }
            }

            if ( addCount != 0 )
            {
                // Calculate the real color.
                colorItem.luminance.lum = std::min( lumSumm / addCount, color_defaults <decltype( lumSumm )>::one );
                colorItem.luminance.alpha = std::min( alphaSumm / addCount, color_defaults <decltype( alphaSumm )>::one );

                colorItem.model = COLORMODEL_LUMINANCE;
            }
        }
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        RegisterResizeFiltering( engineInterface, "blur", this );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        UnregisterResizeFiltering( engineInterface, this );
    }
};

static optional_struct_space <PluginDependantStructRegister <resizeFilterBlurPlugin, RwInterfaceFactory_t>> resizeFilterBlurPluginRegister;

void registerRasterSizeBlurPlugin( void )
{
    resizeFilterBlurPluginRegister.Construct( engineFactory );
}

void unregisterRasterSizeBlurPlugin( void )
{
    resizeFilterBlurPluginRegister.Destroy();
}

};