/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.raw.h
*  PURPOSE:     Raw OS filesystem file link
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_RAW_OS_LINK_
#define _FILESYSTEM_RAW_OS_LINK_

class CRawFile : public CFile
{
public:
                            CRawFile        ( filePath absFilePath, filesysAccessFlags flags );
                            ~CRawFile       ( void );

    size_t                  Read            ( void *buffer, size_t readCount ) override;
    size_t                  Write           ( const void *buffer, size_t writeCount ) override;
    int                     Seek            ( long iOffset, int iType ) override;
    int                     SeekNative      ( fsOffsetNumber_t iOffset, int iType ) override;
    long                    Tell            ( void ) const override;
    fsOffsetNumber_t        TellNative      ( void ) const override;
    bool                    IsEOF           ( void ) const override;
    bool                    QueryStats      ( filesysStats& statsOut ) const override;
    void                    SetFileTimes    ( time_t atime, time_t ctime, time_t mtime ) override;
    void                    SetSeekEnd      ( void ) override;
    size_t                  GetSize         ( void ) const override;
    fsOffsetNumber_t        GetSizeNative   ( void ) const override;
    void                    Flush           ( void ) override;
    CFileMappingProvider*   CreateMapping   ( void ) override;
    filePath                GetPath         ( void ) const override;
    bool                    IsReadable      ( void ) const noexcept override;
    bool                    IsWriteable     ( void ) const noexcept override;

private:
    friend class CSystemFileTranslator;
    friend class CFileSystem;

#ifdef _WIN32
    HANDLE              m_file;
#elif defined(__linux__)
    int                 m_fileIndex;
#endif //OS DEPENDANT CODE
    filesysAccessFlags  m_access;
    filePath            m_path;
};

#endif //_FILESYSTEM_RAW_OS_LINK_
