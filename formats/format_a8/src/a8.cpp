#include <MagicFormats.h>

#include <magfapi.h>

static const MagicFormatPluginInterface *_moduleIntf = NULL;

MAGICAPI void __MAGICCALL SetInterface( const MagicFormatPluginInterface *intf )
{
    _moduleIntf = intf;
}

class FormatA8 : public MagicFormat
{
	D3DFORMAT_SDK GetD3DFormat(void) const override
	{
		return D3DFMT_A8;
	}

	const char* GetFormatName(void) const override
	{
		return "A8";
	}

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
		for ( unsigned int row = 0; row < texMipHeight; row++ )
        {
            const unsigned char *rowData = (unsigned char*)getD3DBitmapConstRow(texData, stride, row);
            void *dstRowData = getD3DBitmapRow(texOut, dstRowStride, row);

            for ( unsigned int col = 0; col < texMipWidth; col++ )
            {
			    _moduleIntf->PutTexelRGBA(dstRowData, col, 0, 0, 0, RASTER_8888, 32, COLOR_BGRA, 0, 0, 0, rowData[col]);
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
            unsigned char *dstRowData = (unsigned char*)getD3DBitmapRow(texOut, stride, row);

            for (unsigned int col = 0; col < texMipWidth; col++)
            {
			    unsigned char r, g, b, a;
			    _moduleIntf->BrowseTexelRGBA(texelSource, col, texMipWidth, texRowAlignment, row, rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, r, g, b, a);
			    dstRowData[col] = a;
            }
		}
	}
};

static FormatA8 a8Format;

MAGICAPI MagicFormat * __MAGICCALL GetFormatInstance(unsigned int& versionOut)
{
    versionOut = MagicFormatAPIVersion();
	return &a8Format;
}