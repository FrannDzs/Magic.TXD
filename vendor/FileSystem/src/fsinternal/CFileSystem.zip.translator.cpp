/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.zip.translator.cpp
*  PURPOSE:     ZIP archive filesystem
*
*  Docu: https://users.cs.jmu.edu/buchhofp/forensics/formats/pkzip.html
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include <StdInc.h>

#ifdef FILESYS_ENABLE_ZIP
#include <zlib.h>
#include <sys/stat.h>
#endif //FILESYS_ENABLE_ZIP

// Include internal (private) definitions.
#include "fsinternal/CFileSystem.internal.h"
#include "fsinternal/CFileSystem.zip.internal.h"

#include "fsinternal/CFileSystem.stream.chunkbuf.h"

extern CFileSystem *fileSystem;

#include "CFileSystem.Utils.hxx"
#include "CFileSystem.zip.utils.hxx"

#include <sdk/MemoryUtils.stream.h>

// For std::max_align_t
#include <cstddef>

using namespace FileSystem;

/*=======================================
    CZIPArchiveTranslator::fileDeflate

    ZIP file seeking and handling
=======================================*/

struct compression_progress
{
    fsUInt_t sizeCompressed;
    fsUInt_t crc32val;
};

#ifdef FILESYS_ENABLE_ZIP

// TODO: actually make those zlib allocation functions point to a user-defined callback by FileSystem.
static void* zlib_malloc( void *mgr, unsigned int acnt, unsigned int asize )
{
    return fileSystem->MemAlloc( acnt * asize, sizeof(std::max_align_t) );
}

static void zlib_free( void *mgr, void *ptr )
{
    fileSystem->MemFree( ptr );
}

struct zip_inflate_decompression
{
    zip_inflate_decompression( bool hasHeader )
    {
        m_stream.zalloc = zlib_malloc;
        m_stream.zfree = zlib_free;
        m_stream.opaque = nullptr;

        if ( inflateInit2( &m_stream, hasHeader ? MAX_WBITS : -MAX_WBITS ) != Z_OK )
        {
            throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
        }
    }
    zip_inflate_decompression( zip_inflate_decompression&& right ) = delete;

    ~zip_inflate_decompression( void )
    {
        inflateEnd( &m_stream );
    }

    inline void reset( void )
    {
        inflateReset( &m_stream );
    }

    inline void prepare( const void *buf, size_t size, bool eof )
    {
        m_stream.avail_in = (uInt)size;
        m_stream.next_in = (Bytef*)buf;
    }

    inline void do_inflate( void )
    {
        int result = inflate( &m_stream, Z_FULL_FLUSH );

        switch( result )
        {
        case Z_STREAM_ERROR:
            throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
        case Z_DATA_ERROR:
        case Z_NEED_DICT:
            throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
        case Z_MEM_ERROR:
            throw filesystem_exception( eGenExceptCode::MEMORY_INSUFFICIENT );
        }
    }

    inline bool parse( void *buf, size_t size, size_t& sout )
    {
        m_stream.avail_out = (uInt)size;
        m_stream.next_out = (Bytef*)buf;

        do_inflate();

        sout = (size_t)( size - m_stream.avail_out );
        return m_stream.avail_out == 0;
    }

    z_stream m_stream;
};

struct zip_inflate_chunk_proc
{
    AINLINE zip_inflate_chunk_proc( bool hasHeader, fsOffsetNumber_t streamBaseOffset, fsOffsetNumber_t streamSize ) : dataPipe( hasHeader )
    {
        this->streamBaseOffset = streamBaseOffset;
        this->streamSize = streamSize;
    }
    AINLINE zip_inflate_chunk_proc( zip_inflate_chunk_proc&& ) = delete;

    AINLINE void SetBaseOffset( fsOffsetNumber_t off )
    {
        this->streamBaseOffset = off;
    }

    AINLINE void TransitionSeek( CFile *sourceFile, fsOffsetNumber_t prevSeek, fsOffsetNumber_t newSeek )
    {
        char buf[1024];

        bool doSkip = false;
        fsOffsetNumber_t skipFromOff;

        if ( prevSeek < newSeek )
        {
            // Just skip bytes.
            skipFromOff = prevSeek;
            doSkip = true;
        }
        else if ( prevSeek > newSeek )
        {
            // Reset the stream and skip the bytes.
            dataPipe.reset();

            sourceFile->SeekNative( this->streamBaseOffset, SEEK_SET );

            skipFromOff = 0;
            doSkip = true;
        }

        if ( doSkip )
        {
            chunked_iterator <fsOffsetNumber_t> chunk_iter( skipFromOff, newSeek, sizeof(buf) );

            while ( !chunk_iter.IsEnd() )
            {
                size_t skipCount = (size_t)chunk_iter.GetCurrentChunkEndCount();

                // Need to read the stuff from backbuffer.
                size_t actualReadCount = this->ReadToBuffer( sourceFile, buf, skipCount );

                if ( actualReadCount != skipCount )
                {
                    break;
                }

                chunk_iter.Increment();
            }

            // Done.
        }
    }

    AINLINE size_t ReadToBuffer( CFile *sourceFile, void *buffer, size_t readCount )
    {
        dataPipe.m_stream.next_out = (Bytef*)buffer;
        dataPipe.m_stream.avail_out = (uInt)readCount;

        while ( dataPipe.m_stream.avail_out != 0 )
        {
            char buf[1024];

            size_t actualReadCount;
            {
                fsOffsetNumber_t curLocalSeek = ( sourceFile->TellNative() - this->streamBaseOffset );

                size_t canReadCount = BoundedBufferOperations <fsOffsetNumber_t>::CalculateReadCount( curLocalSeek, this->streamSize, sizeof(buf) );

                actualReadCount = sourceFile->Read( buf, canReadCount );
            }

            dataPipe.m_stream.next_in = (Bytef*)buf;
            dataPipe.m_stream.avail_in = (uInt)actualReadCount;

            dataPipe.do_inflate();

            if ( actualReadCount != sizeof(buf) )
            {
                break;
            }
        }

        // Must discard the remaining bytes.
        size_t remainingInput = dataPipe.m_stream.avail_in;

        // Check if there is anything we have to seek-back because we read too many.
        if ( remainingInput > 0 )
        {
            fsOffsetNumber_t negativeSeekBack = -(fsOffsetNumber_t)remainingInput;

            sourceFile->SeekNative( negativeSeekBack, SEEK_CUR );
        }

        return (size_t)( dataPipe.m_stream.next_out - (Bytef*)buffer );
    }

    zip_inflate_decompression dataPipe;
    fsOffsetNumber_t streamBaseOffset;
    fsOffsetNumber_t streamSize;
};

struct zip_compression_base
{
    zip_compression_base( compression_progress& header ) : m_header( header )
    {
        m_header.sizeCompressed = 0;
        m_header.crc32val = 0xFFFFFFFF;
    }

