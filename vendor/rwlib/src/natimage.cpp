/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/natimage.cpp
*  PURPOSE:     Native image format implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "natimage.hxx"

#include "txdread.raster.hxx"

// This is the main core of the Native Image Format system.
// You can register your own fileformats into this, so that the framework will automatically use them.

// Known things to improve:
// * the allocation flag is based on all-mipmaps-at-once instead of single-mipmap, making
//   optimizations really awkward; we gotta change this to single-mipmap
//   also consider doing this for the rasters; parallel change.

namespace rw
{

NativeImagePrivate::NativeImagePrivate( EngineInterface *engineInterface )
{
    this->engineInterface = engineInterface;
    this->hasPaletteDataRef = false;
    this->hasPixelDataRef = false;
    this->pixelOwner = nullptr;

    this->externalRasterRef = false;
}

NativeImagePrivate::NativeImagePrivate( const NativeImagePrivate& right )
{
    EngineInterface *engineInterface = right.engineInterface;

    this->engineInterface = engineInterface;

    // Since we just take the pixel data pointers without copying, we increment the raster ref-count.
    this->hasPaletteDataRef = right.hasPaletteDataRef;
    this->hasPixelDataRef = right.hasPixelDataRef;

    this->pixelOwner = AcquireRaster( right.pixelOwner );

    // If we have a pixel owner, we also need a const reference.
    if ( Raster *pixelOwner = this->pixelOwner )
    {
        pixelOwner->addConstRef();
    }

    this->externalRasterRef = right.externalRasterRef;

    // Native data is cloned automatically.
}

NativeImagePrivate::~NativeImagePrivate( void )
{
    // Make sure that we have released all references by now.
    assert( this->hasPaletteDataRef == false );
    assert( this->hasPixelDataRef == false );
    assert( this->pixelOwner == nullptr );
}

optional_struct_space <nativeImageEnvRegister_t> nativeImageEnvRegister;

struct nativeImageTypeInterface : public RwTypeSystem::typeInterface
{
    AINLINE nativeImageTypeInterface( EngineInterface *engineInterface, nativeImageTypeManager *typeMan, size_t objSize, size_t objAlignment )
    {
        // objAlignment has to be a power-of-two.
        this->engineInterface = engineInterface;
        this->typeMan = typeMan;
        this->objSize = objSize;
        this->objAlignment = objAlignment;
    }

    static inline void* GetPrivateMemoryFromImageMemory( NativeImagePrivate *natImg, size_t natImgClassSize, size_t privAlign )
    {
        return GetObjectImplementationPointerForObject( natImg, natImgClassSize, privAlign );
    }

    void Construct( void *mem, EngineInterface *engineInterface, void *construct_params ) const override
    {
        nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetPluginStruct( engineInterface );

        struct native_img_constructor
        {
            EngineInterface *engineInterface;

            AINLINE NativeImagePrivate* Construct( void *mem ) const
            {
                return new (mem) NativeImagePrivate( engineInterface );
            }
        };

        native_img_constructor constr;
        constr.engineInterface = engineInterface;

        NativeImagePrivate *natImg = imgEnv->nativeImgFact.ConstructPlacementEx( mem, constr );

        try
        {
            void *nativeDataMem = GetPrivateMemoryFromImageMemory( natImg, imgEnv->nativeImgFact.GetClassSize(), this->objAlignment );

            this->typeMan->ConstructImage( engineInterface, nativeDataMem );
        }
        catch( ... )
        {
            natImg->~NativeImagePrivate();

            throw;
        }
    }

    void CopyConstruct( void *mem, const void *srcMem ) const override
    {
        EngineInterface *engineInterface = this->engineInterface;

        nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetPluginStruct( engineInterface );

        const NativeImagePrivate *srcImg = (const NativeImagePrivate*)srcMem;

        NativeImagePrivate *clonedImg = imgEnv->nativeImgFact.ClonePlacement( mem, srcImg );

        try
        {
            size_t natImgClassSize = imgEnv->nativeImgFact.GetClassSize();
            size_t objAlignment = this->objAlignment;

            const void *srcNativeData = GetPrivateMemoryFromImageMemory( (NativeImagePrivate*)srcImg, natImgClassSize, objAlignment );
            void *dstNativeData = GetPrivateMemoryFromImageMemory( clonedImg, natImgClassSize, objAlignment );

            this->typeMan->CopyConstructImage( engineInterface, dstNativeData, srcNativeData );
        }
        catch( ... )
        {
            clonedImg->~NativeImagePrivate();

            throw;
        }
    }

