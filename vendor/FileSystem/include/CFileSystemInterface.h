/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/include/CFileSystemInterface.h
*  PURPOSE:     File management
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _CFileSystemInterface_
#define _CFileSystemInterface_

#include "CFileSystem.common.h"

#include <stdarg.h>
#include <algorithm>
#include <type_traits>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif //_MSC_VER

// File open flags.
enum eFileOpenFlags : unsigned int
{
    FILE_FLAG_NONE =            0x00000000,
    FILE_FLAG_TEMPORARY =       0x00000001,
    FILE_FLAG_UNBUFFERED =      0x00000002,
    FILE_FLAG_GRIPLOCK =        0x00000004,
    FILE_FLAG_WRITESHARE =      0x00000008
};

enum eDirOpenFlags : unsigned int
{
    DIR_FLAG_NONE =             0x00000000,
    DIR_FLAG_EXCLUSIVE =        0x00000001,
    DIR_FLAG_WRITABLE =         0x00000002,
    DIR_FLAG_NO_READ =          0x00000004
};

// Exception system information.
namespace FileSystem
{
    enum class eGenExceptCode : std::uint32_t
    {
        RESOURCE_UNAVAILABLE = 0,       // failed to open file
        MEMORY_INSUFFICIENT = 1,        // could not allocate memory
        INVALID_SYSPARAM = 2,
        INVALID_PARAM = 3,
        ILLEGAL_PATHCHAR = 4,
        UNSUPPORTED_OPERATION = 5,
        INVALID_OBJECT_STATE = 6,
        // TODO: add more exception codes.

        INTERNAL_ERROR = 0xFFFFFFFF
    };

    // Base exception class of CFileSystem.
    // Could be thrown at any time if errors happen.
    // We advise APIs to use return codes if possible.
    struct filesystem_exception
    {
        inline filesystem_exception( eGenExceptCode code ) noexcept
        {
            this->code = code;
        }
        inline filesystem_exception( const filesystem_exception& ) = default;
        inline filesystem_exception( filesystem_exception&& ) = default;

        virtual ~filesystem_exception( void )
        {
            return;
        }

        inline filesystem_exception& operator = ( const filesystem_exception& ) = default;
        inline filesystem_exception& operator = ( filesystem_exception&& ) = default;

        inline eGenExceptCode get_code( void ) const noexcept
        {
            return this->code;
        }

    private:
        // We do not want to have locale-dependent data inside exceptions.
        // To retrieve a human-readable message an API should be called instead.
        // There could also be (less generic) exception classes inheriting from this.
        eGenExceptCode code;
    };

    /* Thrown whenever an object or another object that was operated on inside of called-scope
       is unavailable with (possibly) no future self-recovery in sight. The obvious path for recovery is to 
       try recreating the object using the same parameters as previous time.
       NOTE: system file translators are terminated based on their configuration; a change in outbreak mode
       could result in a recovery of it's state. */
    struct TerminatedObjectException : public filesystem_exception
    {
        inline TerminatedObjectException( void ) : filesystem_exception( eGenExceptCode::INVALID_OBJECT_STATE )
        {
            return;
        }
        inline TerminatedObjectException( const TerminatedObjectException& ) = default;
        inline TerminatedObjectException( TerminatedObjectException&& ) = default;

        inline ~TerminatedObjectException( void )
        {
            return;
        }

        inline TerminatedObjectException& operator = ( const TerminatedObjectException& ) = default;
        inline TerminatedObjectException& operator = ( TerminatedObjectException&& ) = default;
    };
};

// Include common stuff that is not without dependencies.
#include "CFileSystem.pathlogic.h"

enum class eFilesysItemType
{
    UNKNOWN,
    FILE,
    DIRECTORY
};

// Filesystem item attributes, as queried from the OS.
struct filesysAttributes
{
    eFilesysItemType type = eFilesysItemType::UNKNOWN;
    bool isSystem = false;
    bool isHidden = false;
    bool isTemporary = false;
    bool isJunctionOrLink = false;
};

// Statistics about a file/stream.
struct filesysStats
{
    filesysAttributes attribs;
    time_t atime = 0;
    time_t ctime = 0;
    time_t mtime = 0;
};

enum class eFileOpenDisposition
{
    OPEN_EXISTS,            // attempts to open an already existing file
    CREATE_OVERWRITE,       // creates a new file at location
    CREATE_NO_OVERWRITE,    // creates a file only if the file did not exist
    OPEN_OR_CREATE          // creates a new file if it did not exist, otherwise opens existing
};

struct filesysAccessFlags
{
    bool allowWrite = false;    // writing bytes to stream, truncation, time modification may work
    bool allowRead = true;      // reading bytes from stream may work
};

// Open-mode for file translators.
struct filesysOpenMode
{
    filesysAccessFlags access;
    bool seekAtEnd = false;
    bool createParentDirs = false;
    eFileOpenDisposition openDisposition = eFileOpenDisposition::OPEN_EXISTS;
};

namespace FileSystem
{
    template <typename charType>
    inline bool ParseOpenMode( const charType *mode, filesysOpenMode& modeOut )
    {
        typedef character_env <charType> char_env;

        try
        {
            character_env_iterator_tozero <charType> mode_iter( mode );

            filesysOpenMode mode;

            switch( mode_iter.ResolveAndIncrement() )
            {
            case 'w':
                mode.openDisposition = eFileOpenDisposition::CREATE_OVERWRITE;
                mode.access.allowWrite = true;
                mode.access.allowRead = false;
                mode.seekAtEnd = false;
                mode.createParentDirs = true;
                break;
            case 'r':
                mode.openDisposition = eFileOpenDisposition::OPEN_EXISTS;
                mode.access.allowWrite = false;
                mode.access.allowRead = true;
                mode.seekAtEnd = false;
                mode.createParentDirs = false;
                break;
            case 'a':
                mode.openDisposition = eFileOpenDisposition::OPEN_OR_CREATE;
                mode.access.allowWrite = true;
                mode.access.allowRead = false;
                mode.seekAtEnd = true;
                mode.createParentDirs = true;
                break;
            default:
                return false;
            }

            // Check advanced options.
            // We can do this because we ensured that the last codepoint was not a 0.
            typename char_env::ucp_t modulator_cp = mode_iter.ResolveAndIncrement();

            // Skip any binary qualifier, because we only support binary.
            if ( modulator_cp == 'b' )
            {
                modulator_cp = mode_iter.Resolve();
            }

            // Should both read and write be supported?
            if ( modulator_cp == '+' )
            {
                mode.access.allowRead = true;
                mode.access.allowWrite = true;
            }

            modeOut = std::move( mode );
            return true;
        }
        catch( eir::codepoint_exception& )
        {
            // Something went horribly wrong.
            return false;
        }
    }
    AINLINE bool ParseOpenMode( const filePath& mode, filesysOpenMode& modeOut )
    {
        return filePath_dispatch( mode,
            [&]( auto mode )
            {
                return ParseOpenMode( mode, modeOut );
            }
        );
    }

    inline bool IsModeCreation( eFileOpenDisposition mode )
    {
        return ( mode == eFileOpenDisposition::CREATE_NO_OVERWRITE ||
                 mode == eFileOpenDisposition::CREATE_OVERWRITE ||
                 mode == eFileOpenDisposition::OPEN_OR_CREATE );
    }
};

// Part of CFileMappingProvider.
struct filemapAccessMode
{
    bool allowRead = true;
    bool allowWrite = true;
    bool makePrivate = false;
};

/*===================================================
    CFileMappingProvider (manager class)

    Class that represents a set of direct memory pages from file contents.
    You can designate memory locations so that reading and writing
    semantics are directly linked to bytes on the persistent storage.
    Closing a file mapping provider does release all memory pages
    associated with it.

    To be honest, this class came to be because integration into Qt5.13
    required it.
===================================================*/
struct CFileMappingProvider abstract
{
    virtual ~CFileMappingProvider( void )       {}

    virtual void* MapFileRegion( fsOffsetNumber_t fileOff, size_t regLength, const filemapAccessMode& rights ) = 0;
    virtual bool UnMapFileRegion( void *dataptr ) = 0;
};

/*===================================================
    CFile (stream class)

    This is the access interface to files/streams. You can read,
    write to and obtain information from this. Once destroyed, the
    connection is unlinked. During class life-time, the file may be locked
    for deletion. Locks depend on the nature of the stream and of
    the OS/environment.
===================================================*/
class CFile abstract
{
public:
    virtual                 ~CFile( void )
    {
    }

    /*===================================================
        CFile::Read

        Arguments:
            buffer - memory location to write data to
            readSize - amount of bytes to read
        Purpose:
            Requests data from the file/stream and returns the
            amount of bytes actually read.
    ===================================================*/
    virtual	size_t          Read( void *buffer, size_t readSize ) = 0;

    /*===================================================
        CFile::Write

        Arguments:
            buffer - memory location to read data from
            writeCount - amount of bytes to write
        Purpose:
            Reads data chunks from buffer per sElement stride and
            forwards it to the file/stream. Returns the amount of
            bytes actually written.
    ===================================================*/
    virtual	size_t          Write( const void *buffer, size_t writeSize ) = 0;

    /*===================================================
        CFile::Seek

        Arguments:
            iOffset - positive or negative value to offset the stream by
            iType - SET_* ANSI enum to specify the procedure
        Purpose:
            Relocates the position of the file/stream. If successful,
            zero is returned. Otherwise, any other value than zero
            is returned.
    ===================================================*/
    virtual	int             Seek( long iOffset, int iType ) = 0;

    /*===================================================
        CFile::SeekNative

        Arguments:
            iOffset - positive or negative value to offset the stream by
            iType - SET_* ANSI enum to specify the procedure
        Purpose:
            Relocates the position of the file/stream. This function
            uses the native number type for maximum file addressing.
            If successful, zero is returned. Otherwise, any other value
            than zero is returned.
    ===================================================*/
    virtual int             SeekNative( fsOffsetNumber_t iOffset, int iType )
    {
        // Overwrite this function to offer actual native functionality.
        // Implementations do not have to support broader access.
        return Seek( (long)iOffset, iType );
    }

    /*===================================================
        CFile::Tell

        Purpose:
            Returns the absolute file/stream location.
    ===================================================*/
    virtual	long            Tell( void ) const = 0;

