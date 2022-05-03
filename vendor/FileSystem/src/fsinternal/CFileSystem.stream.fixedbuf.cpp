/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.fixedbuf.cpp
*  PURPOSE:     User-buffer based memory stream (non-expanding)
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

#include <sdk/MemoryUtils.stream.h>

// Since this is a really simple thing we do not provide global coverage.
struct CUserBufferFile final : public CFile
{
    inline CUserBufferFile( void *memPtr, fsOffsetNumber_t memSize ) : stream( memPtr, memSize, bufMan )
    {
        return;
    }

    size_t Read( void *buf, size_t readCount ) override
    {
        return stream.Read( buf, readCount );
    }

    size_t Write( const void *buf, size_t writeCount ) override
    {
        return stream.Write( buf, writeCount );
    }

    int Seek( long off, int type ) override
    {
        long base;

        if ( type == SEEK_SET )
        {
            base = 0;
        }
        else if ( type == SEEK_CUR )
        {
            base = (long)this->stream.Tell();
        }
        else if ( type == SEEK_END )
        {
            base = (long)this->stream.Size();
        }
        else
        {
            return -1;
        }

        this->stream.Seek( (long)( base + off ) );

        return 0;
    }

    int SeekNative( fsOffsetNumber_t off, int type ) override
    {
        fsOffsetNumber_t base;

        if ( type == SEEK_SET )
        {
            base = 0;
        }
        else if ( type == SEEK_CUR )
        {
            base = this->stream.Tell();
        }
        else if ( type == SEEK_END )
        {
            base = this->stream.Size();
        }
        else
        {
            return -1;
        }

        this->stream.Seek( base + off );

        return 0;
    }

    long Tell( void ) const override
    {
        return (long)this->stream.Tell();
    }

    fsOffsetNumber_t TellNative( void ) const override
    {
        return this->stream.Tell();
    }

    bool IsEOF( void ) const override
    {
        return ( this->stream.Tell() >= this->stream.Size() );
    }

    bool QueryStats( filesysStats& statsOut ) const override
    {
        statsOut.atime = 0;
        statsOut.ctime = 0;
        statsOut.mtime = 0;
        statsOut.attribs.type = eFilesysItemType::FILE;
        // TODO: fill this out more, especially file times.
        return true;
    }

    void SetFileTimes( time_t atime, time_t ctime, time_t mtime ) override
    {
        return;
    }

    void SetSeekEnd( void ) override
    {
        this->stream.Truncate( this->stream.Tell() );
    }

    size_t GetSize( void ) const override
    {
        return (size_t)this->stream.Size();
    }

    fsOffsetNumber_t GetSizeNative( void ) const override
    {
        return this->stream.Size();
    }

    void Flush( void ) override
    {
        return;
    }

    CFileMappingProvider* CreateMapping( void ) override
    {
        // TODO: maybe add support for this.
        return nullptr;
    }

    filePath GetPath( void ) const override
    {
        return "<memory>";
    }

    bool IsReadable( void ) const noexcept override
    {
        return true;
    }

    bool IsWriteable( void ) const noexcept override
    {
        return true;
    }

private:
    struct nullBufferManager
    {
        static AINLINE void EstablishBufferView( void*& memPtrInOut, fsOffsetNumber_t& sizeInOut, fsOffsetNumber_t newReqSize )
        {
            return;
        }
    };

    nullBufferManager bufMan;
    memoryBufferStream <fsOffsetNumber_t, nullBufferManager, false, false> stream;
};

CFile* CFileSystem::CreateUserBufferFile( void *buf, size_t bufSize )
{
    if ( bufSize > (std::make_unsigned <fsOffsetNumber_t>::type)( std::numeric_limits <fsOffsetNumber_t>::max() ) )
    {
        return nullptr;
    }

    fsOffsetNumber_t realSize = (fsOffsetNumber_t)bufSize;

    return new CUserBufferFile( buf, realSize );
}