    void Destruct( void *mem ) const override
    {
        EngineInterface *engineInterface = this->engineInterface;

        nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetPluginStruct( engineInterface );

        NativeImagePrivate *natImg = (NativeImagePrivate*)mem;

        // Before destroying anything, we have to clear image data.
        natImg->clearImageData();

        // Destroy the native data.
        {
            void *nativeData = GetPrivateMemoryFromImageMemory( natImg, imgEnv->nativeImgFact.GetClassSize(), this->objAlignment );

            this->typeMan->DestroyImage( engineInterface, nativeData );
        }

        // Now the handle itself.
        imgEnv->nativeImgFact.DestroyPlacement( natImg );
    }

    size_t GetTypeSize( EngineInterface *engineInterface, void *construct_params ) const override final
    {
        const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface );

        size_t totalSize = imgEnv->nativeImgFact.GetClassSize();

        totalSize = ALIGN_SIZE( totalSize, this->objAlignment );
        totalSize += this->objSize;

        return totalSize;
    }

    size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *mem ) const override final
    {
        return this->GetTypeSize( engineInterface, nullptr );
    }

    size_t GetTypeAlignment( EngineInterface *engineInterface, void *construct_params ) const override final
    {
        const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface );

        return CalculateAlignmentForObjectWithHeader( this->objAlignment, imgEnv->nativeImgFact.GetClassAlignment() );
    }

    size_t GetTypeAlignmentByObject( EngineInterface *engineInterface, const void *mem ) const override final
    {
        return this->GetTypeAlignment( engineInterface, nullptr );
    }

    EngineInterface *engineInterface;
    nativeImageTypeManager *typeMan;
    size_t objSize;
    size_t objAlignment;
};

AINLINE static const void* CastToConstNativeImageTypeData( const NativeImagePrivate *imgPtr, const nativeImageEnv *imgEnv )
{
    return ( (const char*)imgPtr + imgEnv->nativeImgFact.GetClassSize() );
}

AINLINE static void* CastToNativeImageTypeData( NativeImagePrivate *imgPtr, const nativeImageEnv *imgEnv )
{
    return ( (char*)imgPtr + imgEnv->nativeImgFact.GetClassSize() );
}

AINLINE static nativeImageTypeManager* GetNativeImageTypeManager( EngineInterface *engineInterface, const NativeImagePrivate *imgPtr )
{
    const GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromConstObject( imgPtr );

    if ( rtObj )
    {
        RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

        nativeImageTypeInterface *nativeIntf = dynamic_cast <nativeImageTypeInterface*> ( typeInfo->tInterface );

        if ( nativeIntf )
        {
            return nativeIntf->typeMan;
        }
    }

    return nullptr;
}

// Private methods.
void NativeImagePrivate::clearImageData( void )
{
    // Clears all color data associated with this native image.
    // MUST BE CALLED UNDER write-lock.

    EngineInterface *engineInterface = this->engineInterface;

    nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetPluginStruct( engineInterface );

    void *nativeData = CastToNativeImageTypeData( this, imgEnv );

    nativeImageTypeManager *typeMan = GetNativeImageTypeManager( engineInterface, this );

    // Release all image data from this native image.
    bool shouldFreeNativePaletteData = ( this->hasPaletteDataRef == false );

    typeMan->ClearPaletteData( engineInterface, nativeData, shouldFreeNativePaletteData );

    bool shouldFreeNativeImageData = ( this->hasPixelDataRef == false );

    typeMan->ClearImageData( engineInterface, nativeData, shouldFreeNativeImageData );

    // Clear meta-data based on allocation tracking.
    this->hasPaletteDataRef = false;
    this->hasPixelDataRef = false;

    // Also release any raster lock.
    // Release the link to the raster and the const ref.
    if ( Raster *pixelOwner = this->pixelOwner )
    {
        if ( this->externalRasterRef == false )
        {
            pixelOwner->remConstRef();
        }

        DeleteRaster( pixelOwner );

        this->pixelOwner = nullptr;
    }

    // Some clean-up.
    this->externalRasterRef = false;
}

