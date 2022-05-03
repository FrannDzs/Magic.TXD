/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/include/CFileSystem.h
*  PURPOSE:     File management
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _CFileSystem_
#define _CFileSystem_

// We need the interface anyway.
#include "CFileSystemInterface.h"

// Include extensions (public headers only)
#include "CFileSystem.zip.h"
#include "CFileSystem.img.h"

#include "CFileSystem.DiskManager.h"

enum class eGlobalFileRootType
{
    CUR_DIR,
    APP_ROOT_DIR
};

struct fs_construction_params
{
    void *nativeExecMan = nullptr;      // set this field if you want MT support in FileSystem (of type NativeExecutive::CExecutiveManager)
    filePath fileRootPath;
    filesysPathProcessMode defaultPathProcessMode = filesysPathProcessMode::DISTINGUISHED;
};

class CFileSystem : public CFileSystemInterface
{
protected:
    // Creation is not allowed by the general runtime anymore.
                            CFileSystem             ( const fs_construction_params& params );
                            ~CFileSystem            ( void );

public:
    // Global functions for initialization of the FileSystem library.
    // There can be only one CFileSystem object alive at a time.
    static CFileSystem*     Create                  ( const fs_construction_params& params );
    static void             Destroy                 ( CFileSystem *lib );

    bool                    CanLockDirectories      ( void );

    // Memory management globals.
    void*                   MemAlloc                ( size_t memSize, size_t alignment ) noexcept;
    bool                    MemResize               ( void *memPtr, size_t memSize ) noexcept;
    void                    MemFree                 ( void *memPtr ) noexcept;

    using CFileSystemInterface::GetSystemRootDescriptor;
    using CFileSystemInterface::CreateTranslator;
    using CFileSystemInterface::CreateSystemMinimumAccessPoint;

    bool                    GetSystemRootDescriptor ( const char *path, filePath& descOut ) const override final;
    bool                    GetSystemRootDescriptor ( const wchar_t *path, filePath& descOut ) const override final;
    bool                    GetSystemRootDescriptor ( const char8_t *path, filePath& descOut ) const override final;

    CFileTranslator*        CreateTranslator        ( const char *path, eDirOpenFlags flags = DIR_FLAG_NONE ) override final;
    CFileTranslator*        CreateTranslator        ( const wchar_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) override final;
    CFileTranslator*        CreateTranslator        ( const char8_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) override final;

    CFileTranslator*        CreateSystemMinimumAccessPoint  ( const char *path, eDirOpenFlags flags = DIR_FLAG_NONE ) override final;
    CFileTranslator*        CreateSystemMinimumAccessPoint  ( const wchar_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) override final;
    CFileTranslator*        CreateSystemMinimumAccessPoint  ( const char8_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) override final;

    CArchiveTranslator*     OpenArchive             ( CFile& file ) override final;

    CArchiveTranslator*     OpenZIPArchive          ( CFile& file ) override final;
    CArchiveTranslator*     CreateZIPArchive        ( CFile& file ) override final;

    using CFileSystemInterface::OpenIMGArchive;
    using CFileSystemInterface::CreateIMGArchive;

    CIMGArchiveTranslatorHandle*    OpenIMGArchiveDirect    ( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode ) override final;
    CIMGArchiveTranslatorHandle*    CreateIMGArchiveDirect  ( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode ) override final;

