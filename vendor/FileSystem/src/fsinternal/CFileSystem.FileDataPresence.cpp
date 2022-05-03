/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.FileDataPresence.cpp
*  PURPOSE:     File data presence scheduling
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// TODO: port to Linux.
// TODO: some users complained that removable devices were still queried; fix this.

#include "StdInc.h"

// Sub modules.
#include "CFileSystem.platform.h"
#include "CFileSystem.internal.h"
#include "CFileSystem.stream.memory.h"
#include "CFileSystem.translator.system.h"
#include "CFileSystem.platformutils.hxx"

#include "CFileSystem.lock.hxx"

#include "CFileSystem.Utils.hxx"

#ifdef _WIN32
#include "CFileSystem.native.win32.hxx"
#endif //_WIN32

#include <sdk/PluginHelpers.h>
#include <sdk/Set.h>

// We need OS features to request device capabilities.

using namespace FileSystem;

#ifdef _WIN32
inline eir::String <wchar_t, FileSysCommonAllocator> getDriveRootDesc( wchar_t driveChar )
{
    wchar_t rootBuf[4];
    rootBuf[0] = driveChar;
    rootBuf[1] = L':';
    rootBuf[2] = L'/';
    rootBuf[3] = L'\0';

    return ( rootBuf );
}
#endif //_WIN32

// Should request immutable information from non-removable disk drives in a management struct.
struct fileDataPresenceEnvInfo
{
    inline void Initialize( CFileSystemNative *fileSys )
    {
        this->fileSys = fileSys;
        this->hasInitializedDriveTrauma = false;

        // We should store an access device which is best suited to write temporary files to.
        // Our users will greatly appreciate this effort.
        // I know certain paranoid people that complain if you "trash [their] SSD drive".
        this->bestTempDriveRoot = nullptr;

        this->sysTempRoot = nullptr;        // allocated on demand inside temp drive root or global system root.
    }

    inline void Shutdown( CFileSystemNative *fileSys )
    {
        // We might want to even delete the folder of the application temp root, not sure.
        if ( CFileTranslator *tmpRoot = this->sysTempRoot )
        {
            delete tmpRoot;

            this->sysTempRoot = nullptr;
        }

        // Destroy our access to the temporary root again.
        if ( CFileTranslator *tmpRoot = this->bestTempDriveRoot )
        {
            delete tmpRoot;

            this->bestTempDriveRoot = nullptr;
        }

        this->hasInitializedDriveTrauma = false;
    }

    // We have to guard the access to the temporary root.
    // PLATFORM CODE.
    inline CFileTranslator* GetSystemTempDriveRoot( void )
    {
        // TODO: add lock here.

        if ( !this->hasInitializedDriveTrauma )
        {
            // Initialize things.
            CFileTranslator *bestTempDriveRoot = nullptr;

            try
            {
                auto mntpts_unsorted = fileSystem->GetCurrentMountPoints();

                struct mntpt_freeSpace_comparator
                {
                    static AINLINE eir::eCompResult compare_values( const mountPointInfo& left, const mountPointInfo& right )
                    {
                        return eir::flip_comp_result( eir::DefaultValueCompare( left.freeSpace, right.freeSpace ) );
                    }
                };

                eir::Set <mountPointInfo, FSObjectHeapAllocator, mntpt_freeSpace_comparator> mntpts_sorted;
                
                for ( auto& mntpt : mntpts_unsorted )
                {
                    // We do not want to care about removable stuff.
                    if ( mntpt.isRemovable == false )
                    {
                        mntpts_sorted.Insert( std::move( mntpt ) );
                    }
                }

                // We need to select the spindle drive with most free space as best place to put files at.
                // If we dont have any such spindle drive, we settle for any drive with most free space.
                for ( auto& mntpt : mntpts_sorted )
                {
                    if ( mntpt.mediaTypes.Find( eDiskMediaType::ROTATING_SPINDLE ) )
                    {
                        if ( CFileTranslator *tmpRoot = fileSys->CreateTranslator( mntpt.mnt_path ) )
                        {
                            bestTempDriveRoot = tmpRoot;
                            break;
                        }
                    }
                }

                // If we could not find any of rotating spindle, we pick any other.
                if ( bestTempDriveRoot == nullptr )
                {
                    for ( auto& mntpt : mntpts_sorted )
                    {
                        // Now try all unspecified ones.
                        if ( mntpt.mediaTypes.Find( eDiskMediaType::ROTATING_SPINDLE ) == nullptr && mntpt.mediaTypes.Find( eDiskMediaType::SOLID_STATE ) == nullptr )
                        {
                            if ( CFileTranslator *tempRoot = fileSys->CreateTranslator( mntpt.mnt_path ) )
                            {
                                bestTempDriveRoot = tempRoot;
                                break;
                            }
                        }
                    }
                }

                // Last we try the solid state ones.
                if ( bestTempDriveRoot == nullptr )
                {
                    for ( auto& mntpt : mntpts_sorted )
                    {
                        if (  mntpt.mediaTypes.Find( eDiskMediaType::SOLID_STATE ) )
                        {
                            if ( CFileTranslator *tempRoot = fileSys->CreateTranslator( mntpt.mnt_path ) )
                            {
                                bestTempDriveRoot = tempRoot;
                                break;
                            }
                        }
                    }
                }
            }
            catch( ... )
            {
                // Usually this must not happen.
                if ( bestTempDriveRoot )
                {
                    delete bestTempDriveRoot;
                }

                throw;
            }

            // If we could decide one, set it.
            if ( bestTempDriveRoot )
            {
                this->bestTempDriveRoot = bestTempDriveRoot;
            }

            // If we could not establish _any_ safe temporary root, we will have to allocate all files on RAM.
            // This is entirely possible but of course we wish to put files on disk to not stress the user too much.
            // Applications should display a warning that there is no efficient temp disk storage available.

            this->hasInitializedDriveTrauma = true;
        }

        return this->bestTempDriveRoot;
    }

