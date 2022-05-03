/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.lock.hxx
*  PURPOSE:     Threading support definitions for CFileSystem
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_MAIN_THREADING_SUPPORT_
#define _FILESYSTEM_MAIN_THREADING_SUPPORT_

struct fsLockProvider
{
    inline fsLockProvider( void )
    {
        this->pluginOffset = fileSystemFactory_t::INVALID_PLUGIN_OFFSET;
        this->isConstructorRegistered = false;
    }
    inline fsLockProvider( const fs_construction_params& params )
    {
        this->pluginOffset = fileSystemFactory_t::INVALID_PLUGIN_OFFSET;
        this->isConstructorRegistered = true;

        this->RegisterPlugin( params );
    }
    inline fsLockProvider( const fsLockProvider& ) = delete;
    inline fsLockProvider( fsLockProvider&& ) = delete;

    inline ~fsLockProvider( void )
    {
        if ( this->isConstructorRegistered )
        {
            this->UnregisterPlugin();
        }

        assert( this->pluginOffset == fileSystemFactory_t::INVALID_PLUGIN_OFFSET );
    }

    inline fsLockProvider& operator = ( const fsLockProvider& ) = delete;
    inline fsLockProvider& operator = ( fsLockProvider&& ) = delete;

#ifdef FILESYS_MULTI_THREADING
    struct lock_item
    {
        inline void Initialize( CFileSystemNative *fsys )
        {
            fsys->nativeMan->CreatePlacedReadWriteLock( this );
        }

        inline void Shutdown( CFileSystemNative *fsys )
        {
            fsys->nativeMan->ClosePlacedReadWriteLock( (NativeExecutive::CReadWriteLock*)this );
        }
    };

    inline NativeExecutive::CReadWriteLock* GetReadWriteLock( const CFileSystem *fsys )
    {
        return (NativeExecutive::CReadWriteLock*)fileSystemFactory_t::RESOLVE_STRUCT <NativeExecutive::CReadWriteLock> ( (const CFileSystemNative*)fsys, this->pluginOffset );
    }
#endif //FILESYS_MULTI_THREADING
    
    inline void RegisterPlugin( const fs_construction_params& params )
    {
#ifdef FILESYS_MULTI_THREADING
        // We only want to register the lock plugin if the FileSystem has threading support.
        if ( NativeExecutive::CExecutiveManager *nativeMan = (NativeExecutive::CExecutiveManager*)params.nativeExecMan )
        {
            size_t lock_size = nativeMan->GetReadWriteLockStructSize();

            this->pluginOffset =
                _fileSysFactory.RegisterDependantStructPlugin <lock_item> ( fileSystemFactory_t::ANONYMOUS_PLUGIN_ID, lock_size );
        }
#endif //FILESYS_MULTI_THREADING
    }

    inline void UnregisterPlugin( void ) noexcept
    {
#ifdef FILESYS_MULTI_THREADING
        if ( fileSystemFactory_t::IsOffsetValid( this->pluginOffset ) )
        {
            _fileSysFactory.UnregisterPlugin( this->pluginOffset );

            this->pluginOffset = fileSystemFactory_t::INVALID_PLUGIN_OFFSET;
        }
#endif //FILESYS_MULTI_THREADING
    }

private:
    bool isConstructorRegistered;
    fileSystemFactory_t::pluginOffset_t pluginOffset;
};

// A lock everyone has to know about.
extern constinit optional_struct_space <fsLockProvider> _fileSysLockProvider;

#endif //_FILESYSTEM_MAIN_THREADING_SUPPORT_