    ~zip_compression_base( void )
    {
        m_header.crc32val ^= 0xFFFFFFFF;
    }

    inline void checksum( const char *buf, size_t size )
    {
        m_header.crc32val = crc32_z_ex( m_header.crc32val, (Bytef*)buf, (z_size_t)size, 0 );
    }

    compression_progress& m_header;
};

struct zip_raw_compression : public zip_compression_base
{
    using zip_compression_base::zip_compression_base;

    inline void prepare( const char *buf, size_t size, bool eof )
    {
        m_rcv = size;
        m_buf = buf;

        checksum( buf, size );
    }

    inline bool parse( char *buf, size_t size, size_t& sout )
    {
        size_t toWrite = std::min( size, m_rcv );
        memcpy( buf, m_buf, toWrite );

        m_header.sizeCompressed += (uInt)toWrite;

        m_buf += toWrite;
        m_rcv -= toWrite;

        sout = size;
        return m_rcv != 0;
    }

    size_t m_rcv;
    const char* m_buf;
};

struct zip_deflate_compression : public zip_compression_base
{
    zip_deflate_compression( compression_progress& header, int level, bool putHeader ) : zip_compression_base( header )
    {
        m_stream.zalloc = zlib_malloc;
        m_stream.zfree = zlib_free;
        m_stream.opaque = nullptr;

        if ( deflateInit2( &m_stream, level, Z_DEFLATED, putHeader ? MAX_WBITS : -MAX_WBITS, 8, Z_DEFAULT_STRATEGY ) != Z_OK )
        {
            throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
        }
    }

    ~zip_deflate_compression( void )
    {
        deflateEnd( &m_stream );
    }

    inline void prepare( const char *buf, size_t size, bool eof )
    {
        m_flush = eof ? Z_FINISH : Z_NO_FLUSH;

        m_stream.avail_in = (uInt)size;
        m_stream.next_in = (Bytef*)buf;

        checksum( buf, size );
    }

    inline bool parse( char *buf, size_t size, size_t& sout )
    {
        m_stream.avail_out = (uInt)size;
        m_stream.next_out = (Bytef*)buf;

        int ret = deflate( &m_stream, m_flush );

        sout = size - m_stream.avail_out;

        m_header.sizeCompressed += (uInt)sout;

        if ( ret == Z_STREAM_ERROR )
        {
            throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
        }

        return m_stream.avail_out == 0;
    }

    int m_flush;
    z_stream m_stream;
};

#endif //FILESYS_ENABLE_ZIP

// Calculating the size of a node.
fsOffsetNumber_t CZIPArchiveTranslator::fileMetaData::GetSize( void ) const
{
    fsOffsetNumber_t resourceSize = 0;

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::CReadWriteWriteContextSafe <> ctxAtomic( this->lockAtomic );
#endif //FILESYS_MULTI_THREADING

    eFileDataState dataState = this->dataState;

    if ( dataState == eFileDataState::ARCHIVED || dataState == eFileDataState::PRESENT_COMPRESSED )
    {
        // If we are not compressed, then we actually store our size in the compressed-size field!
        // Because that field must not be invalid.
		unsigned short comprType = this->compression;

        if ( comprType == 0 )
        {
            resourceSize = this->sizeCompressed;
        }
        else
        {
            if ( this->sizeRealIsVerified == false )
            {
                // Calculate the real size.
                fsOffsetNumber_t realSize = 0;

                fsOffsetNumber_t dataOffset = this->dataOffset;

                CFile *useFile = nullptr;

#ifdef FILESYS_MULTI_THREADING
                NativeExecutive::CReadWriteWriteContextSafe <> ctxDecompressFile( nullptr );
#endif //FILESYS_MULTI_THREADING

                if ( dataState == eFileDataState::ARCHIVED )
                {
                    useFile = &this->translator->m_file;

#ifdef FILESYS_MULTI_THREADING
                    ctxDecompressFile = this->translator->m_fileLock.get();
#endif //FILESYS_MULTI_THREADING
                }
                else if ( dataState == eFileDataState::PRESENT_COMPRESSED )
                {
                    useFile = this->dataStream;
                }

                // Must not forget to set the seek.
                useFile->SeekNative( dataOffset, SEEK_SET );

				if ( comprType == 8 )
				{
#ifdef FILESYS_ENABLE_ZIP
					char readBuf[ 4096 ];

					zip_inflate_chunk_proc proc( false, dataOffset, this->sizeCompressed );

					while ( true )
					{
						size_t curReadCount = proc.ReadToBuffer( useFile, readBuf, sizeof(readBuf) );

						if ( curReadCount == 0 )
						{
							break;
						}

						realSize += (fsOffsetNumber_t)curReadCount;
					}
#else
					throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::RESOURCE_UNAVAILABLE );
#endif //FILESYS_ENABLE_ZIP
				}
				else
				{
					throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::RESOURCE_UNAVAILABLE );
				}

                this->sizeReal = (size_t)realSize;
                this->sizeRealIsVerified = true;
            }

            resourceSize = this->sizeReal;
        }
    }
    else if ( dataState == eFileDataState::PRESENT )
    {
        if ( CFile *dataStream = this->dataStream )
        {
            resourceSize = dataStream->GetSizeNative();
        }
    }

    return resourceSize;
}

// Implement the stream now.
struct zipStream final : public CFile
{
    friend class CZIPArchiveTranslator;

    inline zipStream( CZIPArchiveTranslator& zip, CZIPArchiveTranslator::file& info, filePath path, bool hasStreamRef, bool readable, bool writeable )
        : m_archive( zip ), m_info( info ), m_path( std::move( path ) ), m_hasStreamRef( hasStreamRef )
    {
        this->m_ourSeek = 0;

        info.metaData.locks.AddToBack( this );

        this->m_readable = readable;
        this->m_writeable = writeable;

        // Initialize compression.
        {
            unsigned short comprType = info.metaData.compression;

            if ( comprType == 0 )
            {
                // Nothing to do.
            }
#ifdef FILESYS_ENABLE_ZIP
            else if ( comprType == 8 )
            {
                new (&compressor.zlib_dec) zlib_dec_t( eir::constr_with_alloc::DEFAULT, false, info.metaData.dataOffset, info.metaData.sizeCompressed );
            }
#endif //FILESYS_ENABLE_ZIP
            else
            {
                throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
            }
        }

        // Keep this up-to-date.
        if ( info.metaData.dataState == CZIPArchiveTranslator::eFileDataState::ARCHIVED )
        {
            this->m_compressedSeek = info.metaData.dataOffset;
        }
        else
        {
            this->m_compressedSeek = 0;
        }
    }
    inline zipStream( const zipStream& ) = delete;
    inline zipStream( zipStream&& ) = delete;

