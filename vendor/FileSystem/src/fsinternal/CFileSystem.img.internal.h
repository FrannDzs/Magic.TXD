/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.img.internal.h
*  PURPOSE:     IMG R* Games archive management
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_IMG_ROCKSTAR_MANAGEMENT_INTERNAL_
#define _FILESYSTEM_IMG_ROCKSTAR_MANAGEMENT_INTERNAL_

#include <sdk/eirutils.h>

#include "CFileSystem.FileDataPresence.h"

#ifdef FILESYS_ENABLE_LZO

// Implement the GTAIII/GTAVC XBOX IMG archive compression.
struct xboxIMGCompression : public CIMGArchiveCompressionHandler
{
                xboxIMGCompression( void );
                ~xboxIMGCompression( void );

    bool        IsStreamCompressed( CFile *stream ) const;

    bool        Decompress( CFile *input, CFile *output );
    bool        Compress( CFile *input, CFile *output );

    struct simpleWorkBuffer
    {
        inline simpleWorkBuffer( void ) noexcept
        {
            this->bufferSize = 0;
            this->buffer = nullptr;
        }

        inline simpleWorkBuffer( const simpleWorkBuffer& right ) = delete;
        inline simpleWorkBuffer( simpleWorkBuffer&& right ) noexcept
        {
            this->bufferSize = right.bufferSize;
            this->buffer = right.buffer;

            right.bufferSize = 0;
            right.buffer = nullptr;
        }

        inline ~simpleWorkBuffer( void )
        {
            if ( void *ptr = this->buffer )
            {
                FSObjectHeapAllocator memAlloc;

                memAlloc.Free( nullptr, ptr );

                this->buffer = nullptr;
            }
        }

        inline simpleWorkBuffer& operator = ( const simpleWorkBuffer& right ) = delete;
        inline simpleWorkBuffer& operator = ( simpleWorkBuffer&& right ) noexcept
        {
            this->~simpleWorkBuffer();

            return *new (this) simpleWorkBuffer( std::move( right ) );
        }

        inline bool MinimumSize( size_t minSize )
        {
            bool hasChanged = false;

            void *bufPtr = this->buffer;
            size_t bufSize = this->bufferSize;

            if ( bufPtr == nullptr || bufSize < minSize )
            {
                FSObjectHeapAllocator memAlloc;

                void *newBufPtr = TemplateMemTransalloc( memAlloc, nullptr, bufPtr, bufSize, minSize, 1 );

                if ( minSize == 0 )
                {
                    this->buffer = nullptr;
                    this->bufferSize = 0;

                    hasChanged = true;
                }
                else if ( newBufPtr != nullptr )
                {
                    this->buffer = newBufPtr;
                    this->bufferSize = minSize;

                    hasChanged = true;
                }
            }

            return hasChanged;
        }

        inline void Grow( size_t growBy )
        {
            if ( growBy == 0 )
                return;

            size_t newSize = ( this->bufferSize + growBy );

            FSObjectHeapAllocator memAlloc;

            void *newBufPtr = TemplateMemTransalloc( memAlloc, nullptr, this->buffer, this->bufferSize, newSize, 1 );

            // Growing can fail.
            if ( newBufPtr != nullptr )
            {
                this->buffer = newBufPtr;
                this->bufferSize = newSize;
            }
        }

        inline void* GetPointer( void )
        {
            return this->buffer;
        }

        inline size_t GetSize( void ) const
        {
            return bufferSize;
        }

        inline bool IsReady( void ) const
        {
            return ( this->buffer != nullptr );
        }

    private:
        size_t bufferSize;
        void *buffer;
    };

    simpleWorkBuffer decompressBuffer;

    size_t      compressionMaximumBlockSize;
};

#endif //FILESYS_ENABLE_LZO

// IMG extension struct.
struct imgExtension
{
    static fileSystemFactory_t::pluginOffset_t _imgPluginOffset;

