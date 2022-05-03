/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/qtfilesystem.h
*  PURPOSE:     Header of the Qt filesystem wrapper.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef _QT_FILESYSTEM_EMBEDDING_HEADER_
#define _QT_FILESYSTEM_EMBEDDING_HEADER_

#include <CFileSystem.h>

void register_file_translator( CFileTranslator *source );
void unregister_file_translator( CFileTranslator *source );

#endif //_QT_FILESYSTEM_EMBEDDING_HEADER_