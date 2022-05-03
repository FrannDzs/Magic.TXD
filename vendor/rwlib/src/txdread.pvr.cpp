/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.pvr.cpp
*  PURPOSE:     PowerVR native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE

#include "pixelformat.hxx"

#include "txdread.d3d.hxx"
#include "txdread.pvr.hxx"

#include "pluginutil.hxx"

namespace rw
{

void pvrNativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

	// Read the texture image data chunk.
    {
        BlockProvider texImageDataChunk( &inputProvider, CHUNK_STRUCT );

        pvr::textureMetaHeaderGeneric metaHeader;
        texImageDataChunk.read( &metaHeader, sizeof(metaHeader) );

	    uint32 platform = metaHeader.platformDescriptor;

	    if (platform != PLATFORM_PVR)
        {
            throw NativeTextureRwObjectsStructuralErrorException( "PowerVR", AcquireObject( theTexture ), L"POWERVR_STRUCTERR_PLATFORMID" );
        }

        // Cast to our native texture type.
        NativeTexturePVR *platformTex = (NativeTexturePVR*)nativeTex;

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

        // Check that we have a valid compression parameter.
        ePVRInternalFormat internalFormat = metaHeader.internalFormat;
                
        if ( internalFormat != GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG &&
                internalFormat != GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG &&
                internalFormat != GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG &&
                internalFormat != GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "PowerVR", AcquireObject( theTexture ), L"POWERVR_STRUCTERR_INTERNALFMT" );
        }

        platformTex->internalFormat = internalFormat;
                
        platformTex->hasAlpha = metaHeader.hasAlpha;

        // Store some unknown properties.
        platformTex->unk1 = metaHeader.unk1;
        platformTex->unk8 = metaHeader.unk8;

#ifdef _DEBUG
        assert( metaHeader.unk1 == false );
#endif

        // Read the data sizes and keep track of how much we have read already.
        uint32 validImageStreamSize = metaHeader.imageDataStreamSize;

        uint32 maybeMipmapCount = metaHeader.mipmapCount;

        // First comes a list of mipmap data sizes.
        // Read them into a temporary buffer.
        rwVector <uint32> mipDataSizes( eir::constr_with_alloc::DEFAULT, engineInterface );

        mipDataSizes.Resize( maybeMipmapCount );

        // Keep track of how much we have read.
        uint32 imageDataSectionSize = 0;

        for ( uint32 n = 0; n < maybeMipmapCount; n++ )
        {
            uint32 dataSize = texImageDataChunk.readUInt32();
                    
            imageDataSectionSize += sizeof( uint32 );
            imageDataSectionSize += dataSize;

            mipDataSizes[ n ] = dataSize;
        }

        // Verify that the image data section size is correct.
        if ( imageDataSectionSize != metaHeader.imageDataStreamSize )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "PowerVR", AcquireObject( theTexture ), L"POWERVR_STRUCTERR_IMGSTREAMSECTSIZE" );
        }

        // Prepare format compression dimensions.
        uint32 texDepth = getDepthByPVRFormat( internalFormat );

        uint32 comprBlockWidth, comprBlockHeight;
                
        bool gotComprDimms = getPVRCompressionBlockDimensions( texDepth, comprBlockWidth, comprBlockHeight );

        if ( !gotComprDimms )
        {
            throw NativeTextureRwObjectsInternalErrorException( "PowerVR", AcquireObject( theTexture ), L"POWERVR_INTERNERR_COMPRDIMMS" );
        }

        // Read the mipmap layers.
        mipGenLevelGenerator mipLevelGen( metaHeader.width, metaHeader.height );

        if ( !mipLevelGen.isValidLevel() )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "PowerVR", AcquireObject( theTexture ), L"POWERVR_STRUCTERR_INVMIPDIMMS" );
        }

        uint32 mipmapCount = 0;

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

            // Read the data.
            NativeTexturePVR::mipmapLayer newLayer;

            newLayer.layerWidth = mipLevelGen.getLevelWidth();
            newLayer.layerHeight = mipLevelGen.getLevelHeight();

            uint32 texWidth = newLayer.layerWidth;
            uint32 texHeight = newLayer.layerHeight;
            {
                // We need to make sure the dimensions are aligned.
                texWidth = ALIGN_SIZE( texWidth, comprBlockWidth );
                texHeight = ALIGN_SIZE( texHeight, comprBlockHeight );
            }

            newLayer.width = texWidth;
            newLayer.height = texHeight;

            // Verify the data size.
            uint32 texItemCount = ( texWidth * texHeight );

            uint32 texReqDataSize = getPackedRasterDataSize( texItemCount, texDepth );

            uint32 actualDataSize = mipDataSizes[ n ];

            if ( texReqDataSize != actualDataSize )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "PowerVR", AcquireObject( theTexture ), L"POWERVR_STRUCTERR_MIPDAMAGE" );
            }

            // Add the layer.
            newLayer.dataSize = texReqDataSize;

            // Verify that we even have that much data.
            texImageDataChunk.check_read_ahead( texReqDataSize );

            newLayer.texels = engineInterface->PixelAllocate( texReqDataSize );

            try
            {
                texImageDataChunk.read( newLayer.texels, texReqDataSize );
            }
            catch( ... )
            {
                engineInterface->PixelFree( newLayer.texels );

                throw;
            }

            platformTex->mipmaps.AddToBack( std::move( newLayer ) );

            // Increment our mipmap count.
            mipmapCount++;
        }

        if ( mipmapCount == 0 )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "PowerVR", AcquireObject( theTexture ), L"POWERVR_STRUCTERR_EMPTY" );
        }

        // Fix filtering mode.
        fixFilteringMode( *theTexture, mipmapCount );

        // Increment past any mipmap data that we skipped.
        if ( mipmapCount < maybeMipmapCount )
        {
            for ( uint32 n = mipmapCount; n < maybeMipmapCount; n++ )
            {
                uint32 skipDataSize = mipDataSizes[ n ];

                texImageDataChunk.skip( skipDataSize );
            }
        }
    }

    // Now the extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

optional_struct_space <pvrNativeTextureTypeProviderRegister_t> pvrNativeTextureTypeProviderRegister;

void registerPVRNativePlugin( void )
{
    pvrNativeTextureTypeProviderRegister.Construct( engineFactory );
}

void unregisterPVRNativePlugin( void )
{
    pvrNativeTextureTypeProviderRegister.Destroy();
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE