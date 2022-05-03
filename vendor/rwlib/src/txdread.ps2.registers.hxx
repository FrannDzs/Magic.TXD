/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.ps2shared.hxx
*  PURPOSE:     Hardware register definitions of the PS2.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_PS2_HARDWARE_REGISTERS_HEADER_
#define _RENDERWARE_PS2_HARDWARE_REGISTERS_HEADER_

#include "txdread.ps2mem.hxx"

#include <stdint.h>

#include <assert.h>

namespace rw
{

enum class eCLUTStorageMode
{
    CSM1 = 0,
    CSM2
};

AINLINE const wchar_t* GetValidCLUTStorageModeName( eCLUTStorageMode mode )
{
    switch( mode )
    {
    case eCLUTStorageMode::CSM1:        return L"CSM1";
    case eCLUTStorageMode::CSM2:        return L"CSM2";
    }

    return nullptr;
}

enum class eTextureColorComponent
{
    RGB = 0,
    RGBA
};

AINLINE const wchar_t* GetValidTextureColorComponentName( eTextureColorComponent value )
{
    switch( value )
    {
    case eTextureColorComponent::RGB:   return L"RGB";
    case eTextureColorComponent::RGBA:  return L"RGBA";
    }

    return nullptr;
}

enum class eTextureFunction
{
    MODULATE = 0,
    DECAL,
    HIGHLIGHT,
    HIGHLIGHT2
};

AINLINE const wchar_t* GetValidTextureFunctionName( eTextureFunction func )
{
    switch( func )
    {
    case eTextureFunction::MODULATE:    return L"MODULATE";
    case eTextureFunction::DECAL:       return L"DECAL";
    case eTextureFunction::HIGHLIGHT:   return L"HIGHLIGHT";
    case eTextureFunction::HIGHLIGHT2:  return L"HIGHLIGHT2";
    }

    return nullptr;
}

// General purpose registers.
// 64bit wide.
enum class eGSRegister
{
    PRIM,
    RGBAQ,
    ST,
    UV,
    XYZF2,
    XYZ2,
    TEX0_1,
    TEX0_2,
    CLAMP_1,
    CLAMP_2,
    FOG,
    XYZF3 = 0x0C,
    XYZ3,

    TEX1_1 = 0x14,
    TEX1_2,
    TEX2_1,
    TEX2_2,
    XYOFFSET_1,
    XYOFFSET_2,
    PRMODECONT,
    PRMODE,
    TEXCLUT,

    SCANMSK = 0x22,

    MIPTBP1_1 = 0x34,
    MIPTBP1_2,
    MIPTBP2_1,
    MIPTBP2_2,

    TEXA = 0x3B,

    FOGCOL = 0x3D,

    TEXFLUSH = 0x3F,
    SCISSOR_1,
    SCISSOR_2,
    ALPHA_1,
    ALPHA_2,
    DIMX,
    DTHE,
    COLCLAMP,
    TEST_1,
    TEST_2,
    PABE,
    FBA_1,
    FBA_2,
    FRAME_1,
    FRAME_2,
    ZBUF_1,
    ZBUF_2,
    BITBLTBUF,

    TRXPOS,
    TRXREG,
    TRXDIR,

    HWREG,

