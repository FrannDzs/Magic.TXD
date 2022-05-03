/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.errsys.h
*  PURPOSE:     Error system exception definitions.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_ERROR_SYSTEM_HEADER_
#define _RENDERWARE_ERROR_SYSTEM_HEADER_

#include "renderware.errsys.common.h"
#include "renderware.errsys.ext.h"

#include <utility>

namespace rw
{

// Return a string representation of an exception object.
rwStaticString <wchar_t> DescribeException( Interface *rwEngine, const RwException& except );

//====================================================
// *** Subsystem describing exception classes ***
//====================================================

struct ImagingException : public virtual RwException
{
    inline ImagingException( const char *imageFormatName ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::IMAGING ), imageFormatName( imageFormatName )
    {
        return;
    }

    inline ImagingException( ImagingException&& ) = default;
    inline ImagingException( const ImagingException& ) = default;

    inline ImagingException& operator = ( ImagingException&& ) = default;
    inline ImagingException& operator = ( const ImagingException& ) = default;

    inline const char* getImageFormatName( void ) const noexcept
    {
        return this->imageFormatName;
    }

private:
    const char *imageFormatName;
};

struct NativeTextureException : public virtual RwException
{
    inline NativeTextureException( const char *nativeTextureName ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::NATIVE_TEXTURE ), nativeTextureName( nativeTextureName )
    {
        return;
    }

    inline NativeTextureException( NativeTextureException&& ) = default;
    inline NativeTextureException( const NativeTextureException& ) = default;

    inline NativeTextureException& operator = ( NativeTextureException&& ) = default;
    inline NativeTextureException& operator = ( const NativeTextureException& ) = default;

    inline const char* getNativeTextureName( void ) const noexcept
    {
        return this->nativeTextureName;
    }

private:
    const char *nativeTextureName;
};

struct RasterException : public virtual RwException
{
    inline RasterException( RasterPtr relevantRaster ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::RASTER ), relevantRaster( ::std::move( relevantRaster ) )
    {
        return;
    }

    inline RasterException( RasterException&& ) = default;
    inline RasterException( const RasterException& ) = default;

    inline RasterException& operator = ( RasterException&& ) = default;
    inline RasterException& operator = ( const RasterException& ) = default;

    inline const RasterPtr& getRelevantRaster( void ) const noexcept
    {
        return this->relevantRaster;
    }

private:
    RasterPtr relevantRaster;
};

struct DriverException : public virtual RwException
{
    inline DriverException( const char *driverName ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::DRIVER ), driverName( driverName )
    {
        return;
    }

    inline DriverException( DriverException&& ) = default;
    inline DriverException( const DriverException& ) = default;

    inline DriverException& operator = ( DriverException&& ) = default;
    inline DriverException& operator = ( const DriverException& ) = default;

    inline const char* getDriverName( void ) const noexcept
    {
        return this->driverName;
    }

private:
    const char *driverName;
};

struct DriverProgramException : public virtual RwException
{
    inline DriverProgramException( const char *progType ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::DRIVERPROG ), progType( progType )
    {
        return;
    }

    inline DriverProgramException( DriverProgramException&& ) = default;
    inline DriverProgramException( const DriverProgramException& ) = default;

    inline DriverProgramException& operator = ( DriverProgramException&& ) = default;
    inline DriverProgramException& operator = ( const DriverProgramException& ) = default;

    inline const char* getDriverProgramType( void ) const noexcept
    {
        return this->progType;
    }

private:
    const char *progType;
};

struct NativeImagingException : public virtual RwException
{
    inline NativeImagingException( const char *nativeImageFormatName ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::NATIVE_IMAGING ), nativeImageFormatName( nativeImageFormatName )
    {
        return;
    }

    inline NativeImagingException( NativeImagingException&& ) = default;
    inline NativeImagingException( const NativeImagingException& ) = default;

    inline NativeImagingException& operator = ( NativeImagingException&& ) = default;
    inline NativeImagingException& operator = ( const NativeImagingException& ) = default;

