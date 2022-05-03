/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.img.translator.cpp
*  PURPOSE:     IMG R* Games archive management
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include <StdInc.h>
#include <sys/stat.h>

#include <sdk/MemoryUtils.stream.h>

// Include internal (private) definitions.
#include "fsinternal/CFileSystem.internal.h"
#include "fsinternal/CFileSystem.img.internal.h"

#include "fsinternal/CFileSystem.img.serialize.hxx"

extern CFileSystem *fileSystem;

#include "CFileSystem.Utils.hxx"

/*=====================================================
    CIMGArchiveTranslator::dataSectorStream

    IMG archive main data stream. It reads data
    from a file entry independent from the state.

    NOTE: In the past we used to have multiple stream
      types that were tied to different file data states.
      This has become inconvenient because the file data
      states could change while stream handles were active,
      causing conflicts.
=====================================================*/

inline CIMGArchiveTranslator::dataSectorStream::dataSectorStream( CIMGArchiveTranslator *translator, file *theFile, filesysAccessFlags accessMode ) : streamBase( translator, theFile, std::move( accessMode ) )
{
    this->m_currentSeek = 0;
}

inline CIMGArchiveTranslator::dataSectorStream::~dataSectorStream( void )
{
    return;
}

fsOffsetNumber_t CIMGArchiveTranslator::dataSectorStream::_getsize( void ) const
{
    // NOTE: this routine is internal; it expects to have a decompressed stream.

    file *fileInfo = this->m_info;

    eFileDataState dataState = fileInfo->metaData.dataState;

    fsOffsetNumber_t sizeOut = 0;

    if ( dataState == eFileDataState::PRESENT )
    {
        CFile *contentStream = fileInfo->metaData.GetDataStream();

        sizeOut = contentStream->GetSizeNative();
    }
    else if ( dataState == eFileDataState::ARCHIVED )
    {
        sizeOut = fileInfo->metaData.GetArchivedBlockSize();
    }

    return sizeOut;
}

size_t CIMGArchiveTranslator::dataSectorStream::Read( void *buffer, size_t readCount )
{
    if ( !IsReadable() )
        return 0;

    size_t actuallyReadItems = 0;

    file *fileInfo = this->m_info;

    // Touch data so that we can read decompressed data.
    fileInfo->metaData.PulseDecompression();

    eFileDataState dataState = fileInfo->metaData.dataState;

    if ( dataState == eFileDataState::PRESENT )
    {
        // TODO: this might need locking.

        CFile *contentStream = m_info->metaData.GetDataStream();

        contentStream->SeekNative( this->m_currentSeek, SEEK_SET );

        actuallyReadItems = contentStream->Read( buffer, readCount );

        this->m_currentSeek = contentStream->TellNative();
    }
    else if ( dataState == eFileDataState::ARCHIVED )
    {
        // Check the read region and respond accordingly.
        fsOffsetNumber_t currentSeek = ( this->m_currentSeek );

        size_t readable = BoundedBufferOperations <fsOffsetNumber_t>::CalculateReadCount( currentSeek, _getsize(), readCount );

        if ( readable > 0 )
        {
            fileInfo->metaData.TargetContentFile( currentSeek );

            // Attempt to read.
            size_t actuallyRead = this->m_translator->m_contentFile->Read( buffer, readable );

            // Advance the seek by the bytes that have been read.
            this->m_currentSeek += actuallyRead;

            actuallyReadItems = actuallyRead;
        }
    }
    else
    {
        assert( 0 );
    }

    return actuallyReadItems;
}

fsOffsetNumber_t CIMGArchiveTranslator::dataSectorStream::archivedTruncMan_t::Size( void ) const
{
    const dataSectorStream *stream = this->GetStream();

    file *fileInfo = stream->m_info;

    assert( fileInfo->metaData.dataState == eFileDataState::ARCHIVED );

    return fileInfo->metaData.GetArchivedBlockSize();
}

void CIMGArchiveTranslator::dataSectorStream::archivedTruncMan_t::Truncate( fsOffsetNumber_t targetSize )
{
    //TODO:
    // If we intersected the starting border of the seek slice, it means that the end point is after the end of the file bounds.
    // We have to expand this file region then.
    // This may require relocating the file to a position of bigger available space.

#if 0
    dataSectorStream *stream = this->GetStream();

    CIMGArchiveTranslator *translator = stream->m_translator;

    file *fileInfo = stream->m_info;
#endif

    assert( 0 );
}

size_t CIMGArchiveTranslator::dataSectorStream::Write( const void *buffer, size_t writeCount )
{
    if ( !IsWriteable() )
        return 0;

    size_t actuallyWrittenItems = 0;

    file *fileInfo = this->m_info;

    // Touch data so that we can read decompressed data.
    fileInfo->metaData.PulseDecompression();

    eFileDataState dataState = fileInfo->metaData.dataState;

    if ( dataState == eFileDataState::PRESENT )
    {
        // This might need locking, again.

        CFile *contentStream = m_info->metaData.GetDataStream();

        contentStream->SeekNative( this->m_currentSeek, SEEK_SET );

        actuallyWrittenItems = contentStream->Write( buffer, writeCount );

        this->m_currentSeek = contentStream->TellNative();
    }
    else if ( dataState == eFileDataState::ARCHIVED )
    {
        fsOffsetNumber_t currentSeekOffset = this->m_currentSeek;

        size_t writeable = BoundedBufferOperations <fsOffsetNumber_t>::CalculateWriteCount( currentSeekOffset, this->_getsize(), writeCount, &this->archivedTruncMan );

        if ( writeable > 0 )
        {
            // Since the calculation routine says go, we can easily write.
            fileInfo->metaData.TargetContentFile( currentSeekOffset );

            size_t actuallyWritten = this->m_translator->m_contentFile->Write( buffer, writeable );

            // Advance the seek by the amount we actually wrote.
            this->m_currentSeek += actuallyWritten;

            actuallyWrittenItems = actuallyWritten;
        }
    }
    else
    {
        assert( 0 );
    }

    return actuallyWrittenItems;
}

