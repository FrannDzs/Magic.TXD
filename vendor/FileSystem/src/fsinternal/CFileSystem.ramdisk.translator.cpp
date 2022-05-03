/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.img.translator.cpp
*  PURPOSE:     Anonymous on-memory filesystem tree implementation
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

#include "CFileSystem.ramdisk.internal.h"

// Provides a basic memory-only filesystem tree that is used for generarilly
// non-persistent data storage.

CFileTranslator* CFileSystem::CreateRamdisk( bool isCaseSensitive )
{
    CRamdiskTranslator *trans = new CRamdiskTranslator( isCaseSensitive );

    return trans;
}