// Locks for the NativeImage object for thread-safety.
struct _getNatImgFactCallbackStructoid
{
    AINLINE static nativeImageEnv::nativeImgFact_t* getFactory( EngineInterface *engineInterface )
    {
        nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetPluginStruct( engineInterface );

        if ( imgEnv )
        {
            return &imgEnv->nativeImgFact;
        }

        return nullptr;
    }
};

typedef factLockProviderEnv <nativeImageEnv::nativeImgFact_t, _getNatImgFactCallbackStructoid> nativeImgLockEnv_t;

static optional_struct_space <PluginDependantStructRegister <nativeImgLockEnv_t, RwInterfaceFactory_t>> nativeImgLockRegister;

inline rwlock* GetNativeImageLock( EngineInterface *engineInterface, const NativeImagePrivate *nativeImg )
{
    nativeImgLockEnv_t *lockEnv = nativeImgLockRegister.get().GetPluginStruct( engineInterface );

    if ( lockEnv )
    {
        return lockEnv->GetLock( nativeImg );
    }

    return nullptr;
}

// Native image data API.
NativeImage* CreateNativeImage( Interface *intf, const char *typeName )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    // This function just creates an empty native image interface object.
    nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetPluginStruct( engineInterface );

    if ( imgEnv )
    {
        if ( RwTypeSystem::typeInfoBase *natImgBase = imgEnv->natImgType )
        {
            if ( RwTypeSystem::typeInfoBase *natImgType = engineInterface->typeSystem.FindTypeInfo( typeName, natImgBase ) )
            {
                // Create the native image interface.
                GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, natImgType, nullptr );

                if ( rtObj )
                {
                    // Return this amazing new object!
                    return (NativeImagePrivate*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
                }
            }
        }
    }

    return nullptr;
}

void DeleteNativeImage( NativeImage *imageHandle )
{
    EngineInterface *engineInterface = (EngineInterface*)imageHandle->getEngine();

    // We just delete this dynamic object.
    GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( imageHandle );

    if ( rtObj )
    {
        // This is good in debugging mode to find bugs in highly insecure code.
        // I doubt that any good code will fail that check.
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
    else
    {
        engineInterface->PushWarningToken( L"ENGINE_WARN_DELETEINVALIDNATIVETEX" );
    }
}

// NativeImage interface method implementations.
const char* NativeImage::getTypeName( void ) const
{
    const GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromConstObject( this );

    RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

    return typeInfo->name;
}

const char* NativeImage::getRecommendedNativeTextureTarget( void ) const
{
    const NativeImagePrivate *nativeImg = (const NativeImagePrivate*)this;

    EngineInterface *engineInterface = nativeImg->engineInterface;

    const nativeImageEnv *imageEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface );

    const void *nativeData = CastToConstNativeImageTypeData( nativeImg, imageEnv );

    nativeImageTypeManager *typeMan = GetNativeImageTypeManager( engineInterface, nativeImg );

    scoped_rwlock_reader <rwlock> ctxFetchImmutableProp( GetNativeImageLock( engineInterface, nativeImg ) );

    return typeMan->GetBestSupportedNativeTexture( engineInterface, nativeData );
}

const char* GetNativeImageTypeForStream( Stream *stream )
{
    // Return the type of the native image that matches the stream at the given position.
    EngineInterface *engineInterface = (EngineInterface*)stream->engineInterface;

    nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetPluginStruct( engineInterface );

    if ( imgEnv )
    {
        scoped_rwlock_reader <rwlock> ctxBrowseNativeImageTypes( imgEnv->lockImgFmtConsist );

        int64 streamObjectPos = stream->tell();

        bool needsReset = false;

        LIST_FOREACH_BEGIN( nativeImageTypeManager, imgEnv->formatsList.root, manData.node )

            if ( needsReset )
            {
                stream->seek( streamObjectPos, RWSEEK_BEG );

                needsReset = false;
            }

            // For each registered native image type, query if it is compatible.
            // The first one that is compatible with this stream is the image type that definately matches.
            bool isCompatible = item->IsStreamNativeImage( engineInterface, stream );

            needsReset = true;

            if ( isCompatible )
            {
                // We found a compatibility!
                // That means return that thing.
                stream->seek( streamObjectPos, RWSEEK_BEG );

                return item->manData.imgType->name;
            }

        LIST_FOREACH_END

        // Clean up by resetting the stream.
        if ( needsReset )
        {
            stream->seek( streamObjectPos, RWSEEK_BEG );
        }
    }

    // There is no native image type matching the contents of the stream.
    return nullptr;
}