    /*===================================================
        CFile::TellNative

        Purpose:
            Returns the absolute file/stream location. The return
            value is a native number, so it has maximum file addressing
            range.
    ===================================================*/
    virtual fsOffsetNumber_t    TellNative( void ) const
    {
        // Overwrite this method to offset actual native functionality.
        // Implementations do not have to do that.
        return (fsOffsetNumber_t)Tell();
    }

    /*===================================================
        CFile::IsEOF

        Purpose:
            Returns whether the file/stream has reached it's end. Other
            than the ANSI feof, this is not triggered by reaching over
            the file/stream boundary.
    ===================================================*/
    virtual	bool            IsEOF( void ) const = 0;

    /*===================================================
        CFile::QueryStats

        Arguments:
            statsOut - file statistics output
        Purpose:
            Returns true whether it could request information
            about the file/stream. If successful, useful
            information was written to the output struct.
    ===================================================*/
    virtual	bool            QueryStats( filesysStats& attribOut ) const = 0;

    /*===================================================
        CFile::SetFileTimes

        Arguments:
            atime - time of access
            ctime - time of creation
            mtime - time of modification
        Purpose:
            Updates the file/stream time information.
    ===================================================*/
    virtual void            SetFileTimes( time_t atime, time_t ctime, time_t mtime ) = 0;

    /*===================================================
        CFile::SetSeekEnd

        Purpose:
            Sets the file/stream end at the current seek location.
            It effectively cuts off bytes beyond that.
    ===================================================*/
    virtual void            SetSeekEnd( void ) = 0;

    /*===================================================
        CFile::GetSize

        Purpose:
            Returns the total file/stream size if available.
            Otherwise it should return 0.
    ===================================================*/
    virtual	size_t          GetSize( void ) const = 0;

    /*===================================================
        CFile::GetSizeNative

        Purpose:
            Returns the total file/stream size if available.
            Otherwise it should return 0. This function returns
            the size in a native number. The returned value must
            not be negative.
    ===================================================*/
    virtual fsOffsetNumber_t    GetSizeNative( void ) const
    {
        // Overwrite this function to enable actual native support.
        return (fsOffsetNumber_t)GetSize();
    }

    /*===================================================
        CFile::Flush

        Purpose:
            Writes pending file/stream buffers to disk, thus having
            an updated representation in the filesystem to be read
            by different applications.
    ===================================================*/
    virtual	void            Flush( void ) = 0;

    /*===================================================
        CFile::CreateMapping

        Purpose:
            Returns a new memory mapping provider that is a set of
            memory pages directly linked with file content. Streams
            which do not support it return nullptr.
            You can keep the mapping provider alive even after
            destruction of the stream itself. But you must close
            the mapping provider prior to destruction of the
            underlying filesystem. For example, before destroying
            a ramdisk you must close all of its mapping providers.
    ===================================================*/
    virtual CFileMappingProvider*   CreateMapping( void ) = 0;

    /*===================================================
        CFile::GetPath

        Purpose:
            Returns the unique filesystem location descriptor of this
            file/stream.
    ===================================================*/
    virtual filePath        GetPath( void ) const = 0;

    /*===================================================
        CFile::IsReadable

        Purpose:
            Returns whether read operations are possible on this
            file/stream. If not, all attempts to request data
            from this are going to fail.
    ===================================================*/
    virtual bool            IsReadable( void ) const noexcept = 0;

    /*===================================================
        CFile::IsWriteable

        Purpose:
            Returns whether write operations are possible on this
            file/stream. If not, all attempts to push data into
            this are going to fail.
    ===================================================*/
    virtual bool            IsWriteable( void ) const noexcept = 0;

    // Utility definitions, mostly self-explanatory
    // These functions should be used if you want to preserve binary compatibility between systems.
    virtual	bool            ReadInt     ( fsInt_t& out_i )          { return ReadStruct( out_i ); }
    virtual bool            ReadUInt    ( fsUInt_t& out_ui )        { return ReadStruct( out_ui ); }
    virtual	bool            ReadShort   ( fsShort_t& out_s )        { return ReadStruct( out_s ); }
    virtual bool            ReadUShort  ( fsUShort_t& out_us )      { return ReadStruct( out_us ); }
    virtual	bool            ReadByte    ( fsChar_t& out_b )         { return ReadStruct( out_b ); }
    virtual bool            ReadByte    ( fsUChar_t& out_b )        { return ReadStruct( out_b ); }
    virtual bool            ReadWideInt ( fsWideInt_t out_wi )      { return ReadStruct( out_wi ); }
    virtual bool            ReadWideUInt( fsUWideInt_t out_uwi )    { return ReadStruct( out_uwi ); }
    virtual	bool            ReadFloat   ( fsFloat_t& out_f )        { return ReadStruct( out_f ); }
    virtual bool            ReadDouble  ( fsDouble_t& out_d )       { return ReadStruct( out_d ); }
    virtual bool            ReadBool    ( fsBool_t& out_b )         { return ReadStruct( out_b ); }

    virtual	size_t          WriteInt        ( fsInt_t iInt )            { return WriteStruct( iInt ); }
    virtual size_t          WriteUInt       ( fsUInt_t uiInt )          { return WriteStruct( uiInt ); }
    virtual size_t          WriteShort      ( fsShort_t iShort )        { return WriteStruct( iShort ); }
    virtual size_t          WriteUShort     ( fsUShort_t uShort )       { return WriteStruct( uShort ); }
    virtual size_t          WriteChar       ( fsChar_t cByte )          { return WriteStruct( cByte ); }
    virtual size_t          WriteByte       ( fsUChar_t ucByte )        { return WriteStruct( ucByte ); }
    virtual size_t          WriteWideInt    ( fsWideInt_t wInt )        { return WriteStruct( wInt ); }
    virtual size_t          WriteUWideInt   ( fsUWideInt_t uwInt )      { return WriteStruct( uwInt ); }
    virtual size_t          WriteFloat      ( fsFloat_t fFloat )        { return WriteStruct( fFloat ); }
    virtual size_t          WriteDouble     ( fsDouble_t dDouble )      { return WriteStruct( dDouble ); }
    virtual size_t          WriteBool       ( fsBool_t bBool )          { return WriteStruct( bBool ); }

    template <class type>
    AINLINE bool    ReadStruct( type& buf )
    {
        return Read( &buf, sizeof( type ) ) == sizeof( type );
    }

    template <class type>
    AINLINE bool    WriteStruct( type& buf )
    {
        return Write( &buf, sizeof( type ) ) == sizeof( type );
    }
};

typedef void (*pathCallback_t)( const filePath& path, void *userdata );

// Scanning filtering options.
struct scanFilteringFlags
{
    bool noCurrentDirDesc = true;       // "." entry
    bool noParentDirDesc = true;        // ".." entry
    bool noPatternOnDirs = false;       // if true then all dirs are returned
    bool noSystem = true;
    bool noHidden = true;
    bool noTemporary = true;
    bool noJunctionOrLink = false;
    //
    bool noDirectory = false;
    bool noFile = false;
};

/*===================================================
    CDirectoryIterator (scandir iterator class)

    Flat directory-entry iterator of CFileTranslator instance.
    This class exists to decouple the entry iteration from
    a stack frame, as it is the case for the ScanDirectory
    method.

    Because of this, thís iterator does use more memory than
    the ScanDirectory method.

    Note that this iterator is not recursive. If you want
    to recursively iterate anyway then you have to create
    a recurse function and create an iterator for each
    directory you encounter.
===================================================*/
struct CDirectoryIterator abstract
{
    struct item_info
    {
        filePath filename;
        bool isDirectory;
        filesysAttributes attribs;
    };

    virtual                 ~CDirectoryIterator( void )
    {}

    virtual void            Rewind( void ) = 0;
    virtual bool            Next( item_info& infoOut ) = 0;
};

/*===================================================
    filesysPathProcessMode (enum descriptor)

    The path process mode decides how to look up filesystem
    entries based on provided paths. Traditionally FileSystem
    used the DISTINGUISHED model where if a path was looking
    like a file path it only did process files.
===================================================*/
enum class filesysPathProcessMode
{
    DISTINGUISHED,      // path is either file or directory and no mixing
    AMBIVALENT_FILE     // path is either directory or any if file-looking
};

// Ability to rewrite the path access logic of certain translator methods.
enum class filesysPathOperatingMode
{
    DEFAULT,            // process it like the path describes.
    FILE_ONLY,          // only process if describes a file.
    DIR_ONLY            // only process if describes a directory.
};

/*===================================================
    CFileTranslator (directory class)

    A file translator is an access point to filesystems on the local
    filesystem, the network or archives. Before destroying this, all files
    created by this have to be previously destroyed.

    It resides in a root directory and can change it's current directory.
    All these directories are locked for deletion for security reasons.
===================================================*/
class CFileTranslator abstract
{
public:
    virtual                 ~CFileTranslator( void )
    {}