    inline ~zipStream( void )
    {
        // Shutdown compression.
        {
            unsigned short comprType = m_info.metaData.compression;

            if ( comprType == 0 )
            {
                // Nothing to do.
            }
#ifdef FILESYS_ENABLE_ZIP
            else if ( comprType == 8 )
            {
                this->compressor.zlib_dec.~chunked_buffer_processor();
            }
#endif //FILESYS_ENABLE_ZIP
            // else we cannot do anything.
        }

        CZIPArchiveTranslator::file *fileInfo = &this->m_info;

#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteWriteContextSafe <> ctxRemoveRef( fileInfo->metaData.lockAtomic );
#endif //FILESYS_MULTI_THREADING

        m_info.metaData.locks.RemoveByValue( this );

        if ( this->m_hasStreamRef )
        {
            m_info.metaData.ReleaseDataStream();
        }
    }

    inline zipStream& operator = ( const zipStream& ) = delete;
    inline zipStream& operator = ( zipStream&& ) = delete;

    filePath GetPath( void ) const override
    {
        return m_path;
    }

    // Actual API.
    size_t                  Read( void *buffer, size_t readCount ) override;
    size_t                  Write( const void *buffer, size_t writeCount ) override;
    int                     Seek( long iOffset, int iType ) override;
    int                     SeekNative( fsOffsetNumber_t iOffset, int iType ) override;
    long                    Tell( void ) const override;
    fsOffsetNumber_t        TellNative( void ) const override;
    bool                    IsEOF( void ) const override;
    bool                    QueryStats( filesysStats& statsOut ) const override;
    void                    SetFileTimes( time_t atime, time_t ctime, time_t mtime ) override;
    void                    SetSeekEnd( void ) override;
    size_t                  GetSize( void ) const override;
    fsOffsetNumber_t        GetSizeNative( void ) const override;
    void                    Flush( void ) override;
    CFileMappingProvider*   CreateMapping( void ) override;
    bool                    IsReadable( void ) const noexcept override;
    bool                    IsWriteable( void ) const noexcept override;

private:
    // Helpers.
    inline void AcquireStreamRef( void )
    {
        if ( this->m_hasStreamRef == false )
        {
            bool couldGetRef = m_info.metaData.AcquireDataStream();

            if ( !couldGetRef )
            {
                throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
            }

            this->m_hasStreamRef = true;
        }
    }

    CZIPArchiveTranslator&          m_archive;
    CZIPArchiveTranslator::file&    m_info;
    filePath                        m_path;

    fsOffsetNumber_t                m_compressedSeek;
    fsOffsetNumber_t                m_ourSeek;

    bool                            m_writeable;
    bool                            m_readable;

    bool                            m_hasStreamRef;

#ifdef FILESYS_ENABLE_ZIP
    typedef chunked_buffer_processor <zip_inflate_chunk_proc, 2048> zlib_dec_t;
#endif //FILESYS_ENABLE_ZIP

    union compressorData
    {
        AINLINE compressorData( void )  {}
        AINLINE ~compressorData( void ) {}

#ifdef FILESYS_ENABLE_ZIP
        zlib_dec_t zlib_dec;
#endif //FILESYS_ENABLE_ZIP
        char pad;
    } compressor;
};

#ifdef FILESYS_MULTI_THREADING

NativeExecutive::CReadWriteLock* CZIPArchiveTranslator::get_file_lock( CFile *useFile )
{
    if ( useFile == &this->m_file )
    {
        return this->m_fileLock;
    }

    return nullptr;
}

#endif //FILESYS_MULTI_THREADING

void CZIPArchiveTranslator::fileMetaData::ChangeToState( eFileDataState changeToState )
{
    eFileDataState curDataState = this->dataState;

    if ( curDataState == changeToState )
    {
        return;
    }

    unsigned short comprType = this->compression;

    CFile *useFile = nullptr;

    CZIPArchiveTranslator *archive = this->translator;

    fsOffsetNumber_t curSize;

    if ( curDataState == eFileDataState::ARCHIVED )
    {
        useFile = &archive->m_file;

        useFile->SeekNative( this->dataOffset, SEEK_SET );

        curSize = (fsOffsetNumber_t)this->sizeCompressed;
    }
    else if ( curDataState == eFileDataState::PRESENT || curDataState == eFileDataState::PRESENT_COMPRESSED )
    {
        useFile = this->dataStream;

        useFile->SeekNative( 0, SEEK_SET );

        curSize = useFile->GetSizeNative();
    }
    else
    {
        throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
    }

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::CReadWriteWriteContextSafe <> ctxDecompressFile( archive->get_file_lock( useFile ) );
#endif //FILESYS_MULTI_THREADING

    compression_progress _compression_info;

    CFile *targetStream = archive->fileMan.AllocateTemporaryDataDestination();

    bool keepState = false;

    try
    {
        bool doCompress = false;
        bool doDecompress = false;

        (void)doCompress;
        (void)doDecompress;

        if ( ( ( curDataState == eFileDataState::ARCHIVED || curDataState == eFileDataState::PRESENT_COMPRESSED ) && changeToState == eFileDataState::PRESENT_COMPRESSED ) ||
             comprType == 0 )
        {
            keepState = true;
        }
        else if ( curDataState == eFileDataState::PRESENT && changeToState == eFileDataState::PRESENT_COMPRESSED )
        {
            doCompress = true;
        }
        else if ( ( curDataState == eFileDataState::ARCHIVED || curDataState == eFileDataState::PRESENT_COMPRESSED ) && changeToState == eFileDataState::PRESENT )
        {
            doDecompress = true;
        }

        bool couldChangeState = false;

        if ( keepState )
        {
            FileSystem::StreamCopyCount( *useFile, *targetStream, curSize );

            couldChangeState = true;
        }
#ifdef FILESYS_ENABLE_ZIP
        else if ( comprType == 8 )
        {
            if ( doCompress )
            {
                zip_deflate_compression comp( _compression_info, 9, false );

                FileSystem::StreamParserCount( *useFile, *targetStream, curSize, comp );

                couldChangeState = true;
            }
            else if ( doDecompress )
            {
                // TODO: decompress the stream and mark this as decompressed stream; we recommit data compressed on file close.
                zip_inflate_decompression decomp( false );

                FileSystem::StreamParserCount( *useFile, *targetStream, curSize, decomp );

                couldChangeState = true;
            }
        }
#endif //FILESYS_ENABLE_ZIP

        if ( !couldChangeState )
        {
            throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
        }
    }
    catch( ... )
    {
        delete targetStream;

        throw;
    }

    // Replace our file pointer with the new one.
    this->SetDataStream( targetStream );
    this->dataState = changeToState;

    if ( keepState == false )
    {
        // Update some meta-data.
        if ( changeToState == eFileDataState::PRESENT )
        {
            this->sizeReal = (size_t)useFile->GetSizeNative();
            this->sizeRealIsVerified = true;
        }
        else if ( changeToState == eFileDataState::PRESENT_COMPRESSED || changeToState == eFileDataState::ARCHIVED )
        {
            this->crc32val = _compression_info.crc32val;
            this->sizeCompressed = _compression_info.sizeCompressed;
        }
    }
}

