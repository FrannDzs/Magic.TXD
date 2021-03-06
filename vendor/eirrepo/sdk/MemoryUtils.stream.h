/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MemoryUtils.stream.h
*  PURPOSE:     Memory utilities based on stream processing.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _MEMORY_UTILITIES_STREAM_
#define _MEMORY_UTILITIES_STREAM_

#include "Exceptions.h"

#include <cstdint>
#include <cmath>
#include <cstring>

#include "MemoryRaw.h"
#include "Endian.h"
#include "MacroUtils.h"

#include <algorithm>

#ifdef _DEBUG
#include <assert.h>
#endif //_DEBUG

namespace SeekPointerUtil
{
    template <typename seekNumberType>
    AINLINE seekNumberType RegressToSeekType( size_t count )
    {
        seekNumberType seekCount;

        constexpr typename std::make_unsigned <seekNumberType>::type maxSeek = std::numeric_limits <seekNumberType>::max();

        if ( maxSeek < count )
        {
            // If we overshoot the maximum seek number, we still try to do the most.
            seekCount = maxSeek;
        }
        else
        {
            // We can safely assign.
            seekCount = (seekNumberType)count;
        }

        return seekCount;
    }
};

// Algorithms-only for performing read and write access on a size-bounded device.
template <typename seekNumberType>
struct BoundedBufferOperations
{
private:
    typedef sliceOfData <seekNumberType> streamSlice_t;

public:
    template <typename managerType>
    static AINLINE size_t CalculateWriteCount( seekNumberType currentSeekOffset, seekNumberType streamSize, size_t tryWriteCount, managerType *truncMan )
    {
        // We must properly transform writeCount into seek-space.
        seekNumberType seekWriteCount = SeekPointerUtil::RegressToSeekType <seekNumberType> ( tryWriteCount );

        streamSlice_t writeSlice( currentSeekOffset, seekWriteCount );

        // We can only write as much as there is space available.
        eir::eIntersectionResult intResult;

        if ( streamSize != 0 )
        {
            streamSlice_t fileBound( 0, streamSize );

            intResult = writeSlice.intersectWith( fileBound );
        }

        if ( streamSize != 0 &&
             intResult != eir::INTERSECT_EQUAL &&
             intResult != eir::INTERSECT_INSIDE &&
             intResult != eir::INTERSECT_BORDER_START &&
             intResult != eir::INTERSECT_FLOATING_END &&
             intResult != eir::INTERSECT_ENCLOSING )
        {
            // All other intersections do not produce a valid write access.
            return 0;
        }

        // If we are at crossing the end border of the file stream or just requesting data way
        // ahead to be written, we need to make a file buffer extension request to provide space.
        // This operation could fail.
        size_t possibleWriteCount = 0;

        if ( streamSize == 0 ||
             intResult == eir::INTERSECT_BORDER_START ||
             intResult == eir::INTERSECT_FLOATING_END ||
             intResult == eir::INTERSECT_ENCLOSING )
        {
            if ( streamSize != 0 && intResult == eir::INTERSECT_ENCLOSING && currentSeekOffset < 0 )
            {
                // We cannot write if our startpoint is invalid.
                return 0;
            }

            // Request a buffer update from the manager.
            {
                seekNumberType requiredBufferSize = ( writeSlice.GetSliceEndPoint() + 1 );

                if ( requiredBufferSize > streamSize )
                {
                    truncMan->Truncate( requiredBufferSize );
                }
            }

            streamSize = truncMan->Size();

            if ( streamSize != 0 )
            {
                // If possible, check if operation can be done, and how much.
                streamSlice_t newFileBound( 0, streamSize );

                eir::eIntersectionResult newIntResult = writeSlice.intersectWith( newFileBound );

                if ( newIntResult != eir::INTERSECT_EQUAL &&
                     newIntResult != eir::INTERSECT_INSIDE &&
                     newIntResult != eir::INTERSECT_BORDER_START )
                {
                    // We just cannot.
                    return 0;
                }

                // Safe to get a write count.
                possibleWriteCount = std::min( (size_t)seekWriteCount, (size_t)( newFileBound.GetSliceEndPoint() + 1 - currentSeekOffset ) );
            }
            else
            {
                possibleWriteCount = 0;
            }
        }
        else
        {
            // Assume we can just write all.
            possibleWriteCount = (size_t)seekWriteCount;
        }

        return possibleWriteCount;
    }

    static AINLINE size_t CalculateReadCount( seekNumberType currentSeekOffset, seekNumberType streamSize, size_t tryReadCount )
    {
        // Transform number into seek space.
        seekNumberType seekReadCount = SeekPointerUtil::RegressToSeekType <seekNumberType> ( tryReadCount );

        // Check read slice against file slice.
        streamSlice_t readSlice( currentSeekOffset, seekReadCount );

        streamSlice_t boundSlice( 0, streamSize );

        eir::eIntersectionResult intResult = readSlice.intersectWith( boundSlice );

        if ( intResult != eir::INTERSECT_EQUAL &&
             intResult != eir::INTERSECT_INSIDE &&
             intResult != eir::INTERSECT_BORDER_START &&
             intResult != eir::INTERSECT_ENCLOSING )
        {
            // We can only read valid data.
            return 0;
        }

        // Determine how much we can read.
        size_t readable;

        if ( intResult == eir::INTERSECT_BORDER_START ||
             intResult == eir::INTERSECT_ENCLOSING )
        {
            if ( intResult == eir::INTERSECT_ENCLOSING && currentSeekOffset < 0 )
            {
                // Cannot read if our startpoint is invalid.
                return 0;
            }

            readable = std::min( (size_t)seekReadCount, (size_t)( streamSize - currentSeekOffset ) );
        }
        else
        {
            readable = (size_t)seekReadCount;
        }

        return readable;
    }
};