int CIMGArchiveTranslator::dataSectorStream::Seek( long iOffset, int iType )
{
    // Update the seek pointer depending on operation.
    long offsetBase = 0;

    if ( iType == SEEK_SET )
    {
        offsetBase = 0;
    }
    else if ( iType == SEEK_CUR )
    {
        offsetBase = (long)this->m_currentSeek;
    }
    else if ( iType == SEEK_END )
    {
        file *fileInfo = this->m_info;

        // Touch data so that we can read decompressed data.
        fileInfo->metaData.PulseDecompression();

        offsetBase = (long)this->_getsize();
    }
    else
    {
        return -1;
    }

    fsOffsetNumber_t newOffset = (fsOffsetNumber_t)( offsetBase + iOffset );

    // Alright, set the new offset.
    this->m_currentSeek = newOffset;

    return 0;
}

int CIMGArchiveTranslator::dataSectorStream::SeekNative( fsOffsetNumber_t iOffset, int iType )
{
    // Do the same as seek, but do so with higher accuracy.
    fsOffsetNumber_t offsetBase;

    if ( iType == SEEK_SET )
    {
        offsetBase = 0;
    }
    else if ( iType == SEEK_CUR )
    {
        offsetBase = this->m_currentSeek;
    }
    else if ( iType == SEEK_END )
    {
        file *fileInfo = this->m_info;

        // Touch data so that we can read decompressed data.
        fileInfo->metaData.PulseDecompression();

        offsetBase = this->_getsize();
    }
    else
    {
        return -1;
    }

    fsOffsetNumber_t newOffset = ( offsetBase + iOffset );

    // Update our offset.
    this->m_currentSeek = newOffset;

    return 0;
}

long CIMGArchiveTranslator::dataSectorStream::Tell( void ) const noexcept
{
    return (long)this->m_currentSeek;
}

fsOffsetNumber_t CIMGArchiveTranslator::dataSectorStream::TellNative( void ) const noexcept
{
    return this->m_currentSeek;
}

bool CIMGArchiveTranslator::dataSectorStream::IsEOF( void ) const noexcept
{
    // Dispatch properly according to file data state.
    file *fileInfo = this->m_info;

    // Touch data so that we can read decompressed data.
    fileInfo->metaData.PulseDecompression();

    eFileDataState dataState = fileInfo->metaData.dataState;

    bool isEnded = false;

    if ( dataState == eFileDataState::PRESENT )
    {
        CFile *contentStream = fileInfo->metaData.GetDataStream();

        // Check if we went past the content stream.
        fsOffsetNumber_t contentStreamSize = contentStream->GetSizeNative();

        isEnded = ( this->m_currentSeek >= contentStreamSize );
    }
    else if ( dataState == eFileDataState::ARCHIVED )
    {
        isEnded = ( this->m_currentSeek >= _getsize() );
    }

    return isEnded;
}

void CIMGArchiveTranslator::dataSectorStream::SetSeekEnd( void )
{
    if ( !IsWriteable() )
        return;

    file *fileInfo = this->m_info;

    // Touch data so that we can read decompressed data.
    fileInfo->metaData.PulseDecompression();

    eFileDataState dataState = fileInfo->metaData.dataState;

    if ( dataState == eFileDataState::PRESENT )
    {
        // Truncate the stream to the current seek.
        CFile *contentStream = fileInfo->metaData.GetDataStream();

        contentStream->SeekNative( this->m_currentSeek, SEEK_SET );

        contentStream->SetSeekEnd();
    }
    else if ( dataState == eFileDataState::PRESENT_COMPRESSED )
    {
        // TODO: properly handle truncation to the stream, just like would happen during write.
    }
}

size_t CIMGArchiveTranslator::dataSectorStream::GetSize( void ) const noexcept
{
    // Please note that while the size may return a value it is not a guarrantee that
    // all bytes of that size are either readable or writeable.

    file *fileInfo = this->m_info;

    // Touch data so that we can read decompressed data.
    fileInfo->metaData.PulseDecompression();

    return (size_t)_getsize();
}

fsOffsetNumber_t CIMGArchiveTranslator::dataSectorStream::GetSizeNative( void ) const noexcept
{
    file *fileInfo = this->m_info;

    // Touch data so that we can read decompressed data.
    fileInfo->metaData.PulseDecompression();

    return _getsize();
}

void CIMGArchiveTranslator::dataSectorStream::Flush( void )
{
    if ( !IsWriteable() )
        return;

    file *fileInfo = this->m_info;

    // We do not have to decompress in this call, because Flush is just a hint.

    eFileDataState dataState = fileInfo->metaData.dataState;

    if ( dataState == eFileDataState::PRESENT )
    {
        // Just push the request to the stream underneath.
        CFile *contentStream = fileInfo->metaData.GetDataStream();

        contentStream->Flush();
    }
    else if ( dataState == eFileDataState::ARCHIVED )
    {
        // We do not keep buffers ourselves over here.
        // This could change in the future, when we actually will acquire an IMG-model block based stream.

        this->m_translator->m_contentFile->Flush();
    }
}

/*=======================================
    CIMGArchiveTranslator

    R* Games IMG archive management
=======================================*/

// Header of a file entry in the IMG archive.
struct resourceFileHeader_ver1
{
    endian::little_endian <fsUInt_t>    offset;         // 0
    endian::little_endian <fsUInt_t>    fileDataSize;   // 4
    char                                name[24];       // 8
};

struct resourceFileHeader_ver2
{
    endian::little_endian <fsUInt_t>    offset;         // 0
    endian::little_endian <fsUShort_t>  fileDataSize;   // 4
    endian::little_endian <fsUShort_t>  expandedSize;   // 6
    char                                name[24];       // 8
};

CIMGArchiveTranslator::CIMGArchiveTranslator( imgExtension& imgExt, CFile *contentFile, CFile *registryFile, eIMGArchiveVersion theVersion, bool isLiveMode )
    : CSystemPathTranslator( basicRootPathType <pathcheck_always_allow> (), false, true ), m_imgExtension( imgExt ), m_contentFile( contentFile ), m_registryFile( registryFile ), fileMan( imgExt.GetSystem() ), m_virtualFS( true )
{
    // Set up the virtual file system by giving the
    // translator pointer to it.
    m_virtualFS.hostTranslator = this;

    this->isLiveMode = isLiveMode;

    // Fill out default fields.
    this->m_version = theVersion;

    // We have no compression handler by default.
    this->m_compressionHandler = nullptr;
}

