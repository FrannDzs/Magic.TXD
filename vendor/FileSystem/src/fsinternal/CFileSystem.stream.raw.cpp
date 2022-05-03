/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.raw.cpp
*  PURPOSE:     Raw OS filesystem file link
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/
#include <StdInc.h>

// Include internal header.
#include "CFileSystem.internal.h"

// Sub modules.
#include "CFileSystem.platform.h"
#include "CFileSystem.stream.raw.h"

#include "CFileSystem.internal.nativeimpl.hxx"

using namespace FileSystem;

/*===================================================
    CRawFile

    This class represents a file on the system.
    As long as it is present, the file is opened.

    WARNING: using this class directly is discouraged,
        as it uses direct methods of writing into
        hardware. wrap it with CBufferedStreamWrap instead!

    fixme: Port to mac
===================================================*/

CRawFile::CRawFile( filePath absFilePath, filesysAccessFlags flags ) : m_access( std::move( flags ) ), m_path( std::move( absFilePath ) )
{
    return;
}

CRawFile::~CRawFile( void )
{
#ifdef _WIN32
    CloseHandle( m_file );
#elif defined(__linux__)
    close( m_fileIndex );
#else
#error no OS file destructor implementation
#endif //OS DEPENDANT CODE
}

size_t CRawFile::Read( void *pBuffer, size_t readCount )
{
#ifdef _WIN32
    DWORD dwBytesRead;

    if (readCount == 0)
        return 0;

    DWORD win32ReadCount = TRANSFORM_INT_CLAMP <DWORD> ( readCount );

    BOOL readComplete = ReadFile( m_file, pBuffer, win32ReadCount, &dwBytesRead, nullptr );

    if ( readComplete == FALSE )
    {
        DWORD lasterr = GetLastError();

        if ( lasterr == ERROR_FILE_INVALID )
        {
            throw TerminatedObjectException();
        }

        return 0;
    }

    return dwBytesRead;
#elif defined(__linux__)
    int linuxFD = this->m_fileIndex;

    ssize_t actualReadCount = read( linuxFD, pBuffer, readCount );

    if ( actualReadCount < 0 )
    {
        int error_code = errno;

        if ( error_code == EINVAL )
        {
            throw TerminatedObjectException();
        }

        return 0;
    }

    return (size_t)actualReadCount;
#else
#error no OS file read implementation
#endif //OS DEPENDANT CODE
}

size_t CRawFile::Write( const void *pBuffer, size_t writeCount )
{
#ifdef _WIN32
    DWORD dwBytesWritten;

    if (writeCount == 0)
        return 0;

    DWORD win32WriteCount = TRANSFORM_INT_CLAMP <DWORD> ( writeCount );

    BOOL writeComplete = WriteFile( m_file, pBuffer, win32WriteCount, &dwBytesWritten, nullptr );

    if ( writeComplete == FALSE )
    {
        DWORD lasterr = GetLastError();

        if ( lasterr == ERROR_FILE_INVALID )
        {
            throw TerminatedObjectException();
        }

        return 0;
    }

    return dwBytesWritten;
#elif defined(__linux__)
    ssize_t actualWriteCount = write( this->m_fileIndex, pBuffer, writeCount );

    if ( actualWriteCount < 0 )
    {
        int error_code = errno;

        if ( error_code == EINVAL )
        {
            throw TerminatedObjectException();
        }

        return 0;
    }

    return (size_t)actualWriteCount;
#else
#error no OS file write implementation
#endif //OS DEPENDANT CODE
}

int CRawFile::Seek( long iOffset, int iType )
{
#ifdef _WIN32
    SetLastError( NO_ERROR );

    DWORD newOff = SetFilePointer(m_file, iOffset, nullptr, iType);
    DWORD lasterr = GetLastError();

    if ( newOff == INVALID_SET_FILE_POINTER )
    {
        return -1;
    }

    // Undocumented, but does make sense.
    // The Win32 documentation does state that you do not have to check error value.
    // I guess somebody forgot about this error case?
    if ( lasterr == ERROR_FILE_INVALID )
    {
        throw TerminatedObjectException();
    }

    return 0;
#elif defined(__linux__)
    off_t new_off = lseek( m_fileIndex, iOffset, iType );

    return ( new_off != (off_t)-1 ? 0 : -1 );
#else
#error no OS file seek implementation
#endif //OS DEPENDANT CODE
}

int CRawFile::SeekNative( fsOffsetNumber_t iOffset, int iType )
{
#ifdef _WIN32
    // Split our offset into two DWORDs.
    LONG numberParts[ 2 ];
    unsigned int splitCount = 0;

    SplitIntoNativeNumbers( iOffset, numberParts, NUMELMS(numberParts), splitCount, NUMBER_LITTLE_ENDIAN, NUMBER_LITTLE_ENDIAN );

    // Tell the OS.
    // Using the preferred method.
    DWORD resultVal = INVALID_SET_FILE_POINTER;

    SetLastError( NO_ERROR );

    if ( splitCount == 1 )
    {
        resultVal = SetFilePointer( this->m_file, numberParts[0], nullptr, iType );
    }
    else if ( splitCount >= 2 )
    {
        resultVal = SetFilePointer( this->m_file, numberParts[0], &numberParts[1], iType );
    }

    DWORD lasterr = GetLastError();

    if ( lasterr == ERROR_FILE_INVALID )
    {
        throw TerminatedObjectException();
    }

    if ( resultVal == INVALID_SET_FILE_POINTER )
    {
        return -1;
    }

    return 0;
#elif defined(__linux__)
    off64_t new_off = lseek64( m_fileIndex, (off64_t)iOffset, iType );

    return ( new_off != (off64_t)-1 ? 0 : -1 );
#else
#error no OS file seek native implementation
#endif //OS DEPENDANT CODE
}

