/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/include/CFileSystem.qol.h
*  PURPOSE:     Helpers for runtime constructs typical to C++
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_QUALITY_OF_LIFE_
#define _FILESYSTEM_QUALITY_OF_LIFE_

#include <exception>

namespace FileSystem
{

// Smart pointer for a CFileSystem instance.
struct fileSysInstance
{
    inline fileSysInstance( void )
    {
        using namespace FileSystem;

        // Creates a CFileSystem without any special things.
        fs_construction_params params;
        params.nativeExecMan = nullptr;

        CFileSystem *fileSys = CFileSystem::Create( params );

        if ( !fileSys )
        {
            throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
        }

        this->fileSys = fileSys;
    }

    inline fileSysInstance( const fs_construction_params& params )
    {
        using namespace FileSystem;

        CFileSystem *fileSys = CFileSystem::Create( params );

        if ( !fileSys )
        {
            throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
        }

        this->fileSys = fileSys;
    }

    inline ~fileSysInstance( void )
    {
        CFileSystem::Destroy( this->fileSys );
    }

    // Direct availability.
    inline CFileSystem& inst( void )
    {
        return *fileSystem;
    }

    inline operator CFileSystem* ( void )
    {
        return &inst();
    }

    inline CFileSystem* operator -> ( void )
    {
        return &inst();
    }

private:
    CFileSystem *fileSys;
};

// File translator.
struct fileTrans
{
    inline fileTrans( CFileTranslator *fileTrans )
    {
        this->theTrans = fileTrans;
    }

    inline fileTrans( void ) : fileTrans( nullptr )
    {
        return;
    }

    template <typename charType>
    inline fileTrans( CFileSystem *fileSys, const charType *path, eDirOpenFlags dirFlags = DIR_FLAG_NONE )
        : fileTrans( fileSys->CreateTranslator( path, dirFlags ) )
    {
        return;
    }

    inline fileTrans( CFileSystem *fileSys, const filePath& path, eDirOpenFlags dirFlags = DIR_FLAG_NONE )
        : fileTrans( fileSys->CreateTranslator( path, dirFlags ) )
    {
        return;
    }

    inline fileTrans( fileTrans&& right ) noexcept
    {
        this->theTrans = right.theTrans;

        right.theTrans = nullptr;
    }

    inline ~fileTrans( void )
    {
        if ( CFileTranslator *theTrans = this->theTrans )
        {
            delete theTrans;
        }
    }

    inline fileTrans& operator = ( const fileTrans& ) = delete;
    inline fileTrans& operator = ( fileTrans&& right ) noexcept
    {
        if ( CFileTranslator *old = this->theTrans )
        {
            delete old;
        }

        this->theTrans = right.theTrans;

        right.theTrans = nullptr;

        return *this;
    }

    inline bool is_good( void ) const
    {
        return ( this->theTrans != nullptr );
    }

    // Can return nullptr.
    inline CFileTranslator* get_opt( void ) const
    {
        return this->theTrans;  
    }

    inline CFileTranslator& inst( void )
    {
        CFileTranslator *theTrans = this->theTrans;

        if ( !theTrans )
        {
            throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
        }

        return *theTrans;
    }

    inline operator CFileTranslator* ( void )
    {
        return &inst();
    }

    inline CFileTranslator* operator -> ( void )
    {
        return &inst();
    }
    
private:
    CFileTranslator *theTrans;
};

// Archived file translator.
struct archiveTrans
{
    inline archiveTrans( CArchiveTranslator *fileTrans )
    {
        this->theTrans = fileTrans;
    }

    inline archiveTrans( archiveTrans&& right ) noexcept
    {
        this->theTrans = right.theTrans;

        right.theTrans = nullptr;
    }

    inline ~archiveTrans( void )
    {
        if ( CArchiveTranslator *theTrans = this->theTrans )
        {
            delete theTrans;
        }
    }

    inline archiveTrans& operator = ( const archiveTrans& ) = delete;
    inline archiveTrans& operator = ( archiveTrans&& right ) noexcept
    {
        if ( CArchiveTranslator *old = this->theTrans )
        {
            delete old;
        }

        this->theTrans = right.theTrans;

        right.theTrans = nullptr;

        return *this;
    }

    inline bool is_good( void ) const
    {
        return ( this->theTrans != nullptr );
    }

    // Can return nullptr.
    inline CArchiveTranslator* get_opt( void ) const
    {
        return this->theTrans;   
    }

    inline CArchiveTranslator& inst( void )
    {
        CArchiveTranslator *theTrans = this->theTrans;

        if ( !theTrans )
        {
            throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
        }

        return *theTrans;
    }

    inline operator CArchiveTranslator* ( void )
    {
        return &inst();
    }

    inline CArchiveTranslator* operator -> ( void )
    {
        return &inst();
    }
    
private:
    CArchiveTranslator *theTrans;
};

// File pointer.
struct filePtr
{
    // Since files could be unavailable very frequently we make it a habbit of the user to check for availability explicitly (theFile could be nullptr).

    inline filePtr( CFile *theFile, bool destroy_on_leave = true ) noexcept : destroy_on_leave( destroy_on_leave )
    {
        this->theFile = theFile;
    }

    inline filePtr( void ) noexcept : filePtr( nullptr )
    {
        return;
    }

    template <typename charType>
    inline filePtr( CFileTranslator *fileTrans, const charType *path, const filesysOpenMode& mode, eFileOpenFlags fileFlags = FILE_FLAG_NONE )
        : filePtr( fileTrans->Open( path, mode, fileFlags ) )
    {
        return;
    }

