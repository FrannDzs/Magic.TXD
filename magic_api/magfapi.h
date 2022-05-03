/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        magic_api/magfapi.h
*  PURPOSE:     Helper structurs for dealing with Direct3D texture data.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef _MAGIC_FORMAT_API_
#define _MAGIC_FORMAT_API_

#include <d3d9.h>

// Calculate the stride of a bitmap raster.
// The stride is used to advance through the texture row by row.
inline size_t getD3DBitmapStride( unsigned int width, unsigned int depth )
{
    // Get the amount of bits a row takes.
    unsigned int bitsByRow = ( width * depth );

    // Calculate how many bytes that is.
    unsigned int bytesByRow = ( ( bitsByRow + 7 ) / 8 );

    // Align it to the Direct3D row padding.
    const unsigned int d3dRowPadding = sizeof( DWORD );

    unsigned int alignedBytesByRow = ( ( bytesByRow + ( d3dRowPadding - 1 ) ) / d3dRowPadding ) * d3dRowPadding;

    // Return it.
    return alignedBytesByRow;
}

// Calculate the size of a bitmap raster.
inline size_t getD3DBitmapDataSize( unsigned int width, unsigned int height, unsigned int depth )
{
    // The bitmap size is the stride multiplied by the row.
    size_t bitmapStride = getD3DBitmapStride( width, depth );

    return ( bitmapStride * height );
}

// Returns a row of a bitmap raster.
inline void* getD3DBitmapRow( void *texelData, size_t stride, unsigned int row )
{
    return ( (char*)texelData + stride * row );
}

inline const void* getD3DBitmapConstRow( const void *texelData, size_t stride, unsigned int row )
{
    return ( (const char*)texelData + stride * row );
}

#endif //_MAGIC_FORMAT_API_