inline bool DoesNativeImageTypeSupportNativeTexture_internal( const nativeImageTypeManager *imgTypeMan, const char *nativeTexName )
{
    size_t suppCount = imgTypeMan->manData.suppNatTexCount;

    for ( size_t n = 0; n < suppCount; n++ )
    {
        const char *suppNatTex = imgTypeMan->manData.suppNatTex[ n ].nativeTexName;

        // Note that in good practive, type names are checked case-sensitively for.

        if ( strcmp( nativeTexName, suppNatTex ) == 0 )
        {
            // We found a supported type!
            return true;
        }
    }

    // Found no support.
    return false;
}

void GetNativeImageTypesForNativeTexture( Interface *intf, const char *nativeTexName, nativeImageRasterResults_t& resultsOut )
{
    // Returns all native image types that are available to for creation out of
    // a given native texture type.

    EngineInterface *engineInterface = (EngineInterface*)intf;

    nativeImageRasterResults_t results;

    if ( nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetPluginStruct( engineInterface ) )
    {
        scoped_rwlock_reader <rwlock> ctxBrowseNativeImageTypes( imgEnv->lockImgFmtConsist );

        // Check all registered native image types.
        LIST_FOREACH_BEGIN( nativeImageTypeManager, imgEnv->formatsList.root, manData.node )

            bool doesSupport = DoesNativeImageTypeSupportNativeTexture_internal( item, nativeTexName );

            if ( doesSupport )
            {
                // We found a supported type!
                results.AddToBack( item->manData.imgType->name );
            }

        LIST_FOREACH_END
    }

    resultsOut = std::move( results );
}

bool DoesNativeImageSupportNativeTextureFriendly( Interface *intf, const char *nativeImageName, const char *nativeTexName )
{
    // Returns whether the native image type specified by nativeImageName supports output and input semantics in combination
    // with the native texture type nativeTexName.

    EngineInterface *engineInterface = (EngineInterface*)intf;

    if ( const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface ) )
    {
        // Get the native image type that was requested.
        if ( nativeImageTypeManager *natImgTypeMan = imgEnv->GetNativeImageTypeManagerByName( nativeImageName ) )
        {
            // We check if this native texture is among the supported.
            return DoesNativeImageTypeSupportNativeTexture_internal( natImgTypeMan, nativeTexName );
        }
    }

    // We found no support for this combination.
    return false;
}

const char* GetNativeImageTypeNameFromFriendlyName( Interface *intf, const char *nativeImageName )
{
    // Returns the type name from the friendly name.

    EngineInterface *engineInterface = (EngineInterface*)intf;

    if ( const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface ) )
    {
        if ( nativeImageTypeManager *imgTypeMan = imgEnv->GetNativeImageTypeManagerByName( nativeImageName ) )
        {
            return imgTypeMan->manData.imgType->name;
        }
    }

    // Could not detect a native image format with that name.
    return nullptr;
}

inline void _fillOutImageDescription( const nativeImageTypeManager *typeMan, registered_image_format& desc )
{
    // Fill out some cool stuff.
    desc.formatName = typeMan->manData.friendlyName;
    desc.num_ext = (uint32)typeMan->manData.fileExtCount;
    desc.ext_array = typeMan->manData.fileExtensions;
}

bool GetNativeImageInfo( Interface *intf, const char *nativeImageName, registered_image_format& infoOut )
{
    // Return format-specific meta-info about the requested native image type.

    EngineInterface *engineInterface = (EngineInterface*)intf;

    if ( const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface ) )
    {
        if ( nativeImageTypeManager *typeMan = imgEnv->GetNativeImageTypeManagerByName( nativeImageName ) )
        {
            _fillOutImageDescription( typeMan, infoOut );
            return true;
        }
    }

    return false;
}

