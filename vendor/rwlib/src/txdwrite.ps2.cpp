/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdwrite.ps2.cpp
*  PURPOSE:     PlayStation2 native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2

#include <cstring>
#include <assert.h>
#include <math.h>

#include "streamutil.hxx"

#include "txdread.ps2.hxx"
#include "txdread.ps2mem.hxx"

#include "txdread.ps2gsman.hxx"

namespace rw
{

bool NativeTexturePS2::generatePS2GPUData(
    LibraryVersion gameVersion,
    ps2GSRegisters& gpuData,
    const uint32 mipmapBasePointer[], const uint32 mipmapBufferWidth[], uint32 maxMipmaps, eMemoryLayoutType memLayoutType,
    uint32 clutBasePointer
) const
{
    const NativeTexturePS2::GSTexture& mainTex = this->mipmaps[0];

    // This algorithm is guarranteed to produce correct values on identity-transformed PS2 GTA:SA textures.
    // There is no guarrantee that this works for modified textures!
    uint32 width = mainTex.width;
    uint32 height = mainTex.height;
    //uint32 depth = this->depth;

    // Reconstruct GPU flags, kinda.
    rw::ps2GSRegisters::TEX0_REG tex0;

    // The base pointers are stored differently depending on game version.
    uint32 finalTexBasePointer = 0;
    uint32 finalClutBasePointer = 0;

    if (gameVersion.rwLibMinor <= 2)
    {
        // We actually preallocate the textures on the game engine GS memory.
        uint32 totalMemOffset = this->recommendedBufferBasePointer;

        finalTexBasePointer = mipmapBasePointer[ 0 ] + totalMemOffset;
        finalClutBasePointer = clutBasePointer + totalMemOffset;
    }
    else
    {
        finalTexBasePointer = mipmapBasePointer[ 0 ];
    }

    tex0.textureBasePointer = finalTexBasePointer;
    tex0.textureBufferWidth = mipmapBufferWidth[ 0 ];
    tex0.pixelStorageFormat = memLayoutType;

    // Store texture dimensions.
    {
        if ( width == 0 || height == 0 )
            return false;

        double log2val = log(2.0);

        double expWidth = ( log((double)width) / log2val );
        double expHeight = ( log((double)height) / log2val );

        // Check that dimensions are power-of-two.
        if ( expWidth != floor(expWidth) || expHeight != floor(expHeight) )
            return false;

        // Check that dimensions are not too big.
        if ( expWidth > 10 || expHeight > 10 )
            return false;

        tex0.textureWidthLog2 = (unsigned int)expWidth;
        tex0.textureHeightLog2 = (unsigned int)expHeight;
    }

    tex0.texColorComponent = (uint32)this->texColorComponent;
    tex0.texFunction = (ps2GSRegisters::ps2reg_t)this->textureFunction;
    tex0.clutBufferBase = finalClutBasePointer;

    bool hasCLUT = DoesMemoryLayoutTypeRequireCLUT( memLayoutType );

    tex0.clutStorageFmt = (uint32)this->clutStorageFmt;
    tex0.clutMode = (uint32)this->clutStorageMode;
    tex0.clutEntryOffset = this->clutEntryOffset;

    if ( hasCLUT )
    {
        tex0.clutLoadControl = 1;
    }
    else
    {
        tex0.clutLoadControl = 0;
    }

    // Calculate TEX1 register.
    rw::ps2GSRegisters::SKY_TEX1_REG_LOW tex1_low;

    tex1_low.lodCalculationModel = this->lodCalculationModel;
    tex1_low.maximumMIPLevel = ( this->mipmaps.GetCount() - 1 );

    // Store mipmap data.
    ps2GSRegisters::MIPTBP1_REG miptbp1;
    ps2GSRegisters::MIPTBP2_REG miptbp2;

    // We want to fill out the entire fields of the registers.
    // This is why we assume the max mipmaps should meet that standard.
    assert(maxMipmaps >= 7);

    // Store the sizes and widths in the registers.
    miptbp1.textureBasePointer1 = mipmapBasePointer[1];
    miptbp1.textureBufferWidth1 = mipmapBufferWidth[1];
    miptbp1.textureBasePointer2 = mipmapBasePointer[2];
    miptbp1.textureBufferWidth2 = mipmapBufferWidth[2];
    miptbp1.textureBasePointer3 = mipmapBasePointer[3];
    miptbp1.textureBufferWidth3 = mipmapBufferWidth[3];

    miptbp2.textureBasePointer4 = mipmapBasePointer[4];
    miptbp2.textureBufferWidth4 = mipmapBufferWidth[4];
    miptbp2.textureBasePointer5 = mipmapBasePointer[5];
    miptbp2.textureBufferWidth5 = mipmapBufferWidth[5];
    miptbp2.textureBasePointer6 = mipmapBasePointer[6];
    miptbp2.textureBufferWidth6 = mipmapBufferWidth[6];

    // Give the data to the runtime.
    gpuData.tex0 = tex0;
    gpuData.clutOffset = clutBasePointer;
    gpuData.tex1_low = tex1_low;

    gpuData.miptbp1 = miptbp1;
    gpuData.miptbp2 = miptbp2;

    return true;
}

uint32 NativeTexturePS2::GSTexture::writeGIFPacket(
    Interface *engineInterface,
    BlockProvider& outputProvider,
    TextureBase *writeToTexture
) const
{
    uint32 writeCount = 0;

    // Write all header GS primitives first.
    LIST_FOREACH_BEGIN( GSPrimitive, this->list_header_primitives.root, listNode )

        GIFtag::eFLG type = item->type;

        uint32 nloop = item->nloop;
        uint32 nreg = item->nregs;

        GIFtag header;
        header.nloop = nloop;
        header.eop = (uint32)GIFtag::eEOP::FOLLOWING_PRIMITIVE;
        header.pad1 = 0;
        header.pre = item->hasPrimValue;
        header.prim = item->primValue;
        header.flg = (uint32)type;
        header.nreg = nreg;
        header.regs = item->registers;

        GIFtag_serialized ser_header;
        ser_header = header;

        outputProvider.writeStruct( ser_header );

        writeCount += sizeof(ser_header);

        uint32 numItems = ( nloop * nreg );

        if ( type == GIFtag::eFLG::PACKED_MODE )
        {
            const GSPrimitivePackedItem *packed_arr = item->GetItemArray <GSPrimitivePackedItem> ();

            for ( size_t n = 0; n < numItems; n++ )
            {
                const GSPrimitivePackedItem& item = packed_arr[ n ];

                outputProvider.writeUInt64( item.gifregvalue_hi );
                outputProvider.writeUInt64( item.gifregvalue_lo );
            }

            writeCount += ( numItems * sizeof(GSPrimitivePackedItem) );
        }
        else if ( type == GIFtag::eFLG::REGLIST_MODE )
        {
            const GSPrimitiveReglistItem *reglist_arr = item->GetItemArray <GSPrimitiveReglistItem> ();

            for ( size_t n = 0; n < numItems; n++ )
            {
                const GSPrimitiveReglistItem& item = reglist_arr[ n ];

                endian::little_endian <GSPrimitiveReglistItem> ser_item( item );

                outputProvider.writeStruct( ser_item );
            }


            writeCount += ( numItems * sizeof(GSPrimitiveReglistItem) );
        }
        else
        {
            throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
        }

    LIST_FOREACH_END

    // Finally write our IMAGE mode GS primitive.
    // We first prepare the parameters.

    uint32 dataSize = this->dataSize;

    // Make sure that our data does properly fit written out as PS2 qwords.
    // If it does not then we will pad zeroes.
    uint32 padded_size = ALIGN_SIZE( dataSize, 16u );

    if ( padded_size >= ( 1 << 15 ) * 16u )     // PS2 qwords.
    {
        throw NativeTextureRwObjectsInternalErrorException( "PlayStation2", AcquireObject( writeToTexture ), L"PS2_INTERNERR_EXCEEDGIFPACKETSIZE" );
    }

    GIFtag imgtag;
    imgtag.nloop = ( padded_size / 16u );
    imgtag.eop = (uint32)GIFtag::eEOP::FOLLOWING_PRIMITIVE;
    imgtag.pad1 = 0;
    imgtag.pre = (uint32)GIFtag::ePRE::IGNORE_PRIM;
    imgtag.prim = 0;
    imgtag.flg = (uint32)GIFtag::eFLG::IMAGE_MODE;
    imgtag.nreg = 0;
    imgtag.regs = 0;

    GIFtag_serialized ser_imgtag;
    ser_imgtag = imgtag;

    outputProvider.writeStruct( ser_imgtag );

    writeCount += sizeof(ser_imgtag);

    // Write the image data now.
    outputProvider.write( this->texels, dataSize );

    // Make sure we pad the data properly.
    uint32 left_padcnt = ( padded_size - dataSize );

    if ( left_padcnt > 0 )
    {
        static const char _zeroes[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

        outputProvider.write( _zeroes, left_padcnt );
    }

    writeCount += padded_size;

    return writeCount;
}

inline void updateTextureRegisters( Interface *rwEngine, NativeTexturePS2::GSTexture& gsTex )
{
    // TRXPOS
    {
        ps2GSRegisters::TRXPOS_REG trxpos;
        trxpos.ssax = 0;
        trxpos.ssay = 0;
        trxpos.dsax = gsTex.transmissionOffsetX;
        trxpos.dsay = gsTex.transmissionOffsetY;
        trxpos.dir = 0;

        gsTex.setGSRegister( rwEngine, eGSRegister::TRXPOS, trxpos.qword );
    }

    // TRXREG
    {
        ps2GSRegisters::TRXREG_REG trxreg;
        trxreg.transmissionAreaWidth = gsTex.transmissionAreaWidth;
        trxreg.transmissionAreaHeight = gsTex.transmissionAreaHeight;

        gsTex.setGSRegister( rwEngine, eGSRegister::TRXREG, trxreg.qword );
    }

    // TRXDIR
    {
        ps2GSRegisters::TRXDIR_REG trxdir;
        trxdir.xdir = 0;

        gsTex.setGSRegister( rwEngine, eGSRegister::TRXDIR, trxdir.qword );
    }
}

void NativeTexturePS2::UpdateStructure( Interface *engineInterface )
{
    LibraryVersion version = this->texVersion;

    // Check whether we have to update the texture contents.
    size_t mipmapCount = this->mipmaps.GetCount();

    eRasterFormatPS2 rasterFormat = this->rasterFormat;
    ePaletteTypePS2 paletteType = this->paletteType;

    bool hasToUpdateContents = ( mipmapCount > 0 );

    if ( hasToUpdateContents )
    {
        // Make sure all textures are in the required encoding format.
        eMemoryLayoutType pixelStorageFmt = this->pixelStorageMode;

        eMemoryLayoutType currentBITBLTLayout = this->bitblt_targetFmt;

        eMemoryLayoutType requiredBITBLTLayout = GetBITBLTMemoryLayoutType( pixelStorageFmt, version );

        if ( currentBITBLTLayout != requiredBITBLTLayout )
        {
            for ( size_t n = 0; n < mipmapCount; n++ )
            {
                NativeTexturePS2::GSTexture& mipLayer = this->mipmaps[ n ];

                uint32 swizzleWidth = mipLayer.transmissionAreaWidth;
                uint32 swizzleHeight = mipLayer.transmissionAreaHeight;

                // Recalculate the packed destination coordinates.
                uint32 packedWidth, packedHeight;
                {
                    ps2MemoryLayoutTypeProperties srcProps = getMemoryLayoutTypeProperties( currentBITBLTLayout );
                    ps2MemoryLayoutTypeProperties dstProps = getMemoryLayoutTypeProperties( requiredBITBLTLayout );

                    uint32 colsWidth = CEIL_DIV( swizzleWidth, srcProps.pixelWidthPerColumn );
                    uint32 colsHeight = CEIL_DIV( swizzleHeight, srcProps.pixelHeightPerColumn );

                    packedWidth = ( colsWidth * dstProps.pixelWidthPerColumn );
                    packedHeight = ( colsHeight * dstProps.pixelHeightPerColumn );
                }

                void *srcTexels = mipLayer.texels;
                uint32 srcDataSize = mipLayer.dataSize;

                uint32 newDataSize;

                void *newtexels;

                // TODO: need to straighten out the permutation engine again.
                // But this can wait until a much further point in time.

                newtexels = permutePS2Data(
                    engineInterface, swizzleWidth, swizzleHeight, srcTexels, srcDataSize,
                    packedWidth, packedHeight,
                    currentBITBLTLayout, requiredBITBLTLayout,
                    getPS2TextureDataRowAlignment(), getPS2TextureDataRowAlignment(),
                    newDataSize
                );

                // Update parameters.
                mipLayer.dataSize = newDataSize;
                mipLayer.texels = newtexels;

                mipLayer.transmissionAreaWidth = packedWidth;
                mipLayer.transmissionAreaHeight = packedHeight;

                // Delete the old texels.
                if ( newtexels != srcTexels )
                {
                    engineInterface->PixelFree( srcTexels );
                }
            }

            // We are now encoded differently.
            this->bitblt_targetFmt = requiredBITBLTLayout;
        }
    }

    // Prepare palette data.
    if ( paletteType != ePaletteTypePS2::NONE )
    {
        uint32 reqPalWidth, reqPalHeight;

        getPaletteTextureDimensions(paletteType, version, reqPalWidth, reqPalHeight);

        // Update the texture.
        NativeTexturePS2::GSTexture& palTex = this->paletteTex;

        void *palDataSource = palTex.texels;
        uint32 palSize = ( palTex.transmissionAreaWidth * palTex.transmissionAreaHeight );
        void *newPalTexels = nullptr;
        uint32 newPalDataSize;

        eRasterFormat frm_rasterFormat = GetGenericRasterFormatFromPS2( rasterFormat );
        ePaletteType frm_paletteType = GetGenericPaletteTypeFromPS2( paletteType );

        genpalettetexeldata(
            engineInterface,
            reqPalWidth, reqPalHeight,
            palDataSource,
            frm_rasterFormat, frm_paletteType, palSize,
            newPalTexels, newPalDataSize
        );

        if ( newPalTexels != palDataSource )
        {
            palTex.transmissionAreaWidth = reqPalWidth;
            palTex.transmissionAreaHeight = reqPalHeight;
            palTex.dataSize = newPalDataSize;
            palTex.texels = newPalTexels;

            engineInterface->PixelFree( palDataSource );
        }
    }
}

void ps2NativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    LibraryVersion version = outputProvider.getBlockVersion();

    // Get access to our native texture.
    NativeTexturePS2 *platformTex = (NativeTexturePS2*)nativeTex;

    // Write the master header.
    {
        BlockProvider texNativeMasterBlock( &outputProvider );

        texNativeMasterBlock.writeUInt32( PS2_FOURCC );

        texFormatInfo formatInfo;
        formatInfo.set( *theTexture );

        formatInfo.writeToBlock( texNativeMasterBlock );
    }

    // Write texture name.
    utils::writeStringChunkANSI(engineInterface, outputProvider, theTexture->GetName().GetConstString(), theTexture->GetName().GetLength());

    // Write mask name.
    utils::writeStringChunkANSI(engineInterface, outputProvider, theTexture->GetMaskName().GetConstString(), theTexture->GetMaskName().GetLength());

    // Prepare the image data (if not already prepared).
    size_t mipmapCount = platformTex->mipmaps.GetCount();

    if ( mipmapCount == 0 )
    {
        throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_INTERNERR_WRITEEMPTY" );
    }

    // Graphics Synthesizer package struct.
    {
        BlockProvider gsNativeBlock( &outputProvider );

        uint16 texVersion = platformTex->encodingVersion;

        // Write the texture meta information.
        const size_t maxMipmaps = 7;

        if ( mipmapCount > maxMipmaps )
        {
            throw NativeTextureRwObjectsInternalErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_INTERNERR_WRITE_TOOMANYMIPMAPS" );
        }

        int64 metaDataHeaderOff_abs;
        {
            BlockProvider metaDataStruct( &gsNativeBlock );

            // Allocate textures.
            uint32 mipmapBasePointer[ maxMipmaps ];
            uint32 mipmapBufferWidth[ maxMipmaps ];

            uint32 clutBasePointer;

            ps2MipmapTransmissionData mipmapTransData[ maxMipmaps ];
            ps2MipmapTransmissionData clutTransData;

            uint32 texMemSize;

            bool couldAllocate = platformTex->allocateTextureMemory(mipmapBasePointer, mipmapBufferWidth, mipmapTransData, maxMipmaps, clutBasePointer, clutTransData, texMemSize);

            (void)couldAllocate;

            ps2GSRegisters gpuData;
            bool isCompatible = platformTex->generatePS2GPUData(version, gpuData, mipmapBasePointer, mipmapBufferWidth, maxMipmaps, platformTex->pixelStorageMode, clutBasePointer);

            if ( isCompatible == false )
            {
                throw NativeTextureRwObjectsInternalErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_INTERNERR_WRITE_FMTFAIL" );
            }

            if ( texVersion > 0 )
            {
                // Update mipmap texture registers.
                for ( uint32 n = 0; n < mipmapCount; n++ )
                {
                    NativeTexturePS2::GSTexture& gsTex = platformTex->mipmaps[ n ];

                    ps2MipmapTransmissionData& transData = mipmapTransData[ n ];
                    gsTex.transmissionOffsetX = transData.destX;
                    gsTex.transmissionOffsetY = transData.destY;

                    updateTextureRegisters( engineInterface, gsTex );
                }

                // Update CLUT registers.
                if ( platformTex->paletteType != ePaletteTypePS2::NONE )
                {
                    platformTex->paletteTex.transmissionOffsetX = clutTransData.destX;
                    platformTex->paletteTex.transmissionOffsetY = clutTransData.destY;

                    updateTextureRegisters( engineInterface, platformTex->paletteTex );
                }
            }

            // Build the raster format flags.
            ps2RasterFormatFlags ps2RasterFlags;
            {
                ps2RasterFlags.rasterType = platformTex->rasterType;
                ps2RasterFlags.privateFlags = platformTex->privateFlags;
                ps2RasterFlags.formatNum = (uint32)platformTex->rasterFormat;
                ps2RasterFlags.autoMipmaps = platformTex->autoMipmaps;
                ps2RasterFlags.palType = (uint32)platformTex->paletteType;
                ps2RasterFlags.hasMipmaps = ( mipmapCount > 1 );
            }

            const NativeTexturePS2::GSTexture& mainTex = platformTex->mipmaps[0];

            textureMetaDataHeader metaHeader;
            metaHeader.miptbp1 = gpuData.miptbp1;
            metaHeader.miptbp2 = gpuData.miptbp2;
            metaHeader.baseLayerWidth = mainTex.width;
            metaHeader.baseLayerHeight = mainTex.height;
            metaHeader.depth = platformTex->depth;
            metaHeader.tex0 = gpuData.tex0;
            metaHeader.clutOffset = gpuData.clutOffset;
            metaHeader.tex1_low = gpuData.tex1_low;
            metaHeader.ps2RasterFlags = ps2RasterFlags;
            metaHeader.version = texVersion;
            metaHeader.gsMipmapDataSize = 0;
            metaHeader.gsCLUTDataSize = 0;
            metaHeader.combinedGPUDataSize = texMemSize;
            metaHeader.skyMipmapVal = platformTex->skyMipMapVal;

            // Remember the offset of this header for later patching when we know the sizes of GIF structs.
            metaDataHeaderOff_abs = metaDataStruct.tell_absolute();

            metaDataStruct.writeStruct( metaHeader );
        }

        // Block sizes.
        uint32 justTextureSize = 0;
        uint32 justPaletteSize = 0;

        // GS packet struct.
        {
            BlockProvider gsPacketBlock( &gsNativeBlock, CHUNK_STRUCT );

            for ( uint32 n = 0; n < mipmapCount; n++ )
            {
                const NativeTexturePS2::GSTexture& gsTex = platformTex->mipmaps[n];

                // Write the packet.
                uint32 writeCount = gsTex.writeGIFPacket(engineInterface, gsPacketBlock, theTexture);

                justTextureSize += writeCount;
            }

            // Write palette information.
            if ( platformTex->paletteType != ePaletteTypePS2::NONE )
            {
                uint32 writeCount = platformTex->paletteTex.writeGIFPacket(engineInterface, gsPacketBlock, theTexture);

                justPaletteSize += writeCount;
            }
        }

        // Patch the GIF section sizes.
        gsNativeBlock.seek_absolute( metaDataHeaderOff_abs + offsetof(textureMetaDataHeader, gsMipmapDataSize) );

        gsNativeBlock.writeUInt32( justTextureSize );
        gsNativeBlock.writeUInt32( justPaletteSize );
    }

    // Extension (TODO: add parsing for the sky mipmap extension)
    engineInterface->SerializeExtensions( theTexture, outputProvider );
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2