    CFileTranslator *volatile sysTempRoot;

private:
    CFileSystemNative *fileSys;

    // Immutable properties fetched from the OS.
    bool hasInitializedDriveTrauma;

    CFileTranslator *bestTempDriveRoot;
};

static PluginDependantStructRegister <fileDataPresenceEnvInfo, fileSystemFactory_t> fileDataPresenceEnvInfoRegister;

static fsLockProvider _fileSysTmpDirLockProvider;

static void GetSystemTemporaryRootPath( filePath& absPathOut )
{
    filePath tmpDirBase;

#ifdef _WIN32
    wchar_t buf[2048];

    GetTempPathW( NUMELMS( buf ) - 1, buf );

    buf[ NUMELMS( buf ) - 1 ] = 0;

    // Transform the path into something we can recognize.
    tmpDirBase.insert( 0, buf, 2 );
    tmpDirBase += FileSystem::GetDirectorySeparator <wchar_t> ( true );

    normalNodePath normalPath = _File_NormalizeRelativePath( buf + 3, pathcheck_win32() );

    assert( normalPath.isFilePath == false && normalPath.backCount == 0 );

    _File_OutputPathTree( normalPath.travelNodes, normalPath.isFilePath, true, tmpDirBase );
#elif defined(__linux__)
    const char *dir = getenv("TEMPDIR");

    if ( !dir )
        tmpDirBase = "/tmp";
    else
        tmpDirBase = dir;

    tmpDirBase += '/';

    // On linux we cannot be sure that our directory exists.
    if ( !_File_CreateDirectory( tmpDirBase ) )
    {
        assert( 0 );

        exit( 7098 );
    }
#endif //OS DEPENDANT CODE

    absPathOut = std::move( tmpDirBase );
}

// PLATFORM CODE.
static bool IsPathOnSystemDrive( const wchar_t *sysPath )
{
#ifdef _WIN32
    // We define the system drive as host of the Windows directory.
    // This thing does only make sense on Windows systems anyway.
    // Refactoring so that we support both Linux and Windows has to be done again at some point.

    UINT charCount = GetWindowsDirectoryW( nullptr, 0 );

    eir::Vector <wchar_t, FileSysCommonAllocator> winDirPath;

    winDirPath.Resize( charCount + 1 );

    GetWindowsDirectoryW( (wchar_t*)winDirPath.GetData(), charCount );

    winDirPath[ charCount ] = L'\0';

    platform_rootPathType rootSysPath;
    {
        if ( !rootSysPath.buildFromSystemPath( sysPath, false ) )
        {
            return false;
        }
    }

    platform_rootPathType rootWinDirPath;
    {
        if ( !rootWinDirPath.buildFromSystemPath( winDirPath.GetData(), false ) )
        {
            return false;
        }
    }

    // Check whether the drives/volumes match.
    return rootSysPath.doesRootDescriptorMatch( rootWinDirPath );
#else
    // Else we just say that everything is on the system drive.
    // Might reapproach this sometime.
    return true;
#endif
}