CIMGArchiveTranslator::~CIMGArchiveTranslator( void )
{
    // Deallocate all files.
    {
        // Deallocate the file headers.
        this->releaseFileHeadersBlock();

        // Now there should be only legit files in the allocated list.

        LIST_FOREACH_BEGIN( fileAddrAlloc_t::block_t, this->fileAddressAlloc.blockList.root, node )

            fileMetaData *fileMeta = LIST_GETITEM( fileMetaData, item, allocBlock );

            fileMeta->isAllocated = false;

        LIST_FOREACH_END

        this->fileAddressAlloc.Clear();
    }

    // Unlock the archive file.
    delete this->m_contentFile;

    if ( this->m_version == IMG_VERSION_1 )
    {
        delete this->m_registryFile;
    }

    // All file links will be deleted automatically.
}

bool CIMGArchiveTranslator::CreateDir( const char *path, filesysPathOperatingMode opMode, bool createParentDirs )
{
    return m_virtualFS.CreateDir( path, opMode, createParentDirs );
}

CFile* CIMGArchiveTranslator::OpenNativeFileStream( file *fsObject, eVFSFileOpenMode openMode, const filesysAccessFlags& flags )
{
    CFile *outputStream = nullptr;

    bool isLiveMode = this->isLiveMode;

    // TODO.
    assert( isLiveMode == false );

    eFileDataState dataState = fsObject->metaData.dataState;

    if ( openMode == eVFSFileOpenMode::OPEN )
    {
        // Sometimes data has to be prepared before opening.
        if ( dataState == eFileDataState::ARCHIVED ||
             dataState == eFileDataState::PRESENT_COMPRESSED )
        {
            // Determine whether our stream needs extraction.
            bool needsExtraction = false;

            if ( !needsExtraction )
            {
                // If we want write access, we definately have to extract.
                if ( flags.allowWrite )
                {
                    needsExtraction = true;
                }
            }

            if ( !needsExtraction )
            {
                // Another case of requiring extraction is if the stream is compressed.
                // We must check for that.
                needsExtraction = this->RequiresExtraction( fsObject );
            }

            // If we have to extract then do it right away.
            // This will turn our file into PRESENT status.
            if ( needsExtraction )
            {
                bool extractSuccess = this->ExtractStream( fsObject );

                if ( !extractSuccess )
                {
                    // We failed.
                    return nullptr;
                }

                dataState = fsObject->metaData.dataState;
            }
        }

        // If the data has been extracted already from the archive, we need to open a disk stream.
        if ( dataState == eFileDataState::PRESENT ||
             dataState == eFileDataState::PRESENT_COMPRESSED ||
             dataState == eFileDataState::ARCHIVED )
        {
            // Create our stream.
            outputStream = new dataSectorStream( this, fsObject, flags );
        }
        else
        {
            // Anything we have not dealt with yet?
            return nullptr;
        }
    }
    else if ( openMode == eVFSFileOpenMode::CREATE )
    {
        CFile *newDataStream = this->fileMan.AllocateTemporaryDataDestination();

        if ( newDataStream )
        {
            fsObject->metaData.dataState = eFileDataState::PRESENT;
            fsObject->metaData.AcquireDataStream( newDataStream );

            // Return a stream to it.
            outputStream = new dataSectorStream( this, fsObject, flags );
        }
    }

    return outputStream;
}

CFile* CIMGArchiveTranslator::fileMetaData::GetInputStream( fsOffsetNumber_t& streamBegOffsetOut )
{
    CFile *inputStream;
    fsOffsetNumber_t inputBegOffset;

    if ( dataState == eFileDataState::ARCHIVED )
    {
        inputStream = this->GetTranslator()->m_contentFile;
        inputBegOffset = this->GetArchivedOffsetToFile();
    }
    else if ( dataState == eFileDataState::PRESENT_COMPRESSED || dataState == eFileDataState::PRESENT )
    {
        inputStream = this->dataStream;
        inputBegOffset = 0;
    }
    else
    {
        throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
    }

    if ( inputStream )
    {
        streamBegOffsetOut = inputBegOffset;
    }

    return inputStream;
}

bool CIMGArchiveTranslator::RequiresExtraction( file *fileInfo )
{
    return fileInfo->metaData.TouchDataRequiresExtraction();
}

void CIMGArchiveTranslator::fileMetaData::AcquireDataStream( CFile *dataStream )
{
    this->releaseDataStream();

    this->dataStream = dataStream;
}

CFile* CIMGArchiveTranslator::fileMetaData::GetDataStream( void )
{
    return this->dataStream;
}

bool CIMGArchiveTranslator::fileMetaData::TouchDataRequiresExtraction( void )
{
    eFileDataState dataState = this->dataState;

    // Check according to the data state.
    if ( dataState != eFileDataState::ARCHIVED && dataState != eFileDataState::PRESENT_COMPRESSED )
        return false;

    // Determine the input stream.
    fsOffsetNumber_t inputBegOffset;
    CFile *inputStream = this->GetInputStream( inputBegOffset );

    if ( !inputStream )
        return false;

    bool needsExtraction = false;

    CIMGArchiveTranslator *translator = this->GetTranslator();

    // If we have a compression provider, check it.
    if ( CIMGArchiveCompressionHandler *compressHandler = translator->m_compressionHandler )
    {
        inputStream->SeekNative( inputBegOffset, SEEK_SET );

        // Query the compression provider.
        bool isCompressed = compressHandler->IsStreamCompressed( inputStream );

        if ( isCompressed )
        {
            // If we are compressed, we must decompress.
            // This is done at extraction time.
            needsExtraction = true;
        }
    }

    return needsExtraction;
}