    /*===================================================
        CFileTranslator::CreateDir

        Arguments:
            path - target path for directory creation
            opMode - how to process the path
            createParentDirs - spawning parent dirs if true
        Purpose:
            Attempts to create the directory tree pointed at by
            path. Returns whether the operation was successful.
            It creates all directory along the way, so if path
            is valid, the operation will most-likely succeed.
    ===================================================*/
    virtual bool            CreateDir( const char *path, filesysPathOperatingMode opMode = filesysPathOperatingMode::DIR_ONLY, bool createParentDirs = true ) = 0;
    virtual bool            CreateDir( const wchar_t *path, filesysPathOperatingMode opMode = filesysPathOperatingMode::DIR_ONLY, bool createParentDirs = true ) = 0;
    virtual bool			CreateDir( const char8_t *path, filesysPathOperatingMode opMode = filesysPathOperatingMode::DIR_ONLY, bool createParentDirs = true ) = 0;
    AINLINE bool            CreateDir( const filePath& path, filesysPathOperatingMode opMode = filesysPathOperatingMode::DIR_ONLY, bool createParentDirs = true )
    {
        return filePath_dispatch( path, [&] ( auto path ) { return CreateDir( path, opMode, createParentDirs ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            CreateDir( const eir::String <charType, stringArgs...>& path, filesysPathOperatingMode opMode = filesysPathOperatingMode::DIR_ONLY, bool createParentDirs = true )
    {
        return CreateDir( path.GetConstString(), opMode, createParentDirs );
    }

    /*===================================================
        CFileTranslator::Open

        Arguments:
            path - target path to attempt access to
            mode - allow reading, writing, seek-location, etc
            flags - extended mode for opening file
        Purpose:
            Attempt to access resources located at path. The access type
            is described by mode. If the operation fails, nullptr is returned.
            Failure is either caused due to locks set by the filesystem
            or by an invalid path or invalid mode descriptor.
    ===================================================*/
    virtual CFile*          Open( const char *path, const filesysOpenMode& mode, eFileOpenFlags flags = FILE_FLAG_NONE ) = 0;
    virtual CFile*          Open( const wchar_t *path, const filesysOpenMode& mode, eFileOpenFlags flags = FILE_FLAG_NONE ) = 0;
    virtual CFile*			Open( const char8_t *path, const filesysOpenMode& mode, eFileOpenFlags flags = FILE_FLAG_NONE ) = 0;
    AINLINE CFile*          Open( const filePath& path, const filesysOpenMode& mode, eFileOpenFlags flags = FILE_FLAG_NONE )
    {
        return filePath_dispatch( path, [&]( auto path ) { return Open( path, mode, flags ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE CFile*          Open( const eir::String <charType, stringArgs...>& path, const filesysOpenMode& mode, eFileOpenFlags flags = FILE_FLAG_NONE )
    {
        return Open( path.GetConstString(), mode, flags );
    }

    // Helper using the ANSI C string mode descriptor.
    template <typename modeCharType>
    AINLINE CFile*      Open( const char *path, const modeCharType *mode, eFileOpenFlags flags = FILE_FLAG_NONE )
    {
        filesysOpenMode openMode;
        bool gotMode = FileSystem::ParseOpenMode( mode, openMode );

        return ( gotMode ? Open( path, openMode, flags ) : nullptr );
    }
    template <typename modeCharType>
    AINLINE CFile*      Open( const wchar_t *path, const modeCharType *mode, eFileOpenFlags flags = FILE_FLAG_NONE )
    {
        filesysOpenMode openMode;
        bool gotMode = FileSystem::ParseOpenMode( mode, openMode );

        return ( gotMode ? Open( path, openMode, flags ) : nullptr );
    }
    template <typename modeCharType>
    AINLINE CFile*      Open( const char8_t *path, const modeCharType *mode, eFileOpenFlags flags = FILE_FLAG_NONE )
    {
        filesysOpenMode openMode;
        bool gotMode = FileSystem::ParseOpenMode( mode, openMode );

        return ( gotMode ? Open( path, openMode, flags ) : nullptr );
    }
    AINLINE CFile*          Open( const filePath& path, const filePath& mode, eFileOpenFlags flags = FILE_FLAG_NONE )
    {
        return filePath_dispatch( path,
            [&] ( auto path )
            {
                typedef typename resolve_type <decltype(path)>::type charType;

                filePath modeLink( mode );

                modeLink.transform_to <charType> ();

                return Open( path, modeLink.to_char <charType> (), flags );
            }
        );
    }
    template <typename charType, typename... fstringArgs, typename... sstringArgs>
    AINLINE CFile*          Open( const eir::String <charType, fstringArgs...>& path, const eir::String <charType, sstringArgs...>& mode, eFileOpenFlags flags = FILE_FLAG_NONE )
    {
        return Open( path.GetConstString(), mode.GetConstString(), flags );
    }

    /*===================================================
        CFileTranslator::Exists

        Arguments:
            path - target path
        Purpose:
            Returns whether the resource at path exists.
    ===================================================*/
    virtual bool            Exists( const char *path ) const = 0;
    virtual bool            Exists( const wchar_t *path ) const = 0;
    virtual bool			Exists( const char8_t *path ) const = 0;
    AINLINE bool            Exists( const filePath& path ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return Exists( path ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            Exists( const eir::String <charType, stringArgs...>& path ) const
    {
        return Exists( path.GetConstString() );
    }

    /*===================================================
        CFileTranslator::Delete

        Arguments:
            path - target path
            opMode - how to process the path
        Purpose:
            Attempts to delete the resources located at path. If it is a single
            resource, it is deleted. If it is a directory, all contents are
            recursively deleted and finally the diretory entry itself. If any
            resource fails to be deleted, false is returned.
    ===================================================*/
    virtual bool            Delete( const char *path, filesysPathOperatingMode opMode = filesysPathOperatingMode::DEFAULT ) = 0;
    virtual bool            Delete( const wchar_t *path, filesysPathOperatingMode opMode = filesysPathOperatingMode::DEFAULT ) = 0;
    virtual bool			Delete( const char8_t *path, filesysPathOperatingMode opMode = filesysPathOperatingMode::DEFAULT ) = 0;
    AINLINE bool            Delete( const filePath& path, filesysPathOperatingMode opMode = filesysPathOperatingMode::DEFAULT )
    {
        return filePath_dispatch( path, [&] ( auto path ) { return Delete( path, opMode ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            Delete( const eir::String <charType, stringArgs...>& path )
    {
        return Delete( path.GetConstString() );
    }

    /*===================================================
        CFileTranslator::Copy

        Arguments:
            src - location of the source resource
            dst - location to copy the resource to
        Purpose:
            Creates another copy of the resource pointed at by src
            at the dst location. Returns whether the operation
            was successful.
    ===================================================*/
    virtual bool            Copy( const char *src, const char *dst ) = 0;
    virtual bool            Copy( const wchar_t *src, const wchar_t *dst ) = 0;
    virtual bool			Copy( const char8_t *src, const char8_t *dst ) = 0;
    AINLINE bool            Copy( const filePath& src, const filePath& dst )
    {
        return filePath_dispatch( src,
            [&] ( auto src )
            {
                typedef typename resolve_type <decltype(src)>::type charType;

                filePath dstLink( dst );

                dstLink.transform_to <charType> ();

                return Copy( src, dstLink.to_char <charType> () );
            }
        );
    }
    template <typename charType, typename... fstringArgs, typename... sstringArgs>
    AINLINE bool            Copy( const eir::String <charType, fstringArgs...>& src, const eir::String <charType, sstringArgs...>& dst )
    {
        return Copy( src.GetConstString(), dst.GetConstString() );
    }

    /*===================================================
        CFileTranslator::Rename

        Arguments:
            src - location of the source resource
            dst - location to move the resource to
        Purpose:
            Moves the resource pointed to by src to dst location.
            Returns whether the operation was successful.
            If the file at dst does already exist then the operation
            will fail.
    ===================================================*/
    virtual bool            Rename( const char *src, const char *dst ) = 0;
    virtual bool            Rename( const wchar_t *src, const wchar_t *dst ) = 0;
    virtual bool			Rename( const char8_t *src, const char8_t *dst ) = 0;
    AINLINE bool            Rename( const filePath& src, const filePath& dst )
    {
        return filePath_dispatch( src,
            [&] ( auto src )
            {
                typedef typename resolve_type <decltype(src)>::type charType;

                filePath dstLink( dst );

                dstLink.transform_to <charType> ();

                return Rename( src, dstLink.to_char <charType> () );
            }
        );
    }
    template <typename charType, typename... fstringArgs, typename... sstringArgs>
    AINLINE bool            Rename( const eir::String <charType, fstringArgs...>& src, const eir::String <charType, sstringArgs...>& dst )
    {
        return Rename( src.GetConstString(), dst.GetConstString() );
    }

    /*===================================================
        CFileTranslator::Size

        Arguments:
            path - path of the query resource
        Purpose:
            Returns the size of the resource at path. The result
            is zero if an error occurred.
    ===================================================*/
    virtual size_t          Size( const char *path ) const = 0;
    virtual size_t          Size( const wchar_t *path ) const = 0;
    virtual size_t			Size( const char8_t *path ) const = 0;
    AINLINE size_t          Size( const filePath& path ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return Size( path ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE size_t          Size( const eir::String <charType, stringArgs...>& path ) const
    {
        return Size( path.GetConstString() );
    }

    /*===================================================
        CFileTranslator::QueryStats

        Arguments:
            path - path of the query resource
            statsOut - file system statistics
        Purpose:
            Attempts to receive resource meta information at path.
            Returns false if operation failed; then statsOut remains
            unchanged.
    ===================================================*/
    virtual bool            QueryStats( const char *path, filesysStats& statsOut ) const = 0;
    virtual bool            QueryStats( const wchar_t *path, filesysStats& statsOut ) const = 0;
    virtual bool			QueryStats( const char8_t *path, filesysStats& statsOut ) const = 0;
    AINLINE bool            QueryStats( const filePath& path, filesysStats& statsOut ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return QueryStats( path, statsOut ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            QueryStats( const eir::String <charType, stringArgs...>& path, filesysStats& statsOut ) const
    {
        return QueryStats( path.GetConstString(), statsOut );
    }

    /*===================================================
        CFileTranslator::IsCaseSensitive

        Purpose:
            Returns true if path resolution operations inside
            this translator are performed case-sensitively, false
            otherwise.
    ===================================================*/
    virtual bool            IsCaseSensitive( void ) const = 0;

    /*===================================================
        CFileTranslator::SetOutbreakEnabled

        Arguments:
            enabled - boolean flag
        Purpose:
            Switches the outbreak from the translator root on
            or off. If false then all path resolution functions
            reject translating paths that attempt to leave
            translator root. In general, an outbreak system
            translator has full access to the local computer fs.
    ===================================================*/
    virtual void            SetOutbreakEnabled( bool enabled ) = 0;

    /*===================================================
        CFileTranslator::IsOutbreakEnabled

        Purpose:
            Returns true if path resolution can break out of the
            translator root. By default translators are created
            with outbreak disabled.
    ===================================================*/
    virtual bool            IsOutbreakEnabled( void ) const = 0;

    /*==============================================================
      * Path Translation functions

        Any path provided to these functions is valid as long as it
        it follows the rules of the underlying file system, be it
        virtual or platform based.

        Paths may not leave the translator's root directory if outbreak
        mode is disabled.
    ==============================================================*/

    /*===================================================
        CFileTranslator::GetFullPathNodesFromRoot

        Arguments:
            path - target path based on root of trans
            nodes - (out) resulting node path
        Purpose:
            Attempts to parse the provided path into an unique absolute path.
            In a Windows OS, the path starts from the root of the drive.
            The path is not constructed, but rather separated into the list
            pointed to at variable tree. Input path is based against the root
            of this translator: path itself is located on the current directory
            but the current directory is also baked into the result.

            The resulting node path is in normal form which is the shortest
            possible path in the requested representation.
    ===================================================*/
    virtual bool            GetFullPathNodesFromRoot( const char *path, normalNodePath& nodes ) const = 0;
    virtual bool            GetFullPathNodesFromRoot( const wchar_t *path, normalNodePath& nodes ) const = 0;
    virtual bool            GetFullPathNodesFromRoot( const char8_t *path, normalNodePath& nodes ) const = 0;
    AINLINE bool            GetFullPathNodesFromRoot( const filePath& path, normalNodePath& nodes ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return GetFullPathNodesFromRoot( path, nodes ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            GetFullPathNodesFromRoot( const eir::String <charType, stringArgs...>& path, normalNodePath& nodes ) const
    {
        return GetFullPathNodesFromRoot( path.GetConstString(), nodes );
    }

    /*===================================================
        CFileTranslator::GetFullPathNodes

        Arguments:
            path - target path
            nodes - (out) resulting node path
        Purpose:
            Does the same as GetFullPathNodesFromRoot, but the path is based
            against the current directory of the translator: the current
            directory is not baked into the result.
    ===================================================*/
    virtual bool            GetFullPathNodes( const char *path, normalNodePath& nodes ) const = 0;
    virtual bool            GetFullPathNodes( const wchar_t *path, normalNodePath& nodes ) const = 0;
    virtual bool            GetFullPathNodes( const char8_t *path, normalNodePath& nodes ) const = 0;
    AINLINE bool            GetFullPathNodes( const filePath& path, normalNodePath& nodes ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return GetFullPathNodes( path, nodes ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            GetFullPathNodes( const eir::String <charType, stringArgs...>& path, normalNodePath& nodes ) const
    {
        return GetFullPathNodes( path.GetConstString(), nodes );
    }

    /*===================================================
        CFileTranslator::GetRelativePathNodesFromRoot

        Arguments:
            path - target path
            nodes - (out) resulting node path
        Purpose:
            Attempts to parse the provided path to a unique representation
            based on the root directory of the translator. The resulting path
            can be considered an unique representation for this translator.
            The resulting path is split into it's components at the tree list.
    ===================================================*/
    virtual bool            GetRelativePathNodesFromRoot( const char *path, normalNodePath& nodes ) const = 0;
    virtual bool            GetRelativePathNodesFromRoot( const wchar_t *path, normalNodePath& nodes ) const = 0;
    virtual bool			GetRelativePathNodesFromRoot( const char8_t *path, normalNodePath& nodes ) const = 0;
    AINLINE bool            GetRelativePathNodesFromRoot( const filePath& path, normalNodePath& nodes ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return GetRelativePathNodesFromRoot( path, nodes ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            GetRelativePathNodesFromRoot( const eir::String <charType, stringArgs...>& path, normalNodePath& nodes ) const
    {
        return GetRelativePathNodesFromRoot( path.GetConstString(), nodes );
    }

    /*===================================================
        CFileTranslator::GetRelativePathNodes

        Arguments:
            path - target path
            nodes - (out) resulting node path
        Purpose:
            Does the same as GetRelativePathNodesFromRoot, but bases the
            resulting path on the translator's current directory.
    ===================================================*/
    virtual bool            GetRelativePathNodes( const char *path, normalNodePath& nodes ) const = 0;
    virtual bool            GetRelativePathNodes( const wchar_t *path, normalNodePath& nodes ) const = 0;
    virtual bool			GetRelativePathNodes( const char8_t *path, normalNodePath& nodes ) const = 0;
    AINLINE bool            GetRelativePathNodes( const filePath& path, normalNodePath& nodes ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return GetRelativePathNodes( path, nodes ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            GetRelativePathNodes( const eir::String <charType, stringArgs...>& path, normalNodePath& nodes ) const
    {
        return GetRelativePathNodes( path.GetConstString(), nodes );
    }

    /*===================================================
        CFileTranslator::GetFullPathFromRoot

        Arguments:
            path - target path
            allowFile - if false only directory paths are returned
            output - write location for output path
        Purpose:
            Executes GetFullPathNodesFromRoot and parses it's output
            into a full (system) path. That is to convert path into a
            full (system) path based on the translator's root.
    ===================================================*/
    virtual bool            GetFullPathFromRoot( const char *path, bool allowFile, filePath& output ) const = 0;
    virtual bool            GetFullPathFromRoot( const wchar_t *path, bool allowFile, filePath& output ) const = 0;
    virtual bool			GetFullPathFromRoot( const char8_t *path, bool allowFile, filePath& output ) const = 0;
    AINLINE bool            GetFullPathFromRoot( const filePath& path, bool allowFile, filePath& output ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return GetFullPathFromRoot( path, allowFile, output ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            GetFullPathFromRoot( const eir::String <charType, stringArgs...>& path, bool allowFile, filePath& output ) const
    {
        return GetFullPathFromRoot( path.GetConstString(), allowFile, output );
    }

    /*===================================================
        CFileTranslator::GetFullPath

        Arguments:
            path - target path
            allowFile - if false only directory paths are returned
            output - write location for output path
        Purpose:
            Executes GetFullPathNodes and parses it's output into a full
            (system) path. This translation is based on the translator's
            current directory.
    ===================================================*/
    virtual bool            GetFullPath( const char *path, bool allowFile, filePath& output ) const = 0;
    virtual bool            GetFullPath( const wchar_t *path, bool allowFile, filePath& output ) const = 0;
    virtual bool			GetFullPath( const char8_t *path, bool allowFile, filePath& output ) const = 0;
    AINLINE bool            GetFullPath( const filePath& path, bool allowFile, filePath& output ) const
    {
        return filePath_dispatch( path, [&]( auto path ) { return GetFullPath( path, allowFile, output ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            GetFullPath( const eir::String <charType, stringArgs...>& path, bool allowFile, filePath& output ) const
    {
        return GetFullPath( path.GetConstString(), allowFile, output );
    }

    /*===================================================
        CFileTranslator::GetRelativePathFromRoot

        Arguments:
            path - target path
            allowFile - if false only directory paths are returned
            output - write location for output path
        Purpose:
            Executes GetRelativePathNodesFromRoot and parses it's output
            into a path relative to the translator's root directory.
    ===================================================*/
    virtual bool            GetRelativePathFromRoot( const char *path, bool allowFile, filePath& output ) const = 0;
    virtual bool            GetRelativePathFromRoot( const wchar_t *path, bool allowFile, filePath& output ) const = 0;
    virtual bool			GetRelativePathFromRoot( const char8_t *path, bool allowFile, filePath& output ) const = 0;
    AINLINE bool            GetRelativePathFromRoot( const filePath& path, bool allowFile, filePath& output ) const
    {
        return filePath_dispatch( path, [&]( auto path ) { return GetRelativePathFromRoot( path, allowFile, output ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            GetRelativePathFromRoot( const eir::String <charType, stringArgs...>& path, bool allowFile, filePath& output ) const
    {
        return GetRelativePathFromRoot( path.GetConstString(), allowFile, output );
    }

    /*===================================================
        CFileTranslator::GetRelativePath

        Arguments:
            path - target path
            allowFile - if false only directory paths are returned
            output - write location for output path
        Purpose:
            Executes GetRelativePathNodes and parses it's output
            into a path relative to the translator's current directory.
    ===================================================*/
    virtual bool            GetRelativePath( const char *path, bool allowFile, filePath& output ) const = 0;
    virtual bool            GetRelativePath( const wchar_t *path, bool allowFile, filePath& output ) const = 0;
    virtual bool			GetRelativePath( const char8_t *path, bool allowFile, filePath& output ) const = 0;
    AINLINE bool            GetRelativePath( const filePath& path, bool allowFile, filePath& output ) const
    {
        return filePath_dispatch( path, [&]( auto path ) { return GetRelativePath( path, allowFile, output ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            GetRelativePath( const eir::String <charType, stringArgs...>& path, bool allowFile, filePath& output ) const
    {
        return GetRelativePath( path.GetConstString(), allowFile, output );
    }

    /*===================================================
        CFileTranslator::ChangeDirectory

        Arguments:
            path - target path
        Purpose:
            Attempts to change the current directory of the translator.
            Returns whether the operation succeeded.
    ===================================================*/
    virtual bool            ChangeDirectory( const char *path ) = 0;
    virtual bool            ChangeDirectory( const wchar_t *path ) = 0;
    virtual bool			ChangeDirectory( const char8_t *path ) = 0;
    AINLINE bool            ChangeDirectory( const filePath& path )
    {
        return filePath_dispatch( path, [&]( auto path ) { return ChangeDirectory( path ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool            ChangeDirectory( const eir::String <charType, stringArgs...>& path )
    {
        return ChangeDirectory( path.GetConstString() );
    }

    /*===================================================
        CFileTranslator::GetDirectory

        Arguments:
            output - structure to save path at
        Purpose:
            Returns the current directory of the translator.
            It is a relative directory path starting from the
            translator root.
    ===================================================*/
    virtual filePath        GetDirectory( void ) const = 0;

    /*===================================================
        CFileTranslator::SetPathProcessMode

        Arguments:
            mode - the process mode to set
        Purpose:
            Changes the path process mode of this file translator.
            If the change is not possible then an exception is
            thrown. By default the process mode is DISTINGUISHED.
            See the details of filesysPathProcessMode for more
            information.
    ===================================================*/
    virtual void            SetPathProcessMode( filesysPathProcessMode mode ) = 0;

    /*===================================================
        CFileTranslator::GetPathProcessMode

        Purpose:
            Returns the path process mode of this file translator.
            By default DISTINGUISHED is returned.
    ===================================================*/
    virtual filesysPathProcessMode  GetPathProcessMode( void ) const noexcept = 0;

    /*===================================================
        CFileTranslator::ScanDirectory

        Arguments:
            directory - location of the scan
            wildcard - pattern to check filenames onto
            recurse - if true then sub directories are scanned, too
            dirCallback - executed for every sub directory found
            fileCallback - executed for every file found
            userdata - passed to every callback
        Purpose:
            Scans the designated directory for files and directories.
            The callback is passed the full path of the found resource
            and the userdata.
    ===================================================*/
    virtual void            ScanDirectory( const char *directory, const char *wildcard, bool recurse,
                                pathCallback_t dirCallback,
                                pathCallback_t fileCallback,
                                void *userdata ) const = 0;
    virtual void            ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse,
                                pathCallback_t dirCallback,
                                pathCallback_t fileCallback,
                                void *userdata ) const = 0;
    virtual void			ScanDirectory( const char8_t *directory, const char8_t *wildcard, bool recurse,
                                pathCallback_t dirCallback,
                                pathCallback_t fileCallback,
                                void *userdata ) const = 0;
    AINLINE void            ScanDirectory( const filePath& directory, const filePath& wildcard, bool recurse,
                                pathCallback_t dirCallback,
                                pathCallback_t fileCallback,
                                void *userdata ) const
    {
        filePath_dispatchTrailing( directory, wildcard,
            [&] ( auto directory, auto wildcard )
            {
                ScanDirectory( directory, wildcard, recurse, dirCallback, fileCallback, userdata );
            }
        );
    }

    // Helpers with lambdas, because lambdas are really, really good.
private:
    template <typename dirCallbackType, typename fileCallbackType>
    struct ScanDirectory_lambdaHelper
    {
        struct combined_userdata
        {
            inline combined_userdata( dirCallbackType dir_cb, fileCallbackType file_cb ) : dir_cb( std::move( dir_cb ) ), file_cb( std::move( file_cb ) )
            {
                return;
            }

            dirCallbackType dir_cb;
            fileCallbackType file_cb;
        };

        AINLINE static void fileCallback( const filePath& path, void *ud )
        {
            combined_userdata *comb_cb = (combined_userdata*)ud;

            comb_cb->file_cb( path );
        }

        AINLINE static void dirCallback( const filePath& path, void *ud )
        {
            combined_userdata *comb_cb = (combined_userdata*)ud;

            comb_cb->dir_cb( path );
        }
    };

    template <typename callable_obj>
    struct is_lambda_trait
    {
        static const bool value = std::is_invocable_r <void, callable_obj, const filePath&>::value;
    };

    template <typename T>
    using is_lambda = std::enable_if <is_lambda_trait <T>::value>;

public:
    template <typename charType, typename dirCallbackType, typename fileCallbackType, typename = typename is_lambda <dirCallbackType>::type, typename = typename is_lambda <fileCallbackType>::type>
    AINLINE void            ScanDirectory( const charType *directory, const charType *wildcard, bool recurse,
                                dirCallbackType dirCallback,
                                fileCallbackType fileCallback ) const
    {
        typedef ScanDirectory_lambdaHelper <dirCallbackType, fileCallbackType> lambdaHelper;

        typename lambdaHelper::combined_userdata comb_ud( std::move( dirCallback ), std::move( fileCallback ) );

        ScanDirectory(
            directory, wildcard, recurse,
            lambdaHelper::dirCallback,
            lambdaHelper::fileCallback,
            &comb_ud
        );
    }

    template <typename dirCallbackType, typename fileCallbackType, typename = typename is_lambda <dirCallbackType>::type, typename = typename is_lambda <fileCallbackType>::type>
    AINLINE void            ScanDirectory( const filePath& directory, const filePath& wildcard, bool recurse,
                                dirCallbackType dirCallback,
                                fileCallbackType fileCallback ) const
    {
        filePath_dispatchTrailing( directory, wildcard,
            [&] ( auto directory, auto wildcard )
            {
                ScanDirectory( directory, wildcard, recurse, std::move( dirCallback ), std::move( fileCallback ) );
            }
        );
    }

private:
    template <typename callbackType>
    struct ScanDirectory_singleLambdaCallbackHelper
    {
        struct meta_object
        {
            inline meta_object( pathCallback_t path_cb, void *path_ud, callbackType&& cb ) : path_cb( path_cb ), cb( std::move( cb ) ), path_ud( path_ud )
            {
                return;
            }

            pathCallback_t path_cb;
            callbackType cb;
            void *path_ud;
        };

        AINLINE static void lambda_callback( const filePath& path, void *ud )
        {
            meta_object *meta = (meta_object*)ud;

            meta->cb( path );
        }

        AINLINE static void reg_callback( const filePath& path, void *ud )
        {
            meta_object *meta = (meta_object*)ud;

            meta->path_cb( path, meta->path_ud );
        }
    };

public:
    template <typename charType, typename dirCallbackType, typename = typename is_lambda <dirCallbackType>::type>
    AINLINE void            ScanDirectory( const charType *directory, const charType *wildcard, bool recurse,
                                dirCallbackType dirCallback,
                                pathCallback_t fileCallback,
                                void *file_ud ) const
    {
        typedef ScanDirectory_singleLambdaCallbackHelper <dirCallbackType> lambdaHelper;

        typename lambdaHelper::meta_object meta( fileCallback, file_ud, std::move( dirCallback ) );

        ScanDirectory(
            directory, wildcard, recurse,
            lambdaHelper::lambda_callback,
            ( fileCallback ? lambdaHelper::reg_callback : nullptr ),
            &meta
        );
    }

    template <typename dirCallbackType, typename = typename is_lambda <dirCallbackType>::type>
    AINLINE void            ScanDirectory( const filePath& directory, const filePath& wildcard, bool recurse,
                                dirCallbackType dirCallback,
                                pathCallback_t fileCallback,
                                void *file_ud ) const
    {
        filePath_dispatchTrailing( directory, wildcard,
            [&]( auto directory, auto wildcard )
        {
            ScanDirectory( directory, wildcard, recurse, std::move( dirCallback ), fileCallback, file_ud );
        });
    }

    template <typename charType, typename fileCallbackType, typename = typename is_lambda <fileCallbackType>::type>
    AINLINE void            ScanDirectory( const charType *directory, const charType *wildcard, bool recurse,
                                pathCallback_t dirCallback,
                                fileCallbackType fileCallback,
                                void *dir_ud ) const
    {
        typedef ScanDirectory_singleLambdaCallbackHelper <fileCallbackType> lambdaHelper;

        typename lambdaHelper::meta_object meta( dirCallback, dir_ud, std::move( fileCallback ) );

        ScanDirectory(
            directory, wildcard, recurse,
            ( dirCallback ? lambdaHelper::reg_callback : nullptr ),
            lambdaHelper::lambda_callback,
            &meta
        );
    }

    template <typename fileCallbackType, typename = typename is_lambda <fileCallbackType>::type>
    AINLINE void            ScanDirectory( const filePath& directory, const filePath& wildcard, bool recurse,
                                pathCallback_t dirCallback,
                                fileCallbackType fileCallback,
                                void *dir_ud ) const
    {
        filePath_dispatchTrailing( directory, wildcard,
            [&]( auto directory, auto wildcard )
        {
            ScanDirectory( directory, wildcard, recurse, dirCallback, std::move( fileCallback ), dir_ud );
        });
    }

    // These functions are easy helpers for ScanDirectory.
    virtual void            GetDirectories( const char *path, const char *wildcard, bool recurse, dirNames& output ) const = 0;
    virtual void            GetDirectories( const wchar_t *path, const wchar_t *wildcard, bool recurse, dirNames& output ) const = 0;
    virtual void			GetDirectories( const char8_t *path, const char8_t *wildcard, bool recurse, dirNames& output ) const = 0;
    AINLINE void            GetDirectories( const filePath& path, const filePath& wildcard, bool recurse, dirNames& output ) const
    {
        filePath_dispatchTrailing( path, wildcard,
            [&] ( auto path, auto wildcard )
            {
                GetDirectories( path, wildcard, recurse, output );
            }
        );
    }

    virtual void            GetFiles( const char *path, const char *wildcard, bool recurse, dirNames& output ) const = 0;
    virtual void            GetFiles( const wchar_t *path, const wchar_t *wildcard, bool recurse, dirNames& output ) const = 0;
    virtual void			GetFiles( const char8_t *path, const char8_t *wildcard, bool recurse, dirNames& output ) const = 0;
    AINLINE void            GetFiles( const filePath& path, const filePath& wildcard, bool recurse, dirNames& output ) const
    {
        filePath_dispatchTrailing( path, wildcard,
            [&] ( auto path, auto wildcard )
            {
                GetDirectories( path, wildcard, recurse, output );
            }
        );
    }

    /*===================================================
        CFileTranslator::BeginDirectoryListing

        Arguments:
            path - directory listing location
            wildcard - pattern matching for entry names (glob)
            filter_flags - specific filtering options
        Purpose:
            If successful, returns an iterator over file entries
            of a directory on this translator. Can be used to
            traverse the entire filesystem tree, without the
            need of deep-stack traversal such as in
            ScanDirectory. You must destroy the iterator using
            delete when you finished using it.
    ===================================================*/
    virtual CDirectoryIterator* BeginDirectoryListing( const char *path, const char *wildcard, const scanFilteringFlags& filter_flags ) const = 0;
    virtual CDirectoryIterator* BeginDirectoryListing( const wchar_t *path, const wchar_t *wildcard, const scanFilteringFlags& filter_flags ) const = 0;
    virtual CDirectoryIterator* BeginDirectoryListing( const char8_t *path, const char8_t *wildcard, const scanFilteringFlags& filter_flags ) const = 0;
    AINLINE CDirectoryIterator* BeginDirectoryListing( const filePath& path, const filePath& wildcard, const scanFilteringFlags& filter_flags ) const
    {
        return filePath_dispatchTrailing( path, wildcard,
            [&]( auto path, auto wildcard )
            {
                return BeginDirectoryListing( path, wildcard, filter_flags );
            }
        );
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER

// Not a good idea. See inside.
#include "CFileSystem.common.stl.h"

/*===================================================
    CArchiveTranslator (archive root class)

    This is a special form of CFileTranslator that is
    an archive root. It manages content to-and-from
    the underlying archive.
===================================================*/
class CArchiveTranslator abstract : public virtual CFileTranslator
{
public:
    virtual void            Save( void ) = 0;
};

// Include public extension headers.
#include "CFileSystem.zip.public.h"
#include "CFileSystem.img.public.h"

#include "CFileSystem.rwcb.h"

class CFileSystemInterface
{
public:
    virtual bool                GetSystemRootDescriptor ( const char *path, filePath& descOut ) const = 0;
    virtual bool                GetSystemRootDescriptor ( const wchar_t *path, filePath& descOut ) const = 0;
    virtual bool				GetSystemRootDescriptor	( const char8_t *path, filePath& descOut ) const = 0;
    AINLINE bool                GetSystemRootDescriptor ( const filePath& path, filePath& descOut ) const
    {
        return filePath_dispatch( path, [&]( auto path ) { return GetSystemRootDescriptor( path, descOut ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE bool                GetSystemRootDescriptor ( const eir::String <charType, stringArgs...>& path, filePath& descOut ) const
    {
        return GetSystemRootDescriptor( path.GetConstString(), descOut );
    }

    virtual CFileTranslator*    CreateTranslator        ( const char *path, eDirOpenFlags flags = DIR_FLAG_NONE ) = 0;
    virtual CFileTranslator*    CreateTranslator        ( const wchar_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) = 0;
    virtual CFileTranslator*	CreateTranslator        ( const char8_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) = 0;
    AINLINE CFileTranslator*    CreateTranslator        ( const filePath& path, eDirOpenFlags flags = DIR_FLAG_NONE )
    {
        return filePath_dispatch( path, [&]( auto path ) { return CreateTranslator( path, flags ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE CFileTranslator*    CreateTranslator        ( const eir::String <charType, stringArgs...>& path, eDirOpenFlags flags = DIR_FLAG_NONE )
    {
        return CreateTranslator( path.GetConstString(), flags );
    }

    virtual CFileTranslator*    CreateSystemMinimumAccessPoint  ( const char *path, eDirOpenFlags flags = DIR_FLAG_NONE ) = 0;
    virtual CFileTranslator*    CreateSystemMinimumAccessPoint  ( const wchar_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) = 0;
    virtual CFileTranslator*    CreateSystemMinimumAccessPoint  ( const char8_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) = 0;
    AINLINE CFileTranslator*    CreateSystemMinimumAccessPoint  ( const filePath& path, eDirOpenFlags flags = DIR_FLAG_NONE )
    {
        return filePath_dispatch( path, [&]( auto path ) { return CreateSystemMinimumAccessPoint( path, flags ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE CFileTranslator*    CreateSystemMinimumAccessPoints ( const eir::String <charType, stringArgs...>& path, eDirOpenFlags flags = DIR_FLAG_NONE )
    {
        return CreateSystemMinimumAccessPoint( path.GetConstString(), flags );
    }

    virtual CArchiveTranslator* OpenArchive         ( CFile& file ) = 0;

    virtual CArchiveTranslator* OpenZIPArchive      ( CFile& file ) = 0;
    virtual CArchiveTranslator* CreateZIPArchive    ( CFile& file ) = 0;

    // Standard IMG archive functions that should be used.
    virtual CIMGArchiveTranslatorHandle* OpenIMGArchiveDirect   ( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode = false ) = 0;
    virtual CIMGArchiveTranslatorHandle* CreateIMGArchiveDirect ( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode = false ) = 0;

    virtual CIMGArchiveTranslatorHandle* OpenIMGArchive         ( CFileTranslator *srcRoot, const char *srcPath, bool writeAccess, bool isLiveMode = false ) = 0;
    virtual CIMGArchiveTranslatorHandle* OpenIMGArchive         ( CFileTranslator *srcRoot, const wchar_t *srcPath, bool writeAccess, bool isLiveMode = false ) = 0;
    virtual CIMGArchiveTranslatorHandle* OpenIMGArchive         ( CFileTranslator *srcRoot, const char8_t *srcPath, bool writeAccess, bool isLiveMode = false ) = 0;
    AINLINE CIMGArchiveTranslatorHandle* OpenIMGArchive         ( CFileTranslator *srcRoot, const filePath& srcPath, bool writeAccess, bool isLiveMode = false )
    {
        return filePath_dispatch( srcPath, [&] ( auto srcPath ) { return OpenIMGArchive( srcRoot, srcPath, writeAccess, isLiveMode ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE CIMGArchiveTranslatorHandle* OpenIMGArchive         ( CFileTranslator *srcRoot, const eir::String <charType, stringArgs...>& srcPath, bool writeAccess, bool isLiveMode = false )
    {
        return OpenIMGArchive( srcRoot, srcPath.GetConstString(), writeAccess, isLiveMode );
    }

    virtual CIMGArchiveTranslatorHandle* CreateIMGArchive       ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version, bool isLiveMode = false ) = 0;
    virtual CIMGArchiveTranslatorHandle* CreateIMGArchive       ( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version, bool isLiveMode = false ) = 0;
    virtual CIMGArchiveTranslatorHandle* CreateIMGArchive       ( CFileTranslator *srcRoot, const char8_t *srcPath, eIMGArchiveVersion version, bool isLiveMode = false ) = 0;
    AINLINE CIMGArchiveTranslatorHandle* CreateIMGArchive       ( CFileTranslator *srcRoot, const filePath& srcPath, eIMGArchiveVersion version, bool isLiveMode = false )
    {
        return filePath_dispatch( srcPath, [&]( auto srcPath ) { return CreateIMGArchive( srcRoot, srcPath, version, isLiveMode ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE CIMGArchiveTranslatorHandle* CreateIMGArchive       ( CFileTranslator *srcRoot, const eir::String <charType, stringArgs...>& srcPath, eIMGArchiveVersion version, bool isLiveMode = false )
    {
        return CreateIMGArchive( srcRoot, srcPath.GetConstString(), version, isLiveMode );
    }

    // Special functions for IMG archives that should support compression.
    virtual CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchiveDirect  ( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode = false ) = 0;
    virtual CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchiveDirect( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode = false ) = 0;

    virtual CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive        ( CFileTranslator *srcRoot, const char *srcPath, bool writeAccess, bool isLiveMode = false ) = 0;
    virtual CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive        ( CFileTranslator *srcRoot, const wchar_t *srcPath, bool writeAccess, bool isLiveMode = false ) = 0;
    virtual CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive        ( CFileTranslator *srcRoot, const char8_t *srcPath, bool writeAccess, bool isLiveMode = false ) = 0;
    AINLINE CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive        ( CFileTranslator *srcRoot, const filePath& srcPath, bool writeAccess, bool isLiveMode = false )
    {
        return filePath_dispatch( srcPath, [&]( auto srcPath ) { return OpenCompressedIMGArchive( srcRoot, srcPath, writeAccess, isLiveMode ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive        ( CFileTranslator *srcRoot, const eir::String <charType, stringArgs...>& srcPath, bool writeAccess, bool isLiveMode = false )
    {
        return OpenCompressedIMGArchive( srcRoot, srcPath.GetConstString(), writeAccess, isLiveMode );
    }

    virtual CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive      ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version, bool isLiveMode = false ) = 0;
    virtual CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive      ( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version, bool isLiveMode = false ) = 0;
    virtual CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive      ( CFileTranslator *srcRoot, const char8_t *srcPath, eIMGArchiveVersion version, bool isLiveMode = false ) = 0;
    AINLINE CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive      ( CFileTranslator *srcRoot, const filePath& srcPath, eIMGArchiveVersion version, bool isLiveMode = false )
    {
        return filePath_dispatch( srcPath, [&]( auto srcPath ) { return CreateCompressedIMGArchive( srcRoot, srcPath, version, isLiveMode ); } );
    }
    template <typename charType, typename... stringArgs>
    AINLINE CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive      ( CFileTranslator *srcRoot, const eir::String <charType, stringArgs...>& srcPath, eIMGArchiveVersion version, bool isLiveMode = false )
    {
        return CreateCompressedIMGArchive( srcRoot, srcPath.GetConstString(), version, isLiveMode );
    }

    // Open failure codes in case the Open method of any file translator returns nullptr.
    virtual void                ResetLastTranslatorOpenFailure  ( void ) = 0;
    virtual eFileOpenFailure    GetLastTranslatorOpenFailure    ( void ) = 0;

    virtual CFile*              CreateUserBufferFile( void *bufPtr, size_t bufSize ) = 0;
    virtual CFile*              CreateMemoryFile    ( void ) = 0;
    virtual CFile*              CreateRWCallbackFile( FileSystem::readWriteCallbackInterface *intf ) = 0;

    virtual CFile*              WrapStreamBuffered  ( CFile *stream, bool deleteOnQuit, size_t bufSize = 0 ) = 0;

    virtual CFileTranslator*    CreateRamdisk       ( bool isCaseSensitive ) = 0;

    virtual CFile*              CreateFileIterative ( CFileTranslator *trans, const char *prefix, const char *suffix, unsigned int maxDupl ) = 0;
    virtual CFile*              CreateFileIterative ( CFileTranslator *trans, const wchar_t *prefix, const wchar_t *suffix, unsigned int maxDupl ) = 0;
    virtual CFile*              CreateFileIterative ( CFileTranslator *trans, const char8_t *prefix, const char8_t *suffix, unsigned int maxDupl ) = 0;

    // Insecure, use with caution!
    virtual bool                IsDirectory         ( const char *path ) = 0;
    virtual bool                Exists              ( const char *path ) = 0;
    virtual size_t              Size                ( const char *path ) = 0;
    virtual bool                ReadToBuffer        ( const char *path, fsDataBuffer& output ) = 0;

    // Settings.
    virtual void                SetIncludeAllDirectoriesInScan  ( bool enable ) = 0;
    virtual bool                GetIncludeAllDirectoriesInScan  ( void ) const = 0;

    virtual void                SetDoBufferAllRaw   ( bool enable ) = 0;
    virtual bool                GetDoBufferAllRaw   ( void ) const = 0;
};

namespace FileSystem
{
    // These functions are not for noobs.

    // Reads the file and gives possible patterns to a callback interface.
    // The interface may break the scan through the file and specify the location
    // where the seek should reside at. This function is used by the .zip extension
    // to find where the .zip stream starts at.
    template <class t, typename F>
    inline bool MappedReaderReverse( CFile& file, F& f )
    {
        t buf;
        long off;

        fsOffsetNumber_t curSeek = file.GetSizeNative();

        curSeek -= (fsOffsetNumber_t)sizeof( buf );

        file.SeekNative( curSeek, SEEK_SET );

        while ( true )
        {
            size_t readCount = file.Read( &buf, sizeof( buf ) );

            if ( f.Perform( buf, readCount, off ) )
            {
                file.SeekNative( curSeek + off, SEEK_SET );
                return true;
            }

            curSeek -= 1;

            if ( curSeek < 0 )
            {
                break;
            }

            file.SeekNative( curSeek, SEEK_SET );
        }

        return false;
    }

    // Memory friendly file copy function.
    inline void StreamCopy( CFile& src, CFile& dst )
    {
        char buf[8096];

#ifdef FILESYSTEM_STREAM_PARANOIA
        // Check for nasty implementation bugs.
        size_t actualFileSize = src.GetSize() - src.Tell();
        size_t addedFileSize = 0;
#endif //FILESYSTEM_STREAM_PARANOIA

        while ( !src.IsEOF() )
        {
            size_t rb = src.Read( buf, sizeof( buf ) );
#ifdef FILESYSTEM_STREAM_PARANOIA
            size_t writtenBytes =
#endif
            dst.Write( buf, rb );

#ifdef FILESYSTEM_STREAM_PARANOIA
            assert( rb == writtenBytes );

            addedFileSize += rb;
#endif //FILESYSTEM_STREAM_PARANOIA
        }

#ifdef FILESYSTEM_STREAM_PARANOIA
        if ( actualFileSize != addedFileSize )
        {
            __asm int 3
        }

        assert( actualFileSize == addedFileSize );
#endif //FILESYSTEM_STREAM_PARANOIA
    }

    // Memory friendly file copy function which only copies 'cnt' bytes
    // from src to dst.
    inline void StreamCopyCount( CFile& src, CFile& dst, fsOffsetNumber_t cnt )
    {
        if ( cnt < 0 )
            return;

        size_t toRead;
        char buf[8096];

        while ( ( toRead = (size_t)std::min( (fsOffsetNumber_t)sizeof( buf ), cnt ) ) != 0 )
        {
            size_t rb = src.Read( buf, toRead );

            if ( rb == 0 )
            {
                break;
            }

            cnt -= rb;

            dst.Write( buf, rb );
        }
    }

    // Function which is used to parse a source stream into
    // an appropriate dst representation. It reads the src stream
    // into a temporary buffer and the callback structure may modify it.
    template <class cb>
    inline void StreamParser( CFile& src, CFile& dst, cb& f )
    {
        char buf[8096];
        char outBuf[16192];
        size_t outSize;

        for (;;)
        {
            size_t rb = src.Read( buf, sizeof( buf ) );

            bool eof = src.IsEOF();
            f.prepare( buf, rb, eof );

            for (;;)
            {
                bool cnt = f.parse( outBuf, sizeof( outBuf ), outSize );
                dst.Write( outBuf, outSize );

                if ( !cnt )
                    break;
            }

            if ( eof )
                break;
        }

        dst.SetSeekEnd();
    }

    // Parses the stream same as StreamParser, but limited to 'cnt' bytes of the
    // source stream.
    template <typename cb>
    inline void StreamParserCount( CFile& src, CFile& dst, fsOffsetNumber_t cnt, cb& f )
    {
        if ( cnt < 0 )
            return;

        auto ucnt = (std::make_unsigned <fsOffsetNumber_t>::type)cnt;

        char buf[8096];
        char outBuf[16192];
        size_t outSize;
        size_t toRead;

        for (;;)
        {
            bool eof;

            if ( sizeof( buf ) >= ucnt )
            {
                eof = true;

                toRead = (size_t)ucnt;
            }
            else
            {
                eof = false;

                toRead = sizeof( buf );
                ucnt -= toRead;
            }

            size_t rb = src.Read( buf, toRead );

            f.prepare( buf, rb, eof );

            for (;;)
            {
                bool continu = f.parse( outBuf, sizeof( outBuf ), outSize );
                dst.Write( outBuf, outSize );

                if ( !continu )
                    break;
            }

            if ( eof )
                break;
        }

        dst.SetSeekEnd();
    }

    // Helpful things.
    template <eir::CharacterType charType>
    AINLINE const charType* FileGetReadModeBinary( void )
    {
        if constexpr ( std::is_same <charType, char>::value )
        {
            return "rb";
        }
        else if constexpr ( std::is_same <charType, wchar_t>::value )
        {
            return L"rb";
        }
        else if constexpr ( std::is_same <charType, char8_t>::value )
        {
            return u8"rb";
        }
        else if constexpr ( std::is_same <charType, char16_t>::value )
        {
            return u"rb";
        }
        else if constexpr ( std::is_same <charType, char32_t>::value )
        {
            return U"rb";
        }
    }

    template <eir::CharacterType charType>
    AINLINE const charType* FileGetWriteModeBinary( void )
    {
        if constexpr ( std::is_same <charType, char>::value )
        {
            return "wb";
        }
        else if constexpr ( std::is_same <charType, wchar_t>::value )
        {
            return L"wb";
        }
        else if constexpr ( std::is_same <charType, char8_t>::value )
        {
            return u8"wb";
        }
        else if constexpr ( std::is_same <charType, char32_t>::value )
        {
            return U"wb";
        }
        else if constexpr ( std::is_same <charType, char16_t>::value )
        {
            return u"wb";
        }
    }

    template <eir::CharacterType charType>
    inline const charType* GetAnyWildcardSelector( void )
    {
        if constexpr ( std::is_same <charType, char>::value )
        {
            return "*";
        }
        else if constexpr ( std::is_same <charType, wchar_t>::value )
        {
            return L"*";
        }
        else if constexpr ( std::is_same <charType, char8_t>::value )
        {
            return u8"*";
        }
        else if constexpr ( std::is_same <charType, char16_t>::value )
        {
            return u"*";
        }
        else if constexpr ( std::is_same <charType, char32_t>::value )
        {
            return U"*";
        }
    }

    template <eir::CharacterType charType>
    inline const charType* GetDirectorySeparator( bool trueForwardFalseBackward )
    {
        if constexpr ( std::same_as <charType, char> )
        {
            if ( trueForwardFalseBackward )
            {
                return "/";
            }
            else
            {
                return "\\";
            }
        }
        else if constexpr ( std::same_as <charType, wchar_t> )
        {
            if ( trueForwardFalseBackward )
            {
                return L"/";
            }
            else
            {
                return L"\\";
            }
        }
        else if constexpr ( std::same_as <charType, char8_t> )
        {
            if ( trueForwardFalseBackward )
            {
                return u8"/";
            }
            else
            {
                return u8"\\";
            }
        }
        else if constexpr ( std::same_as <charType, char16_t> )
        {
            if ( trueForwardFalseBackward )
            {
                return u"/";
            }
            else
            {
                return u"\\";
            }
        }
        else if constexpr ( std::same_as <charType, char32_t> )
        {
            if ( trueForwardFalseBackward )
            {
                return U"/";
            }
            else
            {
                return U"\\";
            }
        }
    }

    template <eir::CharacterType charType>
    AINLINE bool IsDirectorySeparator( charType cp )
    {
        if constexpr ( std::is_same <charType, char>::value )
        {
            return ( cp == '/' || cp == '\\' );
        }
        else if constexpr ( std::is_same <charType, wchar_t>::value )
        {
            return ( cp == L'/' || cp == L'\\' );
        }
        else if constexpr ( std::is_same <charType, char16_t>::value )
        {
            return ( cp == u'/' || cp == u'\\' );
        }
        else if constexpr ( std::is_same <charType, char8_t>::value )
        {
            return ( cp == u8'/' || cp == u8'\\' );
        }
        else if constexpr ( std::is_same <charType, char32_t>::value )
        {
            return ( cp == U'/' || cp == U'\\' );
        }
    }

    template <eir::CharacterType charType>
    inline const charType* GetDefaultDirectorySeparator( void )
    {
        return GetDirectorySeparator <charType> ( true );
    }

    template <eir::CharacterType charType>
    inline charType GetDotCharacter( void )
    {
        if constexpr ( std::is_same <charType, char>::value )
        {
            return '.';
        }
        else if constexpr ( std::is_same <charType, wchar_t>::value )
        {
            return L'.';
        }
        else if constexpr ( std::is_same <charType, char8_t>::value )
        {
            return u8'.';
        }
        else if constexpr ( std::is_same <charType, char16_t>::value )
        {
            return u'.';
        }
        else if constexpr ( std::is_same <charType, char32_t>::value )
        {
            return U'.';
        }
    }

    // Copies from one translator to another using optimizations.
    template <typename charType>
    inline bool FileCopy( CFileTranslator *srcTranslator, const charType *srcPath, CFileTranslator *dstTranslator, const charType *dstPath )
    {
        if ( srcTranslator == dstTranslator )
        {
            return srcTranslator->Copy( srcPath, dstPath );
        }

        CFile *srcStream = srcTranslator->Open( srcPath, FileGetReadModeBinary <charType> () );

        bool successful = false;

        if ( srcStream )
        {
            try
            {
                CFile *dstStream = dstTranslator->Open( dstPath, FileGetWriteModeBinary <charType> () );

                if ( dstStream )
                {
                    try
                    {
                        StreamCopy( *srcStream, *dstStream );

                        successful = true;
                    }
                    catch( ... )
                    {
                        delete dstStream;

                        throw;
                    }

                    delete dstStream;
                }
            }
            catch( ... )
            {
                delete srcStream;

                throw;
            }

            delete srcStream;
        }

        return successful;
    }

    inline bool FileCopy( CFileTranslator *srcTranslator, const filePath& srcPath, CFileTranslator *dstTranslator, const filePath& dstPath )
    {
        return filePath_dispatch( srcPath,
            [&] ( auto srcPath )
            {
                typedef typename resolve_type <decltype(srcPath)>::type charType;

                filePath dstLink( dstPath );

                dstLink.transform_to <charType> ();

                return FileCopy( srcTranslator, srcPath, dstTranslator, dstLink.to_char <charType> () );
            }
        );
    }

    template <typename charType>
    AINLINE const charType* GetFileNameItemStart( const charType *name )
    {
        const charType *origName = name;

        character_env_iterator_tozero <charType> nameIter( name );

        const charType *fileStartFrom = nullptr;

        while ( true )
        {
            auto ichr = nameIter.Resolve();

            if ( ichr == '\0' )
            {
                if ( !fileStartFrom )
                {
                    fileStartFrom = origName;
                }

                break;
            }

            nameIter.Increment();

            if ( ichr == '\\' || ichr == '/' )
            {
                fileStartFrom = nameIter.GetPointer();
            }
        }

        return fileStartFrom;
    }

    template <typename charType>
    AINLINE const charType* GetFileNameItemEndWithExtension( const charType *name, const charType*& extOut )
    {
        character_env_iterator_tozero <charType> nameIter( name );

        const charType *strEnd = nullptr;
        const charType *extStart = nullptr;

        while ( true )
        {
            auto ichr = nameIter.Resolve();

            if ( ichr == '\0' )
            {
                strEnd = nameIter.GetPointer();
                break;
            }

            nameIter.Increment();

            if ( ichr == '.' )
            {
                extStart = nameIter.GetPointer();
            }
        }

        extOut = extStart;
        return strEnd;
    }

    // Useful utility to get the file name out of a path.
    template <typename allocatorType, typename charType, typename... allocArgs>
    inline eir::MultiString <allocatorType> GetFileNameItem(
        const charType *name, bool includeExtension = false,
        eir::MultiString <allocatorType> *outDirectory = nullptr,
        eir::MultiString <allocatorType> *outExtention = nullptr,
        allocArgs... args
    )
    {
        const charType *fileStartFrom = GetFileNameItemStart( name );

        const charType *extStart = nullptr;
        const charType *strEnd = GetFileNameItemEndWithExtension( fileStartFrom, extStart );

        // Dispatch the results.
        const charType *fileEnd = nullptr;

        if ( !includeExtension && extStart != nullptr )
        {
            fileEnd = extStart - 1;
        }
        else
        {
            fileEnd = strEnd;
        }

        // Grab the extension if required.
        if ( outExtention )
        {
            outExtention->clear();

            if ( extStart != nullptr )
            {
                outExtention->append( extStart, strEnd - extStart );
            }
        }

        if ( outDirectory )
        {
            outDirectory->clear();

            // Only create directory path if it is applicable.
            if ( name != fileStartFrom )
            {
                outDirectory->append( name, fileStartFrom - name );
            }
        }

        eir::MultiString <allocatorType> result( eir::constr_with_alloc::DEFAULT, std::forward <allocArgs> ( args )... );

        result.append( fileStartFrom, fileEnd - fileStartFrom );

        return result;
    }

    template <typename allocatorType>
    AINLINE eir::MultiString <allocatorType> GetFileNameItem(
        const eir::MultiString <allocatorType>& nameIn, bool includeExtension = false,
        eir::MultiString <allocatorType> *outDirectory = nullptr, eir::MultiString <allocatorType> *outExtention = nullptr
    )
    {
        return filePath_dispatch( nameIn,
            [&]( auto name )
            {
                eir::MultiString <allocatorType> result( eir::constr_with_alloc::DEFAULT, nameIn.GetAllocData() );

                result = GetFileNameItem( name, includeExtension, outDirectory, outExtention );

                return result;
            }
        );
    }

    // Useful function to get just the extension of a filename.
    template <typename charType>
    inline const charType* FindFileNameExtension( const charType *name )
    {
        const charType *fileStartFrom = GetFileNameItemStart( name );

        const charType *extStart = nullptr;
        const charType *strEnd = GetFileNameItemEndWithExtension( fileStartFrom, extStart );

        (void)strEnd;

        return extStart;
    }

    // Useful function to get just the directory of a filename, if available.
    template <typename allocatorType, typename charType>
    inline bool GetFileNameDirectory( const charType *name, eir::MultiString <allocatorType>& dirOut )
    {
        const charType *fileStartFrom = GetFileNameItemStart( name );

        const charType *extStart = nullptr;
        const charType *strEnd = GetFileNameItemEndWithExtension( fileStartFrom, extStart );

        // Did the function fail?
        if ( strEnd == nullptr )
            return false;

        // Check if we have no directory.
        if ( name == fileStartFrom )
            return false;

        dirOut.clear();
        dirOut.append( name, fileStartFrom - name );
        return true;
    }

    template <typename allocatorType>
    AINLINE bool GetFileNameDirectory( const eir::MultiString <allocatorType>& name, eir::MultiString <allocatorType>& dirOut )
    {
        return filePath_dispatch( name, [&]( auto name ) { return GetFileNameDirectory( name, dirOut ); } );
    }

    // Returns whether a path is a directory.
    inline bool IsPathDirectory( const filePath& thePath )
    {
        size_t pathSize = thePath.charlen();

        if ( pathSize == 0 )
            return true;

        if ( thePath.compareCharAt( '/', pathSize - 1 ) ||
             thePath.compareCharAt( '\\', pathSize - 1 ) )
        {
            return true;
        }

        return false;
    }

    // Helpers for CFile convenience.
    // Reads the whole file into a STL buffer.
    template <typename charType, typename allocatorType>
    inline bool TranslatorReadToBuffer( CFileTranslator *trans, const charType *srcPath, eir::Vector <char, allocatorType>& buffer )
    {
        buffer.Clear();

        CFile *fileHandle = trans->Open( srcPath, FileGetReadModeBinary <charType> () );

        if ( !fileHandle )
            return false;

        bool success = false;

        try
        {
            fsOffsetNumber_t wholeFileSize = fileHandle->GetSizeNative();

            if ( wholeFileSize > 0 && (std::make_unsigned <fsOffsetNumber_t>::type)wholeFileSize <= std::numeric_limits <size_t>::max() )
            {
                size_t realFileSize = (size_t)wholeFileSize;

                buffer.Resize( realFileSize );

                size_t readCount = fileHandle->Read( buffer.GetData(), realFileSize );

                if ( readCount == realFileSize )
                {
                    success = true;
                }
            }
        }
        catch( ... )
        {
            // We should not pass on any exceptions.
            success = false;
        }

        delete fileHandle;

        return success;
    }

    template <typename allocatorType>
    inline bool TranslatorReadToBuffer( CFileTranslator *trans, const filePath& srcPath, eir::Vector <char, allocatorType>& buffer )
    {
        return filePath_dispatch( srcPath,
            [&]( auto srcPath )
        {
            return TranslatorReadToBuffer( trans, srcPath, buffer );
        });
    }

    // Writes data into a file location.
    template <typename charType>
    inline bool TranslatorWriteData( CFileTranslator *trans, const charType *dstPath, const void *dataBuf, size_t dataBufSize )
    {
        bool success = false;

        if ( CFile *writeFile = trans->Open( dstPath, FileGetWriteModeBinary <charType> () ) )
        {
            try
            {
                size_t realWriteCount = writeFile->Write( dataBuf, dataBufSize );

                success = ( realWriteCount == dataBufSize );
            }
            catch( ... )
            {
                // We should not pass on exceptions.
                success = false;
            }

            delete writeFile;
        }

        return success;
    }

    inline bool TranslatorWriteData( CFileTranslator *trans, const filePath& dstPath, const void *dataBuf, size_t dataBufSize )
    {
        return filePath_dispatch( dstPath,
            [&]( auto dstPath )
        {
            return TranslatorWriteData( trans, dstPath, dataBuf, dataBufSize );
        });
    }

    /*===================================================
        FileGetString

        Arguments:
            file - the stream to read from
            output - eir::String type to write the string to
        Purpose:
            Reads a line from this file/stream. Lines are seperated
            by \n. Returns whether anything could be read.
    ===================================================*/
    template <typename allocatorType>
    inline bool FileGetString( CFile *file, eir::String <char, allocatorType>& output )
    {
        if ( file->IsEOF() )
            return false;

        do
        {
            char c;

            bool successful = file->ReadByte( c );

            if ( !successful || !c || c == '\n' )
                break;

            if ( c == '\r' )
            {
                char next_c;

                file->ReadByte( next_c );

                if ( next_c == '\n' )
                {
                    break;
                }
                else
                {
                    file->Seek( -1, SEEK_CUR );
                }
            }

            output += c;
        }
        while ( !file->IsEOF() );

        return true;
    }

    /*===================================================
        FileGetString

        Arguments:
            file - the stream to read from
            buf - memory location to write a C string to
            max - has to be >1; maximum valid range of the
                  memory area pointed to by buf.
        Purpose:
            Same as above, but C-style interface. Automatically
            terminates buf contents by \n if successful.
    ===================================================*/
    inline bool FileGetString( CFile *file, char *buf, const size_t max )
    {
        size_t n = 0;

        if ( max < 2 || file->IsEOF() )
            return false;

        do
        {
            char c;

            bool successful = file->ReadByte( c );

            if ( !successful || !c || c == '\n' )
                goto finish;

            buf[n++] = c;

            if ( n == max - 1 )
                goto finish;
        }
        while ( !file->IsEOF() );

finish:
        buf[n] = '\0';
        return true;
    }

    // Write a string into a file.
    inline bool FileWriteString( CFile *stream, const char *theString )
    {
        size_t len = cplen_tozero( theString );

        size_t written_len = stream->Write( theString, len );

        return ( written_len == len );
    }

    static AINLINE bool CanDirectoryPathHaveTrailingFile( filesysPathProcessMode procMode, filesysPathOperatingMode opMode )
    {
        // Determine how many tokens actually designate directories.
        bool canHaveFileAtEnd = false;

        if ( opMode == filesysPathOperatingMode::DEFAULT )
        {
            // The process mode along with the path decides what happens next.
            if ( procMode == filesysPathProcessMode::DISTINGUISHED )
            {
                // If we have a file path, then we need to remove the file.
                canHaveFileAtEnd = false;
            }
            else if ( procMode == filesysPathProcessMode::AMBIVALENT_FILE )
            {
                // Since a file path can both designate a file or directory, we count it as possible dirpath.
                canHaveFileAtEnd = true;
            }
            else
            {
#ifdef _DEBUG
                assert( 0 );
#endif //_DEBUG

                throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
            }
        }
        else if ( opMode == filesysPathOperatingMode::FILE_ONLY )
        {
            // Any file is counted as directory aswell so do not do anything.
            canHaveFileAtEnd = true;
        }
        else if ( opMode == filesysPathOperatingMode::DIR_ONLY )
        {
            // We only care about the true directory portion so remove any file.
            canHaveFileAtEnd = false;
        }
        else
        {
#ifdef _DEBUG
            assert( 0 );
#endif //_DEBUG
        
            throw filesystem_exception( eGenExceptCode::INTERNAL_ERROR );
        }

        return canHaveFileAtEnd;
    }
}

#endif //_CFileSystemInterface_
