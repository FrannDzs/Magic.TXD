/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.common.h
*  PURPOSE:     Common definitions used by the FileSystem module
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_INTERNAL_COMMON_DEFINITIONS_
#define _FILESYSTEM_INTERNAL_COMMON_DEFINITIONS_

#include <sdk/MultiString.h>

// The internal version of filePath.
typedef eir::MultiString <FSObjectHeapAllocator> internalFilePath;

#endif //_FILESYSTEM_INTERNAL_COMMON_DEFINITIONS_