    inline filePtr( CFileTranslator *fileTrans, const filePath& path, const filesysOpenMode& mode, eFileOpenFlags fileFlags = FILE_FLAG_NONE )
        : filePtr( fileTrans->Open( path, mode, fileFlags ) )
    {
        return;
    }

    template <typename charType>
    inline filePtr( CFileTranslator *fileTrans, const charType *path, const charType *mode, eFileOpenFlags fileFlags = FILE_FLAG_NONE )
        : filePtr( fileTrans->Open( path, mode, fileFlags ) )
    {
        return;
    }

    inline filePtr( CFileTranslator *fileTrans, const filePath& path, const filePath& mode, eFileOpenFlags fileFlags = FILE_FLAG_NONE )
        : filePtr( fileTrans->Open( path, mode, fileFlags ) )
    {
        return;
    }

    inline filePtr( filePtr&& right ) noexcept
    {
        this->theFile = right.theFile;
        this->destroy_on_leave = right.destroy_on_leave;

        right.theFile = nullptr;
    }

private:
    AINLINE void _clean_ref( void )
    {
        if ( CFile *theFile = this->theFile )
        {
            if ( this->destroy_on_leave )
            {
                delete theFile;
            }
        }
    }

public:
    inline ~filePtr( void )
    {
        _clean_ref();
    }

    inline filePtr& operator = ( filePtr&& right ) noexcept
    {
        _clean_ref();

        this->theFile = right.theFile;
        this->destroy_on_leave = right.destroy_on_leave;

        right.theFile = nullptr;

        return *this;
    }

    inline bool is_good( void ) const
    {
        return ( this->theFile != nullptr );
    }

    // Can return nullptr.
    inline CFile* get_opt( void ) const
    {
        return this->theFile;
    }

    inline CFile& inst( void )
    {
        CFile *theFile = this->theFile;

        if ( theFile == nullptr )
        {
            throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
        }

        return *theFile;
    }

    inline operator CFile* ( void )
    {
        return &inst();
    }

    inline CFile* operator -> ( void )
    {
        return &inst();
    }

private:
    CFile *theFile;
    bool destroy_on_leave;
};

// Helper for the directory iterator that you can get at each translator.
struct dirIterator
{
    inline dirIterator( CDirectoryIterator *iter )
    {
        this->iterator = iter;
    }
    inline dirIterator( const dirIterator& ) = delete;
    inline dirIterator( dirIterator&& right ) noexcept
    {
        this->iterator = right.iterator;

        right.iterator = nullptr;
    }
    
    inline void clear( void )
    {
        if ( CDirectoryIterator *iter = this->iterator )
        {
            delete iter;

            this->iterator = nullptr;
        }
    }

    inline ~dirIterator( void )
    {
        this->clear();
    }

    inline bool is_good( void ) const
    {
        return ( this->iterator != nullptr );
    }

    inline dirIterator& operator = ( const dirIterator& ) = delete;
    inline dirIterator& operator = ( dirIterator&& right ) noexcept
    {
        this->clear();

        this->iterator = right.iterator;

        right.iterator = nullptr;

        return *this;
    }

    // Can return nullptr.
    inline CDirectoryIterator* get_opt( void ) const
    {
        return this->iterator;
    }

    inline CDirectoryIterator& inst( void )
    {
        CDirectoryIterator *iter = this->iterator;

        if ( iter == nullptr )
        {
            throw filesystem_exception( eGenExceptCode::RESOURCE_UNAVAILABLE );
        }

        return *iter;
    }

    inline operator CDirectoryIterator* ( void )
    {
        return &inst();
    }

    inline CDirectoryIterator* operator -> ( void )
    {
        return &inst();
    }

private:
    CDirectoryIterator *iterator;
};

// Helper for stack-based disk watching.
struct diskWatcherStackReg
{
    inline diskWatcherStackReg( void ) noexcept
    {
        this->watcher = nullptr;
        this->fileSys = nullptr;
    }
    inline diskWatcherStackReg( CFileSystem *fileSys, CLogicalDiskUpdateNotificationInterface *watcher )
    {
        fileSys->RegisterDiskNotification( watcher );

        this->watcher = watcher;
        this->fileSys = fileSys;
    }
    inline diskWatcherStackReg( const diskWatcherStackReg& ) = delete;
    inline diskWatcherStackReg( diskWatcherStackReg&& right ) noexcept
    {
        this->watcher = right.watcher;
        this->fileSys = right.fileSys;

        right.watcher = nullptr;
        right.fileSys = nullptr;
    }

    inline void release( void )
    {
        if ( CLogicalDiskUpdateNotificationInterface *watcher = this->watcher )
        {
            this->fileSys->UnregisterDiskNotification( watcher );

            this->watcher = nullptr;
            this->fileSys = nullptr;
        }
    }

    inline ~diskWatcherStackReg( void ) noexcept(false)
    {
        // I decided to pass on this very rare exception at the expense of being
        // able to allocate this instance inside of smart containers.
        try
        {
            this->release();
        }
        catch( ... )
        {
            if ( std::uncaught_exceptions() <= 0 )
            {
                throw;
            }

            // Just have to continue because we are already unwinding.
        }
    }

    inline diskWatcherStackReg& operator = ( const diskWatcherStackReg& ) = delete;
    inline diskWatcherStackReg& operator = ( diskWatcherStackReg&& right )
    {
        this->release();

        this->watcher = right.watcher;
        this->fileSys = right.fileSys;

        return *this;
    }

private:
    CFileSystem *fileSys;
    CLogicalDiskUpdateNotificationInterface *watcher;
};

} // namespace FileSystem

#endif //_FILESYSTEM_QUALITY_OF_LIFE_