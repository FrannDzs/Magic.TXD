/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.internal.nativeimpl.hxx
*  PURPOSE:     Native implementation utilities to share across files
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_NATIVE_SHARED_IMPLEMENTATION_PRIVATE_
#define _FILESYSTEM_NATIVE_SHARED_IMPLEMENTATION_PRIVATE_

// Sub-modules.
#include "CFileSystem.platform.h"

#include <sdk/Set.h>

enum eNumberConversion
{
    NUMBER_LITTLE_ENDIAN,
    NUMBER_BIG_ENDIAN
};

// Utilities for number splitting.
static inline unsigned int CalculateNumberSplitCount( size_t toBeSplitNumberSize, size_t nativeNumberSize )
{
    return (unsigned int)( toBeSplitNumberSize / nativeNumberSize );
}

template <typename nativeNumberType, typename splitNumberType>
static inline nativeNumberType* GetNumberSector(
            splitNumberType& numberToBeSplit,
            unsigned int index,
            eNumberConversion splitConversion, eNumberConversion nativeConversion )
{
    // Extract the sector out of numberToBeSplit.
    nativeNumberType& nativeNumberPartial = *( (nativeNumberType*)&numberToBeSplit + index );

    return &nativeNumberPartial;
}

template <typename splitNumberType, typename nativeNumberType>
AINLINE void SplitIntoNativeNumbers(
            splitNumberType numberToBeSplit,
            nativeNumberType *nativeNumbers,
            unsigned int maxNativeNumbers, unsigned int& nativeCount,
            eNumberConversion toBeSplitConversion, eNumberConversion nativeConversion )
{
    // todo: add endian-ness support.
    assert( toBeSplitConversion == nativeConversion );

    // We assume a lot of things here.
    // - a binary number system
    // - endian integer system
    // else this routine will produce garbage.

    // On Visual Studio 2008, this routine optimizes down completely into compiler intrinsics.

    const size_t nativeNumberSize = sizeof( nativeNumberType );
    const size_t toBeSplitNumberSize = sizeof( splitNumberType );

    // Calculate the amount of numbers we can fit into the array.
    unsigned int splitNumberCount = std::min( CalculateNumberSplitCount( toBeSplitNumberSize, nativeNumberSize ), maxNativeNumbers );
    unsigned int actualWriteCount = 0;

    for ( unsigned int n = 0; n < splitNumberCount; n++ )
    {
        // Write it into the array.
        nativeNumbers[ actualWriteCount++ ] = *GetNumberSector <nativeNumberType> ( numberToBeSplit, n, toBeSplitConversion, nativeConversion );
    }

    // Notify the runtime about how many numbers we have successfully written.
    nativeCount = actualWriteCount;
}

template <typename splitNumberType, typename nativeNumberType>
AINLINE void ConvertToWideNumber(
            splitNumberType& wideNumberOut,
            nativeNumberType *nativeNumbers, unsigned int numNativeNumbers,
            eNumberConversion wideConversion, eNumberConversion nativeConversion )
{
    // todo: add endian-ness support.
    assert( wideConversion == nativeConversion );

    // we assume the same deals as SplitIntoNativeNumbers.
    // else this routine is garbage.

    // On Visual Studio 2008, this routine optimizes down completely into compiler intrinsics.

    const size_t nativeNumberSize = sizeof( nativeNumberType );
    const size_t toBeSplitNumberSize = sizeof( splitNumberType );

    // Calculate the amount of numbers we need to put together.
    unsigned int splitNumberCount = CalculateNumberSplitCount( toBeSplitNumberSize, nativeNumberSize );

    for ( unsigned int n = 0; n < splitNumberCount; n++ )
    {
        // Write it into the number.
        nativeNumberType numberToWrite = (nativeNumberType)0;

        if ( n < numNativeNumbers )
        {
            numberToWrite = nativeNumbers[ n ];
        }

        *GetNumberSector <nativeNumberType> ( wideNumberOut, n, wideConversion, nativeConversion ) = numberToWrite;
    }
}