    SIGNAL,
    FINISH,
    LABEL
};

AINLINE const wchar_t* GetValidGSRegisterName( eGSRegister reg )
{
    switch( reg )
    {
    case eGSRegister::PRIM:         return L"PRIM";
    case eGSRegister::RGBAQ:        return L"RGBAQ";
    case eGSRegister::ST:           return L"ST";
    case eGSRegister::UV:           return L"UV";
    case eGSRegister::XYZF2:        return L"XYZF2";
    case eGSRegister::XYZ2:         return L"XYZ2";
    case eGSRegister::TEX0_1:       return L"TEX0_1";
    case eGSRegister::TEX0_2:       return L"TEX0_2";
    case eGSRegister::CLAMP_1:      return L"CLAMP_1";
    case eGSRegister::CLAMP_2:      return L"CLAMP_2";
    case eGSRegister::FOG:          return L"FOG";
    case eGSRegister::XYZF3:        return L"XYZF3";
    case eGSRegister::XYZ3:         return L"XYZ3";
    case eGSRegister::TEX1_1:       return L"TEX1_1";
    case eGSRegister::TEX1_2:       return L"TEX1_2";
    case eGSRegister::TEX2_1:       return L"TEX2_1";
    case eGSRegister::TEX2_2:       return L"TEX2_2";
    case eGSRegister::XYOFFSET_1:   return L"XYOFFSET_1";
    case eGSRegister::XYOFFSET_2:   return L"XYOFFSET_2";
    case eGSRegister::PRMODECONT:   return L"PRMODECONT";
    case eGSRegister::PRMODE:       return L"PRMODE";
    case eGSRegister::TEXCLUT:      return L"TEXCLUT";
    case eGSRegister::SCANMSK:      return L"SCANMSK";
    case eGSRegister::MIPTBP1_1:    return L"MIPTBP1_1";
    case eGSRegister::MIPTBP1_2:    return L"MIPTBP1_2";
    case eGSRegister::MIPTBP2_1:    return L"MIPTBP2_1";
    case eGSRegister::MIPTBP2_2:    return L"MIPTBP2_2";
    case eGSRegister::TEXA:         return L"TEXA";
    case eGSRegister::FOGCOL:       return L"FOGCOL";
    case eGSRegister::TEXFLUSH:     return L"TEXFLUSH";
    case eGSRegister::SCISSOR_1:    return L"SCISSOR_1";
    case eGSRegister::SCISSOR_2:    return L"SCISSOR_2";
    case eGSRegister::ALPHA_1:      return L"ALPHA_1";
    case eGSRegister::ALPHA_2:      return L"ALPHA_2";
    case eGSRegister::DIMX:         return L"DIMX";
    case eGSRegister::DTHE:         return L"DTHE";
    case eGSRegister::COLCLAMP:     return L"COLCLAMP";
    case eGSRegister::TEST_1:       return L"TEST_1";
    case eGSRegister::TEST_2:       return L"TEST_2";
    case eGSRegister::PABE:         return L"PABE";
    case eGSRegister::FBA_1:        return L"FBA_1";
    case eGSRegister::FBA_2:        return L"FBA_2";
    case eGSRegister::FRAME_1:      return L"FRAME_1";
    case eGSRegister::FRAME_2:      return L"FRAME_2";
    case eGSRegister::ZBUF_1:       return L"ZBUF_1";
    case eGSRegister::ZBUF_2:       return L"ZBUF_2";
    case eGSRegister::BITBLTBUF:    return L"BITBLTBUF";
    case eGSRegister::TRXPOS:       return L"TRXPOS";
    case eGSRegister::TRXREG:       return L"TRXREG";
    case eGSRegister::TRXDIR:       return L"TRXDIR";
    case eGSRegister::HWREG:        return L"HWREG";
    case eGSRegister::SIGNAL:       return L"SIGNAL";
    case eGSRegister::FINISH:       return L"FINISH";
    case eGSRegister::LABEL:        return L"LABEL";
    }

    return nullptr;
}

// Special register descriptions for the GIFtag.
// The underlying data is actually 128bits wide.
enum class eGIFTagRegister
{
    PRIM,
    RGBAQ,
    ST,
    UV,
    XYZF2,
    XYZ2,
    TEX0_1,
    TEX0_2,
    CLAMP_1,
    CLAMP_2,
    FOG = 0xA,
    A_PLUS_D = 0xE,
    NOP,
};

AINLINE const wchar_t* GetValidGIFTagRegisterName( eGIFTagRegister reg )
{
    switch( reg )
    {
    case eGIFTagRegister::PRIM:         return L"PRIM";
    case eGIFTagRegister::RGBAQ:        return L"RGBAQ";
    case eGIFTagRegister::ST:           return L"ST";
    case eGIFTagRegister::UV:           return L"UV";
    case eGIFTagRegister::XYZF2:        return L"XYZF2";
    case eGIFTagRegister::XYZ2:         return L"XYZ2";
    case eGIFTagRegister::TEX0_1:       return L"TEX0_1";
    case eGIFTagRegister::TEX0_2:       return L"TEX0_2";
    case eGIFTagRegister::CLAMP_1:      return L"CLAMP_1";
    case eGIFTagRegister::CLAMP_2:      return L"CLAMP_2";
    case eGIFTagRegister::FOG:          return L"FOG";
    case eGIFTagRegister::A_PLUS_D:     return L"A+D";
    case eGIFTagRegister::NOP:          return L"NOP";
    }

    return nullptr;
}

// Returns the register type for REGLIST operation mode of GIFtags.
AINLINE bool GetDirectGIFTagRegisterToGSRegisterMapping( eGIFTagRegister reg, eGSRegister& gsRegOut )
{
    switch( reg )
    {
    case eGIFTagRegister::PRIM:         gsRegOut = eGSRegister::PRIM; return true;
    case eGIFTagRegister::RGBAQ:        gsRegOut = eGSRegister::RGBAQ; return true;
    case eGIFTagRegister::ST:           gsRegOut = eGSRegister::ST; return true;
    case eGIFTagRegister::UV:           gsRegOut = eGSRegister::UV; return true;
    case eGIFTagRegister::XYZF2:        gsRegOut = eGSRegister::XYZF2; return true;
    case eGIFTagRegister::XYZ2:         gsRegOut = eGSRegister::XYZ2; return true;
    case eGIFTagRegister::TEX0_1:       gsRegOut = eGSRegister::TEX0_1; return true;
    case eGIFTagRegister::TEX0_2:       gsRegOut = eGSRegister::TEX0_2; return true;
    case eGIFTagRegister::CLAMP_1:      gsRegOut = eGSRegister::CLAMP_1; return true;
    case eGIFTagRegister::CLAMP_2:      gsRegOut = eGSRegister::CLAMP_2; return true;
    case eGIFTagRegister::FOG:          gsRegOut = eGSRegister::FOG; return true;
    // A+D and NOP turn into nothing.
    default: break;
    }

    return false;
}

// For REGLIST mode operation.
AINLINE bool GetDirectGSRegisterToGIFTagRegisterMapping( eGSRegister reg, eGIFTagRegister& gifRegOut )
{
    switch( reg )
    {
    case eGSRegister::PRIM:             gifRegOut = eGIFTagRegister::PRIM; return true;
    case eGSRegister::RGBAQ:            gifRegOut = eGIFTagRegister::RGBAQ; return true;
    case eGSRegister::ST:               gifRegOut = eGIFTagRegister::ST; return true;
    case eGSRegister::UV:               gifRegOut = eGIFTagRegister::UV; return true;
    case eGSRegister::XYZF2:            gifRegOut = eGIFTagRegister::XYZF2; return true;
    case eGSRegister::XYZ2:             gifRegOut = eGIFTagRegister::XYZ2; return true;
    case eGSRegister::TEX0_1:           gifRegOut = eGIFTagRegister::TEX0_1; return true;
    case eGSRegister::TEX0_2:           gifRegOut = eGIFTagRegister::TEX0_2; return true;
    case eGSRegister::CLAMP_1:          gifRegOut = eGIFTagRegister::CLAMP_1; return true;
    case eGSRegister::CLAMP_2:          gifRegOut = eGIFTagRegister::CLAMP_2; return true;
    case eGSRegister::FOG:              gifRegOut = eGIFTagRegister::FOG; return true;
    default:
        break;
    }

    return false;
}

struct ps2GSRegisters
{
    typedef std::uint64_t ps2reg_t;

