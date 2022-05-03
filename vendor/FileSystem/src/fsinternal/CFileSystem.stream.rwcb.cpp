/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.rwcb.cpp
*  PURPOSE:     Stream that is powered by user-provided callbacks.
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

#include <sdk/MemoryRaw.h>

namespace FileSystem
{

struct CReadWriteCallbackStream : public CFile
{
    inline CReadWriteCallbackStream( readWriteCallbackInterface *intf ) : seek( 0 ), usercb( intf )
    {
        return;
    }
    CReadWriteCallbackStream( const CReadWriteCallbackStream& ) = delete;
    CReadWriteCallbackStream( CReadWriteCallbackStream&& ) = delete;

    inline ~CReadWriteCallbackStream( void )
    {
        return;
    }

    CReadWriteCallbackStream& operator = ( const CReadWriteCallbackStream& ) = delete;
    CReadWriteCallbackStream& operator = ( CReadWriteCallbackStream&& ) = delete;

    size_t Read( void *buf, size_t cnt ) override
    {
        fsOffsetNumber_t seek = this->seek;

        size_t readcnt = this->usercb->Read( seek, buf, cnt );

        this->seek = ( seek + TRANSFORM_INT_CLAMP <fsOffsetNumber_t> ( readcnt ) );

        return readcnt;
    }

    size_t Write( const void *buf, size_t cnt ) override
    {
        fsOffsetNumber_t seek = this->seek;

        size_t writecnt = this->usercb->Write( seek, buf, cnt );

        this->seek = ( seek + TRANSFORM_INT_CLAMP <fsOffsetNumber_t> ( writecnt ) );

        return writecnt;
    }

    int Seek( long off, int type ) override
    {
        fsOffsetNumber_t base;

        if ( type == SEEK_SET )
        {
            base = 0;
        }
        else if ( type == SEEK_CUR )
        {
            base = this->TellNative();
        }
        else if ( type == SEEK_END )
        {
            base = this->GetSizeNative();
        }
        else
        {
            return -1;
        }

        this->seek = ( base + off );

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
            base = this->TellNative();
        }
        else if ( type == SEEK_END )
        {
            base = this->GetSizeNative();
        }
        else
        {
            return -1;
        }

        this->seek = ( base + off );

        return 0;
    }

    long Tell( void ) const override
    {
        return (long)this->seek;
    }

    fsOffsetNumber_t TellNative( void ) const override
    {
        return this->seek;
    }

    bool IsEOF( void ) const override
    {
        throw filesystem_exception( eGenExceptCode::UNSUPPORTED_OPERATION );
    }

    bool QueryStats( filesysStats& attribOut ) const override
    {
        return false;
    }

    void SetFileTimes( time_t atime, time_t ctime, time_t mtime ) override
    {
        throw filesystem_exception( eGenExceptCode::UNSUPPORTED_OPERATION );
    }

    void SetSeekEnd( void ) override
    {
        throw filesystem_exception( eGenExceptCode::UNSUPPORTED_OPERATION );
    }

    size_t GetSize( void ) const override
    {
        throw filesystem_exception( eGenExceptCode::UNSUPPORTED_OPERATION );
    }

    fsOffsetNumber_t GetSizeNative( void ) const override
    {
        throw filesystem_exception( eGenExceptCode::UNSUPPORTED_OPERATION );
    }

    void Flush( void ) override
    {
        // Flushing is done automatically.
    }

    CFileMappingProvider* CreateMapping( void ) override
    {
        return nullptr;
    }

    filePath GetPath( void ) const override
    {
        throw filesystem_exception( eGenExceptCode::UNSUPPORTED_OPERATION );
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
    fsOffsetNumber_t seek;
    readWriteCallbackInterface *usercb;
};

} // namespace FileSystem

CFile* CFileSystem::CreateRWCallbackFile( FileSystem::readWriteCallbackInterface *intf )
{
    if ( intf == nullptr )
    {
        return nullptr;
    }

    return new FileSystem::CReadWriteCallbackStream( intf );
}