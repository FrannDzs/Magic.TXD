/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwimaging.hxx
*  PURPOSE:     Internal header for the imaging components and environment.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_IMAGING_INTERNALS_
#define _RENDERWARE_IMAGING_INTERNALS_

namespace rw
{

struct imagingLayerTraversal
{
    // We do want to support compression traversal with this struct.
    // That is why this looks like a mipmap layer, but not really.
    uint32 layerWidth, layerHeight;
    uint32 mipWidth, mipHeight;
    void *texelSource;  // always newly allocated.
    uint32 dataSize;

    eRasterFormat rasterFormat;
    uint32 depth;
    uint32 rowAlignment;
    eColorOrdering colorOrder;
    ePaletteType paletteType;
    void *paletteData;
    uint32 paletteSize;
    eCompressionType compressionType;
    
    bool hasAlpha;  // only valid for deserialization, if capabilities say so.
};

// Interface for various image formats that this library should support.
struct imagingFormatExtension abstract
{
    // Method to verify whether a stream should be compatible with this format.
    // This method does not have to completely verify the integrity of the stream, so that deserialization may fail anyway.
    virtual bool IsStreamCompatible( Interface *engineInterface, Stream *inputStream ) const = 0;

    // Method that reports storage capabilities of this format.
    // What it can store is directly what it can take, nothing more.
    virtual void GetStorageCapabilities( pixelCapabilities& capsOut ) const = 0;

    // Pull and fetch methods.
    virtual void DeserializeImage( Interface *engineInterface, Stream *inputStream, imagingLayerTraversal& outputPixels ) const = 0;
    virtual void SerializeImage( Interface *engineInterface, Stream *outputStream, const imagingLayerTraversal& inputPixels ) const = 0;
};

#define IMAGING_COUNT_EXT(x)    ( sizeof(x) / sizeof(*x) )

// Function to register new imaging formats.
bool RegisterImagingFormat( Interface *engineInterface, const char *formatName, uint32 num_ext, const imaging_filename_ext *ext_array, imagingFormatExtension *intf );
bool UnregisterImagingFormat( Interface *engineInterface, imagingFormatExtension *intf );

}

#endif //_RENDERWARE_IMAGING_INTERNALS_