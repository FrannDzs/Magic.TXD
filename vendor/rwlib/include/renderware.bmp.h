/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.bmp.h
*  PURPOSE:     Bitmap structures and helpers
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include <sdk/DataUtil.h>

#include <string.h>

namespace rw
{

enum class eRasterDataRowMode
{
    ALIGNED,    // byte aligned by power-of-2 value > 0
    PACKED      // no alignment, packed together without any bit padding
};

struct rasterRowSize
{
    AINLINE rasterRowSize( void ) noexcept
    {
        this->mode = eRasterDataRowMode::ALIGNED;
        this->byteSize = 0;
    }

    AINLINE uint32 estimateByteSize( void ) const
    {
        eRasterDataRowMode mode = this->mode;

        if ( mode == eRasterDataRowMode::ALIGNED )
        {
            return byteSize;
        }
        else if ( mode == eRasterDataRowMode::PACKED )
        {
            return (uint32)CEIL_DIV( (size_t)packed.depth * (size_t)packed.itemCount, 8u );
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }
    }

    AINLINE size_t getBitSize( void ) const
    {
        eRasterDataRowMode mode = this->mode;

        if ( mode == eRasterDataRowMode::ALIGNED )
        {
            return (size_t)byteSize * 8u;
        }
        else if ( mode == eRasterDataRowMode::PACKED )
        {
            return ( (size_t)packed.depth * packed.itemCount );
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }
    }

    eRasterDataRowMode mode;
    union
    {
        uint32 byteSize;
        struct
        {
            uint32 itemCount;
            uint32 depth;
        } packed;
    };

