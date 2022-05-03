/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwblockapi.cpp
*  PURPOSE:     BlockProvider implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "streamutil.hxx"

namespace rw
{

struct rwBlockHeader
{
    endian::p_little_endian <uint32> type;
    endian::p_little_endian <uint32> length;
    HeaderInfo::PackedLibraryVersion libVer;
};

struct packedVersionStruct_rev1
{
    union
    {
        struct
        {
            uint16 packedReleaseMinorRev : 4;
            uint16 packedReleaseMajorRev : 4;
            uint16 packedLibraryMajorVer : 2;
            uint16 pad : 6;
        };
        uint16 libVer;
    };
};

struct packedVersionStruct_rev2
{
    union
    {
        struct
        {
            uint16 packedBinaryFormatRev : 6;
            uint16 packedReleaseMinorRev : 4;
            uint16 packedReleaseMajorRev : 4;
            uint16 packedLibraryMajorVer : 2;
        };
        uint16 libVer;
    };
};

void HeaderInfo::read(std::istream& rw)
{
	type = readUInt32(rw);
	length = readUInt32(rw);

    // Read the packed version.
    rw.read((char*)&this->packedVersion, sizeof(this->packedVersion));
}

uint32 HeaderInfo::write(std::ostream &rw)
{
	writeUInt32(type, rw);
	writeUInt32(length, rw);

    // Write the packed version.
    rw.write((const char*)&this->packedVersion, sizeof(this->packedVersion));

	return 3*sizeof(uint32);
}

inline bool isNewStyleVersioning( const LibraryVersion& libVer )
{
    return ( ( libVer.rwLibMajor == 3 && libVer.rwLibMinor >= 1 && ( libVer.rwRevMajor >= 1 || libVer.rwRevMinor >= 1 ) ) ||
             ( libVer.rwLibMajor >  3 ) ) ||
             ( libVer.buildNumber != 0xFFFF ) ||  // kind of want to support everything the user throws at us.
             ( libVer.rwRevMinor != 0 );
}

inline HeaderInfo::PackedLibraryVersion packVersion( LibraryVersion version )
{
    HeaderInfo::PackedLibraryVersion packedVersion;

    // Here, there can be two different versioning schemes.
    // Apparently, any version 3.1.0.0 and below use the rev1 version scheme, where
    // there is no build number. We have to obey that.
    bool isNewStyleVer = isNewStyleVersioning( version );

    if ( isNewStyleVer )
    {
        HeaderInfo::PackedLibraryVersion_rev2& rev2 = packedVersion.GetRevision2();

        rev2.buildNumber = version.buildNumber;

        packedVersionStruct_rev2 packVer;

        packVer.packedLibraryMajorVer = version.rwLibMajor - 3;
        packVer.packedReleaseMajorRev = version.rwLibMinor;
        packVer.packedReleaseMinorRev = version.rwRevMajor;
        packVer.packedBinaryFormatRev = version.rwRevMinor;

        rev2.packedVer = packVer.libVer;
    }
    else
    {
        // Old stuff. Does not support build numbers.
        packedVersionStruct_rev1 packVer;

        packVer.packedLibraryMajorVer = version.rwLibMajor;
        packVer.packedReleaseMajorRev = version.rwLibMinor;
        packVer.packedReleaseMinorRev = version.rwRevMajor;
        packVer.pad = 0;    // remember to zero things out nicely.

        packedVersion.GetRevision1().packedVer = packVer.libVer;
    }

    return packedVersion;
}

void HeaderInfo::setVersion(const LibraryVersion& version)
{
    this->packedVersion = packVersion( version );
}

inline LibraryVersion unpackVersion( HeaderInfo::PackedLibraryVersion packedVersion )
{
    LibraryVersion outVer;

    // Now we basically decide on the binary format of the packed struct.
    bool isNewStyleVer = packedVersion.isNewStyle();

    if ( isNewStyleVer )
    {
        const HeaderInfo::PackedLibraryVersion_rev2& rev2 = packedVersion.GetRevision2();

        outVer.buildNumber = rev2.buildNumber;

        packedVersionStruct_rev2 packVer;
        packVer.libVer = rev2.packedVer;

        outVer.rwLibMajor = 3 + packVer.packedLibraryMajorVer;
        outVer.rwLibMinor = packVer.packedReleaseMajorRev;
        outVer.rwRevMajor = packVer.packedReleaseMinorRev;
        outVer.rwRevMinor = packVer.packedBinaryFormatRev;
    }
    else
    {
        // Ugly old version ;)
        outVer.buildNumber = 0xFFFF;

        packedVersionStruct_rev1 packVer;
        packVer.libVer = packedVersion.GetRevision1().packedVer;

        outVer.rwLibMajor = packVer.packedLibraryMajorVer;
        outVer.rwLibMinor = packVer.packedReleaseMajorRev;
        outVer.rwRevMajor = packVer.packedReleaseMinorRev;
        outVer.rwRevMinor = 0;  // not used.
    }

    return outVer;
}

LibraryVersion HeaderInfo::getVersion(void) const
{
    return unpackVersion( this->packedVersion );
}

BlockProvider::BlockProvider( Stream *contextStream, eBlockMode blockMode )
{
    EngineInterface *rwEngine = (EngineInterface*)contextStream->engineInterface;

    this->parent = nullptr;
    this->current_child = nullptr;
    this->blockMode = blockMode;
    this->isInContext = false;
    this->contextStream = contextStream;
    this->ignoreBlockRegions = rwEngine->GetIgnoreSerializationBlockRegions();
    this->acquisitionMode = rwEngine->GetBlockAcquisitionMode();
    this->hasConstructorContext = false;
    this->isInErrorCondition = false;
}

BlockProvider::BlockProvider( Stream *contextStream, eBlockMode blockMode, bool ignoreBlockRegions, eBlockAcquisitionMode acqMode )
{
    this->parent = nullptr;
    this->current_child = nullptr;
    this->blockMode = blockMode;
    this->isInContext = false;
    this->contextStream = contextStream;
    this->ignoreBlockRegions = ignoreBlockRegions;
    this->acquisitionMode = acqMode;
    this->hasConstructorContext = false;
    this->isInErrorCondition = false;
}

void BlockProvider::moveFrom( BlockProvider&& right ) noexcept
{
    BlockProvider *parent = right.parent;
    BlockProvider *current_child = right.current_child;

    if ( parent )
    {
        parent->current_child = this;
    }

    if ( current_child )
    {
        current_child->parent = this;
    }

    this->parent = parent;
    this->current_child = current_child;
    this->blockMode = right.blockMode;
    this->isInContext = right.isInContext;
    this->contextStream = right.contextStream;
    this->ignoreBlockRegions = right.ignoreBlockRegions;
    this->acquisitionMode = right.acquisitionMode;
    this->hasConstructorContext = right.hasConstructorContext;
    this->blockContext = std::move( right.blockContext );
    this->isInErrorCondition = right.isInErrorCondition;
}

BlockProvider::BlockProvider( BlockProvider&& right ) noexcept
{
    this->moveFrom( std::move( right ) );
}

BlockProvider& BlockProvider::operator = ( BlockProvider&& right ) noexcept
{
    this->clear( true );
    this->moveFrom( std::move( right ) );

    return *this;
}

void BlockProvider::signal_blockapi_error( void ) noexcept
{
    BlockProvider *curprov = this;

    do
    {
        curprov->isInErrorCondition = true;

        curprov = curprov->parent;
    }
    while ( curprov != nullptr );
}

void BlockProvider::check_error_condition( void ) const
{
    if ( this->isInErrorCondition )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INTERNERR_ERRORCOND" );
    }
}