bool CIMGArchiveTranslator::fileMetaData::TouchDataExtractStream( void )
{
    eFileDataState dataState = this->dataState;

    // We could be extracted already.
    if ( dataState == eFileDataState::PRESENT )
        return true;

    // Determine the input stream.
    fsOffsetNumber_t inputBegOffset;
    CFile *inputStream = this->GetInputStream( inputBegOffset );

    if ( !inputStream )
        return false;

    CIMGArchiveTranslator *translator = this->GetTranslator();

    // Establish an output stream.
    CFile *outputStream = translator->fileMan.AllocateTemporaryDataDestination();

    if ( !outputStream )
        return false;

    // Another instance where I absolutely hate that C++ does not have try-finally statements.
    // I know that outputStream != NULL, but using any special std ptr would check for NULL again,
    // wasting my knowledge on the runtime. With try-finally I could simply return inside of that
    // try-block and my clear-cut finally statement could execute.
    // But since we do not have any finally-block returning inside of the try-block destroys resources.
    // How about promoting C++ as a language of your home-grown coding principles instead?

    // You might ask why still. Imagine that resources are defined in an incomplete state here.
    // Creating RAII objects for all the possible incomplete states is a waste of coding space,
    // because the complexity of incompleteness scales exponentially.

    bool success = false;

    try
    {
        // Set it to beginning for operations.
        inputStream->SeekNative( inputBegOffset, SEEK_SET );

        // If we have a compressed provider and it recognizes that the stream is compressed,
        // then let it process the stream.
        bool isStreamCompressed = false;

        CIMGArchiveCompressionHandler *compressHandler = translator->m_compressionHandler;

        if ( compressHandler )
        {
            isStreamCompressed = compressHandler->IsStreamCompressed( inputStream );

            // Set the stream into position because we must process it and
            // it may have been distorted by the compression handler.
            inputStream->SeekNative( inputBegOffset, SEEK_SET );
        }

        if ( isStreamCompressed )
        {
            assert( compressHandler != nullptr );

            success = compressHandler->Decompress( inputStream, outputStream );
        }
        else
        {
            // Simply copy the stream.
            FileSystem::StreamCopy( *inputStream, *outputStream );

            success = true;
        }
    }
    catch( ... )
    {
        delete outputStream;

        throw;
    }

    if ( !success )
    {
        delete outputStream;

        return false;
    }

    // We successfully copied, so mark as present.
    this->dataState = eFileDataState::PRESENT;
    this->AcquireDataStream( outputStream );

    return true;
}

void CIMGArchiveTranslator::fileMetaData::PulseDecompression( void )
{
    // When a stream wants to touch data inside of the archive, it could be compressed.
    // In that case we have to turn it into a decompressed entry and compress it when
    // it has to be in the future. To prevent decompressing user-data and to not cause
    // infinite loops we only check when its not PRESENT data state.

    if ( TouchDataRequiresExtraction() )
    {
        TouchDataExtractStream();
    }
}

bool CIMGArchiveTranslator::ExtractStream( file *theFile )
{
    return theFile->metaData.TouchDataExtractStream();
}

CFile* CIMGArchiveTranslator::Open( const char *path, const filesysOpenMode& mode, eFileOpenFlags flags )
{
    // TODO: any use for flags?

    return m_virtualFS.OpenStream( path, mode );
}

bool CIMGArchiveTranslator::Exists( const char *path ) const
{
    return m_virtualFS.Exists( path );
}

void CIMGArchiveTranslator::fileMetaData::OnFileDelete( void )
{
    // Since we do not keep external storage, there is nothing to do here.
}

bool CIMGArchiveTranslator::Delete( const char *path, filesysPathOperatingMode opMode )
{
    return m_virtualFS.Delete( path, opMode );
}

bool CIMGArchiveTranslator::fileMetaData::OnFileCopy( const dirNames& tree, const filePath& newName ) const
{
    // TODO: think about how to handle live-mode data.

    // Since we copy data directly over during copying file attributes, there is nothing to do here.
    return true;
}

void CIMGArchiveTranslator::OnRemoveRemoteFile( const dirNames& tree, const filePath& newName )
{
    // We do not have any meta-data to delete.
}

bool CIMGArchiveTranslator::Copy( const char *src, const char *dst )
{
    return m_virtualFS.Copy( src, dst );
}

bool CIMGArchiveTranslator::fileMetaData::OnFileRename( const dirNames& tree, const filePath& newName )
{
    // Data is being handled by an internal link, so we do not have to worry.
    return true;
}

bool CIMGArchiveTranslator::Rename( const char *src, const char *dst )
{
    return m_virtualFS.Rename( src, dst );
}

bool CIMGArchiveTranslator::fileMetaData::CopyAttributesTo( fileMetaData& dstEntry ) const
{
    CIMGArchiveTranslator *translator = this->GetTranslator();

    if ( translator->isLiveMode )
    {
        // TODO.
        assert( 0 );
    }
    else
    {
        eFileDataState dataState = this->dataState;

        dstEntry.dataState = dataState;
        memcpy( dstEntry.resourceName, this->resourceName, sizeof( this->resourceName ) );

        if ( dataState == eFileDataState::ARCHIVED )
        {
            assert( translator->isLiveMode == false );

            // If we are not live-mode, then data is assumed to be immutable inside of
            // the archive during edit-phase.
            dstEntry.blockOffset = this->blockOffset;
            dstEntry.resourceSize = this->resourceSize;
        }
        else if ( dataState == eFileDataState::PRESENT ||
                  dataState == eFileDataState::PRESENT_COMPRESSED )
        {
            // Nothing really to do here, for now.
        }
        else
        {
            assert( 0 );
        }

        // Copy over any available data stream.
        {
            CFile *dataStream = this->dataStream;

            if ( dataStream )
            {
                // We always choose the repository for files.
                CFile *dstDataStream = translator->fileMan.AllocateTemporaryDataDestination();

                if ( !dstDataStream )
                {
                    return false;
                }

                // Copy over the entire data.
                dataStream->SeekNative( 0, SEEK_SET );

                FileSystem::StreamCopy( *dataStream, *dstDataStream );

                dstEntry.dataStream = dstDataStream;
            }
        }
    }

    return true;
}

size_t CIMGArchiveTranslator::Size( const char *path ) const
{
    return (size_t)m_virtualFS.Size( path );
}

bool CIMGArchiveTranslator::QueryStats( const char *path, filesysStats& statsOut ) const
{
    return m_virtualFS.QueryStats( path, statsOut );
}

bool CIMGArchiveTranslator::OnConfirmDirectoryChange( const translatorPathResult& nodePath )
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

void CIMGArchiveTranslator::ScanDirectory( const char *path, const char *wildcard, bool recurse, pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata ) const
{
    m_virtualFS.ScanDirectory( path, wildcard, recurse, dirCallback, fileCallback, userdata );
}

