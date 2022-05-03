#include <MagicFormats.h>

#include <magfapi.h>

static const MagicFormatPluginInterface *_moduleIntf = NULL;

MAGICAPI void __MAGICCALL SetInterface( const MagicFormatPluginInterface *intf )
{
    _moduleIntf = intf;
}

class FormatV8U8 : public MagicFormat
{
	D3DFORMAT_SDK GetD3DFormat(void) const override
	{
		return D3DFMT_V8U8;
	}

	const char* GetFormatName(void) const override
	{
		return "V8U8";
	}

	struct pixel_t
	{
		unsigned char u;
		unsigned char v;
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

	void ConvertToRW(const void *texData, unsigned int texMipWidth, unsigned int texMipHeight, size_t dstRowStride, size_t texDataSize, void *texOut) const override
	{
        size_t stride = getD3DBitmapStride(texMipWidth, 16);
		for (unsigned int row = 0; row < texMipHeight; row++)
		{
            const pixel_t *srcRowData = (const pixel_t*)getD3DBitmapConstRow(texData, stride, row);
            void *dstRowData = getD3DBitmapRow(texOut, dstRowStride, row);

            for (unsigned int col = 0; col < texMipWidth; col++)
            {
			    const pixel_t *theTexel = srcRowData + col;
			    _moduleIntf->PutTexelRGBA(dstRowData, col, 0, 0, 0, RASTER_8888, 32, COLOR_BGRA, theTexel->u, theTexel->v, 0, 255);
            }
		}
	}

	void ConvertFromRW(unsigned int texMipWidth, unsigned int texMipHeight, unsigned int texRowAlignment, const void *texelSource, MAGIC_RASTER_FORMAT rasterFormat,
		unsigned int depth, MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
		void *texOut) const override
	{
		size_t stride = getD3DBitmapStride(texMipWidth, 16);
		for (unsigned int row = 0; row < texMipHeight; row++)
		{
            pixel_t *dstRowData = (pixel_t*)getD3DBitmapRow(texOut, stride, row);

            for (unsigned int col = 0; col < texMipWidth; col++)
            {
			    unsigned char r, g, b, a;
			    _moduleIntf->BrowseTexelRGBA(texelSource, col, texMipWidth, texRowAlignment, row, rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, r, g, b, a);
			    pixel_t *theTexel = (dstRowData + col);
			    theTexel->u = r;
			    theTexel->v = g;
            }
		}
	}
};

static FormatV8U8 v8u8Format;

MAGICAPI MagicFormat * __MAGICCALL GetFormatInstance(unsigned int& versionOut)
{
    versionOut = MagicFormatAPIVersion();
	return &v8u8Format;
}