bool IsNativeImageFormatAvailable( Interface *intf, const char *nativeImageName )
{
    // Return whether a native image format is ultimatively available on this RW engine.

    EngineInterface *engineInterface = (EngineInterface*)intf;

    if ( const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface ) )
    {
        if ( imgEnv->GetNativeImageTypeManagerByName( nativeImageName ) )
        {
            // We found something addressible.
            return true;
        }
    }

    return false;
}

void GetRegisteredNativeImageTypes( Interface *intf, registered_image_formats_t& formatsOut )
{
    // Return an array of image format information of all registered native image types.

    EngineInterface *engineInterface = (EngineInterface*)intf;

    if ( const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface ) )
    {
        LIST_FOREACH_BEGIN( nativeImageTypeManager, imgEnv->formatsList.root, manData.node )

            registered_image_format info;
            _fillOutImageDescription( item, info );

            formatsOut.AddToBack( std::move( info ) );

        LIST_FOREACH_END
    }
}

inline void NativeImageFetchFromRaster_internal(
    EngineInterface *engineInterface,
    nativeImageTypeManager *typeMan,
    NativeImagePrivate *nativeImg,
    void *nativeImageMem, Raster *raster, const char *nativeTexName,
    bool& needsRefOut
)
{
    // We push the native handle to the routine.
    void *nativeTex = raster->platformData;

    if ( !nativeTex )
    {
        throw RasterNotInitializedException( AcquireRaster( raster ), L"NATIVETEXTURE_FRIENDLYNAME" );
    }

    nativeImageTypeManager::acquireFeedback_t acquireFeedback;

    typeMan->ReadFromNativeTexture( engineInterface, nativeImageMem, nativeTexName, nativeTex, acquireFeedback );

    // Upon success, check how we need to treat the raster reference.
    // If we have taken any data from the raster we are now depending on it.
    // This has to be reflected in this interface.
    bool hasPaletteReference = acquireFeedback.hasDirectlyAcquiredPalette;
    bool hasMipmapReference = acquireFeedback.hasDirectlyAcquired;

    if ( hasPaletteReference || hasMipmapReference )
    {
        // Take a reference on this raster.
        nativeImg->pixelOwner = AcquireRaster( raster );

        // Keep the reference.
        needsRefOut = true;
    }

    nativeImg->hasPaletteDataRef = hasPaletteReference;
    nativeImg->hasPixelDataRef = hasMipmapReference;
}

void NativeImage::fetchFromRaster( Raster *raster )
{
    NativeImagePrivate *nativeImg = (NativeImagePrivate*)this;

    EngineInterface *engineInterface = nativeImg->engineInterface;

    const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface );

    void *nativeImageMem = CastToNativeImageTypeData( nativeImg, imgEnv );

    nativeImageTypeManager *typeMan = GetNativeImageTypeManager( engineInterface, nativeImg );

    scoped_rwlock_writer <rwlock> ctxReadFromRaster( GetNativeImageLock( engineInterface, nativeImg ) );

    // If there was any previous data in this native image, clear it.
    nativeImg->clearImageData();

    assert( nativeImg->pixelOwner == nullptr );

    // We need to keep some immutability across function calls for a moment.
    raster->addConstRef();

    bool needsRef = false;

    try
    {
        const char *nativeTexName = raster->getNativeDataTypeName();

        // Attempt to read from the raster into this native texture.
        // This can fail in many cases, we basically rely on the runtime creating a good dispatcher.
        {
            scoped_rwlock_reader <rwlock> ctxHandle( GetRasterLock( raster ) );

            NativeImageFetchFromRaster_internal(
                engineInterface,
                typeMan,
                nativeImg,
                nativeImageMem, raster, nativeTexName,
                needsRef
            );

            // We do not have an external reference.
            nativeImg->externalRasterRef = false;
        }
    }
    catch( ... )
    {
        raster->remConstRef();

        throw;
    }

    // Clean up the reference, if it is not required anymore.
    if ( !needsRef )
    {
        raster->remConstRef();
    }
}

