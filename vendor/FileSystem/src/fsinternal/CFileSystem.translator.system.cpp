/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.translator.system.cpp
*  PURPOSE:     FileSystem OS translator that represents directory links
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/
#include <StdInc.h>
#include <sys/stat.h>
#include <string>

// Include the internal definitions.
#include "CFileSystem.internal.h"

// Sub modules.
#include "CFileSystem.platform.h"
#include "CFileSystem.translator.system.h"
#include "CFileSystem.stream.raw.h"
#include "CFileSystem.platformutils.hxx"

// Include common fs utilitites.
#include "../CFileSystem.Utils.hxx"

// Include native utilities for platforms.
#include "CFileSystem.internal.nativeimpl.hxx"

// ScanDirectory utilities.
#include "CFileSystem.translator.scanutil.hxx"

using namespace FileSystem;

/*===================================================
    File_IsDirectoryAbsolute

    Arguments:
        pPath - Absolute path pointing to an OS filesystem entry.
    Purpose:
        Checks the given path and returns true if it points
        to a directory, false if a file or no entry was found
        at the path.
===================================================*/
bool File_IsDirectoryAbsolute( const char *pPath )
{
#ifdef _WIN32
    return _FileWin32_IsDirectoryAbsolute( pPath );
#elif defined(__linux__)
    return _FileLinux_IsDirectoryAbsolute( pPath );
#else
#error No implementation for File_IsDirectoryAbsolute
#endif //_WIN32
}

/*====================================================
    CSystemFileTranslator

    Default file translator which is located on the
    OS file system. Operations on this translator
    should persist across application executions.
====================================================*/

CSystemFileTranslator::~CSystemFileTranslator( void )
{
#ifdef _WIN32
    if ( m_curDirHandle && m_curDirHandle != m_rootHandle )
    {
        CloseHandle( m_curDirHandle );
    }

    CloseHandle( m_rootHandle );
#elif defined(__linux__)
    if ( m_curDirHandle && m_curDirHandle != m_rootHandle )
    {
        closedir( m_curDirHandle );
    }

    closedir( m_rootHandle );
#else
#error Missing implementation for CSystemFileTranslator destructor
#endif //OS DEPENDANT CODE
}

template <typename charType>
bool CSystemFileTranslator::ParseSystemPath( const charType *path, bool allowFile, translatorPathResult& transPathOut ) const
{
    try
    {
        return this->ParseTranslatorPathGuided( path, this->m_curDirPath, eBaseDirDesignation::ROOT_DIR, allowFile, transPathOut );
    }
    catch( filesystem_exception& )
    {
        return false;
    }
    catch( eir::codepoint_exception& )
    {
        return false;
    }
}

filePath CSystemFileTranslator::GetFullRootDirPath( const translatorPathResult& path, bool& slashDirOut ) const
{
    bool slashDirection;
    filePath root;

    eRequestedPathResolution pathType = path.type;

    if ( pathType == eRequestedPathResolution::RELATIVE_PATH )
    {
        const normalNodePath& relPath = path.relpath;

#ifdef _WIN32
        bool shouldBeExtended = fileSystem->m_useExtendedPaths;

        root = this->m_rootPath.RootDescriptorExtended( shouldBeExtended );
        slashDirection = this->m_rootPath.DecideSlashDirectionExtended( shouldBeExtended );
#else
        root = this->m_rootPath.RootDescriptor();
        slashDirection = this->m_rootPath.DecideSlashDirection();
#endif //CROSS PLATFORM CODE

#ifdef _DEBUG
        assert( relPath.backCount <= this->m_rootPath.RootNodes().GetCount() );
#endif //_DEBUG

        size_t rootTravelCount = ( this->m_rootPath.RootNodes().GetCount() - relPath.backCount );

        _File_OutputPathTreeCount( this->m_rootPath.RootNodes(), rootTravelCount, false, slashDirection, root );

        size_t relPathTravelCount = relPath.travelNodes.GetCount();

        bool relPathIsFile = relPath.isFilePath;

        _File_OutputPathTreeCount( relPath.travelNodes, relPathTravelCount, relPathIsFile, slashDirection, root );
    }
    else if ( pathType == eRequestedPathResolution::FULL_PATH )
    {
#ifdef _WIN32
        bool shouldBeExtended = fileSystem->m_useExtendedPaths;

        root = path.fullpath.RootDescriptorExtended( shouldBeExtended );
        slashDirection = path.fullpath.DecideSlashDirectionExtended( shouldBeExtended );
#else
        root = path.fullpath.RootDescriptor();
        slashDirection = path.fullpath.DecideSlashDirection();
#endif //CROSS PLATFORM CODE.

        _File_OutputPathTree( path.fullpath.RootNodes(), path.fullpath.IsFilePath(), slashDirection, root );
    }
    else
    {
        throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
    }

    slashDirOut = slashDirection;

    return root;
}

AINLINE bool _File_Exists( filePath& path )
{
#ifdef _WIN32
    if ( const char *sysPath = path.to_char <char> () )
    {
        return _FileWin32_DoesFileExist( sysPath );
    }
    else if ( const wchar_t *sysPath = path.to_char <wchar_t> () )
    {
        return _FileWin32_DoesFileExist( sysPath );
    }
    else
    {
        path.transform_to <wchar_t> ();

        return _FileWin32_DoesFileExist( path.to_char <wchar_t> () );
    }
#elif defined(__linux__)
    path.transform_to <char> ();

    return _FileLinux_DoesFileExist( path.to_char <char> () );
#else
#error Missing implementation for _File_Exists
#endif //OS DEPENDANT CODE
}

void CSystemFileTranslator::CheckForFaultyTranslator( void ) const
{
    // If we are not in outbreak mode then we can only be valid if our root handle is valid.
    if ( IsOutbreakEnabled() == false )
    {
#ifdef _WIN32
        DWORD sizeReq = GetFileSize( this->m_rootHandle, nullptr );
        DWORD lastErr = GetLastError();

        if ( sizeReq == INVALID_FILE_SIZE && lastErr == ERROR_FILE_INVALID )
        {
            throw TerminatedObjectException();
        }
#elif defined(__linux__)
        // Check whether our root directory does still exist.
        // Since there is no mandatory locking support under linux we can only pray that nobody deletes our stuff.
        filePath rootPath;

        this->GetFullPath( "//", false, rootPath );

        if ( _File_Exists( rootPath ) == false )
        {
            throw TerminatedObjectException();
        }
#else
#error no translator validity checking implementation
#endif //CROSS PLATFORM CODE
    }
}