#ifdef _WIN32

bool _FileWin32_IsDirectoryAbsolute( const char *pPath );
bool _FileWin32_IsDirectoryAbsolute( const wchar_t *pPath );
bool _FileWin32_DeleteDirectory( const char *path );
bool _FileWin32_DeleteDirectory( const wchar_t *path );
bool _FileWin32_DeleteFile( const char *path );
bool _FileWin32_DeleteFile( const wchar_t *path );
bool _FileWin32_CopyFile( const char *src, const char *dst );
bool _FileWin32_CopyFile( const wchar_t *src, const wchar_t *dst );
bool _FileWin32_RenameFile( const char *src, const char *dst );
bool _FileWin32_RenameFile( const wchar_t *src, const wchar_t *dst );
HANDLE _FileWin32_OpenDirectoryHandle( const filePath& absPath, eDirOpenFlags flags = DIR_FLAG_NONE );
bool _FileWin32_DoesFileExist( const char *src );
bool _FileWin32_DoesFileExist( const wchar_t *src );
bool _FileWin32_GetFileInformation( HANDLE fileHandle, filesysStats& statsOut );
bool _FileWin32_GetFileAttributesByPath( const char *path, filesysAttributes& attribOut );
bool _FileWin32_GetFileAttributesByPath( const wchar_t *path, filesysAttributes& attribOut );
bool _FileWin32_GetFileInformationByPath( const filePath& path, filesysStats& statsOut );
fsOffsetNumber_t _FileWin32_GetFileSize( const char *path );
fsOffsetNumber_t _FileWin32_GetFileSize( const wchar_t *path );

AINLINE filesysAttributes _FileWin32_GetAttributes( DWORD win32Attribs )
{
    filesysAttributes attribOut;

    attribOut.isSystem = ( win32Attribs & FILE_ATTRIBUTE_SYSTEM ) != 0;
    attribOut.isHidden = ( win32Attribs & FILE_ATTRIBUTE_HIDDEN ) != 0;
    attribOut.isTemporary = ( win32Attribs & FILE_ATTRIBUTE_TEMPORARY ) != 0;
    attribOut.isJunctionOrLink = ( win32Attribs & ( FILE_ATTRIBUTE_REPARSE_POINT ) ) != 0;

    bool isDirectory = ( ( win32Attribs & FILE_ATTRIBUTE_DIRECTORY ) != 0 );

    eFilesysItemType itemType;

    if ( isDirectory )
    {
        itemType = eFilesysItemType::DIRECTORY;
    }
    else
    {
        itemType = eFilesysItemType::FILE;
    }
    attribOut.type = itemType;

    return attribOut;
}

// Filesystem item iterator, for cross-platform support.
struct win32_fsitem_iterator
{
    struct info_data
    {
        decltype(WIN32_FIND_DATAW::cFileName) filename;
        bool isDirectory;
        filesysAttributes attribs;
    };

    filePath query;
    HANDLE findHandle;
    bool hasEnded;

    AINLINE win32_fsitem_iterator( const filePath& absDirPath )
    {
        // Create the query string to send to Windows.
        filePath query = absDirPath;
        query += FileSystem::GetAnyWildcardSelector <wchar_t> ();
        query.transform_to <wchar_t> ();

        // Initialize things.
        this->findHandle = INVALID_HANDLE_VALUE;
        this->hasEnded = false;

        // Remember the query string.
        this->query = std::move( query );
    }

private:
    AINLINE void Close( void )
    {
        HANDLE findHandle = this->findHandle;

        if ( findHandle != INVALID_HANDLE_VALUE )
        {
            FindClose( findHandle );

            this->findHandle = INVALID_HANDLE_VALUE;
        }

        this->hasEnded = true;
    }

public:
    AINLINE ~win32_fsitem_iterator( void )
    {
        this->Close();
    }

