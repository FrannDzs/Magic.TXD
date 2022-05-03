/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/natimage.hxx
*  PURPOSE:
*       RenderWare native image format support (.DDS, .PVR, .TM2, .GVR, etc).
*       Native image formats are extended basic image formats that additionally come with mipmaps and, possibly, rendering properties.
*       While native imaging formats are more complicated they map stronger to native textures.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "pluginutil.hxx"

#include <sdk/UniChar.h>

namespace rw
{

// Structure to store a supported native texture entry in.
struct natimg_supported_native_desc
{
    const char *nativeTexName;
};

// Virtual interface for native image types.
struct nativeImageTypeManager abstract
{
    inline nativeImageTypeManager( void )
    {
        this->manData.isRegistered = false;
    }

    inline ~nativeImageTypeManager( void )
    {
        // Make sure we aint registered.
        assert( this->manData.isRegistered == false );
    }

    // Basic object management.
    virtual void ConstructImage( Interface *engineInterface, void *imageMem ) const = 0;
    virtual void CopyConstructImage( Interface *engineInterface, void *imageMem, const void *srcImageMem ) const = 0;
    virtual void DestroyImage( Interface *engineInterface, void *imageMem ) const = 0;

    struct acquireFeedback_t
    {
        bool hasDirectlyAcquiredPalette;    // a native image does not have to support palette.
        bool hasDirectlyAcquired;
    };

    // Description API.
    virtual const char* GetBestSupportedNativeTexture( Interface *engineInterface, const void *imageMem ) const = 0;

    // Pixel movement.
    virtual void ClearImageData( Interface *engineInterface, void *imageMem, bool deallocate ) const = 0;
    virtual void ClearPaletteData( Interface *engineInterface, void *imageMem, bool deallocate ) const = 0;

    virtual void ReadFromNativeTexture( Interface *engineInterface, void *imageMem, const char *nativeTexType, void *nativeTexMem, acquireFeedback_t& feedbackOut ) const = 0;
    virtual void WriteToNativeTexture( Interface *engineInterface, void *imageMem, const char *nativeTexType, void *nativeTexMem, acquireFeedback_t& feedbackOut ) const = 0;

    virtual bool IsStreamNativeImage( Interface *engineInterface, Stream *imgStream ) const = 0;
    virtual void ReadNativeImage( Interface *engineInterface, void *imageMem, Stream *inputStream ) const = 0;
    virtual void WriteNativeImage( Interface *engineInterface, const void *imageMem, Stream *outputStream ) const = 0;

    // DO NOT ACCESS THE FOLLOWING FIELDS FROM CODE OUTSIDE OF
    // THE NATIVE IMAGING FORMAT MANAGER.

    struct
    {
        RwTypeSystem::typeInfoBase *imgType;
        const char *friendlyName;
        const imaging_filename_ext *fileExtensions;
        size_t fileExtCount;
        const natimg_supported_native_desc *suppNatTex;
        size_t suppNatTexCount;

        RwListEntry <nativeImageTypeManager> node;
        bool isRegistered;
    } manData;
};

// The actual NativeImage object.
// We want to do private types for all public RW interfaces.
struct NativeImagePrivate : public NativeImage
{
    NativeImagePrivate( EngineInterface *engineInterface );
    NativeImagePrivate( const NativeImagePrivate& right );

    ~NativeImagePrivate( void );

    // Those methods should be called under specific access to
    // the native image lock.
    void clearImageData( void );

    EngineInterface *engineInterface;

    bool hasPaletteDataRef;
    bool hasPixelDataRef;

    bool externalRasterRef;

    Raster *pixelOwner;
};

struct nativeImageEnv
{
    typedef StaticPluginClassFactory <NativeImagePrivate, RwDynMemAllocator, rwEirExceptionManager> nativeImgFact_t;

    inline nativeImageEnv( EngineInterface *engineInterface ) : nativeImgFact( eir::constr_with_alloc::DEFAULT, engineInterface )
    {
        return;
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        // We need a type for the native imaging formats to be categorized under.
        this->natImgType =
            engineInterface->typeSystem.RegisterAbstractType <NativeImage> ( "native_image" );

        LIST_CLEAR( formatsList.root );

        this->lockImgFmtConsist =
            CreateReadWriteLock( engineInterface );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // We do not need the lock anymore.
        if ( rwlock *lockImgFmtConsist = this->lockImgFmtConsist )
        {
            CloseReadWriteLock( engineInterface, lockImgFmtConsist );
        }

        // Clear all registered native imaging registrations.
        LIST_FOREACH_BEGIN( nativeImageTypeManager, this->formatsList.root, manData.node )

            // Delete the type.
            engineInterface->typeSystem.DeleteType( item->manData.imgType );

            item->manData.isRegistered = false;

        LIST_FOREACH_END

        LIST_CLEAR( this->formatsList.root );

        // Unregister our types.
        if ( RwTypeSystem::typeInfoBase *natImgType = this->natImgType )
        {
            engineInterface->typeSystem.DeleteType( natImgType );

            this->natImgType = nullptr;
        }
    }

    inline void operator =( const nativeImageEnv& right )
    {
        // Nothing to do here.
        return;
    }

    AINLINE nativeImageTypeManager* GetNativeImageTypeManagerByName( const char *natImgName ) const
    {
        // This is considered a "smart-routine". It uses less-strict checking to find
        // the requested native image type.

        LIST_FOREACH_BEGIN( nativeImageTypeManager, this->formatsList.root, manData.node )

            // Note that we check case-insensitively against a type name here.
            if ( StringEqualToZero( item->manData.imgType->name, natImgName, false ) )
            {
                return item;
            }

            // We also want to allow checking against the friendly name.
            if ( StringEqualToZero( item->manData.friendlyName, natImgName, false ) )
            {
                return item;
            }

        LIST_FOREACH_END

        return nullptr;
    }

    RwTypeSystem::typeInfoBase *natImgType;         // the type all native imaging types inherit from.

    // List of all registered native imaging formats.
    RwList <nativeImageTypeManager> formatsList;

    rwlock *lockImgFmtConsist;

    // We want to allow plugins for the native image type.
    nativeImgFact_t nativeImgFact;
};

typedef PluginDependantStructRegister <nativeImageEnv, RwInterfaceFactory_t> nativeImageEnvRegister_t;

extern optional_struct_space <nativeImageEnvRegister_t> nativeImageEnvRegister;

// Native image API.
bool RegisterNativeImageType(
    EngineInterface *engineInterface,
    nativeImageTypeManager *typeManager,
    const char *typeName, size_t memSize, size_t memAlign, const char *friendlyName,
    const imaging_filename_ext *fileExtensions, size_t fileExtCount,
    const natimg_supported_native_desc *suppNatTex, size_t suppNatTexCount
);
bool UnregisterNativeImageType(
    EngineInterface *engineInterface,
    const char *typeName
);

};
