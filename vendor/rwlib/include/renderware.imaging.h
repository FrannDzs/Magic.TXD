/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.imaging.h
*  PURPOSE:     RenderWare bitmap loading pipeline for popular image formats.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

namespace rw
{

// Special optimized mipmap pushing algorithms.
bool IsImagingFormatAvailable( Interface *engineInterface, const char *formatDescriptor );

// The main API for pushing and pulling pixels.
bool DeserializeImage( Stream *inputStream, Bitmap& outputPixels );
bool SerializeImage( Stream *outputStream, const char *formatDescriptor, const Bitmap& inputPixels );

// Struct to specify the imaging format extensions that the framework should use.
// This has to be stored as array in the imaging extension itself.
struct imaging_filename_ext
{
    const char *ext;
    bool isDefault;
};

// Utilities to deal with imaging_filename_ext.
bool IsImagingFormatExtension( uint32 num_ext, const imaging_filename_ext *ext_array, const char *query_ext );
bool GetDefaultImagingFormatExtension( uint32 num_ext, const imaging_filename_ext *ext_array, const char*& out_ext );
const char* GetLongImagingFormatExtension( uint32 num_ext, const imaging_filename_ext *ext_array );

// Get information about all registered image formats.
struct registered_image_format
{
    const char *formatName;
    uint32 num_ext;
    const imaging_filename_ext *ext_array;
};

typedef rwStaticVector <registered_image_format> registered_image_formats_t;

void GetRegisteredImageFormats( Interface *engineInterface, registered_image_formats_t& formatsOut );

// Native imaging.

// Virtual interface to native image formats.
struct NativeImage abstract RWSDK_FINALIZER
{
    RW_NOT_DIRECTLY_CONSTRUCTIBLE;

    const char* getTypeName( void ) const;

    const char* getRecommendedNativeTextureTarget( void ) const;

    void fetchFromRaster( Raster *raster );
    void putToRaster( Raster *raster );

    void readFromStream( Stream *stream );
    void writeToStream( Stream *stream );

    Interface* getEngine( void ) const;
};

NativeImage* CreateNativeImage( Interface *engineInterface, const char *typeName );
void DeleteNativeImage( NativeImage *imageHandle );

// Description API to know what native images are creatable from sources.
const char* GetNativeImageTypeForStream( Stream *stream );

typedef rwStaticVector <rwStaticString <char>> nativeImageRasterResults_t;

void GetNativeImageTypesForNativeTexture( Interface *engineInterface, const char *nativeTexName, nativeImageRasterResults_t& resultsOut );
bool DoesNativeImageSupportNativeTextureFriendly( Interface *engineInterface, const char *nativeImageName, const char *nativeTexName );
const char* GetNativeImageTypeNameFromFriendlyName( Interface *engineInterface, const char *nativeImageName );

bool GetNativeImageInfo( Interface *engineInterface, const char *nativeImageName, registered_image_format& infoOut );
bool IsNativeImageFormatAvailable( Interface *engineInterface, const char *nativeImageName );
void GetRegisteredNativeImageTypes( Interface *engineInterface, registered_image_formats_t& formatsOut );

} // namespace rw