    inline const char* getNativeImageFormatName( void ) const noexcept
    {
        return this->nativeImageFormatName;
    }

private:
    const char *nativeImageFormatName;
};

struct PaletteException : public virtual RwException
{
    inline PaletteException( ePaletteRuntimeType palContext ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::PALETTE ), palContext( palContext )
    {
        return;
    }

    inline PaletteException( PaletteException&& ) = default;
    inline PaletteException( const PaletteException& ) = default;

    inline PaletteException& operator = ( PaletteException&& ) = default;
    inline PaletteException& operator = ( const PaletteException& ) = default;

    inline ePaletteRuntimeType getPaletteContext( void ) const noexcept
    {
        return this->palContext;
    }

private:
    ePaletteRuntimeType palContext;
};

struct TypeSystemException : public virtual RwException
{
    inline TypeSystemException( const char *relevantTypeName ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::TYPESYSTEM ), relevantTypeName( relevantTypeName )
    {
        return;
    }

    inline TypeSystemException( TypeSystemException&& ) = default;
    inline TypeSystemException( const TypeSystemException& ) = default;

    inline TypeSystemException& operator = ( TypeSystemException&& ) = default;
    inline TypeSystemException& operator = ( const TypeSystemException& ) = default;

    inline const char* getRelevantTypeName( void ) const noexcept
    {
        return this->relevantTypeName;
    }

private:
    const char *relevantTypeName;
};

struct RwObjectsException : public virtual RwException
{
    inline RwObjectsException( ObjectPtr <RwObject> relevantObject ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::RWOBJECTS ), relevantObject( ::std::move( relevantObject ) )
    {
        return;
    }

    inline RwObjectsException( RwObjectsException&& ) = default;
    inline RwObjectsException( const RwObjectsException& ) = default;

    inline RwObjectsException& operator = ( RwObjectsException&& ) = default;
    inline RwObjectsException& operator = ( const RwObjectsException& ) = default;

    inline const ObjectPtr <RwObject>& getRelevantObject( void ) const noexcept
    {
        return this->relevantObject;
    }

private:
    ObjectPtr <RwObject> relevantObject;
};

struct BlockAPIException : public virtual RwException
{
    inline BlockAPIException( uint64 faultBlockOffset, uint32 faultBlockID ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::BLOCKAPI ), faultBlockOffset( faultBlockOffset ), faultBlockID( faultBlockID )
    {
        return;
    }

    inline BlockAPIException( BlockAPIException&& ) = default;
    inline BlockAPIException( const BlockAPIException& ) = default;

    inline BlockAPIException& operator = ( BlockAPIException&& ) = default;
    inline BlockAPIException& operator = ( const BlockAPIException& ) = default;

    inline uint64 getFaultBlockOffset( void ) const noexcept
    {
        return this->faultBlockOffset;
    }

