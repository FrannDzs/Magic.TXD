/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.FileDataPresence.h
*  PURPOSE:     File data presence scheduling
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_DATA_PRESENCE_SCHEDULING_
#define _FILESYSTEM_DATA_PRESENCE_SCHEDULING_

struct CFileDataPresenceManager
{
    CFileDataPresenceManager( CFileSystemNative *fileSys );
    ~CFileDataPresenceManager( void );

    void SetMaximumDataQuotaRAM( size_t maxQuota );
    void UnsetMaximumDataQuotaRAM( void );

    CFile* AllocateTemporaryDataDestination( fsOffsetNumber_t minimumExpectedSize = 0 );

private:
    typedef sliceOfData <fsOffsetNumber_t> fileStreamSlice_t;
    
    enum class eFilePresenceType
    {
        MEMORY,
        LOCALFILE
    };

    // TODO: this has to be turned into a managed object.
    struct swappableDestDevice : public CFile
    {
        inline swappableDestDevice( CFileDataPresenceManager *manager, CFile *dataSource, eFilePresenceType presenceType )
        {
            this->manager = manager;
            this->dataSource = dataSource;
            this->presenceType = presenceType;
            this->lastRegisteredFileSize = dataSource->GetSizeNative();

            LIST_INSERT( manager->activeFiles.root, this->node );

            // If we are a memory file, increase the RAM total to initial count.
            if ( presenceType == eFilePresenceType::MEMORY )
            {
                manager->IncreaseRAMTotal( this, this->lastRegisteredFileSize );
            }

            // Initialize the stats as something useful.
            time_t curtime = time( nullptr );

            this->meta_atime = curtime;
            this->meta_ctime = curtime;
            this->meta_mtime = curtime;

            // What to do with these?
            this->isReadable = true;
            this->isWriteable = true;
        }

        inline ~swappableDestDevice( void )
        {
            LIST_REMOVE( this->node );

            if ( CFile *sourceMem = this->dataSource )
            {
                bool wasDestroyed = false;

                if ( this->presenceType == eFilePresenceType::MEMORY )
                {
                    // If we are a RAM file, remove our memory usage from the stats.
                    this->manager->DecreaseRAMTotal( this, this->lastRegisteredFileSize );
                }
                else if ( this->presenceType == eFilePresenceType::LOCALFILE )
                {
                    // If we are a local FS file, we should be deleted from disk aswell.
                    this->manager->CleanupLocalFile( sourceMem );

                    // file link has been destroyed by cleanup routine.
                    wasDestroyed = true;
                }

                if ( !wasDestroyed )
                {
                    delete sourceMem;
                }

                this->dataSource = nullptr;
            }
        }

        // We need to manage read and writes.
        size_t Read( void *buffer, size_t readCount ) override;
        size_t Write( const void *buffer, size_t writeCount ) override;

        int Seek( long offset, int iType ) override                         { return dataSource->Seek( offset, iType ); }
        int SeekNative( fsOffsetNumber_t offset, int iType ) override       { return dataSource->SeekNative( offset, iType ); }

        long Tell( void ) const override                                    { return dataSource->Tell(); }
        fsOffsetNumber_t TellNative( void ) const override                  { return dataSource->TellNative(); }

        bool IsEOF( void ) const override                                   { return dataSource->IsEOF(); }

        bool QueryStats( filesysStats& statsOut ) const override;
        void SetFileTimes( time_t atime, time_t ctime, time_t mtime ) override;

        void SetSeekEnd( void ) override;

        size_t GetSize( void ) const override                               { return dataSource->GetSize(); }
        fsOffsetNumber_t GetSizeNative( void ) const override               { return dataSource->GetSizeNative(); }

        void Flush( void ) override                                         { return dataSource->Flush(); }

        // Cannot provide sufficient file mapping because we swap streams.
        CFileMappingProvider* CreateMapping( void ) override                { return nullptr; }

        filePath GetPath( void ) const override                             { return dataSource->GetPath(); }

        bool IsReadable( void ) const noexcept override                     { return isReadable; }
        bool IsWriteable( void ) const noexcept override                    { return isWriteable; }

        CFileDataPresenceManager *manager;
        CFile *dataSource;

        fsOffsetNumber_t lastRegisteredFileSize;

        eFilePresenceType presenceType;

        RwListEntry <swappableDestDevice> node;

        // File statistics meta-data (required).
        time_t meta_atime;
        time_t meta_mtime;
        time_t meta_ctime;

        bool isReadable, isWriteable;
    };

    inline size_t       GetFileMemoryFadeInSize( void ) const
    {
        return (size_t)( this->fileMaxSizeInRAM * this->percFileMemoryFadeIn );
    }

    void    IncreaseRAMTotal( swappableDestDevice *memFile, fsOffsetNumber_t memSize );
    void    DecreaseRAMTotal( swappableDestDevice *memFile, fsOffsetNumber_t memSize );

    CFileTranslator*    GetLocalFileTranslator( void );

    void    NotifyFileSizeChange( swappableDestDevice *file, fsOffsetNumber_t newProposedSize );
    void    UpdateFileSizeMetrics( swappableDestDevice *file );

    void    CleanupLocalFile( CFile *file ) noexcept;

    // Members.
    CFileSystemNative *fileSys;

    RwList <swappableDestDevice> activeFiles;

    // Data repository parameters.
    CFileTranslator *volatile onDiskTempRoot;

    // Runtime configuration.
    size_t maximumDataQuotaRAM;
    bool hasMaximumDataQuotaRAM;
    size_t fileMaxSizeInRAM;

    float percFileMemoryFadeIn; // 0..1 percentage that if the file size goes below the file can be put into memory again.

    // Statistics.
    fsOffsetNumber_t totalRAMMemoryUsageByFiles;
};

#endif //_FILESYSTEM_DATA_PRESENCE_SCHEDULING_