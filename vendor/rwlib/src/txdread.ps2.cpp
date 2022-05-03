/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.ps2.cpp
*  PURPOSE:     PlayStation2 native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2

#include <bitset>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <cmath>

#include <type_traits>

#include "txdread.ps2.hxx"
#include "txdread.d3d.hxx"

#include "pixelformat.hxx"

#include "pixelutil.hxx"

#include "txdread.ps2gsman.hxx"

#include "pluginutil.hxx"

#include "txdread.miputil.hxx"

#include "txdread.ps2shared.enc.hxx"

#include <sdk/Permutation.h>

namespace rw
{

#ifdef RWLIB_ENABLE_PS2GSTRACE

void _ps2gstrace_on_texture_deserialize( NativeTexturePS2 *tex );
void _ps2gstrace_finalize_on_exit( void );

#endif //RWLIB_ENABLE_PS2GSTRACE

template <typename callbackType>
AINLINE void for_all_native_gs_registers( const NativeTexturePS2::GSTexture& gsTex, callbackType&& cb )
{
    LIST_FOREACH_BEGIN( NativeTexturePS2::GSPrimitive, gsTex.list_header_primitives.root, listNode )

        GIFtag::eFLG type = item->type;

        // Said register can only occur in packed mode.
        if ( type != GIFtag::eFLG::PACKED_MODE )
        {
            continue;
        }

        GIFtag::RegisterList registers = item->registers;

        NativeTexturePS2::GSPrimitivePackedItem *packed_arr = item->GetItemArray <NativeTexturePS2::GSPrimitivePackedItem> ();

        size_t nregs = item->nregs;
        size_t nloop = item->nloop;

        for ( size_t regiter = 0; regiter < nregs; regiter++ )
        {
            eGIFTagRegister regID = registers.getRegisterID( regiter );

            if ( regID == eGIFTagRegister::A_PLUS_D )
            {
                // Check this entire column of registers (we have to).
                for ( size_t packedregiter = 0; packedregiter < nloop; packedregiter++ )
                {
                    NativeTexturePS2::GSPrimitivePackedItem& regitem = packed_arr[ regiter * nregs + packedregiter ];

                    GIFtag_aplusd& packregInfo = (GIFtag_aplusd&)regitem;

                    eGSRegister packregID = packregInfo.getRegisterType();

                    cb( packregID, regitem.gifregvalue_lo );
                }
            }
        }

    LIST_FOREACH_END
}

// Returns true if the GIF packet register is an usual register type found inside of textures.
AINLINE bool IsGIFTagRegisterUsuallyFoundInTextures( eGIFTagRegister type )
{
    if ( type == eGIFTagRegister::A_PLUS_D )
    {
        return true;
    }

    return false;
}

AINLINE void calcsize_giftag( GIFtag::eFLG type, size_t itemCount, size_t& memsize, size_t& memalign, size_t& veclength )
{
    if ( type == GIFtag::eFLG::PACKED_MODE )
    {
        NativeTexturePS2::GSPrimitive::GetAllocationMetaInfo <NativeTexturePS2::GSPrimitivePackedItem> ( itemCount, memsize, memalign, &veclength );
    }
    else if ( type == GIFtag::eFLG::REGLIST_MODE )
    {
        NativeTexturePS2::GSPrimitive::GetAllocationMetaInfo <NativeTexturePS2::GSPrimitiveReglistItem> ( itemCount, memsize, memalign, &veclength );
    }
    else
    {
        throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
    }
}

AINLINE void datacpy_giftag( NativeTexturePS2::GSPrimitive *prim, const NativeTexturePS2::GSPrimitive *cpysrc, GIFtag::eFLG type, uint16 nloop, uint8 nregs, size_t veclength )
{
    prim->nloop = nloop;
    prim->hasPrimValue = cpysrc->hasPrimValue;
    prim->primValue = cpysrc->primValue;
    prim->type = type;
    prim->nregs = nregs;
    prim->registers = cpysrc->registers;

    // Clone the regular data.
    if ( type == GIFtag::eFLG::PACKED_MODE )
    {
        const NativeTexturePS2::GSPrimitivePackedItem *srcVec = cpysrc->GetItemArray <NativeTexturePS2::GSPrimitivePackedItem> ();
        NativeTexturePS2::GSPrimitivePackedItem *dstVec = prim->GetItemArray <NativeTexturePS2::GSPrimitivePackedItem> ();

        memcpy( dstVec, srcVec, veclength );
    }
    else if ( type == GIFtag::eFLG::REGLIST_MODE )
    {
        const NativeTexturePS2::GSPrimitiveReglistItem *srcVec = cpysrc->GetItemArray <NativeTexturePS2::GSPrimitiveReglistItem> ();
        NativeTexturePS2::GSPrimitiveReglistItem *dstVec = prim->GetItemArray <NativeTexturePS2::GSPrimitiveReglistItem> ();

        memcpy( dstVec, srcVec, veclength );
    }
    else
    {
        throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
    }
}

NativeTexturePS2::GSPrimitive* NativeTexturePS2::GSPrimitive::clone( Interface *rwEngine, uint16 add_cnt )
{
    uint16 nloop = this->nloop;
    uint8 nregs = this->nregs;

    nloop += add_cnt;

    GIFtag::eFLG type = this->type;

    size_t memsize;
    size_t memalign;

    uint32 itemCount = (uint32)nloop * nregs;
    size_t veclength;

    calcsize_giftag( type, itemCount, memsize, memalign, veclength );

    GSPrimitive *prim = (GSPrimitive*)rwEngine->MemAllocate( memsize, memalign );

    try
    {
        datacpy_giftag( prim, this, type, nloop, nregs, veclength );

        // Registration of the primitive is handled by the runtime that requested the clone.
        // Initialization of any newly added values is the responsbility of the runtime.
    }
    catch( ... )
    {
        rwEngine->MemFree( prim );

        throw;
    }

    return prim;
}

NativeTexturePS2::GSPrimitive* NativeTexturePS2::GSPrimitive::extend_by( Interface *rwEngine, uint16 extend_cnt )
{
    // Try to increase the memory size.
    std::uint16_t nloop = this->nloop;
    std::uint8_t nregs = this->nregs;

    nloop += extend_cnt;

    GIFtag::eFLG type = this->type;

    size_t memsize;
    size_t memalign;

    uint32 itemCount = (uint32)nloop * nregs;
    size_t veclength;

    calcsize_giftag( type, itemCount, memsize, memalign, veclength );

    bool extendSuccess = rwEngine->MemResize( this, memsize );

    if ( extendSuccess )
    {
        this->nloop = nloop;
        return this;
    }

    GSPrimitive *prim = (GSPrimitive*)rwEngine->MemAllocate( memsize, memalign );

    try
    {
        datacpy_giftag( prim, this, type, nloop, nregs, veclength );
    }
    catch( ... )
    {
        rwEngine->MemFree( prim );

        throw;
    }

    // Put the new GIFtag at the position of the old.
    prim->listNode.moveFrom( std::move( this->listNode ) );

    // Release the old GIFtag.
    rwEngine->MemFree( this );

    return prim;
}

void NativeTexturePS2::GSTexture::setGSRegister( Interface *rwEngine, eGSRegister regType, uint64 value )
{
    GSPrimitiveReglistItem *lastfound_direct_replace_reg = nullptr;
    GIFtag_aplusd *lastfound_replace_reg = nullptr;
    GSPrimitive *lastfound_extendprim = nullptr;
    size_t lastfound_extendprim_numrows = 0;

    // Search for data where we can replace it at.
    LIST_FOREACH_BEGIN( GSPrimitive, this->list_header_primitives.root, listNode )

        GIFtag::eFLG type = item->type;

        GIFtag::RegisterList registers = item->registers;

        uint32 num_rows = item->nloop;
        uint32 num_cols = item->nregs;

        if ( type == GIFtag::eFLG::PACKED_MODE )
        {
            GSPrimitivePackedItem *packed_arr = item->GetItemArray <GSPrimitivePackedItem> ();

            bool hasFoundAPlusD = false;

            for ( uint32 n = 0; n < num_cols; n++ )
            {
                if ( registers.getRegisterID( n ) == eGIFTagRegister::A_PLUS_D )
                {
                    hasFoundAPlusD = true;

                    for ( uint32 regvalidx = 0; regvalidx < num_rows; regvalidx++ )
                    {
                        GSPrimitivePackedItem& item = packed_arr[ regvalidx * num_cols + n ];

                        GIFtag_aplusd& regID = (GIFtag_aplusd&)item;

                        if ( regID.getRegisterType() == regType )
                        {
                            lastfound_replace_reg = &regID;
                        }
                    }
                }
            }

            if ( num_cols == 1 && hasFoundAPlusD )
            {
                lastfound_extendprim = item;
                lastfound_extendprim_numrows = num_rows;
            }
        }
        else if ( type == GIFtag::eFLG::REGLIST_MODE )
        {
            GSPrimitiveReglistItem *reglist_arr = item->GetItemArray <GSPrimitiveReglistItem> ();

            for ( uint32 n = 0; n < num_cols; n++ )
            {
                eGIFTagRegister colRegType = registers.getRegisterID( n );

                eGSRegister directRegType;

                if ( GetDirectGIFTagRegisterToGSRegisterMapping( colRegType, directRegType ) && directRegType == regType )
                {
                    if ( num_rows > 0 )
                    {
                        uint32 row_iter = ( num_rows - 1 );

                        lastfound_direct_replace_reg = &reglist_arr[ row_iter * num_cols + n ];
                    }
                    else if ( num_cols == 1 )
                    {
                        lastfound_extendprim = item;
                        lastfound_extendprim_numrows = num_rows;
                    }
                }
            }
        }

    LIST_FOREACH_END

    if ( lastfound_replace_reg )
    {
        lastfound_replace_reg->gsregvalue = value;
        return;
    }
    else if ( lastfound_direct_replace_reg )
    {
        lastfound_direct_replace_reg->gsregvalue = value;
        return;
    }

    GSPrimitive *extendprim;
    size_t extendprim_rowcount;

    if ( lastfound_extendprim )
    {
        extendprim = lastfound_extendprim->extend_by( rwEngine, 1 );
        extendprim_rowcount = lastfound_extendprim_numrows;
    }
    else
    {
        size_t memsize, memalign;

        uint16 nloop = 1;
        uint16 nreg = 1;

        GIFtag::eFLG typeOfPrim;
        GIFtag::RegisterList registers;

        // Depends on what register we want to set.
        eGIFTagRegister directRegType;

        uint32 itemCount = (uint32)nloop * nreg;

        if ( GetDirectGSRegisterToGIFTagRegisterMapping( regType, directRegType ) )
        {
            GSPrimitive::GetAllocationMetaInfo <GSPrimitiveReglistItem> ( itemCount, memsize, memalign );

            registers.setRegisterID( 0, directRegType );

            typeOfPrim = GIFtag::eFLG::REGLIST_MODE;
        }
        else
        {
            // Make a new primitive here with just one column of A_PLUS_D registers.
            GSPrimitive::GetAllocationMetaInfo <GSPrimitivePackedItem> ( itemCount, memsize, memalign );

            registers.setRegisterID( 0, eGIFTagRegister::A_PLUS_D );

            typeOfPrim = GIFtag::eFLG::PACKED_MODE;
        }

        GSPrimitive *prim = (GSPrimitive*)rwEngine->MemAllocate( memsize, memalign );

        prim->nloop = nloop;
        prim->hasPrimValue = false;
        prim->primValue = 0;
        prim->type = typeOfPrim;
        prim->nregs = nreg;
        prim->registers = registers;

        // Add the primitive to the texture.
        LIST_APPEND( this->list_header_primitives.root, prim->listNode );

        // Return the new primitive.
        extendprim = prim;
        extendprim_rowcount = 0;
    }

    GIFtag::eFLG type = extendprim->type;

    if ( type == GIFtag::eFLG::PACKED_MODE )
    {
        GSPrimitivePackedItem *packed_arr = extendprim->GetItemArray <GSPrimitivePackedItem> ();

        GSPrimitivePackedItem& newitem = packed_arr[ extendprim_rowcount ];

        GIFtag_aplusd& gsreg = (GIFtag_aplusd&)newitem;
        gsreg.pad1 = 0;
        gsreg.regID = (uint8)regType;
        gsreg.gsregvalue = value;
    }
    else if ( type == GIFtag::eFLG::REGLIST_MODE )
    {
        GSPrimitiveReglistItem *reglist_arr = extendprim->GetItemArray <GSPrimitiveReglistItem> ();

        GSPrimitiveReglistItem& newitem = reglist_arr[ extendprim_rowcount ];

        newitem.gsregvalue = value;
    }
    else
    {
        throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
    }
}

uint32 NativeTexturePS2::GSTexture::readGIFPacket( Interface *engineInterface, BlockProvider& inputProvider, TextureBase *readFromTexture )
{
    // See https://www.dropbox.com/s/onjaprt82y81sj7/EE_Users_Manual.pdf page 151

    // If there are headers then we read GIF packets, otherwise we just read texel data and
    // calculate the required data size from the given parameters.

    // We read until we find an image data packet. We compress all register writes into a big container
    // so that we remember them all. Registers will be stored in order of their occurrance.
    // Since registers do have side effects when written to the GS we cannot squash them.
    // In fact we must preserve GIFtags as they are.

    uint32 readCount = 0;

    // Debug register contents (only the important ones).
    bool hasTRXPOS = false;
    bool hasTRXREG = false;
    bool hasTRXDIR = false;

    bool hasUnusualGSRegister = false;
    bool hasUnusualGIFtagRegister = false;

    auto on_gs_native_register = [&]( eGSRegister packregID, uint64 regcontent )
    {
        // But for important registers we do actually check the contents.
        if ( packregID == eGSRegister::TRXPOS )
        {
            // TRXPOS
            const ps2GSRegisters::TRXPOS_REG& trxpos = (const ps2GSRegisters::TRXPOS_REG&)regcontent;

            if ( trxpos.ssax != 0 )
            {
                engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_INVREG_TRXPOS_SSAX" );
            }
            if ( trxpos.ssay != 0 )
            {
                engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_INVREG_TRXPOS_SSAY" );
            }
            if ( trxpos.dir != 0 )
            {
                engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_INVREG_TRXPOS_DIR" );
            }

            // Store a copy of the transmission offset destination.
            this->transmissionOffsetX = trxpos.dsax;
            this->transmissionOffsetY = trxpos.dsay;

            hasTRXPOS = true;
        }
        else if ( packregID == eGSRegister::TRXREG )
        {
            // TRXREG
            const ps2GSRegisters::TRXREG_REG& trxreg = (const ps2GSRegisters::TRXREG_REG&)regcontent;

            // Store the transmission area dimensions.
            this->transmissionAreaWidth = trxreg.transmissionAreaWidth;
            this->transmissionAreaHeight = trxreg.transmissionAreaHeight;

            // TODO: maybe verify whether the transmission area does match the image data size that
            // we have got? Might need extensive native raster format checks at one point during runtime.

            hasTRXREG = true;
        }
        else if ( packregID == eGSRegister::TRXDIR )
        {
            // TRXDIR
            const ps2GSRegisters::TRXDIR_REG& trxdir = (const ps2GSRegisters::TRXDIR_REG&)regcontent;

            // Textures have to be transferred to the GS memory.
            if ( trxdir.xdir != 0 )
            {
                engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_INVREG_TRXDIR_XDIR" );
            }

            // After writing to the TRXDIR register the GS does kick of transmission with the
            // currently set TRXREG/TRXDIR/BITBLTREG configuration.
            // Hence if no such register was set before TRXDIR then the GS does not know what to send.
            if ( hasTRXREG == false || hasTRXPOS == false )
            {
                engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_TRXDIRWITHOUTCFGREGS" );
            }

            hasTRXDIR = true;
        }
        else
        {
            // We usually only expect transmission registers inside of PS2 textures.
            // Anything else might cause unwanted effects inside of the pipeline but
            // we do trust the texture enough to keep unusual stuff around.
            hasUnusualGSRegister = true;
        }
    };

    // Continue reading until finish.
    // We do NOT finish if we do not find an IMAGE tag so no need to check if we found it.
    while ( true )
    {
        GIFtag_serialized currentTag_ser;
        inputProvider.readStruct( currentTag_ser );

        readCount += sizeof(currentTag_ser);

        GIFtag currentTag = currentTag_ser;

        if ( (GIFtag::eEOP)currentTag.eop == GIFtag::eEOP::END_OF_PACKET )
        {
            // We do not terminate ongoing GS primitive chains early so do not remember this EOP=1 marker.
            engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_GIFPACKETEARLYEOP" );
        }

        GIFtag::eFLG type = (GIFtag::eFLG)currentTag.flg;

        uint32 nloop = currentTag.nloop;

        if ( type == GIFtag::eFLG::PACKED_MODE || type == GIFtag::eFLG::REGLIST_MODE )
        {
            // Get the primitive allocation info.
            size_t primitiveMemSize;
            size_t primitiveMemAlignment;

            uint32 nreg_raw = currentTag.nreg;
            uint32 numberOfRegisterDesc = ( nreg_raw == 0 ? 16 : nreg_raw );

            // Verify the validity of the register list because it is important now.
            // An invalid value could mean HAVOC.
            // It could be argued that we could recover from this error by patching the structure,
            // but we do not want to implement too much fixing because the community around PS2
            // native texture is very elitist.
            GIFtag::RegisterList registers = currentTag.regs;

            for ( uint32 n = 0; n < numberOfRegisterDesc; n++ )
            {
                eGIFTagRegister gifRegType = registers.getRegisterID( n );

                if ( GetValidGIFTagRegisterName( gifRegType ) == nullptr )
                {
                    throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( readFromTexture ), L"PS2_STRUCTERR_INVGIFTAGREGID" );
                }

                if ( IsGIFTagRegisterUsuallyFoundInTextures( gifRegType ) == false )
                {
                    hasUnusualGIFtagRegister = true;
                }
            }

            uint32 numItems = numberOfRegisterDesc * nloop;

            // Check if we even have the required data in the stream before allocation work.
            // Could be that this is an attack vector after all.
            size_t rawItemSize;

            // EE_Users_Manual page 151.

            if ( type == GIFtag::eFLG::PACKED_MODE )
            {
                rawItemSize = sizeof(GSPrimitivePackedItem);       // PS2 qword

                GSPrimitive::GetAllocationMetaInfo <GSPrimitivePackedItem> ( numItems, primitiveMemSize, primitiveMemAlignment );
            }
            else if ( type == GIFtag::eFLG::REGLIST_MODE )
            {
                rawItemSize = sizeof(GSPrimitiveReglistItem);       // PS2 dword

                GSPrimitive::GetAllocationMetaInfo <GSPrimitiveReglistItem> ( numItems, primitiveMemSize, primitiveMemAlignment );
            }
            else
            {
                FATAL_ABORT();
            }

            size_t dataToBeRead = ( rawItemSize * numItems );

            inputProvider.check_read_ahead( dataToBeRead );

            GSPrimitive *prim = (GSPrimitive*)engineInterface->MemAllocate( primitiveMemSize, primitiveMemAlignment );

            prim->nloop = nloop;
            prim->hasPrimValue = currentTag.pre;
            prim->primValue = currentTag.prim;
            prim->registers = registers;
            prim->nregs = numberOfRegisterDesc;
            prim->type = type;

            try
            {
                if ( type == GIFtag::eFLG::PACKED_MODE )
                {
                    GSPrimitivePackedItem *packed_arr = prim->GetItemArray <GSPrimitivePackedItem> ();

                    for ( size_t n = 0; n < numItems; n++ )
                    {
                        GSPrimitivePackedItem& curItem = packed_arr[ n ];
                        curItem.gifregvalue_hi = inputProvider.readUInt64();
                        curItem.gifregvalue_lo = inputProvider.readUInt64();

                        eGIFTagRegister gifRegType = registers.getRegisterID( (uint32)( n % numberOfRegisterDesc ) );

                        if ( gifRegType == eGIFTagRegister::A_PLUS_D )
                        {
                            // Verify that this is a valid register.
                            // We do not check the contents anyway because that would be too much effort.
                            // But we can scan the buffers for casual validity.
                            GIFtag_aplusd& regID = (GIFtag_aplusd&)curItem;

                            eGSRegister regType = regID.getRegisterType();

                            if ( GetValidGSRegisterName( regType ) == nullptr )
                            {
                                throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( readFromTexture ), L"PS2_STRUCTERR_INVPACKREGTYPE" );
                            }

                            // Do advanced checks and stuff.
                            on_gs_native_register( regType, regID.gsregvalue );
                        }
                    }
                }
                else if ( type == GIFtag::eFLG::REGLIST_MODE )
                {
                    GSPrimitiveReglistItem *reglist_arr = prim->GetItemArray <GSPrimitiveReglistItem> ();

                    for ( size_t n = 0; n < numItems; n++ )
                    {
                        endian::little_endian <GSPrimitiveReglistItem> ser_val;
                        inputProvider.readStruct( ser_val );

                        reglist_arr[n] = ser_val;
                    }
                }
            }
            catch( ... )
            {
                engineInterface->MemFree( prim );

                throw;
            }

            // So we read the data. Acknoledge that.
            readCount += (uint32)dataToBeRead;

            // Link our primitive with the texture.
            LIST_APPEND( this->list_header_primitives.root, prim->listNode );

            // Go for the next primitive, hopefully the IMAGE_MODE tag.
        }
        else if ( type == GIFtag::eFLG::IMAGE_MODE )
        {
            // Special GIFtag, we store as mipmap instead.
            // Then we terminate our loop.

            // The specification says that REGS, NREG, PRIM and PRE are discarded in this mode.
            // So we can ignore their values.
            if ( currentTag.prim != 0 )
            {
                engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_IMGTAG_PRIMNOTZERO" );
            }
            if ( currentTag.regs != 0 )
            {
                engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_IMGTAG_REGSNOTZERO" );
            }
            if ( currentTag.nreg != 0 )
            {
                engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_IMGTAG_NREGNOTZERO" );
            }
            if ( currentTag.pre != 0 )
            {
                engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_IMGTAG_PRENOTZERO" );
            }

            uint32 mipmapLayerSize = ( nloop * 16 );    // PS2 qwords

            inputProvider.check_read_ahead( mipmapLayerSize );

            // Please note that we read texels in transmission packed format here.
            // All texels are expected to have transnmission depth instead of their
            // regular depth, most notably the PSMCT24 type having 24bit instead
            // of the unpacked 32bit in GS memory after GIF passthrough.
            // Thus to unswizzle a texture we must take transmission depths using
            // GetMemoryLayoutTypeGIFPackingDepth.
            void *texels = nullptr;

            if ( mipmapLayerSize != 0 )
            {
                // TODO: do the texels need any special alignment? need to check that framework-wide.
                texels = engineInterface->PixelAllocate( mipmapLayerSize );

                try
                {
                    inputProvider.read( texels, mipmapLayerSize );
                }
                catch( ... )
                {
                    engineInterface->PixelFree( texels );

                    throw;
                }
            }

            readCount += mipmapLayerSize;

            this->texels = texels;
            this->dataSize = mipmapLayerSize;

            break;
        }
        else // if ( type == GIFtag::eFLG::DISABLED )
        {
            // Just skip this tag.
            // It is marked to be disabled anyway.
            uint32 skipSize = nloop * 16;

            inputProvider.check_read_ahead( skipSize );
            inputProvider.skip( skipSize );
        }
    }

    // We kinda require all registers in HW mode.
    if ( hasTRXPOS == false )
    {
        throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( readFromTexture ), L"PS2_STRUCTERR_MISSINGREG_TRXPOS" );
    }
    if ( hasTRXREG == false )
    {
        throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( readFromTexture ), L"PS2_STRUCTERR_MISSINGREG_TRXREG" );
    }
    if ( hasTRXDIR == false )
    {
        throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( readFromTexture ), L"PS2_STRUCTERR_MISSINGREG_TRXDIR" );
    }

    // Report unusual findings.
    if ( hasUnusualGIFtagRegister )
    {
        engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_UNUSUALTEXGIFTAGREGISTER" );
    }
    if ( hasUnusualGSRegister )
    {
        engineInterface->PushWarningObject( readFromTexture, L"PS2_WARN_UNUSUALTEXGSREGISTER" );
    }

    return readCount;
}

