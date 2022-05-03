/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.gpures.h
*  PURPOSE:
*       Header for RenderWare GPU resource management.
*       Allows allocation of GPU memory and associated consistency-management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_GPU_RESOURCES_HEADER_
#define _RENDERWARE_GPU_RESOURCES_HEADER_

namespace rw
{

// Descriptor of a resource surface.
// Depending on the surface format the GPU is allowed to address the data differently.
struct ResourceSurfaceFormat
{
    // We should strictly say that each descriptor points to one GPU layout the entire time.
    // There could be changes though, by nature of development, so we prohibit this thinking,
    // at least across versions of the framework.

    uint32 resWidth;
    uint32 resHeight;
    // Note that some adapters do not support some raster formats.
    gfxRasterFormat resUnitFormat;  // color storage type, including depth
    bool isNativeOrdering;          // swizzled or tiled
};

// Resource surface information returned by driver.
struct ResourceSurfaceInfo
{
    size_t surfSize;
};

// GPU resource data interface used to get the data that
// should be uploaded to the GPU.
struct ResourceInterface abstract
{
    // Returns the surface format of the data.
    virtual void getSurfaceFormat( ResourceSurfaceFormat& formatOut ) const = 0;
    // Returns the CPU-based resource data, in linear format.
    virtual size_t getResourceData( const void*& dataPtrOut ) const = 0;
};

// GPU resource descriptor that belongs to a GfxResourceBunch.
struct GfxResource
{
    inline GfxResource( Driver *owner, const ResourceSurfaceFormat& format ) : surfFormat( format )
    {
        this->ownerDriver = owner;
    }

    inline ~GfxResource( void )
    {
        return;
    }

private:
    Driver *ownerDriver;
    ResourceSurfaceFormat surfFormat;

    RwListEntry <GfxResource> bunchNode;
};

// Collection of resources that are managed by RenderWare.
struct GfxResourceBunch
{
    inline GfxResourceBunch( void )
    {
        return;
    }

private:
    RwList <GfxResource> resourceList;
};

} // namespace rw

#endif //_RENDERWARE_GPU_RESOURCES_HEADER_