    AINLINE bool Next( info_data& dataOut )
    {
        if ( this->hasEnded )
            return false;

        HANDLE curFindHandle = this->findHandle;

        WIN32_FIND_DATAW findData;

        if ( curFindHandle == INVALID_HANDLE_VALUE )
        {
            curFindHandle = FindFirstFileW( this->query.w_str(), &findData );

            if ( curFindHandle == INVALID_HANDLE_VALUE )
            {
                return false;
            }

            this->findHandle = curFindHandle;
        }
        else
        {
            BOOL hasNext = FindNextFileW( curFindHandle, &findData );

            if ( hasNext == FALSE )
            {
                goto hasEnded;
            }
        }

        // Create an information structure.
        {
            info_data data;

            // Store all attributes.
            DWORD win32Attribs = findData.dwFileAttributes;
            data.attribs = _FileWin32_GetAttributes( win32Attribs );

            // Output the new info.
            memcpy( data.filename, findData.cFileName, sizeof( data.filename ) );
            data.isDirectory = ( data.attribs.type == eFilesysItemType::DIRECTORY );
            dataOut = std::move( data );
            return true;
        }

    hasEnded:
        this->Close();

        return false;
    }

    AINLINE void Rewind( void )
    {
        this->Close();

        this->hasEnded = false;
    }
};

// File memory-mapping provider.
struct win32_file_mapping_provider : public CFileMappingProvider
{
    inline win32_file_mapping_provider( HANDLE hFile, const filesysAccessFlags& accessRights )
    {
        DWORD protectionFlags = 0;

        // On Win32 there is no way to open a file mapping for just writing.
        bool allowRead = accessRights.allowRead;
        bool allowWrite = accessRights.allowWrite;

        if ( allowRead && allowWrite )
        {
            protectionFlags |= PAGE_READWRITE;
        }
        else if ( allowRead )
        {
            protectionFlags |= PAGE_READONLY;
        }
        else if ( allowWrite )
        {
            // Kind of hacky.
            protectionFlags |= PAGE_READWRITE;
        }

        HANDLE current_process = GetCurrentProcess();

        HANDLE fileHandle;

        BOOL duplicateSuccess = DuplicateHandle(
            current_process, hFile,
            current_process, &fileHandle,
            0, FALSE, DUPLICATE_SAME_ACCESS
        );

        if ( duplicateSuccess == FALSE )
        {
            throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::RESOURCE_UNAVAILABLE );
        }

        try
        {
            HANDLE mappingProv = CreateFileMappingW( fileHandle, nullptr, protectionFlags, 0, 0, nullptr );

            if ( mappingProv == NULL )
            {
                DWORD lasterr = GetLastError();

                if ( lasterr == ERROR_FILE_INVALID )
                {
                    throw FileSystem::TerminatedObjectException();
                }

                throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INVALID_PARAM );
            }