    struct TEX0_REG
    {
        inline TEX0_REG( void )
        {
            *(ps2reg_t*)this = 0L;
        }

        inline TEX0_REG( ps2reg_t val )
        {
            *(ps2reg_t*)this = val;
        }

        ps2reg_t textureBasePointer : 14;
        ps2reg_t textureBufferWidth : 6;
        ps2reg_t pixelStorageFormat : 6;        // eMemoryLayoutType
        ps2reg_t textureWidthLog2 : 4;          // max value: 10
        ps2reg_t textureHeightLog2 : 4;         // max value: 10
        ps2reg_t texColorComponent : 1;         // eTextureColorComponent
        ps2reg_t texFunction : 2;               // eTextureFunction
        ps2reg_t clutBufferBase : 14;
        ps2reg_t clutStorageFmt : 4;            // eCLUTMemoryLayoutType
        ps2reg_t clutMode : 1;                  // eCLUTStorageMode
        ps2reg_t clutEntryOffset : 5;
        ps2reg_t clutLoadControl : 3;

        inline bool operator ==( const TEX0_REG& right ) const
        {
            return ( *(ps2reg_t*)this == *(ps2reg_t*)&right );
        }

        inline bool operator !=( const TEX0_REG& right ) const
        {
            return !( *this == right );
        }