// Only use after you have verified that the seek pointer really is not negative.
template <typename seekableType>
AINLINE typename std::make_unsigned <seekableType>::type stream_ptr_unsigned( const seekableType& ptr )
{
    static_assert( std::is_signed <seekableType>::value == true );

#ifdef _DEBUG
    assert( ptr >= 0 );
#endif //_DEBUG

    return (typename std::make_unsigned <seekableType>::type)ptr;
}

// Comparison helper for stream ptr with size integer.
template <typename seekableType>
AINLINE bool stream_ptr_less( const seekableType& ptr, const size_t& val )
{
    if constexpr ( std::is_signed <seekableType>::value )
    {
        if ( ptr < 0 )
        {
            return true;
        }

        return ( stream_ptr_unsigned( ptr ) < val );
    }
    else
    {
        return ( ptr < val );
    }
}

// We need a generic memory-aware bounded buffer device.
// Most importantly this is just a helper-class based on the BoundedBufferOperations.
template <typename seekNumberType, typename streamManagerExtension, bool isConst = false, bool releaseOnDestroy = true, typename exceptMan = eir::DefaultExceptionManager>
struct memoryBufferStream
{
    //static_assert( std::is_signed <seekNumberType>::value, "memory stream seek number must be of signed type" );

    using dataAccessType = typename std::conditional <isConst, const void, void>::type;

    inline memoryBufferStream( dataAccessType *memptr, seekNumberType streamSize, streamManagerExtension& manager ) : manager( &manager )
    {
        this->memptr = memptr;
        this->curOffset = 0;
        this->streamSize = streamSize;
    }

    inline memoryBufferStream( const memoryBufferStream& right ) = delete;
    inline memoryBufferStream( memoryBufferStream&& right ) noexcept
    {
        this->memptr = right.memptr;
        this->curOffset = std::move( right.curOffset );
        this->streamSize = std::move( right.streamSize );
        this->manager = right.manager;

        right.memptr = nullptr;
        right.curOffset = 0;
        right.streamSize = 0;
    }

private:
    inline void nativeRelease( void )
    {
        // Release all memory.
        manager->EstablishBufferView( this->memptr, this->streamSize, 0 );

#ifdef _DEBUG
        assert( this->memptr == nullptr );
#endif //_DEBUG
    }

public:
    inline ~memoryBufferStream( void )
    {
        if ( releaseOnDestroy )
        {
            this->nativeRelease();
        }
    }

    inline memoryBufferStream& operator = ( const memoryBufferStream& right ) = delete;
    inline memoryBufferStream& operator = ( memoryBufferStream&& right ) noexcept
    {
        // Delete our own resources, if available.
        this->nativeRelease();

        // Since resources have been released, the context of this object does not matter anymore.
        new (this) memoryBufferStream( std::move( right ) );

        return *this;
    }

    inline size_t Write( const void *inbuf, size_t _writeCount )
    {
        seekNumberType currentSeekOffset = this->curOffset;

        size_t possibleWriteCount = BoundedBufferOperations <seekNumberType>::CalculateWriteCount( currentSeekOffset, this->streamSize, _writeCount, this );

        if ( possibleWriteCount != 0 )
        {
            // Do the write operation.
            memcpy( (char*)this->memptr + currentSeekOffset, inbuf, possibleWriteCount );

            // We can safely cast to seekNumberType here because possibleWriteCount cannot
            // overshoot seekWriteCount anyway.
            this->curOffset += (seekNumberType)possibleWriteCount;
        }

        return possibleWriteCount;
    }

    inline size_t Read( void *outbuf, size_t _readCount )
    {
        seekNumberType currentSeekOffset = this->curOffset;

        size_t readable = BoundedBufferOperations <seekNumberType>::CalculateReadCount( currentSeekOffset, this->streamSize, _readCount );

        // We are safe to read.
        if ( readable > 0 )
        {
            const void *readSource = ( (char*)this->memptr + currentSeekOffset );

            memcpy( outbuf, readSource, readable );

            // Advance the seek.
            this->curOffset += (seekNumberType)readable;
        }

        return readable;
    }

    inline void Seek( seekNumberType offset )
    {
        this->curOffset = offset;
    }

    inline seekNumberType Tell( void ) const
    {
        return this->curOffset;
    }

    inline seekNumberType Size( void ) const
    {
        return this->streamSize;
    }

