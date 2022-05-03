/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/native.win32.hxx
*  PURPOSE:     Header to include windows stuff into the project.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

// DO NOT INCLUDE THIS OR windows.h INTO THE GLOBAL NAMESPACE.

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#endif //_WIN32