void CIMGArchiveTranslator::ScanFilesInSerializationOrder( pathCallback_t filecb, void *ud ) const
{
    const_cast <CIMGArchiveTranslator*> ( this )->ForAllAllocatedFiles(
        [&]( const file *info )
        {
            filecb( info->GetRelativePath(), ud );
        }
    );
}

void CIMGArchiveTranslator::GetDirectories( const char *path, const char *wildcard, bool recurse, dirNames& output ) const
{
    ScanDirectory( path, wildcard, recurse, (pathCallback_t)_scanFindCallback, nullptr, &output );
}

void CIMGArchiveTranslator::GetFiles( const char *path, const char *wildcard, bool recurse, dirNames& output ) const
{
    ScanDirectory( path, wildcard, recurse, nullptr, (pathCallback_t)_scanFindCallback, &output );
}

CDirectoryIterator* CIMGArchiveTranslator::BeginDirectoryListing( const char *path, const char *wildcard, const scanFilteringFlags& filter_flags ) const
{
    return m_virtualFS.BeginDirectoryListing( path, wildcard, filter_flags );
}

template <std::unsigned_integral numberType>
static AINLINE fsUInt_t getDataBlockCount( numberType dataSize )
{
    return (fsUInt_t)ALIGN_SIZE( dataSize, (numberType)IMG_BLOCK_SIZE ) / (numberType)IMG_BLOCK_SIZE;
}

bool CIMGArchiveTranslator::AllocateFileEntry( file *fileEntry )
{
    // Only valid to call if not allocated already.
    if ( fileEntry->metaData.isAllocated )
        return false;

    // We use the resource size that is stored in the file entry.
    size_t reqBlockCount = fileEntry->metaData.resourceSize;

    // Try to find a position where our file fits into.
    // Remember that the IMG container is specified to be a concatenation of
    // file chunks without free space in-between. The fixup of this
    // has to be applied during saving.
    fileAddrAlloc_t::allocInfo allocInfo;

    bool foundSpace = this->fileAddressAlloc.FindSpace( reqBlockCount, allocInfo, 1 );  // alignment == 1 because we count in blocks, not bytes.

    if ( !foundSpace )
        return false;

    // Save this allocation.
    this->fileAddressAlloc.PutBlock( &fileEntry->metaData.allocBlock, allocInfo );

    fileEntry->metaData.isAllocated = true;

    // Sync the entries.
    fileEntry->metaData.blockOffset = allocInfo.slice.GetSliceStartPoint();

    // Success!
    return true;
}

bool CIMGArchiveTranslator::ReallocateFileEntry( file *fileEntry, fsOffsetNumber_t fileSize )
{
    if ( fileSize < 0 )
        return false;

    // Only valid to call if the file entry is allocated already.
    if ( fileEntry->metaData.isAllocated == false )
        return false;

    // Calculate the block count.
    size_t newBlockCount = getDataBlockCount( (std::make_unsigned <fsOffsetNumber_t>::type)fileSize );

    size_t oldBlockCount = fileEntry->metaData.resourceSize;

    // If the block count did not change, no point in continuing.
    if ( newBlockCount == oldBlockCount )
    {
        return true;
    }

    // If we are some blocks shorter, then truncating is really simple (no-op).
    // Otherwise we might get more complicated tasks.
    {
        // Check if we can reallocate in place.
        // If we can, we just do that.
        bool canReallocateInPlace =
            this->fileAddressAlloc.SetBlockSize( &fileEntry->metaData.allocBlock, newBlockCount );

        if ( !canReallocateInPlace )
        {
            // Turns out we cannot simply reallocate our size.
            // This means that we have to actually _move our data_.
            assert( 0 );    // todo.
        }
    }

    // Update the boundaries, as they have been updated on physical storage.
    fileEntry->metaData.resourceSize = newBlockCount;

    return true;
}

void CIMGArchiveTranslator::fileMetaData::releaseAllocation( void )
{
    file *fileInfo = LIST_GETITEM( file, this, metaData );

    CIMGArchiveTranslator *translator = this->GetTranslator();

    translator->DeallocateFileEntry( fileInfo );
}

void CIMGArchiveTranslator::DeallocateFileEntry( file *fileEntry )
{
    // TODO: add locks to the allocation routines.

    if ( fileEntry->metaData.isAllocated == false )
        return;

    this->fileAddressAlloc.RemoveBlock( &fileEntry->metaData.allocBlock );

    fileEntry->metaData.isAllocated = false;
}

void CIMGArchiveTranslator::GenerateFileHeaderStructure( headerGenPresence& genOut )
{
    // Generate for all sub directories first.
    this->m_virtualFS.ForAllItems(
        [&]( fsActiveEntry *item )
    {
        if ( item->isFile )
        {
            // Just add together all the file counts.
            genOut.numOfFiles++;
        }
    });
}

