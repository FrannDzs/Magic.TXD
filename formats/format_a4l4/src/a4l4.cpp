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

class FormatA4L4 : public MagicFormat
{
	D3DFORMAT_SDK GetD3DFormat(void) const override
	{
		return D3DFMT_A4L4;
	}

	const char* GetFormatName(void) const override
	{
		return "A4L4";
	}

	struct pixel_t
	{
		unsigned char lum : 4;
		unsigned char alpha : 4;
	};

	size_t GetFormatTextureDataSize(unsigned int width, unsigned int height) const override
	{
		return getD3DBitmapDataSize( width, height, 8 );
	}

	void GetTextureRWFormat(MAGIC_RASTER_FORMAT& rasterFormatOut, unsigned int& depthOut, MAGIC_COLOR_ORDERING& colorOrderOut) const
	{
		rasterFormatOut = RASTER_8888;
		depthOut = 32;
		colorOrderOut = COLOR_BGRA;
	}

	void ConvertToRW(const void *texData, unsigned int texMipWidth, unsigned int texMipHeight, size_t dstRowStride, size_t texDataSize, void *texOut) const override
	{
        size_t stride = getD3DBitmapStride(texMipWidth, 8);
		for (unsigned int row = 0; row < texMipHeight; row++)
		{
            const pixel_t *rowData = (const pixel_t*)getD3DBitmapConstRow(texData, stride, row);
            void *dstRow = getD3DBitmapRow(texOut, dstRowStride, row);

            for (unsigned int col = 0; col < texMipWidth; col++)
            {
			    const pixel_t *theTexel = rowData + col;
			    unsigned char lum = theTexel->lum * 17;
			    _moduleIntf->PutTexelRGBA(dstRow, col, 0, 0, 0, RASTER_8888, 32, COLOR_BGRA, lum, lum, lum, theTexel->alpha * 17);
            }
		}
	}

	void ConvertFromRW(unsigned int texMipWidth, unsigned int texMipHeight, unsigned int texRowAlignment, const void *texelSource, MAGIC_RASTER_FORMAT rasterFormat,
		unsigned int depth, MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
		void *texOut) const override
	{
        size_t stride = getD3DBitmapStride(texMipWidth, 8);
		for (unsigned int row = 0; row < texMipHeight; row++)
		{
            pixel_t *dstRow = (pixel_t*)getD3DBitmapRow(texOut, stride, row);

            for (unsigned int col = 0; col < texMipWidth; col++)
            {
			    unsigned char r, g, b, a;
			    _moduleIntf->BrowseTexelRGBA(texelSource, col, texMipWidth, texRowAlignment, row, rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, r, g, b, a);
			    unsigned char lumVal = rgbToLuminance(r, g, b);
			    pixel_t *theTexel = (dstRow + col);
			    theTexel->lum = lumVal / 17;
			    theTexel->alpha = a / 17;
            }
		}
	}
};

static FormatA4L4 a4l4Format;

MAGICAPI MagicFormat * __MAGICCALL GetFormatInstance(unsigned int& versionOut)
{
    versionOut = MagicFormatAPIVersion();
	return &a4l4Format;
}