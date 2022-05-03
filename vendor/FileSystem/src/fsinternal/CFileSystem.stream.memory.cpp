/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.memory.cpp
*  PURPOSE:     Memory-based read/write stream
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

// Sub modules.
#include "CFileSystem.internal.h"
#include "CFileSystem.platform.h"
#include "CFileSystem.stream.memory.h"

#include <sdk/PluginHelpers.h>

using namespace FileSystem;

CMemoryMappedFile::CMemoryMappedFile( CFileSystemNative *fileSys ) : manExt( fileSys ), memStream( nullptr, 0, manExt )
{
    // Initialize time variables.
    time_t curtime = time( nullptr );

    this->meta_atime = curtime;
    this->meta_mtime = curtime;
    this->meta_ctime = curtime;
}

CMemoryMappedFile::~CMemoryMappedFile( void )
{
    // We have defined the memoryBufferStream to be freeing its own buffer on destroy.
    return;
}

size_t CMemoryMappedFile::Read( void *buffer, size_t readCount )
{
    if ( readCount == 0 )
        return 0;

    return memStream.Read( buffer, readCount );
}

size_t CMemoryMappedFile::Write( const void *buffer, size_t writeCount )
{
    if ( writeCount == 0 )
        return 0;

    size_t writeByteCount = memStream.Write( buffer, writeCount );

    // We have modified the file, so remember that.
    this->meta_mtime = time( nullptr );

    return writeByteCount;
}

int CMemoryMappedFile::Seek( long offset, int iType )
{
    long fileOffset = 0;

    switch( iType )
    {
    case SEEK_SET:
        fileOffset = 0;
        break;
    case SEEK_CUR:
        fileOffset = (long)this->memStream.Tell();
        break;
    case SEEK_END:
        fileOffset = (long)this->memStream.Size();
        break;
    default:
        return -1;
    }

    long absoluteOffset = ( fileOffset + offset );

    this->memStream.Seek( absoluteOffset );

    return 0;
}

int CMemoryMappedFile::SeekNative( fsOffsetNumber_t offset, int iType )
{
    fsOffsetNumber_t fileOffset = 0;

    switch( iType )
    {
    case SEEK_SET:
        fileOffset = 0;
        break;
    case SEEK_CUR:
        fileOffset = this->memStream.Tell();
        break;
    case SEEK_END:
        fileOffset = this->memStream.Size();
        break;
    default:
        return -1;
    }

    fsOffsetNumber_t absoluteOffset = ( fileOffset + offset );

    this->memStream.Seek( absoluteOffset );

    return 0;
}

long CMemoryMappedFile::Tell( void ) const
{
    return (long)memStream.Tell();
}

fsOffsetNumber_t CMemoryMappedFile::TellNative( void ) const
{
    return memStream.Tell();
}

bool CMemoryMappedFile::IsEOF( void ) const
{
    return ( memStream.Tell() >= memStream.Size() );
}

bool CMemoryMappedFile::QueryStats( filesysStats& statsOut ) const
{
    statsOut.atime = this->meta_atime;
    statsOut.ctime = this->meta_ctime;
    statsOut.mtime = this->meta_mtime;
    statsOut.attribs.type = eFilesysItemType::FILE;
    statsOut.attribs.isTemporary = true;
    statsOut.attribs.isHidden = true;
    return true;
}

void CMemoryMappedFile::SetFileTimes( time_t atime, time_t ctime, time_t mtime )
{
    // Store time-based things.
    this->meta_atime = atime;
    this->meta_mtime = mtime;
    this->meta_ctime = ctime;
}

void CMemoryMappedFile::SetSeekEnd( void )
{
    // Truncate the memory file at the current seek pointer.
    this->memStream.Truncate( this->memStream.Tell() );

    // Modification of file.
    this->meta_mtime = time( nullptr );
}

size_t CMemoryMappedFile::GetSize( void ) const
{
    return (size_t)memStream.Size();
}

fsOffsetNumber_t CMemoryMappedFile::GetSizeNative( void ) const
{
    return memStream.Size();
}

void CMemoryMappedFile::Flush( void )
{
    // Nothing to do, since we have no backing storage.
    return;
}

CFileMappingProvider* CMemoryMappedFile::CreateMapping( void )
{
    // TODO: maybe support this.
    return nullptr;
}

static const filePath _memFilePath( "<memory>" );

filePath CMemoryMappedFile::GetPath( void ) const
{
    return _memFilePath;
}

bool CMemoryMappedFile::IsReadable( void ) const noexcept
{
    // We can always read memory streams.
    return true;
}

bool CMemoryMappedFile::IsWriteable( void ) const noexcept
{
    // We can always write memory streams.
    return true;
}

const void* CMemoryMappedFile::GetBuffer( size_t& sizeOut ) const
{
    memoryMappedNativePageAllocator::pageHandle *curHandle = this->manExt.currentMemory;

    if ( curHandle == nullptr )
        return nullptr;

    void *memPtr = curHandle->GetTargetPointer();

    sizeOut = this->manExt.currentMemorySize;
    return memPtr;
}

// Memory management extension for FileSystem to properly dispatch memory between memory-mapped files.
struct mappedMemoryDispatcherEnv
{
    inline mappedMemoryDispatcherEnv( CFileSystemNative *fileSys )
#ifdef FILESYS_MULTI_THREADING
        : lock_nativeAlloc( fileSys->nativeMan )
#endif //FILESYS_MULTI_THREADING
    {
        return;
    }