// Calculates the memory layout dimensions from raw image dimensions.
inline void calculateMemoryLayoutDimensions( eMemoryLayoutType type, uint32 rawWidth, uint32 rawHeight, uint32& memWidthOut, uint32& memHeightOut )
{
    ps2MemoryLayoutTypeProperties props = getMemoryLayoutTypeProperties( type );

    memWidthOut = ALIGN_SIZE( rawWidth, props.pixelWidthPerColumn );
    memHeightOut = rawHeight;   // there are no restrictions on the height, surprisingly.
}

uint32 NativeTexturePS2::GSTexture::readRawTexels( Interface *rwEngine, eMemoryLayoutType memLayoutType, BlockProvider& inputProvider )
{
    // Calculate the encoded dimensions.
    calculateMemoryLayoutDimensions(
        memLayoutType,
        this->width, this->height,
        this->transmissionAreaWidth, this->transmissionAreaHeight
    );

    // Calculate the texture data size.
    uint32 numTexItems = ( this->transmissionAreaWidth * this->transmissionAreaHeight );

    uint32 transferDepth = GetMemoryLayoutTypeGIFPackingDepth( memLayoutType );

    uint32 dataSize = ALIGN_SIZE( numTexItems * transferDepth, 8u ) / 8u;

    // First check that we have the data in the stream.
    // Prevents attack vectors.
    inputProvider.check_read_ahead( dataSize );

    void *texels = rwEngine->PixelAllocate( dataSize );

    try
    {
        inputProvider.read( texels, dataSize );
    }
    catch( ... )
    {
        rwEngine->PixelFree( texels );

        throw;
    }

    this->texels = texels;
    this->dataSize = dataSize;

    return dataSize;
}