bool CSystemFileTranslator::_CreateDirTree( const translatorPathResult& dirPath, bool shouldRemoveFileAtEnd, bool doCreateParentDirs, bool doCreateActualDir )
{
    // If shouldRemoveFileAtEnd is true, then the trimmed row of nodes is the actual directory line-up; the end of that lineup is the directory that must be created.
    // If shouldRemoveFileAtEnd is false, then the actual file at the end is the directory that should be created meaning that if it is empty then all tokens must be parent dirs.

    const dirNames *travelNodes;
    filePath pathFromRoot;
    bool slashDir;
    bool isFileNodes;

    eRequestedPathResolution pathType = dirPath.type;

    if ( pathType == eRequestedPathResolution::RELATIVE_PATH )
    {
        // We go back to the root directory item that our dirPath starts growing from.
        translatorPathResult toRootPath;
        toRootPath.type = eRequestedPathResolution::RELATIVE_PATH;
        toRootPath.relpath.backCount = dirPath.relpath.backCount;
        toRootPath.relpath.isFilePath = false;

        pathFromRoot = this->GetFullRootDirPath( toRootPath, slashDir );
        travelNodes = &dirPath.relpath.travelNodes;
        isFileNodes = dirPath.relpath.isFilePath;
    }
    else
    {
#ifdef _WIN32
        bool shouldBeExtended = fileSystem->m_useExtendedPaths;

        pathFromRoot = dirPath.fullpath.RootDescriptorExtended( shouldBeExtended );
        slashDir = dirPath.fullpath.DecideSlashDirectionExtended( shouldBeExtended );
#else
        pathFromRoot = dirPath.fullpath.RootDescriptor();
        slashDir = dirPath.fullpath.DecideSlashDirection();
#endif //CROSS PLATFORM CODE

        travelNodes = &dirPath.fullpath.RootNodes();
        isFileNodes = dirPath.fullpath.IsFilePath();
    }

    // Build up nodes until we hit the requested file location or stuff.
    size_t dirTokenCount = travelNodes->GetCount();

    if ( shouldRemoveFileAtEnd && isFileNodes )
    {
        assert( dirTokenCount > 0 );

        dirTokenCount--;
    }

    // Is there even anything do be done?
    if ( dirTokenCount == 0 )
        return true;

    size_t parentDirCount = ( dirTokenCount - 1 );

    for ( size_t n = 0; n < dirTokenCount; n++ )
    {
        pathFromRoot += (*travelNodes)[ n ];
        pathFromRoot += FileSystem::GetDirectorySeparator <char> ( slashDir );

        // Should we create parent directories?
        if ( ( doCreateParentDirs == true && n < parentDirCount ) || ( doCreateActualDir == true && n >= parentDirCount ) )
        {
            bool success = _File_CreateDirectory( pathFromRoot );

            if ( !success )
            {
                return false;
            }
        }
    }

    return true;
}

template <typename charType>
bool CSystemFileTranslator::GenCreateDir( const charType *path, filesysPathOperatingMode opMode, bool doCreateParentDirs )
{
    CheckForFaultyTranslator();

    translatorPathResult transPath;

    bool allowFileAtEnd = FileSystem::CanDirectoryPathHaveTrailingFile( this->m_pathProcessMode, opMode );

    if ( !ParseSystemPath( path, allowFileAtEnd, transPath ) )
        return false;

    return _CreateDirTree( transPath, false, doCreateParentDirs, true );
}

bool CSystemFileTranslator::CreateDir( const char *path, filesysPathOperatingMode opMode, bool createParentDirs )       { return GenCreateDir( path, opMode, createParentDirs ); }
bool CSystemFileTranslator::CreateDir( const wchar_t *path, filesysPathOperatingMode opMode, bool createParentDirs )    { return GenCreateDir( path, opMode, createParentDirs ); }
bool CSystemFileTranslator::CreateDir( const char8_t *path, filesysPathOperatingMode opMode, bool createParentDirs )    { return GenCreateDir( path, opMode, createParentDirs ); }

template <typename charType>
CFile* CSystemFileTranslator::GenOpen( const charType *path, const filesysOpenMode& mode, eFileOpenFlags flags )
{
    CheckForFaultyTranslator();

    translatorPathResult transPath;

    if ( !ParseSystemPath( path, true, transPath ) )
    {
        ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::PATH_OUT_OF_SCOPE );

        return nullptr;
    }

    // We can only open files!
    if ( !transPath.IsFilePath() )
    {
        ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::INVALID_PARAMETERS );

        return nullptr;
    }

    bool slashDirection;
    filePath output = this->GetFullRootDirPath( transPath, slashDirection );

    eFileOpenDisposition openType = mode.openDisposition;

    // Have we requested the creation of parent directories?
    // Only do so if we are creating the file anyway.
    if ( mode.createParentDirs && FileSystem::IsModeCreation( openType ) )
    {
        bool dirSuccess = _CreateDirTree( transPath, true, true, true );

        if ( !dirSuccess )
        {
            ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::ACCESS_DENIED );

            return nullptr;
        }
    }