        inline operator ps2reg_t ( void ) const
        {
            return *(ps2reg_t*)this;
        }
    };
    static_assert( sizeof(TEX0_REG) == sizeof(ps2reg_t) );

    struct TEX1_REG
    {
        inline TEX1_REG( void )
        {
            *(ps2reg_t*)this = 0L;
        }

        inline TEX1_REG( ps2reg_t val )
        {
            *(ps2reg_t*)this = val;
        }

        ps2reg_t lodCalculationModel : 1;
        ps2reg_t unused1 : 1;
        ps2reg_t maximumMIPLevel : 3;
        ps2reg_t mmag : 1;
        ps2reg_t mmin : 3;
        ps2reg_t mtba : 1;
        ps2reg_t unused2 : 9;
        ps2reg_t lodParamL : 2;
        ps2reg_t unused3 : 11;
        ps2reg_t lodParamK : 12;

        inline bool operator ==( const TEX1_REG& right ) const
        {
#if 0
            return
                this->lodCalculationModel == right.lodCalculationModel &&
                this->maximumMIPLevel == right.maximumMIPLevel &&
                this->mmag == right.mmag &&
                this->mmin == right.mmin &&
                this->mtba == right.mtba &&
                this->lodParamL == right.lodParamL &&
                this->lodParamK == right.lodParamK &&
                this->unknown == right.unknown &&
                this->unknown2 == right.unknown2;
#else
            return ( *(ps2reg_t*)this == *(ps2reg_t*)&right );
#endif
        }

        inline bool operator !=( const TEX1_REG& right ) const
        {
            return !( *this == right );
        }

        inline operator ps2reg_t ( void ) const
        {
            return *(ps2reg_t*)this;
        }
    };
    static_assert( sizeof(TEX1_REG) == sizeof(ps2reg_t) );

    struct SKY_TEX1_REG_LOW
    {
        inline SKY_TEX1_REG_LOW( void )
        {
            *(uint32*)this = 0L;
        }

        inline SKY_TEX1_REG_LOW( uint32 val )
        {
            *(uint32*)this = val;
        }

        uint32 lodCalculationModel : 1;
        uint32 unused1 : 1;
        uint32 maximumMIPLevel : 3;
        uint32 mmag : 1;
        uint32 mmin : 3;
        uint32 mtba : 1;
        uint32 unused2 : 9;
        uint32 lodParamL : 2;
        uint32 unused3 : 11;

        inline bool operator ==( const SKY_TEX1_REG_LOW& right ) const
        {
            return ( *(uint32*)this == *(uint32*)&right );
        }

        inline bool operator !=( const SKY_TEX1_REG_LOW& right ) const
        {
            return !( *this == right );
        }

        inline operator uint32 ( void ) const
        {
            return *(uint32*)this;
        }
    };
    static_assert( sizeof(SKY_TEX1_REG_LOW) == sizeof(uint32) );

    struct MIPTBP1_REG
    {
        inline MIPTBP1_REG( void )
        {
            *(ps2reg_t*)this = 0L;
        }

        inline MIPTBP1_REG( ps2reg_t val )
        {
            *(ps2reg_t*)this = val;
        }

        ps2reg_t textureBasePointer1 : 14;
        ps2reg_t textureBufferWidth1 : 6;
        ps2reg_t textureBasePointer2 : 14;
        ps2reg_t textureBufferWidth2 : 6;
        ps2reg_t textureBasePointer3 : 14;
        ps2reg_t textureBufferWidth3 : 6;