eTexNativeCompatibility ps2NativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility returnCompat = RWTEXCOMPAT_NONE;

    // Go into the master header.
    BlockProvider texNativeMasterHeader( &inputProvider, CHUNK_STRUCT );

    // We simply verify the checksum.
    // If it matches, we believe it definately is a PS2 texture.
    uint32 checksum = texNativeMasterHeader.readUInt32();

    if ( checksum == PS2_FOURCC )
    {
        returnCompat = RWTEXCOMPAT_ABSOLUTE;
    }

    // We are not a PS2 texture for whatever reason.
    return returnCompat;
}

void ps2NativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    // Read the PS2 master header struct.
    {
        BlockProvider texNativeMasterHeader( &inputProvider, CHUNK_STRUCT );

	    uint32 checksum = texNativeMasterHeader.readUInt32();

	    if (checksum != PS2_FOURCC)
        {
            throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_PLATFORMID" );
        }

        texFormatInfo formatInfo;
        formatInfo.readFromBlock( texNativeMasterHeader );

        // Read texture format.
        formatInfo.parse( *theTexture );
    }

    int engineWarningLevel = engineInterface->GetWarningLevel();

    // Cast our native texture.
    NativeTexturePS2 *platformTex = (NativeTexturePS2*)nativeTex;

    // Read the name chunk section.
    {
        rwStaticString <char> nameOut;

        utils::readStringChunkANSI( engineInterface, inputProvider, nameOut );

        theTexture->SetName( nameOut.GetConstString() );
    }

    // Read the mask name chunk section.
    {
        rwStaticString <char> nameOut;

        utils::readStringChunkANSI( engineInterface, inputProvider, nameOut );

        theTexture->SetMaskName( nameOut.GetConstString() );
    }

    // Absolute maximum of mipmaps.
    const size_t maxMipmaps = 7;

    // Graphics Synthesizer package struct.
    {
        BlockProvider gsNativeBlock( &inputProvider, CHUNK_STRUCT );

        // Texture Meta Struct.
        textureMetaDataHeader textureMeta;
        {
            BlockProvider textureMetaChunk( &gsNativeBlock, CHUNK_STRUCT );

            // Read it.
            textureMetaChunk.read( &textureMeta, sizeof( textureMeta ) );

            // If we still have bytes after this struct then do want the user.
            if ( textureMetaChunk.getBlockLength() > textureMetaChunk.tell() )
            {
                engineInterface->PushWarningObject( theTexture, L"PS2_WARN_TEXMETACHUNKDATALEFT" );
            }
        }

        ps2RasterFormatFlags rasterFormatFlags = textureMeta.ps2RasterFlags;

        uint32 depth = textureMeta.depth;

        // Deconstruct the rasterFormat.
        eRasterFormatPS2 ps2RasterFormat;
        {
            ps2RasterFormat = (eRasterFormatPS2)rasterFormatFlags.formatNum;

            if ( GetValidPS2RasterFormatName( ps2RasterFormat ) == nullptr )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_INVRASTERFMT" );
            }

            ePaletteTypePS2 ps2PaletteType = (ePaletteTypePS2)rasterFormatFlags.palType;

            if ( GetValidPS2PaletteTypeName( ps2PaletteType ) == nullptr )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_INVPALTYPE" );
            }

            platformTex->rasterFormat = ps2RasterFormat;
            platformTex->paletteType = ps2PaletteType;
            platformTex->autoMipmaps = rasterFormatFlags.autoMipmaps;
        }

        platformTex->encodingVersion = textureMeta.version;

        // Store the raster type.
        platformTex->rasterType = rasterFormatFlags.rasterType;

        platformTex->privateFlags = rasterFormatFlags.privateFlags;

        // Store unique parameters from the texture registers.
        ps2GSRegisters::TEX0_REG tex0( textureMeta.tex0 );
        ps2GSRegisters::SKY_TEX1_REG_LOW tex1_low( textureMeta.tex1_low );

        ps2GSRegisters::MIPTBP1_REG miptbp1( textureMeta.miptbp1 );
        ps2GSRegisters::MIPTBP2_REG miptbp2( textureMeta.miptbp2 );

        // Check for invalid parameters in the textures.
        eMemoryLayoutType basePixelStorageFormat = (eMemoryLayoutType)tex0.pixelStorageFormat;
        eTextureColorComponent textureColorComponent = (eTextureColorComponent)tex0.texColorComponent;
        eCLUTMemoryLayoutType clutStorageFmt = (eCLUTMemoryLayoutType)tex0.clutStorageFmt;
        eCLUTStorageMode clutStorageMode = (eCLUTStorageMode)tex0.clutMode;

        // We request that all the parameters are valid.
        // Since there are not many community tools that would fuck us in the ass here we are "safe".
        // Unless somebody does find a valid texture in the wild that has these parameters fucked?
        // The Sky engine is known to be robust though.
        if ( GetValidMemoryLayoutTypeName( basePixelStorageFormat ) == nullptr )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_INVBASEPIXSTORAGEFMT" );
        }

        bool hasCLUT = DoesMemoryLayoutTypeRequireCLUT( basePixelStorageFormat );

        if ( hasCLUT && GetValidCLUTMemoryLayoutTypeName( clutStorageFmt ) == nullptr )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_INVCLUTPIXSTORAGEFMT" );
        }

        // TCC and CSM cannot be invalid parameter mapping here because they are both 1 bit wide.

        // Verify the texture depth.
        {
            uint32 texDepth = 0;

            if ( platformTex->paletteType == ePaletteTypePS2::PAL4 )
            {
                texDepth = 4;
            }
            else if ( platformTex->paletteType == ePaletteTypePS2::PAL8 )
            {
                texDepth = 8;
            }
            else
            {
                texDepth = GetMemoryLayoutTypeGIFPackingDepth( basePixelStorageFormat );
            }

            if ( texDepth != depth )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_INVDEPTH" );
            }
        }

        // Verify that the raster format does match the memory layout.
        if ( IsPS2RasterFormatCompatibleWithMemoryLayoutType( ps2RasterFormat, platformTex->paletteType, basePixelStorageFormat, clutStorageFmt ) == false )
        {
            throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_PIXSTORAGEFMTMISMATCH" );
        }

        platformTex->depth = depth;

        platformTex->pixelStorageMode = basePixelStorageFormat;
        platformTex->texColorComponent = textureColorComponent;
        platformTex->clutStorageFmt = clutStorageFmt;
        platformTex->clutStorageMode = clutStorageMode;
        platformTex->clutEntryOffset = tex0.clutEntryOffset;
        platformTex->clutLoadControl = tex0.clutLoadControl;
        platformTex->textureFunction = (eTextureFunction)tex0.texFunction;
        platformTex->lodCalculationModel = tex1_low.lodCalculationModel;
        platformTex->clutOffset = textureMeta.clutOffset;

        platformTex->bitblt_targetFmt = GetBITBLTMemoryLayoutType( basePixelStorageFormat, gsNativeBlock.getBlockVersion() );

        platformTex->texMemSize = textureMeta.combinedGPUDataSize;

        if ( tex1_low.unused1 != 0 || tex1_low.unused2 != 0 || tex1_low.unused3 != 0 )
        {
            engineInterface->PushWarningObject( theTexture, L"PS2_WARN_DATAINUNUSEDTEX1LOW" );
        }

        // Warn about unsupported stuff; we just set them to zero upon serialization.
        // I don't think that RW does actually support them.
        if ( tex1_low.mtba != 0 )
        {
            engineInterface->PushWarningObject( theTexture, L"PS2_WARN_TEX1MTBAUNSUPP" );
        }

        // Warn about unused fields; they are overridden using RW stuff.
        if ( tex1_low.mmin != 0 )
        {
            engineInterface->PushWarningObjectSingleTemplate( theTexture, L"PS2_WARN_UNUSEDTEX1LOWFIELD", L"what", L"MMIN" );
        }
        if ( tex1_low.mmag != 0 )
        {
            engineInterface->PushWarningObjectSingleTemplate( theTexture, L"PS2_WARN_UNUSEDTEX1LOWFIELD", L"what", L"MMAG" );
        }
        if ( tex1_low.lodParamL != 0 )
        {
            engineInterface->PushWarningObjectSingleTemplate( theTexture, L"PS2_WARN_UNUSEDTEX1LOWFIELD", L"what", L"L" );
        }

        // Try to find some special raster versions.
        if ( textureMeta.version != 0 && textureMeta.version != 2 )
        {
            engineInterface->PushWarningObject( theTexture, L"PS2_WARN_UNKINTERNVER" );
        }

        // Read the number of mipmaps from the stream.
        size_t regNumMipmaps = ( tex1_low.maximumMIPLevel + 1 );

        // If we are on the GTA III engine, we need to store the recommended buffer base pointer.
        LibraryVersion libVer = inputProvider.getBlockVersion();

        if ( libVer.rwLibMinor <= 3 )
        {
            platformTex->recommendedBufferBasePointer = tex0.textureBasePointer;
        }

        uint32 gsMipmapDataSize = textureMeta.gsMipmapDataSize;

        platformTex->skyMipMapVal = textureMeta.skyMipmapVal;

        // TODO: figure out the real meaning behind the "swizzle flag";
        //  does it indicate presence of column-level permutation?

        bool hasHeader = ( platformTex->encodingVersion > 0 );

        // GS packet struct.
        {
            BlockProvider gsPacketBlock( &gsNativeBlock, CHUNK_STRUCT );

            /* Pixels/Indices */
            int64 end = gsPacketBlock.tell();
            end += gsMipmapDataSize;
            uint32 mipidx = 0;

            uint32 remainingImageData = gsMipmapDataSize;

            mipGenLevelGenerator mipLevelGen( textureMeta.baseLayerWidth, textureMeta.baseLayerHeight );

            if ( !mipLevelGen.isValidLevel() )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_INVMIPDIMMS" );
            }

            bool hasMipmaps = rasterFormatFlags.hasMipmaps;

            while (gsPacketBlock.tell() < end)
            {
                if (mipidx == maxMipmaps)
                {
                    // We cannot have more than the maximum mipmaps.
                    break;
                }

                if (mipidx > 0 && !hasMipmaps)
                {
                    break;
                }

                // half dimensions if we have mipmaps
                bool couldEstablishMipmap = true;

                if (mipidx > 0)
                {
                    couldEstablishMipmap = mipLevelGen.incrementLevel();
                }

                if ( !couldEstablishMipmap )
                {
                    break;
                }

                // Create a new mipmap.
                platformTex->mipmaps.Resize( mipidx + 1 );

                NativeTexturePS2::GSTexture& newMipmap = platformTex->mipmaps[mipidx];

                newMipmap.width = mipLevelGen.getLevelWidth();
                newMipmap.height = mipLevelGen.getLevelHeight();

                uint32 readCount;

                if ( hasHeader )
                {
                    readCount = newMipmap.readGIFPacket( engineInterface, gsPacketBlock, theTexture );
                }
                else
                {
                    readCount = newMipmap.readRawTexels( engineInterface, basePixelStorageFormat, inputProvider );
                }

                if ( readCount > remainingImageData )
                {
                    throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_GIFPACKETOVERFLOW" );
                }

                remainingImageData -= readCount;

                // Apply the off-shore GS properties.
                uint32 tbp = 0;
                uint32 tbw = 0;

                if ( mipidx == 0 )
                {
                    tbp = tex0.textureBasePointer;
                    tbw = tex0.textureBufferWidth;
                }
                else if ( mipidx == 1 )
                {
                    tbp = miptbp1.textureBasePointer1;
                    tbw = miptbp1.textureBufferWidth1;
                }
                else if ( mipidx == 2 )
                {
                    tbp = miptbp1.textureBasePointer2;
                    tbw = miptbp1.textureBufferWidth2;
                }
                else if ( mipidx == 3 )
                {
                    tbp = miptbp1.textureBasePointer3;
                    tbw = miptbp1.textureBufferWidth3;
                }
                else if ( mipidx == 4 )
                {
                    tbp = miptbp2.textureBasePointer4;
                    tbw = miptbp2.textureBufferWidth4;
                }
                else if ( mipidx == 5 )
                {
                    tbp = miptbp2.textureBasePointer5;
                    tbw = miptbp2.textureBufferWidth5;
                }
                else if ( mipidx == 6 )
                {
                    tbp = miptbp2.textureBasePointer6;
                    tbw = miptbp2.textureBufferWidth6;
                }

                newMipmap.textureBasePointer = tbp;
                newMipmap.textureBufferWidth = tbw;

                mipidx++;
            }

            // Make sure that we have at least one texture.
            // Otherwise we cannot set the TEX0 register.
            if ( mipidx == 0 )
            {
                throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_EMPTY" );
            }

            // Check the number of actual mipmaps with the TEX0 register value.
            // If the values do not match then warn the user; we will fix the value
            // for the user.
            // * the real mipmap count is assumed to be the amount of IMAGE mode GS primitives
            // * kind of a fair assumption; might be wrong if meta-data GS primitives want to
            // * be supported after all IMAGE modes ones.
            if ( mipidx != regNumMipmaps )
            {
                engineInterface->PushWarningObject( theTexture, L"PS2_WARN_TEX0MAXMIPLEVELWRONG" );
            }

            if ( remainingImageData > 0 )
            {
                engineInterface->PushWarningObject( theTexture, L"PS2_WARN_IMGDATABYTESLEFT" );

                // Make sure we are past the image data.
                gsPacketBlock.skip( remainingImageData );
            }

            /* Palette */
            // vc dyn_trash.txd is weird here
            uint32 remainingPaletteData = textureMeta.gsCLUTDataSize;

            if ( platformTex->paletteType != ePaletteTypePS2::NONE )
            {
                // Get the actual pixel storage mode of the palette from the CLUT type.
                eMemoryLayoutType actualCLUTStorageFormat = GetCommonMemoryLayoutTypeFromCLUTLayoutType( clutStorageFmt );

                // Craft the palette texture.
                NativeTexturePS2::GSTexture& palTex = platformTex->paletteTex;

                // The dimensions of this texture depend on game version.
                getPaletteTextureDimensions( platformTex->paletteType, gsPacketBlock.getBlockVersion(), palTex.width, palTex.height );

                uint32 readCount;

                if ( hasHeader )
                {
                    // Read the GIF packet.
                    readCount = palTex.readGIFPacket( engineInterface, gsPacketBlock, theTexture );
                }
                else
                {
                    readCount = palTex.readRawTexels( engineInterface, actualCLUTStorageFormat, inputProvider );
                }

                if ( readCount > remainingPaletteData )
                {
                    throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_PAL_GIFPACKETOVERFLOW" );
                }

                remainingPaletteData -= readCount;

                // Verify that the palette transmission texture dimensions do match the dimensions that we expect.
                // If not then we have a big problem: there is code that expects the palette to have a fixed structure.
                if ( hasHeader )
                {
                    if ( palTex.transmissionAreaWidth != palTex.width || palTex.transmissionAreaHeight != palTex.height )
                    {
                        throw NativeTextureRwObjectsStructuralErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_STRUCTERR_INVGSPALDIMMS" );
                    }
                }

                // Make sure to take over register data.
                palTex.textureBasePointer = textureMeta.clutOffset;
            }

            if ( remainingPaletteData > 0 )
            {
                engineInterface->PushWarningObject( theTexture, L"PS2_WARN_PALDATABYTESLEFT" );

                // Make sure we are past the palette data.
                gsPacketBlock.skip( remainingPaletteData );
            }

