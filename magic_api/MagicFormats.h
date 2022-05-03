/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        magic_api/MagicFormats.h
*  PURPOSE:     magf API header.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef MAGIC_CORE

#define MAGICAPI extern "C" __declspec(dllexport)
#define __MAGICCALL __cdecl

#endif //MAGIC_CORE

#include <stdint.h>

typedef uint32_t D3DFORMAT_SDK;

inline unsigned int MagicFormatAPIVersion( void )
{
    // We are currently version 2 API.
    // Update this whenever the ABI of the magf API changed!
    // * Rev2: added dynamic loading from any .exe
    return 3;
}

enum MAGIC_RASTER_FORMAT
{
	RASTER_DEFAULT,
	RASTER_1555,
	RASTER_565,
	RASTER_4444,
	RASTER_LUM,
	RASTER_8888,
	RASTER_888,
	RASTER_555 = 10
};

enum MAGIC_COLOR_ORDERING
{
	COLOR_RGBA,
	COLOR_BGRA,
	COLOR_ABGR
};

enum MAGIC_PALETTE_TYPE
{
	PALETTE_NONE,
	PALETTE_4BIT,
	PALETTE_8BIT,
    PALETTE_4BIT_LSB
};

/*
This class manages conversion between extension D3DFORMAT native textures to original RW types.
It is required so that specific D3DFORMAT textures can be integrated into this RenderWare framework.

To use this interface you have to register it into the engine. The engine will then map D3DFORMAT to this interface.
There can be only one handler for one D3DFORMAT type.

Revision 1 ABI.
*/
struct MagicFormat abstract
{
	virtual D3DFORMAT_SDK GetD3DFormat(void) const = 0;

	virtual const char* GetFormatName(void) const = 0;

	// Should return the size that a texture of width * height is supposed to have.
	// The returned value is used to allocate the storage for the pixels you get in ConvertFromRW.
	virtual size_t GetFormatTextureDataSize(unsigned int width, unsigned int height) const = 0;

	// The raster format that this native texture has mapped as original RW type has to stay the same.
	virtual void GetTextureRWFormat(MAGIC_RASTER_FORMAT& rasterFormatOut, unsigned int& depthOut, MAGIC_COLOR_ORDERING& colorOrderOut) const = 0;

	// Converts the D3DFORMAT anonymous data to RW original types and returns it.
	virtual void ConvertToRW(
		const void *texData, unsigned int texMipWidth, unsigned int texMipHeight, size_t dstStride, size_t texDataSize,
		void *texOut    // preallocated memory.
		) const = 0;

	// Converts original RW types into the D3DFORMAT plugin format.
	virtual void ConvertFromRW(
		unsigned int texMipWidth, unsigned int texMipHeight, unsigned int texRowAlignment,
		const void *texelSource, MAGIC_RASTER_FORMAT rasterFormat, unsigned int depth, MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
		void *texOut    // preallocated memory.
		) const = 0;
};

// In Revision 2 we added dynamic loading of the format plugins.
// In Revision 3 we added support for packed bitmaps.
// The format DLLs are no longer bound to a certain EXE in the Windows memory space.
struct MagicFormatPluginInterface abstract
{
    virtual bool PutTexelRGBA(
        void *texelSource, unsigned int texelIndex,
        unsigned int width, unsigned int rowAlignment, unsigned int rowi,
		MAGIC_RASTER_FORMAT rasterFormat, unsigned int depth,
	    MAGIC_COLOR_ORDERING colorOrder, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha
    ) const = 0;

    virtual bool BrowseTexelRGBA(
        const void *texelSource, unsigned int texelIndex,
        unsigned int width, unsigned int rowAlignment, unsigned int rowi,
		MAGIC_RASTER_FORMAT rasterFormat, unsigned int depth,
		MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
	    unsigned char& redOut, unsigned char& greenOut, unsigned char& blueOut, unsigned char& alphaOut
    ) const = 0;
};