void NativeImageFetchFromRasterNoLock( NativeImage *publicNatImg, Raster *raster, const char *nativeTexName, bool& needsRefOut )
{
    // Same as NativeImage::fetchFromRaster, but without lock to raster.
    // CALL ONLY UNDER read-lock OF raster !
    // You have to add a constant reference to raster before calling this method.

    NativeImagePrivate *nativeImg = (NativeImagePrivate*)publicNatImg;

    EngineInterface *engineInterface = nativeImg->engineInterface;

    const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface );

    void *nativeImageMem = CastToNativeImageTypeData( nativeImg, imgEnv );

    nativeImageTypeManager *typeMan = GetNativeImageTypeManager( engineInterface, nativeImg );

    scoped_rwlock_writer <rwlock> ctxReadFromRaster( GetNativeImageLock( engineInterface, nativeImg ) );

    // If there was any previous data in this native image, clear it.
    nativeImg->clearImageData();

    assert( nativeImg->pixelOwner == nullptr );

    bool needsRef = false;

    // We expect a read-lock here.
    NativeImageFetchFromRaster_internal(
        engineInterface,
        typeMan,
        nativeImg,
        nativeImageMem, raster, nativeTexName,
        needsRef
    );

    // We do have an external raster ref.
    nativeImg->externalRasterRef = true;

    // Tell the runtime about clean-up stuff.
    needsRefOut = needsRef;
}

inline void NativeImagePutToRaster_internal(
    EngineInterface *engineInterface,
    nativeImageTypeManager *typeMan,
    void *nativeImageMem, const Raster *raster
)
{
    void *nativeTex = raster->platformData;

    if ( !nativeTex )
    {
        throw RasterNotInitializedException( AcquireRaster( (Raster*)raster ), L"NATIVETEXTURE_FRIENDLYNAME" );
    }

    // Get the typeName of this native texture.
    const char *nativeTexName;
    {
        GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( nativeTex );

        RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

        nativeTexName = typeInfo->name;
    }

    texNativeTypeProvider *rasterTypeMan = GetNativeTextureTypeProvider( engineInterface, nativeTex );

    // Clear the raster from any previous data.
    rasterTypeMan->UnsetPixelDataFromTexture( engineInterface, nativeTex, true );

    // Push the image data from our native image to the raster.
    nativeImageTypeManager::acquireFeedback_t acquireFeedback;

    typeMan->WriteToNativeTexture( engineInterface, nativeImageMem, nativeTexName, nativeTex, acquireFeedback );

    // So, how has the color information been taken by the raster?
    bool isPaletteDataRef = acquireFeedback.hasDirectlyAcquiredPalette;
    bool isMipmapDataRef = acquireFeedback.hasDirectlyAcquired;

    // Since the image data was not owned by any other raster, we can clear it.
    typeMan->ClearPaletteData( engineInterface, nativeImageMem, isPaletteDataRef == false );
    typeMan->ClearImageData( engineInterface, nativeImageMem, isMipmapDataRef == false );
}

void NativeImage::putToRaster( Raster *raster )
{
    // In this routine we try to give up our color data into the given raster.
    // NativeImage is meant to be a light interface between native texture and native image anyway.

    // General-purpose public method.

    NativeImagePrivate *nativeImg = (NativeImagePrivate*)this;

    EngineInterface *engineInterface = nativeImg->engineInterface;

    const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface );

    void *nativeImageMem = CastToNativeImageTypeData( nativeImg, imgEnv );

    nativeImageTypeManager *typeMan = GetNativeImageTypeManager( engineInterface, nativeImg );

    scoped_rwlock_writer <rwlock> ctxPutToRaster( GetNativeImageLock( engineInterface, nativeImg ) );

    // If the color data that we own already belongs to a raster, we cannot continue.
    // This might just be a temporary thing if I decide to improve this API.
    if ( nativeImg->pixelOwner != nullptr )
    {
        throw NativeImagingInvalidOperationException( typeMan->manData.imgType->name, eOperationType::MOVE, L"NATIVEIMG_REASON_IMGDATA_ALREADY_OWNED" );
    }

    {
        scoped_rwlock_writer <rwlock> ctxWriteToRaster( GetRasterLock( raster ) );

        NativeImagePutToRaster_internal( engineInterface, typeMan, nativeImageMem, raster );
    }
}

