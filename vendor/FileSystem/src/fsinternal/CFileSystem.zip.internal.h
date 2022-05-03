/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.zip.internal.h
*  PURPOSE:     Master header of ZIP archive filesystem private modules
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_ZIP_PRIVATE_MODULES_
#define _FILESYSTEM_ZIP_PRIVATE_MODULES_

// ZIP extension struct.
struct zipExtension
{
    static fileSystemFactory_t::pluginOffset_t _zipPluginOffset;

    static zipExtension*        Get( CFileSystem *sys )
    {
        if ( _zipPluginOffset != fileSystemFactory_t::INVALID_PLUGIN_OFFSET )
        {
            return fileSystemFactory_t::RESOLVE_STRUCT <zipExtension> ( (CFileSystemNative*)sys, _zipPluginOffset );
        }
        return nullptr;
    }

    void                        Initialize      ( CFileSystemNative *sys );
    void                        Shutdown        ( CFileSystemNative *sys );

    zipExtension& operator = ( const zipExtension& right ) = delete;

    CArchiveTranslator*         NewArchive      ( CFile& writeStream );
    CArchiveTranslator*         OpenArchive     ( CFile& readWriteStream );

    // Extension members.
    CFileSystemNative*          fileSys;
};

// Checksums.
#define ZIP_SIGNATURE               0x06054B50
#define ZIP_FILE_SIGNATURE          0x02014B50
#define ZIP_LOCAL_FILE_SIGNATURE    0x04034B50

#include <time.h>

#include "CFileSystem.vfs.h"
#include "CFileSystem.FileDataPresence.h"

#include <sdk/String.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4250)
#endif //_MSC_VER

// Forward declaration for an important type.
struct zipStream;

class CZIPArchiveTranslator final : public CSystemPathTranslator <pathcheck_always_allow, basicRootPathType <pathcheck_always_allow>>, public CFileTranslatorWideWrap, public CArchiveTranslator
{
    friend struct zipExtension;
    friend class CArchiveFile;
public:
                    CZIPArchiveTranslator( CFileSystemNative *fileSys, zipExtension& theExtension, CFile& file );
                    ~CZIPArchiveTranslator( void );