long CRawFile::Tell( void ) const
{
#ifdef _WIN32
    LARGE_INTEGER posToMoveTo;
    posToMoveTo.LowPart = 0;
    posToMoveTo.HighPart = 0;

    LARGE_INTEGER currentPos;

    BOOL success = SetFilePointerEx( this->m_file, posToMoveTo, &currentPos, FILE_CURRENT );

    if ( success == FALSE )
    {
        DWORD lasterr = GetLastError();

        if ( lasterr == ERROR_FILE_INVALID )
        {
            throw TerminatedObjectException();
        }

        return -1;
    }

    return (long)( currentPos.LowPart );
#elif defined(__linux__)
    return (long)lseek( this->m_fileIndex, 0, SEEK_CUR );
#else
#error no OS file tell implementation
#endif //OS DEPENDANT CODE
}

fsOffsetNumber_t CRawFile::TellNative( void ) const
{
#ifdef _WIN32
    LARGE_INTEGER posToMoveTo;
    posToMoveTo.LowPart = 0;
    posToMoveTo.HighPart = 0;

    union
    {
        LARGE_INTEGER currentPos;
        DWORD currentPos_split[ sizeof( LARGE_INTEGER ) / sizeof( DWORD ) ];
    };

    BOOL success = SetFilePointerEx( this->m_file, posToMoveTo, &currentPos, FILE_CURRENT );

    if ( success == FALSE )
    {
        DWORD lasterr = GetLastError();

        if ( lasterr == ERROR_FILE_INVALID )
        {
            throw TerminatedObjectException();
        }

        return (fsOffsetNumber_t)0;
    }

    // Create a FileSystem number.
    fsOffsetNumber_t resultNumber = (fsOffsetNumber_t)0;

    ConvertToWideNumber( resultNumber, &currentPos_split[0], NUMELMS(currentPos_split), NUMBER_LITTLE_ENDIAN, NUMBER_LITTLE_ENDIAN );

    return resultNumber;
#elif defined(__linux__)
    return (fsOffsetNumber_t)lseek64( this->m_fileIndex, 0, SEEK_CUR );
#else
#error no OS file tell native implementation
#endif //OS DEPENDANT CODE
}

bool CRawFile::IsEOF( void ) const
{
#if defined(_WIN32) || defined(__linux__)
    // Check that the current file seek is beyond or equal the maximum size.
    return this->TellNative() >= this->GetSizeNative();
#else
#error no OS file end-of implementation
#endif //OS DEPENDANT CODE
}

bool CRawFile::QueryStats( filesysStats& statsOut ) const
{
#ifdef _WIN32
    return _FileWin32_GetFileInformation( m_file, statsOut );
#elif defined(__linux__)
    struct stat linux_stats;

    if ( fstat( this->m_fileIndex, &linux_stats ) != 0 )
    {
        int error_code = errno;

        if ( error_code == EINVAL )
        {
            throw TerminatedObjectException();
        }

        return false;
    }

    statsOut.atime = linux_stats.st_atime;
    statsOut.ctime = linux_stats.st_ctime;
    statsOut.mtime = linux_stats.st_mtime;
    statsOut.attribs.type = eFilesysItemType::FILE;
    // TODO: improve the returned attributes by more information.
    return true;
#else
#error no OS file stat implementation
#endif //OS DEPENDANT CODE
}

#ifdef _WIN32
inline static void TimetToFileTime( time_t t, LPFILETIME pft )
{
    LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
    pft->dwLowDateTime = (DWORD) ll;
    pft->dwHighDateTime = ll >>32;
}
#endif //_WIN32

void CRawFile::SetFileTimes( time_t atime, time_t ctime, time_t mtime )
{
#ifdef _WIN32
    FILETIME win32_ctime;
    FILETIME win32_atime;
    FILETIME win32_mtime;

    TimetToFileTime( ctime, &win32_ctime );
    TimetToFileTime( atime, &win32_atime );
    TimetToFileTime( mtime, &win32_mtime );

    BOOL success = SetFileTime( m_file, &win32_ctime, &win32_atime, &win32_mtime );

    if ( success == FALSE )
    {
        DWORD lasterr = GetLastError();

        if ( lasterr == ERROR_FILE_INVALID )
        {
            throw TerminatedObjectException();
        }

        throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
    }
#elif defined(__linux__)
    struct utimbuf timeBuf;
    timeBuf.actime = atime;
    timeBuf.modtime = mtime;

    auto ansiFilePath = this->m_path.convert_ansi <FSObjectHeapAllocator> ();

    int succ_code = utime( ansiFilePath.GetConstString(), &timeBuf );

    if ( succ_code == -1 )
    {
        int error_code = errno;

        if ( error_code == ENOENT )
        {
            throw TerminatedObjectException();
        }
        if ( error_code == EACCES )
        {
            // Due to a race condition in the Linux kernel we might get this error aswell if a file link has been deleted
            // just prior to calling this function, by destructive unmount. We just treat it as terminated I guess.
            // We did have access to the file when we opened it so wtf, yes?
            throw TerminatedObjectException();
        }
    }
#else
#error no OS file push stat implementation
#endif //OS DEPENDANT CODE
}

