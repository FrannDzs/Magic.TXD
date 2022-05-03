/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.h
*  PURPOSE:     Main public header.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_H_
#define _RENDERWARE_H_

#include <iostream>
#include <bitset>
#include <assert.h>
#include <algorithm>
#include <tuple>
#include <atomic>

#define _USE_MATH_DEFINES
#include <math.h>

// Include some special vendor libraries.
#include <sdk/MemoryRaw.h>
#include <sdk/rwlist.hpp>
#include <sdk/Endian.h>
#include <sdk/MetaHelpers.h>

namespace rw
{

// Forward declaration.
struct Interface;

// Sort-of make it hard to extend from native RW types outside of the core.
// TODO: this should not be necessary; object variables have to be hidden completely!
#ifndef RWCORE
#define RWSDK_FINALIZER final
#else
#define RWSDK_FINALIZER
#endif

// Common types used in this library.
typedef std::int8_t int8;       // 1 byte
typedef std::int16_t int16;     // 2 bytes
typedef std::int32_t int32;     // 4 bytes
typedef std::int64_t int64;     // 8 bytes
typedef std::uint8_t uint8;     // 1 byte
typedef std::uint16_t uint16;   // 2 bytes
typedef std::uint32_t uint32;   // 4 bytes
typedef std::uint64_t uint64;   // 8 bytes
typedef std::float_t float32;   // 4 bytes

// These do not have to be exclusively defined, so be careful.
enum : uint32
{
    PLATFORM_OGL = 2,
    PLATFORM_PS2 = 4,
    PLATFORM_XBOX = 5
};

// An exclusively defined list of chunk ids.
enum CHUNK_TYPE : uint32
{
    CHUNK_NAOBJECT        = 0x0,    // do NOT use this.
    CHUNK_STRUCT          = 0x1,    // anonymous chunk.
    CHUNK_STRING          = 0x2,
    CHUNK_EXTENSION       = 0x3,
    CHUNK_CAMERA          = 0x5,
    CHUNK_TEXTURE         = 0x6,
    CHUNK_MATERIAL        = 0x7,
    CHUNK_MATLIST         = 0x8,
    CHUNK_ATOMICSECT      = 0x9,
    CHUNK_PLANESECT       = 0xA,
    CHUNK_WORLD           = 0xB,
    CHUNK_SPLINE          = 0xC,
    CHUNK_MATRIX          = 0xD,
    CHUNK_FRAMELIST       = 0xE,
    CHUNK_GEOMETRY        = 0xF,
    CHUNK_CLUMP           = 0x10,
    CHUNK_LIGHT           = 0x12,
    CHUNK_UNICODESTRING   = 0x13,
    CHUNK_ATOMIC          = 0x14,
    CHUNK_TEXTURENATIVE   = 0x15,
    CHUNK_TEXDICTIONARY   = 0x16,
    CHUNK_ANIMDATABASE    = 0x17,
    CHUNK_IMAGE           = 0x18,
    CHUNK_SKINANIMATION   = 0x19,
    CHUNK_GEOMETRYLIST    = 0x1A,
    CHUNK_ANIMANIMATION   = 0x1B,
    CHUNK_HANIMANIMATION  = 0x1B,
    CHUNK_TEAM            = 0x1C,
    CHUNK_CROWD           = 0x1D,
    CHUNK_RIGHTTORENDER   = 0x1F,
    CHUNK_MTEFFECTNATIVE  = 0x20,
    CHUNK_MTEFFECTDICT    = 0x21,
    CHUNK_TEAMDICTIONARY  = 0x22,
    CHUNK_PITEXDICTIONARY = 0x23,
    CHUNK_TOC             = 0x24,
    CHUNK_PRTSTDGLOBALDATA = 0x25,
    CHUNK_ALTPIPE         = 0x26,
    CHUNK_PIPEDS          = 0x27,
    CHUNK_PATCHMESH       = 0x28,
    CHUNK_CHUNKGROUPSTART = 0x29,
    CHUNK_CHUNKGROUPEND   = 0x2A,
    CHUNK_UVANIMDICT      = 0x2B,
    CHUNK_COLLTREE        = 0x2C,
    CHUNK_ENVIRONMENT     = 0x2D,
    CHUNK_COREPLUGINIDMAX = 0x2E,

