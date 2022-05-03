/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.internal.repo.h
*  PURPOSE:     Internal repository creation utilities
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_REPOSITORY_UTILITIES_
#define _FILESYSTEM_REPOSITORY_UTILITIES_

#include <sstream>
#include <atomic>

#include <sdk/NumericFormat.h>

class CRepository
{
public:
    inline CRepository( void )
    {
        this->infiniteSequence = 0;
        this->trans = nullptr;

#ifdef FILESYS_MULTI_THREADING
        this->lockRepo = MakeReadWriteLock( fileSystem );
#endif //FILESYS_MULTI_THREADING
    }

    inline ~CRepository( void )
    {
#ifdef FILESYS_MULTI_THREADING
        DeleteReadWriteLock( fileSystem, lockRepo );
#endif //FILESYS_MULTI_THREADING

        if ( this->trans )
        {
            fileSystem->DeleteTempRepository( this->trans );

            this->trans = nullptr;
        }
    }

    inline CFileTranslator* GetTranslator( void )
    {
        if ( !this->trans )
        {
#ifdef FILESYS_MULTI_THREADING
            NativeExecutive::CReadWriteWriteContextSafe <> consistency( this->lockRepo );
#endif //FILESYS_MULTI_THREADING

            if ( !this->trans )
            {
                this->trans = fileSystem->GenerateTempRepository();
            }
        }

        return this->trans;
    }

    inline CFileTranslator* GenerateUniqueDirectory( void )
    {
        // Attempt to get the handle to our temporary directory.
        CFileTranslator *sysTmpRoot = GetTranslator();

        if ( sysTmpRoot )
        {
            // Create temporary storage.
            filePath path;
            {
                eir::String <char, FSObjectHeapAllocator> dirName( "/" );
                dirName += eir::to_string <char, FSObjectHeapAllocator> ( this->infiniteSequence++ );
                dirName += "/";

                sysTmpRoot->CreateDir( dirName.GetConstString() );
                sysTmpRoot->GetFullPathFromRoot( dirName.GetConstString(), false, path );
            }

            return fileSystem->CreateTranslator( path );
        }
        return nullptr;
    }

    static AINLINE CFileTranslator* AcquireDirectoryTranslator( CFileTranslator *fileRoot, const char *dirName )
    {
        CFileTranslator *resultTranslator = nullptr;

        bool canBeAcquired = true;

        if ( !fileRoot->Exists( dirName ) )
        {
            canBeAcquired = fileRoot->CreateDir( dirName );
        }

        if ( canBeAcquired )
        {
            filePath rootPath;

            bool gotRoot = fileRoot->GetFullPath( "", false, rootPath );

            if ( gotRoot )
            {
                rootPath += dirName;

                resultTranslator = fileSystem->CreateTranslator( rootPath );
            }
        }

        return resultTranslator;
    }

private:     
    // Sequence used to create unique directories with.
    std::atomic <unsigned int> infiniteSequence;

    // The translator of this repository.
    CFileTranslator *volatile trans;

#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::CReadWriteLock *lockRepo;
#endif //FILESYS_MULTI_THREADING
};

#endif //_FILESYSTEM_REPOSITORY_UTILITIES_