size_t zipStream::Read( void *buffer, size_t readCount )
{
    if ( !m_readable )
        return 0;

    // TODO: add locks to files so that they can be read (seek + op) without problems
    //  * why locks if files are allowed to be used by one thread only anyway?
    // -> because you can use multiple files at the same time on the same archive handle! thus they all would
    //    concurrently use the archive file handle + seek!
    size_t realReadCount;
    {
        fsOffsetNumber_t ourSeek = this->m_ourSeek;
        {
            CZIPArchiveTranslator::file *fileInfo = &this->m_info;

#ifdef FILESYS_MULTI_THREADING
            NativeExecutive::CReadWriteWriteContextSafe <> ctxFileInfoAccess( fileInfo->metaData.lockAtomic );
#endif //FILESYS_MULTI_THREADING

            CFile *useFile = nullptr;

            CZIPArchiveTranslator *archive = &this->m_archive;

            CZIPArchiveTranslator::eFileDataState dataState = fileInfo->metaData.dataState;

            if ( dataState == CZIPArchiveTranslator::eFileDataState::ARCHIVED )
            {
                useFile = &archive->m_file;
            }
            else if ( dataState == CZIPArchiveTranslator::eFileDataState::PRESENT ||
                      dataState == CZIPArchiveTranslator::eFileDataState::PRESENT_COMPRESSED )
            {
                this->AcquireStreamRef();

                useFile = fileInfo->metaData.GetDataStream();
            }
            else
            {
                assert( 0 );
            }

#ifdef FILESYS_MULTI_THREADING
            NativeExecutive::CReadWriteWriteContextSafe <> ctxReadTransFile( archive->get_file_lock( useFile ) );
#endif //FILESYS_MULTI_THREADING

            unsigned short comprType = fileInfo->metaData.compression;

            if ( comprType == 0 || dataState == CZIPArchiveTranslator::eFileDataState::PRESENT )
            {
                // The same logic appears to be in the reading and the writing logic.
                // So we should investigate about possibilities of code-sharing.
                if ( dataState == CZIPArchiveTranslator::eFileDataState::ARCHIVED )
                {
                    // TODO: check if the read request is still inside the archived region of the file.
                    // if not then read less bytes than requested (bounded buffer logic).
                    // TRIVIA: for the writer's case we would extract the stream into external space so
                    // the writing could still happen (generalized decompress algorithm).

                    // IMPORTANT: because we are not compressed, the compressed size MUST MATCH the real size!
                    size_t canReadCount = BoundedBufferOperations <fsOffsetNumber_t>::CalculateReadCount( ourSeek, fileInfo->metaData.sizeCompressed, readCount );

                    useFile->SeekNative( fileInfo->metaData.dataOffset + ourSeek, SEEK_SET );

                    realReadCount = useFile->Read( buffer, canReadCount );
                }
                else
                {
                    useFile->SeekNative( ourSeek, SEEK_SET );

                    realReadCount = useFile->Read( buffer, readCount );
                }
            }
            else
            {
                assert( comprType != 0 );

                // Bounds-logic should be handled internally by the chunk processor, unlike the explicit stuff above for raw files.

                // We have to manage a compressedSeek that runs in-synchonicity to the real seek because
                // data in compressed-space is smaller/bigger than the actual decompressed data, thus
                // segments transform into segments of potentially different sizes.
                // The_GTA: Do not try to think of it too much. Just be happy that I solved this clusterfuck for you.
                fsOffsetNumber_t compressedSeek = this->m_compressedSeek;

                (void)compressedSeek;

                bool couldRead = false;

#ifdef FILESYS_ENABLE_ZIP
                if ( comprType == 8 )
                {
                    // The file seek is being initialized by the processor automatically.
                    realReadCount = compressor.zlib_dec.ReadBytes( useFile, compressedSeek, ourSeek, buffer, readCount );

                    couldRead = true;
                }
#endif //FILESYS_ENABLE_ZIP

                if ( !couldRead )
                {
                    throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
                }

                this->m_compressedSeek = useFile->TellNative();
            }
        }
        assert( realReadCount <= (std::make_unsigned <fsOffsetNumber_t>::type)std::numeric_limits <fsOffsetNumber_t>::max() );

        this->m_ourSeek = ourSeek + (fsOffsetNumber_t)realReadCount;
    }
    return realReadCount;
}

void CZIPArchiveTranslator::fileMetaData::Decompress( void )
{
    unsigned short comprType = this->compression;

    if ( comprType == 0 )
    {
        return;
    }

    this->ChangeToState( eFileDataState::PRESENT );
}

size_t zipStream::Write( const void *buffer, size_t writeCount )
{
    if ( !m_writeable )
        return 0;

    // TODO: add locks to prevent problems of multi-threading when setting the seek to the shared file handle.
    //  * why locks if files are allowed to be used by one thread only anyway?
    // -> because you can use multiple files at the same time on the same archive handle! thus they all would
    //    concurrently use the archive file handle + seek!
    size_t realWriteCount;
    {
        fsOffsetNumber_t ourSeek = this->m_ourSeek;
        {
            CZIPArchiveTranslator::file *fileInfo = &this->m_info;

#ifdef FILESYS_MULTI_THREADING
            NativeExecutive::CReadWriteWriteContextSafe <> ctxWriteInfo( fileInfo->metaData.lockAtomic );
#endif //FILESYS_MULTI_THREADING

            CZIPArchiveTranslator *archive = &this->m_archive;

            // TODO: think about revising the data model where we write stuff directly into the archive for uncompressed files.
            // Not really important because uncompressed files are rarely used in .ZIP containers anyway.

            // Prelude, if anything compressed goes to here.
            fileInfo->metaData.ChangeToState( CZIPArchiveTranslator::eFileDataState::PRESENT );

            // Determine the source stream.
            CFile *useFile = nullptr;

            CZIPArchiveTranslator::eFileDataState curDataState = fileInfo->metaData.dataState;

            assert( curDataState == CZIPArchiveTranslator::eFileDataState::PRESENT );

            // Must get a stream reference.
            this->AcquireStreamRef();

            useFile = fileInfo->metaData.GetDataStream();

#ifdef FILESYS_MULTI_THREADING
            NativeExecutive::CReadWriteWriteContextSafe <> ctxWriteTransFile( archive->get_file_lock( useFile ) );
#endif //FILESYS_MULTI_THREADING

            // Now the actual logic.
            useFile->SeekNative( ourSeek, SEEK_SET );

            realWriteCount = useFile->Write( buffer, writeCount );

            // Update the stored file size as we go along.
            {
                fsOffsetNumber_t newRealSize = useFile->GetSizeNative();

                fileInfo->metaData.sizeReal = (size_t)newRealSize;
                fileInfo->metaData.sizeRealIsVerified = true;
            }

            // Update the time.
            fileInfo->metaData.UpdateTime();

            // We now have local mods.
            fileInfo->metaData.OnModification();
        }
        assert( realWriteCount <= (std::make_unsigned <fsOffsetNumber_t>::type)std::numeric_limits <fsOffsetNumber_t>::max() );

        this->m_ourSeek = ourSeek + (fsOffsetNumber_t)realWriteCount;
    }
    return realWriteCount;
}