#ifdef RWLIB_ENABLE_PS2GSTRACE
            _ps2gstrace_on_texture_deserialize( platformTex );
#endif //RWLIB_ENABLE_PS2GSTRACE

            // Allocate texture memory.
            // TODO: we want to take over all allocation data into internal structures and verify the allocation semantically
            // instead of by direct-equivalence of calculation; for that we have to put allocation nodes into GSTexture.
            uint32 mipmapBasePointer[ maxMipmaps ];
            uint32 mipmapBufferWidth[ maxMipmaps ];

            ps2MipmapTransmissionData mipmapTransData[ maxMipmaps ];

            uint32 clutBasePointer;
            ps2MipmapTransmissionData clutTransData;

            uint32 texMemSize;

            bool hasAllocatedMemory =
                platformTex->allocateTextureMemory(mipmapBasePointer, mipmapBufferWidth, mipmapTransData, maxMipmaps, clutBasePointer, clutTransData, texMemSize);

            // Could fail if no memory left.
            if ( !hasAllocatedMemory )
            {
                throw NativeTextureRwObjectsInternalErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_INTERNERR_TEXMEMALLOCFAIL" );
            }

            // Verify that our memory calculation routine is correct.
            if ( textureMeta.combinedGPUDataSize > texMemSize )
            {
                // If this assertion is triggered, then adjust the gpu size calculation algorithm
                // so it outputs a big enough number.
                engineInterface->PushWarningObject( theTexture, L"PS2_WARN_TOOSMALLGPUDATA" );

                // TODO: handle this as error?
            }
            else if ( textureMeta.combinedGPUDataSize != texMemSize )
            {
                // It would be perfect if this condition were never triggered for official R* games textures.
                engineInterface->PushWarningObject( theTexture, L"PS2_WARN_INVGPUDATASIZE" );
            }

            // Verify that our GPU data calculation routine is correct.
            ps2GSRegisters gpuData;

            bool isValidTexture = platformTex->generatePS2GPUData(gsPacketBlock.getBlockVersion(), gpuData, mipmapBasePointer, mipmapBufferWidth, maxMipmaps, basePixelStorageFormat, clutBasePointer);

            // If any of those assertions fail then either our is routine incomplete
            // or the input texture is invalid (created by wrong/buggy tool probably.)
            if ( !isValidTexture )
            {
                throw NativeTextureRwObjectsInternalErrorException( "PlayStation2", AcquireObject( theTexture ), L"PS2_INTERNERR_TEXFMTDETECTFAIL" );
            }

            if ( gpuData.tex0 != tex0 )
            {
                if ( engineWarningLevel >= 3 )
                {
                    engineInterface->PushWarningObject( theTexture, L"PS2_WARN_INVREG_TEX0" );
                }
            }
            if ( gpuData.tex1_low != tex1_low )
            {
                if ( engineWarningLevel >= 2 )
                {
                    engineInterface->PushWarningObject( theTexture, L"PS2_WARN_INVREG_TEX1" );
                }
            }
            if ( gpuData.miptbp1 != miptbp1 )
            {
                if ( engineWarningLevel >= 1 )
                {
                    engineInterface->PushWarningObject( theTexture, L"PS2_WARN_INVREG_MIPTBP1" );
                }
            }
            if ( gpuData.miptbp2 != miptbp2 )
            {
                if ( engineWarningLevel >= 1 )
                {
                    engineInterface->PushWarningObject( theTexture, L"PS2_WARN_INVREG_MIPTBP2" );
                }
            }

            // Verify transmission rectangle same-ness.
            if ( hasHeader )
            {
                bool hasValidTransmissionRects = true;

                size_t mipmapCount = platformTex->mipmaps.GetCount();

                for ( size_t n = 0; n < mipmapCount; n++ )
                {
                    const NativeTexturePS2::GSTexture& srcMipmap = platformTex->mipmaps[ n ];
                    const ps2MipmapTransmissionData& dstTransData = mipmapTransData[ n ];

                    if ( srcMipmap.transmissionOffsetX != dstTransData.destX ||
                         srcMipmap.transmissionOffsetY != dstTransData.destY )
                    {
                        hasValidTransmissionRects = false;
                        break;
                    }
                }

                if ( hasValidTransmissionRects == false )
                {
                    engineInterface->PushWarningObject( theTexture, L"PS2_WARN_INVMIPTRANSOFFSETS" );
                }
            }

            // Verify palette transmission rectangle.
            if (hasHeader && platformTex->paletteType != ePaletteTypePS2::NONE)
            {
                if ( clutTransData.destX != platformTex->paletteTex.transmissionOffsetX ||
                     clutTransData.destY != platformTex->paletteTex.transmissionOffsetY )
                {
                    engineInterface->PushWarningObject( theTexture, L"PS2_WARN_INVCLUTTRANSOFFSET" );
                }
            }

            // Fix filtering mode.
            fixFilteringMode( *theTexture, mipidx );
        }

        // Done reading native block.
    }

    // Deserialize extensions aswell.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

