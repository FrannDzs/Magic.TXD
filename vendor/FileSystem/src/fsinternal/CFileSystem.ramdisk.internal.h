/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.img.translator.cpp
*  PURPOSE:     Anonymous on-memory filesystem tree implementation
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_RAMDISK_INTERNAL_HEADER_
#define _FILESYSTEM_RAMDISK_INTERNAL_HEADER_

#include "CFileSystem.internal.h"
#include "CFileSystem.vfs.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4250)
#endif //_MSC_VER

struct CRamdiskTranslator final : public CSystemPathTranslator <pathcheck_always_allow, basicRootPathType <pathcheck_always_allow>>, CFileTranslatorWideWrap
{
    inline CRamdiskTranslator( bool isCaseSensitive ) : CSystemPathTranslator( basicRootPathType <pathcheck_always_allow> (), false, isCaseSensitive ), vfsTree( isCaseSensitive )
    {
        this->vfsTree.hostTranslator = this;
    }

    // Need to provide basic translator methods.
    bool CreateDir( const char *dir, filesysPathOperatingMode opMode, bool createParentDirs ) override
    {
        return this->vfsTree.CreateDir( dir, opMode, createParentDirs );
    }

    CFile* Open( const char *path, const filesysOpenMode& mode, eFileOpenFlags flags ) override
    {
        return this->vfsTree.OpenStream( path, mode );
    }

    bool Exists( const char *path ) const override
    {
        return this->vfsTree.Exists( path );
    }

    bool Delete( const char *path, filesysPathOperatingMode opMode ) override
    {
        return this->vfsTree.Delete( path, opMode );
    }

    bool Copy( const char *src, const char *dst ) override
    {
        return this->vfsTree.Copy( src, dst );
    }

    bool Rename( const char *src, const char *dst ) override
    {
        return this->vfsTree.Rename( src, dst );
    }

    size_t Size( const char *path ) const override
    {
        return (size_t)this->vfsTree.Size( path );
    }

    bool QueryStats( const char *path, filesysStats& statsOut ) const noexcept override
    {
        return this->vfsTree.QueryStats( path, statsOut );
    }

    void ScanDirectory( const char *directory, const char *wildcard, bool recurse, pathCallback_t dirCallback, pathCallback_t fileCallback, void *ud ) const override
    {
        this->vfsTree.ScanDirectory( directory, wildcard, recurse, dirCallback, fileCallback, ud );
    }

    static void _scanFindCallback( const filePath& path, dirNames *output )
    {
        output->AddToBack( path );
    }

    void GetDirectories( const char *directory, const char *wildcard, bool recurse, dirNames& output ) const override
    {
        this->vfsTree.ScanDirectory( directory, wildcard, recurse, (pathCallback_t)_scanFindCallback, nullptr, &output );
    }

    void GetFiles( const char *directory, const char *wildcard, bool recurse, dirNames& output ) const override
    {
        this->vfsTree.ScanDirectory( directory, wildcard, recurse, nullptr, (pathCallback_t)_scanFindCallback, &output );
    }

    CDirectoryIterator* BeginDirectoryListing( const char *path, const char *wildcard, const scanFilteringFlags& filter_flags ) const override
    {
        return this->vfsTree.BeginDirectoryListing( path, wildcard, filter_flags );
    }

    void OnRemoveRemoteFile( const dirNames& treeToFile, const internalFilePath& fileName )
    {
        // Since we keep no meta-data this does not matter I think.
    }

protected:
    bool OnConfirmDirectoryChange( const translatorPathResult& nodePath ) override
    {
        assert( nodePath.type == eRequestedPathResolution::RELATIVE_PATH );
        assert( nodePath.relpath.backCount == 0 );
        // NOTE: we can be a file-path because all it means that the ending slash is not present.
        //  this is valid in ambivalent path-process-mode.

        return this->vfsTree.ChangeDirectory( nodePath.relpath.travelNodes );
    }

private:
    struct fileMetaData
    {
        inline fileMetaData( CRamdiskTranslator *hostTrans, VirtualFileSystem::fileInterface *intf )
        {
            this->dataStream = fileSystem->CreateMemoryFile();

            assert( this->dataStream != nullptr );

            // Fresh time.
            this->mtime = time( nullptr );
            this->ctime = time( nullptr );
            this->atime = time( nullptr );

            this->hasUsingStream = false;
        }

        inline ~fileMetaData( void )
        {
            assert( this->hasUsingStream == false );

            delete this->dataStream;
        }

        void Reset( void )
        {
            assert( this->hasUsingStream == false );

            // Reset data.
            CFile *dataStream = this->dataStream;

            dataStream->Seek( 0, SEEK_SET );
            dataStream->SetSeekEnd();

            // Fresh time.
            this->mtime = time( nullptr );
            this->ctime = time( nullptr );
            this->atime = time( nullptr );
        }

        fsOffsetNumber_t GetSize( void ) const
        {
            return this->dataStream->GetSizeNative();
        }

        bool IsLocked( void ) const
        {
            return ( this->hasUsingStream );
        }

        void OnFileDelete( void )
        {
            // No meta-data to get rid of because we are not persistent.
            return;
        }

        bool OnFileRename( const dirNames& treeToFile, const internalFilePath& fileName )
        {
            // Nothing to change.
            return true;
        }

        bool OnFileCopy( const dirNames& treeToFile, const internalFilePath& fileName )
        {
            // Nothing to take over.
            return true;
        }

        bool CopyAttributesTo( fileMetaData& dstData )
        {
            // No attributes to take over.
            return true;
        }

