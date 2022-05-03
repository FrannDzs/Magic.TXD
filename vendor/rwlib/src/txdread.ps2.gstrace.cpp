/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.ps2.gstrace.cpp
*  PURPOSE:     PlayStation2 native texture implementation variable tracing.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_ENABLE_PS2GSTRACE

#include "txdread.ps2.hxx"

#include <fstream>

namespace rw
{
    
#define VALCMP( left, right ) EIR_VALCMP( left, right )
#define VALCMP_LT( _cmpVal ) EIR_VALCMP_LT( _cmpVal )

struct ps2MipdataRegistrationKey
{
    LibraryVersion rwver;
    eMemoryLayoutType memLayout;
    eMemoryLayoutType bitbltLayout;
    eCLUTMemoryLayoutType clutMemLayout;

    struct mipdata
    {
        uint32 width;
        uint32 height;
    };

    rwStaticVector <mipdata> mips;

    mipdata clutdata;

    AINLINE static eir::eCompResult less_than_cmp_mipdata( const mipdata& lmip, const mipdata& rmip )
    {
        VALCMP( lmip.width, rmip.width );
        VALCMP( lmip.height, rmip.height );

        return eir::eCompResult::EQUAL;
    }

    AINLINE static eir::eCompResult cmp_rwver( const LibraryVersion& left, const LibraryVersion& right )
    {
        if ( left == right )
        {
            return eir::eCompResult::EQUAL;
        }

        return ( left.isNewerThan( right, false ) ? eir::eCompResult::LEFT_GREATER : eir::eCompResult::LEFT_LESS );
    }

    AINLINE bool operator < ( const ps2MipdataRegistrationKey& right ) const
    {
        VALCMP_LT( cmp_rwver( this->rwver, right.rwver ) );
        VALCMP_LT( eir::DefaultValueCompare( this->memLayout, right.memLayout ) );
        VALCMP_LT( eir::DefaultValueCompare( this->bitbltLayout, right.bitbltLayout ) );

        if ( DoesMemoryLayoutTypeRequireCLUT( this->memLayout ) )
        {
            VALCMP_LT( eir::DefaultValueCompare( this->clutMemLayout, right.clutMemLayout ) );
        }

        size_t mipCount = this->mips.GetCount();
        VALCMP_LT( eir::DefaultValueCompare( mipCount, right.mips.GetCount() ) );

        for ( size_t n = 0; n < mipCount; n++ )
        {
            const mipdata& lmip = mips[n];
            const mipdata& rmip = right.mips[n];

            VALCMP_LT( less_than_cmp_mipdata( lmip, rmip ) );
        }

        const mipdata& lclut = this->clutdata;
        const mipdata& rclut = right.clutdata;
        VALCMP_LT( less_than_cmp_mipdata( lclut, rclut ) );

        return false;
    }
};

struct ps2MipdataRegistrationValue
{
    struct gsdata
    {
        uint32 transmissionOffsetX, transmissionOffsetY;
        uint32 transmissionAreaWidth, transmissionAreaHeight;
        uint32 tbw, tbp;
    };

    rwStaticVector <gsdata> mipgsdata;

    gsdata clutgsdata;
    uint32 texMemSize;

    AINLINE static eir::eCompResult cmp_gsdata( const gsdata& left, const gsdata& right )
    {
        VALCMP( left.transmissionOffsetX, right.transmissionOffsetX );
        VALCMP( left.transmissionOffsetY, right.transmissionOffsetY );
        VALCMP( left.transmissionAreaWidth, right.transmissionAreaWidth );
        VALCMP( left.transmissionAreaHeight, right.transmissionAreaHeight );
        VALCMP( left.tbp, right.tbp );
        VALCMP( left.tbw, right.tbw );

        return eir::eCompResult::EQUAL;
    }