void CRawFile::SetSeekEnd( void )
{
#ifdef _WIN32
    BOOL success = SetEndOfFile( m_file );

    if ( success == FALSE )
    {
        DWORD lasterr = GetLastError();

        if ( lasterr == ERROR_FILE_INVALID )
        {
            throw TerminatedObjectException();
        }

        throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
    }
#elif defined(__linux__)
    int fd = this->m_fileIndex;

    off64_t curseek = lseek64( fd, 0, SEEK_CUR );

    int success = ftruncate64( fd, curseek );

    if ( success != 0 )
    {
        int error_code = errno;

        if ( error_code == EINVAL )
        {
            throw TerminatedObjectException();
        }

        throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
    }
#else
#error no OS file set seek end implementation
#endif //OS DEPENDANT CODE
}

size_t CRawFile::GetSize( void ) const
{
#ifdef _WIN32
    DWORD resFileSize = GetFileSize( m_file, nullptr );

    if ( resFileSize == INVALID_FILE_SIZE )
    {
        DWORD lasterr = GetLastError();

        if ( lasterr == ERROR_FILE_INVALID )
        {
            throw TerminatedObjectException();
        }
    }

    return (size_t)resFileSize;
#elif defined(__linux__)
    struct stat fileInfo;

    int success = fstat( this->m_fileIndex, &fileInfo );

    if ( success != 0 )
    {
        int error_code = errno;

        if ( error_code == EINVAL )
        {
            throw TerminatedObjectException();
        }

        return 0;
    }

    return fileInfo.st_size;
#else
#error no OS file get size implementation
#endif //OS DEPENDANT CODE
}

fsOffsetNumber_t CRawFile::GetSizeNative( void ) const
{
#ifdef _WIN32
    union
    {
        LARGE_INTEGER fileSizeOut;
        DWORD fileSizeOut_split[ sizeof( LARGE_INTEGER ) / sizeof( DWORD ) ];
    };

    BOOL success = GetFileSizeEx( this->m_file, &fileSizeOut );

    if ( success == FALSE )
    {
        DWORD lasterr = GetLastError();

        if ( lasterr == ERROR_FILE_INVALID )
        {
            throw TerminatedObjectException();
        }

        return (fsOffsetNumber_t)0;
    }

    // Convert to a FileSystem native number.
    fsOffsetNumber_t bigFileSizeNumber = (fsOffsetNumber_t)0;

    ConvertToWideNumber( bigFileSizeNumber, &fileSizeOut_split[0], NUMELMS(fileSizeOut_split), NUMBER_LITTLE_ENDIAN, NUMBER_LITTLE_ENDIAN );

    return bigFileSizeNumber;
#elif defined(__linux__)
    struct stat64 large_stat;

    int success = fstat64( this->m_fileIndex, &large_stat );

    if ( success != 0 )
    {
        int error_code = errno;

        if ( error_code == EINVAL )
        {
            throw TerminatedObjectException();
        }

        return 0;
    }

    return large_stat.st_size;
#else
#error no OS file get size native implementation
#endif //OS DEPENDANT CODE
}

void CRawFile::Flush( void )
{
#ifdef _WIN32
    BOOL success = FlushFileBuffers( m_file );

    if ( success == FALSE )
    {
        DWORD lasterr = GetLastError();

        if ( lasterr == ERROR_FILE_INVALID )
        {
            throw TerminatedObjectException();
        }
    }
#elif defined(__linux__)
    int succ = fsync( this->m_fileIndex );

    if ( succ == -1 )
    {
        int error_code = errno;

        if ( error_code == EINVAL || error_code == EIO )
        {
            throw TerminatedObjectException();
        }
    }
#else
#error no OS file flush implementation
#endif //OS DEPENDANT CODE
}

CFileMappingProvider* CRawFile::CreateMapping( void )
{
#ifdef _WIN32
    return new win32_file_mapping_provider( this->m_file, this->m_access );
#elif defined(__linux__)
    return new linux_file_mapping_provider( this->m_fileIndex );
#else
    return nullptr;
#endif //OS DEPENDANT CODE
}

filePath CRawFile::GetPath( void ) const
{
    return m_path;
}

bool CRawFile::IsReadable( void ) const noexcept
{
    return m_access.allowRead;
}

bool CRawFile::IsWriteable( void ) const noexcept
{
    return m_access.allowWrite;
}