        void GetANSITimes( time_t& mtimeOut, time_t& ctimeOut, time_t& atimeOut ) const
        {
            mtimeOut = this->mtime;
            ctimeOut = this->ctime;
            atimeOut = this->atime;
        }

        void GetDeviceIdentifier( dev_t& idOut ) const
        {
            // Nothing. We are anonymous.
            idOut = 0;
        }

        CFile *dataStream;
        time_t mtime, ctime, atime;

        bool hasUsingStream;    // for now we only allow one stream at a time.
    };

    struct directoryMetaData
    {
        inline directoryMetaData( CRamdiskTranslator *hostTrans, VirtualFileSystem::directoryInterface *intf )
        {
            return;
        }
    };

    typedef CVirtualFileSystem <CRamdiskTranslator, directoryMetaData, fileMetaData> vfs_t;

    typedef vfs_t::file file;
    typedef vfs_t::directory directory;

    // Native stream management.
    struct fileRedirStream final : public CFile
    {
        inline fileRedirStream( fileMetaData *theFile, bool isReadable, bool isWriteable )
        {
            this->linkedFile = theFile;
            this->curSeek = 0;
            this->canRead = isReadable;
            this->canWrite = isWriteable;

            theFile->hasUsingStream = true;
        }

        ~fileRedirStream( void )
        {
            this->linkedFile->hasUsingStream = false;
        }

        size_t Read( void *bufOut, size_t readCount ) override
        {
            if ( this->canRead == false )
            {
                return 0;
            }

            CFile *memStream = this->linkedFile->dataStream;

            memStream->SeekNative( this->curSeek, SEEK_SET );

            size_t actualReadCount = memStream->Read( bufOut, readCount );

            this->curSeek = memStream->TellNative();

            return actualReadCount;
        }

        size_t Write( const void *bufIn, size_t writeCount ) override
        {
            if ( this->canWrite == false )
            {
                return 0;
            }

            CFile *memStream = this->linkedFile->dataStream;

            memStream->SeekNative( this->curSeek, SEEK_SET );

            size_t actualWriteCount = memStream->Write( bufIn, writeCount );

            this->curSeek = memStream->TellNative();

            return actualWriteCount;
        }

        int Seek( long iOffset, int type ) override
        {
            long base;

            if ( type == SEEK_SET )
            {
                base = 0;
            }
            else if ( type == SEEK_CUR )
            {
                base = (long)this->curSeek;
            }
            else if ( type == SEEK_END )
            {
                base = (long)this->linkedFile->dataStream->GetSizeNative();
            }
            else
            {
                return -1;
            }

            this->curSeek = (long)( base + iOffset );

            return 0;
        }

        int SeekNative( fsOffsetNumber_t iOffset, int type ) override
        {
            fsOffsetNumber_t base;

            if ( type == SEEK_SET )
            {
                base = 0;
            }
            else if ( type == SEEK_CUR )
            {
                base = this->curSeek;
            }
            else if ( type == SEEK_END )
            {
                base = this->linkedFile->dataStream->GetSizeNative();
            }
            else
            {
                return -1;
            }

            this->curSeek = ( base + iOffset );

            return 0;
        }

        long Tell( void ) const override
        {
            return (long)this->curSeek;
        }

        fsOffsetNumber_t TellNative( void ) const override
        {
            return this->curSeek;
        }

        bool IsEOF( void ) const override
        {
            return ( this->curSeek >= this->linkedFile->dataStream->GetSizeNative() );
        }

        bool QueryStats( filesysStats& statsOut ) const override
        {
            const fileMetaData *linkedFile = this->linkedFile;

            statsOut.atime = linkedFile->atime;
            statsOut.mtime = linkedFile->mtime;
            statsOut.ctime = linkedFile->ctime;
            statsOut.attribs.type = eFilesysItemType::FILE;
            // TODO: improve the filling of this structure.
            return true;
        }

        void SetFileTimes( time_t atime, time_t ctime, time_t mtime ) override
        {
            return;
        }

        void SetSeekEnd( void ) override
        {
            CFile *memStream = this->linkedFile->dataStream;

            memStream->SeekNative( this->curSeek, SEEK_SET );
            memStream->SetSeekEnd();
        }

        size_t GetSize( void ) const override
        {
            return this->linkedFile->dataStream->GetSize();
        }

        fsOffsetNumber_t GetSizeNative( void ) const override
        {
            return this->linkedFile->dataStream->GetSizeNative();
        }

        void Flush( void ) override
        {
            this->linkedFile->dataStream->Flush();
        }

        CFileMappingProvider* CreateMapping( void ) override
        {
            return this->linkedFile->dataStream->CreateMapping();
        }

        filePath GetPath( void ) const override
        {
            return "<ramdisk_file>";
        }

        bool IsReadable( void ) const noexcept override
        {
            return this->canRead;
        }

        bool IsWriteable( void ) const noexcept override
        {
            return this->canWrite;
        }

        fileMetaData *linkedFile;
        fsOffsetNumber_t curSeek;
        bool canWrite;
        bool canRead;
    };

public:
    CFile* OpenNativeFileStream( file *theFile, eVFSFileOpenMode mode, const filesysAccessFlags& accessFlags )
    {
        fileRedirStream *stream = new fileRedirStream( &theFile->metaData, accessFlags.allowRead, accessFlags.allowWrite );

        return stream;
    }

private:
    vfs_t vfsTree;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER

#endif //_FILESYSTEM_RAMDISK_INTERNAL_HEADER_