    CIMGArchiveTranslatorHandle*    OpenIMGArchive      ( CFileTranslator *srcRoot, const char *srcPath, bool writeAccess, bool isLiveMode ) override final;
    CIMGArchiveTranslatorHandle*    CreateIMGArchive    ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version, bool isLiveMode ) override final;

    CIMGArchiveTranslatorHandle*    OpenIMGArchive      ( CFileTranslator *srcRoot, const wchar_t *srcPath, bool writeAccess, bool isLiveMode ) override final;
    CIMGArchiveTranslatorHandle*    CreateIMGArchive    ( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version, bool isLiveMode ) override final;

    CIMGArchiveTranslatorHandle*    OpenIMGArchive      ( CFileTranslator *srcRoot, const char8_t *srcPath, bool writeAccess, bool isLiveMode ) override final;
    CIMGArchiveTranslatorHandle*    CreateIMGArchive    ( CFileTranslator *srcRoot, const char8_t *srcPath, eIMGArchiveVersion version, bool isLiveMode ) override final;

    using CFileSystemInterface::OpenCompressedIMGArchive;
    using CFileSystemInterface::CreateCompressedIMGArchive;

    // Special functions for IMG archives that should support compression.
    CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchiveDirect  ( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode ) override final;
    CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchiveDirect( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode ) override final;

    CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive    ( CFileTranslator *srcRoot, const char *srcPath, bool writeAccess, bool isLiveMode ) override final;
    CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive  ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version, bool isLiveMode ) override final;

    CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive    ( CFileTranslator *srcRoot, const wchar_t *srcPath, bool writeAccess, bool isLiveMode ) override final;
    CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive  ( CFileTranslator *srcRoot, const wchar_t *arcPath, eIMGArchiveVersion version, bool isLiveMode ) override final;

    CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive    ( CFileTranslator *srcRoot, const char8_t *srcPath, bool writeAccess, bool isLiveMode ) override final;
    CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive  ( CFileTranslator *srcRoot, const char8_t *arcPath, eIMGArchiveVersion version, bool isLiveMode ) override final;

    // Function to cast a CFileTranslator into a CArchiveTranslator.
    // If not possible, it returns nullptr.
    CArchiveTranslator*     GetArchiveTranslator    ( CFileTranslator *translator );

    // Temporary directory generation for temporary data storage.
    CFileTranslator*        GenerateTempRepository  ( void );
    void                    DeleteTempRepository    ( CFileTranslator *repo );

    CFile*                  GenerateRandomFile      ( CFileTranslator *root, bool forcedReliability = false );

    CFile*                  CreateUserBufferFile    ( void *buf, size_t bufSize ) override;
    CFile*                  CreateMemoryFile        ( void ) override;
    const void*             GetMemoryFileBuffer     ( CFile *stream, size_t& sizeOut ) const;
    CFile*                  CreateRWCallbackFile    ( FileSystem::readWriteCallbackInterface *intf ) override;

    CFile*                  WrapStreamBuffered      ( CFile *stream, bool deleteOnQuit, size_t bufSize = 0 ) override;

    CFileTranslator*        CreateRamdisk           ( bool isCaseSensitive ) override;

    // Common utilities.
    CFile*                  CreateFileIterative     ( CFileTranslator *trans, const char *prefix, const char *suffix, unsigned int maxDupl );
    CFile*                  CreateFileIterative     ( CFileTranslator *trans, const wchar_t *prefix, const wchar_t *suffix, unsigned int maxDupl );
    CFile*                  CreateFileIterative     ( CFileTranslator *trans, const char8_t *prefix, const char8_t *suffix, unsigned int maxDupl );

    // Disk management API.
    void                    RegisterDiskNotification    ( FileSystem::CLogicalDiskUpdateNotificationInterface *intf );
    void                    UnregisterDiskNotification  ( FileSystem::CLogicalDiskUpdateNotificationInterface *intf );

    bool                    GetMountPointDeviceInfo     ( const wchar_t *mntpt, FileSystem::mountPointDeviceInfo& devinfo_out );

    bool                    MountDevice     ( const FileSystem::mountPointDeviceInfo& devinfo, const wchar_t *mntpt );
    bool                    RemountDevice   ( const FileSystem::mountPointDeviceInfo& devinfo, const wchar_t *oldmnt, const wchar_t *newmnt );
    bool                    UnmountDevice   ( const FileSystem::mountPointDeviceInfo& devInfo, const wchar_t *mntpt );

    // LZO Compression tools.
    bool                            IsStreamLZOCompressed   ( CFile *stream ) const;
    CIMGArchiveCompressionHandler*  CreateLZOCompressor     ( void );
    void                            DestroyLZOCompressor    ( CIMGArchiveCompressionHandler *handler );

    // ZLIB Compression tools.
    void                    DecompressZLIBStream    ( CFile *input, CFile *output, size_t inputSize, bool hasHeader ) const;
    void                    CompressZLIBStream      ( CFile *input, CFile *output, bool putHeader ) const;

    // Open failure codes in case the Open method of any file translator returns nullptr.
    void                    ResetLastTranslatorOpenFailure  ( void );
    eFileOpenFailure        GetLastTranslatorOpenFailure    ( void );

    fsStaticVector <FileSystem::mountPointInfo>     GetCurrentMountPoints( void ) const;

    // Insecure functions
    bool                    IsDirectory             ( const char *path ) override final;
    bool                    Exists                  ( const char *path ) override final;
    size_t                  Size                    ( const char *path ) override final;
    bool                    ReadToBuffer            ( const char *path, fsDataBuffer& output ) override final;

    // Settings.
    void                    SetIncludeAllDirectoriesInScan  ( bool enable ) final       { m_includeAllDirsInScan = enable; }
    bool                    GetIncludeAllDirectoriesInScan  ( void ) const final        { return m_includeAllDirsInScan; }

    void                    SetDoBufferAllRaw       ( bool enable ) final   { m_doBufferAllRaw = enable; }
    bool                    GetDoBufferAllRaw       ( void ) const final    { return m_doBufferAllRaw; }

#ifdef _WIN32
    void                    SetUseExtendedPaths     ( bool enable )         { m_useExtendedPaths = enable; }
    bool                    GetUseExtendedPaths     ( void ) const          { return m_useExtendedPaths; }
#endif //_WIN32

    // Helpers.
    bool                    IsInLegacyMode          ( void ) const;

    // Members.
    bool                    m_includeAllDirsInScan;     // decides whether ScanDir implementations should apply patterns on directories
#ifdef _WIN32
    bool                    m_hasDirectoryAccessPriviledge; // decides whether directories can be locked by the application
#endif //_WIN32
    bool                    m_doBufferAllRaw;   // if true then every raw FS stream is buffered in application.
#ifdef _WIN32
    bool                    m_useExtendedPaths;     // if true then paths are passed to OS in extended notation whenever possible (enabled by default)
#endif //_WIN32

    // Legacy compatibility features.
#ifdef _WIN32
    bool                    m_win32HasLegacyPaths;
#endif

    filesysPathProcessMode  m_defaultPathProcessMode;
};

// These variables are exported for easy usage by the application.
// It is common sense that an application has a local filesystem and resides on a space.
extern CFileSystem *fileSystem;     // the local filesystem
extern CFileTranslator *fileRoot;   // the space it resides on

// Global static heap allocator for object memory.
struct FSObjectHeapAllocator
{
    static AINLINE void* Allocate( void *refMem, size_t memSize, size_t alignment ) noexcept
    {
        assert( fileSystem != nullptr );

        return fileSystem->MemAlloc( memSize, alignment );
    }

    static AINLINE bool Resize( void *refMem, void *memPtr, size_t memSize ) noexcept
    {
        assert( fileSystem != nullptr );

        return fileSystem->MemResize( memPtr, memSize );
    }

    static AINLINE void Free( void *refMem, void *memPtr ) noexcept
    {
        assert( fileSystem != nullptr );

        fileSystem->MemFree( memPtr );
    }
};

#include "CFileSystem.qol.h"

#endif //_CFileSystem_