void BlockProvider::_EnterContextWrite( uint32 chunk_id )
{
    // Fill with default values.
    this->blockContext.chunk_id = chunk_id;
    this->blockContext.chunk_length = 0;

    // Decide which version this block should have.
    LibraryVersion blockVer;

    if ( BlockProvider *parentProvider = this->parent )
    {
        blockVer = parentProvider->getBlockVersion();
    }
    else
    {
        blockVer = this->getEngineInterface()->GetVersion();
    }

    this->blockContext.chunk_version = blockVer;

    // Just skip the header.
    this->skip_native( sizeof( rwBlockHeader ) );
}

void BlockProvider::_InternalContextStart( void )
{
    // Establish what device to use.
    Stream *contextStream = this->contextStream;

    // We expect the chunk_id, the chunk_length and the chunk_version to be set.

    this->blockContext.chunk_beg_offset = this->tell_native();

    this->blockContext.chunk_beg_offset_absolute = this->tell_absolute_native();

    this->blockContext.context_seek = 0;

    // Fix some block context things.
    if ( this->ignoreBlockRegions == false )
    {
        if ( this->blockMode == RWBLOCKMODE_READ )
        {
            // Since War Drum Studios even messed up the block header serialization logic, we need to fix things here.
            // Instead of seeing chunk_length as an absolute allocation that must be granted on stream-space, we
            // see it as a wish, if we are in root-block mode. This allows for truncation incase the stream turns out
            // smaller than expected. For proper measure, we shall warn the runtime that stream block truncation was performed.
            // This is only possible if we can request a size from the stream.
            if ( contextStream && contextStream->supportsSize() )
            {
                int64 virtualSize = this->blockContext.chunk_length;

                if ( virtualSize > 0 )
                {
                    int64 virtualOffset = this->blockContext.chunk_beg_offset_absolute;

                    streamMemSlice_t virtualSpace( virtualOffset, virtualSize );

                    int64 fileSize = contextStream->size();

                    streamMemSlice_t fileSpace( 0, fileSize );

                    eir::eIntersectionResult intResult = fileSpace.intersectWith( virtualSpace );

                    if ( intResult == eir::INTERSECT_BORDER_END )
                    {
                        // The file space is smaller than the virtual space suggests.
                        // We can fix that.
                        int64 newBlockLength = ( fileSpace.GetSliceEndPoint() - virtualOffset ) + 1;

                        this->blockContext.chunk_length = newBlockLength;

                        // Warn the runtime.
                        Interface *engineInterface = this->getEngineInterface();

                        if ( engineInterface->GetWarningLevel() >= 3 )
                        {
                            engineInterface->PushWarningToken( L"BLOCKAPI_WARN_BLOCKTRUNCATION" );
                        }
                    }
                }
            }
        }
    }

    // Verify the block context, if we do take the block regions into account.
    if ( this->ignoreBlockRegions == false )
    {
        if ( this->blockMode == RWBLOCKMODE_READ )
        {
            streamMemSlice_t blockAccess( this->blockContext.chunk_beg_offset_absolute, this->blockContext.chunk_length );

            this->verifyStreamAccess( blockAccess );
        }
    }

    this->isInContext = true;
}

