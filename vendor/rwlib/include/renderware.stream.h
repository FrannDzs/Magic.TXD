/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.stream.h
*  PURPOSE:     Stream object definitions
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_STREAM_HEADER_
#define _RENDERWARE_STREAM_HEADER_

#include "renderware.errsys.common.h"

namespace rw
{

enum eBuiltinStreamType
{
    RWSTREAMTYPE_FILE,
    RWSTREAMTYPE_FILE_W,
    RWSTREAMTYPE_MEMORY,
    RWSTREAMTYPE_CUSTOM
};

enum eStreamMode
{
    RWSTREAMMODE_READONLY,
    RWSTREAMMODE_READWRITE,
    RWSTREAMMODE_WRITEONLY,
    RWSTREAMMODE_CREATE
};

struct streamConstructionParam_t abstract
{
    uint32 dwSize;      // size in bytes of this struct

    // add your members here.
};

struct streamConstructionFileParam_t : public streamConstructionParam_t
{
    inline streamConstructionFileParam_t( const char *filename )
    {
        this->dwSize = sizeof( *this );
        this->filename = filename;
    }

    const char *filename;
};

struct streamConstructionFileParamW_t : public streamConstructionParam_t
{
    inline streamConstructionFileParamW_t( const wchar_t *filename )
    {
        this->dwSize = sizeof( *this );
        this->filename = filename;
    }

    const wchar_t *filename;
};

struct streamConstructionMemoryParam_t : public streamConstructionParam_t
{
    inline streamConstructionMemoryParam_t( void *buf, size_t bufSize )
    {
        this->dwSize = sizeof( *this );
        this->buf = buf;
        this->bufSize = bufSize;
    }

    void *buf;
    size_t bufSize;
};

struct streamConstructionCustomParam_t : public streamConstructionParam_t
{
    inline streamConstructionCustomParam_t( const char *typeName, void *ud )
    {
        this->dwSize = sizeof( *this );
        this->typeName = typeName;
        this->userdata = ud;
    }

    const char *typeName;
    void *userdata;
};

enum eSeekMode
{
    RWSEEK_BEG,
    RWSEEK_CUR,
    RWSEEK_END
};

struct customStreamInterface abstract
{
    // General stream type management routines.
    virtual void OnConstruct( eStreamMode streamMode, void *userdata, void *memBuf, size_t memSize ) const = 0;
    virtual void OnDestruct( void *memBuf, size_t memSize ) const = 0;

    // Stream API.
    virtual size_t Read( void *memBuf, void *out_buf, size_t readCount ) const = 0;
    virtual size_t Write( void *memBuf, const void *in_buf, size_t writeCount ) const = 0;

    virtual void Skip( void *memBuf, int64 skipCount ) const = 0;

    // Advanced stream API.
    virtual int64 Tell( const void *memBuf ) const = 0;
    virtual void Seek( void *memBuf, int64 stream_offset, eSeekMode seek_mode ) const = 0;

    virtual int64 Size( const void *memBuf ) const
    {
        throw UnsupportedOperationException( eSubsystemType::STREAM, eOperationType::GETSIZE, nullptr );
    }

    virtual bool SupportsSize( const void *memBuf ) const
    {
        return false;
    }
};

struct Stream abstract
{
    inline Stream( Interface *engineInterface, void *construction_params )
    {
        this->engineInterface = engineInterface;
    }

    inline Stream( const Stream& right )
    {
        throw InvalidOperationException( eSubsystemType::STREAM, eOperationType::COPY_CONSTRUCT, nullptr );
    }

    inline void operator = ( const Stream& right )
    {
        throw InvalidOperationException( eSubsystemType::STREAM, eOperationType::ASSIGN, nullptr );
    }

    inline ~Stream( void )
    {
        // TODO.
        return;
    }

    inline Interface* getEngine( void ) const
    {
        return this->engineInterface;
    }

    // Pointer to the engine.
    Interface *engineInterface;

    // Data processing algorithms.
    virtual size_t read( void *out_buf, size_t readCount );
    virtual size_t write( const void *in_buf, size_t writeCount );

    virtual void skip( int64 skipCount );

    // More advanced methods.
    virtual int64 tell( void ) const;
    virtual void seek( int64 seek_off, eSeekMode seek_mode );

    virtual int64 size( void ) const;

    // Capability functions.
    virtual bool supportsSize( void ) const;
};

} // namespace rw

#endif //_RENDERWARE_STREAM_HEADER_