static optional_struct_space <PluginDependantStructRegister <ps2NativeTextureTypeProvider, RwInterfaceFactory_t>> ps2NativeTexturePlugin;

void registerPS2NativePlugin( void )
{
    ps2NativeTexturePlugin.Construct( engineFactory );
}

void unregisterPS2NativePlugin( void )
{
#ifdef RWLIB_ENABLE_PS2GSTRACE
    _ps2gstrace_finalize_on_exit();
#endif //RWLIB_ENABLE_PS2GSTRACE

    ps2NativeTexturePlugin.Destroy();
}

inline void* TruncateMipmapLayerPS2(
    Interface *engineInterface,
    const void *srcTexels, uint32 srcMipWidth, uint32 srcMipHeight, uint32 srcDepth, uint32 srcRowAlignment,
    uint32 dstMipWidth, uint32 dstMipHeight, uint32 dstRowAlignment,
    uint32& dstDataSizeOut
)
{
    // Allocate a new layer.
    rasterRowSize dstRowSize = getRasterDataRowSize( dstMipWidth, srcDepth, dstRowAlignment );

    uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, dstMipHeight );

    void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

    try
    {
        // Perform the truncation.
        // We want to fill the entire destination buffer with data, but only fill it with source pixels if they exist.
        // The other texels are cleared.
        rasterRowSize srcRowSize = getRasterDataRowSize( srcMipWidth, srcDepth, srcRowAlignment );

        for ( uint32 row = 0; row < dstMipHeight; row++ )
        {
            constRasterRow srcRow;
            rasterRow dstRow = getTexelDataRow( dstTexels, dstRowSize, row );

            if ( row < srcMipHeight )
            {
                srcRow = getConstTexelDataRow( srcTexels, srcRowSize, row );
            }

            for ( uint32 col = 0; col < dstMipWidth; col++ )
            {
                // If we have a row, we fetch a color item and apply it.
                // Otherwise we just clear the coordinate.
                if ( srcRow && col < srcMipWidth )
                {
                    dstRow.writeBitsFromRow( srcRow, (size_t)col * srcDepth, (size_t)col * srcDepth, srcDepth );
                }
                else
                {
                    dstRow.setBits( false, (size_t)col * srcDepth, srcDepth );
                }
            }
        }
    }
    catch( ... )
    {
        engineInterface->PixelFree( dstTexels );

        throw;
    }

    // Return the data size aswell.
    dstDataSizeOut = dstDataSize;

    return dstTexels;
}

inline void GetPS2TextureTranscodedMipmapData(
    Interface *engineInterface,
    uint32 layerWidth, uint32 layerHeight, uint32 swizzleWidth, uint32 swizzleHeight, const void *srcTexels, uint32 srcTexDataSize,
    eMemoryLayoutType rawLayoutType, eMemoryLayoutType bitbltLayoutType,
    eRasterFormat srcRasterFormat, uint32 srcDepth, eColorOrdering srcColorOrder,
    eRasterFormat dstRasterFormat, uint32 dstDepth, eColorOrdering dstColorOrder,
    ePaletteType paletteType, uint32 paletteSize,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    void *texelData = nullptr;
    uint32 dstDataSize = 0;

    bool doesSourceNeedDeletion = false;

    uint32 srcRowAlignment;
    uint32 dstRowAlignment = getPS2ExportTextureDataRowAlignment(); // it _must_ be this.

    uint32 srcLayerWidth = swizzleWidth;
    uint32 srcLayerHeight = swizzleHeight;

    bool hasToTranscode = ( rawLayoutType != bitbltLayoutType );

    // Take care about a stable source texel buffer.
    if ( hasToTranscode )
    {
        uint32 newDataSize;

        void *newTexels =
            permutePS2Data(
                engineInterface,
                srcLayerWidth, srcLayerHeight, srcTexels, srcTexDataSize,
                layerWidth, layerHeight,
                bitbltLayoutType, rawLayoutType,
                getPS2TextureDataRowAlignment(),
                dstRowAlignment, newDataSize
            );

        if ( newTexels == nullptr )
        {
            throw NativeTextureInternalErrorException( "PlayStation2", L"PS2_INTERNERR_UNSWIZZLEFAIL" );
        }

        srcLayerWidth = layerWidth;
        srcLayerHeight = layerHeight;

        // Now we have the swizzled data taken care of.
        srcTexels = newTexels;
        srcTexDataSize = newDataSize;

        doesSourceNeedDeletion = true;

        // The source texels are always permuted.
        srcRowAlignment = dstRowAlignment;
    }
    else
    {
        // If the encoded texture has a bigger buffer size than the raw format should have,
        // we actually must trimm it!
        if ( swizzleWidth != layerWidth || swizzleHeight != layerHeight )
        {
            srcRowAlignment = getPS2ExportTextureDataRowAlignment();

            srcTexels = TruncateMipmapLayerPS2(
                engineInterface,
                srcTexels,
                srcLayerWidth, srcLayerHeight, srcDepth, getPS2TextureDataRowAlignment(),
                layerWidth, layerHeight, srcRowAlignment, srcTexDataSize
            );

            if ( srcTexels == nullptr )
            {
                throw NativeTextureInternalErrorException( "PlayStation2", L"PS2_INTERNERR_TRUNCATEFAIL" );
            }

            srcLayerWidth = layerWidth;
            srcLayerHeight = layerHeight;

            doesSourceNeedDeletion = true;
        }
        else
        {
            srcRowAlignment = getPS2TextureDataRowAlignment();  // crossing my fingers here!
        }
    }

    try
    {
        // Cache important values.
        bool isConversionComplyingItemSwap = !hasConflictingAddressing( srcLayerWidth, srcDepth, srcRowAlignment, paletteType, dstDepth, dstRowAlignment, paletteType );

        if ( hasToTranscode )
        {
            if ( isConversionComplyingItemSwap )
            {
                texelData = (void*)srcTexels;   // safe cast, because srcTexels is mutable buffer.
                dstDataSize = srcTexDataSize;

                // We want to operate the conversion on ourselves.
                // Also, the source texel buffer will just be taken.
                doesSourceNeedDeletion = false;
            }
            else
            {
                rasterRowSize dstRowSize = getRasterDataRowSize( srcLayerWidth, dstDepth, dstRowAlignment );

                dstDataSize = getRasterDataSizeByRowSize( dstRowSize, srcLayerHeight );

                texelData = engineInterface->PixelAllocate( dstDataSize );

                // We need to transcode into a bigger array.
            }
        }
        else
        {
            if ( isConversionComplyingItemSwap )
            {
                // At best, we simply want to copy the texels.
                dstDataSize = srcTexDataSize;
            }
            else
            {
                rasterRowSize dstRowSize = getRasterDataRowSize( layerWidth, dstDepth, dstRowAlignment );

                dstDataSize = getRasterDataSizeByRowSize( dstRowSize, layerHeight );
            }

            texelData = engineInterface->PixelAllocate( dstDataSize );
        }

        // Now that the texture is in linear format, we can prepare it.
        bool fixAlpha = false;

        // TODO: do we have to fix alpha for 16bit raster depths?

        if (srcRasterFormat == RASTER_8888)
        {
            fixAlpha = true;
        }

        // Prepare colors.
        if (paletteType == PALETTE_NONE)
        {
            convertTexelsFromPS2(
                srcTexels, texelData, layerWidth, layerHeight, srcTexDataSize,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder,
                fixAlpha
            );
        }
        else
        {
            if ( texelData != srcTexels )
            {
                if ( isConversionComplyingItemSwap )
                {
                    memcpy( texelData, srcTexels, std::min(dstDataSize, srcTexDataSize) );
                }
                else
                {
                    // We need to convert the palette indice into another bit depth.
                    ConvertPaletteDepth(
                        srcTexels, texelData,
                        layerWidth, layerHeight,
                        paletteType, paletteType, paletteSize,
                        srcDepth, dstDepth,
                        srcRowAlignment, dstRowAlignment
                    );
                }
            }
        }
    }
    catch( ... )
    {
        // Clean up memory, because we failed for some reason.
        if ( texelData != nullptr && texelData != srcTexels )
        {
            engineInterface->PixelFree( texelData );
        }

        if ( doesSourceNeedDeletion )
        {
            engineInterface->PixelFree( (void*)srcTexels );
        }

        throw;
    }

    // Make sure we delete temporary texel data.
    if ( doesSourceNeedDeletion )
    {
        engineInterface->PixelFree( (void*)srcTexels );
    }

    // Return stuff to the runtime.
    dstTexelsOut = texelData;
    dstDataSizeOut = dstDataSize;
}