void BlockProvider::EnterContextAny( void )
{
    assert( this->isInContext == false );

    this->check_error_condition();

    if ( this->blockMode == RWBLOCKMODE_READ )
    {
        // Read the header and set context information.
        rwBlockHeader blockHeader;

        this->read_native( &blockHeader, sizeof(blockHeader) );

        // Store the block context.
        this->blockContext.chunk_id = blockHeader.type;
        this->blockContext.chunk_length = blockHeader.length;

        this->blockContext.chunk_version = unpackVersion( blockHeader.libVer );
    }
    else if ( this->blockMode == RWBLOCKMODE_WRITE )
    {
        _EnterContextWrite( CHUNK_STRUCT );
    }

    _InternalContextStart();
}

void BlockProvider::EnterContextDirect( uint32 chunk_id )
{
    assert( this->isInContext == false );

    this->check_error_condition();

    if ( this->blockMode == RWBLOCKMODE_READ )
    {
        // Depends on the acquisition mode.
        eBlockAcquisitionMode acquireMode = this->acquisitionMode;

        rwBlockHeader blockHeader;

        if ( acquireMode == eBlockAcquisitionMode::FIND )
        {
            // Search through chunks in a line and return the first one we find with matching ID.
            while ( true )
            {
                this->read_native( &blockHeader, sizeof(blockHeader) );

                if ( blockHeader.type == chunk_id )
                {
                    break;
                }

                // Skip the data.
                this->skip_native( blockHeader.length );
            }
        }
        else if ( acquireMode == eBlockAcquisitionMode::EXPECTED )
        {
            uint64 possibleFaultOffset = this->tell_absolute_native();

            // If the first one we read does not match the request then bail.
            this->read_native( &blockHeader, sizeof(blockHeader) );

            uint32 foundChunkID = blockHeader.type;

            if ( foundChunkID != chunk_id )
            {
                uint32 parentChunkID = 0;

                if ( BlockProvider *parent = this->parent )
                {
                    parentChunkID = parent->blockContext.chunk_id;
                }

                throw BlockAPIUnexpectedChunkException( possibleFaultOffset, parentChunkID, chunk_id, foundChunkID );
            }
        }

        // Store the block context.
        this->blockContext.chunk_id = chunk_id;
        this->blockContext.chunk_length = blockHeader.length;

        this->blockContext.chunk_version = unpackVersion( blockHeader.libVer );
    }
    else if ( this->blockMode == RWBLOCKMODE_WRITE )
    {
        _EnterContextWrite( chunk_id );
    }

    _InternalContextStart();
}