    AINLINE static rasterRowSize from_bytesize( uint32 bytesize )
    {
        rasterRowSize rowSize;
        rowSize.mode = eRasterDataRowMode::ALIGNED;
        rowSize.byteSize = bytesize;
        return rowSize;
    }
};

inline rasterRowSize getRasterDataRowSize( uint32 planeWidth, uint32 depth, uint32 alignment )
{
    rasterRowSize rowSize;

    if ( alignment == 0 )
    {
        rowSize.mode = eRasterDataRowMode::PACKED;
        rowSize.packed.itemCount = planeWidth;
        rowSize.packed.depth = depth;
    }
    else
    {
        uint32 rowSizeWithoutPadding = CEIL_DIV( planeWidth * depth, 8u );

        rowSize.mode = eRasterDataRowMode::ALIGNED;
        rowSize.byteSize = ALIGN_SIZE( rowSizeWithoutPadding, alignment );
    }

    return rowSize;
}

inline uint32 getRasterDataSizeByRowSize( const rasterRowSize& rowSize, uint32 height )
{
    eRasterDataRowMode mode = rowSize.mode;

    if ( mode == eRasterDataRowMode::PACKED )
    {
        return CEIL_DIV( rowSize.packed.itemCount * height * rowSize.packed.depth, 8u );
    }
    else if ( mode == eRasterDataRowMode::ALIGNED )
    {
        return ( rowSize.byteSize * height );
    }
    else
    {
        throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
    }
}

inline uint32 getPaletteDataSize( uint32 paletteCount, uint32 depth )
{
    return CEIL_DIV( paletteCount * depth, 8u );
}

inline uint32 getPackedRasterDataSize( uint32 itemCount, uint32 depth )
{
    return CEIL_DIV( itemCount * depth, 8u );
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif //__GNUC__

template <bool isConst = false>
struct genericRasterRow
{
    typedef typename std::conditional <isConst, const void*, void*>::type rowptr_t;

    AINLINE genericRasterRow( void ) noexcept
    {
        this->mode = eRasterDataRowMode::ALIGNED;
        this->aligned.aligned_rowPtr = nullptr;
        this->aligned.rowByteSize = 0;
    }

    AINLINE genericRasterRow( rowptr_t ptr ) noexcept
    {
        this->mode = eRasterDataRowMode::ALIGNED;
        this->aligned.aligned_rowPtr = ptr;
        this->aligned.rowByteSize = 0;  // no idea.
    }

    AINLINE genericRasterRow( const genericRasterRow& ) = default;

    eRasterDataRowMode mode;
    union
    {
        struct
        {
            rowptr_t aligned_rowPtr;
            uint32 rowByteSize;
        } aligned;
        struct
        {
            rowptr_t packed_dataPtr;
            uint32 itemOff;
            uint32 depth;
            uint32 row_width_cnt;
        } packed;
    };

    AINLINE operator bool ( void ) const noexcept
    {
        return ( this->mode != eRasterDataRowMode::ALIGNED || this->aligned.aligned_rowPtr != nullptr );
    }

    AINLINE void readBits( void *dstbuf, size_t srcbitoff, size_t bitcnt ) const
    {
        eRasterDataRowMode mode = this->mode;

        if ( mode == eRasterDataRowMode::ALIGNED )
        {
            const void *srcbuf = this->aligned.aligned_rowPtr;

            if ( ( srcbitoff % 8u ) == 0 )
            {
                size_t srcbyteoff = ( srcbitoff / 8u );
                size_t readbytecnt = CEIL_DIV( bitcnt, 8u );

                memcpy( dstbuf, (const char*)srcbuf + srcbyteoff, readbytecnt );
            }
            else
            {
                eir::moveBits( dstbuf, srcbuf, srcbitoff, 0, bitcnt );
            }
        }
        else if ( mode == eRasterDataRowMode::PACKED )
        {
            eir::moveBits(
                dstbuf,
                this->packed.packed_dataPtr,
                (size_t)this->packed.itemOff * this->packed.depth + srcbitoff,
                0,
                bitcnt
            );
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }
    }

    AINLINE void readBytes( void *dstbuf, uint32 srcbyteoff, uint32 numbytes ) const
    {
        eRasterDataRowMode mode = this->mode;

        if ( mode == eRasterDataRowMode::ALIGNED )
        {
            memcpy( dstbuf, (const char*)this->aligned.aligned_rowPtr + srcbyteoff, numbytes );
        }
        else if ( mode == eRasterDataRowMode::PACKED )
        {
            eir::moveBits(
                dstbuf,
                this->packed.packed_dataPtr,
                (size_t)this->packed.itemOff * this->packed.depth + (size_t)srcbyteoff * 8u,
                0,
                (size_t)numbytes * 8u
            );
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }
    }

    template <typename structType>
    AINLINE structType readStructItem( uint32 srcitemoff ) const
    {
        structType value;
        readBytes( &value, srcitemoff * sizeof(structType), sizeof(structType) );

        return value;
    }

    template <bool doOptimizeAlignment = true>
    AINLINE static genericRasterRow from_rowsize( rowptr_t texelData, const rasterRowSize& rowSize, uint32 n )
    {
        eRasterDataRowMode mode = rowSize.mode;

        genericRasterRow row;

        if ( mode == eRasterDataRowMode::ALIGNED )
        {
            uint32 byteSize = rowSize.byteSize;

            row.mode = eRasterDataRowMode::ALIGNED;
            row.aligned.aligned_rowPtr = (char*)texelData + (uint32)( byteSize * n );
            row.aligned.rowByteSize = byteSize;
        }
        else if ( mode == eRasterDataRowMode::PACKED )
        {
            uint32 width = rowSize.packed.itemCount;
            uint32 itemOff = ( width * n );
            uint32 depth = rowSize.packed.depth;

            if constexpr ( doOptimizeAlignment )
            {
                // Are offset into the buffer and the rowsize byte-aligned?
                // Optimization.
                size_t totalItemBitOff = ( (size_t)itemOff * depth );
                size_t rowBitSize = ( (size_t)width * depth );

                if ( ( totalItemBitOff % 8u ) == 0 && ( rowBitSize % 8u ) == 0 )
                {
                    uint32 totalItemByteOff = (uint32)( totalItemBitOff / 8u );
                    uint32 rowByteSize = (uint32)( rowBitSize / 8u );

                    row.mode = eRasterDataRowMode::ALIGNED;
                    row.aligned.aligned_rowPtr = (char*)texelData + totalItemByteOff;
                    row.aligned.rowByteSize = rowByteSize;
                    return row;
                }
            }

            row.mode = eRasterDataRowMode::PACKED;
            row.packed.packed_dataPtr = texelData;
            row.packed.itemOff = itemOff;
            row.packed.depth = depth;
            row.packed.row_width_cnt = width;
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }

        return row;
    }
};

struct constRasterRow : public genericRasterRow <true>
{
    using genericRasterRow::genericRasterRow;

    AINLINE constRasterRow( genericRasterRow <true>&& right ) : genericRasterRow( std::move( right ) )
    {
        return;
    }

    AINLINE constRasterRow( const genericRasterRow <false>& right )
    {
        eRasterDataRowMode rowMode = right.mode;

        this->mode = rowMode;

        if ( rowMode == eRasterDataRowMode::ALIGNED )
        {
            this->aligned.aligned_rowPtr = right.aligned.aligned_rowPtr;
            this->aligned.rowByteSize = right.aligned.rowByteSize;
        }
        else if ( rowMode == eRasterDataRowMode::PACKED )
        {
            this->packed.itemOff = right.packed.itemOff;
            this->packed.depth = right.packed.depth;
            this->packed.row_width_cnt = right.packed.row_width_cnt;
            this->packed.packed_dataPtr = right.packed.packed_dataPtr;
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }
    }
};

struct rasterRow : public genericRasterRow <false>
{
    using genericRasterRow::genericRasterRow;

    AINLINE rasterRow( genericRasterRow <false>&& right ) : genericRasterRow( std::move( right ) )
    {
        return;
    }

    AINLINE void writeBits( const void *srcbuf, size_t dstbitoff, size_t bitcnt, size_t srcbitoff = 0 )
    {
        eRasterDataRowMode mode = this->mode;

        if ( mode == eRasterDataRowMode::ALIGNED )
        {
            void *dstbuf = this->aligned.aligned_rowPtr;

            if ( ( dstbitoff % 8u ) == 0 && ( bitcnt % 8u ) == 0 && ( srcbitoff % 8u ) == 0 )
            {
                uint32 dstbyteoff = (uint32)( dstbitoff / 8u );
                uint32 bytecnt = (uint32)( bitcnt / 8u );
                uint32 srcbyteoff = (uint32)( srcbitoff / 8u );

                memcpy( (char*)dstbuf + dstbyteoff, (const char*)srcbuf + srcbyteoff, bytecnt );
            }
            else
            {
                eir::moveBits( dstbuf, srcbuf, srcbitoff, dstbitoff, bitcnt );
            }
        }
        else if ( mode == eRasterDataRowMode::PACKED )
        {
            eir::moveBits(
                this->packed.packed_dataPtr,
                srcbuf,
                srcbitoff,
                (size_t)this->packed.itemOff * this->packed.depth + dstbitoff,
                bitcnt
            );
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }
    }

    AINLINE void writeBytes( const void *srcbuf, uint32 dstbyteoff, uint32 bytecnt, uint32 srcbyteoff = 0 )
    {
        eRasterDataRowMode mode = this->mode;

        if ( mode == eRasterDataRowMode::ALIGNED )
        {
            memcpy( (char*)this->aligned.aligned_rowPtr + dstbyteoff, (const char*)srcbuf + srcbyteoff, bytecnt );
        }
        else if ( mode == eRasterDataRowMode::PACKED )
        {
            eir::moveBits(
                this->packed.packed_dataPtr,
                srcbuf,
                (size_t)srcbyteoff * 8u,
                (size_t)this->packed.itemOff * this->packed.depth + (size_t)dstbyteoff * 8u,
                bytecnt * 8u
            );
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }
    }

    template <typename structType>
    AINLINE void writeStructItem( const structType& value, uint32 dstitemoff )
    {
        writeBytes( &value, dstitemoff * sizeof(structType), sizeof(structType) );
    }

    // New stuff for setting bits.
    AINLINE void setBits( bool bitValue, size_t dstbitoff, size_t bitcnt )
    {
        eRasterDataRowMode rowMode = this->mode;

        if ( rowMode == eRasterDataRowMode::ALIGNED )
        {
            eir::setBits(
                this->aligned.aligned_rowPtr,
                bitValue,
                dstbitoff,
                bitcnt
            );
        }
        else if ( rowMode == eRasterDataRowMode::PACKED )
        {
            eir::setBits(
                this->packed.packed_dataPtr,
                bitValue,
                (size_t)this->packed.itemOff * this->packed.depth + dstbitoff,
                bitcnt
            );
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }
    }

    // Writing across rows.
    template <bool isConst>
    AINLINE void writeBitsFromRow( const genericRasterRow <isConst>& srcRow, size_t srcbitoff, size_t dstbitoff, size_t bitcnt )
    {
        eRasterDataRowMode srcMode = srcRow.mode;

        // Establish the source location.
        size_t srcbitstart;
        const void *srcbuf;

        if ( srcMode == eRasterDataRowMode::ALIGNED )
        {
            srcbitstart = 0;
            srcbuf = srcRow.aligned.aligned_rowPtr;
        }
        else if ( srcMode == eRasterDataRowMode::PACKED )
        {
            srcbitstart = (size_t)srcRow.packed.itemOff * srcRow.packed.depth;
            srcbuf = srcRow.packed.packed_dataPtr;
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }

        this->writeBits( srcbuf, dstbitoff, bitcnt, srcbitstart + srcbitoff );
    }

    template <bool isConst>
    AINLINE void writeBytesFromRow( const genericRasterRow <isConst>& srcRow, uint32 srcbyteoff, uint32 dstbyteoff, uint32 bytecnt )
    {
        eRasterDataRowMode srcMode = srcRow.mode;

        // Establish the source location.
        size_t srcbitstart;
        const void *srcbuf;

        if ( srcMode == eRasterDataRowMode::ALIGNED )
        {
            srcbitstart = 0;
            srcbuf = srcRow.aligned.aligned_rowPtr;
        }
        else if ( srcMode == eRasterDataRowMode::PACKED )
        {
            srcbitstart = (size_t)srcRow.packed.itemOff * srcRow.packed.depth;
            srcbuf = srcRow.packed.packed_dataPtr;
        }
        else
        {
            throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
        }

        this->writeBits( srcbuf, dstbyteoff * 8u, bytecnt * 8u, srcbitstart + srcbyteoff * 8u );
    }
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif //__GNUC__

template <bool isConst>
inline uint32 estimateRasterRowByteSize( const genericRasterRow <isConst>& row )
{
    eRasterDataRowMode mode = row.mode;

    if ( mode == eRasterDataRowMode::ALIGNED )
    {
        return row.aligned.rowByteSize;
    }
    else if ( mode == eRasterDataRowMode::PACKED )
    {
        return (uint32)CEIL_DIV( (size_t)row.packed.row_width_cnt * (size_t)row.packed.depth, 8u );
    }
    else
    {
        throw InternalErrorException( eSubsystemType::UTILITIES, nullptr );
    }
}

inline rasterRow getTexelDataRow( void *texelData, const rasterRowSize& rowSize, uint32 n )
{
    return rasterRow::from_rowsize( texelData, rowSize, n );
}

inline constRasterRow getConstTexelDataRow( const void *texelData, const rasterRowSize& rowSize, uint32 n )
{
    return constRasterRow::from_rowsize( texelData, rowSize, n );
}

enum eColorModel
{
    COLORMODEL_RGBA,
    COLORMODEL_LUMINANCE,
    COLORMODEL_DEPTH
};

struct rwAbstractColorItem
{
    eColorModel model;
    union
    {
        struct
        {
            float r, g, b, a;
        } rgbaColor;
        struct
        {
            float lum;
            float alpha;
        } luminance;
    };
};

// Bitmap software rendering includes.
struct Bitmap
{
    Bitmap( Interface *engineInterface );
    Bitmap( Interface *engineInterface, uint32 depth, eRasterFormat theFormat, eColorOrdering colorOrder );

private:
    void assignFrom( const Bitmap& right );

public:
    inline Bitmap( const Bitmap& right )
    {
        this->assignFrom( right );
    }

private:
    void moveFrom( Bitmap&& right );

public:
    inline Bitmap( Bitmap&& right )
    {
        this->moveFrom( std::move( right ) );
    }

private:
    void clearTexelData( void );

public:
    inline void operator =( const Bitmap& right )
    {
        this->clearTexelData();

        this->assignFrom( right );
    }

    inline void operator =( Bitmap&& right )
    {
        this->clearTexelData();

        this->moveFrom( std::move( right ) );
    }

    inline ~Bitmap( void )
    {
        this->clearTexelData();
    }

    inline static uint32 getRasterFormatDepth( eRasterFormat format )
    {
        // Returns the default raster format depth.
        // This one is standardized to be used by palette colors.

        uint32 theDepth = 0;

        if ( format == RASTER_8888 || format == RASTER_888 || format == RASTER_24 || format == RASTER_32 )
        {
            theDepth = 32;
        }
        else if ( format == RASTER_1555 || format == RASTER_565 || format == RASTER_4444 || format == RASTER_555 || format == RASTER_16 )
        {
            theDepth = 16;
        }
        else if ( format == RASTER_LUM )
        {
            theDepth = 8;
        }
        // NEW formats :3
        else if ( format == RASTER_LUM_ALPHA )
        {
            theDepth = 16;
        }

        return theDepth;
    }

    inline static uint32 getRasterImageDataSize( uint32 width, uint32 height, uint32 depth, uint32 rowAlignment )
    {
        rasterRowSize rowSize = getRasterDataRowSize( width, depth, rowAlignment );

        return getRasterDataSizeByRowSize( rowSize, height );
    }

    void setImageData( void *theTexels, eRasterFormat theFormat, eColorOrdering colorOrder, uint32 depth, uint32 rowAlignment, uint32 width, uint32 height, uint32 dataSize, bool assignData = false );

    inline void setImageDataSimple( void *theTexels, eRasterFormat theFormat, eColorOrdering colorOrder, uint32 depth, uint32 rowAlignment, uint32 width, uint32 height )
    {
        rasterRowSize rowSize = getRasterDataRowSize( width, depth, rowAlignment );

        uint32 dataSize = getRasterDataSizeByRowSize( rowSize, height );

        setImageData( theTexels, theFormat, colorOrder, depth, rowAlignment, width, height, dataSize );
    }

    void setSize( uint32 width, uint32 height );

    inline void getSize( uint32& width, uint32& height ) const
    {
        width = this->width;
        height = this->height;
    }

    void scale(
        Interface *engineInterface,
        uint32 width, uint32 height,
        const char *downsamplingMode = "blur", const char *upscaleMode = "linear"
    );

    inline uint32 getWidth( void ) const
    {
        return this->width;
    }

    inline uint32 getHeight( void ) const
    {
        return this->height;
    }

    inline uint32 getRowAlignment( void ) const
    {
        return this->rowAlignment;
    }

    inline void enlargePlane( uint32 reqWidth, uint32 reqHeight )
    {
        bool needsResize = false;

        uint32 curWidth = this->width;
        uint32 curHeight = this->height;

        if ( curWidth < reqWidth )
        {
            curWidth = reqWidth;

            needsResize = true;
        }

        if ( curHeight < reqHeight )
        {
            curHeight = reqHeight;

            needsResize = true;
        }

        if ( needsResize )
        {
            this->setSize( curWidth, curHeight );
        }
    }

    inline uint32 getDepth( void ) const
    {
        return this->depth;
    }

    inline eRasterFormat getFormat( void ) const
    {
        return this->rasterFormat;
    }

    inline eColorOrdering getColorOrder( void ) const
    {
        return this->colorOrder;
    }

    inline uint32 getDataSize( void ) const noexcept
    {
        return this->dataSize;
    }

	inline void *getTexelsData(void) const
	{
		return this->texels;
	}

    void* copyPixelData( void ) const;

    inline void setBgColor( double red, double green, double blue, double alpha = 1.0 )
    {
        this->bgRed = red;
        this->bgGreen = green;
        this->bgBlue = blue;
        this->bgAlpha = alpha;
    }

    inline void getBgColor( double& red, double& green, double& blue ) const
    {
        red = this->bgRed;
        green = this->bgGreen;
        blue = this->bgBlue;
    }

    bool browsecolor(uint32 x, uint32 y, uint8& redOut, uint8& greenOut, uint8& blueOut, uint8& alphaOut) const;
    bool browselum(uint32 x, uint32 y, uint8& lum, uint8& a) const;
    bool browsecolorex(uint32 x, uint32 y, rwAbstractColorItem& colorItem ) const;

    eColorModel getColorModel( void ) const;

    enum eBlendMode
    {
        BLEND_MODULATE,
        BLEND_ADDITIVE
    };

    enum eShadeMode
    {
        SHADE_SRCALPHA,
        SHADE_INVSRCALPHA,
        SHADE_ZERO,
        SHADE_ONE
    };

    struct sourceColorPipeline abstract
    {
        virtual uint32 getWidth( void ) const = 0;
        virtual uint32 getHeight( void ) const = 0;

        virtual void fetchcolor( uint32 x, uint32 y, double& red, double& green, double& blue, double& alpha ) = 0;
    };

    void draw(
        sourceColorPipeline& colorSource,
        uint32 offX, uint32 offY, uint32 drawWidth, uint32 drawHeight,
        eShadeMode srcChannel, eShadeMode dstChannel, eBlendMode blendMode
    );

    void drawBitmap(
        const Bitmap& theBitmap, uint32 offX, uint32 offY, uint32 drawWidth, uint32 drawHeight,
        eShadeMode srcChannel, eShadeMode dstChannel, eBlendMode blendMode
    );

private:
    Interface *engineInterface;

    uint32 width, height;
    uint32 depth;
    uint32 rowAlignment;
    rasterRowSize rowSize;
    eRasterFormat rasterFormat;
    void *texels;
    uint32 dataSize;

    eColorOrdering colorOrder;

    double bgRed, bgGreen, bgBlue, bgAlpha;
};

} // namespace rw