        inline bool operator ==( const MIPTBP1_REG& right ) const
        {
#if 0
            return
                this->textureBasePointer1 == right.textureBasePointer1 &&
                this->textureBufferWidth1 == right.textureBufferWidth1 &&
                this->textureBasePointer2 == right.textureBasePointer2 &&
                this->textureBufferWidth2 == right.textureBufferWidth2 &&
                this->textureBasePointer3 == right.textureBasePointer3 &&
                this->textureBufferWidth3 == right.textureBufferWidth3;
#else
            return ( *(ps2reg_t*)this == *(ps2reg_t*)&right );
#endif
        }

        inline bool operator !=( const MIPTBP1_REG& right ) const
        {
            return !( *this == right );
        }

        inline operator ps2reg_t ( void ) const
        {
            return *(ps2reg_t*)this;
        }
    };

    struct MIPTBP2_REG
    {
        inline MIPTBP2_REG( void )
        {
            *(ps2reg_t*)this = 0L;
        }

        inline MIPTBP2_REG( ps2reg_t val )
        {
            *(ps2reg_t*)this = val;
        }

        ps2reg_t textureBasePointer4 : 14;
        ps2reg_t textureBufferWidth4 : 6;
        ps2reg_t textureBasePointer5 : 14;
        ps2reg_t textureBufferWidth5 : 6;
        ps2reg_t textureBasePointer6 : 14;
        ps2reg_t textureBufferWidth6 : 6;

        inline bool operator ==( const MIPTBP2_REG& right ) const
        {
#if 0
            return
                this->textureBasePointer4 == right.textureBasePointer4 &&
                this->textureBufferWidth4 == right.textureBufferWidth4 &&
                this->textureBasePointer5 == right.textureBasePointer5 &&
                this->textureBufferWidth5 == right.textureBufferWidth5 &&
                this->textureBasePointer6 == right.textureBasePointer6 &&
                this->textureBufferWidth6 == right.textureBufferWidth6;
#else
            return ( *(ps2reg_t*)this == *(ps2reg_t*)&right );
#endif
        }

        inline bool operator !=( const MIPTBP2_REG& right ) const
        {
            return !( *this == right );
        }

        inline operator ps2reg_t ( void ) const
        {
            return *(ps2reg_t*)this;
        }
    };

    struct TRXPOS_REG
    {
        inline TRXPOS_REG( void )
        {
            this->qword = 0L;
        }

        inline TRXPOS_REG( ps2reg_t val )
        {
            this->qword = val;
        }

        union
        {
            struct
            {
                ps2reg_t ssax : 11;
                ps2reg_t pad1 : 5;
                ps2reg_t ssay : 11;
                ps2reg_t pad2 : 5;
                ps2reg_t dsax : 11;
                ps2reg_t pad3 : 5;
                ps2reg_t dsay : 11;
                ps2reg_t dir : 2;
            };
            struct
            {
                ps2reg_t qword;
            };
        };

        inline operator ps2reg_t ( void ) const
        {
            return *(ps2reg_t*)this;
        }
    };

    struct TRXREG_REG
    {
        inline TRXREG_REG( void )
        {
            this->qword = 0L;
        }

        inline TRXREG_REG( ps2reg_t val )
        {
            this->qword = val;
        }

        union
        {
            struct
            {
                ps2reg_t transmissionAreaWidth : 12;
                ps2reg_t pad1 : 20;
                ps2reg_t transmissionAreaHeight : 12;
            };
            struct
            {
                ps2reg_t qword;
            };
        };

        inline operator ps2reg_t ( void ) const
        {
            return *(ps2reg_t*)this;
        }
    };

    struct TRXDIR_REG
    {
        inline TRXDIR_REG( void )
        {
            this->qword = 0L;
        }

        inline TRXDIR_REG( ps2reg_t val )
        {
            this->qword = val;
        }

        union
        {
            struct
            {
                ps2reg_t xdir : 2;
            };
            struct
            {
                ps2reg_t qword;
            };
        };

        inline operator ps2reg_t ( void ) const
        {
            return *(ps2reg_t*)this;
        }
    };

    TEX0_REG tex0;
    uint32 clutOffset;
    SKY_TEX1_REG_LOW tex1_low;

    MIPTBP1_REG miptbp1;
    MIPTBP2_REG miptbp2;
};


} // namespace rw

#endif //_RENDERWARE_PS2_HARDWARE_REGISTERS_HEADER_