void BlockProvider::LeaveContext( bool canPropagateException )
{
    assert( this->isInContext == true );
    assert( this->hasConstructorContext == false );

    if ( this->isInErrorCondition )
    {
        // Leaving blocks in error-state should be a NOOP.
        return;
    }

    try
    {
        bool shouldJumpToEnd = false;

        if ( this->blockMode == RWBLOCKMODE_WRITE )
        {
            // Update the block information.
            this->seek_native( this->blockContext.chunk_beg_offset - sizeof( rwBlockHeader ), RWSEEK_BEG );

            rwBlockHeader newHeader;

            newHeader.type = this->blockContext.chunk_id;
            newHeader.length = (uint32)this->blockContext.chunk_length;
            newHeader.libVer = packVersion( this->blockContext.chunk_version );

            this->write_native( &newHeader, sizeof( newHeader ) );

            shouldJumpToEnd = true;
        }
        else if ( this->blockMode == RWBLOCKMODE_READ )
        {
            if ( this->ignoreBlockRegions == false )
            {
                shouldJumpToEnd = true;
            }
        }

        if ( shouldJumpToEnd )
        {
            // Jump to the end of the block.
            int64 endPos = ( this->blockContext.chunk_beg_offset + this->blockContext.chunk_length );

            this->seek_native( endPos, RWSEEK_BEG );
        }

        this->isInContext = false;
    }
    catch( RwException& except )
    {
        this->signal_blockapi_error();

        if ( canPropagateException )
        {
            throw;
        }
        else
        {
            try
            {
                // Notify the runtime about an error that we could not propagate.
                Interface *rwEngine = this->getEngineInterface();

                auto exceptStr = rw::DescribeException( rwEngine, except );

                rwEngine->PushWarningSingleTemplate( L"BLOCKAPI_WARN_EATENEXCEPTION", L"except", exceptStr.GetConstString() );
            }
            catch( ... )
            {}

            // Just trucker on with error condition set.
        }
    }
    catch( ... )
    {
        this->signal_blockapi_error();

        if ( canPropagateException )
        {
            throw;
        }
    }
}

void BlockProvider::read_native( void *out_buf, size_t readCount )
{
    Stream *contextStream = this->contextStream;

    // If we have no stream, try reading from the parent.
    if ( contextStream != nullptr )
    {
        size_t actualReadCount = contextStream->read( out_buf, readCount );

        if ( actualReadCount != readCount )
        {
            throw StructuralErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_STRUCTERR_UNFINISHEDREAD" );
        }
    }
    else
    {
        BlockProvider *parentProvider = this->parent;

        if ( parentProvider )
        {
            parentProvider->read( out_buf, readCount );
        }
        else
        {
            throw StructuralErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INTERNERR_NORDBLOCKCTX" );
        }
    }
}

