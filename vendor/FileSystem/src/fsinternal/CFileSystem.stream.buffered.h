/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.buffered.h
*  PURPOSE:     Buffered stream utilities for block-based streaming
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_BLOCK_BASED_STREAMING_
#define _FILESYSTEM_BLOCK_BASED_STREAMING_

#include <sdk/SortedSliceSector.h>

class CBufferedStreamWrap final : public CFile
{
public:
                            CBufferedStreamWrap( CFile *toBeWrapped, bool deleteOnQuit, size_t bufSize = 0 );
                            ~CBufferedStreamWrap( void );

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
    fsOffsetNumber_t        GetSizeNative   ( void ) const final override;
    void                    Flush           ( void ) override;
    CFileMappingProvider*   CreateMapping   ( void ) override;
    filePath                GetPath         ( void ) const override;
    bool                    IsReadable      ( void ) const noexcept override;
    bool                    IsWriteable     ( void ) const noexcept override;

    template <typename unsignedType>
    static inline unsignedType unsignedBarrierSubtract( unsignedType one, unsignedType two, unsignedType barrier )
    {
        if ( ( one + barrier ) < two )
        {
            return barrier;
        }

        return one - two;
    }

    // Type of the number for seeking operations.
    // Use this number sparsely as operations are slow on it.
    typedef fsOffsetNumber_t seekType_t;

    // Slice that should be used on buffered logic.
    typedef sliceOfData <seekType_t> seekSlice_t;

private:
    bool UpdateIOBufferPosition( fsOffsetNumber_t pos );
    void FlushIOBuffer( void ) const;

public:
    // Pointer to the underlying stream that has to be buffered.
    CFile *underlyingStream;

    // If true, underlyingStream is terminated when this buffered file terminates.
    bool terminateUnderlyingData;

    // Bytes that are being modified currently.
    void *internalIOBuffer;
    size_t internalIOBufferSize;

    // Size of the valid-bytes I/O buffer region (cached variable).
    mutable size_t internalIOBuffer_writeValidSize;
    mutable bool internalIOBuffer_hasDirtyBytes;

    typedef eir::mathSlice <size_t> bufSlice_t;

    // Validity region of bytes that are currently being modified.
    enum class ioBufDataState
    {
        DIRTY,
        COMMITTED
    };

    struct ioBufRegionMetaData
    {
        AINLINE ioBufRegionMetaData( bufSlice_t slice, ioBufDataState state ) : slice( std::move( slice ) ), state( state )
        {
            return;
        }
        AINLINE ioBufRegionMetaData( const ioBufRegionMetaData& right ) : slice( right.slice ), state( right.state )
        {
            return;
        }

        AINLINE bool IsMergeable( ioBufDataState state ) const
        {
            return this->state == state;
        }

        AINLINE void Update( bufSlice_t slice )
        {
            this->slice = std::move( slice );
        }

        AINLINE void Remove( void )
        {
            return;
        }

        AINLINE const bufSlice_t& GetNodeSlice( void ) const
        {
            return this->slice;
        }

        AINLINE ioBufDataState GetDataState( void ) const
        {
            return this->state;
        }

        bufSlice_t slice;
        ioBufDataState state;

        // Node for management purposes.
        mutable RwListEntry <ioBufRegionMetaData> node;
    };

    mutable eir::SortedSliceSector <size_t, FSObjectHeapAllocator, ioBufRegionMetaData> internalIOValidity;

    // Location of the buffer on the file-space.
    mutable fsOffsetNumber_t bufOffset;

private:
    // 64bit number that is the file's seek ptr.
    fsOffsetNumber_t fileSeek;
};

#endif //_FILESYSTEM_BLOCK_BASED_STREAMING_