    CHUNK_MORPH           = 0x105,
    CHUNK_SKYMIPMAP       = 0x110,
    CHUNK_SKIN            = 0x116,
    CHUNK_PARTICLES       = 0x118,
    CHUNK_HANIM           = 0x11E,
    CHUNK_MATERIALEFFECTS = 0x120,
    CHUNK_ADCPLG          = 0x134,
    CHUNK_BINMESH         = 0x50E,
    CHUNK_NATIVEDATA      = 0x510,
    CHUNK_VERTEXFORMAT    = 0x510,

    CHUNK_PIPELINESET      = 0x253F2F3,
    CHUNK_SPECULARMAT      = 0x253F2F6,
    CHUNK_2DFX             = 0x253F2F8,
    CHUNK_NIGHTVERTEXCOLOR = 0x253F2F9,
    CHUNK_COLLISIONMODEL   = 0x253F2FA,
    CHUNK_REFLECTIONMAT    = 0x253F2FC,
    CHUNK_MESHEXTENSION    = 0x253F2FD,
    CHUNK_FRAME            = 0x253F2FE
};

}

// Include some useful headers.
#include "renderware.common.h"

namespace rw
{

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif //_MSC_VER

// Decoded RenderWare version management struct.
struct LibraryVersion
{
    inline LibraryVersion( uint8 libMajor = 3, uint8 libMinor = 0, uint8 revMajor = 0, uint8 revMinor = 0 )
    {
        this->buildNumber = 0xFFFF;
        this->rwLibMajor = libMajor;
        this->rwLibMinor = libMinor;
        this->rwRevMajor = revMajor;
        this->rwRevMinor = revMinor;
    }

    uint16 buildNumber;

    union
    {
        uint32 version;
        struct
        {
            uint8 rwRevMinor;
            uint8 rwRevMajor;
            uint8 rwLibMinor;
            uint8 rwLibMajor;
        };
    };

    inline bool operator ==( const LibraryVersion& right ) const
    {
        return
            this->buildNumber == right.buildNumber &&
            this->version == right.version;
    }

    inline bool operator !=( const LibraryVersion& right ) const
    {
        return !( *this == right );
    }

    inline bool isNewerThan( const LibraryVersion& right, bool allowEqual = true ) const
    {
        // Check *.-.-.-
        if ( this->rwLibMajor > right.rwLibMajor )
            return true;

        if ( this->rwLibMajor == right.rwLibMajor )
        {
            // Check -.*.-.-
            if ( this->rwLibMinor > right.rwLibMinor )
                return true;

            if ( this->rwLibMinor == right.rwLibMinor )
            {
                // Check -.-.*.-
                if ( this->rwRevMajor > right.rwRevMajor )
                    return true;

                if ( this->rwRevMajor == right.rwRevMajor )
                {
                    // Check -.-.-.*
                    if ( this->rwRevMinor > right.rwRevMinor )
                        return true;

                    if ( this->rwRevMinor == right.rwRevMinor )
                    {
                        if ( allowEqual )
                            return true;
                    }
                }
            }
        }

        return false;
    }

    void set(unsigned char LibMajor, unsigned char LibMinor, unsigned char RevMajor, unsigned char RevMinor, unsigned short buildNumber = 0xFFFF)
    {
        this->buildNumber = buildNumber;
        this->rwLibMajor = LibMajor;
        this->rwLibMinor = LibMinor;
        this->rwRevMajor = RevMajor;
        this->rwRevMinor = RevMinor;
    }