int zipStream::Seek( long iOffset, int iType )
{
    long base = 0;

    if ( iType == SEEK_SET )
    {
        base = 0;
    }
    else if ( iType == SEEK_CUR )
    {
        base = (long)this->m_ourSeek;
    }
    else if ( iType == SEEK_END )
    {
        base = (long)this->GetSizeNative();
    }
    else
    {
        return -1;
    }

    fsOffsetNumber_t newSeek = (fsOffsetNumber_t)( base + iOffset );

    this->m_ourSeek = newSeek;

    return 0;
}

int zipStream::SeekNative( fsOffsetNumber_t iOffset, int iType )
{
    fsOffsetNumber_t base = 0;

    if ( iType == SEEK_SET )
    {
        base = 0;
    }
    else if ( iType == SEEK_CUR )
    {
        base = this->m_ourSeek;
    }
    else if ( iType == SEEK_END )
    {
        base = this->GetSizeNative();
    }
    else
    {
        return -1;
    }

    fsOffsetNumber_t newSeek = ( base + iOffset );

    this->m_ourSeek = newSeek;

    return 0;
}

long zipStream::Tell( void ) const
{
    return (long)this->m_ourSeek;
}

fsOffsetNumber_t zipStream::TellNative( void ) const
{
    return this->m_ourSeek;
}

bool zipStream::IsEOF( void ) const
{
    return ( this->m_ourSeek >= this->GetSizeNative() );
}