#ifdef _WIN32
    // Translate to native OS access and create mode.
    DWORD win32AccessMode = 0;

    if ( mode.access.allowRead )
    {
        win32AccessMode |= GENERIC_READ;
    }
    if ( mode.access.allowWrite )
    {
        win32AccessMode |= GENERIC_WRITE;
    }

    DWORD win32CreateMode = 0;

    if ( openType == eFileOpenDisposition::OPEN_EXISTS )
    {
        win32CreateMode = OPEN_EXISTING;
    }
    else if ( openType == eFileOpenDisposition::CREATE_OVERWRITE )
    {
        win32CreateMode = CREATE_ALWAYS;
    }
    else if ( openType == eFileOpenDisposition::CREATE_NO_OVERWRITE )
    {
        win32CreateMode = CREATE_NEW;
    }
    else if ( openType == eFileOpenDisposition::OPEN_OR_CREATE )
    {
        win32CreateMode = OPEN_ALWAYS;
    }
    else
    {
        ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::INVALID_PARAMETERS );

        // Not supported.
        return nullptr;
    }

    DWORD flagAttr = 0;

    if ( flags & FILE_FLAG_TEMPORARY )
    {
        flagAttr |= FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY;
    }

    if ( flags & FILE_FLAG_UNBUFFERED )
    {
        flagAttr |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
    }

    HANDLE sysHandle = INVALID_HANDLE_VALUE;

    DWORD win32ShareMode = FILE_SHARE_READ;

    if ( ( flags & FILE_FLAG_WRITESHARE ) != 0 )
    {
        win32ShareMode |= FILE_SHARE_WRITE;
    }

    if ( const char *sysPath = output.c_str() )
    {
        sysHandle = CreateFileA( sysPath, win32AccessMode, win32ShareMode, nullptr, win32CreateMode, flagAttr, nullptr );
    }
    else if ( const wchar_t *sysPath = output.w_str() )
    {
        sysHandle = CreateFileW( sysPath, win32AccessMode, win32ShareMode, nullptr, win32CreateMode, flagAttr, nullptr );
    }
    else
    {
        // For unknown stuff.
        output.transform_to <wchar_t> ();

        sysHandle = CreateFileW( output.w_str(), win32AccessMode, win32ShareMode, nullptr, win32CreateMode, flagAttr, nullptr );
    }

    if ( sysHandle == INVALID_HANDLE_VALUE )
    {
        // Try to set the runtime error code.
        eFileOpenFailure open_code = eFileOpenFailure::UNKNOWN_ERROR;

        DWORD last_error = GetLastError();

        if ( last_error == ERROR_FILE_NOT_FOUND || last_error == ERROR_PATH_NOT_FOUND )
        {
            open_code = eFileOpenFailure::NOT_FOUND;
        }
        else if ( last_error == ERROR_ACCESS_DENIED )
        {
            open_code = eFileOpenFailure::ACCESS_DENIED;
        }
        else if ( last_error == ERROR_ALREADY_EXISTS || last_error == ERROR_FILE_EXISTS )
        {
            open_code = eFileOpenFailure::ALREADY_EXISTS;
        }

        // Report the failure.
        ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( open_code );

        return nullptr;
    }
#elif defined(__linux__)
    //int linux_flags = O_DIRECT; // to allow read/write throw errors upon first operation if backing storage is gone.
    int linux_flags = 0;

    // Let me explain to you why O_DIRECT is evil/not worth the general usage:
    // The promise of O_DIRECT is really cool: direct DMA read and writes from disk sectors!
    // You get to do block-based direct atomic disk writes, wow! Imagíne how slow it is to use
    // multiple system calls for this task. All you have to do is align the user-buffer pointer,
    // the user buffer size and the file seek to the block-size.
    // And this is where the catch is: you need to align ALL THREE VALUES. I would have understood
    // if you had to align the file offset and the user buffer to the blocksize, but the request
    // size too? This makes it impossible to write files whose size is not a multiple of the
    // disk block sector size without an additional call to truncate the file!
    // In the worst case, the user does notice that the file is a multiple of the sector size until
    // the file was closed by the program. Definately not best sight to be held. And image
    // what this special operation model means for cross-process interaction on FDs: no support,
    // you get haywire interoperability! Usually you make the assumption that files are like
    // inter-process memory with an awkward pointer-like seek mechanism that requires a function
    // call. But since data can only be written in blocks the loss of granularity makes data
    // synchronization across processes a hassle.
    // Thus it is good advice by the Linux guys to make O_DIRECT an optional thing to be turned on
    // by specific needs. Of course, using O_DIRECT is better performance and we will add support
    // to FileSystem using a special ghosting-enabled buffered stream wrapper!

    bool mode_allowRead = mode.access.allowRead;
    bool mode_allowWrite = mode.access.allowWrite;

    if ( mode_allowRead && mode_allowWrite )
    {
        linux_flags |= O_RDWR;
    }
    else if ( mode_allowRead )
    {
        linux_flags |= O_RDONLY;
    }
    else if ( mode_allowWrite )
    {
        linux_flags |= O_WRONLY;
    }

    if ( openType == eFileOpenDisposition::OPEN_EXISTS )
    {
        // Nothing to do.
    }
    else if ( openType == eFileOpenDisposition::CREATE_OVERWRITE )
    {
        linux_flags |= ( O_CREAT | O_TRUNC );
    }
    else if ( openType == eFileOpenDisposition::CREATE_NO_OVERWRITE )
    {
        linux_flags |= ( O_CREAT | O_EXCL );
    }
    else if ( openType == eFileOpenDisposition::OPEN_OR_CREATE )
    {
        linux_flags |= ( O_CREAT );
    }
    else
    {
        ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( eFileOpenFailure::INVALID_PARAMETERS );

        // Not supported.
        return nullptr;
    }

    // TODO: support flags parameter.

    output.transform_to <char> ();

    int fileIndex = open( output.c_str(), linux_flags, 0777 );

    if ( fileIndex < 0 )
    {
        eFileOpenFailure open_code = eFileOpenFailure::UNKNOWN_ERROR;

        int err_code = errno;

        if ( err_code == EACCES || err_code == EPERM )
        {
            open_code = eFileOpenFailure::ACCESS_DENIED;
        }
        else if ( err_code == EDQUOT || err_code == EMFILE || err_code == ENFILE )
        {
            open_code = eFileOpenFailure::RESOURCES_EXHAUSTED;
        }
        else if ( err_code == EEXIST )
        {
            open_code = eFileOpenFailure::ALREADY_EXISTS;
        }
        else if ( err_code == ENOENT )
        {
            open_code = eFileOpenFailure::NOT_FOUND;
        }

        // Report the failure.
        ((CFileSystemNative*)fileSystem)->OnTranslatorOpenFailure( open_code );

        return nullptr;
    }
#else
#error Missing implementation for CSystemFileTranslator::GenOpen file handle open logic
#endif //OS DEPENDANT CODE

    CFile *outFile = nullptr;

    try
    {
        CRawFile *pFile = new CRawFile( std::move( output ), mode.access );

#ifdef _WIN32
        pFile->m_file = sysHandle;
#elif defined(__linux__)
        pFile->m_fileIndex = fileIndex;
#else
#error Missing implementation for CSystemFileTranslator::GenOpen file handle acquisition logic
#endif //OS DEPENDANT CODE

        // Check for append-mode.
        // Have to improve this later by actually supporting automatic seek at stream end.
        if ( mode.seekAtEnd )
        {
            pFile->SeekNative( 0, SEEK_END );
        }

        // Success!
        outFile = pFile;
    }
    catch( ... )
    {
#ifdef _WIN32
        CloseHandle( sysHandle );
#elif defined(__linux__)
        close( fileIndex );
#else
#error Missing implementation for CSystemFileTranslator::GenOpen exception clean-up logic
#endif

        throw;
    }

    // If required, wrap the file into a buffered stream.
    if ( fileSystem->m_doBufferAllRaw )
    {
        try
        {
            outFile = new CBufferedStreamWrap( outFile, true );
        }
        catch( ... )
        {
            // Just return the regular file without buffering.
        }
    }

    return outFile;
}

