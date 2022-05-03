/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.dxtmobile.cpp
*  PURPOSE:     s3tc_mobile native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_S3TC_MOBILE

#include "txdread.dxtmobile.hxx"

#include "pluginutil.hxx"

#include "pixelformat.hxx"

#include "txdread.d3d.dxt.hxx"

namespace rw
{

eTexNativeCompatibility dxtMobileNativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility texCompat = RWTEXCOMPAT_NONE;

    BlockProvider texNativeImageBlock( &inputProvider, CHUNK_STRUCT );

    // Here we can check the platform descriptor, since we know it is unique.
    uint32 platformDescriptor = texNativeImageBlock.readUInt32();

    if ( platformDescriptor == PLATFORMDESC_DXT_MOBILE )
    {
        // We conflict with Direct3D 9.
        // TODO: actually check the structure of this texture native for absolute guarranteee.
        texCompat = RWTEXCOMPAT_MAYBE;
    }

    return texCompat;
}

void dxtMobileNativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    // Read the texture image data block.
    {
        BlockProvider texImageDataBlock( &inputProvider, CHUNK_STRUCT );

        // Read the meta header.
        mobile_dxt::textureNativeGenericHeader metaHeader;

        texImageDataBlock.read( &metaHeader, sizeof( metaHeader ) );

        // Verify the platform descriptor.
        if ( metaHeader.platformDescriptor != PLATFORMDESC_DXT_MOBILE )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "s3tc_mobile", AcquireObject( theTexture ), L"DXTMOBILE_STRUCTERR_PLATFORMID" );
        }

        // Cast to our native format.
        NativeTextureMobileDXT *platformTex = (NativeTextureMobileDXT*)nativeTex;

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

        eS3TCInternalFormat internalFormat = metaHeader.internalFormat;

        // Check whether we got a valid internalFormat.
        if ( internalFormat != COMPRESSED_RGB_S3TC_DXT1 &&
                internalFormat != COMPRESSED_RGBA_S3TC_DXT1 &&
                internalFormat != COMPRESSED_RGBA_S3TC_DXT3 &&
                internalFormat != COMPRESSED_RGBA_S3TC_DXT5 )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "s3tc_mobile", AcquireObject( theTexture ), L"DXTMOBILE_STRUCTERR_INTERNALFMT" );
        }

        // Store advanced parameters.
        platformTex->hasAlpha = metaHeader.hasAlpha;

        platformTex->internalFormat = internalFormat;
        platformTex->unk3 = metaHeader.unk3;

#ifdef _DEBUG
        assert( metaHeader.unk1 == false );