bool zipStream::QueryStats( filesysStats& statsOut ) const
{
    // TODO: debug this, because there are some bugs regarding correct date display.

    tm date;

    m_info.metaData.GetModTime( date );

    date.tm_year -= 1900;

    time_t timeval = mktime( &date );

    statsOut.atime = timeval;
    statsOut.mtime = timeval;
    statsOut.ctime = timeval;
    statsOut.attribs.type = eFilesysItemType::FILE;
    return true;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif //_MSC_VER

void zipStream::SetFileTimes( time_t atime, time_t ctime, time_t mtime )
{
    tm *date = gmtime( &mtime );

    m_info.metaData.SetModTime( *date );
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER

void zipStream::SetSeekEnd( void )
{
    // Cannot resize a file if we cannot write to it.
    if ( this->m_writeable == false )
    {
        return;
    }

    CZIPArchiveTranslator::file *fileInfo = &this->m_info;

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::CReadWriteWriteContextSafe <> ctxTruncateInfo( fileInfo->metaData.lockAtomic );
#endif //FILESYS_MULTI_THREADING

    // If we are compressed then we have to decompress, because the file is about to change.
    // But it is also a problem if we are part of the archive, thus just extract.
    fileInfo->metaData.ChangeToState( CZIPArchiveTranslator::eFileDataState::PRESENT );

    // TODO: add locking here, too.

    this->AcquireStreamRef();

    CFile *useFile = fileInfo->metaData.GetDataStream();

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::CReadWriteWriteContextSafe <> ctxTrimArchiveFile( this->m_archive.get_file_lock( useFile ) );
#endif //FILESYS_MULTI_THREADING

    // Trim the file.
    useFile->SeekNative( this->m_ourSeek, SEEK_SET );
    useFile->SetSeekEnd();

    // We now have local mods.
    fileInfo->metaData.OnModification();

    fileInfo->metaData.UpdateTime();
}

size_t zipStream::GetSize( void ) const
{
    // Just redirect it.
    return (size_t)this->GetSizeNative();
}

fsOffsetNumber_t zipStream::GetSizeNative( void ) const
{
    CZIPArchiveTranslator::file *fileInfo = &this->m_info;

    // Just return the stuff that we calculate from the node.
    return fileInfo->metaData.GetSize();
}

void zipStream::Flush( void )
{
    // What do we actually do here anyway?
}

CFileMappingProvider* zipStream::CreateMapping( void )
{
    // Not supported.
    return nullptr;
}

bool zipStream::IsReadable( void ) const noexcept
{
    return m_readable;
}

bool zipStream::IsWriteable( void ) const noexcept
{
    return m_writeable;
}

/*=======================================
    CZIPArchiveTranslator

    ZIP translation utility
=======================================*/

CZIPArchiveTranslator::CZIPArchiveTranslator( CFileSystemNative *fileSys, zipExtension& theExtension, CFile& fileStream )
    : CSystemPathTranslator( basicRootPathType <pathcheck_always_allow> (), false, true ), m_zipExtension( theExtension ), m_file( fileStream )
#ifdef FILESYS_MULTI_THREADING
    , m_fileLock( nativeFileSystem->nativeMan )
#endif //FILESYS_MULTI_THREADING
    , fileMan( fileSys ), m_virtualFS( true )
{
    // TODO: Get real .zip structure offset
    m_structOffset = 0;

    // Set up the file presence management.
    fileMan.SetMaximumDataQuotaRAM( 100000000 );    // about 100mb.

    // Set up the virtual filesystem.
    m_virtualFS.hostTranslator = this;
}

CZIPArchiveTranslator::~CZIPArchiveTranslator( void )
{
    return;
}

bool CZIPArchiveTranslator::CreateDir( const char *path, filesysPathOperatingMode opMode, bool createParentDirs )
{
    return m_virtualFS.CreateDir( path, opMode, createParentDirs );
}

CFile* CZIPArchiveTranslator::OpenNativeFileStream( file *fsObject, eVFSFileOpenMode openMode, const filesysAccessFlags& access )
{
    bool isArchiveFile = false;

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::CReadWriteWriteContextSafe <> ctxOpenFile( fsObject->metaData.lockAtomic );
#endif //FILESYS_MULTI_THREADING

    if ( openMode == eVFSFileOpenMode::OPEN )
    {
        eFileDataState dataState = fsObject->metaData.dataState;

        // Attempt to get a handle to the realtime root.
        if ( dataState == eFileDataState::ARCHIVED )
        {
            // We want to stay compressed for as long as possible.
            isArchiveFile = true;
        }
        else if ( dataState == eFileDataState::PRESENT || dataState == eFileDataState::PRESENT_COMPRESSED )
        {
            // We read from the file pointer we have.
            if ( fsObject->metaData.AcquireDataStream() == false )
            {
                return nullptr;
            }
        }
        else
        {
            return nullptr;
        }
    }
    else if ( openMode == eVFSFileOpenMode::CREATE )
    {
        CFile *dstFile = this->fileMan.AllocateTemporaryDataDestination();

        if ( dstFile )
        {
            fsObject->metaData.SetDataStream( dstFile );
            fsObject->metaData.dataState = eFileDataState::PRESENT;
            fsObject->metaData.hasDataLocalMods = true;

            // Actually needing the file aswell.
            fsObject->metaData.AcquireDataStream();
        }
        else
        {
            return nullptr;
        }
    }
    else
    {
        // TODO: implement any unknown cases.
        return nullptr;
    }

    zipStream *redirStream = nullptr;

    try
    {
        redirStream = new zipStream( *this, *fsObject, fsObject->relPath, ( isArchiveFile == false ), access.allowRead, access.allowWrite );
    }
    catch( ... )
    {
        if ( !isArchiveFile )
        {
            fsObject->metaData.ReleaseDataStream();
        }

        throw;
    }

    return redirStream;
}

CFile* CZIPArchiveTranslator::Open( const char *path, const filesysOpenMode& mode, eFileOpenFlags flags )
{
    return m_virtualFS.OpenStream( path, mode );
}

bool CZIPArchiveTranslator::Exists( const char *path ) const
{
    return m_virtualFS.Exists( path );
}

void CZIPArchiveTranslator::fileMetaData::OnFileDelete( void )
{
    // Delete the resources that are associated with this file entry.
    if ( CFile *dataStream = this->dataStream )
    {
        delete dataStream;

        this->dataStream = nullptr;
    }

    this->dataState = eFileDataState::ARCHIVED;
}

bool CZIPArchiveTranslator::Delete( const char *path, filesysPathOperatingMode opMode )
{
    return m_virtualFS.Delete( path, opMode );
}

bool CZIPArchiveTranslator::fileMetaData::OnFileCopy( const dirNames& tree, const filePath& newName ) const
{
    // We have no more meta-data that need preparing on external storage devices.
    return true;
}

void CZIPArchiveTranslator::OnRemoveRemoteFile( const dirNames& tree, const filePath& newName )
{
    // Since we do not use remote file storage anymore, this is a no-op.
    return;
}

bool CZIPArchiveTranslator::fileMetaData::CopyAttributesTo( fileMetaData& dstEntry )
{
    CZIPArchiveTranslator *trans = this->translator;

    // Copy over general attributes
    dstEntry.flags          = flags;
    dstEntry.compression    = compression;
    dstEntry.modTime        = modTime;
    dstEntry.modDate        = modDate;
    dstEntry.diskID         = diskID;
    dstEntry.internalAttr   = internalAttr;
    dstEntry.externalAttr   = externalAttr;
    dstEntry.extra          = extra;
    dstEntry.comment        = comment;
    dstEntry.dataState      = dataState;

    if ( dataState == eFileDataState::ARCHIVED )
    {
        dstEntry.version            = version;
        dstEntry.reqVersion         = reqVersion;
        dstEntry.crc32val           = crc32val;
        dstEntry.sizeCompressed     = sizeCompressed;
        dstEntry.sizeReal           = sizeReal;
        dstEntry.sizeRealIsVerified = sizeRealIsVerified;
        dstEntry.dataOffset         = dataOffset;
    }

    // Also copy the file data.
    if ( dataState != eFileDataState::ARCHIVED )
    {
        CFile *dstDataStream = nullptr;

        if ( CFile *srcDataStream = this->dataStream )
        {
            dstDataStream = trans->fileMan.AllocateTemporaryDataDestination();

            if ( !dstDataStream )
            {
                return false;
            }

            try
            {
                srcDataStream->Seek( 0, SEEK_SET );

                FileSystem::StreamCopy( *srcDataStream, *dstDataStream );
            }
            catch( ... )
            {
                delete dstDataStream;

                throw;
            }
        }

        dstEntry.SetDataStream( dstDataStream );
    }

    return true;
}

bool CZIPArchiveTranslator::Copy( const char *src, const char *dst )
{
    return m_virtualFS.Copy( src, dst );
}

bool CZIPArchiveTranslator::fileMetaData::OnFileRename( const dirNames& tree, const filePath& newName )
{
    // We have no more meta-data that needs movement based on the object's filename.
    return true;
}

bool CZIPArchiveTranslator::Rename( const char *src, const char *dst )
{
    return m_virtualFS.Rename( src, dst );
}

size_t CZIPArchiveTranslator::Size( const char *path ) const
{
    return (size_t)m_virtualFS.Size( path );
}

bool CZIPArchiveTranslator::QueryStats( const char *path, filesysStats& statsOut ) const
{
    return m_virtualFS.QueryStats( path, statsOut );
}

bool CZIPArchiveTranslator::OnConfirmDirectoryChange( const translatorPathResult& nodePath )
{
    assert( nodePath.type == eRequestedPathResolution::RELATIVE_PATH );
    assert( nodePath.relpath.backCount == 0 );
    // NOTE: we can be a file-path because all it means that the ending slash is not present.
    //  this is valid in ambivalent path-process-mode.

    return m_virtualFS.ChangeDirectory( nodePath.relpath.travelNodes );
}

static void _scanFindCallback( const filePath& path, dirNames *output )
{
    output->AddToBack( path );
}

void CZIPArchiveTranslator::ScanDirectory( const char *path, const char *wildcard, bool recurse, pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata ) const
{
    m_virtualFS.ScanDirectory( path, wildcard, recurse, dirCallback, fileCallback, userdata );
}

void CZIPArchiveTranslator::GetDirectories( const char *path, const char *wildcard, bool recurse, dirNames& output ) const
{
    ScanDirectory( path, wildcard, recurse, (pathCallback_t)_scanFindCallback, nullptr, &output );
}

void CZIPArchiveTranslator::GetFiles( const char *path, const char *wildcard, bool recurse, dirNames& output ) const
{
    ScanDirectory( path, wildcard, recurse, nullptr, (pathCallback_t)_scanFindCallback, &output );
}

CDirectoryIterator* CZIPArchiveTranslator::BeginDirectoryListing( const char *path, const char *wildcard, const scanFilteringFlags& filter_flags ) const
{
    return m_virtualFS.BeginDirectoryListing( path, wildcard, filter_flags );
}

void CZIPArchiveTranslator::ReadFiles( unsigned int count )
{
    char buf[65536];

    size_t file_idx = 0;

    // First we fetch the central headers.
    while ( count-- )
    {
        _centralFile header;

        if ( !m_file.ReadStruct( header ) || header.signature != ZIP_FILE_SIGNATURE )
        {
            throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
        }

        // Acquire the path
        m_file.Read( buf, header.nameLen );
        buf[ header.nameLen ] = 0;

        normalNodePath normalPath;

        if ( !GetRelativePathNodesFromRoot( buf, normalPath ) )
        {
            throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
        }

        union
        {
            genericMetaData *fsObject;
            fileMetaData *fileEntry;
            directoryMetaData *dirEntry;
        };
        fsObject = nullptr;

        bool isFilePath = normalPath.isFilePath;

        if ( isFilePath )
        {
            filePath name = std::move( normalPath.travelNodes.GetBack() );
            normalPath.travelNodes.RemoveFromBack();

            // Deposit in the correct directory
            fileEntry = &m_virtualFS._CreateDirTree( m_virtualFS.GetRootDir(), normalPath.travelNodes, file_idx, true ).AddFile( std::move( name ), file_idx, true ).metaData;
        }
        else
        {
            // Try to create the directory.
            dirEntry = &m_virtualFS._CreateDirTree( m_virtualFS.GetRootDir(), normalPath.travelNodes, file_idx, true ).metaData;
        }

        // Initialize common data.
        if ( fsObject )
        {
            fsObject->version = header.version;
            fsObject->reqVersion = header.reqVersion;
            fsObject->flags = header.flags;
            fsObject->modTime = header.modTime;
            fsObject->modDate = header.modDate;
            fsObject->diskID = header.diskID;
            fsObject->localHeaderOffset = header.localHeaderOffset;
            fsObject->hasLocalHeaderOffset = true;

            if ( header.extraLen != 0 )
            {
                m_file.Read( buf, header.extraLen );
                fsObject->extra = zipString( buf, header.extraLen );
            }

            if ( header.commentLen != 0 )
            {
                m_file.Read( buf, header.commentLen );
                fsObject->comment = zipString( buf, header.commentLen );
            }
        }
        else
        {
            m_file.SeekNative( header.extraLen + header.commentLen, SEEK_CUR );
        }

        if ( fileEntry && isFilePath )
        {
            // Store file-orriented attributes
            fileEntry->compression = header.compression;
            fileEntry->crc32val = header.crc32val;
            fileEntry->sizeCompressed = header.sizeCompressed;
            fileEntry->sizeReal = header.sizeReal;
            fileEntry->internalAttr = header.internalAttr;
            fileEntry->externalAttr = header.externalAttr;
            fileEntry->dataState = eFileDataState::ARCHIVED;
        }

        file_idx++;
    }

    // Now scan all local headers of files/directories we registered and check if they are correct.
    this->m_virtualFS.ForAllItems(
        [&]( fsActiveEntry *entry )
    {
        union
        {
            genericMetaData *genericInfo;
            fileMetaData *fileInfo;
            directoryMetaData *dirInfo;
        };

        if ( entry->isFile )
        {
            file *theFile = (file*)entry;

            fileInfo = &theFile->metaData;
        }
        else if ( entry->isDirectory )
        {
            directory *theDir = (directory*)entry;

            dirInfo = &theDir->metaData;
        }
        else
        {
            return;
        }

        if ( genericInfo->hasLocalHeaderOffset == false )
        {
            return;
        }

        size_t localHeaderOffset = genericInfo->localHeaderOffset;

        m_file.SeekNative( localHeaderOffset, SEEK_SET );

        _localHeader header;

        if ( !m_file.ReadStruct( header ) )
        {
            throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
        }

        if ( header.signature != ZIP_LOCAL_FILE_SIGNATURE )
        {
            throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
        }

        // TODO: maybe remember the local attributes that could be different, like comment, and write them back if required.
        // TODO: maybe check that directory have no data attached to them (wtf, why would you even, even if you can?)

        // Ignore the comment and the name.
        fsOffsetNumber_t fileDataSeekPtr = ( m_file.TellNative() + header.nameLen + header.commentLen );

        if ( entry->isFile )
        {
            // Remember it because it is immutable until rebuild.
            fileInfo->dataOffset = fileDataSeekPtr;
        }
    });
}

void CFileSystem::DecompressZLIBStream( CFile *input, CFile *output, size_t inputSize, bool hasHeader ) const
{
#ifdef FILESYS_ENABLE_ZIP
    zip_inflate_decompression decompressor( hasHeader );

    FileSystem::StreamParserCount( *input, *output, inputSize, decompressor );
#else
    throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
#endif //FILESYS_ENABLE_ZIP
}

void CZIPArchiveTranslator::CacheData( void )
{
    this->m_virtualFS.ForAllItems(
        [&]( fsActiveEntry *entry )
    {
        if ( entry->isFile )
        {
            file *fileEntry = (file*)entry;

            // Dump the compressed content
            // This is important because we will rebuild the archive.
            if ( fileEntry->metaData.dataState == eFileDataState::ARCHIVED )
            {
                fileEntry->metaData.ChangeToState( eFileDataState::PRESENT_COMPRESSED );
            }
        }
    });
}

void CFileSystem::CompressZLIBStream( CFile *input, CFile *output, bool putHeader ) const
{
#ifdef FILESYS_ENABLE_ZIP
    compression_progress progress;
    zip_deflate_compression compressor( progress, Z_DEFAULT_COMPRESSION, putHeader );

    FileSystem::StreamParser( *input, *output, compressor );
#else
    throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
#endif //FILESYS_ENABLE_ZIP
}

static inline void WriteStreamString( CFile *stream, const eir::String <char, FSObjectHeapAllocator>& string )
{
    stream->Write( string.GetConstString(), string.GetLength() );
}

void CZIPArchiveTranslator::SaveData( size_t& size )
{
    this->m_virtualFS.ForAllItems(
        [&]( fsActiveEntry *entry )
    {
        if ( entry->isDirectory )
        {
            directory *dir = (directory*)entry;

            if ( dir->metaData.NeedsWriting() )
            {
                _localHeader header = dir->metaData.ConstructLocalHeader();

                // Allocate space in the archive.
                dir->metaData.AllocateArchiveSpace( this, header, size );

                header.version = dir->metaData.version;
                header.flags = dir->metaData.flags;
                header.compression = 0;
                header.crc32val = 0;
                header.sizeCompressed = 0;
                header.sizeReal = 0;

                m_file.WriteStruct( header );
                WriteStreamString( &m_file, dir->relPath.convert_ansi <FSObjectHeapAllocator> () );
                WriteStreamString( &m_file, dir->metaData.comment );
            }
        }
        else if ( entry->isFile )
        {
            file& info = *(file*)entry;

            _localHeader header = info.metaData.ConstructLocalHeader();

            // Allocate space in the archive.
            info.metaData.AllocateArchiveSpace( this, header, size );

            eFileDataState dataState = info.metaData.dataState;

            if ( dataState == eFileDataState::PRESENT_COMPRESSED )
            {
                header.version          = info.metaData.version;
                header.flags            = info.metaData.flags;
                header.compression      = info.metaData.compression;
                header.crc32val         = info.metaData.crc32val;
                header.sizeCompressed   = (fsUInt_t)info.metaData.sizeCompressed;
                header.sizeReal         = (fsUInt_t)info.metaData.sizeReal;

                size += info.metaData.sizeCompressed;

                m_file.WriteStruct( header );
                WriteStreamString( &m_file, info.relPath.convert_ansi <FSObjectHeapAllocator> () );
                WriteStreamString( &m_file, info.metaData.comment );

                bool couldAcquire = info.metaData.AcquireDataStream();

                if ( couldAcquire )
                {
                    CFile *src = info.metaData.GetDataStream();

                    try
                    {
                        src->Seek( 0, SEEK_SET );

                        FileSystem::StreamCopy( *src, m_file );

                        m_file.SetSeekEnd();
                    }
                    catch( ... )
                    {
                        info.metaData.ReleaseDataStream();

                        throw;
                    }

                    info.metaData.ReleaseDataStream();
                }
            }
            else if ( dataState == eFileDataState::PRESENT )
            {
                header.version      = 10;    // WINNT
                header.flags        = info.metaData.flags;
                header.compression  = info.metaData.compression;
                header.crc32val     = 0;

                m_file.WriteStruct( header );
                WriteStreamString( &m_file, info.relPath.convert_ansi <FSObjectHeapAllocator> () );
                WriteStreamString( &m_file, info.metaData.comment );

                size_t actualFileSize = 0;
                fsUInt_t sizeCompressed = 0;
                fsUInt_t crc32val = 0;

                if ( info.metaData.AcquireDataStream() )
                {
                    CFile *src = info.metaData.GetDataStream();

                    try
                    {
                        src->Seek( 0, SEEK_SET );

                        actualFileSize = src->GetSize();

                        compression_progress c_prog;

#ifdef FILESYS_ENABLE_ZIP
                        if ( header.compression == 0 )
                        {
                            zip_raw_compression compressor( c_prog );

                            FileSystem::StreamParser( *src, m_file, compressor );
                        }
                        else if ( header.compression == 8 )
                        {
                            zip_deflate_compression compressor( c_prog, Z_DEFAULT_COMPRESSION, false );

                            FileSystem::StreamParser( *src, m_file, compressor );
                        }
                        else
#endif //FILESYS_ENABLE_ZIP
                        {
                            throw filesystem_exception( eGenExceptCode::INVALID_PARAM );
                        }

                        // Finalize the compression information.
                        sizeCompressed = c_prog.sizeCompressed;
                        crc32val = c_prog.crc32val;
                    }
                    catch( ... )
                    {
                        info.metaData.ReleaseDataStream();

                        throw;
                    }

                    info.metaData.ReleaseDataStream();
                }

                info.metaData.sizeReal = actualFileSize;
                info.metaData.sizeRealIsVerified = true;
                info.metaData.sizeCompressed = sizeCompressed;
                info.metaData.crc32val = crc32val;

                header.sizeReal = (fsUInt_t)actualFileSize;
                header.sizeCompressed = sizeCompressed;
                header.crc32val = crc32val;

                size += sizeCompressed;

                fsOffsetNumber_t wayOff = sizeCompressed + header.nameLen + header.commentLen;

                m_file.SeekNative( -wayOff - (fsOffsetNumber_t)sizeof( header ), SEEK_CUR );
                m_file.WriteStruct( header );
                m_file.SeekNative( wayOff, SEEK_CUR );
            }
            else
            {
                // Must not happen.
                throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
            }
        }
    });
}

unsigned int CZIPArchiveTranslator::BuildCentralFileHeaders( size_t& size )
{
    unsigned int cnt = 0;

    this->m_virtualFS.ForAllItems(
        [&]( fsActiveEntry *entry )
    {
        if ( entry->isDirectory )
        {
            directory *dir = (directory*)entry;

            if ( dir->metaData.NeedsWriting() )
            {
                _centralFile header = dir->metaData.ConstructCentralHeader();

                // Zero out fields which make no sense
                header.compression = 0;
                header.crc32val = 0;
                header.sizeCompressed = 0;
                header.sizeReal = 0;
                header.internalAttr = 0;
                header.externalAttr = 0x10;     // FILE_ATTRIBUTE_DIRECTORY (win32)

                m_file.WriteStruct( header );
                WriteStreamString( &m_file, dir->relPath.convert_ansi <FSObjectHeapAllocator> () );
                WriteStreamString( &m_file, dir->metaData.extra );
                WriteStreamString( &m_file, dir->metaData.comment );

                size += sizeof( header ) + header.nameLen + header.extraLen + header.commentLen;
                cnt++;
            }
        }
        else if ( entry->isFile )
        {
            file *info = (file*)entry;

            _centralFile header = info->metaData.ConstructCentralHeader();

            header.compression      = info->metaData.compression;
            header.crc32val         = info->metaData.crc32val;
            header.sizeCompressed   = (fsUInt_t)info->metaData.sizeCompressed;
            header.sizeReal         = (fsUInt_t)info->metaData.sizeReal;
            header.internalAttr     = info->metaData.internalAttr;
            header.externalAttr     = info->metaData.externalAttr;

            m_file.WriteStruct( header );
            WriteStreamString( &m_file, info->relPath.convert_ansi <FSObjectHeapAllocator> () );
            WriteStreamString( &m_file, info->metaData.extra );
            WriteStreamString( &m_file, info->metaData.comment );

            size += sizeof( header ) + header.nameLen + header.extraLen + header.commentLen;
            cnt++;
        }
    });

    return cnt;
}

void CZIPArchiveTranslator::Save( void )
{
    if ( !m_file.IsWriteable() )
        return;

    // Cache the .zip content
    CacheData();

    // Rewrite the archive
    m_file.SeekNative( m_structOffset, SEEK_SET );

    size_t fileSize = 0;
    SaveData( fileSize );

    // Create the central directory
    size_t centralSize = 0;
    unsigned int entryCount = BuildCentralFileHeaders( centralSize );

    // Finishing entry
    m_file.WriteInt( ZIP_SIGNATURE );

    _endDir tail;
    tail.diskID = 0;
    tail.diskAlign = 0;
    tail.entries = entryCount;
    tail.totalEntries = entryCount;
    tail.centralDirectorySize = (fsUInt_t)centralSize;
    tail.centralDirectoryOffset = (fsUInt_t)fileSize; // we might need the offset of the .zip here
    tail.commentLen = (fsUShort_t)m_comment.GetLength();

    m_file.WriteStruct( tail );
    WriteStreamString( &m_file, m_comment );

    // Cap the stream
    m_file.SetSeekEnd();
}
