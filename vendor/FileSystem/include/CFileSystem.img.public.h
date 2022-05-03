/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/include/CFileSystem.img.public.h
*  PURPOSE:     IMG R* Games archive management
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_PUBLIC_IMG_HEADER_
#define _FILESYSTEM_PUBLIC_IMG_HEADER_

enum eIMGArchiveVersion
{
    IMG_VERSION_1,
    IMG_VERSION_2,
    IMG_VERSION_FASTMAN92
};

// Interface to handle compression of IMG archive (for XBOX Vice City and III)
struct CIMGArchiveCompressionHandler abstract
{
    virtual bool        IsStreamCompressed( CFile *stream ) const = 0;

    virtual bool        Decompress( CFile *inputStream, CFile *outputStream ) = 0;
    virtual bool        Compress( CFile *inputStream, CFile *outputStream ) = 0;
};

class CIMGArchiveTranslatorHandle abstract : public CArchiveTranslator
{
public:
    // Special functions just available for IMG archives.
    virtual void        SetCompressionHandler( CIMGArchiveCompressionHandler *handler ) = 0;

    virtual eIMGArchiveVersion  GetVersion( void ) const = 0;

    // Iterate through all files in serialization order.
    virtual void        ScanFilesInSerializationOrder( pathCallback_t filecb, void *ud ) const = 0;
};


#endif //_FILESYSTEM_PUBLIC_IMG_HEADER_