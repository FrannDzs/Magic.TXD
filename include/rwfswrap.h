/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/rwfswrap.h
*  PURPOSE:     Header of the editor magic-rw filesystem wrapper.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef _RENDERWARE_FILESYSTEM_STREAM_WRAP_
#define _RENDERWARE_FILESYSTEM_STREAM_WRAP_

bool RawGlobalFileExists( const filePath& thePath );
CFile* OpenGlobalFile( MainWindow *mainWnd, const filePath& path, const filePath& mode );

rw::Stream* RwStreamCreateTranslated( rw::Interface *rwEngine, CFile *stream );

#endif //_RENDERWARE_FILESYSTEM_STREAM_WRAP_