/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/include/CFileSystem.common.filePath.h
*  PURPOSE:     Path resolution logic of the FileSystem module
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_COMMON_PATHRESOLUTION_
#define _FILESYSTEM_COMMON_PATHRESOLUTION_

#include <sdk/MultiString.h>

#include "CFileSystem.common.alloc.h"

typedef eir::MultiString <FileSysCommonAllocator> filePath;

template <typename funcType>
AINLINE decltype( auto ) filePath_dispatch( const filePath& path, const funcType& b )
{
    if ( const char *sysPath = path.c_str() )
    {
        return b( sysPath );
    }
    else if ( const wchar_t *sysPath = path.w_str() )
    {
        return b( sysPath );
    }
    else if ( const char8_t *sysPath = path.u8_str() )
    {
        return b( sysPath );
    }

    // Unicode is supposed to be able to display anything.
    auto unicode = path.convert_unicode <FileSysCommonAllocator> ();

    return b( unicode.GetConstString() );
}

template <typename charType>
struct resolve_type
{
    typedef charType type;
};

template <typename charType>
struct resolve_type <const charType*>
    : public resolve_type <charType>
{};

template <typename funcType>
AINLINE decltype( auto ) filePath_dispatchTrailing( const filePath& leading, const filePath& trailing, const funcType& cb )
{
    return filePath_dispatch( leading,
        [&] ( auto leading )
        {
            typedef typename resolve_type <decltype(leading)>::type charType;

            filePath trailingLink( trailing );

            trailingLink.transform_to <charType> ();

            return cb( leading, trailingLink.to_char <charType> () );
        }
    );
}

#endif //_FILESYSTEM_COMMON_PATHRESOLUTION_