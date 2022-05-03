/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.unc.cpp
*  PURPOSE:     unc_mobile native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE

#include "txdread.unc.hxx"

#include "pluginutil.hxx"

namespace rw
{

eTexNativeCompatibility uncNativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility texCompat = RWTEXCOMPAT_NONE;

    BlockProvider texNativeImageBlock( &inputProvider, CHUNK_STRUCT );

    // Here we can check the platform descriptor, since we know it is unique.
    uint32 platformDescriptor = texNativeImageBlock.readUInt32();

    if ( platformDescriptor == PLATFORMDESC_UNC_MOBILE )
    {
        texCompat = RWTEXCOMPAT_ABSOLUTE;
    }

    return texCompat;
}

void uncNativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    // Read the texture native block.
    {
        BlockProvider texImageDataBlock( &inputProvider, CHUNK_STRUCT );

        // Read the meta header first.
        mobile_unc::textureNativeGenericHeader metaHeader;

        texImageDataBlock.read( &metaHeader, sizeof( metaHeader ) );

        // Make sure we got the right platform descriptor.
        if ( metaHeader.platformDescriptor != PLATFORMDESC_UNC_MOBILE )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "uncompressed_mobile", AcquireObject( theTexture ), L"UNCMOBILE_STRUCTERR_PLATFORMID" );
        }

        // Cast out native texture type.
        NativeTextureMobileUNC *platformTex = (NativeTextureMobileUNC*)nativeTex;

        // Read the format info.
        metaHeader.formatInfo.parse( *theTexture );

        // Read the texture names.
        {
            char tmpbuf[ sizeof( metaHeader.name ) + 1 ];

            // Make sure the name buffer is zero terminted.
            tmpbuf[ sizeof( metaHeader.name ) ] = '\0';

            // Move over the texture name.
            memcpy( tmpbuf, metaHeader.name, sizeof( metaHeader.name ) );

            theTexture->SetName( tmpbuf );

            // Move over the texture mask name.
            memcpy( tmpbuf, metaHeader.maskName, sizeof( metaHeader.maskName ) );

            theTexture->SetMaskName( tmpbuf );
        }
                
        // Read some advanced properties.
        bool hasAlpha = metaHeader.hasAlpha;

        platformTex->hasAlpha = hasAlpha;
                
        platformTex->unk2 = metaHeader.unk2;
        platformTex->unk3 = metaHeader.unk3;

#ifdef _DEBUG
        assert( metaHeader.unk1 == false );
        assert( metaHeader.unk2 == 0 );
#endif

        // This texture format is very primitive.
        // It supports only RASTER_4444 textures with 16 depth.
        // Everything that this format stores is already storable in the Direct3D 8/9 platform.
        eRasterFormat rasterFormat;
        uint32 depth;
        eColorOrdering colorOrder;

        getUNCRasterFormat( hasAlpha, rasterFormat, colorOrder, depth );

        // Parse all mipmaps.
        // This format is pretty simple.
        uint32 maybeMipmapCount = metaHeader.mipmapCount;

        mipGenLevelGenerator mipLevelGen( metaHeader.width, metaHeader.height );

        if ( !mipLevelGen.isValidLevel() )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "uncompressed_mobile", AcquireObject( theTexture ), L"UNCMOBILE_STRUCTERR_INVMIPDIMMS" );
        }

        uint32 mipmap_index = 0;
                
        uint32 remainingTexImageDataSize = metaHeader.imageDataSectionSize;

        while ( true )
        {
            if ( remainingTexImageDataSize == 0 )
            {
                break;
            }

            if ( mipmap_index >= maybeMipmapCount )
            {
                break;
            }

            bool couldEstablishLevel = true;

            if ( mipmap_index > 0 )
            {
                couldEstablishLevel = mipLevelGen.incrementLevel();
            }

            if ( !couldEstablishLevel )
            {
                break;
            }

            // Create the new mipmap layer.
            NativeTextureMobileUNC::mipmapLayer newLayer;

            uint32 width = mipLevelGen.getLevelWidth();
            uint32 height = mipLevelGen.getLevelHeight();

            newLayer.layerWidth = width;
            newLayer.layerHeight = height;

            // Since we are an uncompressed texture, the layer dimensions equal the raw dimensions.
            newLayer.width = width;
            newLayer.height = height;

            // Calculate the size of this layer.
            rasterRowSize texRowSize = getUNCRasterDataRowSize( width, depth );

            uint32 texDataSize = getRasterDataSizeByRowSize( texRowSize, height );

            // Reduce the texture image data section remainder.
            if ( remainingTexImageDataSize < texDataSize )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "uncompressed_mobile", AcquireObject( theTexture ), L"UNCMOBILE_STRUCTERR_IMGDATASECTSIZE" );
            }

            remainingTexImageDataSize -= texDataSize;

            // Store the texels.
            texImageDataBlock.check_read_ahead( texDataSize );

            void *texels = engineInterface->PixelAllocate( texDataSize );

            try
            {
                texImageDataBlock.read( texels, texDataSize );
            }
            catch( ... )
            {
                engineInterface->PixelFree( texels );

                throw;
            }

            newLayer.texels = texels;
            newLayer.dataSize = texDataSize;

            try
            {
                // Store this layer.
                platformTex->mipmaps.AddToBack( std::move( newLayer ) );
            }
            catch( ... )
            {
                engineInterface->PixelFree( texels );

                throw;
            }

            // Increase the mipmap index.
            mipmap_index++;
        }

        // We do not want no empty textures.
        if ( mipmap_index == 0 )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "uncompressed_mobile", AcquireObject( theTexture ), L"UNCMOBILE_STRUCTERR_EMPTY" );
        }

        // Fix filtering mode.
        fixFilteringMode( *theTexture, mipmap_index );

        int warningLevel = engineInterface->GetWarningLevel();

        // Check whether we have any remaining texture image data.
        if ( remainingTexImageDataSize != 0 )
        {
            if ( warningLevel >= 3 )
            {
                engineInterface->PushWarningObject( theTexture, L"UNCMOBILE_WARN_IMGDATASECTBYTESLEFT" );
            }

            // Skip the meta-data.
            texImageDataBlock.skip( remainingTexImageDataSize );
        }

        // We are done!
    }

    // Deserialize extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

static optional_struct_space <PluginDependantStructRegister <uncNativeTextureTypeProvider, RwInterfaceFactory_t>> uncNativeTexturePlugin;

void registerMobileUNCNativePlugin( void )
{
    uncNativeTexturePlugin.Construct( engineFactory );
}

void unregisterMobileUNCNativePlugin( void )
{
    uncNativeTexturePlugin.Destroy();
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE