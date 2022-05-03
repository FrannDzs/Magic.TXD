/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwfile.system.hxx
*  PURPOSE:     Basic RenderWare data repository system so subsystems can carry
*               shaders and things with them.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

namespace rw
{

#ifndef _RENDERWARE_FILE_SYSTEM_INTERNAL_
#define _RENDERWARE_FILE_SYSTEM_INTERNAL_

namespace fs
{

// RW-wide data access interface.
Stream* OpenDataStream( EngineInterface *engineInterface, const wchar_t *filePath, eStreamMode mode );
FileTranslator::fileList_t GetDirectoryListing( EngineInterface *rwEngine, const wchar_t *dir, const wchar_t *file_pattern, bool recursive = false );

}

#endif //_RENDERWARE_FILE_SYSTEM_INTERNAL_

}