    bool            CreateDir( const char *path, filesysPathOperatingMode opMode, bool createParentDirs ) override;
    CFile*          Open( const char *path, const filesysOpenMode& mode, eFileOpenFlags flags ) override;
    bool            Exists( const char *path ) const override;
    bool            Delete( const char *path, filesysPathOperatingMode opMode ) override;
    bool            Copy( const char *src, const char *dst ) override;
    bool            Rename( const char *src, const char *dst ) override;
    size_t          Size( const char *path ) const override;
    bool            QueryStats( const char *path, filesysStats& statsOut ) const override;

protected:
    bool            OnConfirmDirectoryChange( const translatorPathResult& nodePath ) override;

public:
    void            ScanDirectory( const char *directory, const char *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const override;

    void            GetDirectories( const char *path, const char *wildcard, bool recurse, dirNames& output ) const override;
    void            GetFiles( const char *path, const char *wildcard, bool recurse, dirNames& output ) const override;
    CDirectoryIterator* BeginDirectoryListing( const char *path, const char *wildcard, const scanFilteringFlags& filter_flags ) const override;

    void            Save( void ) override;

    // Members.
    zipExtension&   m_zipExtension;
    CFile&          m_file;

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::natOptRWLock   m_fileLock;     // taken if zipStream operates on the archive file directly.
#endif //FILESYS_MULTI_THREADING

#pragma pack(1)
    struct _localHeader
    {
        endian::p_little_endian <fsUInt_t>      signature;          // 0
        endian::p_little_endian <fsUShort_t>    version;            // 4
        endian::p_little_endian <fsUShort_t>    flags;              // 6
        endian::p_little_endian <fsUShort_t>    compression;        // 8
        endian::p_little_endian <fsUShort_t>    modTime;            // 10
        endian::p_little_endian <fsUShort_t>    modDate;            // 12
        endian::p_little_endian <fsUInt_t>      crc32val;           // 14

        endian::p_little_endian <fsUInt_t>      sizeCompressed;     // 18
        endian::p_little_endian <fsUInt_t>      sizeReal;           // 22

        endian::p_little_endian <fsUShort_t>    nameLen;            // 26
        endian::p_little_endian <fsUShort_t>    commentLen;         // 28
    };
    static_assert( sizeof(_localHeader) == 30 );

    struct _dataDescriptor
    {
        endian::p_little_endian <fsUInt_t>      crc32val;           // 0
        endian::p_little_endian <fsUInt_t>      sizeCompressed;     // 4
        endian::p_little_endian <fsUInt_t>      sizeReal;           // 8
    };
    static_assert( sizeof(_dataDescriptor) == 12 );

    struct _centralFile
    {
        endian::p_little_endian <fsUInt_t>      signature;          // 0
        endian::p_little_endian <fsUShort_t>    version;            // 4
        endian::p_little_endian <fsUShort_t>    reqVersion;         // 6
        endian::p_little_endian <fsUShort_t>    flags;              // 8
        endian::p_little_endian <fsUShort_t>    compression;        // 10
        endian::p_little_endian <fsUShort_t>    modTime;            // 12
        endian::p_little_endian <fsUShort_t>    modDate;            // 14
        endian::p_little_endian <fsUInt_t>      crc32val;           // 16

        endian::p_little_endian <fsUInt_t>      sizeCompressed;     // 20
        endian::p_little_endian <fsUInt_t>      sizeReal;           // 24

        endian::p_little_endian <fsUShort_t>    nameLen;            // 28
        endian::p_little_endian <fsUShort_t>    extraLen;           // 30
        endian::p_little_endian <fsUShort_t>    commentLen;         // 32

        endian::p_little_endian <fsUShort_t>    diskID;             // 34
        endian::p_little_endian <fsUShort_t>    internalAttr;       // 36
        endian::p_little_endian <fsUInt_t>      externalAttr;       // 38
        endian::p_little_endian <fsUInt_t>      localHeaderOffset;  // 42
    };
    static_assert( sizeof(_centralFile) == 46 );
#pragma pack()

private:
    void            ReadFiles( unsigned int count );    // throws an exception on failure.

public:
    struct fileMetaData;
    struct directoryMetaData;

public:
    typedef eir::String <char, FSObjectHeapAllocator> zipString;

    struct genericMetaData abstract
    {
        VirtualFileSystem::fsObjectInterface *genericInterface;

        inline genericMetaData( CZIPArchiveTranslator *trans, VirtualFileSystem::fsObjectInterface *intf )
        {
            this->genericInterface = intf;
            this->hasLocalHeaderOffset = false;
        }

        unsigned short      version;
        unsigned short      reqVersion;
        unsigned short      flags;

        unsigned short      diskID;

        unsigned short      modTime;
        unsigned short      modDate;

        size_t              localHeaderOffset;
        bool                hasLocalHeaderOffset;

        zipString           extra;
        zipString           comment;

        inline _localHeader ConstructLocalHeader( void ) const
        {
            _localHeader header;
            header.signature = ZIP_LOCAL_FILE_SIGNATURE;
            header.modTime = modTime;
            header.modDate = modDate;
            header.nameLen = (fsUShort_t)this->genericInterface->GetRelativePath().size();
            header.commentLen = (fsUShort_t)comment.GetLength();
            return header;
        }

        inline _centralFile ConstructCentralHeader( void ) const
        {
            _centralFile header;
            header.signature = ZIP_FILE_SIGNATURE;
            header.version = version;
            header.reqVersion = reqVersion;
            header.flags = flags;
            //header.compression
            header.modTime = modTime;
            header.modDate = modDate;
            //header.crc32
            //header.sizeCompressed
            //header.sizeReal
            header.nameLen = (fsUShort_t)this->genericInterface->GetRelativePath().size();
            header.extraLen = (fsUShort_t)extra.GetLength();
            header.commentLen = (fsUShort_t)comment.GetLength();
            header.diskID = diskID;
            //header.internalAttr
            //header.externalAttr
            header.localHeaderOffset = (fsUInt_t)localHeaderOffset;
#ifdef _DEBUG
            assert( this->hasLocalHeaderOffset == true );
#endif //_DEBUG
            return header;
        }

        inline void AllocateArchiveSpace( CZIPArchiveTranslator *archive, _localHeader& entryHeader, size_t& sizeCount )
        {
            // Update the offset.
            localHeaderOffset = archive->m_file.Tell() - archive->m_structOffset;
            hasLocalHeaderOffset = true;

            sizeCount += sizeof( entryHeader ) + entryHeader.nameLen + entryHeader.commentLen;
        }

        inline void Reset( void )
        {
#ifdef _WIN32
            version = 10;
#else
            version = 0x03; // Unix
#endif //_WIN32
            reqVersion = 0x14;
            flags = 0;

            UpdateTime();

            diskID = 0;

            extra.Clear();
            comment.Clear();
        }

        inline void SetModTime( const tm& date )
        {
            unsigned short year = date.tm_year;

            if ( date.tm_year > 1980 )
            {
                year -= 1980;
            }
            else if ( date.tm_year > 80 )
            {
                year -= 80;
            }

            modDate = date.tm_mday | ((date.tm_mon + 1) << 5) | (year << 9);
            modTime = date.tm_sec >> 1 | (date.tm_min << 5) | (date.tm_hour << 11);
        }

        inline void GetModTime( tm& date ) const
        {
            date.tm_mday = modDate & 0x1F;
            date.tm_mon = ((modDate & 0x1E0) >> 5) - 1;
            date.tm_year = ((modDate & 0x0FE00) >> 9) + 1980;

            date.tm_hour = (modTime & 0xF800) >> 11;
            date.tm_min = (modTime & 0x7E0) >> 5;
            date.tm_sec = (modTime & 0x1F) << 1;

            date.tm_wday = 0;
            date.tm_yday = 0;
        }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif //_MSC_VER

        virtual void UpdateTime( void )
        {
            time_t curTime = time( nullptr );
            SetModTime( *gmtime( &curTime ) );
        }

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER
    };

    enum class eFileDataState
    {
        ARCHIVED,
        PRESENT,
        PRESENT_COMPRESSED
    };

    struct fileMetaData : public genericMetaData
    {
        inline fileMetaData( CZIPArchiveTranslator *trans, VirtualFileSystem::fileInterface *intf ) : genericMetaData( trans, intf ), dataRefCount( 0 )
#ifdef FILESYS_MULTI_THREADING
            , lockAtomic( nativeFileSystem->nativeMan )
#endif //FILESYS_MULTI_THREADING
        {
            this->dataStream = nullptr;
            this->hasDataLocalMods = false;
            this->translator = trans;
            this->dataOffset = 0;

            // Make it default properly.
            this->Reset();
        }

        inline ~fileMetaData( void )
        {
            assert( this->dataRefCount == 0 );

            if ( CFile *dataStream = this->dataStream )
            {
                delete dataStream;

                this->dataStream = nullptr;
            }
        }

        fsOffsetNumber_t    dataOffset; // non-zero if inside archive.

        unsigned short  compression;

        unsigned int    crc32val;
        size_t          sizeCompressed;
        mutable size_t  sizeReal;
        mutable bool    sizeRealIsVerified;

        unsigned short  internalAttr;
        unsigned int    externalAttr;

        eFileDataState  dataState;

        typedef eir::Vector <zipStream*, FSObjectHeapAllocator> lockList_t;
        lockList_t locks;

        bool            hasDataLocalMods;

    private:
        CFile*          dataStream;

        std::atomic <unsigned long> dataRefCount;

    public:
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::natOptRWLock lockAtomic;
#endif //FILESYS_MULTI_THREADING

        CZIPArchiveTranslator *translator;

        inline void Reset( void )
        {
            // Reset generic data.
            genericMetaData::Reset();

            // File-specific reset.
            compression = 8;

            crc32val = 0;
            sizeCompressed = 0;
            sizeReal = 0;
            sizeRealIsVerified = false;
            internalAttr = 0;
            externalAttr = 0;

            dataState = eFileDataState::ARCHIVED;
            dataOffset = 0;

            assert( this->dataRefCount == 0 );      // nobody must be using our data.

            hasDataLocalMods = false;

            if ( CFile *dataStream = this->dataStream )
            {
                delete dataStream;
            }

            dataStream = nullptr;
        }

        // API for obtaining and updating the dataStream.
        inline void SetDataStream( CFile *dataStream )
        {
            // TODO: of course this needs a lock :-)

            CFile *curDataStream = this->dataStream;

            if ( curDataStream == dataStream )
                return;

            assert( this->dataRefCount == 0 );

            if ( curDataStream )
            {
                delete curDataStream;
            }

            this->dataStream = dataStream;
        }

        inline bool IsDataStream( CFile *dataStream ) const
        {
            return ( dataStream == this->dataStream );
        }

        inline bool AcquireDataStream( void )
        {
            CFile *curDataStream = this->dataStream;

            if ( !curDataStream )
                return false;

            this->dataRefCount++;

            return true;
        }

        inline void GarbageCollectDataStream( void )
        {
            if ( this->hasDataLocalMods || this->dataRefCount > 0 )
                return;

            // Release the data stream.
            if ( CFile *dataStream = this->dataStream )
            {
                delete dataStream;
            }

            this->dataStream = nullptr;
            this->dataState = eFileDataState::ARCHIVED;
        }

        inline CFile* GetDataStream( void )
        {
            assert( this->dataRefCount >= 1 );

            return this->dataStream;
        }

        inline void ReleaseDataStream( void )
        {
            assert( this->dataRefCount >= 1 );

            this->dataRefCount--;

            this->GarbageCollectDataStream();
        }

        inline bool IsLocked( void ) const
        {
            return ( locks.GetCount() != 0 );
        }

        inline bool IsNative( void ) const
        {
#ifdef _WIN32
            return version == 10;
#else
            return version == 0x03; // Unix
#endif //_WIN32
        }

        inline void UpdateTime( void )
        {
            genericMetaData::UpdateTime();

            // Update the time of its parent directories.
            VirtualFileSystem::fileInterface *ourIntf = dynamic_cast <VirtualFileSystem::fileInterface*> ( this->genericInterface );

            if ( VirtualFileSystem::directoryInterface *parentDirAbs = ourIntf->GetParent() )
            {
                directory *parentDir = (directory*)parentDirAbs;

                parentDir->metaData.UpdateTime();
            }
        }

        // (Virtual) node methods.
        fsOffsetNumber_t GetSize( void ) const;

        bool OnFileCopy( const dirNames& tree, const filePath& newName ) const;
        bool OnFileRename( const dirNames& tree, const filePath& newName );
        void OnFileDelete( void );

        inline void GetANSITimes( time_t& mtime, time_t& ctime, time_t& atime ) const
        {
            tm date;
            GetModTime( date );

            date.tm_year -= 1900;

            mtime = ctime = atime = mktime( &date );
        }

        inline void GetDeviceIdentifier( dev_t& deviceIdx ) const
        {
            deviceIdx = (dev_t)this->diskID;
        }

        bool CopyAttributesTo( fileMetaData& dstEntry );

        void ChangeToState( eFileDataState changeToState );
        void Decompress( void );

        inline void OnModification( void )
        {
            this->localHeaderOffset = 0;
            this->hasLocalHeaderOffset = false;
            this->dataOffset = 0;
            this->hasDataLocalMods = true;
        }
    };

public:
    struct directoryMetaData : public genericMetaData
    {
        VirtualFileSystem::directoryInterface *dirInterface;

        inline directoryMetaData( CZIPArchiveTranslator *trans, VirtualFileSystem::directoryInterface *intf ) : genericMetaData( trans, intf )
        {
            this->dirInterface = intf;

            this->Reset();
        }

        inline bool     NeedsWriting( void ) const
        {
            return ( this->dirInterface->IsEmpty() || comment.GetLength() != 0 || extra.GetLength() != 0 );
        }

        inline void     UpdateTime( void )
        {
            genericMetaData::UpdateTime();

            // Update the time of its parent directories.
            if ( VirtualFileSystem::directoryInterface *parent = dirInterface->GetParent() )
            {
                ((directory*)parent)->metaData.UpdateTime();
            }
        }
    };

    // Runtime management for files.
    CFileDataPresenceManager fileMan;

    // Tree of all filesystem objects.
    typedef CVirtualFileSystem <CZIPArchiveTranslator, directoryMetaData, fileMetaData> vfs_t;

    vfs_t m_virtualFS;

    typedef vfs_t::fsActiveEntry fsActiveEntry;
    typedef vfs_t::file file;
    typedef vfs_t::directory directory;

    // Special functions for the VFS.
    CFile* OpenNativeFileStream( file *fsObject, eVFSFileOpenMode openMode, const filesysAccessFlags& access );

    void OnRemoveRemoteFile( const dirNames& tree, const filePath& name );

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::CReadWriteLock* get_file_lock( CFile *useFile );
#endif //FILESYS_MULTI_THREADING

private:
    void            CacheData( void );
    void            SaveData( size_t& size );
    unsigned int    BuildCentralFileHeaders( size_t& size );

    zipString   m_comment;

    size_t      m_structOffset;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER

#endif //_FILESYSTEM_ZIP_PRIVATE_MODULES_