    inline uint32 getFaultBlockID( void ) const noexcept
    {
        return this->faultBlockID;
    }

private:
    uint64 faultBlockOffset;
    uint32 faultBlockID;
};

//====================================================================
// *** Specialized exception classes thrown in subsystem contexts ***
//====================================================================

#define RWEXCEPT_IMAGING_ARGUMENTS_DEFINE               const char *imageFormatName
#define RWEXCEPT_IMAGING_ARGUMENTS_USE                  imageFormatName
#define RWEXCEPT_NATIVE_TEXTURE_ARGUMENTS_DEFINE        const char *nativeTextureName
#define RWEXCEPT_NATIVE_TEXTURE_ARGUMENTS_USE           nativeTextureName
#define RWEXCEPT_RASTER_ARGUMENTS_DEFINE                RasterPtr relevantRaster
#define RWEXCEPT_RASTER_ARGUMENTS_USE                   ::std::move( relevantRaster )
#define RWEXCEPT_DRIVER_ARGUMENTS_DEFINE                const char *driverName
#define RWEXCEPT_DRIVER_ARGUMENTS_USE                   driverName
#define RWEXCEPT_DRIVERPROG_ARGUMENTS_DEFINE            const char *progType
#define RWEXCEPT_DRIVERPROG_ARGUMENTS_USE               progType
#define RWEXCEPT_NATIVE_IMAGING_ARGUMENTS_DEFINE        const char *nativeImageFormatName
#define RWEXCEPT_NATIVE_IMAGING_ARGUMENTS_USE           nativeImageFormatName
#define RWEXCEPT_PALETTE_ARGUMENTS_DEFINE               ePaletteRuntimeType palContext
#define RWEXCEPT_PALETTE_ARGUMENTS_USE                  palContext
#define RWEXCEPT_TYPESYSTEM_ARGUMENTS_DEFINE            const char *relevantType
#define RWEXCEPT_TYPESYSTEM_ARGUMENTS_USE               relevantType
#define RWEXCEPT_RWOBJECTS_ARGUMENTS_DEFINE             ObjectPtr <RwObject> relevantObject
#define RWEXCEPT_RWOBJECTS_ARGUMENTS_USE                ::std::move( relevantObject )
#define RWEXCEPT_BLOCKAPI_ARGUMENTS_DEFINE              uint64 faultBlockOffset, uint32 faultBlockID
#define RWEXCEPT_BLOCKAPI_ARGUMENTS_USE                 faultBlockOffset, faultBlockID

// ** NotInitializedException
RW_EXCEPTION_TEMPLATE( NotInitialized, Raster, RASTER, NOTINIT );
RW_EXCEPTION_TEMPLATE( NotInitialized, RwObjects, RWOBJECTS, NOTINIT );

RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS( NotInitialized, Serialization, SERIALIZATION, Raster, RASTER, NOTINIT );

// ** UnsupportedOperationException
RW_EXCEPTION_TEMPLATE_2( UnsupportedOperation, Imaging, IMAGING, UNSUPPOP );
RW_EXCEPTION_TEMPLATE_2( UnsupportedOperation, NativeTexture, NATIVE_TEXTURE, UNSUPPOP );
RW_EXCEPTION_TEMPLATE_2( UnsupportedOperation, Raster, RASTER, UNSUPPOP );
RW_EXCEPTION_TEMPLATE_2( UnsupportedOperation, Driver, DRIVER, UNSUPPOP );
RW_EXCEPTION_TEMPLATE_2( UnsupportedOperation, DriverProgram, DRIVERPROG, UNSUPPOP );
RW_EXCEPTION_TEMPLATE_2( UnsupportedOperation, NativeImaging, NATIVE_IMAGING, UNSUPPOP );
RW_EXCEPTION_TEMPLATE_2( UnsupportedOperation, Palette, PALETTE, UNSUPPOP );
RW_EXCEPTION_TEMPLATE_2( UnsupportedOperation, TypeSystem, TYPESYSTEM, UNSUPPOP );
RW_EXCEPTION_TEMPLATE_2( UnsupportedOperation, RwObjects, RWOBJECTS, UNSUPPOP );

RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS_2( UnsupportedOperation, Palette, PALETTE, Raster, RASTER, UNSUPPOP );

// ** InvalidConfigurationException
RW_EXCEPTION_TEMPLATE( InvalidConfiguration, Imaging, IMAGING, INVALIDCFG );
RW_EXCEPTION_TEMPLATE( InvalidConfiguration, NativeTexture, NATIVE_TEXTURE, INVALIDCFG );
RW_EXCEPTION_TEMPLATE( InvalidConfiguration, Raster, RASTER, INVALIDCFG );
RW_EXCEPTION_TEMPLATE( InvalidConfiguration, Driver, DRIVER, INVALIDCFG );
RW_EXCEPTION_TEMPLATE( InvalidConfiguration, DriverProgram, DRIVERPROG, INVALIDCFG );
RW_EXCEPTION_TEMPLATE( InvalidConfiguration, NativeImaging, NATIVE_IMAGING, INVALIDCFG );
RW_EXCEPTION_TEMPLATE( InvalidConfiguration, Palette, PALETTE, INVALIDCFG );
RW_EXCEPTION_TEMPLATE( InvalidConfiguration, TypeSystem, TYPESYSTEM, INVALIDCFG );
RW_EXCEPTION_TEMPLATE( InvalidConfiguration, RwObjects, RWOBJECTS, INVALIDCFG );

RW_EXCEPTION_2BASE_TEMPLATE( InvalidConfiguration, NativeImaging, NativeTexture, IMAGING, NATIVE_TEXTURE, INVALIDCFG );
RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS( InvalidConfiguration, Resizing, RESIZING, Raster, RASTER, INVALIDCFG );
RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS( InvalidConfiguration, Serialization, SERIALIZATION, RwObjects, RWOBJECTS, INVALIDCFG );

// ** StructuralErrorException
RW_EXCEPTION_TEMPLATE( StructuralError, Imaging, IMAGING, STRUCTERR );
RW_EXCEPTION_TEMPLATE( StructuralError, NativeTexture, NATIVE_TEXTURE, STRUCTERR );
RW_EXCEPTION_TEMPLATE( StructuralError, Driver, DRIVER, STRUCTERR );
RW_EXCEPTION_TEMPLATE( StructuralError, DriverProgram, DRIVERPROG, STRUCTERR );
RW_EXCEPTION_TEMPLATE( StructuralError, NativeImaging, NATIVE_IMAGING, STRUCTERR );
RW_EXCEPTION_TEMPLATE( StructuralError, RwObjects, RWOBJECTS, STRUCTERR );

RW_EXCEPTION_2BASE_TEMPLATE( StructuralError, NativeTexture, RwObjects, NATIVE_TEXTURE, RWOBJECTS, STRUCTERR );

// ** InvalidOperationException
RW_EXCEPTION_TEMPLATE_2( InvalidOperation, Imaging, IMAGING, INVALIDOP );
RW_EXCEPTION_TEMPLATE_2( InvalidOperation, NativeTexture, NATIVE_TEXTURE, INVALIDOP );
RW_EXCEPTION_TEMPLATE_2( InvalidOperation, Raster, RASTER, INVALIDOP );
RW_EXCEPTION_TEMPLATE_2( InvalidOperation, Driver, DRIVER, INVALIDOP );
RW_EXCEPTION_TEMPLATE_2( InvalidOperation, DriverProgram, DRIVERPROG, INVALIDOP );
RW_EXCEPTION_TEMPLATE_2( InvalidOperation, NativeImaging, NATIVE_IMAGING, INVALIDOP );
RW_EXCEPTION_TEMPLATE_2( InvalidOperation, Palette, PALETTE, INVALIDOP );
RW_EXCEPTION_TEMPLATE_2( InvalidOperation, TypeSystem, TYPESYSTEM, INVALIDOP );
RW_EXCEPTION_TEMPLATE_2( InvalidOperation, RwObjects, RWOBJECTS, INVALIDOP );

RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS_2( InvalidOperation, Palette, PALETTE, Raster, RASTER, INVALIDOP );

// ** InvalidParameterException
RW_EXCEPTION_TEMPLATE( InvalidParameter, Imaging, IMAGING, INVALIDPARAM );
RW_EXCEPTION_TEMPLATE( InvalidParameter, NativeTexture, NATIVE_TEXTURE, INVALIDPARAM );
RW_EXCEPTION_TEMPLATE( InvalidParameter, Raster, RASTER, INVALIDPARAM );
RW_EXCEPTION_TEMPLATE( InvalidParameter, Driver, DRIVER, INVALIDPARAM );
RW_EXCEPTION_TEMPLATE( InvalidParameter, DriverProgram, DRIVERPROG, INVALIDPARAM );
RW_EXCEPTION_TEMPLATE( InvalidParameter, NativeImaging, NATIVE_IMAGING, INVALIDPARAM );
RW_EXCEPTION_TEMPLATE( InvalidParameter, Palette, PALETTE, INVALIDPARAM );
RW_EXCEPTION_TEMPLATE( InvalidParameter, TypeSystem, TYPESYSTEM, INVALIDPARAM );
RW_EXCEPTION_TEMPLATE( InvalidParameter, RwObjects, RWOBJECTS, INVALIDPARAM );

RW_EXCEPTION_2BASE_TEMPLATE( InvalidParameter, NativeImaging, NativeTexture, NATIVE_IMAGING, NATIVE_TEXTURE, INVALIDPARAM );
RW_EXCEPTION_2BASE_TEMPLATE( InvalidParameter, Driver, Raster, DRIVER, RASTER, INVALIDPARAM );

RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS( InvalidParameter, Palette, PALETTE, Raster, RASTER, INVALIDPARAM );

// ** ResourcesExhaustedException
RW_EXCEPTION_TEMPLATE( ResourcesExhausted, Driver, DRIVER, RESEXHAUST );
RW_EXCEPTION_TEMPLATE( ResourcesExhausted, RwObjects, RWOBJECTS, RESEXHAUST );

// ** InternalErrorException
RW_EXCEPTION_TEMPLATE( InternalError, Imaging, IMAGING, INTERNERR );
RW_EXCEPTION_TEMPLATE( InternalError, NativeTexture, NATIVE_TEXTURE, INTERNERR );
RW_EXCEPTION_TEMPLATE( InternalError, Raster, RASTER, INTERNERR );
RW_EXCEPTION_TEMPLATE( InternalError, Driver, DRIVER, INTERNERR );
RW_EXCEPTION_TEMPLATE( InternalError, DriverProgram, DRIVERPROG, INTERNERR );
RW_EXCEPTION_TEMPLATE( InternalError, NativeImaging, NATIVE_IMAGING, INTERNERR );
RW_EXCEPTION_TEMPLATE( InternalError, Palette, PALETTE, INTERNERR );
RW_EXCEPTION_TEMPLATE( InternalError, TypeSystem, TYPESYSTEM, INTERNERR );
RW_EXCEPTION_TEMPLATE( InternalError, RwObjects, RWOBJECTS, INTERNERR );

RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS( InternalError, Serialization, SERIALIZATION, TypeSystem, TYPESYSTEM, INTERNERR );
RW_EXCEPTION_2BASE_TEMPLATE( InternalError, NativeTexture, RwObjects, NATIVE_TEXTURE, RWOBJECTS, INTERNERR );
RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS( InternalError, Serialization, SERIALIZATION, RwObjects, RWOBJECTS, INTERNERR );
RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS( InternalError, Serialization, SERIALIZATION, Raster, RASTER, INTERNERR );

// ** UnlocalizedAInternalErrorException
RW_EXCEPTION_TEMPLATE( UnlocalizedAInternalError, Imaging, IMAGING, UNLOCALAINTERNERR );
RW_EXCEPTION_TEMPLATE( UnlocalizedAInternalError, Driver, DRIVER, UNLOCALAINTERNERR );
RW_EXCEPTION_TEMPLATE( UnlocalizedAInternalError, DriverProgram, DRIVERPROG, UNLOCALAINTERNERR );

// ** UnlocalizedWInternalErrorException
RW_EXCEPTION_TEMPLATE( UnlocalizedWInternalError, Imaging, IMAGING, UNLOCALWINTERNERR );

// ** NotFoundException
RW_EXCEPTION_TEMPLATE( NotFound, Imaging, IMAGING, NOTFOUND );
RW_EXCEPTION_TEMPLATE( NotFound, NativeTexture, NATIVE_TEXTURE, NOTFOUND );
RW_EXCEPTION_TEMPLATE( NotFound, Raster, RASTER, NOTFOUND );
RW_EXCEPTION_TEMPLATE( NotFound, Driver, DRIVER, NOTFOUND );
RW_EXCEPTION_TEMPLATE( NotFound, DriverProgram, DRIVERPROG, NOTFOUND );
RW_EXCEPTION_TEMPLATE( NotFound, NativeImaging, NATIVE_IMAGING, NOTFOUND );
RW_EXCEPTION_TEMPLATE( NotFound, Palette, PALETTE, NOTFOUND );
RW_EXCEPTION_TEMPLATE( NotFound, TypeSystem, TYPESYSTEM, NOTFOUND );
RW_EXCEPTION_TEMPLATE( NotFound, RwObjects, RWOBJECTS, NOTFOUND );

// ** UnexpectedChunkException
RW_EXCEPTION_TEMPLATE( UnexpectedChunk, BlockAPI, BLOCKAPI, UNEXPCHUNK );

} // namespace rw

#endif //_RENDERWARE_ERROR_SYSTEM_HEADER_