    inline void Truncate( seekNumberType size )
    {
        manager->EstablishBufferView( this->memptr, this->streamSize, size );

        // New file space has undefined content.
    }

    inline typename std::conditional <isConst, const void*, void*>::type Data( void )
    {
        return this->memptr;
    }

    inline const void* Data( void ) const
    {
        return this->memptr;
    }

private:
    dataAccessType *memptr;
    seekNumberType curOffset;
    seekNumberType streamSize;

    streamManagerExtension *manager;

public:
    // Helpers.
    template <typename structType>
    inline bool ReadStruct( structType& dataOut )
    {
        size_t dataReadCount = this->Read( &dataOut, sizeof( dataOut ) );

        return ( dataReadCount == sizeof( dataOut ) );
    }

    template <typename structType>
    inline bool WriteStruct( const structType& dataIn )
    {
        size_t dataWriteCount = this->Write( &dataIn, sizeof( dataIn ) );

        return ( dataWriteCount == sizeof( dataIn ) );
    }

#define MEMSTREAM_PIPE_HELPER( appendix, typeName ) \
    inline bool Read##appendix( typeName& outValue ) \
    { \
        endian::little_endian <typeName> val_le; \
        bool couldRead = ReadStruct( val_le ); \
        if ( !couldRead ) return false; \
        outValue = val_le; \
        return true; \
    } \
    inline bool Write##appendix( const typeName& inValue ) \
    { \
        endian::little_endian <typeName> val_le( inValue ); \
        bool couldWrite = WriteStruct( val_le ); \
        return couldWrite; \
    } \
    inline typeName Read##appendix##_EX( void ) \
    { \
        endian::little_endian <typeName> val_le; \
        bool couldRead = ReadStruct( val_le ); \
        if ( !couldRead ) exceptMan::throw_operation_failed( eir::eOperationType::READ ); \
        return val_le; \
    } \
    inline bool Read##appendix##_BE( typeName& outValue ) \
    { \
        endian::big_endian <typeName> val_le; \
        bool couldRead = ReadStruct( val_le ); \
        if ( !couldRead ) return false; \
        outValue = val_le; \
        return true; \
    } \
    inline bool Write##appendix##_BE( const typeName& inValue ) \
    { \
        endian::big_endian <typeName> val_le( inValue ); \
        bool couldWrite = WriteStruct( val_le ); \
        return couldWrite; \
    } \
    inline typeName Read##appendix##_BE_EX( void ) \
    { \
        endian::big_endian <typeName> val_le; \
        bool couldRead = ReadStruct( val_le ); \
        if ( !couldRead ) exceptMan::throw_operation_failed( eir::eOperationType::WRITE ); \
        return val_le; \
    } \

    MEMSTREAM_PIPE_HELPER( UInt8, std::uint8_t );
    MEMSTREAM_PIPE_HELPER( UInt16, std::uint16_t );
    MEMSTREAM_PIPE_HELPER( UInt32, std::uint32_t );
    MEMSTREAM_PIPE_HELPER( UInt64, std::uint64_t );
    MEMSTREAM_PIPE_HELPER( Int8, std::int8_t );
    MEMSTREAM_PIPE_HELPER( Int16, std::int16_t );
    MEMSTREAM_PIPE_HELPER( Int32, std::int32_t );
    MEMSTREAM_PIPE_HELPER( Int64, std::int64_t );
    MEMSTREAM_PIPE_HELPER( Float, std::float_t );
    MEMSTREAM_PIPE_HELPER( Double, std::double_t );
};

// Useful for constant read-only memory-streams.
template <typename seekType>
struct nullBufAllocMan
{
    inline void EstablishBufferView( const void*& memPtr, seekType& bufSizeOut, seekType reqSize )
    {
        return;
    }
};

namespace BasicMemStream
{
    template <typename numberType>
    struct basicMemStreamAllocMan
    {
        inline basicMemStreamAllocMan( void ) = default;
        inline basicMemStreamAllocMan( basicMemStreamAllocMan&& right ) = default;
        inline basicMemStreamAllocMan( const basicMemStreamAllocMan& right ) = delete;

        inline basicMemStreamAllocMan& operator = ( basicMemStreamAllocMan&& right ) = default;
        inline basicMemStreamAllocMan& operator = ( const basicMemStreamAllocMan& right ) = delete;

        inline void EstablishBufferView( void*& bufferPtrOut, numberType& bufSizeOut, numberType reqSize )
        {
            if ( reqSize == 0 )
            {
                if ( void *bufferPtr = bufferPtrOut )
                {
                    free( bufferPtrOut );

                    bufferPtrOut = nullptr;
                }

                bufSizeOut = 0;
            }
            else
            {
                void *newPtr = realloc( bufferPtrOut, reqSize );

                if ( newPtr )
                {
                    bufferPtrOut = newPtr;
                    bufSizeOut = reqSize;
                }
            }
        }
    };

    // Memory stream type that allocates it's buffer on CRT heap.
    template <typename numberType>
    using basicMemoryBufferStream = memoryBufferStream <numberType, basicMemStreamAllocMan <numberType>>;
};

#endif //_MEMORY_UTILITIES_STREAM_
