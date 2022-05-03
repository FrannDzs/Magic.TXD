/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/imagepipe.hxx
*  PURPOSE:     Image import helpers.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include "rwimageimporter.h"

struct makeRasterImageImportMethods abstract : public imageImportMethods
{
    inline makeRasterImageImportMethods( rw::Interface *engineInterface )
    {
        this->engineInterface = engineInterface;

        // You have to extend this class with a way to get a consistent native texture name.
    }

    virtual rw::rwStaticString <char> GetNativeTextureName( void ) const = 0;

    rw::Raster* MakeRaster( void ) const override
    {
        rw::Interface *rwEngine = this->engineInterface;

        rw::Raster *texRaster = rw::CreateRaster( rwEngine );

        if ( texRaster )
        {
            try
            {
                // We need to give this raster a start native format.
                // For that we should give it the format it actually should have.
                auto nativeName = GetNativeTextureName();

                texRaster->newNativeData( nativeName.GetConstString() );
            }
            catch( ... )
            {
                // Clean up after error.
                rw::DeleteRaster( texRaster );

                throw;
            }
        }

        return texRaster;
    }

protected:
    rw::Interface *engineInterface;
};

static inline rw::TextureBase* RwMakeTextureFromStream(
    rw::Interface *rwEngine, rw::Stream *imgStream, const filePath& extention,
    const makeRasterImageImportMethods& imgImporter
)
{
    // Based on the extention, try to figure out what the user wants to import.
    // For that we better verify that it really is an image type extention.
    eImportExpectation defImpExp = getActualImageImportExpectation( rwEngine, extention );

    // Load texture data.
    makeRasterImageImportMethods::loadActionResult load_result;

    bool didLoad = imgImporter.LoadImage( imgStream, defImpExp, load_result );

    if ( didLoad )
    {
        rw::TextureBase *texReturn = nullptr;

        // If we have a texture, we just return it.
        if ( load_result.texHandle.is_good() )
        {
            rw::TextureBase *loadedTex = load_result.texHandle.detach();

            // Prepare the texture.
            if ( rw::Raster *texRaster = loadedTex->GetRaster() )
            {
                auto nativeName = imgImporter.GetNativeTextureName();

                // Convert the raster to the desired platform.
                bool couldConvert = rw::ConvertRasterTo( texRaster, nativeName.GetConstString() );

                if ( !couldConvert )
                {
                    imgImporter.OnWarning(
                        MAGIC_TEXT("General.RasPlatFail").arg(ansi_to_qt(nativeName))
                    );
                }
            }

            texReturn = loadedTex;
        }
        else
        {
            // OK, we got an image. This means we should put the raster into a texture and return it!
            // Should not call detach because rw::CreateTexture does add a refCount itself.
            texReturn = rw::CreateTexture( rwEngine, load_result.texRaster );
        }

        return texReturn;
    }

    // Nothing I guess.
    return nullptr;
}