    inline void Initialize( CFileSystemNative *fileSys )
    {
        return;
    }

    inline void Shutdown( CFileSystemNative *fileSys )
    {
        return;
    }

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::natOptRWLock lock_nativeAlloc;
#endif //FILESYS_MULTI_THREADING

    NativePageAllocator nativeAlloc;
};

static PluginDependantStructRegister <mappedMemoryDispatcherEnv, fileSystemFactory_t> mappedMemoryDispatcherRegister;

CMemoryMappedFile::streamManagerExtension::streamManagerExtension( CFileSystemNative *fileSys )
{
    this->manager = fileSys;

    // We start out with no valid memory.
    this->currentMemory = nullptr;
    this->currentMemorySize = 0;
}

CMemoryMappedFile::streamManagerExtension::~streamManagerExtension( void )
{
    CFileSystemNative *nativeFSMan = (CFileSystemNative*)this->manager;

    mappedMemoryDispatcherEnv *memoryEnv = mappedMemoryDispatcherRegister.GetPluginStruct( nativeFSMan );

    if ( !memoryEnv )
        return;

    // Clean up memory.
    if ( NativePageAllocator::pageHandle *currentMemory = this->currentMemory )
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteWriteContextSafe <> ctxFreeMemory( memoryEnv->lock_nativeAlloc );
#endif //FILESYS_MULTI_THREADING

        memoryEnv->nativeAlloc.Free( currentMemory );

        this->currentMemory = nullptr;
    }
}

// Memory management of memory mapped files.
void CMemoryMappedFile::streamManagerExtension::EstablishBufferView( void*& bufPtr, fsOffsetNumber_t& sizePtr, fsOffsetNumber_t newSize )
{
    // We manage memory in native pages (win32 only?).

    if ( (std::make_unsigned <decltype(newSize)>::type)newSize > std::numeric_limits <size_t>::max() || newSize < 0 )
        return; // TODO: maybe throw exception?

    mappedMemoryDispatcherEnv *memoryEnv = mappedMemoryDispatcherRegister.GetPluginStruct( this->manager );

    if ( !memoryEnv )
        return; // cannot do anything if we dont have the memory environment.

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::CReadWriteWriteContextSafe <> ctxReallocBufferView( memoryEnv->lock_nativeAlloc );
#endif //FILESYS_MULTI_THREADING

    size_t realSizeNum = (size_t)newSize;

    if ( realSizeNum == 0 )
    {
        // If we have some kind of memory, we release it.
        if ( NativePageAllocator::pageHandle *prevHandle = this->currentMemory )
        {
            memoryEnv->nativeAlloc.Free( prevHandle );

            this->currentMemory = nullptr;
            this->currentMemorySize = 0;
        }

        bufPtr = nullptr;
        sizePtr = 0;
    }
    else
    {
        // We need to provide actual memory.
        // Provide the simplest implementation.
        NativePageAllocator::pageHandle *currentHandle = this->currentMemory;

        if ( !currentHandle )
        {
            currentHandle = memoryEnv->nativeAlloc.Allocate( nullptr, realSizeNum );

            if ( !currentHandle )
            {
                // Meow :(
                throw filesystem_exception( eGenExceptCode::MEMORY_INSUFFICIENT );
            }

            this->currentMemory = currentHandle;
        }
        else
        {
            // Need to properly reallocate memory.
            bool resizeSuccess = memoryEnv->nativeAlloc.SetHandleSize( currentHandle, realSizeNum );

            if ( !resizeSuccess )
            {
                // Need to create a new handle :(
                NativePageAllocator::pageHandle *newHandle = memoryEnv->nativeAlloc.Allocate( nullptr, realSizeNum );

                if ( !newHandle )
                {
                    // What do?????? :(((((((((((((((((((((((((
                    throw filesystem_exception( eGenExceptCode::MEMORY_INSUFFICIENT );
                }

                // This code block cannot throw exceptions.

                void *newMem = newHandle->GetTargetPointer();

                const void *oldMem = currentHandle->GetTargetPointer();

                const size_t oldMemSize = this->currentMemorySize;

                memcpy( newMem, oldMem, oldMemSize );

                memoryEnv->nativeAlloc.Free( currentHandle );

                currentHandle = newHandle;

                this->currentMemory = currentHandle;
            }
        }

        this->currentMemorySize = realSizeNum;

        bufPtr = currentHandle->GetTargetPointer();
        sizePtr = realSizeNum;
    }
}

// Official API.
CFile* CFileSystem::CreateMemoryFile( void )
{
    CFileSystemNative *fileSys = (CFileSystemNative*)this;

    CMemoryMappedFile *memFile = new CMemoryMappedFile( fileSys );

    return memFile;
}

const void* CFileSystem::GetMemoryFileBuffer( CFile *stream, size_t& sizeOut ) const
{
    CMemoryMappedFile *memFile = dynamic_cast <CMemoryMappedFile*> ( stream );

    if ( !memFile )
        return nullptr;

    return memFile->GetBuffer( sizeOut );
}

void registerFileSystemMemoryMappedStreams( void )
{
    mappedMemoryDispatcherRegister.RegisterPlugin( _fileSysFactory );
}

void unregisterFileSystemMemoryMappedStreams( void )
{
    mappedMemoryDispatcherRegister.UnregisterPlugin();
}
