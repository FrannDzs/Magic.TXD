/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.d3d9.formats.cpp
*  PURPOSE:     Direct3D 9 native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9

#include "txdread.d3d9.hxx"

namespace rw
{

bool d3d9NativeTextureTypeProvider::RegisterFormatHandler( uint32 formatNumber, d3dpublic::nativeTextureFormatHandler *handler )
{
    D3DFORMAT format = (D3DFORMAT)formatNumber;

    bool success = false;

    // There can be only one D3DFORMAT linked to any handler.
    // Also, a handler is expected to be only linked to one D3DFORMAT.
    {
        // Check whether this format is already handled.
        // If it is, then ignore shit.
        d3dpublic::nativeTextureFormatHandler *alreadyHandled = this->GetFormatHandler( format );

        if ( alreadyHandled == nullptr )
        {
            // Go ahead. Register it.
            nativeFormatExtension ext;
            ext.theFormat = format;
            ext.handler = handler;

            this->formatExtensions.AddToBack( std::move( ext ) );

            // Yay!
            success = true;
        }
    }

    return success;
}

bool d3d9NativeTextureTypeProvider::UnregisterFormatHandler( uint32 formatNumber )
{
    D3DFORMAT format = (D3DFORMAT)formatNumber;

    bool success = false;

    // We go through the list of registered formats and remove the ugly one.
    {
        size_t numExtensions = this->formatExtensions.GetCount();

        for ( size_t n = 0; n < numExtensions; n++ )
        {
            const nativeFormatExtension& ext = this->formatExtensions[ n ];

            if ( ext.theFormat == format )
            {
                // Remove this.
                this->formatExtensions.RemoveByIndex( n );

                success = true;
                break;
            }
        }
    }

    return success;
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_D3D9