// Temporary root management.
CFileTranslator* CFileSystem::GenerateTempRepository( void )
{
    CFileSystemNative *fileSys = (CFileSystemNative*)this;

    fileDataPresenceEnvInfo *envInfo = fileDataPresenceEnvInfoRegister.GetPluginStruct( fileSys );

    if ( !envInfo )
        return nullptr;

    filePath tmpDirBase;

    // Check whether we have a handle to the global temporary system storage.
    // If not, attempt to retrieve it.
    bool needsTempDirFetch = true;

    if ( !envInfo->sysTempRoot )
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CReadWriteWriteContextSafe <> consistency( _fileSysTmpDirLockProvider.GetReadWriteLock( this ) );
#endif //FILESYS_MULTI_THREADING

        if ( !envInfo->sysTempRoot )
        {
            // Check if we have a recommended system temporary root drive.
            // If we do, then we should get a temp root in there.
            // Otherwise we simply resort to the OS main temp dir.
            bool hasTempRoot = false;

            if ( !hasTempRoot )
            {
                if ( CFileTranslator *recTmpRoot = envInfo->GetSystemTempDriveRoot() )
                {
                    // Is it on the system drive?
                    filePath fullPathOfTemp;

                    recTmpRoot->GetFullPathFromRoot( L"", false, fullPathOfTemp );

                    fullPathOfTemp.transform_to <wchar_t> ();

                    if ( IsPathOnSystemDrive( fullPathOfTemp.w_str() ) == false )
                    {
                        // We can create a generic temporary root.
                        tmpDirBase = ( fullPathOfTemp + L"Temp/" );

                        // It of course has to succeed in creation, too!
                        if ( recTmpRoot->CreateDir( tmpDirBase ) )
                        {
                            hasTempRoot = true;
                        }
                    }
                }
            }

            if ( !hasTempRoot )
            {
                GetSystemTemporaryRootPath( tmpDirBase );

                hasTempRoot = true;
            }

            // If we have a temp root, we can do things.
            if ( !hasTempRoot )
                return nullptr;

            envInfo->sysTempRoot = fileSystem->CreateTranslator( tmpDirBase );

            // We failed to get the handle to the temporary storage, hence we cannot deposit temporary files!
            if ( !envInfo->sysTempRoot )
                return nullptr;

            needsTempDirFetch = false;
        }
    }

    if ( needsTempDirFetch )
    {
        bool success = envInfo->sysTempRoot->GetFullPath( "//", false, tmpDirBase );

        if ( !success )
            return nullptr;
    }

    // Generate a random sub-directory inside of the global OS temp directory.
    // We need to generate until we find a unique directory.
    unsigned int numOfTries = 0;

    filePath tmpDir;

    while ( numOfTries < 50 )
    {
        filePath tmpDir( tmpDirBase );

        auto randNum = eir::to_string <char, FSObjectHeapAllocator> ( fsrandom::getSystemRandom( this ) );

        tmpDir += "&$!reAr";
        tmpDir += randNum;
        tmpDir += "_/";

        if ( envInfo->sysTempRoot->Exists( tmpDir ) == false )
        {
            // Once we found a not existing directory, we must create and acquire a handle
            // to it. This operation can fail if somebody else happens to delete the directory
            // inbetween or snatched away the handle to the directory before us.
            // Those situations are very unlikely, but we want to make sure anyway, for quality's sake.

            // Make sure the temporary directory exists.
            bool creationSuccessful = _File_CreateDirectory( tmpDir );

            if ( creationSuccessful )
            {
                // Create the temporary root
                CFileTranslator *result = fileSystem->CreateTranslator( tmpDir, DIR_FLAG_EXCLUSIVE );

                if ( result )
                {
                    // Success!
                    return result;
                }
            }

            // Well, we failed for some reason, so try again.
        }

        numOfTries++;
    }

    // Nope. Maybe the user wants to try again?
    return nullptr;
}