void ps2NativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    // Cast to our native platform texture.
    NativeTexturePS2 *platformTex = (NativeTexturePS2*)objMem;

    size_t mipmapCount = platformTex->mipmaps.GetCount();

    eRasterFormatPS2 rasterFormat = platformTex->rasterFormat;
    ePaletteTypePS2 paletteType = platformTex->paletteType;

    eRasterFormat frm_rasterFormat = GetGenericRasterFormatFromPS2( rasterFormat );
    ePaletteType frm_paletteType = GetGenericPaletteTypeFromPS2( paletteType );

    // Copy over general attributes.
    uint32 depth = platformTex->depth;

    // Fix wrong auto mipmap property.
    bool hasAutoMipmaps = false;

    if ( mipmapCount == 1 )
    {
        // Direct3D textures can only have mipmaps if they dont come with mipmaps.
        hasAutoMipmaps = platformTex->autoMipmaps;
    }

    pixelsOut.autoMipmaps = hasAutoMipmaps;
    pixelsOut.rasterType = platformTex->rasterType;

    pixelsOut.cubeTexture = false;

    // We will have to swap colors.
    eColorOrdering ps2ColorOrder = COLOR_RGBA;
    eColorOrdering d3dColorOrder = COLOR_BGRA;

    // First we want to decode the CLUT.
    void *palTexels = nullptr;
    uint32 palSize = 0;

    if (paletteType != ePaletteTypePS2::NONE)
    {
        // We need to decode the PS2 CLUT.
        GetPS2TexturePalette(
            engineInterface,
            platformTex->paletteTex.transmissionAreaWidth, platformTex->paletteTex.transmissionAreaHeight, platformTex->clutStorageFmt, platformTex->paletteTex.texels,
            frm_rasterFormat, ps2ColorOrder,
            frm_rasterFormat, d3dColorOrder,
            frm_paletteType,
            palTexels, palSize
        );
    }

    // Process the mipmaps.
    if ( mipmapCount != 0 )
    {
        pixelsOut.mipmaps.Resize( mipmapCount );

        eMemoryLayoutType rawMemLayout = platformTex->pixelStorageMode;
        eMemoryLayoutType bitblt_memLayout = platformTex->bitblt_targetFmt;

        for (size_t j = 0; j < mipmapCount; j++)
        {
            NativeTexturePS2::GSTexture& gsTex = platformTex->mipmaps[ j ];

            // We have to create a new texture buffer that is unswizzled to linear format.
            uint32 layerWidth = gsTex.width;
            uint32 layerHeight = gsTex.height;

            const void *srcTexels = gsTex.texels;
            uint32 texDataSize = gsTex.dataSize;

            void *dstTexels = nullptr;
            uint32 dstDataSize = 0;

            GetPS2TextureTranscodedMipmapData(
                engineInterface,
                layerWidth, layerHeight, gsTex.transmissionAreaWidth, gsTex.transmissionAreaHeight, srcTexels, texDataSize,
                rawMemLayout, bitblt_memLayout,
                frm_rasterFormat, depth, ps2ColorOrder,
                frm_rasterFormat, depth, d3dColorOrder,
                frm_paletteType, palSize,
                dstTexels, dstDataSize
            );

            // Move over the texture data to pixel storage.
            pixelDataTraversal::mipmapResource newLayer;

            newLayer.width = layerWidth;
            newLayer.height = layerHeight;
            newLayer.layerWidth = layerWidth;   // layer dimensions.
            newLayer.layerHeight = layerHeight;

            newLayer.texels = dstTexels;
            newLayer.dataSize = dstDataSize;

            // Store the layer.
            pixelsOut.mipmaps[ j ] = newLayer;
        }
    }

    // Set up general raster attributes.
    pixelsOut.rasterFormat = frm_rasterFormat;
    pixelsOut.colorOrder = d3dColorOrder;
    pixelsOut.depth = depth;
    pixelsOut.rowAlignment = getPS2ExportTextureDataRowAlignment();

    // Copy over more advanced attributes.
    pixelsOut.paletteData = palTexels;
    pixelsOut.paletteSize = palSize;
    pixelsOut.paletteType = frm_paletteType;

    // We are an uncompressed raster.
    pixelsOut.compressionType = RWCOMPRESS_NONE;

    // Actually, since there is no alpha flag in PS2 textures, we should recalculate the alpha flag here.
    pixelsOut.hasAlpha = calculateHasAlpha( engineInterface, pixelsOut );

    // For now, we will always allocate new pixels due to the complexity of the encoding.
    pixelsOut.isNewlyAllocated = true;
}

inline void ConvertMipmapToPS2Format(
    Interface *engineInterface,
    uint32 mipWidth, uint32 mipHeight, const void *srcTexelData, uint32 srcDataSize,
    eMemoryLayoutType rawLayoutType, eMemoryLayoutType bitbltLayoutType,
    eRasterFormat srcRasterFormat, uint32 srcItemDepth, eColorOrdering srcColorOrder,
    eRasterFormat dstRasterFormat, uint32 dstItemDepth, eColorOrdering dstColorOrder,
    ePaletteType srcPaletteType, ePaletteType dstPaletteType, uint32 paletteSize,
    uint32 srcRowAlignment,
    uint32& swizzleWidthOut, uint32& swizzleHeightOut,
    void*& dstSwizzledTexelsOut, uint32& dstSwizzledDataSizeOut
)
{
    // We need to convert the texels before storing them in the PS2 texture.
    bool fixAlpha = false;

    // TODO: do we have to fix alpha for 16bit rasters?

    if (dstRasterFormat == RASTER_8888)
    {
        fixAlpha = true;
    }

    // TODO: optimize for the situation where we do not need to allocate a new texel buffer but
    // use the source texel buffer directly.

    bool hasToTranscode = ( rawLayoutType != bitbltLayoutType );

    // Allocate a new copy of the texel data.
    uint32 swizzledRowAlignment = getPS2TextureDataRowAlignment();

    rasterRowSize dstLinearRowSize = getRasterDataRowSize( mipWidth, dstItemDepth, swizzledRowAlignment );

    uint32 dstLinearDataSize = getRasterDataSizeByRowSize( dstLinearRowSize, mipHeight );

    void *dstLinearTexelData = engineInterface->PixelAllocate( dstLinearDataSize );

    // Swizzle the mipmap.
    // We need to store dimensions into the texture of the current encoding.
    uint32 packedWidth, packedHeight;
    {
        ps2MemoryLayoutTypeProperties rawMemProps = getMemoryLayoutTypeProperties( rawLayoutType );
        ps2MemoryLayoutTypeProperties encMemProps = getMemoryLayoutTypeProperties( bitbltLayoutType );

        uint32 colsWidth = CEIL_DIV( mipWidth, rawMemProps.pixelWidthPerColumn );
        uint32 colsHeight = CEIL_DIV( mipHeight, rawMemProps.pixelHeightPerColumn );

        packedWidth = ( colsWidth * encMemProps.pixelWidthPerColumn );
        packedHeight = ( colsHeight * encMemProps.pixelHeightPerColumn );
    }

    void *dstSwizzledTexelData = nullptr;
    uint32 dstSwizzledDataSize = 0;

    try
    {
        // Convert the texels.
        if (srcPaletteType == PALETTE_NONE)
        {
            convertTexelsToPS2(
                srcTexelData, dstLinearTexelData, mipWidth, mipHeight, srcDataSize,
                srcRasterFormat, dstRasterFormat,
                srcItemDepth, srcRowAlignment, dstItemDepth, swizzledRowAlignment,
                srcColorOrder, dstColorOrder,
                fixAlpha
            );
        }
        else
        {
            // Maybe we need to fix the indice (if the texture comes from PC or XBOX architecture).
            ConvertPaletteDepth(
                srcTexelData, dstLinearTexelData,
                mipWidth, mipHeight,
                srcPaletteType, dstPaletteType, paletteSize,
                srcItemDepth, dstItemDepth,
                srcRowAlignment, swizzledRowAlignment
            );
        }

        // Perform swizzling.
        if ( hasToTranscode )
        {
            dstSwizzledTexelData =
                permutePS2Data(
                    engineInterface,
                    mipWidth, mipHeight, srcTexelData, srcDataSize,
                    packedWidth, packedHeight,
                    rawLayoutType, bitbltLayoutType,
                    srcRowAlignment, swizzledRowAlignment,
                    dstSwizzledDataSize
                );

            if ( dstSwizzledTexelData == nullptr )
            {
                // The probability of this failing is medium.
                throw NativeTextureInternalErrorException( "PlayStation2", L"PS2_INTERNERR_SWIZZLEFAIL" );
            }
        }
        else
        {
            // We have to make sure that we extend the texture dimensions properly!
            // The texture data _must_ be in memory layout, after all!
            if ( mipWidth != packedWidth || mipHeight != packedHeight )
            {
                rasterRowSize dstSwizzledRowSize = getPS2RasterDataRowSize( packedWidth, dstItemDepth );

                dstSwizzledDataSize = getRasterDataSizeByRowSize( dstSwizzledRowSize, packedHeight );

                dstSwizzledTexelData = TruncateMipmapLayerPS2(
                    engineInterface,
                    dstLinearTexelData,
                    mipWidth, mipHeight, dstItemDepth, swizzledRowAlignment,
                    packedWidth, packedHeight, swizzledRowAlignment,
                    dstSwizzledDataSize
                );
            }
            else
            {
                // We are properly sized and optimized, so just take us.
                dstSwizzledTexelData = dstLinearTexelData;
                dstSwizzledDataSize = dstLinearDataSize;
            }
        }

        // Free temporary unswizzled texels.
        if ( dstSwizzledTexelData != dstLinearTexelData )
        {
            engineInterface->PixelFree( dstLinearTexelData );
        }
    }
    catch( ... )
    {
        engineInterface->PixelFree( dstLinearTexelData );

        if ( dstSwizzledTexelData && dstSwizzledTexelData != dstLinearTexelData )
        {
            engineInterface->PixelFree( dstSwizzledTexelData );
        }

        throw;
    }

    // Give results to the runtime.
    swizzleWidthOut = packedWidth;
    swizzleHeightOut = packedHeight;

    dstSwizzledTexelsOut = dstSwizzledTexelData;
    dstSwizzledDataSizeOut = dstSwizzledDataSize;
}

