/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.d3d.dxt.hxx
*  PURPOSE:     Direct3D-based texture helpers for DXT compression
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_D3D_DXT_NATIVE_
#define _RENDERWARE_D3D_DXT_NATIVE_

// DXT specific stuff.
#include <squish.h>

#include "pixelformat.hxx"

namespace rw
{

// WARNING: little endian types!
struct rgb565
{
    union
    {
        struct
        {
            unsigned short blue : 5;
            unsigned short green : 6;
            unsigned short red : 5;
        };
        unsigned short val;
    };
};

AINLINE uint32 getDXTLocalBlockIndex( uint32 local_x, uint32 local_y )
{
    return ( local_y * 4u + local_x );
}

template <typename numType>
AINLINE numType indexlist_lookup( const numType& indexList, numType index, numType bit_count )
{
    numType bitMask = ( (numType)std::pow( (numType)2, bit_count ) - 1 );

    numType shiftCount = ( index * bit_count );

    return ( ( indexList >> shiftCount ) & bitMask );
}

AINLINE uint32 fetchDXTIndexList( const uint32& indexList, uint32 local_x, uint32 local_y )
{
    uint32 coord_index = getDXTLocalBlockIndex( local_x, local_y );

    return indexlist_lookup( indexList, coord_index, 2u );
}

template <typename numType>
AINLINE void indexlist_put( numType& indexList, numType index, numType bit_count, numType value )
{
    numType bitMask = ( (numType)std::pow( (numType)2, bit_count ) - 1 );

    numType shiftCount = ( index * bit_count );

    indexList |= ( ( value & bitMask ) << shiftCount );
}

AINLINE void putDXTIndexList( uint32& indexList, uint32 local_x, uint32 local_y, uint32 value )
{
    uint32 coord_index = getDXTLocalBlockIndex( local_x, local_y );

    indexlist_put( indexList, coord_index, 2u, value );
}

// TODO: maybe force these to be little-endian?
// We might want to convert them to the architecture format before passing
// to SQUISH and other libraries, tbh. Something to think about.

template <template <typename numberType> class endianness>
struct dxt1_block
{
    endianness <rgb565> col0;
    endianness <rgb565> col1;

    endianness <uint32> indexList;
};
static_assert( sizeof( dxt1_block <endian::little_endian> ) == 8, "DXT1 block must be 8 bytes in size!" );

template <template <typename numberType> class endianness>
struct dxt2_3_block
{
    endianness <uint64> alphaList;

    endianness <rgb565> col0;
    endianness <rgb565> col1;

    endianness <uint32> indexList;

    inline uint32 getAlphaForTexel( uint32 index ) const
    {
        uint32 indexList = this->indexList;

        return indexlist_lookup( indexList, index, 4u );
    }
};
static_assert( sizeof( dxt2_3_block <endian::little_endian> ) == 16, "DXT2/3 block must be 16 bytes in size!" );

struct uint48_t
{
    char data[6];
};

template <template <typename numberType> class endianness>
struct dxt4_5_block
{
    uint8 alphaPreMult[2];
    endianness <uint48_t> alphaList;

    endianness <rgb565> col0;
    endianness <rgb565> col1;

    endianness <uint32> indexList;