void BlockProvider::read( void *out_buf, size_t readCount )
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InvalidConfigurationException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    if ( readCount == 0 )
        return;

    if ( this->blockMode == RWBLOCKMODE_READ )
    {
        int64 totalStreamOffset = this->tell_absolute();

        // Verify this reading operation.
        streamMemSlice_t readAccess( totalStreamOffset, readCount );

        this->verifyLocalStreamAccess( readAccess );
    }

    // Do the native operation.
    this->read_native( out_buf, readCount );

    // Advance the virtual block context seek.
    this->blockContext.context_seek += readCount;
}

size_t BlockProvider::read_partial_native( void *out_buf, size_t readCount )
{
    Stream *contextStream = this->contextStream;

    // If we have no stream, try reading from the parent.
    if ( contextStream != nullptr )
    {
        return contextStream->read( out_buf, readCount );
    }
    else
    {
        BlockProvider *parentProvider = this->parent;

        if ( parentProvider )
        {
            return parentProvider->read_partial( out_buf, readCount );
        }
        else
        {
            throw StructuralErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INTERNERR_NORDBLOCKCTX" );
        }
    }
}

size_t BlockProvider::read_partial( void *out_buf, size_t readCount )
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InvalidConfigurationException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    // Actually transform the integer into stream-space.
    int64 readCount_streamMem = TRANSFORM_INT_CLAMP <int64> ( readCount );

    if ( readCount_streamMem == 0 )
        return 0;

    // Make sure that we clamp our read request into the block boundaries.
    streamMemSlice_t readRequest( this->tell_absolute(), readCount_streamMem );

    this->verifyLocalStreamAccess( readRequest, true );

    // Do the native operation.
    size_t actualReadCount = this->read_partial_native( out_buf, (size_t)readRequest.GetSliceSize() );

    // Advance the virtual block context seek.
    this->blockContext.context_seek += actualReadCount;

    return actualReadCount;
}

void BlockProvider::write_native( const void *in_buf, size_t writeCount )
{
    Stream *contextStream = this->contextStream;

    // If we have no stream ourselves, write it into the parent.
    if ( contextStream != nullptr )
    {
        size_t actualWriteCount = contextStream->write( in_buf, writeCount );

        if ( actualWriteCount != writeCount )
        {
            throw StructuralErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_STRUCTERR_UNFINISHEDWRITE" );
        }
    }
    else
    {
        BlockProvider *parentProvider = this->parent;

        if ( parentProvider )
        {
            parentProvider->write( in_buf, writeCount );
        }
        else
        {
            // TODO: maybe turn this into an InvalidOperationException?
            throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INTERNERR_NOWRBLOCKCTX" );
        }
    }
}

void BlockProvider::write( const void *in_buf, size_t writeCount )
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InvalidConfigurationException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    if ( writeCount == 0 )
        return;

    // Create a slice that represents our stream access.
    int64 totalStreamOffset = this->tell_absolute();

    streamMemSlice_t writeAccess( totalStreamOffset, writeCount );

    if ( this->blockMode == RWBLOCKMODE_READ )
    {
        // Verify this writing operation.
        this->verifyStreamAccess( writeAccess );
    }

    // Do the native operation.
    this->write_native( in_buf, writeCount );

    // If we are writing blocks, then extend the zone.
    if ( this->blockMode == RWBLOCKMODE_WRITE )
    {
        int64 virtualSize = this->blockContext.chunk_length;

        streamMemSlice_t virtualSlice( this->blockContext.chunk_beg_offset_absolute, virtualSize );

        eir::eIntersectionResult intResult = writeAccess.intersectWith( virtualSlice );

        if ( intResult == eir::INTERSECT_BORDER_START ||
             intResult == eir::INTERSECT_FLOATING_END ||
             intResult == eir::INTERSECT_UNKNOWN )
        {
            // We expand the valid region.
            this->blockContext.chunk_length = ( writeAccess.GetSliceEndPoint() - virtualSlice.GetSliceStartPoint() ) + 1;
        }
    }

    // Advance the virtual block seek.
    this->blockContext.context_seek += writeCount;
}