void CIMGArchiveTranslator::GenerateArchiveStructure( archiveGenPresence& genOut )
{
    /*
        The purpose of this routine is to prepare all the data storage for building a new IMG archive.

        - NON-LIVE MODE: since the archive stream is about to be rewritten we need to save the
            data in different memory, possibly on the disk.
        - LIVE MODE: todo.
    */

    // TODO: when reading from individual file streams we have to lock the individual files.
    // This is important to keep the seeks valid.

    // Loop through all our files and generate their presence.
    CIMGArchiveCompressionHandler *compressHandler = this->m_compressionHandler;

    this->m_virtualFS.ForAllItems(
        [&]( fsActiveEntry *item )
    {
        if ( item->isFile == false )
            return;

        file *theFile = (file*)item;

        // Determine whether we need to compress this file.
        bool requiresCompression = false;

        if ( compressHandler != nullptr )
        {
            // If we have a compression handler, we generarily compress everything.
            requiresCompression = true;
        }

        eFileDataState dataState = theFile->metaData.dataState;

        // Grab the destination handle.
        // We will calculate the blocksize depending on it.
        CFile *destinationHandle = nullptr;

        CFile *srcHandle = nullptr;
        fsOffsetNumber_t srcBegOffset;

        bool wantsToCompress = false;
        bool recalculateSize = false;

        fsOffsetNumber_t srcFileBounds;
        bool givenBounds = false;

        if ( dataState == eFileDataState::PRESENT )
        {
            // We must have a source stream.
            srcHandle = theFile->metaData.GetDataStream();
            srcBegOffset = 0;

            if ( requiresCompression )
            {
                // The stream has to be compressed and the state changed.
                destinationHandle = this->fileMan.AllocateTemporaryDataDestination();

                wantsToCompress = true;
            }

            recalculateSize = true;

            // Even PRESENT files can be compressed already by user-code.
            // So we check for that.
        }
        else if ( dataState == eFileDataState::PRESENT_COMPRESSED )
        {
            // We are already processed due to being possibly dumped from the archives.
            // So we do not want to compress again.
            srcHandle = theFile->metaData.GetDataStream();
            srcBegOffset = 0;

            recalculateSize = true;
        }
        else if ( dataState == eFileDataState::ARCHIVED )
        {
            // We need to dump the file to disk.
            // Archived streams are expected to be compressed already, so that we just dump them.
            srcHandle = this->m_contentFile;
            srcBegOffset = theFile->metaData.GetArchivedOffsetToFile();

            srcFileBounds = theFile->metaData.GetArchivedBlockSize();

            givenBounds = true;

            destinationHandle = this->fileMan.AllocateTemporaryDataDestination();

            // ARCHIVED files keep one data stream during saving phase.
            // Make sure that we cannot have two save processes running at the same time.

            // If we have not successfully compressed, we need to put it into the unpack root at least.
        }
        else
        {
            assert( 0 );
        }

        // Perform a parsing (if required).
        if ( srcHandle && destinationHandle && destinationHandle != srcHandle )
        {
            srcHandle->SeekNative( srcBegOffset, SEEK_SET );

            bool processedParse = false;
            bool hasDistortedSrcSeekPtr = false;

            if ( wantsToCompress )
            {
                assert( givenBounds == false );

                // Determine if we even have to compress.
                bool isAlreadyCompressed = compressHandler->IsStreamCompressed( srcHandle );

                hasDistortedSrcSeekPtr = true;

                if ( isAlreadyCompressed == false )
                {
                    srcHandle->SeekNative( srcBegOffset, SEEK_SET );

                    bool hasSuccessfullyCompressed = compressHandler->Compress( srcHandle, destinationHandle );

                    if ( hasSuccessfullyCompressed )
                    {
                        processedParse = true;
                    }
                    else
                    {
                        // We for now regress into raw storage mode for files that fail compression
                        // for some reason. This is valid form of storage even though ineffective.
                        destinationHandle->SeekNative( 0, SEEK_SET );
                    }
                }
            }

            if ( !processedParse )
            {
                // The distortion is complicated, so we keep track of it.
                if ( hasDistortedSrcSeekPtr )
                {
                    srcHandle->SeekNative( srcBegOffset, SEEK_SET );

                    hasDistortedSrcSeekPtr = false;
                }

                // Just copy over the file.
                if ( givenBounds )
                {
                    FileSystem::StreamCopyCount( *srcHandle, *destinationHandle, srcFileBounds );
                }
                else
                {
                    FileSystem::StreamCopy( *srcHandle, *destinationHandle );
                }
            }

            // Since destinationHandle has to be a new stream, we dont have to trim it off
            // at the end (there cannot be anything past it).
        }

        // Get the file handle to measure real data by.
        if ( recalculateSize )
        {
            CFile *measureHandle = nullptr;

            if ( destinationHandle )
            {
                measureHandle = destinationHandle;
            }
            else if ( srcHandle )
            {
                measureHandle = srcHandle;
            }

            // Get the block count of this file entry.
            unsigned long blockCount = 0;

            if ( measureHandle )
            {
                // Query its size and calculate the block count depending on it.
                fsOffsetNumber_t realFileSize = measureHandle->GetSizeNative();

                blockCount = getDataBlockCount( (std::make_unsigned <fsOffsetNumber_t>::type)realFileSize );
            }

            theFile->metaData.resourceSize = blockCount;
        }

        // Store any destination storage link.
        if ( destinationHandle )
        {
            // We must store the file storage as what it is, a potentially compressed data link.
            theFile->metaData.dataState = eFileDataState::PRESENT_COMPRESSED;
            theFile->metaData.AcquireDataStream( destinationHandle );
        }

        // TODO: do different allocation strategies depending on live-mode or not.

        // We want to allocate some space for this file.
        bool couldAllocateSpace = this->AllocateFileEntry( theFile );

        // TODO: replace this assert with an exception, very important.

        assert( couldAllocateSpace == true );
    });

    // TODO: after write phase turn all files into ARCHIVED state.
}

template <typename charType, typename allocatorType>
static AINLINE void WriteTrimmedBufferString( const eir::String <charType, allocatorType>& ansiName, char *outBuf, size_t outBufSize )
{
    // Create a (trimmed) filename.
    size_t currentNumFilename = ansiName.GetLength();

    size_t actualCopyCount = std::min( outBufSize - 1, currentNumFilename );

    const char *srcBuf = ansiName.GetConstString();

    FSDataUtil::copy_impl( srcBuf, srcBuf + actualCopyCount, outBuf );

    // Zero terminate the name, including the rest.
    memset( outBuf + actualCopyCount, 0, outBufSize - actualCopyCount );
}

void CIMGArchiveTranslator::WriteFileHeaders( CFile *targetStream, directory& baseDir )
{
    // We have to write files in address order, because that is part of the IMG specification.

    eIMGArchiveVersion theVersion = this->m_version;

    // Now write the file headers.
    ForAllAllocatedFiles(
        [&]( file *theFile )
    {
        assert( theFile->metaData.isAllocated == true );
        assert( theFile->metaData.blockOffset == theFile->metaData.allocBlock.slice.GetSliceStartPoint() );

        if ( theVersion == IMG_VERSION_1 )
        {
            resourceFileHeader_ver1 header;

            header.offset = (fsUInt_t)theFile->metaData.blockOffset;
            header.fileDataSize = (fsUInt_t)theFile->metaData.resourceSize;

            WriteTrimmedBufferString( theFile->relPath.convert_ansi <FSObjectHeapAllocator> (), header.name, NUMELMS(header.name) );

            targetStream->WriteStruct( header );
        }
        else if ( theVersion == IMG_VERSION_2 )
        {
            resourceFileHeader_ver2 header;

            header.offset = (fsUInt_t)theFile->metaData.blockOffset;
            header.fileDataSize = (fsUShort_t)theFile->metaData.resourceSize;
            header.expandedSize = 0;   // always zero.

            WriteTrimmedBufferString( theFile->relPath.convert_ansi <FSObjectHeapAllocator> (), header.name, NUMELMS(header.name) );

            targetStream->WriteStruct( header );
        }
        else if ( theVersion == IMG_VERSION_FASTMAN92 )
        {
            // TODO.
            assert( 0 );
        }
        else
        {
            assert( 0 );
        }
    });
}

