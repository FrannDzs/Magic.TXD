/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.xbox.swizzle.cpp
*  PURPOSE:     XBOX native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_XBOX

#include "txdread.xbox.hxx"

#ifdef _USE_XBOX_SDK_
// Define this macro if you want this tool to use the official XBOX development kit.
#include <XGraphics.h>
#endif //_USE_XBOX_SDK_

namespace rw
{

#ifndef _USE_XBOX_SDK_
enum eBlockOrder
{
    BLOCK_XYZW,
    BLOCK_XZYW
};

struct XBOXSwizzle
{
    // TODO: find an efficient way to get swizzle coordinates just like that.
    static void getSwizzleCoord(uint32 x, uint32 y, uint32 width, uint32 height, uint32& swizzleX, uint32& swizzleY)
    {
        uint32 swizOutX = 0;
        uint32 swizOutY = 0;

        if ( width == 2 && height == 2 )
        {
            
        }
        

        swizzleX = swizOutX;
        swizzleY = swizOutY;
    }
};

// http://gtaforums.com/topic/213907-unswizzle-tool/
// TODO: fix this shit.
template <typename arrayType>
static inline void unswizzleXboxBlock
(
    const arrayType *srcData, arrayType *outData,
    uint32& srcDataOffset, uint32 outDataOffset,
    uint32 imWidth, uint32 imHeight, uint32 imStride,
    bool isUnswizzle,
    uint32 depthIter = 0
)
{
    // srcData must be a different array than outData.

    bool hasSubBlocks = true;

	if (imWidth == 2 && imHeight == 2 ||
        imWidth == 1 && imHeight == 2 ||
        imWidth == 2 && imHeight == 1 ||
        imWidth == 1 || imHeight == 1)
    {
        uint32 curDecodePos = srcDataOffset;

        for (uint32 y = 0; y < imHeight; y++)
        {
            uint32 seekOutOffsetHeight = ( y * imStride );

            for (uint32 x = 0; x < imWidth; x++)
            {
                uint32 seekOutOffset = seekOutOffsetHeight + x;

                arrayType::trav_t srcVal;

                // Decide which index to use where.
                uint32 srcIndex, dstIndex;
                
                if ( isUnswizzle )
                {
                    srcIndex = curDecodePos++;
                    dstIndex = outDataOffset + seekOutOffset;
                }
                else
                {
                    srcIndex = outDataOffset + seekOutOffset;
                    dstIndex = curDecodePos++;
                }

                srcData->getvalue(srcIndex, srcVal);

                outData->setvalue(dstIndex, srcVal);
            }
        }

        // Go to the next block.
		srcDataOffset += ( imWidth * imHeight );

        hasSubBlocks = false;
	}

    if ( hasSubBlocks )
    {
        // Catch an exception.
        if (imWidth == 1 || imHeight == 1)
        {
            assert( 0 );
        }

        // We split the picture into four parts, recursively.
        uint32 subImWidth = ( imWidth / 2 );
        uint32 subImHeight = ( imHeight / 2 );

        // Get the block ordering.
        eBlockOrder blockOrder;
        bool hasOrder = false;

        if ( imWidth == imHeight ||
             imWidth == imHeight * 4 ||
             imWidth * 4 == imHeight)
        {
            blockOrder = BLOCK_XYZW;
        }
        else
        {
            blockOrder = BLOCK_XZYW;
        }

        uint32 nextDepthIter = depthIter + 1;

        // Calculate the block coordinates to use.
        bool hasValidConfiguration = true;

        // Top left.
        uint32 topLeftOutOffset = outDataOffset;

        // Top right.
        uint32 topRightOutOffset = 0;

        if (blockOrder == BLOCK_XYZW)
        {
            topRightOutOffset = outDataOffset + subImWidth;
        }
        else if (blockOrder == BLOCK_XZYW)
        {
            topRightOutOffset = outDataOffset + subImHeight*imStride;
        }
        else
        {
            hasValidConfiguration = false;
        }

        // Bottom left.
        uint32 bottomLeftOutOffset = 0;

        if (blockOrder == BLOCK_XYZW)
        {
            bottomLeftOutOffset = outDataOffset + subImHeight*imStride;
        }
        else if (blockOrder == BLOCK_XZYW)
        {
            bottomLeftOutOffset = outDataOffset + subImWidth;
        }
        else
        {
            hasValidConfiguration = false;
        }

        // Bottom right.
        uint32 bottomRightOutOffset = outDataOffset + subImHeight*imStride + subImWidth;

        /*===============================*/

        if (hasValidConfiguration)
        {
            // Top left.
	        unswizzleXboxBlock(
                srcData, outData, srcDataOffset,
                topLeftOutOffset,
                subImWidth, subImHeight, imStride,
                isUnswizzle,
                nextDepthIter
            );

            if ( depthIter != 0 )
            {
                // Top right.
	            unswizzleXboxBlock(
                    srcData, outData, srcDataOffset,
                    topRightOutOffset,
                    subImWidth, subImHeight, imStride,
                    isUnswizzle,
                    nextDepthIter
                );

                // Bottom left.
	            unswizzleXboxBlock(
                    srcData, outData, srcDataOffset,
                    bottomLeftOutOffset,
                    subImWidth, subImHeight, imStride,
                    isUnswizzle,
                    nextDepthIter
                );

                // Bottom right.
	            unswizzleXboxBlock(
                    srcData, outData, srcDataOffset,
                    bottomRightOutOffset,
                    subImWidth, subImHeight, imStride,
                    isUnswizzle,
                    nextDepthIter
                );
            }
        }
	}
}
#endif //_USE_XBOX_SDK_

inline void performXBOXSwizzle(
    const void *srcData, void *outData,
    uint32 mipWidth, uint32 mipHeight, uint32 depth, uint32 rowAlignment,
    bool isUnswizzle
)
{
#ifndef _USE_XBOX_SDK_
    uint32 offOut = 0;

    if (depth == 4)
    {
        unswizzleXboxBlock(
            (const PixelFormat::palette4bit*)srcData,
            (PixelFormat::palette4bit*)outData,
            offOut, 0, mipWidth, mipHeight, mipWidth
        );
    }
    else if (depth == 8)
    {
        unswizzleXboxBlock(
            (const PixelFormat::palette8bit*)srcData,
            (PixelFormat::palette8bit*)outData,
            offOut, 0, mipWidth, mipHeight, mipWidth
        );
    }
    else if (depth == 16)
    {
        typedef PixelFormat::typedcolor <uint16> theColor;

        unswizzleXboxBlock(
            (const theColor*)srcData,
            (theColor*)outData,
            offOut, 0, mipWidth, mipHeight, mipWidth
        );
    }
    else if (depth == 24)
    {
        struct colorStruct
        {
            uint8 x, y, z;
        };

        typedef PixelFormat::typedcolor <colorStruct> theColor;

        unswizzleXboxBlock(
            (const theColor*)srcData,
            (theColor*)outData,
            offOut, 0, mipWidth, mipHeight, mipWidth
        );
    }
    else if (depth == 32)
    {
        typedef PixelFormat::typedcolor <uint32> theColor;

        unswizzleXboxBlock(
            (const theColor*)srcData,
            (theColor*)outData,
            offOut, 0, mipWidth, mipHeight, mipWidth
        );
    }
    else
    {
        assert( 0 );
    }
#else
    {
        Swizzler swizzler( mipWidth, mipHeight, 0 );

        rasterRowSize srcRowSize = getRasterDataRowSize( mipWidth, depth, rowAlignment );

        swizzler.SetV( 0 );

        for ( uint32 y = 0; y < mipHeight; y++, swizzler.IncV() )
        {
            swizzler.SetU( 0 );

            for ( uint32 x = 0; x < mipWidth; x++, swizzler.IncU() )
            {
                SWIZNUM swizzleIndex = swizzler.Get2D();

                // Transform it to x and y components.
                uint32 swizzleX = ( swizzleIndex % mipWidth );
                uint32 swizzleY = ( swizzleIndex / mipWidth );

                // Decide which index to use for which array.
                uint32 srcX, srcY;
                uint32 dstX, dstY;

                if ( isUnswizzle )
                {
                    srcX = swizzleX;
                    srcY = swizzleY;
                    dstX = x;
                    dstY = y;
                }
                else
                {
                    srcX = x;
                    srcY = y;
                    dstX = swizzleX;
                    dstY = swizzleY;
                }

                if ( dstX < mipWidth && dstY < mipHeight )
                {
                    rasterRow dstRow = getTexelDataRow( outData, srcRowSize, dstY );

                    if ( srcX < mipWidth && srcY < mipHeight )
                    {
                        constRasterRow srcRow = getConstTexelDataRow( srcData, srcRowSize, srcY );

                        dstRow.writeBitsFromRow( srcRow, srcX * depth, dstX * depth, depth );
                    }
                    else
                    {
                        dstRow.setBits( false, dstX * depth, depth );
                    }
                }
            }
        }
    }
#endif //_USE_XBOX_SDK_
}

void NativeTextureXBOX::swizzleMipmap( Interface *engineInterface, swizzleMipmapTraversal& pixelData )
{
    // We are a raw raster; take care about swizzling.
    uint32 depth = pixelData.depth;
    uint32 rowAlignment = pixelData.rowAlignment;
    
    uint32 mipWidth = pixelData.mipWidth;
    uint32 mipHeight = pixelData.mipHeight;

    // The dataSize will not change.
    uint32 dataSize = pixelData.dataSize;

    // Let's try allocating a new array for the swizzled texels.
    void *newtexels = engineInterface->PixelAllocate( dataSize );

    const void *srcTexels = pixelData.texels;

    // Do the permutation.
    performXBOXSwizzle(
        srcTexels, newtexels,
        mipWidth, mipHeight,
        depth, rowAlignment,
        false
    );

    // Give new stuff to the runtime.
    pixelData.newWidth = mipWidth;
    pixelData.newHeight = mipHeight;
    pixelData.newtexels = newtexels;
    pixelData.newDataSize = dataSize;
}

void NativeTextureXBOX::unswizzleMipmap( Interface *engineInterface, swizzleMipmapTraversal& pixelData )
{
    // unswizzle the mipmap layer.
    uint32 depth = pixelData.depth;
    uint32 rowAlignment = pixelData.rowAlignment;
    
    uint32 mipWidth = pixelData.mipWidth;
    uint32 mipHeight = pixelData.mipHeight;

    // The dataSize will not change.
    uint32 dataSize = pixelData.dataSize;

    // Let's try allocating a new array for the unswizzled texels.
    void *newtexels = engineInterface->PixelAllocate( dataSize );

    const void *srcTexels = pixelData.texels;

    // Do the permutation.
    performXBOXSwizzle(
        srcTexels, newtexels,
        mipWidth, mipHeight,
        depth, rowAlignment,
        true
    );

    // Give new stuff to the runtime.
    pixelData.newWidth = mipWidth;
    pixelData.newHeight = mipHeight;
    pixelData.newtexels = newtexels;
    pixelData.newDataSize = dataSize;
}

}

#endif //RWLIB_INCLUDE_NATIVETEX_XBOX