void CFileSystem::DeleteTempRepository( CFileTranslator *repo )
{
    CFileSystemNative *fileSys = (CFileSystemNative*)this;

    fileDataPresenceEnvInfo *envInfo = fileDataPresenceEnvInfoRegister.GetPluginStruct( fileSys );

    CFileTranslator *sysTmp = nullptr;

    if ( envInfo )
    {
        sysTmp = envInfo->sysTempRoot;
    }

    // Keep deleting like usual.
    filePath pathOfDir;
    bool gotActualPath = false;

    try
    {
        gotActualPath = repo->GetFullPathFromRoot( L"//", false, pathOfDir );
    }
    catch( ... )
    {
        gotActualPath = false;

        // Continue.
    }

    // We can now release the handle to the directory.
    delete repo;

    // We can only really delete if we have the system temporary root.
    if ( sysTmp )
    {
        if ( gotActualPath )
        {
            // Delete us.
            sysTmp->Delete( pathOfDir );
        }
    }
}

CFileDataPresenceManager::CFileDataPresenceManager( CFileSystemNative *fileSys )
{
    LIST_CLEAR( activeFiles.root );

    this->fileSys = fileSys;

    this->onDiskTempRoot = nullptr;     // we initialize this on demand.

    this->maximumDataQuotaRAM = 0;
    this->hasMaximumDataQuotaRAM = false;
    this->fileMaxSizeInRAM = 0x40000;           // todo: properly calculate this value.

    this->percFileMemoryFadeIn = 0.667f;

    // Setup statistics.
    this->totalRAMMemoryUsageByFiles = 0;
}

CFileDataPresenceManager::~CFileDataPresenceManager( void )
{
    // Make sure everyone released active files beforehand.
    assert( LIST_EMPTY( activeFiles.root ) == true );

    // We should have no RAM usage by memory files.
    assert( this->totalRAMMemoryUsageByFiles == 0 );

    // Clean up the temporary root.
    if ( CFileTranslator *tmpRoot = this->onDiskTempRoot )
    {
        this->fileSys->DeleteTempRepository( tmpRoot );

        this->onDiskTempRoot = nullptr;
    }
}

void CFileDataPresenceManager::SetMaximumDataQuotaRAM( size_t maxQuota )
{
    this->maximumDataQuotaRAM = maxQuota;

    this->hasMaximumDataQuotaRAM = true;
}

void CFileDataPresenceManager::UnsetMaximumDataQuotaRAM( void )
{
    this->hasMaximumDataQuotaRAM = false;
}

CFileTranslator* CFileDataPresenceManager::GetLocalFileTranslator( void )
{
    // TODO: add lock here.

    if ( !this->onDiskTempRoot )
    {
        this->onDiskTempRoot = fileSys->GenerateTempRepository();
    }

    return this->onDiskTempRoot;
}

CFile* CFileDataPresenceManager::AllocateTemporaryDataDestination( fsOffsetNumber_t minimumExpectedSize )
{
    // TODO: take into account the minimumExpectedSize parameter!

    CFile *outFile = nullptr;
    {
        // We simply start out by putting the file into RAM.
        CMemoryMappedFile *memFile = new CMemoryMappedFile( this->fileSys );

        if ( memFile )
        {
            try
            {
                // Create our managed wrapper.
                swappableDestDevice *swapDevice = new swappableDestDevice( this, memFile, eFilePresenceType::MEMORY );

                if ( swapDevice )
                {
                    outFile = swapDevice;
                }
            }
            catch( ... )
            {
                delete memFile;

                throw;
            }

            if ( !outFile )
            {
                delete memFile;
            }
        }
    }
    return outFile;
}

void CFileDataPresenceManager::IncreaseRAMTotal( swappableDestDevice *memFile, fsOffsetNumber_t memSize )
{
    this->totalRAMMemoryUsageByFiles += memSize;
}

void CFileDataPresenceManager::DecreaseRAMTotal( swappableDestDevice *memFile, fsOffsetNumber_t memSize )
{
    this->totalRAMMemoryUsageByFiles -= memSize;
}