CFile* CSystemFileTranslator::Open( const char *path, const filesysOpenMode& mode, eFileOpenFlags flags )       { return GenOpen( path, mode, flags ); }
CFile* CSystemFileTranslator::Open( const wchar_t *path, const filesysOpenMode& mode, eFileOpenFlags flags )    { return GenOpen( path, mode, flags ); }
CFile* CSystemFileTranslator::Open( const char8_t *path, const filesysOpenMode& mode, eFileOpenFlags flags )    { return GenOpen( path, mode, flags ); }

AINLINE bool _File_Stat( filePath& path, filesysStats& statsOut )
{
#ifdef _WIN32
    return _FileWin32_GetFileInformationByPath( path, statsOut );
#elif defined(__linux__)
    int iStat = -1;

    if ( const char *sysPath = path.c_str() )
    {
        iStat = _FileLinux_StatFile( sysPath, statsOut );
    }
    else
    {
        path.transform_to <char> ();

        iStat = _FileLinux_StatFile( path.c_str(), statsOut );
    }

    return ( iStat == 0 );
#else
#error Missing implementation for _File_Stat
#endif //OS DEPENDANT CODE
}

AINLINE bool _File_GetAttributesByPath( filePath& path, filesysAttributes& attribsOut )
{
    if ( const char *sysPath = path.c_str() )
    {
#ifdef _WIN32
        return _FileWin32_GetFileAttributesByPath( sysPath, attribsOut );
#elif defined(__linux__)
        return _FileLinux_GetFileAttributesByPath( sysPath, attribsOut );
#else
#error Missing implementation for _File_GetAttributesByPath is-file-or-directory check
#endif //CROSS PLATFORM CODE
    }
#ifdef _WIN32
    else if ( const wchar_t *sysPath = path.w_str() )
    {
        return _FileWin32_GetFileAttributesByPath( sysPath, attribsOut );
    }
#endif //_WIN32
    else
    {
#ifdef _WIN32
        path.transform_to <wchar_t> ();

        return _FileWin32_GetFileAttributesByPath( path.w_str(), attribsOut );
#elif defined(__linux__)
        path.transform_to <char> ();

        return _FileLinux_GetFileAttributesByPath( path.c_str(), attribsOut );
#else
#error Missing implementation for _File_GetAttributesByPath is-file-or-directory unknown character type check
#endif //CROSS PLATFORM CODE
    }
}

template <typename charType>
bool CSystemFileTranslator::GenExists( const charType *path ) const
{
    CheckForFaultyTranslator();

    bool isFilePath;
    filePath absPath;
    {
        translatorPathResult transPath;

        if ( !ParseSystemPath( path, true, transPath ) )
            return false;

        bool slashDir;
        absPath = this->GetFullRootDirPath( transPath, slashDir );
        isFilePath = transPath.IsFilePath();
    }

    // Now it depends on if we wanted a directory or a file.
    // This is because on native file systems (Windows, Linux, etc) for each file path there can only be one file or folder, not both.
    filesysPathProcessMode procMode = this->m_pathProcessMode;

    if ( procMode == filesysPathProcessMode::DISTINGUISHED )
    {
        // Acquire the stats so we can check if the correct type was queried.
        filesysStats tmp;

        if ( _File_Stat( absPath, tmp ) == false )
        {
            return false;
        }

        eFilesysItemType itemType = tmp.attribs.type;

        if ( isFilePath )
        {
            return ( itemType == eFilesysItemType::FILE || itemType == eFilesysItemType::UNKNOWN );
        }
        else
        {
            return ( itemType == eFilesysItemType::DIRECTORY );
        }
    }
    else if ( procMode == filesysPathProcessMode::AMBIVALENT_FILE )
    {
        if ( isFilePath == false )
        {
            // Acquire the stats so we can check if the correct type was queried.
            filesysStats tmp;

            if ( _File_Stat( absPath, tmp ) == false )
            {
                return false;
            }

            // If we tried a directory path but the result is a file, then we just say it ain't there.
            eFilesysItemType itemType = tmp.attribs.type;

            if ( itemType != eFilesysItemType::DIRECTORY )
            {
                return false;
            }
        }
        else
        {
            // Optimization for ambivalent file existance check.
            if ( _File_Exists( absPath ) == false )
            {
                return false;
            }
        }

        // Since either a file or a directory exists in this location we are fine.
        return true;
    }
    else
    {
#ifdef _DEBUG
        assert( 0 );
#endif //_DEBUG

        // No idea what this is.
        return false;
    }
}

bool CSystemFileTranslator::Exists( const char *path ) const        { return GenExists( path ); }
bool CSystemFileTranslator::Exists( const wchar_t *path ) const     { return GenExists( path ); }
bool CSystemFileTranslator::Exists( const char8_t *path ) const     { return GenExists( path ); }

inline bool _deleteFileCallback_gen( const filePath& path )
{
    bool deletionSuccess = false;

    if ( const char *sysPath = path.c_str() )
    {
#ifdef _WIN32
        deletionSuccess = _FileWin32_DeleteFile( sysPath );
#elif defined(__linux__)
        deletionSuccess = _FileLinux_DeleteFile( sysPath );
#else
#error Missing implementation for _deleteFileCallback_gen char deletion
#endif
    }
#ifdef _WIN32
    else if ( const wchar_t *sysPath = path.w_str() )
    {
        deletionSuccess = _FileWin32_DeleteFile( sysPath );
    }
#endif //_WIN32
    else
    {
        // Unknown character type.
#ifdef _WIN32
        auto widePath = path.convert_unicode <FSObjectHeapAllocator> ();

        deletionSuccess = _FileWin32_DeleteFile( widePath.GetConstString() );
#elif defined(__linux__)
        auto ansiPath = path.convert_ansi <FSObjectHeapAllocator> ();

        deletionSuccess = _FileLinux_DeleteFile( ansiPath.GetConstString() );
#else
#error Missing implementation for _deleteFileCallback_gen unknown character type
#endif
    }

    return deletionSuccess;
}

static void _deleteFileCallback( const filePath& path, void *ud )
{
#ifdef _DEBUG
    bool deletionSuccess =
#endif //_DEBUG
    _deleteFileCallback_gen( path );

#ifdef _DEBUG
    if ( !deletionSuccess )
    {
        assert( 0 );
    }
#endif //_DEBUG
}

