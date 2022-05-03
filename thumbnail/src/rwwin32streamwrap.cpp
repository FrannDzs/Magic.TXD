#include "StdInc.h"

struct win32StreamEnv
{
    struct meta_obj
    {
        inline meta_obj( IStream *stream ) : stream( stream )
        {}

        IStream *stream;
    };

    struct stream_handler : public rw::customStreamInterface
    {
        void OnConstruct( rw::eStreamMode streamMode, void *userdata, void *memBuf, size_t memSize ) const override
        {
            meta_obj *fsObj = new (memBuf) meta_obj( (IStream*)userdata );
        }

        void OnDestruct( void *memBuf, size_t memSize ) const override
        {
            meta_obj *fsObj = (meta_obj*)memBuf;

            fsObj->~meta_obj();
        }

        size_t Read( void *memBuf, void *out_buf, size_t readCount ) const override
        {
            const meta_obj *fsObj = (const meta_obj*)memBuf;

            ULONG actualReadCount = (ULONG)readCount;
            ULONG actualResultReadCount = 0;

            HRESULT readResult = fsObj->stream->Read( out_buf, actualReadCount, &actualResultReadCount );

            switch( readResult )
            {
            case S_OK:
                // We succeeded. Yay!
                break;
            case S_FALSE:
                // :(
                return 0;
            case E_PENDING:
                // Well, I guess it works? It means there is more data to come?
                break;
            default:
                // Some shit we do not handle.
                return 0;
            }

            return (size_t)actualResultReadCount;
        }

        size_t Write( void *memBuf, const void *in_buf, size_t writeCount ) const override
        {
            const meta_obj *fsObj = (const meta_obj*)memBuf;

            ULONG actualWriteCount = (ULONG)writeCount;
            ULONG actualDoneWrittenCount = 0;

            HRESULT writeResult = fsObj->stream->Write( in_buf, actualWriteCount, &actualDoneWrittenCount );

            switch( writeResult )
            {
            case S_OK:
                // :-)
                break;
            case S_FALSE:
                // :-/
                return 0;
            case E_PENDING:
                // Meh.
                break;
            default:
                // Eh...?
                return 0;
            }

            return (size_t)actualDoneWrittenCount;
        }

        void Skip( void *memBuf, rw::int64 skipCount ) const override
        {
            const meta_obj *fsObj = (const meta_obj*)memBuf;

            assert( skipCount > 0 );

            LARGE_INTEGER dlibMove;
            dlibMove.QuadPart = skipCount;

            ULARGE_INTEGER new_pos;

            HRESULT seekResult = fsObj->stream->Seek( dlibMove, STREAM_SEEK_CUR, &new_pos );

            // We do not care about the result, because the runtime can check for success in other ways.
        }

        rw::int64 Tell( const void *memBuf ) const override
        {
            const meta_obj *fsObj = (const meta_obj*)memBuf;

            LARGE_INTEGER move_cnt;
            move_cnt.QuadPart = 0L;

            ULARGE_INTEGER curPos;

            HRESULT tellResult = fsObj->stream->Seek( move_cnt, STREAM_SEEK_CUR, &curPos );

            return (rw::int64)curPos.QuadPart;
        }

        void Seek( void *memBuf, rw::int64 stream_offset, rw::eSeekMode seek_mode ) const override
        {
            const meta_obj *fsObj = (const meta_obj*)memBuf;

            LARGE_INTEGER off_cnt;
            off_cnt.QuadPart = stream_offset;

            ULARGE_INTEGER newPos;

            // Translate what to do with the stream.
            DWORD win32_seek_mode = STREAM_SEEK_SET;

            switch( seek_mode )
            {
            case rw::RWSEEK_BEG:
                win32_seek_mode = STREAM_SEEK_SET;
                break;
            case rw::RWSEEK_CUR:
                win32_seek_mode = STREAM_SEEK_CUR;
                break;
            case rw::RWSEEK_END:
                win32_seek_mode = STREAM_SEEK_END;
                break;
            default:
                assert( 0 );
            }

            HRESULT seekResult = fsObj->stream->Seek( off_cnt, win32_seek_mode, &newPos );

            // Let the user figure out errors.
        }

        rw::int64 Size( const void *memBuf ) const override
        {
            const meta_obj *fsObj = (const meta_obj*)memBuf;

            STATSTG fileStat;

            HRESULT statsResult = fsObj->stream->Stat( &fileStat, STATFLAG_NONAME );

            if ( statsResult != S_OK )
                return 0;

            return (rw::int64)fileStat.cbSize.QuadPart;
        }

        bool SupportsSize( const void *memBuf ) const override
        {
            return true;
        }
    };

    stream_handler _handler;

    inline void Initialize( rw::Interface *engineInterface )
    {
        engineInterface->RegisterStream( "win32_IStream", sizeof( meta_obj ), &_handler );
    }

    inline void Shutdown( rw::Interface *engineInterface )
    {
        return;
    }
};

static win32StreamEnv streamEnv;

rw::Stream* RwStreamCreateFromWin32( rw::Interface *engineInterface, IStream *pStream )
{
    rw::streamConstructionCustomParam_t customParam( "win32_IStream", pStream );

    return engineInterface->CreateStream( rw::RWSTREAMTYPE_CUSTOM, rw::RWSTREAMMODE_READWRITE, &customParam );
}

void InitializeRwWin32StreamEnv( void )
{
    streamEnv.Initialize( rwEngine );
}

void ShutdownRwWin32StreamEnv( void )
{
    streamEnv.Shutdown( rwEngine );
}