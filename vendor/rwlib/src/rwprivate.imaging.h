/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwprivate.imaging.h
*  PURPOSE:     Framework-private global include header file about imaging extennsions.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_PRIVATE_IMAGING_
#define _RENDERWARE_PRIVATE_IMAGING_

namespace rw
{

// Special mipmap pushing algorithms.
// This was once a public export, but it seems to dangerous to do that.
bool DeserializeMipmapLayer( Stream *inputStream, rawMipmapLayer& rawLayer );
bool SerializeMipmapLayer( Stream *outputStream, const char *formatDescriptor, const rawMipmapLayer& rawLayer );

// Native imaging internal functions with special requirements.
// Read them up before using them!
void NativeImagePutToRasterNoLock( NativeImage *nativeImg, Raster *raster );
void NativeImageFetchFromRasterNoLock( NativeImage *nativeImg, Raster *raster, const char *nativeTexName, bool& needsRefOut );

} // namespace rw

#endif //_RENDERWARE_PRIVATE_IMAGING_