AINLINE void MapCompatiblePS2MemoryLayoutFromRasterFormat(
    eRasterFormat fmt, ePaletteType palType, eColorOrdering colorOrder,
    eMemoryLayoutType& memLayoutOut, eCLUTMemoryLayoutType& clutMemLayoutOut,
    bool& isDirectFormatMappingOut
)
{
    // To check whether depth matches get it from the GIF packing depth.
    // isDirectFormatMappingOut does not specify direct acquisition by itself; check depth too!

    bool isPalFmt = false;

    // Check for any (direct) mapping that makes sense.
    if ( palType == PALETTE_4BIT || palType == PALETTE_4BIT_LSB ||
         palType == PALETTE_8BIT )
    {
        bool isPalDirectAcq;

        if ( palType == PALETTE_4BIT )
        {
            memLayoutOut = PSMT4;
            isPalDirectAcq = true;
        }
        else if ( palType == PALETTE_4BIT_LSB )
        {
            memLayoutOut = PSMT4;
            isPalDirectAcq = false;
        }
        else if ( palType == PALETTE_8BIT )
        {
            memLayoutOut = PSMT8;
            isPalDirectAcq = true;
        }
        else
        {
            throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
        }

        isPalFmt = true;

        bool isFormatDirectAcq;

        if ( fmt == RASTER_1555 )
        {
            clutMemLayoutOut = eCLUTMemoryLayoutType::PSMCT16S;
            isFormatDirectAcq = ( colorOrder == COLOR_RGBA );
        }
        else if ( fmt == RASTER_555 )
        {
            clutMemLayoutOut = eCLUTMemoryLayoutType::PSMCT16S;
            isFormatDirectAcq = false;
        }
        else if ( fmt == RASTER_8888 )
        {
            clutMemLayoutOut = eCLUTMemoryLayoutType::PSMCT32;
            isFormatDirectAcq = ( colorOrder == COLOR_RGBA );
        }
        else
        {
            clutMemLayoutOut = eCLUTMemoryLayoutType::PSMCT32;
            isFormatDirectAcq = false;
        }

        isDirectFormatMappingOut = ( isPalDirectAcq && isFormatDirectAcq );
    }
    else if ( fmt == RASTER_1555 )
    {
        memLayoutOut = PSMCT16S;
        isDirectFormatMappingOut = ( colorOrder == COLOR_RGBA );
    }
    else if ( fmt == RASTER_565 )
    {
        memLayoutOut = PSMCT16S;
        isDirectFormatMappingOut = false;
    }
    else if ( fmt == RASTER_8888 )
    {
        memLayoutOut = PSMCT32;
        isDirectFormatMappingOut = ( colorOrder == COLOR_RGBA );
    }
    else if ( fmt == RASTER_888 )
    {
        memLayoutOut = PSMCT24;
        isDirectFormatMappingOut = ( colorOrder == COLOR_RGBA );
    }
    else if ( fmt == RASTER_16 )
    {
        memLayoutOut = PSMZ16;
        isDirectFormatMappingOut = true;
    }
    else if ( fmt == RASTER_24 )
    {
        memLayoutOut = PSMZ24;
        isDirectFormatMappingOut = true;
    }
    else if ( fmt == RASTER_32 )
    {
        memLayoutOut = PSMZ32;
        isDirectFormatMappingOut = true;
    }
    else if ( fmt == RASTER_555 )
    {
        memLayoutOut = PSMCT16S;
        isDirectFormatMappingOut = false;
    }
    else
    {
        memLayoutOut = PSMCT32;
        isDirectFormatMappingOut = false;
    }

    if ( isPalFmt == false )
    {
        clutMemLayoutOut = eCLUTMemoryLayoutType::PSMCT32;
    }
}

void ps2NativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    // Cast to our native format.
    NativeTexturePS2 *ps2tex = (NativeTexturePS2*)objMem;

    // Verify mipmap dimensions.
    {
        nativeTextureSizeRules sizeRules;
        getPS2NativeTextureSizeRules( sizeRules );

        if ( !sizeRules.verifyPixelData( pixelsIn ) )
        {
            throw NativeTextureInvalidConfigurationException( "PlayStation2", L"PS2_INVALIDCFG_MIPDIMMS" );
        }
    }

    LibraryVersion currentVersion = ps2tex->texVersion;

    // Make sure that we got uncompressed bitmap data.
    assert( pixelsIn.compressionType == RWCOMPRESS_NONE );

    // The maximum amount of mipmaps supported by PS2 textures.
    static constexpr size_t maxMipmaps = 7;

    {
        // The PlayStation 2 does NOT support all raster formats.
        // We need to avoid giving it raster formats that are prone to crashes, like RASTER_888.
        eRasterFormat srcRasterFormat = pixelsIn.rasterFormat;
        uint32 srcItemDepth = pixelsIn.depth;
        uint32 srcRowAlignment = pixelsIn.rowAlignment;

        ePaletteType paletteType = pixelsIn.paletteType;
        uint32 paletteSize = pixelsIn.paletteSize;

        eColorOrdering d3dColorOrder = pixelsIn.colorOrder;

        // Decide about a target mapping.
        eMemoryLayoutType pixelMemLayoutType;
        eCLUTMemoryLayoutType clutMemLayoutType;
        bool isFormatDirectMapping;

        MapCompatiblePS2MemoryLayoutFromRasterFormat( srcRasterFormat, paletteType, d3dColorOrder, pixelMemLayoutType, clutMemLayoutType, isFormatDirectMapping );

        // Get the target depth.
        uint32 dstItemDepth = GetMemoryLayoutTypeGIFPackingDepth( pixelMemLayoutType );

        // Get the raster format that we target to.
        eRasterFormatPS2 targetRasterFormat;
        ePaletteTypePS2 targetPaletteType;

        GetPS2RasterFormatFromMemoryLayoutType( pixelMemLayoutType, clutMemLayoutType, targetRasterFormat, targetPaletteType );

        // Set the palette type.
        ps2tex->paletteType = targetPaletteType;

        // Finally, set the raster format.
        ps2tex->rasterFormat = targetRasterFormat;

        // Store the encoding information.
        ps2tex->pixelStorageMode = pixelMemLayoutType;
        ps2tex->clutStorageFmt = clutMemLayoutType;
        ps2tex->clutEntryOffset = 0;    // gets set later.
        ps2tex->clutLoadControl = ( targetPaletteType != ePaletteTypePS2::NONE ? 1 : 0 );
        ps2tex->clutStorageMode = eCLUTStorageMode::CSM1;

        ps2tex->texColorComponent =
            ( pixelsIn.hasAlpha ?
              eTextureColorComponent::RGBA : eTextureColorComponent::RGB );

        ps2tex->textureFunction = eTextureFunction::MODULATE;
        ps2tex->lodCalculationModel = 0;

        ps2tex->clutEntryOffset = 0;
        ps2tex->clutOffset = 0; // we will calculate this later if we get to keeping allocation data.
        ps2tex->texMemSize = 0;

        eMemoryLayoutType bitblt_targetFmt = GetBITBLTMemoryLayoutType( pixelMemLayoutType, currentVersion );

        ps2tex->bitblt_targetFmt = bitblt_targetFmt;

        // Prepare mipmap data.
        eColorOrdering ps2ColorOrder = COLOR_RGBA;

        eRasterFormat frm_targetRasterFormat = GetGenericRasterFormatFromPS2( targetRasterFormat );
        ePaletteType frm_targetPaletteType = GetGenericPaletteTypeFromPS2( targetPaletteType );

        // Prepare swizzling parameters.
        size_t mipmapCount = pixelsIn.mipmaps.GetCount();
        {
            size_t mipProcessCount = std::min( maxMipmaps, mipmapCount );

            ps2tex->mipmaps.Resize( mipProcessCount );

            for ( size_t n = 0; n < mipProcessCount; n++ )
            {
                // Process every mipmap individually.
                NativeTexturePS2::GSTexture& newMipmap = ps2tex->mipmaps[ n ];

                const pixelDataTraversal::mipmapResource& oldMipmap = pixelsIn.mipmaps[ n ];

                uint32 layerWidth = oldMipmap.width;
                uint32 layerHeight = oldMipmap.height;
                uint32 srcDataSize = oldMipmap.dataSize;

                const void *srcTexelData = oldMipmap.texels;

                // Convert the mipmap and fetch new parameters.
                uint32 packedWidth, packedHeight;

                void *dstSwizzledTexelData;
                uint32 dstSwizzledDataSize;

                ConvertMipmapToPS2Format(
                    engineInterface,
                    layerWidth, layerHeight, srcTexelData, srcDataSize,
                    pixelMemLayoutType, bitblt_targetFmt,
                    srcRasterFormat, srcItemDepth, d3dColorOrder,
                    frm_targetRasterFormat, dstItemDepth, ps2ColorOrder,
                    paletteType, frm_targetPaletteType, paletteSize,
                    srcRowAlignment,
                    packedWidth, packedHeight,
                    dstSwizzledTexelData, dstSwizzledDataSize
                );

                // Store the new mipmap.
                newMipmap.width = layerWidth;
                newMipmap.height = layerHeight;

                newMipmap.transmissionAreaWidth = packedWidth;
                newMipmap.transmissionAreaHeight = packedHeight;

                newMipmap.texels = dstSwizzledTexelData;
                newMipmap.dataSize = dstSwizzledDataSize;
            }
        }

        // Copy over general attributes.
        ps2tex->depth = dstItemDepth;

        // Make sure we apply autoMipmap property just like the R* converter.
        bool hasAutoMipmaps = pixelsIn.autoMipmaps;

        if ( mipmapCount > 1 )
        {
            hasAutoMipmaps = true;
        }

        ps2tex->autoMipmaps = hasAutoMipmaps;
        ps2tex->rasterType = pixelsIn.rasterType;

        // Move over the palette texels.
        if ( targetPaletteType != ePaletteTypePS2::NONE )
        {
            // Prepare the palette texels.
            const void *srcPalTexelData = pixelsIn.paletteData;

            uint32 srcPalFormatDepth = Bitmap::getRasterFormatDepth( srcRasterFormat );
            uint32 targetPalFormatDepth = Bitmap::getRasterFormatDepth( frm_targetRasterFormat );

            // Swizzle the CLUT.
            uint32 palWidth, palHeight;
            getPaletteTextureDimensions( targetPaletteType, currentVersion, palWidth, palHeight );

            void *clutSwizzledTexels = nullptr;
            uint32 newPalDataSize = 0;

            GeneratePS2CLUT(
                engineInterface, palWidth, palHeight,
                srcPalTexelData, frm_targetPaletteType, paletteSize, clutMemLayoutType,
                srcRasterFormat, srcPalFormatDepth, d3dColorOrder,
                frm_targetRasterFormat, targetPalFormatDepth, ps2ColorOrder,
                clutSwizzledTexels, newPalDataSize
            );

            // Store the palette texture.
            NativeTexturePS2::GSTexture& palTex = ps2tex->paletteTex;

            palTex.width = palWidth;
            palTex.height = palHeight;
            palTex.transmissionAreaWidth = palWidth;
            palTex.transmissionAreaHeight = palHeight;
            palTex.dataSize = newPalDataSize;

            palTex.texels = clutSwizzledTexels;
        }
    }

    // TODO: improve exception safety.

    // We do not take the pixels directly, because we need to encode them.
    feedbackOut.hasDirectlyAcquired = false;
}

void ps2NativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    if ( deallocate )
    {
        size_t mipmapCount = nativeTex->mipmaps.GetCount();

        // Free all mipmaps.
        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            NativeTexturePS2::GSTexture& mipLayer = nativeTex->mipmaps[ n ];

            mipLayer.Cleanup( engineInterface );
        }

        // Free the palette texture.
        nativeTex->paletteTex.Cleanup( engineInterface );
    }

    // Clear our mipmaps.
    nativeTex->mipmaps.Clear();

    // Clear the palette texture.
    nativeTex->paletteTex.DetachTexels();

    // For debugging purposes, reset the texture raster information.
    nativeTex->rasterFormat = eRasterFormatPS2::RASTER_DEFAULT;
    nativeTex->depth = 0;
    nativeTex->paletteType = ePaletteTypePS2::NONE;
    nativeTex->recommendedBufferBasePointer = 0;
    nativeTex->autoMipmaps = false;
    nativeTex->rasterType = 4;
    nativeTex->pixelStorageMode = PSMCT32;
    nativeTex->clutStorageFmt = eCLUTMemoryLayoutType::PSMCT32;
    nativeTex->clutEntryOffset = 0;
    nativeTex->clutStorageMode = eCLUTStorageMode::CSM1;
    nativeTex->clutLoadControl = 0;
    nativeTex->bitblt_targetFmt = PSMCT32;
    nativeTex->texColorComponent = eTextureColorComponent::RGB;
}