static void _deleteDirCallback( const filePath& path, void *ud );

inline bool _deleteDirCallback_gen( const filePath& path, CFileTranslator *sysRoot )
{
    bool deletionSuccess = false;

    // Continue recursively.
    path.char_dispatch(
        [&]( auto path )
    {
        sysRoot->ScanDirectory( path, GetAnyWildcardSelector <typename std::decay <decltype(*path)>::type> (), false, _deleteDirCallback, _deleteFileCallback, sysRoot );
    });

    if ( const char *sysPath = path.c_str() )
    {
#ifdef _WIN32
        deletionSuccess = _FileWin32_DeleteDirectory( sysPath );
#elif defined(__linux__)
        deletionSuccess = _FileLinux_DeleteDirectory( sysPath );
#else
#error Missing implementation for _deleteDirCallback_gen char deletion
#endif //CROSS PLATFORM CODE
    }
#ifdef _WIN32
    else if ( const wchar_t *sysPath = path.w_str() )
    {
        deletionSuccess = _FileWin32_DeleteDirectory( sysPath );
    }
#endif //_WIN32
    else
    {
#ifdef _WIN32
        auto widePath = path.convert_unicode <FSObjectHeapAllocator> ();

        deletionSuccess = _FileWin32_DeleteDirectory( widePath.GetConstString() );
#elif defined(__linux__)
        auto ansiPath = path.convert_ansi <FSObjectHeapAllocator> ();

        deletionSuccess = _FileLinux_DeleteDirectory( ansiPath.GetConstString() );
#else
#error Missing implementation for _deleteDirCallback_gen unknown character type deletion
#endif //CROSS PLATFORM CODE
    }

    return deletionSuccess;
}

static void _deleteDirCallback( const filePath& path, void *ud )
{
    // Delete all subdirectories too.
    CFileTranslator *sysRoot = (CFileTranslator*)ud;

#ifdef _DEBUG
    bool deletionSuccess =
#endif //_DEBUG
    _deleteDirCallback_gen( path, sysRoot );

#ifdef _DEBUG
    if ( !deletionSuccess )
    {
        assert( 0 );
    }
#endif //_DEBUG
}

template <typename charType>
bool CSystemFileTranslator::GenDelete( const charType *path, filesysPathOperatingMode opMode )
{
    CheckForFaultyTranslator();

    filePath output;
    bool isFilePath;
    {
        translatorPathResult transPath;

        if ( !ParseSystemPath( path, true, transPath ) )
            return false;

        bool slashDir;
        output = this->GetFullRootDirPath( transPath, slashDir );
        isFilePath = transPath.IsFilePath();
    }

    // Check whether we want to allow processing of directories or files.
    bool doDeleteFile = false;
    bool doDeleteDirectory = false;

    if ( opMode == filesysPathOperatingMode::DEFAULT )
    {
        filesysPathProcessMode procMode = this->m_pathProcessMode;

        if ( procMode == filesysPathProcessMode::DISTINGUISHED )
        {
            doDeleteFile = isFilePath;
            doDeleteDirectory = !isFilePath;
        }
        else if ( procMode == filesysPathProcessMode::AMBIVALENT_FILE )
        {
            doDeleteFile = isFilePath;
            doDeleteDirectory = true;
        }
        else
        {
#ifdef _DEBUG
            assert( 0 );
#endif //_DEBUG

            throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
        }
    }
    else if ( opMode == filesysPathOperatingMode::DIR_ONLY )
    {
        doDeleteFile = false;
        doDeleteDirectory = true;
    }
    else if ( opMode == filesysPathOperatingMode::FILE_ONLY )
    {
        doDeleteFile = true;
        doDeleteDirectory = false;
    }
    else
    {
#ifdef _DEBUG
        assert( 0 );
#endif //_DEBUG

        throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
    }

    // Check what kind of item we have.
    filesysAttributes itemAttribs;

    bool doesItemExist = _File_GetAttributesByPath( output, itemAttribs );

    // If there is nothing to delete then we simply return failure.
    if ( doesItemExist == false )
    {
        return false;
    }

    eFilesysItemType itemType = itemAttribs.type;

    if ( itemType == eFilesysItemType::DIRECTORY && doDeleteDirectory )
    {
        // Remove all files and directories inside
        return _deleteDirCallback_gen( output, this );
    }
    else if ( ( itemType == eFilesysItemType::FILE || itemType == eFilesysItemType::UNKNOWN ) && doDeleteFile )
    {
        return _deleteFileCallback_gen( output );
    }

    return false;
}

bool CSystemFileTranslator::Delete( const char *path, filesysPathOperatingMode opMode )         { return GenDelete( path, opMode ); }
bool CSystemFileTranslator::Delete( const wchar_t *path, filesysPathOperatingMode opMode )      { return GenDelete( path, opMode ); }
bool CSystemFileTranslator::Delete( const char8_t *path, filesysPathOperatingMode opMode )      { return GenDelete( path, opMode ); }

template <typename charType>
bool CSystemFileTranslator::GenCopy( const charType *src, const charType *dst )
{
    CheckForFaultyTranslator();

    // TODO: do we want to support directory-copy too?

    filePath source, target;
    {
        translatorPathResult srcTransPath;
        translatorPathResult dstTransPath;

        if ( !ParseSystemPath( src, true, srcTransPath ) || !srcTransPath.IsFilePath() || !ParseSystemPath( dst, true, dstTransPath ) || !dstTransPath.IsFilePath() )
            return false;

        // We always start from root.
        bool target_slashDirection;
        target = this->GetFullRootDirPath( dstTransPath, target_slashDirection );

        // Make sure dir exists.
        bool dirSuccess = _CreateDirTree( dstTransPath, true, true, true );

        if ( !dirSuccess )
            return false;

        bool source_slashDirection;
        source = this->GetFullRootDirPath( srcTransPath, source_slashDirection );
    }

    // Copy data using quick kernel calls.
#ifdef _WIN32
    target.transform_to <wchar_t> ();
    source.transform_to <wchar_t> ();

    return _FileWin32_CopyFile( source.to_char <wchar_t> (), target.to_char <wchar_t> () );
#elif defined(__linux__)
    target.transform_to <char> ();
    source.transform_to <char> ();

    return _FileLinux_CopyFile( source.to_char <char> (), target.to_char <char> () );
#else
#error Missing implementation for CSystemFileTranslator::GenCopy
#endif //CROSS PLATFORM CODE
}

