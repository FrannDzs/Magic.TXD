/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwimaging.utils.cpp
*  PURPOSE:     Imaging utilities implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include <sdk/UniChar.h>

#include "rwimaging.hxx"

namespace rw
{

bool IsImagingFormatExtension( uint32 num_ext, const imaging_filename_ext *ext_array, const char *query_ext )
{
    for ( uint32 n = 0; n < num_ext; n++ )
    {
        const imaging_filename_ext& entry = ext_array[ n ];

        if ( StringEqualToZero( entry.ext, query_ext, false ) )
        {
            return true;
        }
    }

    return false;
}

bool GetDefaultImagingFormatExtension( uint32 num_ext, const imaging_filename_ext *ext_array, const char*& out_ext )
{
    for ( uint32 n = 0; n < num_ext; n++ )
    {
        const imaging_filename_ext& entry = ext_array[ n ];

        if ( entry.isDefault )
        {
            out_ext = entry.ext;
            return true;
        }
    }

    return false;
}

const char* GetLongImagingFormatExtension( uint32 num_ext, const imaging_filename_ext *ext_array )
{
    const char *cur_ext = nullptr;
    size_t ext_len = 0;

    for ( uint32 n = 0; n < num_ext; n++ )
    {
        const imaging_filename_ext& entry = ext_array[ n ];

        size_t len = strlen( entry.ext );

        if ( !cur_ext || ext_len < len )
        {
            cur_ext = entry.ext;
            ext_len = len;
        }
    }

    return cur_ext;
}

};