template <bool isConst>
struct ps2MipmapManager
{
    typedef typename std::conditional <isConst, const NativeTexturePS2, NativeTexturePS2>::type nativeType;

    nativeType *nativeTex;

    inline ps2MipmapManager( nativeType *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTexturePS2::GSTexture& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.width;
        layerHeight = mipLayer.height;
    }

    inline void GetSizeRules( nativeTextureSizeRules& rulesOut ) const
    {
        getPS2NativeTextureSizeRules( rulesOut );
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTexturePS2::GSTexture& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        uint32& dstRowAlignment,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocatedOut
    )
    {
        // We need to decode our mipmap layer.
        uint32 layerWidth = mipLayer.width;
        uint32 layerHeight = mipLayer.height;

        void *srcTexels = mipLayer.texels;
        uint32 dataSize = mipLayer.dataSize;

        eRasterFormat srcRasterFormat = GetGenericRasterFormatFromPS2( nativeTex->rasterFormat );
        uint32 srcDepth = nativeTex->depth;
        eColorOrdering srcColorOrder = COLOR_RGBA;

        ePaletteType srcPaletteType = GetGenericPaletteTypeFromPS2( nativeTex->paletteType );

        // Get the decoded palette data.
        void *decodedPaletteData = nullptr;
        uint32 decodedPaletteSize = 0;

        if (srcPaletteType != PALETTE_NONE)
        {
            // We need to decode the PS2 CLUT.
            GetPS2TexturePalette(
                engineInterface,
                nativeTex->paletteTex.transmissionAreaWidth, nativeTex->paletteTex.transmissionAreaHeight, nativeTex->clutStorageFmt, nativeTex->paletteTex.texels,
                srcRasterFormat, srcColorOrder,
                srcRasterFormat, srcColorOrder,
                srcPaletteType,
                decodedPaletteData, decodedPaletteSize
            );
        }

        // Get the unswizzled texel data.
        void *dstTexels = nullptr;
        uint32 dstDataSize = 0;

        GetPS2TextureTranscodedMipmapData(
            engineInterface,
            layerWidth, layerHeight, mipLayer.transmissionAreaWidth, mipLayer.transmissionAreaHeight, srcTexels, dataSize,
            nativeTex->pixelStorageMode, nativeTex->bitblt_targetFmt,
            srcRasterFormat, srcDepth, srcColorOrder,
            srcRasterFormat, srcDepth, srcColorOrder,
            srcPaletteType, decodedPaletteSize,
            dstTexels, dstDataSize
        );

        // Return parameters to the runtime.
        widthOut = layerWidth;
        heightOut = layerHeight;

        layerWidthOut = layerWidth;
        layerHeightOut = layerHeight;

        dstRasterFormat = srcRasterFormat;
        dstDepth = srcDepth;
        dstRowAlignment = getPS2ExportTextureDataRowAlignment();
        dstColorOrder = srcColorOrder;

        dstPaletteType = srcPaletteType;
        dstPaletteData = decodedPaletteData;
        dstPaletteSize = decodedPaletteSize;

        dstCompressionType = RWCOMPRESS_NONE;

        // Since the PS2 native texture does not care about the alpha status,
        // we have to always calculate this field, because the virtual framework _does_ care.
        hasAlpha =
            rawMipmapCalculateHasAlpha(
                engineInterface,
                layerWidth, layerHeight, dstTexels, dstDataSize,
                srcRasterFormat, srcDepth, getPS2ExportTextureDataRowAlignment(), srcColorOrder,
                srcPaletteType, decodedPaletteData, decodedPaletteSize
            );

        dstTexelsOut = dstTexels;
        dstDataSizeOut = dstDataSize;

        // We have newly allocated both texels and palette data.
        isNewlyAllocatedOut = true;
        isPaletteNewlyAllocatedOut = true;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTexturePS2::GSTexture& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        uint32 rowAlignment,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        // Check whether we have reached the maximum mipmap count.
        static constexpr uint32 maxMipmaps = 7;

        if ( nativeTex->mipmaps.GetCount() >= maxMipmaps )
        {
            throw NativeTextureInvalidConfigurationException( "PlayStation2", L"PS2_INVALIDCFG_TOOMANYMIPMAPS" );
        }

        // Get the texture properties on the stack.
        eRasterFormat texRasterFormat = GetGenericRasterFormatFromPS2( nativeTex->rasterFormat );
        uint32 texDepth = nativeTex->depth;
        eColorOrdering texColorOrder = COLOR_RGBA;

        ePaletteType texPaletteType = GetGenericPaletteTypeFromPS2( nativeTex->paletteType );

        // If we are a palette texture, decode our palette for remapping.
        void *texPaletteData = nullptr;
        uint32 texPaletteSize = 0;

        if ( texPaletteType != PALETTE_NONE )
        {
            // We need to decode the PS2 CLUT.
            GetPS2TexturePalette(
                engineInterface,
                nativeTex->paletteTex.transmissionAreaWidth, nativeTex->paletteTex.transmissionAreaHeight, nativeTex->clutStorageFmt, nativeTex->paletteTex.texels,
                texRasterFormat, texColorOrder,
                texRasterFormat, texColorOrder,
                texPaletteType,
                texPaletteData, texPaletteSize
            );
        }

        // Convert the input data to our texture's format.
        bool srcTexelsNewlyAllocated = false;

        bool hasConverted =
            ConvertMipmapLayerNative(
                engineInterface,
                width, height, layerWidth, layerHeight, srcTexels, dataSize,
                rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                texRasterFormat, texDepth, rowAlignment, texColorOrder, texPaletteType, texPaletteData, texPaletteSize, RWCOMPRESS_NONE,
                false,
                width, height,
                srcTexels, dataSize
            );

        if ( hasConverted )
        {
            srcTexelsNewlyAllocated = true;
        }

        // We do not need the CLUT anymore, if we allocated it.
        if ( texPaletteData )
        {
            engineInterface->PixelFree( texPaletteData );
        }

        // Prepare swizzling parameters.
        eMemoryLayoutType pixelMemLayoutType = nativeTex->pixelStorageMode;
        eMemoryLayoutType bitblt_targetFmt = nativeTex->bitblt_targetFmt;

        // Now we have to encode our texels.
        void *dstSwizzledTexels = nullptr;
        uint32 dstSwizzledDataSize = 0;

        uint32 packedWidth, packedHeight;

        ConvertMipmapToPS2Format(
            engineInterface,
            layerWidth, layerHeight, srcTexels, dataSize,
            pixelMemLayoutType, bitblt_targetFmt,
            texRasterFormat, texDepth, texColorOrder,
            texRasterFormat, texDepth, texColorOrder,
            texPaletteType, texPaletteType, texPaletteSize,
            rowAlignment,
            packedWidth, packedHeight,
            dstSwizzledTexels, dstSwizzledDataSize
        );

        // Free the linear data.
        if ( srcTexelsNewlyAllocated )
        {
            engineInterface->PixelFree( srcTexels );
        }

        // Store the encoded texels.
        mipLayer.width = layerWidth;
        mipLayer.height = layerHeight;

        mipLayer.transmissionAreaWidth = packedWidth;
        mipLayer.transmissionAreaHeight = packedHeight;

        mipLayer.texels = dstSwizzledTexels;
        mipLayer.dataSize = dstSwizzledDataSize;

        // Since we encoded the texels, we cannot ever directly acquire them.
        hasDirectlyAcquiredOut = false;
    }
};

bool ps2NativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    ps2MipmapManager <false> mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTexturePS2::GSTexture> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool ps2NativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    ps2MipmapManager <false> mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTexturePS2::GSTexture> (
            engineInterface, mipMan,
            nativeTex->mipmaps,
            layerIn, feedbackOut
        );
}

void ps2NativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    return
        virtualClearMipmaps <NativeTexturePS2::GSTexture> ( engineInterface, nativeTex->mipmaps );
}

bool ps2NativeTextureTypeProvider::DoesTextureHaveAlpha( const void *objMem )
{
    const NativeTexturePS2 *nativeTex = (const NativeTexturePS2*)objMem;

    Interface *engineInterface = nativeTex->engineInterface;

    // The PS2 native texture does not store the alpha status, because it uses alpha blending all the time.
    // Hence we have to calculate the alpha flag if the framework wants it.
    // This is an expensive operation, actually, because we have to decode the texture.

    // Let's just use the methods we already wrote.
    ps2MipmapManager <true> mipMan( nativeTex );

    rw::rawMipmapLayer rawLayer;

    bool gotLayer = virtualGetMipmapLayer <NativeTexturePS2::GSTexture> (
        engineInterface, mipMan,
        0,      // we just check the first layer, should be enough.
        nativeTex->mipmaps,
        rawLayer
    );

    if ( !gotLayer )
        return false;

    bool hasAlpha = false;

    try
    {
        // Just a security measure.
        assert( rawLayer.compressionType == RWCOMPRESS_NONE );

        hasAlpha =
            rawMipmapCalculateHasAlpha(
                engineInterface,
                rawLayer.mipData.layerWidth, rawLayer.mipData.layerHeight, rawLayer.mipData.texels, rawLayer.mipData.dataSize,
                rawLayer.rasterFormat, rawLayer.depth, rawLayer.rowAlignment, rawLayer.colorOrder,
                rawLayer.paletteType, rawLayer.paletteData, rawLayer.paletteSize
            );
    }
    catch( ... )
    {
        if ( rawLayer.isNewlyAllocated )
        {
            engineInterface->PixelFree( rawLayer.mipData.texels );
        }

        throw;
    }

    // Free memory.
    if ( rawLayer.isNewlyAllocated )
    {
        engineInterface->PixelFree( rawLayer.mipData.texels );
    }

    return hasAlpha;
}

void ps2NativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    size_t mipmapCount = nativeTex->mipmaps.GetCount();

    infoOut.mipmapCount = (uint32)mipmapCount;

    uint32 baseWidth = 0;
    uint32 baseHeight = 0;

    if ( mipmapCount > 0 )
    {
        baseWidth = nativeTex->mipmaps[ 0 ].width;
        baseHeight = nativeTex->mipmaps[ 0 ].height;
    }

    infoOut.baseWidth = baseWidth;
    infoOut.baseHeight = baseHeight;
}

void ps2NativeTextureTypeProvider::GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    // We are just a standard raster.
    // The PS2 specific encoding does not matter.
    rwStaticString <char> formatString = "PS2 ";

    getDefaultRasterFormatString(
        GetGenericRasterFormatFromPS2( nativeTex->rasterFormat ),
        nativeTex->depth,
        GetGenericPaletteTypeFromPS2( nativeTex->paletteType ),
        COLOR_RGBA,
        formatString
    );

    if ( buf )
    {
        strncpy( buf, formatString.GetConstString(), bufLen );
    }

    lengthOut = formatString.GetLength();
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2