void NativeImagePutToRasterNoLock( NativeImage *publicNatImg, Raster *raster )
{
    // Same as the NativeImage::putToRaster method, but does not lock the raster.
    // CALL THIS METHOD ONLY UNDER A write-lock OF raster !

    NativeImagePrivate *nativeImg = (NativeImagePrivate*)publicNatImg;

    EngineInterface *engineInterface = nativeImg->engineInterface;

    const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface );

    void *nativeImageMem = CastToNativeImageTypeData( nativeImg, imgEnv );

    nativeImageTypeManager *typeMan = GetNativeImageTypeManager( engineInterface, nativeImg );

    scoped_rwlock_writer <rwlock> ctxPutToRasterNoLock( GetNativeImageLock( engineInterface, nativeImg ) );

    // If the color data that we own already belongs to a raster, we cannot continue.
    // This might just be a temporary thing if I decide to improve this API.
    if ( nativeImg->pixelOwner != nullptr )
    {
        throw NativeImagingInvalidOperationException( typeMan->manData.imgType->name, eOperationType::MOVE, L"NATIVEIMG_REASON_IMGDATA_ALREADY_OWNED" );
    }

    NativeImagePutToRaster_internal( engineInterface, typeMan, nativeImageMem, raster );
}

void NativeImage::readFromStream( Stream *stream )
{
    // Reads the stream for an expected NativeImage type.
    // This function does not reset the stream on exception.

    NativeImagePrivate *nativeImg = (NativeImagePrivate*)this;

    EngineInterface *engineInterface = nativeImg->engineInterface;

    const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface );

    void *nativeImageMem = CastToNativeImageTypeData( nativeImg, imgEnv );

    nativeImageTypeManager *typeMan = GetNativeImageTypeManager( engineInterface, nativeImg );

    scoped_rwlock_writer <rwlock> ctxDeserializeStream( GetNativeImageLock( engineInterface, nativeImg ) );

    // As always, clean up any dat that was there before.
    nativeImg->clearImageData();

    // Now read the stream.
    typeMan->ReadNativeImage( engineInterface, nativeImageMem, stream );

    // We have our own color references now.
    nativeImg->hasPaletteDataRef = false;
    nativeImg->hasPixelDataRef = false;
}

void NativeImage::writeToStream( Stream *stream )
{
    // Writes the memory of a native image into a stream.
    // This function does not reset the stream on exception.

    NativeImagePrivate *nativeImg = (NativeImagePrivate*)this;

    EngineInterface *engineInterface = nativeImg->engineInterface;

    const nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetConstPluginStruct( engineInterface );

    void *nativeImageMem = CastToNativeImageTypeData( nativeImg, imgEnv );

    nativeImageTypeManager *typeMan = GetNativeImageTypeManager( engineInterface, nativeImg );

    scoped_rwlock_reader <rwlock> ctxSerializeStream( GetNativeImageLock( engineInterface, nativeImg ) );

    // Just serialize things.
    typeMan->WriteNativeImage( engineInterface, nativeImageMem, stream );
}

Interface* NativeImage::getEngine( void ) const
{
    // This function does not need a lock, because the engine is an immutable property.

    NativeImagePrivate *nativeImg = (NativeImagePrivate*)this;

    return nativeImg->engineInterface;
}