    static imgExtension*        Get( CFileSystem *sys )
    {
        if ( _imgPluginOffset != fileSystemFactory_t::INVALID_PLUGIN_OFFSET )
        {
            return fileSystemFactory_t::RESOLVE_STRUCT <imgExtension> ( (CFileSystemNative*)sys, _imgPluginOffset );
        }
        return nullptr;
    }

    inline CFileSystemNative* GetSystem( void )
    {
        return fileSystemFactory_t::BACK_RESOLVE_STRUCT( this, _imgPluginOffset );
    }

    void                        Initialize      ( CFileSystemNative *sys );
    void                        Shutdown        ( CFileSystemNative *sys );

    CIMGArchiveTranslatorHandle*    NewArchiveDirect    ( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion version, bool isLiveMode = false );
    CIMGArchiveTranslatorHandle*    OpenArchiveDirect   ( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion version, bool isLiveMode = false );

    CIMGArchiveTranslatorHandle*    NewArchive  ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version, bool isLiveMode = false );
    CIMGArchiveTranslatorHandle*    OpenArchive ( CFileTranslator *srcRoot, const char *srcPath, bool writeAccess, bool isLiveMode = false );

    CIMGArchiveTranslatorHandle*    NewArchive  ( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version, bool isLiveMode = false );
    CIMGArchiveTranslatorHandle*    OpenArchive ( CFileTranslator *srcRoot, const wchar_t *srcPath, bool writeAccess, bool isLiveMode = false );

    CIMGArchiveTranslatorHandle*    NewArchive  ( CFileTranslator *srcRoot, const char8_t *srcPath, eIMGArchiveVersion version, bool isLiveMode = false );
    CIMGArchiveTranslatorHandle*    OpenArchive ( CFileTranslator *srcRoot, const char8_t *srcPath, bool writeAccess, bool isLiveMode = false );
};

#include <sys/stat.h>

#include "CFileSystem.vfs.h"

// Global IMG management definitions.
#define IMG_BLOCK_SIZE          2048

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4250)
#endif //_MSC_VER

class CIMGArchiveTranslator : public CSystemPathTranslator <pathcheck_always_allow, basicRootPathType <pathcheck_always_allow>>, public CFileTranslatorWideWrap, public CIMGArchiveTranslatorHandle
{
    friend class CFileSystem;
    friend struct imgExtension;
public:
                    CIMGArchiveTranslator( imgExtension& imgExt, CFile *contentFile, CFile *registryFile, eIMGArchiveVersion theVersion, bool isLiveMode );
                    ~CIMGArchiveTranslator( void );