    AINLINE bool operator < ( const ps2MipdataRegistrationValue& right ) const
    {
        size_t mipcount = this->mipgsdata.GetCount();
        VALCMP_LT( eir::DefaultValueCompare( mipcount, right.mipgsdata.GetCount() ) );

        for ( size_t n = 0; n < mipgsdata.GetCount(); n++ )
        {
            const gsdata& lmip = mipgsdata[n];
            const gsdata& rmip = right.mipgsdata[n];

            VALCMP_LT( cmp_gsdata( lmip, rmip ) );
        }

        VALCMP_LT( cmp_gsdata( clutgsdata, right.clutgsdata ) );
        VALCMP_LT( eir::DefaultValueCompare( this->texMemSize, right.texMemSize ) );

        return false;
    }
};

static rwStaticMap <ps2MipdataRegistrationKey, rwStaticSet <ps2MipdataRegistrationValue>> _gs_data_map;

static void store_mip( const NativeTexturePS2::GSTexture& gsmip, ps2MipdataRegistrationKey::mipdata& mip )
{
    mip.width = gsmip.width;
    mip.height = gsmip.height;
}

static void store_gsdata( const NativeTexturePS2::GSTexture& gsmip, ps2MipdataRegistrationValue::gsdata& mip )
{
    mip.transmissionOffsetX = gsmip.transmissionOffsetX;
    mip.transmissionOffsetY = gsmip.transmissionOffsetY;
    mip.transmissionAreaWidth = gsmip.transmissionAreaWidth;
    mip.transmissionAreaHeight = gsmip.transmissionAreaHeight;
    mip.tbp = gsmip.textureBasePointer;
    mip.tbw = gsmip.textureBufferWidth; 
}

void _ps2gstrace_on_texture_deserialize( NativeTexturePS2 *tex )
{
    ps2MipdataRegistrationKey key;
    key.rwver = tex->texVersion;
    key.memLayout = tex->pixelStorageMode;
    key.bitbltLayout = tex->bitblt_targetFmt;
    key.clutMemLayout = tex->clutStorageFmt;
    
    for ( const NativeTexturePS2::GSTexture& gsmip : tex->mipmaps )
    {
        ps2MipdataRegistrationKey::mipdata mip;
        store_mip( gsmip, mip );

        key.mips.AddToBack( std::move( mip ) );
    }

    store_mip( tex->paletteTex, key.clutdata );

    auto& vecResults = _gs_data_map[ std::move( key ) ];

    ps2MipdataRegistrationValue gsitem;

    for ( const NativeTexturePS2::GSTexture& gsmip : tex->mipmaps )
    {
        ps2MipdataRegistrationValue::gsdata data;
        store_gsdata( gsmip, data );

        gsitem.mipgsdata.AddToBack( std::move( data ) );
    }

    store_gsdata( tex->paletteTex, gsitem.clutgsdata );
    gsitem.texMemSize = tex->texMemSize;

    vecResults.Insert( std::move( gsitem ) );
}

static void print_gstexture_info( std::wostream& outstr, const ps2MipdataRegistrationValue::gsdata& gsmip, bool print_tbw = true )
{
    outstr << "DSAX: " << gsmip.transmissionOffsetX << std::endl;
    outstr << "DSAY: " << gsmip.transmissionOffsetY << std::endl;
    outstr << "RRW: " << gsmip.transmissionAreaWidth << std::endl;
    outstr << "RRH: " << gsmip.transmissionAreaHeight << std::endl;
    outstr << "TBP: " << gsmip.tbp << std::endl;

    if ( print_tbw )
    {
        outstr << "TBW: " << gsmip.tbw << std::endl;
    }
}

void _ps2gstrace_finalize_on_exit( void )
{
    // Write out the results to disk.
    // We use a really cheap way of doing it.
    // This is only used for debugging so that's OK.
    std::wfstream outstr( "gsdataout.txt", std::ios_base::out | std::ios_base::binary );

    if ( outstr.good() )
    {
        size_t numEntries = _gs_data_map.GetKeyValueCount();

#ifdef RWLIB_PS2GSTRACE_TYPE_TEXT
        outstr << L"GS data count: " << numEntries << std::endl;
        outstr << L"Compilation date: " << __DATE__ << std::endl;
#endif //RWLIB_PS2GSTRACE_TYPE_TEXT

        outstr << std::endl;

        for ( auto *node : _gs_data_map )
        {
            const auto& mipdata = node->GetKey();
            const auto& gsdatalist = node->GetValue();

            auto verstr = mipdata.rwver.toString();

            eMemoryLayoutType memLayout = mipdata.memLayout;

            bool needsCLUT = DoesMemoryLayoutTypeRequireCLUT( memLayout );

#ifdef RWLIB_PS2GSTRACE_TYPE_TEXT
            outstr << L"RW Version: " << verstr.GetConstString() << std::endl;
            outstr << L"MemLayout: " << GetValidMemoryLayoutTypeName( memLayout ) << std::endl;
            outstr << L"BITBLT Layout: " << GetValidMemoryLayoutTypeName( mipdata.bitbltLayout ) << std::endl;

            if ( needsCLUT )
            {
                outstr << L"CLUTMemLayout: " << GetValidCLUTMemoryLayoutTypeName( mipdata.clutMemLayout ) << std::endl;
            }

            outstr << L"Base level W: " << mipdata.mips[0].width << std::endl;
            outstr << L"Base Level H: " << mipdata.mips[0].height << std::endl;
            outstr << std::endl;
#endif //RWLIB_PS2GSTRACE_TYPE_TEXT

            size_t n = 0;

#ifdef RWLIB_PS2GSTRACE_TYPE_TEXT
            size_t gsdatalistcnt = gsdatalist.GetValueCount();
#endif //RWLIB_PS2GSTRACE_TYPE_TEXT
            
            for ( auto& gsdata : gsdatalist )
            {
#ifdef RWLIB_PS2GSTRACE_TYPE_TEXT
                if ( gsdatalistcnt > 1 )
                {
                    outstr << L"Alternative number " << n << std::endl;
                }

                size_t mipidx = 0;
                
                for ( auto& gsmip : gsdata.mipgsdata )
                {
                    outstr << "Mip level " << mipidx << std::endl;
                    print_gstexture_info( outstr, gsmip, true );

                    mipidx++;
                }

                if ( needsCLUT )
                {
                    outstr << "CLUT texture" << std::endl;
                    print_gstexture_info( outstr, gsdata.clutgsdata, false );
                }

                outstr << "Memory Size: " << gsdata.texMemSize << std::endl;
#elif defined(RWLIB_PS2GSTRACE_TYPE_UNITTEST)
                outstr << "{" << std::endl;
                outstr << "    const ps2TextureMetrics mipmaps[] = { ";

                bool hasMipEntry = false;

                for ( const auto& gsitem : gsdata.mipgsdata )
                {
                    if ( hasMipEntry )
                    {
                        outstr << ", ";
                    }

                    outstr << "{ " << gsitem.transmissionAreaWidth << ", " << gsitem.transmissionAreaHeight << " }";

                    hasMipEntry = true;
                }

                outstr << " };" << std::endl;
                outstr << "    const ps2TextureMetrics clutTex = { " << mipdata.clutdata.width << ", " << mipdata.clutdata.height << " };" << std::endl;
                outstr << std::endl;
                outstr << "    rw::uint32 mipmapBasePointer[countof(mipmaps)];" << std::endl;
                outstr << "    rw::uint32 mipmapBufferWidth[countof(mipmaps)];" << std::endl;
                outstr << "    ps2MipmapTransmissionData mipmapTransData[countof(mipmaps)];" << std::endl;
                outstr << std::endl;
                outstr << "    ps2MipmapTransmissionData clutTransData;" << std::endl;
                outstr << "    rw::uint32 clutBasePointer;" << std::endl;
                outstr << std::endl;
                outstr << "    rw::uint32 texMemSize;" << std::endl;
                outstr << std::endl;
                outstr << "    bool allocSuccess = ps2texdriver->CalculateTextureMemoryAllocation(" << std::endl;
                outstr << "        eMemoryLayoutType::" << GetValidMemoryLayoutTypeName( mipdata.memLayout ) << "," << std::endl;
                outstr << "        eMemoryLayoutType::" << GetValidMemoryLayoutTypeName( mipdata.bitbltLayout ) << "," << std::endl;
                outstr << "        eCLUTMemoryLayoutType::" << GetValidCLUTMemoryLayoutTypeName( mipdata.clutMemLayout ) << "," << std::endl;
                outstr << "        mipmaps, countof(mipmaps), clutTex," << std::endl;
                outstr << "        mipmapBasePointer, mipmapBufferWidth, mipmapTransData," << std::endl;
                outstr << "        clutBasePointer, clutTransData," << std::endl;
                outstr << "        texMemSize" << std::endl;
                outstr << "    );" << std::endl;
                outstr << std::endl;
                outstr << "    assert( allocSuccess == true );" << std::endl;

                size_t mipidx = 0;

                for ( const auto& gsitem : gsdata.mipgsdata )
                {
                    outstr << "    assert( mipmapBasePointer[" << mipidx << "] == " << gsitem.tbp << " );" << std::endl;
                    outstr << "    assert( mipmapBufferWidth[" << mipidx << "] == " << gsitem.tbw << " );" << std::endl;
                    outstr << "    assert( mipmapTransData[" << mipidx << "].destX == " << gsitem.transmissionOffsetX << " && mipmapTransData[" << mipidx << "].destY == " << gsitem.transmissionOffsetY << " );" << std::endl;

                    mipidx++;
                }

                if ( needsCLUT )
                {
                    outstr << "    assert( clutBasePointer == " << gsdata.clutgsdata.tbp << " );" << std::endl;
                    outstr << "    assert( clutTransData.destX == " << gsdata.clutgsdata.transmissionOffsetX << " && clutTransData.destY == " << gsdata.clutgsdata.transmissionOffsetY << " );" << std::endl;
                }

                outstr << "    assert( texMemSize == " << gsdata.texMemSize << " );" << std::endl;
                outstr << "}" << std::endl;
#endif //GSTRACE OUTPUT TYPE

                outstr << std::endl;

                n++;
            }

            outstr << std::endl;
        }
    }
}

} // namespace rw

#endif //RWLIB_ENABLE_PS2GSTRACE