void BlockProvider::skip_native( size_t skipCount )
{
    Stream *contextStream = this->contextStream;

    if ( contextStream != nullptr )
    {
        contextStream->skip( skipCount );
    }
    else
    {
        BlockProvider *parentProvider = this->parent;

        if ( parentProvider )
        {
            parentProvider->skip( skipCount );
        }
        else
        {
            throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INTERNERR_NOSTREAMFORSKIP" );
        }
    }
}

void BlockProvider::skip( size_t skipCount )
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InvalidConfigurationException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    if ( skipCount == 0 )
        return;

    // Do the native operation.
    this->skip_native( skipCount );

    // Advance the virtual seek pointer.
    this->blockContext.context_seek += skipCount;
}

void BlockProvider::seek_native( int64 pos, eSeekMode mode )
{
    Stream *contextStream = this->contextStream;

    if ( contextStream )
    {
        contextStream->seek( pos, mode );
    }
    else
    {
        BlockProvider *parentProvider = this->parent;

        if ( parentProvider )
        {
            parentProvider->seek( pos, mode );
        }
        else
        {
            throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INTERNERR_NOSTREAMFORSEEK" );
        }
    }
}

int64 BlockProvider::tell_native( void ) const
{
    Stream *contextStream = this->contextStream;

    if ( contextStream )
    {
        return contextStream->tell();
    }

    BlockProvider *parentProvider = this->parent;

    if ( parentProvider )
    {
        return parentProvider->tell();
    }

    throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INTERNERR_NOSTREAMFORTELL" );
}

int64 BlockProvider::tell_absolute_native( void ) const
{
    int64 returnAbsolutePos = 0;

    Stream *contextStream = this->contextStream;

    if ( contextStream )
    {
        returnAbsolutePos = contextStream->tell();
    }
    else
    {
        const BlockProvider *parentProvider = this->parent;

        if ( parentProvider )
        {
            returnAbsolutePos = parentProvider->tell_absolute();
        }
        else
        {
            throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INTERNERR_NOSTREAMFORTELL" );
        }
    }

    return returnAbsolutePos;
}

int64 BlockProvider::tell( void ) const
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    return this->blockContext.context_seek;
}

int64 BlockProvider::tell_absolute( void ) const
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    return this->blockContext.chunk_beg_offset_absolute + this->blockContext.context_seek;
}

void BlockProvider::set_seek_internal( int64 pos )
{
    // Transform into absolute ones now to seek on our file.
    {
        int64 parentRelativeBlockOffset = pos + this->blockContext.chunk_beg_offset;

        // Do the native method.
        this->seek_native( parentRelativeBlockOffset, RWSEEK_BEG );
    }

    // Update the seek pointer.
    this->blockContext.context_seek = pos;
}

void BlockProvider::seek( int64 pos, eSeekMode mode )
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    // We expect local coordinates, so lets transform into absolute ones.
    int64 blockBaseOffset = 0;

    if ( mode == RWSEEK_BEG )
    {
        blockBaseOffset = 0;
    }
    else if ( mode == RWSEEK_CUR )
    {
        blockBaseOffset = this->blockContext.context_seek;
    }
    else if ( mode == RWSEEK_END )
    {
        blockBaseOffset = this->blockContext.chunk_length;
    }

    set_seek_internal( blockBaseOffset + pos );
}

void BlockProvider::seek_absolute( int64 off )
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    int64 newOffset = ( off - this->blockContext.chunk_beg_offset_absolute );

    // Push the seek down the block providers.
    set_seek_internal( newOffset );
}

void BlockProvider::check_read_ahead( size_t readCount ) const
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    if ( readCount == 0 )
        return;

    if ( this->blockMode == RWBLOCKMODE_READ )
    {
        // Simulate a read access.
        int64 totalStreamOffset = this->tell_absolute();

        // Verify this reading operation.
        streamMemSlice_t readAccess( totalStreamOffset, readCount );

        this->verifyStreamAccess( readAccess );
    }
}

Interface* BlockProvider::getEngineInterface( void ) const
{
    // We do not check for error condition in this special thing.

    Stream *contextStream = this->contextStream;

    if ( contextStream )
    {
        return contextStream->engineInterface;
    }

    BlockProvider *parentProvider = this->parent;

    if ( parentProvider )
    {
        return parentProvider->getEngineInterface();
    }

    throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INTERNERR_NOSTREAMFORENGINE" );
}