    rwStaticString <char> toString( bool includeBuild = true ) const
    {
        rwStaticString <char> returnVer;

        char numBuf[ 10 ];

        // Zero it.
        numBuf[ sizeof( numBuf ) - 1 ] = '\0';

        snprintf( numBuf, sizeof( numBuf ) - 1, "%u", this->rwLibMajor );

        returnVer += numBuf;
        returnVer += ".";

        snprintf( numBuf, sizeof( numBuf ) - 1, "%u", this->rwLibMinor );

        returnVer += numBuf;
        returnVer += ".";

        snprintf( numBuf, sizeof( numBuf ) - 1, "%u", this->rwRevMajor );

        returnVer += numBuf;
        returnVer += ".";

        snprintf( numBuf, sizeof( numBuf ) - 1, "%u", this->rwRevMinor );

        returnVer += numBuf;

        if ( includeBuild )
        {
            // If we have a valid build number, append it.
            if ( this->buildNumber != 0xFFFF )
            {
                snprintf( numBuf, sizeof( numBuf ) - 1, "%u", this->buildNumber );

                returnVer += " (build: ";
                returnVer += numBuf;
                returnVer += ")";
            }
        }

        return returnVer;
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER

/*
 * DFFs
 */

enum
{
    FLAGS_TRISTRIP   = 0x01,
    FLAGS_POSITIONS  = 0x02,
    FLAGS_TEXTURED   = 0x04,
    FLAGS_PRELIT     = 0x08,
    FLAGS_NORMALS    = 0x10,
    FLAGS_LIGHT      = 0x20,
    FLAGS_MODULATEMATERIALCOLOR  = 0x40,
    FLAGS_TEXTURED2  = 0x80
};

enum
{
	MATFX_BUMPMAP = 0x1,
	MATFX_ENVMAP = 0x2,
	MATFX_BUMPENVMAP = 0x3,
	MATFX_DUAL = 0x4,
	MATFX_UVTRANSFORM = 0x5,
	MATFX_DUALUVTRANSFORM = 0x6,
};

enum
{
	FACETYPE_STRIP = 0x1,
	FACETYPE_LIST = 0x0
};

namespace KnownVersions
{
    enum eGameVersion
    {
        GTA3_PS2,
        GTA3_PC,
        GTA3_XBOX,
        VC_PS2,
        VC_PC,
        VC_XBOX,
        SA,
        MANHUNT_PC,
        MANHUNT_PS2,
        BULLY,
        LCS_PSP,
        SHEROES_GC
    };

    LibraryVersion  getGameVersion( eGameVersion gameVer );
};

// If this macro is inside RW types, you must not attempt to construct them from outside of RenderWare code.
#define RW_NOT_DIRECTLY_CONSTRUCTIBLE \
    void* operator new( size_t memSize, void *place ) { return place; } \
    void operator delete( void *ptr, void *place ) { return; } \
    void* operator new( size_t ) = delete; \
    void operator delete( void* ) { assert( 0 ); }

// Main RenderWare abstraction type.
struct RwObject abstract
{
    Interface *engineInterface;

    RwObject( Interface *engineInterface, void *construction_params );

    inline RwObject( const RwObject& right )
    {
        // Constructor that is called for cloning.
        this->engineInterface = right.engineInterface;
        this->objVersion = right.objVersion;
    }

    inline ~RwObject( void )
    {
        return;
    }

    inline Interface* GetEngine( void ) const
    {
        return this->engineInterface;
    }

    inline void SetEngineVersion( const LibraryVersion& version )
    {
        if ( version != this->objVersion )
        {
            // Notify the object about this change.
            this->OnVersionChange( this->objVersion, version );

            // Update our version.
            this->objVersion = version;
        }
    }

    inline const LibraryVersion& GetEngineVersion( void ) const
    {
        return this->objVersion;
    }

    // Override this function from a special RW object to be notified of version changes.
    virtual void OnVersionChange( const LibraryVersion& oldVer, const LibraryVersion& newVer )
    {
        return;
    }

    RW_NOT_DIRECTLY_CONSTRUCTIBLE;

protected:
    LibraryVersion objVersion;
};

// Special acquisition function using reference counting.
// Objects do not have to support reference counts, but if they do then there are special destruction semantics.
RwObject* AcquireObject( RwObject *obj );
void ReleaseObject( RwObject *obj );

uint32 GetRefCount( RwObject *obj );

#if 0

struct Frame : public RwObject
{
    inline Frame( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
    {
        this->parent = -1;
        this->hasHAnim = false;
        this->hAnimUnknown1 = 0;
        this->hAnimBoneId = -1;
        this->hAnimBoneCount = 0;
        this->hAnimUnknown2 = 0;
        this->hAnimUnknown3 = 0;

	    for (int i = 0; i < 3; i++)
        {
		    position[i] = 0.0f;

		    for ( int j = 0; j < 3; j++ )
            {
			    rotationMatrix[ i*3 + j ] = (i == j) ? 1.0f : 0.0f;
            }
	    }
    }

	float32 rotationMatrix[9];
	float32 position[3];
	int32 parent;

	/* Extensions */

	/* node name */
	rwStaticString <char> name;

	/* hanim */
	bool hasHAnim;
	uint32 hAnimUnknown1;
	int32 hAnimBoneId;
	uint32 hAnimBoneCount;
	uint32 hAnimUnknown2;
	uint32 hAnimUnknown3;
	rwStaticVector<int32> hAnimBoneIds;
	rwStaticVector<uint32> hAnimBoneNumbers;
	rwStaticVector<uint32> hAnimBoneTypes;

	/* functions */
	void readStruct(std::istream &dff);
	void readExtension(std::istream &dff);
	uint32 writeStruct(std::ostream &dff);
	uint32 writeExtension(std::ostream &dff);

	void dump(uint32 index, rwStaticString <char> ind = "");
};

#endif //disabled

// For early-initialization of plugins and the ability to register own plugins.
void ReferenceEngineEnvironment( void );
void DereferenceEngineEnvironment( void );

} // namespace rw

#include "renderware.math.h"
#include "renderware.threading.h"
#include "renderware.stream.h"
#include "renderware.blockapi.h"
#include "renderware.txd.h"
#include "renderware.material.h"
#include "renderware.dff.h"
#include "renderware.imaging.h"
#include "renderware.file.h"
#include "renderware.events.h"
#include "renderware.windowing.h"
#include "renderware.driver.h"
#include "renderware.drawing.h"
#include "renderware.shader.h"
#include "renderware.gpures.h"
#include "renderware.plg.h"
#include "renderware.lateinit.h"
#include "renderware.serialize.h"

namespace rw
{

// Some pretty useful mappings.
typedef Vector <float, 2> RwV2d;
typedef Vector <float, 3> RwV3d;
typedef Vector <float, 4> RwV4d;

typedef SquareMatrix <float, 4> RwMatrix;

// Warning manager interface.
struct WarningManagerInterface abstract
{
    virtual void OnWarning( rwStaticString <wchar_t>&& message ) = 0;
};

// Software meta information provider struct.
struct softwareMetaInfo
{
    const char *applicationName;
    const char *applicationVersion;
    const char *description;
};

// Palettization configuration.
enum ePaletteRuntimeType
{
    PALRUNTIME_NATIVE,      // use the palettizer that is embedded into rwtools
    PALRUNTIME_PNGQUANT     // use the libimagequant vendor
};

// DXT compression configuration.
enum eDXTCompressionMethod
{
    DXTRUNTIME_NATIVE,      // prefer our own logic
    DXTRUNTIME_SQUISH       // prefer squish
};

typedef rwStaticMap <rwStaticString <wchar_t>, rwStaticString <wchar_t>, lexical_string_comparator <true>> languageTokenMap_t;

struct Interface abstract
{
protected:
    Interface( void ) = default;
    ~Interface( void ) = default;

public:
    void                SetVersion              ( LibraryVersion version );
    LibraryVersion      GetVersion              ( void ) const;

    // Give details about the running application to RenderWare.
    void                SetApplicationInfo      ( const softwareMetaInfo& metaInfo );
    void                SetMetaDataTagging      ( bool enabled );
    bool                GetMetaDataTagging      ( void ) const;

    void                SetFileInterface        ( FileInterface *fileIntf );
    FileInterface*      GetFileInterface        ( void );

    bool                RegisterStream          ( const char *typeName, size_t memBufSize, size_t memBufAlignment, customStreamInterface *streamInterface );
    Stream*             CreateStream            ( eBuiltinStreamType streamType, eStreamMode streamMode, streamConstructionParam_t *param );
    void                DeleteStream            ( Stream *theStream );

    // Serialization interface.
    void                SerializeBlock          ( RwObject *objectToStore, BlockProvider& outputProvider );
    void                Serialize               ( RwObject *objectToStore, Stream *outputStream );
    RwObject*           DeserializeBlock        ( BlockProvider& inputProvider );
    RwObject*           Deserialize             ( Stream *inputStream );

    // RwObject general API.
    RwObject*           ConstructRwObject       ( const char *typeName );       // construct any registered RwObject class
    RwObject*           CloneRwObject           ( const RwObject *srcObj );     // creates a new copy of an object
    void                DeleteRwObject          ( RwObject *obj );              // force kills an object (or decrements ref count)

    typedef rwStaticVector <rwStaticString <char>> rwobjTypeNameList_t;

    void                GetObjectTypeNames      ( rwobjTypeNameList_t& listOut ) const;
    bool                IsObjectRegistered      ( const char *typeName ) const;
    const char*         GetObjectTypeName       ( const RwObject *rwObj ) const;

    void                SerializeExtensions     ( const RwObject *rwObj, BlockProvider& outputProvider );
    void                DeserializeExtensions   ( RwObject *rwObj, BlockProvider& inputProvider );

    // Memory management.
    // * The regular methods throw an OutOfMemoryException if no memory could be allocated.
    // * The P methods return a nullptr on error instead.
    void*               MemAllocate             ( size_t memSize, size_t alignment = sizeof(void*) );
    void*               MemAllocateP            ( size_t memSize, size_t alignment = sizeof(void*) ) noexcept;
    bool                MemResize               ( void *ptr, size_t memSize ) noexcept;
    void                MemFree                 ( void *ptr ) noexcept;

    void*               PixelAllocate           ( size_t memSize, size_t alignment = sizeof(uint32) );
    void*               PixelAllocateP          ( size_t memSize, size_t alignment = sizeof(uint32) ) noexcept;
    bool                PixelResize             ( void *ptr, size_t memSize ) noexcept;
    void                PixelFree               ( void *pixels ) noexcept;

    void                SetWarningManager       ( WarningManagerInterface *warningMan );
    WarningManagerInterface*    GetWarningManager( void ) const;

    void                SetWarningLevel         ( int level );
    int                 GetWarningLevel         ( void ) const;

    void                SetIgnoreSecureWarnings ( bool doIgnore );
    bool                GetIgnoreSecureWarnings ( void ) const;

    // Warning system.
    void                PushWarningToken                ( const wchar_t *token );
    void                PushWarningDynamic              ( rwStaticString <wchar_t>&& localized_message );
    void                PushWarningSingleTemplate       ( const wchar_t *template_token, const wchar_t *tokenKey, const wchar_t *tokenValue );
    void                PushWarningTemplate             ( const wchar_t *template_token, const languageTokenMap_t& tokenMap );
    void                PushWarningObject               ( const RwObject *relevantObject, const wchar_t *token );
    void                PushWarningObjectSingleTemplate ( const RwObject *relevantObject, const wchar_t *template_token, const wchar_t *tokenKey, const wchar_t *tokenValue );

    bool                SetPaletteRuntime       ( ePaletteRuntimeType palRunType );
    ePaletteRuntimeType GetPaletteRuntime       ( void ) const;

    void                    SetDXTRuntime       ( eDXTCompressionMethod dxtRunType );
    eDXTCompressionMethod   GetDXTRuntime       ( void ) const;

    void                SetFixIncompatibleRasters   ( bool doFix );
    bool                GetFixIncompatibleRasters   ( void ) const;

    void                SetCompatTransformNativeImaging ( bool transfEnable );
    bool                GetCompatTransformNativeImaging ( void ) const;

    void                SetPreferPackedSampleExport ( bool preferPacked );
    bool                GetPreferPackedSampleExport ( void ) const;

    void                SetDXTPackedDecompression   ( bool packedDecompress );
    bool                GetDXTPackedDecompression   ( void ) const;

    void                SetIgnoreSerializationBlockRegions  ( bool doIgnore );
    bool                GetIgnoreSerializationBlockRegions  ( void ) const;

    void                    SetBlockAcquisitionMode     ( eBlockAcquisitionMode mode );
    eBlockAcquisitionMode   GetBlockAcquisitionMode     ( void ) const;
};

// Now implement the memory template(s).
IMPL_HEAP_REDIR_METH_ALLOCATE_RETURN RwDynMemAllocator::Allocate IMPL_HEAP_REDIR_METH_ALLOCATE_ARGS
{
    return this->engineInterface->MemAllocateP( memSize, alignment );
}
IMPL_HEAP_REDIR_METH_RESIZE_RETURN RwDynMemAllocator::Resize IMPL_HEAP_REDIR_METH_RESIZE_ARGS_DIRECT
{
    return this->engineInterface->MemResize( objMem, reqNewSize );
}
IMPL_HEAP_REDIR_METH_FREE_RETURN RwDynMemAllocator::Free IMPL_HEAP_REDIR_METH_FREE_ARGS
{
    this->engineInterface->MemFree( memPtr );
}

} // namespace rw

#include "renderware.utils.h"

namespace rw
{

// To create a RenderWare interface, you have to go through a constructor.
Interface*  CreateEngine( LibraryVersion engine_version );
void        DeleteEngine( Interface *theEngine );

// Configuration interface.
void AssignThreadedRuntimeConfig( Interface *engineInterface );
void ReleaseThreadedRuntimeConfig( Interface *engineInterface );

}

#include "renderware.qol.h"
#include "renderware.errsys.h"

#endif //_RENDERWARE_H_