void CIMGArchiveTranslator::WriteFiles( CFile *targetStream, directory& baseDir )
{
    // Remember that we write files in address order.

    // Now write our files.
    ForAllAllocatedFiles(
        [&]( file *theFile )
    {
        // Seek to the required position.
        {
            fsOffsetNumber_t requiredArchivePos = ( theFile->metaData.blockOffset * IMG_BLOCK_SIZE );

            targetStream->SeekNative( requiredArchivePos, SEEK_SET );
        }

        // Write the data.
        CFile *srcStream = theFile->metaData.GetDataStream();

        // It should never be NULL, but it can be, if something goes horribly wrong.
        assert( srcStream != nullptr );

        if ( srcStream )
        {
            // Make sure to reset the source stream.
            srcStream->SeekNative( 0, SEEK_SET );

            FileSystem::StreamCopy( *srcStream, *targetStream );
        }
    });
}

struct generalHeader
{
    char checksum[4];
    fsUInt_t numberOfEntries;
};

void CIMGArchiveTranslator::Save( void )
{
    // We can only work if the underlying stream is writeable.
    if ( !m_contentFile->IsWriteable() || !m_registryFile->IsWriteable() )
        return;

    bool isLiveMode = this->isLiveMode;

    // If we are not in live mode and there are IMG-space allocated entries,
    // we have to get rid of them because we need to establish an absolutely
    // linear concatenation of blocks without empty blocks in between.
    // Live-mode is expected to deal with validity of allocations on its own.
    if ( !isLiveMode )
    {
        // WARNING: this does only work because when removing allocated entries, their list nodes
        // stay intact as if they were still inside, yielding the correct next-ptr. Very unsafe!
        ForAllAllocatedFiles(
            [&]( file *fileInfo )
        {
            if ( fileInfo->metaData.isAllocated )
            {
                this->DeallocateFileEntry( fileInfo );
            }
        });
    }

    CFile *targetStream = this->m_contentFile;
    CFile *registryStream = this->m_registryFile;

    eIMGArchiveVersion imgVersion = this->m_version;

    // Write things depending on version.
    {
        // Generate header meta information.
        headerGenPresence headerGenMetaData;
        headerGenMetaData.numOfFiles = 0;

        GenerateFileHeaderStructure( headerGenMetaData );

        // Generate the archive structure for files in this archive.
        archiveGenPresence genMetaData;

        // If we are live then the allocation is being handled during runtime.
        if ( isLiveMode == false )
        {
            // Free any active allocation of file headers.
            this->releaseFileHeadersBlock();

            // If we are version two, then we prepend the file headers before the content blocks.
            // Take that into account.
            if ( targetStream == registryStream )   // this is a pretty weak check tbh. but it works for the most part.
            {
                size_t headerSize = 0;

                if (imgVersion == IMG_VERSION_2)
                {
                    // First, there is a general header.
                    headerSize += sizeof( generalHeader );
                }

                // Now come the file entries.
                size_t resourceFileHeaderSize = 0;

                if (imgVersion == IMG_VERSION_1)
                {
                    resourceFileHeaderSize = sizeof(resourceFileHeader_ver1);
                }
                else if (imgVersion == IMG_VERSION_2)
                {
                    resourceFileHeaderSize = sizeof(resourceFileHeader_ver2);
                }

                headerSize += resourceFileHeaderSize * headerGenMetaData.numOfFiles;

                // We have to allocate at position zero.
                {
                    size_t headersBlockCount = getDataBlockCount( headerSize );

                    fileAddrAlloc_t::allocInfo allocInfo;

                    bool couldAllocate = this->fileAddressAlloc.ObtainSpaceAt( 0, headersBlockCount, allocInfo );

                    assert( couldAllocate == true );

                    this->fileAddressAlloc.PutBlock( &this->fileHeaderAllocBlock, allocInfo );

                    this->areFileHeadersAllocated = true;
                }
            }
        }

        GenerateArchiveStructure( genMetaData );

        // Preallocate required file space.
        {
            size_t fileBlockCount = this->fileAddressAlloc.GetSpanSize();

            fsOffsetNumber_t realFileSize = ( (fsOffsetNumber_t)fileBlockCount * IMG_BLOCK_SIZE );

            targetStream->SeekNative( realFileSize, SEEK_SET );
            targetStream->SetSeekEnd();
        }

        // Prepare the streams for writing, by resetting them.
        targetStream->SeekNative( 0, SEEK_SET );

        if ( targetStream != registryStream )
        {
            registryStream->SeekNative( 0, SEEK_SET );
        }

        // We only write a header in version two archives.
        if ( imgVersion == IMG_VERSION_2 )
        {
            // Write the main header of the archive.
            generalHeader mainHeader;
            mainHeader.checksum[0] = 'V';
            mainHeader.checksum[1] = 'E';
            mainHeader.checksum[2] = 'R';
            mainHeader.checksum[3] = '2';
            mainHeader.numberOfEntries = (fsUInt_t)headerGenMetaData.numOfFiles;

            targetStream->WriteStruct( mainHeader );
        }

        // Write all file headers.
        WriteFileHeaders( registryStream, m_virtualFS.GetRootDir() );

        // Now write all the files.
        WriteFiles( targetStream, m_virtualFS.GetRootDir() );
    }

    // Clean up the compressed files, since we do not need them anymore
    // from here on.
    {
        ForAllAllocatedFiles(
            [&]( file *fileInfo )
        {
            eFileDataState dataState = fileInfo->metaData.dataState;

            if ( dataState == eFileDataState::PRESENT_COMPRESSED )
            {
                // Since those entries have been written into the archive and we have
                // no idea whether they are compressed or not we can get rid of the
                // memory streams and read directly from the archive.
                fileInfo->metaData.dataState = eFileDataState::ARCHIVED;
                fileInfo->metaData.releaseDataStream();
            }
        });
    }
}