void CFileDataPresenceManager::NotifyFileSizeChange( swappableDestDevice *file, fsOffsetNumber_t newProposedSize )
{
    eFilePresenceType curPresence = file->presenceType;

    // Check if the file needs movement/where the file should be at.
    eFilePresenceType reqPresence = curPresence;
    {
        bool shouldSwapToLocalFile = false;
        bool shouldSwapToMemory = false;

        if ( curPresence == eFilePresenceType::MEMORY )
        {
            size_t sizeSwapToLocalFile = this->fileMaxSizeInRAM;

            // Check local maximum.
            if ( newProposedSize >= (fsOffsetNumber_t)sizeSwapToLocalFile )
            {
                shouldSwapToLocalFile = true;
            }

            if ( this->hasMaximumDataQuotaRAM )
            {
                // Check global maximum.
                if ( this->totalRAMMemoryUsageByFiles - file->lastRegisteredFileSize + newProposedSize > (fsOffsetNumber_t)this->maximumDataQuotaRAM )
                {
                    shouldSwapToLocalFile = true;
                }
            }
        }
        else if ( curPresence == eFilePresenceType::LOCALFILE )
        {
            // We can shrink enough so that we can get into memory again.
            size_t sizeMoveBackToMemory = this->GetFileMemoryFadeInSize();

            // Check local maximum.
            if ( newProposedSize < (fsOffsetNumber_t)sizeMoveBackToMemory )
            {
                shouldSwapToMemory = true;
            }

            // We do not do operations based on the global maximum.
        }

        if ( shouldSwapToLocalFile )
        {
            reqPresence = eFilePresenceType::LOCALFILE;
        }
        else if ( shouldSwapToMemory )
        {
            reqPresence = eFilePresenceType::MEMORY;
        }
    }

    // Have we even decided that a move makes sense?
    if ( curPresence == reqPresence )
        return;

    // Perform actions that have been decided.
    //bool hasSwapped = false;

    CFile *handleToMoveTo = nullptr;

    if ( reqPresence == eFilePresenceType::LOCALFILE )
    {
        if ( CFileTranslator *localTrans = this->GetLocalFileTranslator() )
        {
            // Try performing said operation once.
            CFile *tempFileOnDisk = this->fileSys->GenerateRandomFile( localTrans );

            // TODO: maybe ask for reliable random files?

            if ( tempFileOnDisk )
            {
                handleToMoveTo = tempFileOnDisk;

                //hasSwapped = true;
            }
        }
    }

    // If we have no handle, no point in continuing.
    if ( !handleToMoveTo )
        return;

    // Copy stuff over.
    try
    {
        CFile *currentDataSource = file->dataSource;

        fsOffsetNumber_t currentSeek = currentDataSource->TellNative();

        currentDataSource->Seek( 0, SEEK_SET );

        FileSystem::StreamCopy( *currentDataSource, *handleToMoveTo );

        handleToMoveTo->SeekNative( currentSeek, SEEK_SET );

        // Success!
        // (it does not matter if we succeed or not, the show must go on)
    }
    catch( ... )
    {
        delete handleToMoveTo;

        throw;
    }

    // Delete old source.
    delete file->dataSource;

    // Register the change.
    file->dataSource = handleToMoveTo;
    file->presenceType = reqPresence;

    // Terminate registration of previous presence type.
    if ( curPresence == eFilePresenceType::MEMORY )
    {
        this->totalRAMMemoryUsageByFiles -= file->lastRegisteredFileSize;

        file->lastRegisteredFileSize = 0;
    }
}

void CFileDataPresenceManager::UpdateFileSizeMetrics( swappableDestDevice *file )
{
    // TODO: maybe verify how often our assumption of file growth were incorrect, so that we
    // exceeded the total "allowed" RAM usage.

    if ( file->presenceType == eFilePresenceType::MEMORY )
    {
        CFile *currentDataSource = file->dataSource;

        fsOffsetNumber_t newFileSize = currentDataSource->GetSizeNative();

        this->totalRAMMemoryUsageByFiles -= file->lastRegisteredFileSize;
        this->totalRAMMemoryUsageByFiles += newFileSize;

        file->lastRegisteredFileSize = newFileSize;
    }
}

void CFileDataPresenceManager::CleanupLocalFile( CFile *file ) noexcept
{
    CFileTranslator *tmpRoot = this->onDiskTempRoot;

    filePath localFilePath;

    try
    {
        localFilePath = file->GetPath();
    }
    catch( ... )
    {
        // We simply cary on.
    }

    delete file;

    if ( tmpRoot && localFilePath.empty() == false )
    {
        tmpRoot->Delete( localFilePath );
    }
}