    bool            CreateDir( const char *path, filesysPathOperatingMode opMode, bool createParentDirs ) override final;
    CFile*          Open( const char *path, const filesysOpenMode& mode, eFileOpenFlags flags ) override final;
    bool            Exists( const char *path ) const override final;
    bool            Delete( const char *path, filesysPathOperatingMode opMode ) override final;
    bool            Copy( const char *src, const char *dst ) override final;
    bool            Rename( const char *src, const char *dst ) override final;
    size_t          Size( const char *path ) const override final;
    bool            QueryStats( const char *path, filesysStats& statsOut ) const override final;

protected:
    bool            OnConfirmDirectoryChange( const translatorPathResult& tree ) override final;

public:
    void            ScanDirectory( const char *directory, const char *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const override final;

    void            ScanFilesInSerializationOrder( pathCallback_t filecb, void *ud ) const override final;

    void            GetDirectories( const char *path, const char *wildcard, bool recurse, dirNames& output ) const override final;
    void            GetFiles( const char *path, const char *wildcard, bool recurse, dirNames& output ) const override final;
    CDirectoryIterator* BeginDirectoryListing( const char *path, const char *wildcard, const scanFilteringFlags& filter_flags ) const override;

    void            Save( void ) override final;

    void            SetCompressionHandler( CIMGArchiveCompressionHandler *handler ) override final;

    eIMGArchiveVersion  GetVersion( void ) const override final     { return m_version; }

    // Members.
    imgExtension&   m_imgExtension;
    CFile*          m_contentFile;
    CFile*          m_registryFile;
    eIMGArchiveVersion  m_version;

    bool isLiveMode;    // if true then the archive is editable directly without saving.

    CIMGArchiveCompressionHandler*  m_compressionHandler;

protected:
    // Allocator of space on the IMG file.
    typedef InfiniteCollisionlessBlockAllocator <size_t> fileAddrAlloc_t;

    enum class eFileDataState
    {
        ARCHIVED,
        PRESENT,
        PRESENT_COMPRESSED      // only means "potentially compressed"
    };

    struct fileMetaData
    {
        VirtualFileSystem::fileInterface *fileNode;

        inline void Reset( void )
        {
            // Need to establish some things.
            assert( this->lockCount == 0 );

            // Release any previous data.
            this->releaseDataStream();
            this->releaseAllocation();

            // Reset data to standards.
            this->dataState = eFileDataState::ARCHIVED;
            this->dataStream = nullptr;
            this->blockOffset = 0;
            this->resourceSize = 0;
            this->resourceName[0] = 0;
            this->lockCount = 0;
        }

        inline bool IsLocked( void ) const
        {
            return ( this->lockCount != 0 );
        }

        inline fileMetaData( CIMGArchiveTranslator *translator, VirtualFileSystem::fileInterface *intf )
        {
            this->dataState = eFileDataState::ARCHIVED;
            this->dataStream = nullptr;
            this->blockOffset = 0;
            this->resourceSize = 0;
            this->resourceName[0] = 0;
            this->isAllocated = false;
            this->lockCount = 0;
            this->fileNode = intf;
        }

        inline void releaseDataStream( void )
        {
            if ( CFile *prevStream = this->dataStream )
            {
                delete prevStream;

                this->dataStream = nullptr;
            }
        }

        inline ~fileMetaData( void )
        {
            assert( this->lockCount == 0 );

            this->releaseDataStream();
            this->releaseAllocation();
        }

        inline void AddLock( void )
        {
            this->lockCount++;
        }

        inline void RemoveLock( void )
        {
            this->lockCount--;
        }

        fsOffsetNumber_t GetArchivedOffsetToFile( void ) const
        {
            return ( this->blockOffset * IMG_BLOCK_SIZE );
        }

        fsOffsetNumber_t GetArchivedBlockSize( void ) const
        {
            return ( this->resourceSize * IMG_BLOCK_SIZE );
        }

        void TargetContentFile( fsOffsetNumber_t offset = 0 ) const
        {
            fsOffsetNumber_t archiveOffset = this->GetArchivedOffsetToFile();

            // Calculate the offset inside of the file we should seek to.
            fsOffsetNumber_t imgArchiveRealSeek = ( archiveOffset + offset );

            CIMGArchiveTranslator *translator = this->GetTranslator();

            translator->m_contentFile->SeekNative( imgArchiveRealSeek, SEEK_SET );
        }

        CFile* GetInputStream( fsOffsetNumber_t& streamBegOffset );

        void AcquireDataStream( CFile *dataStream );
        CFile* GetDataStream( void );

        bool TouchDataRequiresExtraction( void );
        bool TouchDataExtractStream( void );

        void PulseDecompression( void );

        // The data state decides which fields are valid.
        eFileDataState dataState;
    private:
        CFile *dataStream;      // valid if dataState == PRESENT (and subtypes)

    public:
        size_t blockOffset;     // valid if dataState == ARCHIVED
        size_t resourceSize;    // valid if dataState == ARCHIVED or PRESENT_COMPRESSED

        char resourceName[24 + 1];

        // Live-mode data about allocation inside IMG archive.
        bool isAllocated;
        fileAddrAlloc_t::block_t allocBlock;

        void releaseAllocation( void );

        typedef sliceOfData <size_t> imgBlockSlice_t;

        inline imgBlockSlice_t GetDataSlice( void ) const
        {
            return imgBlockSlice_t( this->blockOffset, this->resourceSize );
        }

        unsigned long lockCount;        // fs lock, amount of streams operating on this

        // Data presence variables.
        //TODO.

        inline CIMGArchiveTranslator* GetTranslator( void ) const
        {
            return (CIMGArchiveTranslator*)( (char*)this->fileNode->GetManager() - offsetof(CIMGArchiveTranslator, m_virtualFS) );
        }

        // Virtual methods.
        inline fsOffsetNumber_t GetSize( void ) const
        {
            fsOffsetNumber_t resourceSize = 0;

            // Make sure it is decompressed.
            const_cast <fileMetaData*> ( this )->PulseDecompression();

            eFileDataState dataState = this->dataState;

            if ( dataState == eFileDataState::ARCHIVED ||
                 dataState == eFileDataState::PRESENT_COMPRESSED )
            {
                resourceSize = ( (fsOffsetNumber_t)this->resourceSize * IMG_BLOCK_SIZE );
            }
            else if ( dataState == eFileDataState::PRESENT )
            {
                CFile *dataStream = this->dataStream;

                if ( dataStream )
                {
                    resourceSize = dataStream->GetSizeNative();
                }
            }

            return resourceSize;
        }

        bool OnFileCopy( const dirNames& tree, const filePath& newName ) const;
        bool OnFileRename( const dirNames& tree, const filePath& newName );
        void OnFileDelete( void );

        inline void GetANSITimes( time_t& mtime, time_t& atime, time_t& ctime ) const
        {
            // There is no time info saved that is related to IMG archive entries.
            return;
        }

        inline void GetDeviceIdentifier( dev_t& deviceIdx ) const
        {
            // Return some funny device id.
            deviceIdx = 0x41;
        }

        bool CopyAttributesTo( fileMetaData& dstEntry ) const;
    };

    struct directoryMetaData
    {
        VirtualFileSystem::directoryInterface *dirNode;

        inline directoryMetaData( CIMGArchiveTranslator *trans, VirtualFileSystem::directoryInterface *intf )
        {
            this->dirNode = intf;
        }
    };

    // Runtime management for files.
    // It is important to place it before m_virtualFS because it contains all data streams.
    CFileDataPresenceManager fileMan;

    // Virtual filesystem implementation.
    typedef CVirtualFileSystem <CIMGArchiveTranslator, directoryMetaData, fileMetaData> vfs_t;

    vfs_t m_virtualFS;

    typedef vfs_t::fsActiveEntry fsActiveEntry;
    typedef vfs_t::file file;
    typedef vfs_t::directory directory;

public:
    // Special VFS functions.
    CFile* OpenNativeFileStream( file *fsObject, eVFSFileOpenMode openMode, const filesysAccessFlags& access );

    void OnRemoveRemoteFile( const dirNames& tree, const filePath& newName );

    // Extraction helper for data still inside the IMG archive.
    bool RequiresExtraction( file *fileInfo );
    bool ExtractStream( file *theFile );

protected:
    // Public stream base-class for IMG archive streams.
    struct streamBase : public CFile
    {
        inline streamBase( CIMGArchiveTranslator *translator, file *theFile, filesysAccessFlags accessMode )
        {
            this->m_translator = translator;
            this->m_info = theFile;
            this->m_accessMode = std::move( accessMode );

            // Increase the lock count.
            theFile->metaData.AddLock();
        }

        inline ~streamBase( void )
        {
            // Decrease the lock count.
            m_info->metaData.RemoveLock();
        }

        bool QueryStats( filesysStats& statsOut ) const override
        {
            m_translator->m_virtualFS.QueryStatsObject( m_info, statsOut );
            return true;
        }

        void SetFileTimes( time_t atime, time_t ctime, time_t mtime ) override
        {
            // Generarilly there is nothing to do, because IMG entries do not store anything.
        }

        // We do not provide support for file mapping.
        CFileMappingProvider* CreateMapping( void ) override
        {
            return nullptr;
        }

        inline filePath GetPath( void ) const
        {
            return m_info->GetRelativePath();
        }

        bool IsReadable( void ) const noexcept final
        {
            return m_accessMode.allowRead;
        }

        bool IsWriteable( void ) const noexcept final
        {
            return m_accessMode.allowWrite;
        }

    protected:
        CIMGArchiveTranslator *m_translator;
        file *m_info;
        filesysAccessFlags m_accessMode;
    };

    // Slice of file stream.
    typedef sliceOfData <fsOffsetNumber_t> fileStreamSlice_t;

    // File stream inside of the IMG archive (read-only).
    struct dataSectorStream : public streamBase
    {
        inline dataSectorStream( CIMGArchiveTranslator *translator, file *theFile, filesysAccessFlags accessMode );
        inline ~dataSectorStream( void );

        // Helpers.
        fsOffsetNumber_t _getsize( void ) const;

    private:
        // Archived truncation API.
        struct archivedTruncMan_t
        {
            inline dataSectorStream* GetStream( void )
            {
                return (dataSectorStream*)( (char*)this - offsetof(dataSectorStream, archivedTruncMan) );
            }

            inline const dataSectorStream* GetStream( void ) const
            {
                return (const dataSectorStream*)( (const char*)this - offsetof(dataSectorStream, archivedTruncMan) );
            }

            fsOffsetNumber_t Size( void ) const;
            void Truncate( fsOffsetNumber_t targetSize );
        };
        archivedTruncMan_t archivedTruncMan;

        friend struct archivedTruncMan_t;

    public:
        size_t Read( void *buffer, size_t readCount ) override;
        size_t Write( const void *buffer, size_t writeCount ) override;

        int Seek( long iOffset, int iType ) override;
        int SeekNative( fsOffsetNumber_t iOffset, int iType ) override;

        long Tell( void ) const noexcept override;
        fsOffsetNumber_t TellNative( void ) const noexcept override;
        bool IsEOF( void ) const noexcept override;

        void SetSeekEnd( void ) override;

        size_t GetSize( void ) const noexcept override;
        fsOffsetNumber_t GetSizeNative( void ) const noexcept override;

        void Flush( void ) override;

        fsOffsetNumber_t m_currentSeek;
    };

    // Methods for heavy-lifting of file entries.
    bool AllocateFileEntry( file *fileEntry );
    bool ReallocateFileEntry( file *fileEntry, fsOffsetNumber_t fileSize );
    void DeallocateFileEntry( file *fileEntry );

    // Files sorted in allocation-order.
    fileAddrAlloc_t fileAddressAlloc;   // used for write-mode block alignment.

    typedef rwListIterator <fileAddrAlloc_t::block_t, offsetof(fileAddrAlloc_t::block_t, node)> fileAddrAllocNode_t;

    template <typename callbackType>
    AINLINE void ForAllAllocatedFiles( const callbackType& cb )
    {
        fileAddrAllocNode_t iter( this->fileAddressAlloc.blockList );

        if ( iter.IsEnd() )
            return;

        // Skip the first, if it is the file header block.
        // This could be improved in the future.
        if ( iter.Resolve() == &this->fileHeaderAllocBlock )
        {
            iter.Increment();
        }

        while ( iter.IsEnd() == false )
        {
            fileAddrAlloc_t::block_t *item = iter.Resolve();

            file *fileInfo = LIST_GETITEM( file, item, metaData.allocBlock );

            cb( fileInfo );

            iter.Increment();
        }
    }

    // File header allocation unit.
    bool areFileHeadersAllocated = false;
    fileAddrAlloc_t::block_t fileHeaderAllocBlock;

    inline void releaseFileHeadersBlock( void )
    {
        if ( this->areFileHeadersAllocated )
        {
            this->fileAddressAlloc.RemoveBlock( &this->fileHeaderAllocBlock );

            this->areFileHeadersAllocated = false;
        }
    }

    // Private members.
    struct headerGenPresence
    {
        size_t numOfFiles;
    };

    void GenerateFileHeaderStructure( headerGenPresence& genOut );

    struct archiveGenPresence
    {
        // TODO.
    };

    void            GenerateArchiveStructure( archiveGenPresence& genOut );
    void            WriteFileHeaders( CFile *targetStream, directory& baseDir );
    void            WriteFiles( CFile *targetStream, directory& baseDir );

public:
    bool            ReadArchive();
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER

#endif //_FILESYSTEM_IMG_ROCKSTAR_MANAGEMENT_INTERNAL_
