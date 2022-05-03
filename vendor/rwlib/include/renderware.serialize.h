/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.serialize.h
*  PURPOSE:     Serialization definitions for chunks/blocks.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_SERIALIZATION_PUBLIC_HEADER_
#define _RENDERWARE_SERIALIZATION_PUBLIC_HEADER_

namespace rw
{

// Registration of POD chunk types.
bool RegisterPODSerialization(
    Interface *engineInterface,
    uint32 chunkID, const wchar_t *tokenChunkName
);
// Unregistration of serialization.
// Do not unregister things that you did not register.
bool UnregisterSerialization(
    Interface *engineInterface,
    uint32 chunkID
);

} // namespace rw

#endif //_RENDERWARE_SERIALIZATION_PUBLIC_HEADER_