size_t CFileDataPresenceManager::swappableDestDevice::Read( void *buffer, size_t readCount )
{
    if ( !this->isReadable )
        return 0;

    return dataSource->Read( buffer, readCount );
}

size_t CFileDataPresenceManager::swappableDestDevice::Write( const void *buffer, size_t writeCount )
{
    if ( !this->isWriteable )
        return 0;

    // TODO: guard the size of this file; it must not overshoot certain limits or else the file has to be relocated.
    // TODO: add locks so this file type becomes thread-safe!

    CFileDataPresenceManager *manager = this->manager;

    bool isExpandingOp = false;
    fsOffsetNumber_t expandTo;
    {
        CFile *currentDataSource = this->dataSource;

        // Check if we would increase in size, and if then, check if we are still allowed to have our storage where it is.
        fsOffsetNumber_t currentSeek = currentDataSource->TellNative();
        fsOffsetNumber_t fsWriteCount = (fsOffsetNumber_t)( writeCount );

        fileStreamSlice_t opSlice( currentSeek, fsWriteCount );

        expandTo = ( opSlice.GetSliceEndPoint() + 1 );

        // Get the file bounds.
        fsOffsetNumber_t currentSize = currentDataSource->GetSizeNative();

        fileStreamSlice_t boundsSlice( 0, currentSize );

        // An intersection tells us if we try to access out-of-bounds data.
        eir::eIntersectionResult intResult = opSlice.intersectWith( boundsSlice );

        if ( intResult == eir::INTERSECT_BORDER_START ||
             intResult == eir::INTERSECT_FLOATING_END ||
             intResult == eir::INTERSECT_ENCLOSING )
        {
            isExpandingOp = true;
        }
    }

    // If we are expanding, then we should be wary of by how much.
    if ( isExpandingOp )
    {
        // Need to update file stability.
        manager->NotifyFileSizeChange( this, expandTo );
    }

    // Finish the write operation.
    size_t actualWriteCount = this->dataSource->Write( buffer, writeCount );

    // Update our file size.
    manager->UpdateFileSizeMetrics( this );

    return actualWriteCount;
}

bool CFileDataPresenceManager::swappableDestDevice::QueryStats( filesysStats& statsOut ) const
{
    statsOut.atime = this->meta_atime;
    statsOut.ctime = this->meta_ctime;
    statsOut.mtime = this->meta_mtime;
    statsOut.attribs.type = eFilesysItemType::FILE;
    statsOut.attribs.isTemporary = true;
    return true;
}

void CFileDataPresenceManager::swappableDestDevice::SetFileTimes( time_t atime, time_t ctime, time_t mtime )
{
    this->meta_atime = atime;
    this->meta_mtime = mtime;
    this->meta_ctime = ctime;
}

void CFileDataPresenceManager::swappableDestDevice::SetSeekEnd( void )
{
    // TODO: guard this truncation of file and manage the memory properly.

    // Will the file size change?
    bool willSizeChange = false;
    fsOffsetNumber_t newProposedSize;
    {
        CFile *currentDataSource = this->dataSource;

        fsOffsetNumber_t currentSeek = currentDataSource->TellNative();
        fsOffsetNumber_t currentFileSize = currentDataSource->GetSizeNative();

        if ( currentSeek != currentFileSize )
        {
            willSizeChange = true;

            newProposedSize = std::max( (fsOffsetNumber_t)0, currentSeek );
        }
    }

    if ( willSizeChange )
    {
        manager->NotifyFileSizeChange( this, newProposedSize );
    }

    dataSource->SetSeekEnd();

    // Update file size metrics.
    manager->UpdateFileSizeMetrics( this );
}

// Initialization of this module.
void registerFileDataPresenceManagement( const fs_construction_params& params )
{
    _fileSysTmpDirLockProvider.RegisterPlugin( params );

    fileDataPresenceEnvInfoRegister.RegisterPlugin( _fileSysFactory );
}

void unregisterFileDataPresenceManagement( void )
{
    fileDataPresenceEnvInfoRegister.UnregisterPlugin();

    _fileSysTmpDirLockProvider.UnregisterPlugin();
}
