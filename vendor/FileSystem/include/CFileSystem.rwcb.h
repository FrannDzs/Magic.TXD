/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.rwcb.h
*  PURPOSE:     Stream that is powered by user-provided callbacks.
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_RWCALLBACK_STREAM_PUBLIC_HEADER_
#define _FILESYSTEM_RWCALLBACK_STREAM_PUBLIC_HEADER_

namespace FileSystem
{

// Simpler CFile stream interface that can be inherited from by user-code.
// Mainly created to have a stable interface for the unit tests.
struct readWriteCallbackInterface
{
    virtual size_t Read( fsOffsetNumber_t seek, void *dstbuf, size_t cnt ) = 0;
    virtual size_t Write( fsOffsetNumber_t seek, const void *srcbuf, size_t cnt ) = 0;
};

} // namespace FileSystem

#endif //_FILESYSTEM_RWCALLBACK_STREAM_PUBLIC_HEADER_