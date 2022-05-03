/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.chunkbuf.h
*  PURPOSE:     Chunked-stream implementation for (de-)compression
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYS_CHUNKED_BUFFER_STREAM_
#define _FILESYS_CHUNKED_BUFFER_STREAM_

#include <sdk/MemoryUtils.stream.h>

#include <algorithm>

// It is important to give good-founded things meaningful names, so there we go...
template <typename numberType>
AINLINE numberType GetChunkStartOffset( numberType pos, numberType chunkSize )
{
    return ( pos % chunkSize );
}

template <typename numberType>
AINLINE numberType GetChunkOffsetToEnd( numberType off, numberType endOff, numberType chunkSize, numberType offFromStart )
{
    return std::min( endOff - off, chunkSize - offFromStart );
}

// Iterator over a number in chunks.
template <typename numberType>
struct chunked_iterator
{
    AINLINE chunked_iterator( numberType off, numberType endOff, numberType chunkSize )
        : curOff( std::move( off ) ), endOff( std::move( endOff ) ), chunkSize( std::move( chunkSize ) )
    {
        numberType offsetFromChunkStart = GetChunkStartOffset( off, chunkSize );
        numberType offsetToChunkEnd = GetChunkOffsetToEnd( off, endOff, chunkSize, offsetFromChunkStart );

        this->offsetFromChunkStart = offsetFromChunkStart;
        this->offsetToChunkEnd = offsetToChunkEnd;
    }

    AINLINE bool IsEnd( void ) const
    {
        return ( curOff >= endOff );
    }

    AINLINE void Increment( void )
    {
        this->curOff += offsetToChunkEnd;

        this->offsetFromChunkStart = 0;
        this->offsetToChunkEnd = GetChunkOffsetToEnd( this->curOff, this->endOff, this->chunkSize, (numberType)0 );
    }

    AINLINE const numberType& GetCurrentOffset( void ) const
    {
        return this->curOff;
    }

    AINLINE const numberType& GetCurrentChunkOffset( void ) const
    {
        return this->offsetFromChunkStart;
    }

    AINLINE const numberType& GetCurrentChunkEndCount( void ) const
    {
        return this->offsetToChunkEnd;
    }

private:
    numberType curOff;
    numberType endOff;
    numberType chunkSize;
    numberType offsetFromChunkStart;
    numberType offsetToChunkEnd;
};

// In the ZIP and LZO compression streams there is a need to rapidly decompress
// data while still allowing stream seeking.
// Could be used to read raw sectors from sectored devices (ex. rotating-spindle HDDs).

// Assumes that the stream underneath is immutable across processing runtime (except across resets).
template <typename parserMetaType, size_t sectorBufSize>
struct chunked_buffer_processor
{
    template <typename... Args>
    AINLINE chunked_buffer_processor( eir::constr_with_alloc, Args&&... theArgs ) : metaData( std::forward <Args> ( theArgs )... )
    {
        // We assume that every chunk offset starts at 0.
        this->chunkOffset = 0;

        this->validBufCount = 0;
    }
    AINLINE chunked_buffer_processor( const chunked_buffer_processor& ) = delete;
    AINLINE chunked_buffer_processor( chunked_buffer_processor&& ) = delete;

    AINLINE ~chunked_buffer_processor( void )
    {
        return;
    }

    AINLINE chunked_buffer_processor& operator = ( const chunked_buffer_processor& ) = delete;
    AINLINE chunked_buffer_processor& operator = ( chunked_buffer_processor&& ) = delete;

    size_t ReadBytes( CFile *sourceFile, fsOffsetNumber_t backbufferSeek, fsOffsetNumber_t fileSeek, void *buffer, size_t readCount )
    {
        fsOffsetNumber_t fsReadCount = SeekPointerUtil::RegressToSeekType <fsOffsetNumber_t> ( readCount );

        chunked_iterator <fsOffsetNumber_t> chunk_iter( fileSeek, fileSeek + fsReadCount, sectorBufSize );

        size_t validBufCount = this->validBufCount;

        size_t totalReadCount = 0;

        // Initialize the file with the compressed seek.
        {
            int seekSuccess = sourceFile->SeekNative( backbufferSeek, SEEK_SET );

            if ( seekSuccess != 0 )
            {
                throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
            }
        }

        while ( !chunk_iter.IsEnd() )
        {
            fsOffsetNumber_t curFileSeek = chunk_iter.GetCurrentOffset();
            fsOffsetNumber_t fsChunkStart = chunk_iter.GetCurrentChunkOffset();
            fsOffsetNumber_t fsChunkEndOff = chunk_iter.GetCurrentChunkEndCount();

            size_t curBufReadReqCount = (size_t)fsChunkEndOff;

            fsOffsetNumber_t reqBufPos = ( curFileSeek - fsChunkStart );

            if ( this->chunkOffset != reqBufPos || validBufCount < sectorBufSize )
            {
                // Reposition the parsing engine, basically the compressed seek.
                this->metaData.TransitionSeek( sourceFile, this->chunkOffset + validBufCount, reqBufPos );

                this->chunkOffset = reqBufPos;

                validBufCount = this->metaData.ReadToBuffer( sourceFile, this->sectorBuf, sectorBufSize );
            }

            size_t curBufOff = (size_t)fsChunkStart;
            size_t curReadCount = BoundedBufferOperations <fsOffsetNumber_t>::CalculateReadCount( fsChunkStart, (fsOffsetNumber_t)validBufCount, curBufReadReqCount );

            const void *srcBufPtr = ( (const char*)this->sectorBuf + curBufOff );
            void *dstBufPtr = ( (char*)buffer + totalReadCount );

            memcpy( dstBufPtr, srcBufPtr, curReadCount );

            totalReadCount += curReadCount;

            if ( curReadCount != curBufReadReqCount )
            {
                break;
            }

            chunk_iter.Increment();
        }

        this->validBufCount = validBufCount;

        return totalReadCount;
    }

    parserMetaType& GetMetaData( void )
    {
        return this->metaData;
    }

private:
    parserMetaType metaData;

    fsOffsetNumber_t chunkOffset;

    unsigned char sectorBuf[ sectorBufSize ];
    size_t validBufCount;
};

#endif //_FILESYS_CHUNKED_BUFFER_STREAM_