bool CSystemFileTranslator::Copy( const char *src, const char *dst )            { return GenCopy( src, dst ); }
bool CSystemFileTranslator::Copy( const wchar_t *src, const wchar_t *dst )      { return GenCopy( src, dst ); }
bool CSystemFileTranslator::Copy( const char8_t *src, const char8_t *dst )      { return GenCopy( src, dst ); }

template <typename charType>
bool CSystemFileTranslator::GenRename( const charType *src, const charType *dst )
{
    CheckForFaultyTranslator();

    // TODO: unit-test the OS functions that they do accept ending slash as directory indicator.

    filePath source, target;
    bool isSourceFilepath;
    translatorPathResult dstTransPath;
    {
        translatorPathResult srcTransPath;

        if ( !ParseSystemPath( src, true, srcTransPath ) || !ParseSystemPath( dst, true, dstTransPath ) )
            return false;

        // We always start from root.
        bool target_slashDirection;
        target = this->GetFullRootDirPath( dstTransPath, target_slashDirection );

        bool source_slashDirection;
        source = this->GetFullRootDirPath( srcTransPath, source_slashDirection );
        isSourceFilepath = srcTransPath.IsFilePath();
    }

    // The big problem with this function is that the OS functions support ambivalent processing but our API has
    // the possibility to switch between distinguished and ambivalent. Thus for distinguished we must manually
    // check the source file if it matches the behaviour. This should be no problem for our VFS because we have
    // full control over the FS tree there.

    filesysPathProcessMode procMode = this->m_pathProcessMode;

    bool doProcessDirectories = false;
    bool doProcessFiles = false;

    if ( procMode == filesysPathProcessMode::DISTINGUISHED )
    {
        doProcessDirectories = !isSourceFilepath;
        doProcessFiles = isSourceFilepath;
    }
    else if ( procMode == filesysPathProcessMode::AMBIVALENT_FILE )
    {
        doProcessDirectories = true;
        doProcessFiles = isSourceFilepath;
    }
    else
    {
#ifdef _DEBUG
        assert( 0 );
#endif //_DEBUG

        throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
    }

    if ( doProcessDirectories == false && doProcessFiles == false )
    {
        return false;
    }

    // Check whether we have to emulate distinguished processing.
    // Because only one of them is true.
    if ( doProcessDirectories != doProcessFiles )
    {
        filesysAttributes srcAttribs;

        // TODO: we must make sure that "source" is always not-degraded inside this function because it is allowed to transform source to
        //  another character type.
        bool doesExist = _File_GetAttributesByPath( source, srcAttribs );

        // Actually trying to work around some inconsistencies in the Win32 API here where the existannce-check returns false but the
        // moving of the file does work for trailing-slash stuff.
        if ( doesExist == false )
        {
            return false;
        }

        eFilesysItemType itemType = srcAttribs.type;

        if ( doProcessDirectories && itemType != eFilesysItemType::DIRECTORY )
        {
            // We did not find a directory but wanted to copy a directory.
            return false;
        }
        if ( doProcessFiles && ( itemType != eFilesysItemType::FILE && itemType != eFilesysItemType::UNKNOWN ) )
        {
            // We did not find a file but wanted to copy a file.
            return false;
        }
    }

    // Make sure parent directories exist.
    bool dirCreateSuccess = _CreateDirTree( dstTransPath, true, true, false );

    if ( !dirCreateSuccess )
        return false;

#ifdef _WIN32
    source.transform_to <wchar_t> ();
    target.transform_to <wchar_t> ();

    return _FileWin32_RenameFile( source.to_char <wchar_t> (), target.to_char <wchar_t> () );
#elif defined(__linux__)
    source.transform_to <char> ();
    target.transform_to <char> ();

    return _FileLinux_RenameFile( source.to_char <char> (), target.to_char <char> () );
#else
#error Missing implementation for CSystemFileTranslator::GenRename
#endif //CROSS PLATFORM CODE
}

bool CSystemFileTranslator::Rename( const char *src, const char *dst )          { return GenRename( src, dst ); }
bool CSystemFileTranslator::Rename( const wchar_t *src, const wchar_t *dst )    { return GenRename( src, dst ); }
bool CSystemFileTranslator::Rename( const char8_t *src, const char8_t *dst )    { return GenRename( src, dst ); }

template <typename charType>
bool CSystemFileTranslator::GenQueryStats( const charType *path, filesysStats& statsOut ) const
{
    CheckForFaultyTranslator();

    filePath output;
    bool isOutputFilepath;
    {
        translatorPathResult transPath;

        if ( !ParseSystemPath( path, true, transPath ) )
            return false;

        bool slashDir;
        output = this->GetFullRootDirPath( transPath, slashDir );
        isOutputFilepath = transPath.IsFilePath();
    }

    filesysPathProcessMode procMode = this->m_pathProcessMode;

    if ( procMode == filesysPathProcessMode::DISTINGUISHED )
    {
        // We must emulate distinguished processing.
        filesysStats theStats;

        bool gotStats = _File_Stat( output, theStats );

        if ( gotStats == false )
        {
            return false;
        }

        eFilesysItemType itemType = theStats.attribs.type;

        if ( isOutputFilepath == true && ( itemType != eFilesysItemType::FILE && itemType != eFilesysItemType::UNKNOWN ) )
        {
            return false;
        }
        if ( isOutputFilepath == false && itemType != eFilesysItemType::DIRECTORY )
        {
            return false;
        }

        statsOut = std::move( theStats );
        return true;
    }
    else if ( procMode == filesysPathProcessMode::AMBIVALENT_FILE )
    {
        // The OS functions are ambivalent in general.
        filesysStats theStats;

        bool gotStats = _File_Stat( output, theStats );

        if ( gotStats == false )
        {
            return false;
        }

        if ( isOutputFilepath == false && theStats.attribs.type != eFilesysItemType::DIRECTORY )
        {
            return false;
        }

        statsOut = std::move( theStats );
        return true;
    }
    else
    {
#ifdef _DEBUG
        assert( 0 );
#endif //_DEBUG

        throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
    }
}

bool CSystemFileTranslator::QueryStats( const char *path, filesysStats& stats ) const      { return GenQueryStats( path, stats ); }
bool CSystemFileTranslator::QueryStats( const wchar_t *path, filesysStats& stats ) const   { return GenQueryStats( path, stats ); }
bool CSystemFileTranslator::QueryStats( const char8_t *path, filesysStats& stats ) const   { return GenQueryStats( path, stats ); }