bool RegisterNativeImageType(
    EngineInterface *engineInterface,
    nativeImageTypeManager *typeManager,
    const char *typeName, size_t memSize, size_t memAlign, const char *friendlyName,
    const imaging_filename_ext *fileExtensions, size_t fileExtCount,
    const natimg_supported_native_desc *suppNatTex, size_t suppNatTexCount
)
{
    bool success = false;

    nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetPluginStruct( engineInterface );

    if ( imgEnv )
    {
        scoped_rwlock_writer <rwlock> ctxRegisterType( imgEnv->lockImgFmtConsist );

        // We do not want to be registered already.
        if ( typeManager->manData.isRegistered == false )
        {
            // We can only register stuff if we actually got to construct our type.
            if ( RwTypeSystem::typeInfoBase *imgBaseType = imgEnv->natImgType )
            {
                try
                {
                    // We want a type interface that describes how to create our native imaging format container.

                    // Register the native image object type.
                    RwTypeSystem::typeInfoBase *nativeImgType =
                        engineInterface->typeSystem.RegisterCommonTypeInterface <nativeImageTypeInterface> (
                            typeName, imgBaseType, engineInterface, typeManager, memSize, memAlign
                        );

                    // By using RegisterCommonTypeInterface, the nativeImageTypeInterface will be cleaned up automatically
                    // when deleting the type.

                    if ( nativeImgType )
                    {
                        // Put us into the system!
                        typeManager->manData.imgType = nativeImgType;
                        typeManager->manData.friendlyName = friendlyName;
                        typeManager->manData.fileExtensions = fileExtensions;
                        typeManager->manData.fileExtCount = fileExtCount;
                        typeManager->manData.suppNatTex = suppNatTex;
                        typeManager->manData.suppNatTexCount = suppNatTexCount;

                        LIST_INSERT( imgEnv->formatsList.root, typeManager->manData.node );

                        success = true;
                    }
                }
                catch( RwTypeSystem::type_name_conflict_exception& )
                {
                    // There was a problem registering the native image because the name
                    // is already taken.
                    return false;
                }
            }
        }
    }

    return success;
}

bool UnregisterNativeImageType(
    EngineInterface *engineInterface,
    const char *typeName
)
{
    bool success = false;

    nativeImageEnv *imgEnv = nativeImageEnvRegister.get().GetPluginStruct( engineInterface );

    if ( imgEnv )
    {
        scoped_rwlock_writer <rwlock> ctxUnregisterType( imgEnv->lockImgFmtConsist );

        if ( RwTypeSystem::typeInfoBase *imgBaseType = imgEnv->natImgType )
        {
            // Delete the type registration, if it exists.
            if ( RwTypeSystem::typeInfoBase *natImgType = engineInterface->typeSystem.FindTypeInfo( typeName, imgBaseType ) )
            {
                nativeImageTypeInterface *natIntf = dynamic_cast <nativeImageTypeInterface*> ( natImgType->tInterface );

                if ( natIntf )
                {
                    // Unregister us.
                    nativeImageTypeManager *typeMan = natIntf->typeMan;

                    LIST_REMOVE( typeMan->manData.node );

                    typeMan->manData.isRegistered = false;

                    // Delete our type.
                    engineInterface->typeSystem.DeleteType( natImgType );

                    success = true;
                }
            }
        }
    }

    return success;
}

// Native image formats.
#ifdef RWLIB_INCLUDE_DDS_NATIVEIMG
extern void registerDDSNativeImageFormatEnv( void );
extern void unregisterDDSNativeImageFormatEnv( void );
#endif //RWLIB_INCLUDE_DDS_NATIVEIMG
#ifdef RWLIB_INCLUDE_PVR_NATIVEIMG
extern void registerPVRNativeImageTypeEnv( void );
extern void unregisterPVRNativeImageTypeEnv( void );
#endif //RWLIB_INCLUDE_PVR_NATIVEIMG

void registerNativeImagePluginEnvironment( void )
{
    // Register the main environment.
    nativeImageEnvRegister.Construct( engineFactory );

    // Sub extensions for the native image type.
    nativeImgLockRegister.Construct( engineFactory );

    // TODO: register all native image formats here.
#ifdef RWLIB_INCLUDE_DDS_NATIVEIMG
    registerDDSNativeImageFormatEnv();
#endif //RWLIB_INCLUDE_DDS_NATIVEIMG
#ifdef RWLIB_INCLUDE_PVR_NATIVEIMG
    registerPVRNativeImageTypeEnv();
#endif //RWLIB_INCLUDE_PVR_NATIVEIMG
}

void unregisterNativeImagePluginEnvironment( void )
{
#ifdef RWLIB_INCLUDE_PVR_NATIVEIMG
    unregisterPVRNativeImageTypeEnv();
#endif //RWLIB_INCLUDE_PVR_NATIVEIMG
#ifdef RWLIB_INCLUDE_DDS_NATIVEIMG
    unregisterDDSNativeImageFormatEnv();
#endif //RWLIB_INCLUDE_DDS_NATIVEIMG

    nativeImgLockRegister.Destroy();
    nativeImageEnvRegister.Destroy();
}

};