            this->fileHandle = fileHandle;
            this->nativeMappingProv = mappingProv;
        }
        catch( ... )
        {
            CloseHandle( fileHandle );

            throw;
        }
    }

    inline ~win32_file_mapping_provider( void )
    {
        for ( void *ptr : this->setOfMappings )
        {
            // Could fail if drive is no available anymore.
            UnmapViewOfFile( ptr );
        }

        CloseHandle( this->nativeMappingProv );
        CloseHandle( this->fileHandle );
    }

    void* MapFileRegion( fsOffsetNumber_t fileOff, size_t dataLen, const filemapAccessMode& rights ) override
    {
        DWORD filemapProtectionFlags = 0;

        if ( rights.allowRead )
        {
            filemapProtectionFlags |= FILE_MAP_READ;
        }
        if ( rights.allowWrite )
        {
            filemapProtectionFlags |= FILE_MAP_WRITE;
        }
        if ( rights.makePrivate )
        {
            filemapProtectionFlags |= FILE_MAP_COPY;
        }

        DWORD fileOffWords[2];
        unsigned int num_fileOffWords;

        SplitIntoNativeNumbers( fileOff, fileOffWords, countof(fileOffWords), num_fileOffWords, eNumberConversion::NUMBER_LITTLE_ENDIAN, eNumberConversion::NUMBER_LITTLE_ENDIAN );

        if ( num_fileOffWords < 2 )
        {
            fileOffWords[1] = 0;
        }

        void *dataptr = MapViewOfFile( this->nativeMappingProv, filemapProtectionFlags, fileOffWords[1], fileOffWords[0], dataLen );

        if ( dataptr == nullptr )
        {
            DWORD lasterr = GetLastError();

            if ( lasterr == ERROR_FILE_INVALID )
            {
                throw FileSystem::TerminatedObjectException();
            }

            return nullptr;
        }

        try
        {
            this->setOfMappings.Insert( dataptr );
        }
        catch( ... )
        {
            // Could fail if the drive is not available anymore.
            UnmapViewOfFile( dataptr );

            throw;
        }

        return dataptr;
    }

    bool UnMapFileRegion( void *dataptr ) override
    {
        auto *mapNode = this->setOfMappings.Find( dataptr );

        if ( mapNode == nullptr )
        {
            return false;
        }

        BOOL couldUnmap = UnmapViewOfFile( dataptr );

        if ( couldUnmap == FALSE )
        {
            DWORD lasterr = GetLastError();

            if ( lasterr == ERROR_FILE_INVALID )
            {
                throw FileSystem::TerminatedObjectException();
            }

            return false;
        }

        this->setOfMappings.RemoveNode( mapNode );

        return true;
    }

private:
    HANDLE fileHandle;
    HANDLE nativeMappingProv;
    eir::Set <void*, FSObjectHeapAllocator> setOfMappings;
};

#elif defined(__linux__)

#include <dirent.h>

bool _FileLinux_IsDirectoryAbsolute( const char *path );
bool _FileLinux_DeleteDirectory( const char *path );
bool _FileLinux_DeleteFile( const char *path );
bool _FileLinux_CopyFile( const char *src, const char *dst );
bool _FileLinux_RenameFile( const char *src, const char *dst );
bool _FileLinux_DoesFileExist( const char *src );
bool _FileLinux_GetFileAttributesByPath( const char *src, filesysAttributes& attribsOut );
int _FileLinux_StatFile( const char *src, filesysStats& statsOut );
fsOffsetNumber_t _FileLinux_GetFileSize( const char *src );

struct linux_fsitem_iterator
{
    struct info_data
    {
        decltype(dirent::d_name) filename;
        bool isDirectory;
        filesysAttributes attribs;
    };

    filePath absDirPath;
    DIR *iter;

    AINLINE linux_fsitem_iterator( filePath absDirPath ) : absDirPath( std::move( absDirPath ) )
    {
        auto ansiPath = this->absDirPath.convert_ansi <FSObjectHeapAllocator> ();

        this->iter = opendir( ansiPath.GetConstString() );
    }
    AINLINE linux_fsitem_iterator( const linux_fsitem_iterator& ) = delete;

    AINLINE ~linux_fsitem_iterator( void )
    {
        if ( DIR *iter = this->iter )
        {
            closedir( iter );
        }
    }

    AINLINE linux_fsitem_iterator& operator = ( const linux_fsitem_iterator& ) = delete;