AINLINE size_t _File_GetSize( filePath& fullpath )
{
#ifdef _WIN32
    if ( const char *ansiPath = fullpath.to_char <char> () )
    {
        return (size_t)_FileWin32_GetFileSize( ansiPath );
    }
    else if ( const wchar_t *widePath = fullpath.to_char <wchar_t> () )
    {
        return (size_t)_FileWin32_GetFileSize( widePath );
    }
    else
    {
        fullpath.transform_to <wchar_t> ();

        return (size_t)_FileWin32_GetFileSize( fullpath.to_char <wchar_t> () );
    }
#elif defined(__linux__)
    fullpath.transform_to <char> ();

    return (size_t)_FileLinux_GetFileSize( fullpath.to_char <char> () );
#else
#error Missing implementation for _File_GetSize method
#endif //CROSS PLATFORM CODE
}

template <typename charType>
size_t CSystemFileTranslator::GenSize( const charType *path ) const
{
    CheckForFaultyTranslator();

    filePath fullpath;
    bool isFullpathFilepath;
    {
        translatorPathResult transPath;

        if ( !ParseSystemPath( path, true, transPath ) )
        {
            return 0;
        }

        bool slashDir;
        fullpath = this->GetFullRootDirPath( transPath, slashDir );
        isFullpathFilepath = transPath.IsFilePath();
    }

    filesysPathProcessMode procMode = this->m_pathProcessMode;

    bool doProcessDirectories = false;
    bool doProcessFiles = false;

    if ( procMode == filesysPathProcessMode::DISTINGUISHED )
    {
        doProcessDirectories = !isFullpathFilepath;
        doProcessFiles = isFullpathFilepath;
    }
    else if ( procMode == filesysPathProcessMode::AMBIVALENT_FILE )
    {
        doProcessDirectories = true;
        doProcessFiles = isFullpathFilepath;
    }
    else
    {
#ifdef _DEBUG
        assert( 0 );
#endif //_DEBUG

        throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
    }

    filesysAttributes itemAttribs;

    bool doesExist = _File_GetAttributesByPath( fullpath, itemAttribs );

    if ( doesExist == false )
        return 0;

    eFilesysItemType itemType = itemAttribs.type;

    if ( itemType == eFilesysItemType::DIRECTORY )
    {
        if ( doProcessDirectories == false )
        {
            return 0;
        }

        // TODO: when we have a non-stack-recursive way of iterating over an entire filesystem tree then we can implement this.
        return 0;
    }
    else if ( itemType == eFilesysItemType::FILE || itemType == eFilesysItemType::UNKNOWN )
    {
        if ( doProcessFiles == false )
        {
            return 0;
        }

        return _File_GetSize( fullpath );
    }
    else
    {
#ifdef _DEBUG
        assert( 0 );
#endif //_DEBUG

        throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
    }
}

size_t CSystemFileTranslator::Size( const char *path ) const        { return GenSize( path ); }
size_t CSystemFileTranslator::Size( const wchar_t *path ) const     { return GenSize( path ); }
size_t CSystemFileTranslator::Size( const char8_t *path ) const     { return GenSize( path ); }

bool CSystemFileTranslator::OnConfirmDirectoryChange( const translatorPathResult& nodePath )
{
    // NOTE: we can be a file-path because all it means that the ending slash is not present.
    //  this is valid in ambivalent path-process-mode.

    bool slashDir;
    filePath absPath = this->GetFullRootDirPath( nodePath, slashDir );

#ifdef _WIN32
    HANDLE dir = _FileWin32_OpenDirectoryHandle( absPath );

    if ( dir == INVALID_HANDLE_VALUE )
        return false;

    if ( m_curDirHandle )
    {
        CloseHandle( m_curDirHandle );
    }

    m_curDirHandle = dir;
#elif defined(__linux__)
    absPath.transform_to <char> ();

    DIR *dir = opendir( absPath.c_str() );

    if ( dir == nullptr )
        return false;

    if ( m_curDirHandle )
        closedir( m_curDirHandle );

    m_curDirHandle = dir;
#else
#error Missing implementation for CSystemFileTranslator::OnConfirmDirectoryChange logic
#endif //OS DEPENDANT CODE

    return true;
}

template <typename fsitem_iterator_type, typename pattern_env_type>
AINLINE void ImplScanDirectoryNative(
    filePath absDirPath, bool slashDirection,
    const pattern_env_type& patternEnv, const typename pattern_env_type::filePattern_t& pattern,
    bool recurse,
    pathCallback_t dirCallback, pathCallback_t fileCallback,
    void *userdata
)
{
    // TODO: actually protect this against infinite recursion when there are loops inside of the local filesystem.

    scanFilteringFlags flags;
    flags.noPatternOnDirs = !( dirCallback && !recurse );
    flags.noCurrentDirDesc = true;
    flags.noParentDirDesc = true;
    flags.noSystem = true;
    flags.noHidden = true;
    flags.noTemporary = true;
    // everything else can stay default.

    filtered_fsitem_iterator <fsitem_iterator_type, pattern_env_type> sys_iterator( absDirPath, flags, true );

    typename fsitem_iterator_type::info_data item_info;

    while ( sys_iterator.Next( patternEnv, pattern, item_info ) )
    {
        if ( item_info.isDirectory )
        {
            filePath target = absDirPath;
            target += item_info.filename;
            target += FileSystem::GetDirectorySeparator <char> ( slashDirection );

            if ( dirCallback )
            {
                if ( flags.noPatternOnDirs == false || sys_iterator.MatchDirectory( patternEnv, pattern, item_info ) )
                {
                    dirCallback( target, userdata );
                }
            }

            if ( recurse )
            {
                ImplScanDirectoryNative <fsitem_iterator_type> (
                    std::move( target ), slashDirection,
                    patternEnv, pattern,
                    true, dirCallback, fileCallback,
                    userdata
                );
            }
        }
        else if ( fileCallback )
        {
            filePath filename = absDirPath;
            filename += item_info.filename;

            fileCallback( filename, userdata );
        }
    }
}

template <typename charType>
AINLINE bool CSystemFileTranslator::DirPathTransformQualifiedInputPath( const charType *path, bool& slashDirOut, filePath& pathOut ) const
{
    // Decide whether we can take regular file paths as directories.
    bool doAcceptFilePathAsDirectory = FileSystem::CanDirectoryPathHaveTrailingFile( this->m_pathProcessMode, filesysPathOperatingMode::DEFAULT );

    translatorPathResult transDirPath;

    if ( !ParseSystemPath( path, doAcceptFilePathAsDirectory, transDirPath ) )
        return false;

    bool slashDir;
    filePath output = this->GetFullRootDirPath( transDirPath, slashDir );

    // Hack back the trailing slash because it is nice to have.
    if ( doAcceptFilePathAsDirectory && transDirPath.IsFilePath() )
    {
        output += FileSystem::GetDirectorySeparator <char> ( slashDir );
    }

    pathOut = std::move( output );
    slashDirOut = slashDir;
    return true;
}