    static inline uint8 getAlphaByIndex( uint32 first_alpha, uint32 second_alpha, uint32 alphaIndex )
    {
        uint8 theAlpha = 255u;

        if ( alphaIndex < 2 )
        {
            if ( alphaIndex == 0 )
            {
                theAlpha = first_alpha;
            }
            else if ( alphaIndex == 1 )
            {
                theAlpha = second_alpha;
            }
        }
        else
        {
            uint32 displaced_alpha_index = ( alphaIndex - 2 );

            if ( first_alpha > second_alpha )
            {
                uint32 first_linear_arg = ( 6u - displaced_alpha_index );
                uint32 second_linear_arg = ( displaced_alpha_index + 1u );

                theAlpha = ( first_linear_arg * first_alpha + second_linear_arg * second_alpha ) / 7;
            }
            else
            {
                if ( alphaIndex == 6 )
                {
                    theAlpha = 0;
                }
                else if ( alphaIndex == 7 )
                {
                    theAlpha = 255u;
                }
                else
                {
                    uint32 first_linear_arg = ( 4u - displaced_alpha_index );
                    uint32 second_linear_arg = ( displaced_alpha_index + 1u );

                    theAlpha = ( first_linear_arg * first_alpha + second_linear_arg * second_alpha ) / 5;
                }
            }
        }

        return theAlpha;
    }
};
static_assert( sizeof( dxt4_5_block <endian::little_endian> ) == 16, "DXT4/5 block must be 16 bytes in size!" );

struct squish_color
{
    uint8 blue;
    uint8 green;
    uint8 red;
    uint8 alpha;
};

inline void premultiplyByAlpha(
    uint8 red, uint8 green, uint8 blue, uint8 alpha,
    uint8& redOut, uint8& greenOut, uint8& blueOut
)
{
    double r = unpackcolor( red );
    double g = unpackcolor( green );
    double b = unpackcolor( blue );
    double a = unpackcolor( alpha );

    r = r * a;
    g = g * a;
    b = b * a;

    // Write new colors.
    redOut = packcolor( r );
    greenOut = packcolor( g );
    blueOut = packcolor( b );
}

inline void unpremultiplyByAlpha(
    uint8 red, uint8 green, uint8 blue, uint8 alpha,
    uint8& redOut, uint8& greenOut, uint8& blueOut
)
{
    double r = unpackcolor( red );
    double g = unpackcolor( green );
    double b = unpackcolor( blue );
    double a = unpackcolor( alpha );

    if ( a != 0 )
    {
        r = std::min( 1.0, r / a );
        g = std::min( 1.0, g / a );
        b = std::min( 1.0, b / a );
    }

    // Write new colors.
    redOut = packcolor( r );
    greenOut = packcolor( g );
    blueOut = packcolor( b );
}

template <template <typename numberType> class endianness>
inline bool decompressDXTBlock(
    eDXTCompressionMethod dxtMethod,
    const void *theTexels, uint32 blockIndex, uint32 dxtType,
    PixelFormat::pixeldata32bit colorsOut[4][4]
)
{
    bool hasDecompressed = false;

    // Since Squish supports little-endian only, we do the decompression ourselves.
    if (dxtType == 1)
    {
        const dxt1_block <endianness> *block = (const dxt1_block <endianness>*)theTexels + blockIndex;

		/* calculate colors */
		const rgb565 col0 = block->col0;
		const rgb565 col1 = block->col1;
		uint32 c[4][4];

		c[0][0] = col0.red * 0xFF/0x1F;
		c[0][1] = col0.green * 0xFF/0x3F;
		c[0][2] = col0.blue * 0xFF/0x1F;
		c[0][3] = 0xFF;

		c[1][0] = col1.red * 0xFF/0x1F;
		c[1][1] = col1.green * 0xFF/0x3F;
		c[1][2] = col1.blue * 0xFF/0x1F;
        c[1][3] = 0xFF;

		if (col0.val > col1.val)
        {
			c[2][0] = (2*c[0][0] + 1*c[1][0])/3;
			c[2][1] = (2*c[0][1] + 1*c[1][1])/3;
			c[2][2] = (2*c[0][2] + 1*c[1][2])/3;
			c[2][3] = 0xFF;

			c[3][0] = (1*c[0][0] + 2*c[1][0])/3;
			c[3][1] = (1*c[0][1] + 2*c[1][1])/3;
			c[3][2] = (1*c[0][2] + 2*c[1][2])/3;
			c[3][3] = 0xFF;
		}
        else
        {
			c[2][0] = (c[0][0] + c[1][0])/2;
			c[2][1] = (c[0][1] + c[1][1])/2;
			c[2][2] = (c[0][2] + c[1][2])/2;
			c[2][3] = 0xFF;

			c[3][0] = 0x00;
			c[3][1] = 0x00;
			c[3][2] = 0x00;
		    c[3][3] = 0x00;
		}

		/* make index list */
		uint32 indicesint = block->indexList;
		uint8 indices[16];
		for (int32 k = 0; k < 16; k++)
        {
			indices[k] = indicesint & 0x3;
			indicesint >>= 2;
		}

		/* write bytes */
		for (uint32 y_block = 0; y_block < 4; y_block++)
        {
			for (uint32 x_block = 0; x_block < 4; x_block++)
            {
                PixelFormat::pixeldata32bit& pixelOut = colorsOut[ y_block ][ x_block ];

                uint32 coordIndex = ( y_block * 4 + x_block );

                uint32 colorIndex = indices[ coordIndex ];

                uint8 red       = c[ colorIndex ][0];
                uint8 green     = c[ colorIndex ][1];
                uint8 blue      = c[ colorIndex ][2];
                uint8 alpha     = c[ colorIndex ][3];

                pixelOut.red    = red;
                pixelOut.green  = green;
                pixelOut.blue   = blue;
                pixelOut.alpha  = alpha;
			}
        }

        hasDecompressed = true;
    }
    else if (dxtType == 2 || dxtType == 3)
    {
        bool isPremultiplied = ( dxtType == 2 );

        const dxt2_3_block <endianness> *block = (const dxt2_3_block <endianness>*)theTexels + blockIndex;

		/* calculate colors */
		const rgb565 col0 = block->col0;
		const rgb565 col1 = block->col1;
		uint32 c[4][4];

		c[0][0] = col0.red * 0xFF/0x1F;
		c[0][1] = col0.green * 0xFF/0x3F;
		c[0][2] = col0.blue * 0xFF/0x1F;

		c[1][0] = col1.red * 0xFF/0x1F;
		c[1][1] = col1.green * 0xFF/0x3F;
		c[1][2] = col1.blue * 0xFF/0x1F;

		c[2][0] = (2*c[0][0] + 1*c[1][0])/3;
		c[2][1] = (2*c[0][1] + 1*c[1][1])/3;
		c[2][2] = (2*c[0][2] + 1*c[1][2])/3;

		c[3][0] = (1*c[0][0] + 2*c[1][0])/3;
		c[3][1] = (1*c[0][1] + 2*c[1][1])/3;
		c[3][2] = (1*c[0][2] + 2*c[1][2])/3;

		/* make index list */
		uint32 indicesint = block->indexList;
		uint8 indices[16];

		for (int32 k = 0; k < 16; k++)
        {
			indices[k] = indicesint & 0x3;
			indicesint >>= 2;
		}

		uint64 alphasint = block->alphaList;
		uint8 alphas[16];

		for (int32 k = 0; k < 16; k++)
        {
			alphas[k] = (alphasint & 0xF)*17;
			alphasint >>= 4;
		}

		/* write bytes */
		for (uint32 y_block = 0; y_block < 4; y_block++)
        {
			for (uint32 x_block = 0; x_block < 4; x_block++)
            {
                PixelFormat::pixeldata32bit& pixelOut = colorsOut[ y_block ][ x_block ];

                uint32 coordIndex = ( y_block * 4 + x_block );

                uint32 colorIndex = indices[ coordIndex ];

                uint8 red       = c[ colorIndex ][0];
                uint8 green     = c[ colorIndex ][1];
                uint8 blue      = c[ colorIndex ][2];
                uint8 alpha     = alphas[ coordIndex ];

                // Since the color has been premultiplied with the alpha, we should reverse that.
                if ( isPremultiplied )
                {
                    unpremultiplyByAlpha( red, green, blue, alpha, red, green, blue );
                }

                pixelOut.red    = red;
                pixelOut.green  = green;
                pixelOut.blue   = blue;
                pixelOut.alpha  = alpha;
			}
        }

        hasDecompressed = true;
    }
    else if (dxtType == 4 || dxtType == 5)
    {
        bool isPremultiplied = ( dxtType == 4 );

        const dxt4_5_block <endianness> *block = (const dxt4_5_block <endianness>*)theTexels + blockIndex;

		/* calculate colors */
		const rgb565 col0 = block->col0;
		const rgb565 col1 = block->col1;
		uint32 c[4][4];

		c[0][0] = col0.red * 0xFF/0x1F;
		c[0][1] = col0.green * 0xFF/0x3F;
		c[0][2] = col0.blue * 0xFF/0x1F;

		c[1][0] = col1.red * 0xFF/0x1F;
		c[1][1] = col1.green * 0xFF/0x3F;
		c[1][2] = col1.blue * 0xFF/0x1F;

		c[2][0] = (2*c[0][0] + 1*c[1][0])/3;
		c[2][1] = (2*c[0][1] + 1*c[1][1])/3;
		c[2][2] = (2*c[0][2] + 1*c[1][2])/3;

		c[3][0] = (1*c[0][0] + 2*c[1][0])/3;
		c[3][1] = (1*c[0][1] + 2*c[1][1])/3;
		c[3][2] = (1*c[0][2] + 2*c[1][2])/3;

		uint32 a[8];

        uint8 first_alpha = block->alphaPreMult[0];
        uint8 second_alpha = block->alphaPreMult[1];

        for ( uint32 n = 0; n < 8; n++ )
        {
            a[n] = dxt4_5_block <endianness>::getAlphaByIndex( first_alpha, second_alpha, n );
        }

		/* make index list */
		uint32 indicesint = block->indexList;
		uint8 indices[16];
		for (uint32 k = 0; k < 16; k++)
        {
			indices[k] = indexlist_lookup( indicesint, k, 2u );
		}
		// actually 6 bytes
		uint64 alphasint = *((uint64 *) &block->alphaList );
		uint8 alphas[16];
		for (uint32 k = 0; k < 16; k++)
        {
			alphas[k] = (uint8)indexlist_lookup( alphasint, (uint64)k, (uint64)3 );
		}

		/* write bytes */
		for (uint32 y_block = 0; y_block < 4; y_block++)
        {
			for (uint32 x_block = 0; x_block < 4; x_block++)
            {
                PixelFormat::pixeldata32bit& pixelOut = colorsOut[ y_block ][ x_block ];

                uint32 coordIndex = getDXTLocalBlockIndex( x_block, y_block );

                uint32 colorIndex = indices[ coordIndex ];

                uint8 red       = c[ colorIndex ][0];
                uint8 green     = c[ colorIndex ][1];
                uint8 blue      = c[ colorIndex ][2];
                uint8 alpha     = a[ alphas[ coordIndex ] ];

                // Since the color has been premultiplied with the alpha, we should reverse that.
                if ( isPremultiplied )
                {
                    unpremultiplyByAlpha( red, green, blue, alpha, red, green, blue );
                }

                pixelOut.red    = red;
                pixelOut.green  = green;
                pixelOut.blue   = blue;
                pixelOut.alpha  = alpha;
			}
        }

        hasDecompressed = true;
    }

    return hasDecompressed;
}

inline uint32 getDXTBlockSize( uint32 dxtType )
{
    uint32 blockSize = 0;

    if (dxtType == 1)
    {
        blockSize = 8;
    }
    else if (dxtType == 2 || dxtType == 3 ||
             dxtType == 4 || dxtType == 5)
    {
        blockSize = 16;
    }

    return blockSize;
}

inline uint32 getDXTRasterDataSize(uint32 dxtType, uint32 texUnitCount)
{
    uint32 texBlockCount = texUnitCount / 16;

    uint32 blockSize = getDXTBlockSize( dxtType );

    return ( texBlockCount * blockSize );
}

template <template <typename numberType> class endianness>
inline void compressTexelsUsingDXT(
    Interface *engineInterface,
    uint32 dxtType, const void *texelSource, uint32 mipWidth, uint32 mipHeight, uint32 rowAlignment,
    eRasterFormat rasterFormat, const void *paletteData, ePaletteType paletteType, uint32 maxpalette, eColorOrdering colorOrder, uint32 itemDepth,
    void*& texelsOut, uint32& dataSizeOut,
    uint32& realWidthOut, uint32& realHeightOut
)
{
    // Make sure the texture dimensions are aligned by 4.
    uint32 alignedMipWidth = ALIGN_SIZE( mipWidth, 4u );
    uint32 alignedMipHeight = ALIGN_SIZE( mipHeight, 4u );

    uint32 dxtDataSize = getDXTRasterDataSize(dxtType, ( alignedMipWidth * alignedMipHeight ) );

    void *dxtArray = engineInterface->PixelAllocate( dxtDataSize );

    try
    {
        // Calculate the row size of the source texture.
        rasterRowSize rawRowSize = getRasterDataRowSize( mipWidth, itemDepth, rowAlignment );

        // Loop across the image.
        uint32 compressedBlockCount = 0;

        uint32 widthBlocks = alignedMipWidth / 4;
        uint32 heightBlocks = alignedMipHeight / 4;

        uint32 y = 0;

        colorModelDispatcher fetchSrcDispatch( rasterFormat, colorOrder, itemDepth, paletteData, maxpalette, paletteType );

        for ( uint32 y_block = 0; y_block < heightBlocks; y_block++, y += 4 )
        {
            uint32 x = 0;

            for ( uint32 x_block = 0; x_block < widthBlocks; x_block++, x += 4 )
            {
                // Compress a 4x4 color block.
                PixelFormat::pixeldata32bit colors[4][4];

                // Check whether we should premultiply.
                bool isPremultiplied = ( dxtType == 2 || dxtType == 4 );

                for ( uint32 y_iter = 0; y_iter != 4; y_iter++ )
                {
                    for ( uint32 x_iter = 0; x_iter != 4; x_iter++ )
                    {
                        PixelFormat::pixeldata32bit& inColor = colors[ y_iter ][ x_iter ];

                        uint8 r = 0;
                        uint8 g = 0;
                        uint8 b = 0;
                        uint8 a = 0;

                        uint32 targetX = ( x + x_iter );
                        uint32 targetY = ( y + y_iter );

                        if ( targetX < mipWidth && targetY < mipHeight )
                        {
                            constRasterRow rowData = getConstTexelDataRow( texelSource, rawRowSize, targetY );

                            fetchSrcDispatch.getRGBA( rowData, targetX, r, g, b, a );
                        }

                        if ( isPremultiplied )
                        {
                            premultiplyByAlpha( r, g, b, a, r, g, b );
                        }

                        inColor.red = r;
                        inColor.green = g;
                        inColor.blue = b;
                        inColor.alpha = a;
                    }
                }

                // Compress it using SQUISH.

                // Since SQUISH only supports native-word DXT blocks, we will have to
                // convert to the correct endianness after compression.
                if ( dxtType == 1 )
                {
                    struct native_dxt1_block
                    {
                        rgb565 col0;
                        rgb565 col1;

                        uint32 indexList;
                    };
                    native_dxt1_block compr_block;

                    squish::Compress( (const squish::u8*)colors, &compr_block, squish::kDxt1 );

                    // Write it into the texture in correct endianness.
                    dxt1_block <endianness> *dstBlock = (dxt1_block <endianness>*)dxtArray + compressedBlockCount;

                    dstBlock->col0 = compr_block.col0;
                    dstBlock->col1 = compr_block.col1;
                    dstBlock->indexList = compr_block.indexList;
                }
                else if ( dxtType == 2 || dxtType == 3 )
                {
                    struct native_dxt23_block
                    {
                        uint64 alphaList;

                        rgb565 col0;
                        rgb565 col1;

                        uint32 indexList;
                    };
                    native_dxt23_block compr_block;

                    squish::Compress( (const squish::u8*)colors, &compr_block, squish::kDxt3 );

                    // Write it in correct endianness to the texture.
                    dxt2_3_block <endianness> *dstBlock = (dxt2_3_block <endianness>*)dxtArray + compressedBlockCount;

                    dstBlock->alphaList = compr_block.alphaList;
                    dstBlock->col0 = compr_block.col0;
                    dstBlock->col1 = compr_block.col1;
                    dstBlock->indexList = compr_block.indexList;
                }
                else if ( dxtType == 4 || dxtType == 5 )
                {
                    struct native_dxt45_block
                    {
                        uint8 alphaPreMult[2];
                        uint48_t alphaList;

                        rgb565 col0;
                        rgb565 col1;

                        uint32 indexList;
                    };
                    native_dxt45_block compr_block;

                    squish::Compress( (const squish::u8*)colors, &compr_block, squish::kDxt5 );

                    // Write the destination block into the texture.
                    dxt4_5_block <endianness> *dstBlock = (dxt4_5_block <endianness>*)dxtArray + compressedBlockCount;

                    dstBlock->alphaPreMult[0] = compr_block.alphaPreMult[0];
                    dstBlock->alphaPreMult[1] = compr_block.alphaPreMult[1];
                    dstBlock->alphaList = compr_block.alphaList;
                    dstBlock->col0 = compr_block.col0;
                    dstBlock->col1 = compr_block.col1;
                    dstBlock->indexList = compr_block.indexList;
                }
                else
                {
                    assert( 0 );
                }

                // Increment the block count.
                compressedBlockCount++;
            }
        }
    }
    catch( ... )
    {
        engineInterface->PixelFree( dxtArray );

        throw;
    }

    // Give the new texels to the runtime, along with the data size.
    texelsOut = dxtArray;
    dataSizeOut = dxtDataSize;

    realWidthOut = alignedMipWidth;
    realHeightOut = alignedMipHeight;
}

// Generic decompressor based on no fixed types.
template <template <typename numberType> class endianness, typename dstDispatchType>
inline bool genericDecompressTexelsUsingDXT(
    Interface *engineInterface, uint32 dxtType, eDXTCompressionMethod dxtMethod,
    uint32 texWidth, uint32 texHeight, uint32 texRowAlignment,
    uint32 texLayerWidth, uint32 texLayerHeight,
    const void *srcTexels, dstDispatchType& putDispatch, uint32 putDepth,
    void*& dstTexelsOut, uint32& dstTexelsDataSizeOut
)
{
    // Allocate the new texel array.
	rasterRowSize rowSize = getRasterDataRowSize( texLayerWidth, putDepth, texRowAlignment );

    uint32 dataSize = getRasterDataSizeByRowSize( rowSize, texHeight );

	void *newtexels = engineInterface->PixelAllocate( dataSize );

    bool successfullyDecompressed = true;

    try
    {
        // Get the compressed block count.
        uint32 compressedBlockCount = ( texWidth * texHeight ) / 16;

	    uint32 x = 0, y = 0;

	    for (uint32 n = 0; n < compressedBlockCount; n++)
        {
            PixelFormat::pixeldata32bit colors[4][4];

            bool couldDecompressBlock = decompressDXTBlock <endianness> (dxtMethod, srcTexels, n, dxtType, colors);

            if (couldDecompressBlock == false)
            {
                // If even one block fails to decompress, abort.
                successfullyDecompressed = false;
                break;
            }

            // Write the colors.
            for (uint32 y_block = 0; y_block < 4; y_block++)
            {
                for (uint32 x_block = 0; x_block < 4; x_block++)
                {
                    uint32 target_x = ( x + x_block );
                    uint32 target_y = ( y + y_block );

                    if ( target_x < texLayerWidth && target_y < texLayerHeight )
                    {
                        const PixelFormat::pixeldata32bit& srcColor = colors[ y_block ][ x_block ];

                        uint8 red       = srcColor.red;
                        uint8 green     = srcColor.green;
                        uint8 blue      = srcColor.blue;
                        uint8 alpha     = srcColor.alpha;

                        // Get the target row.
                        rasterRow theRow = getTexelDataRow( newtexels, rowSize, target_y );

                        putDispatch.setRGBA(theRow, target_x, red, green, blue, alpha);
                    }
                }
            }

		    x += 4;

		    if (x >= texWidth)
            {
			    y += 4;
			    x = 0;
		    }

            if (y >= texHeight)
            {
                break;
            }
	    }
    }
    catch( ... )
    {
        engineInterface->PixelFree( newtexels );

        throw;
    }

    if ( !successfullyDecompressed )
    {
        // Delete the new texel array again.
        engineInterface->PixelFree( newtexels );
    }
    else
    {
        // Give the data to the runtime.
        dstTexelsOut = newtexels;
        dstTexelsDataSizeOut = dataSize;
    }

    return successfullyDecompressed;
}

// Generic decompressor based on framework types.
template <template <typename numberType> class endianness>
inline bool decompressTexelsUsingDXT(
    Interface *engineInterface, uint32 dxtType, eDXTCompressionMethod dxtMethod,
    uint32 texWidth, uint32 texHeight, uint32 texRowAlignment,
    uint32 texLayerWidth, uint32 texLayerHeight,
    const void *srcTexels, eRasterFormat rawRasterFormat, eColorOrdering rawColorOrder, uint32 rawDepth,
    void*& dstTexelsOut, uint32& dstTexelsDataSizeOut
)
{
    colorModelDispatcher putDispatch( rawRasterFormat, rawColorOrder, rawDepth, nullptr, 0, PALETTE_NONE );

    return genericDecompressTexelsUsingDXT <endianness> (
        engineInterface, dxtType, dxtMethod,
        texWidth, texHeight, texRowAlignment,
        texLayerWidth, texLayerHeight,
        srcTexels, putDispatch, rawDepth,
        dstTexelsOut, dstTexelsDataSizeOut
    );
}

inline bool IsDXTCompressionType( eCompressionType compressionType, uint32& dxtType )
{
    if ( compressionType == RWCOMPRESS_DXT1 ||
         compressionType == RWCOMPRESS_DXT2 ||
         compressionType == RWCOMPRESS_DXT3 ||
         compressionType == RWCOMPRESS_DXT4 ||
         compressionType == RWCOMPRESS_DXT5 )
    {
        if ( compressionType == RWCOMPRESS_DXT1 )
        {
            dxtType = 1;
        }
        else if ( compressionType == RWCOMPRESS_DXT2 )
        {
            dxtType = 2;
        }
        else if ( compressionType == RWCOMPRESS_DXT3 )
        {
            dxtType = 3;
        }
        else if ( compressionType == RWCOMPRESS_DXT4 )
        {
            dxtType = 4;
        }
        else if ( compressionType == RWCOMPRESS_DXT5 )
        {
            dxtType = 5;
        }
        else
        {
            assert( 0 );

            dxtType = 0;
        }

        return true;
    }

    return false;
}

inline eRasterFormat getDXTDecompressionRasterFormat( Interface *engineInterface, uint32 dxtType, bool hasAlpha )
{
    bool doPackedDecompress = engineInterface->GetDXTPackedDecompression();

    eRasterFormat targetRasterFormat;

    if (hasAlpha)
    {
        eRasterFormat packedAlphaRasterFormat = RASTER_8888;

        if (dxtType == 1)
        {
            packedAlphaRasterFormat = RASTER_1555;
        }
        else if (dxtType == 2 || dxtType == 3)
        {
            packedAlphaRasterFormat = RASTER_4444;
        }

        targetRasterFormat = ( doPackedDecompress ) ? packedAlphaRasterFormat : RASTER_8888;
    }
    else
    {
        eRasterFormat packedRasterFormat = RASTER_565;

        targetRasterFormat = ( doPackedDecompress ) ? packedRasterFormat : RASTER_888;
    }

    return targetRasterFormat;
}

}

#endif //_RENDERWARE_D3D_DXT_NATIVE_