// Meta-data API.
void BlockProvider::setBlockID( uint32 id )
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    this->blockContext.chunk_id = id;
}

void BlockProvider::setBlockVersion( LibraryVersion version )
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    this->blockContext.chunk_version = version;
}

uint32 BlockProvider::getBlockID( void ) const
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    return this->blockContext.chunk_id;
}

int64 BlockProvider::getBlockLength( void ) const
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    return this->blockContext.chunk_length;
}

LibraryVersion BlockProvider::getBlockVersion( void ) const
{
    this->check_error_condition();

    if ( this->isInContext == false )
    {
        throw InternalErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_INVALIDCFG_NOBLOCKCONTEXT" );
    }

    return this->blockContext.chunk_version;
}

bool BlockProvider::doesIgnoreBlockRegions( void ) const
{
    this->check_error_condition();

    return this->ignoreBlockRegions;
}

eBlockAcquisitionMode BlockProvider::getBlockAcquisitionMode( void ) const
{
    this->check_error_condition();

    return this->acquisitionMode;
}

bool BlockProvider::hasParent( void ) const
{
    this->check_error_condition();

    return ( this->parent != nullptr );
}

// Validation API.
void BlockProvider::verifyLocalStreamAccess( streamMemSlice_t& requestedMemory, bool allowPartial ) const
{
    if ( requestedMemory.IsEmpty() == false )
    {
        auto isValidRegionAccess = [&]( const streamMemSlice_t& clamp_by ) noexcept -> bool
        {
            eir::eIntersectionResult intResult = requestedMemory.intersectWith( clamp_by );

            if ( allowPartial == false )
            {
                if ( intResult != eir::INTERSECT_EQUAL &&
                     intResult != eir::INTERSECT_INSIDE )
                {
                    return false;
                }
            }
            else
            {
                if ( intResult == eir::INTERSECT_FLOATING_START || intResult == eir::INTERSECT_BORDER_START )
                {
                    return false;
                }
            }

            return true;
        };

        
        // Check the virtual block region first.
        if ( this->ignoreBlockRegions == false )
        {
            int64 virtualSize = this->blockContext.chunk_length;

            streamMemSlice_t virtualSlice( this->blockContext.chunk_beg_offset_absolute, virtualSize );

            if ( isValidRegionAccess( virtualSlice ) == false )
            {
                throw StructuralErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_STRUCTERR_OOBACCESS" );
            }

            if ( allowPartial && virtualSlice.GetSliceEndBound() < requestedMemory.GetSliceEndBound() )
            {
                // Patch the request to actually valid count.
                requestedMemory.SetSliceEndBound( virtualSlice.GetSliceEndBound() );
            }
        }

        if ( Stream *contextStream = this->contextStream )
        {
            if ( contextStream->supportsSize() == true )
            {
                int64 streamSize = contextStream->size();

                streamMemSlice_t fileSlice( 0, streamSize );

                if ( isValidRegionAccess( fileSlice ) == false )
                {
                    throw StructuralErrorException( eSubsystemType::BLOCKAPI, L"BLOCKAPI_STRUCTERR_BLKLENGTHEXCEEDSFILE" );
                }

                if ( allowPartial && fileSlice.GetSliceEndBound() < requestedMemory.GetSliceEndBound() )
                {
                    requestedMemory.SetSliceEndBound( fileSlice.GetSliceEndBound() );
                }
            }
        }
    }
}

void BlockProvider::verifyStreamAccess( streamMemSlice_t& requestedMemory, bool allowPartial ) const
{
    // Check our access first.
    this->verifyLocalStreamAccess( requestedMemory, allowPartial );

    // If we do not have a stream...
    if ( this->contextStream == nullptr )
    {
        // ... check the parent.
        if ( BlockProvider *parentProvider = this->parent )
        {
            parentProvider->verifyStreamAccess( requestedMemory, allowPartial );
        }
    }
}

} // namespace rw