void CIMGArchiveTranslator::SetCompressionHandler( CIMGArchiveCompressionHandler *handler )
{
    this->m_compressionHandler = handler;
}

bool CIMGArchiveTranslator::ReadArchive( void )
{
    // Load archive.
    bool hasFileCount = false;
    unsigned int fileCount = 0;

    if ( this->m_version == IMG_VERSION_2 )
    {
        imgMainHeaderVer2 mainHeader;

        bool hasMainHeader = this->m_registryFile->ReadStruct( mainHeader );

        if ( !hasMainHeader )
        {
            return false;
        }

        fileCount = mainHeader.fileCount;
        hasFileCount = true;
    }

    bool successLoadingFiles = true;

    // Load every file block registration into memory.
    unsigned int n = 0;

    eIMGArchiveVersion theVersion = this->m_version;

    // Keep track of invalid file entries that need write-phase linear fixups.
    RwList <fileAddrAlloc_t::block_t> fixupList;

    while ( true )
    {
        // Halting condition.
        if ( hasFileCount && n == fileCount )
        {
            break;
        }

        // Read the file header.
        resourceFileHeader_ver1 _ver1Header;
        resourceFileHeader_ver2 _ver2Header;

        bool hasReadEntryHeader = false;

        if ( theVersion == IMG_VERSION_1 )
        {
            hasReadEntryHeader = this->m_registryFile->ReadStruct( _ver1Header );
        }
        else if ( theVersion == IMG_VERSION_2 )
        {
            hasReadEntryHeader = this->m_registryFile->ReadStruct( _ver2Header );
        }

        if ( !hasReadEntryHeader )
        {
            if ( hasFileCount )
            {
                successLoadingFiles = false;
            }

            // If we are version 1, we will probably halt here.
            break;
        }

        // Read from the header.
        unsigned long resourceSize = 0;
        unsigned long resourceOffset = 0;

        const char *namePointer = nullptr;
        size_t maxName = 0;

        if ( theVersion == IMG_VERSION_1 )
        {
            resourceOffset = _ver1Header.offset;
            resourceSize = _ver1Header.fileDataSize;

            namePointer = _ver1Header.name;
            maxName = sizeof( _ver1Header.name );
        }
        else if ( theVersion == IMG_VERSION_2 )
        {
            resourceOffset = _ver2Header.offset;
            resourceSize =
                ( _ver2Header.expandedSize != 0 ) ? ( _ver2Header.expandedSize ) : ( _ver2Header.fileDataSize );

            namePointer = _ver2Header.name;
            maxName = sizeof( _ver2Header.name );
        }

        // Register this into our archive struct.
        normalNodePath normalPath;

        char newPath[ sizeof( _ver1Header.name ) + 1 ];

        if ( namePointer )
        {
            memcpy( newPath, namePointer, maxName );

            newPath[ sizeof( newPath ) - 1 ] = '\0';
        }
        else
        {
            newPath[ 0 ] = '\0';
        }

        bool validPath = this->GetRelativePathNodes( newPath, normalPath );

        if ( validPath && normalPath.isFilePath )
        {
            filePath justFileName = std::move( normalPath.travelNodes.GetBack() );
            normalPath.travelNodes.RemoveFromBack();

            // We do not care about file name conflicts, for now.
            // Otherwise this routine would be terribly slow.
            file *fileEntry = m_virtualFS.ConstructFile( normalPath, justFileName, true, n, true );

            // Try to create this file.
            // This operation will overwrite files if they are found double.
            if ( fileEntry )
            {
                fileEntry->metaData.blockOffset = resourceOffset;
                fileEntry->metaData.resourceSize = resourceSize;

                const size_t maxFinalName = sizeof( fileEntry->metaData.resourceName );

                static_assert( maxFinalName == sizeof( newPath ), "wrong array size for resource name" );

                memcpy( fileEntry->metaData.resourceName, newPath, maxFinalName );
                fileEntry->metaData.resourceName[ maxFinalName - 1 ] = '\0';

                // Allocate an entry for this file.
                // If we happen to collide with an existing entry, we have to
                // solve this problem on next write-phase.
                // Since problematic links can only be introduced during loading
                // from disk, we try to put all the debug code here.
                fileAddrAlloc_t::allocInfo allocInfo;

                bool canPutObject = this->fileAddressAlloc.ObtainSpaceAt( resourceOffset, resourceSize, allocInfo );

                if ( canPutObject )
                {
                    this->fileAddressAlloc.PutBlock( &fileEntry->metaData.allocBlock, allocInfo );

                    // We are allocated.
                    fileEntry->metaData.isAllocated = true;
                }
                else
                {
                    // Remember as erratic.
                    // We have to consider a write-phase fixup.
                    LIST_APPEND( fixupList.root, fileEntry->metaData.allocBlock.node );
                }
            }
        }

        // Increment current iter.
        n++;
    }

    if ( !successLoadingFiles )
    {
        return false;
    }

    // Perform the fixups.
    LIST_FOREACH_BEGIN( fileAddrAlloc_t::block_t, fixupList.root, node )

        // We are a fileMetaData entry too.
        fileMetaData *fileMeta = LIST_GETITEM( fileMetaData, item, allocBlock );

        // Allocate it somewhere in the array.
        // Note that this allocation will most likely be somewhere else than it originally was.
        fileAddrAlloc_t::allocInfo allocInfo;

        bool allocSuccess = this->fileAddressAlloc.FindSpace( fileMeta->resourceSize, allocInfo, 1 );

        if ( allocSuccess )
        {
            this->fileAddressAlloc.PutBlock( &fileMeta->allocBlock, allocInfo );

            fileMeta->isAllocated = true;
        }
        else
        {
            // I do not expect this to fail.
            // If it does tho, then we are kind of screwed; we could try again later.
        }

    LIST_FOREACH_END

    // By now, every block should have, at least, a _promised archive position_.

    return true;
}
