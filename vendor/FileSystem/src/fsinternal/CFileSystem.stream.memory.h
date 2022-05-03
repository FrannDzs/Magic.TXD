/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.memory.h
*  PURPOSE:     Memory-based read/write stream
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_MEMORY_STREAM_
#define _FILESYSTEM_MEMORY_STREAM_

#include <sdk/MemoryUtils.stream.h>
#include <sdk/OSUtils.h>

typedef NativePageAllocator memoryMappedNativePageAllocator;

// This is a native implementation of memory-based file I/O.
// You can use it if you want to temporarily do abstract read and write operations
// in the system RAM, the fastest storage available to you.
// Using RAM has its advantages because it does not decay your storage media.
struct CMemoryMappedFile : public CFile
{
    CMemoryMappedFile( CFileSystemNative *fileSys );
    ~CMemoryMappedFile( void );

    size_t Read( void *buffer, size_t readCount ) override;
    size_t Write( const void *buffer, size_t writeCount ) override;

    int Seek( long offset, int iType ) override;
    int SeekNative( fsOffsetNumber_t offset, int iType ) override;

    long Tell( void ) const override;
    fsOffsetNumber_t TellNative( void ) const override;

    bool IsEOF( void ) const override;

    bool QueryStats( filesysStats& statsOut ) const override;
    void SetFileTimes( time_t atime, time_t ctime, time_t mtime ) override;

    void SetSeekEnd( void ) override;

    size_t GetSize( void ) const override;
    fsOffsetNumber_t GetSizeNative( void ) const override;

    void Flush( void ) override;

    CFileMappingProvider* CreateMapping( void ) override;

    filePath GetPath( void ) const override;

    bool IsReadable( void ) const noexcept override;
    bool IsWriteable( void ) const noexcept override;

    // INSECURE API.
    const void* GetBuffer( size_t& sizeOut ) const;

private:
    struct streamManagerExtension
    {
        memoryMappedNativePageAllocator::pageHandle *currentMemory;
        size_t currentMemorySize;

        CFileSystemNative *manager;

        streamManagerExtension( CFileSystemNative *fileSys );
        ~streamManagerExtension( void );

        void EstablishBufferView( void*& memBuffer, fsOffsetNumber_t& curSize, fsOffsetNumber_t reqSize );
    };

    streamManagerExtension manExt;

    memoryBufferStream <fsOffsetNumber_t, streamManagerExtension, false> memStream;

    // We have to keep some meta data.
    time_t meta_atime;
    time_t meta_mtime;
    time_t meta_ctime;
};

#endif //_FILESYSTEM_MEMORY_STREAM_