#endif

        // Read the data sizes of the mipmaps.
        rwVector <uint32> dataSizes( eir::constr_with_alloc::DEFAULT, engineInterface );

        uint32 maybeMipmapCount = metaHeader.mipmapCount;

        uint32 imageDataSectionSize = 0;

        dataSizes.Resize( maybeMipmapCount );

        for ( uint32 n = 0; n < maybeMipmapCount; n++ )
        {
            uint32 dataSize = texImageDataBlock.readUInt32();

            dataSizes[ n ] = dataSize;

            // Also verify the given section size.
            imageDataSectionSize += dataSize;
            imageDataSectionSize += sizeof( uint32 );
        }

        // Check whether the image data section size is correct.
        if ( imageDataSectionSize > metaHeader.imageDataSectionSize )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "s3tc_mobile", AcquireObject( theTexture ), L"DXTMOBILE_STRUCTERR_IMGDATASECTSIZE" );
        }

        // Process all the mipmaps.
        uint32 dxtType = getDXTTypeFromS3TCInternalFormat( internalFormat );

        mipGenLevelGenerator mipLevelGen( metaHeader.width, metaHeader.height );

        if ( !mipLevelGen.isValidLevel() )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "s3tc_mobile", AcquireObject( theTexture ), L"DXTMOBILE_STRUCTERR_MIPDIMMS" );
        }

        uint32 mipmapCount = 0;

        uint32 remainingImageSection = imageDataSectionSize;

        for ( uint32 n = 0; n < maybeMipmapCount; n++ )
        {
            bool couldEstablishLevel = true;

            if ( n > 0 )
            {
                couldEstablishLevel = mipLevelGen.incrementLevel();
            }

            if ( !couldEstablishLevel )
            {
                break;
            }

            // Create a new mipmap layer.
            NativeTextureMobileDXT::mipmapLayer newLayer;

            newLayer.layerWidth = mipLevelGen.getLevelWidth();
            newLayer.layerHeight = mipLevelGen.getLevelHeight();

            // Calculate the real mipmap dimensions.
            uint32 mipWidth = ALIGN_SIZE( newLayer.layerWidth, 4u );
            uint32 mipHeight = ALIGN_SIZE( newLayer.layerHeight, 4u );

            newLayer.width = mipWidth;
            newLayer.height = mipHeight;

            // Calculate the size of this mipmap level.
            uint32 texUnitCount = ( mipWidth * mipHeight );

            uint32 texDataSize = getDXTRasterDataSize( dxtType, texUnitCount );

            // Compare whether the data size is correct.
            if ( texDataSize != dataSizes[ mipmapCount ] )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "s3tc_mobile", AcquireObject( theTexture ), L"DXTMOBILE_STRUCTERR_MIPDAMAGE" );
            }

            // Check whether we got enough image data left.
            if ( remainingImageSection < texDataSize )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "s3tc_mobile", AcquireObject( theTexture ), L"DXTMOBILE_STRUCTERR_MIPDATASIZE" );
            }

            remainingImageSection -= texDataSize;
            remainingImageSection -= sizeof( uint32 );

            // Read the data and store the layer.
            texImageDataBlock.check_read_ahead( texDataSize );

            void *newtexels = engineInterface->PixelAllocate( texDataSize );

            try
            {
                texImageDataBlock.read( newtexels, texDataSize );
            }
            catch( ... )
            {
                engineInterface->PixelFree( newtexels );

                throw;
            }

            // Save the texel data into the layer.
            newLayer.texels = newtexels;
            newLayer.dataSize = texDataSize;

            // Store this layer.
            platformTex->mipmaps.AddToBack( std::move( newLayer ) );

            mipmapCount++;
        }

        if ( mipmapCount == 0 )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "s3tc_mobile", AcquireObject( theTexture ), L"DXTMOBILE_STRUCTERR_EMPTY" );
        }

        // Fix filtering mode.
        fixFilteringMode( *theTexture, mipmapCount );

        // If we skipped any mipmap layers, make sure we skip to the end of the block.
        while ( mipmapCount < maybeMipmapCount )
        {
            uint32 dataSize = dataSizes[ mipmapCount ];

            if ( remainingImageSection < dataSize )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "s3tc_mobile", AcquireObject( theTexture ), L"DXTMOBILE_STRUCTERR_IMGDATASECTSIZE" );
            }

            remainingImageSection -= dataSize;
            remainingImageSection -= sizeof( uint32 );

            mipmapCount++;

            texImageDataBlock.skip( dataSize );
        }

        // Make sure we allow meta-data.
        if ( remainingImageSection > 0 )
        {
            int warningLevel = engineInterface->GetWarningLevel();

            if ( warningLevel >= 3 )
            {
                engineInterface->PushWarningObject( theTexture, L"DXTMOBILE_WARN_IMGSECTBYTESLEFT" );
            }

            // Skip to the end.
            texImageDataBlock.skip( remainingImageSection );
        }

        // Fin.
    }

    // Deserialize extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

static optional_struct_space <PluginDependantStructRegister <dxtMobileNativeTextureTypeProvider, RwInterfaceFactory_t>> dxtMobileNativeTexRegister;

void registerMobileDXTNativePlugin( void )
{
    dxtMobileNativeTexRegister.Construct( engineFactory );
}

void unregisterMobileDXTNativePlugin( void )
{
    dxtMobileNativeTexRegister.Destroy();
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_S3TC_MOBILE