// Helper definition.
#ifdef WIN32
typedef wchar_t platformIOCharacterType;
typedef win32_fsitem_iterator platform_dir_iterator_type;
#elif defined(__linux__)
typedef char platformIOCharacterType;
typedef linux_fsitem_iterator platform_dir_iterator_type;
#else
#error Missing platform directory iterator type
#endif //CROSS PLATFORM CODE

template <typename charType>
void CSystemFileTranslator::GenScanDirectory( const charType *directory, const charType *wildcard, bool recurse,
                                              pathCallback_t dirCallback,
                                              pathCallback_t fileCallback,
                                              void *userdata ) const
{
    CheckForFaultyTranslator();

    bool slashDir;
    filePath output;

    bool transSuccess = this->DirPathTransformQualifiedInputPath( directory, slashDir, output );

    if ( transSuccess == false )
    {
        return;
    }

    // Now glob-patterns are in the Eir SDK and properly unit tested.

    eir::PathPatternEnv <platformIOCharacterType, FSObjectHeapAllocator> patternEnv( eir::constr_with_alloc::DEFAULT );

    patternEnv.SetCaseSensitive( this->IsCaseSensitive() );

    decltype(patternEnv)::filePattern_t pattern = patternEnv.CreatePattern(
        _ResolveValidWildcard( wildcard )
    );

    ImplScanDirectoryNative <platform_dir_iterator_type> (
        std::move( output ), slashDir,
        patternEnv, pattern,
        recurse,
        dirCallback, fileCallback,
        userdata
    );
}

void CSystemFileTranslator::ScanDirectory( const char *directory, const char *wildcard, bool recurse,
                                           pathCallback_t dirCallback,
                                           pathCallback_t fileCallback,
                                           void *userdata ) const
{
    return GenScanDirectory( directory, wildcard, recurse, dirCallback, fileCallback, userdata );
}

void CSystemFileTranslator::ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse,
                                           pathCallback_t dirCallback,
                                           pathCallback_t fileCallback,
                                           void *userdata ) const
{
    return GenScanDirectory( directory, wildcard, recurse, dirCallback, fileCallback, userdata );
}

void CSystemFileTranslator::ScanDirectory( const char8_t *directory, const char8_t *wildcard, bool recurse,
                                           pathCallback_t dirCallback,
                                           pathCallback_t fileCallback,
                                           void *userdata ) const
{
    return GenScanDirectory( directory, wildcard, recurse, dirCallback, fileCallback, userdata );
}

static void _scanFindCallback( const filePath& path, dirNames *output )
{
    output->AddToBack( path );
}

template <typename charType>
void CSystemFileTranslator::GenGetDirectories( const charType *path, const charType *wildcard, bool recurse, dirNames& output ) const
{
    CheckForFaultyTranslator();

    ScanDirectory( path, wildcard, recurse, (pathCallback_t)_scanFindCallback, nullptr, &output );
}

void CSystemFileTranslator::GetDirectories( const char *path, const char *wildcard, bool recurse, dirNames& output ) const
{
    return GenGetDirectories( path, wildcard, recurse, output );
}
void CSystemFileTranslator::GetDirectories( const wchar_t *path, const wchar_t *wildcard, bool recurse, dirNames& output ) const
{
    return GenGetDirectories( path, wildcard, recurse, output );
}
void CSystemFileTranslator::GetDirectories( const char8_t *path, const char8_t *wildcard, bool recurse, dirNames& output ) const
{
    return GenGetDirectories( path, wildcard, recurse, output );
}

template <typename charType>
void CSystemFileTranslator::GenGetFiles( const charType *path, const charType *wildcard, bool recurse, dirNames& output ) const
{
    CheckForFaultyTranslator();

    ScanDirectory( path, wildcard, recurse, nullptr, (pathCallback_t)_scanFindCallback, &output );
}

void CSystemFileTranslator::GetFiles( const char *path, const char *wildcard, bool recurse, dirNames& output ) const
{
    return GenGetFiles( path, wildcard, recurse, output );
}
void CSystemFileTranslator::GetFiles( const wchar_t *path, const wchar_t *wildcard, bool recurse, dirNames& output ) const
{
    return GenGetFiles( path, wildcard, recurse, output );
}
void CSystemFileTranslator::GetFiles( const char8_t *path, const char8_t *wildcard, bool recurse, dirNames& output ) const
{
    return GenGetFiles( path, wildcard, recurse, output );
}

template <typename charType>
CDirectoryIterator* CSystemFileTranslator::GenBeginDirectoryListing( const charType *path, const charType *wildcard, const scanFilteringFlags& filter_flags ) const
{
    CheckForFaultyTranslator();

    bool slashDir;
    filePath output;

    bool transSuccess = this->DirPathTransformQualifiedInputPath( path, slashDir, output );

    if ( transSuccess == false )
    {
        return nullptr;
    }

    bool isCaseSensitive = this->IsCaseSensitive();

    return new CGenericDirectoryIterator <platformIOCharacterType, platform_dir_iterator_type> ( isCaseSensitive, std::move( output ), std::move( filter_flags ), _ResolveValidWildcard( wildcard ) );
}

CDirectoryIterator* CSystemFileTranslator::BeginDirectoryListing( const char *path, const char *wildcard, const scanFilteringFlags& filter_flags ) const
{
    return GenBeginDirectoryListing( path, wildcard, filter_flags );
}
CDirectoryIterator* CSystemFileTranslator::BeginDirectoryListing( const wchar_t *path, const wchar_t *wildcard, const scanFilteringFlags& filter_flags ) const
{
    return GenBeginDirectoryListing( path, wildcard, filter_flags );
}
CDirectoryIterator* CSystemFileTranslator::BeginDirectoryListing( const char8_t *path, const char8_t *wildcard, const scanFilteringFlags& filter_flags ) const
{
    return GenBeginDirectoryListing( path, wildcard, filter_flags );
}

/*====================================================
    Module initialization.
====================================================*/

void registerRawFileTranslatorInterface( const fs_construction_params& params )
{
    return;
}

void unregisterRawFileTranslatorInterface( void )
{
    return;
}
