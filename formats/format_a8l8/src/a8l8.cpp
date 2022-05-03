#include <MagicFormats.h>

#include <magfapi.h>

static const MagicFormatPluginInterface *_moduleIntf = NULL;

MAGICAPI void __MAGICCALL SetInterface( const MagicFormatPluginInterface *intf )
{
    _moduleIntf = intf;
}

inline unsigned char rgbToLuminance(unsigned char r, unsigned char g, unsigned char b)
{
	unsigned int colorSumm = (r + g + b);

	return (colorSumm / 3);
}

class FormatA8L8 : public MagicFormat
{
	D3DFORMAT_SDK GetD3DFormat(void) const override
	{
		return D3DFMT_A8L8;
	}

	const char* GetFormatName(void) const override
	{
		return "A8L8";
	}

	struct pixel_t
	{
		unsigned char lum;
		unsigned char alpha;
	};

	size_t GetFormatTextureDataSize(unsigned int width, unsigned int height) const override
	{
		return getD3DBitmapDataSize( width, height, 16 );
	}

	void GetTextureRWFormat(MAGIC_RASTER_FORMAT& rasterFormatOut, unsigned int& depthOut, MAGIC_COLOR_ORDERING& colorOrderOut) const
	{
		rasterFormatOut = RASTER_8888;
		depthOut = 32;
		colorOrderOut = COLOR_BGRA;
	}

	void ConvertToRW(
		const void *texData, unsigned int texMipWidth, unsigned int texMipHeight, size_t dstRowStride, size_t texDataSize,
		void *texOut
		) const override
	{
		const MAGIC_RASTER_FORMAT rasterFormat = RASTER_8888;
		const unsigned int depth = 32;
		const MAGIC_COLOR_ORDERING colorOrder = COLOR_BGRA;

		// do the conversion.
        size_t srcStride = getD3DBitmapStride(texMipWidth, 16);

		for (unsigned int row = 0; row < texMipHeight; row++)
		{
            const pixel_t *srcRow = (const pixel_t*)getD3DBitmapConstRow(texData, srcStride, row);
            void *dstRow = getD3DBitmapRow(texOut, dstRowStride, row);

            for (unsigned int col = 0; col < texMipWidth; col++)
            {
			    // We are simply a pixel_t.
			    const pixel_t *theTexel = srcRow + col;

			    _moduleIntf->PutTexelRGBA(dstRow, col, 0, 0, 0, rasterFormat, depth, colorOrder, theTexel->lum, theTexel->lum, theTexel->lum, theTexel->alpha);
            }
		}

		// Alright, we are done!
	}

	void ConvertFromRW(
		unsigned int texMipWidth, unsigned int texMipHeight, unsigned int texRowAlignment, const void *texelSource, MAGIC_RASTER_FORMAT rasterFormat, 
		unsigned int depth, MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
		void *texOut
		) const override
	{
		// We write stuff.
		size_t dstRowStride = getD3DBitmapStride(texMipWidth, 16);

		for (unsigned int row = 0; row < texMipHeight; row++)
		{
            pixel_t *dstRowData = (pixel_t*)getD3DBitmapRow(texOut, dstRowStride, row);

            for (unsigned int col = 0; col < texMipWidth; col++)
            {
			    // Get the color as RGBA and convert to closely matching luminance value.
			    unsigned char r, g, b, a;

			    _moduleIntf->BrowseTexelRGBA(
				    texelSource, col, texMipWidth, texRowAlignment, row,
				    rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize,
				    r, g, b, a
				);

			    unsigned char lumVal = rgbToLuminance(r, g, b);

			    pixel_t *theTexel = (dstRowData + col);

			    theTexel->lum = lumVal;
			    theTexel->alpha = a;
            }
		}

		// Done!
	}
};

// If your format is not used by the engine, you can be sure that a more fitting standard way for it exists
// that is bound natively by the library ;)
static FormatA8L8 a8l8Format;

MAGICAPI MagicFormat * __MAGICCALL GetFormatInstance(unsigned int& versionOut)
{
    versionOut = MagicFormatAPIVersion();
	return &a8l8Format;
}