    AINLINE bool Next( info_data& dataOut )
    {
        DIR *iter = this->iter;

        if ( iter == nullptr )
        {
            return false;
        }

    tryNextItem:
        struct dirent *entry = readdir( iter );

        if ( !entry )
            return false;

        filePath path = this->absDirPath;
        path += entry->d_name;
        path.transform_to <char> ();

        struct stat entry_stats;

        if ( stat( path.c_str(), &entry_stats ) == 0 )
        {
            info_data data;

            FSDataUtil::copy_impl( entry->d_name, entry->d_name + sizeof( data.filename ), data.filename );

            auto st_mode = entry_stats.st_mode;

            bool isDirectory = S_ISDIR( entry_stats.st_mode );

            // Fill out attributes.
            eFilesysItemType itemType;

            if ( isDirectory )
            {
                itemType = eFilesysItemType::DIRECTORY;
            }
            else if ( S_ISREG( st_mode ) )
            {
                itemType = eFilesysItemType::FILE;
            }
            else
            {
                itemType = eFilesysItemType::UNKNOWN;
            }
            data.attribs.type = itemType;
            // TODO: actually fill those our truthfully.
            data.attribs.isSystem = false;
            data.attribs.isHidden = false;
            data.attribs.isTemporary = false;
            data.attribs.isJunctionOrLink = false;

            data.isDirectory = isDirectory;

            dataOut = std::move( data );
            return true;
        }

        // Failed to do something, try next instead.
        goto tryNextItem;
    }

    AINLINE void Rewind( void )
    {
        if ( DIR *iter = this->iter )
        {
            rewinddir( iter );
        }
    }
};

#include <unistd.h>
#include <sys/mman.h>

struct linux_file_mapping_provider : public CFileMappingProvider
{
    inline linux_file_mapping_provider( int file_desc )
    {
        int fd = dup( file_desc );

        if ( fd == -1 )
        {
            throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::RESOURCE_UNAVAILABLE );
        }

        this->fd = fd;
    }

    inline ~linux_file_mapping_provider( void )
    {
        for ( const map_registration& reg : this->setOfMappings )
        {
            munmap( reg.dataptr, reg.mappingSize );
        }

        close( this->fd );
    }

    void* MapFileRegion( fsOffsetNumber_t fileOff, size_t regLength, const filemapAccessMode& rights ) override
    {
        int protFlags = 0;

        if ( rights.allowRead )
        {
            protFlags |= PROT_READ;
        }
        if ( rights.allowWrite )
        {
            protFlags |= PROT_WRITE;
        }

        int genFlags = 0;

        if ( rights.makePrivate )
        {
            genFlags |= MAP_PRIVATE;
        }
        else
        {
            genFlags |= MAP_SHARED;
        }

        void *dataptr = mmap( nullptr, regLength, protFlags, genFlags, this->fd, (off_t)fileOff );

        if ( dataptr == MAP_FAILED )
        {
            return nullptr;
        }

        try
        {
            map_registration item;
            item.dataptr = dataptr;
            item.mappingSize = regLength;

            this->setOfMappings.Insert( std::move( item ) );
        }
        catch( ... )
        {
            munmap( dataptr, regLength );

            throw;
        }

        return dataptr;
    }

    bool UnMapFileRegion( void *dataptr ) override
    {
        auto mapNode = this->setOfMappings.Find( dataptr );

        if ( mapNode == nullptr )
        {
            return false;
        }

        const map_registration& item = mapNode->GetValue();

        int unmap_result = munmap( item.dataptr, item.mappingSize );

        if ( unmap_result != 0 )
        {
            return false;
        }

        this->setOfMappings.RemoveNode( mapNode );

        return true;
    }

private:
    int fd;

    struct map_registration
    {
        void *dataptr;
        size_t mappingSize;

        AINLINE static eir::eCompResult compare_values( const map_registration& left, const map_registration& right ) noexcept
        {
            return eir::DefaultValueCompare( left.dataptr, right.dataptr );
        }

        AINLINE static eir::eCompResult compare_values( const map_registration& left, const void *right ) noexcept
        {
            return eir::DefaultValueCompare( left.dataptr, right );
        }

        AINLINE static eir::eCompResult compare_values( const void *left, const map_registration& right ) noexcept
        {
            return eir::DefaultValueCompare( left, right.dataptr );
        }
    };

    eir::Set <map_registration, FSObjectHeapAllocator, map_registration> setOfMappings;
};

#endif

#endif //_FILESYSTEM_NATIVE_SHARED_IMPLEMENTATION_PRIVATE_
