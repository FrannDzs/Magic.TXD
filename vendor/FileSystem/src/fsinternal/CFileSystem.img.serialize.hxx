/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.img.serialize.hxx
*  PURPOSE:     Serialization structures of the IMG container (on-disk)
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYS_IMG_SERIALIZATION_HEADERS_
#define _FILESYS_IMG_SERIALIZATION_HEADERS_

struct imgMainHeaderVer2
{
    unsigned char checksum[4];
    endian::little_endian <fsUInt_t> fileCount;
};

#endif //_FILESYS_IMG_SERIALIZATION_HEADERS_