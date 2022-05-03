/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/tools/dirtools.h
*  PURPOSE:     Directory helpers for the tools.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "shared.h"

template <typename sentryType>
struct gtaFileProcessor
{
    MessageReceiver *module;

    inline gtaFileProcessor( MessageReceiver *module )
    {
        this->reconstruct_archives = true;
        this->use_compressed_img_archives = true;
        this->module = module;
    }

    inline ~gtaFileProcessor( void )
    {
        return;
    }

    inline void process( sentryType *theSentry, CFileTranslator *discHandle, CFileTranslator *buildRoot )
    {
        _discFileTraverse traverse;
        traverse.module = this->module;
        traverse.discHandle = discHandle;
        traverse.buildRoot = buildRoot;
        traverse.isInArchive = false;
        traverse.sentry = theSentry;
        traverse.reconstruct_archives = this->reconstruct_archives;
        traverse.use_compressed_img_archives = this->use_compressed_img_archives;

        discHandle->ScanDirectory( "//", "*", true, nullptr, _discFileCallback, &traverse );
    }

    inline void setArchiveReconstruction( bool doReconstruct )
    {
        this->reconstruct_archives = doReconstruct;
    }

    inline void setUseCompressedIMGArchives( bool doUse )
    {
        this->use_compressed_img_archives = doUse;
    }

private:
    bool reconstruct_archives;
    bool use_compressed_img_archives;

    struct _discFileTraverse
    {
        inline _discFileTraverse( void )
        {
            this->anyWork = false;
        }

        MessageReceiver *module;

        CFileTranslator *discHandle;
        CFileTranslator *buildRoot;
        bool isInArchive;

        bool anyWork;

        bool reconstruct_archives;
        bool use_compressed_img_archives;

        sentryType *sentry;
    };

    static void _discFileCallback( const filePath& discFilePathAbs, void *userdata )
    {
        _discFileTraverse *info = (_discFileTraverse*)userdata;

        // Do not process files that we potentially could have created.
        // This prevents infinite recursion.
        CFileTranslator *buildRoot = info->buildRoot;

        // We do a massive shortcut here in midst of reality.
        // There can only be build root conflicts if both translators are inside the OS filesystem.
        // This could change in the future; I will review this code by then.
        if ( fileSystem->GetArchiveTranslator( info->discHandle ) == nullptr &&
             fileSystem->GetArchiveTranslator( buildRoot ) == nullptr )
        {
            filePath buildRootRelative;

            bool isBuildRootPath = buildRoot->GetRelativePathFromRoot( discFilePathAbs, true, buildRootRelative );

            if ( isBuildRootPath )
            {
                // We cannot process this, because its a thing of creation.
                return;
            }
        }

        MessageReceiver *module = info->module;

        bool anyWork = false;

        // Preprocess the file.
        // Create a destination file inside of the build root using the relative path from the source trans root.
        filePath relPathFromRoot;

        bool hasTargetRelativePath = info->discHandle->GetRelativePathFromRoot( discFilePathAbs, true, relPathFromRoot );

        if ( hasTargetRelativePath )
        {
            bool hasPreprocessedFile = false;

            filePath extention;

            filePath fileName = FileSystem::GetFileNameItem <FileSysCommonAllocator> ( discFilePathAbs, false, nullptr, &extention );

            if ( extention.size() != 0 )
            {
                if ( extention.equals( "IMG", false ) )
                {
                    module->OnMessage( templ_repl( module->TOKEN( "DirTools.Processing" ), L"what", relPathFromRoot.convert_unicode <FileSysCommonAllocator> () ) + L"\n" );

                    // Open the IMG archive.
                    CIMGArchiveTranslatorHandle *srcIMGRoot = nullptr;

                    if ( info->use_compressed_img_archives )
                    {
                        srcIMGRoot = fileSystem->OpenCompressedIMGArchive( info->discHandle, relPathFromRoot, false );
                    }
                    else
                    {
                        srcIMGRoot = fileSystem->OpenIMGArchive( info->discHandle, relPathFromRoot, false );
                    }

                    if ( srcIMGRoot )
                    {
                        try
                        {
                            // If we have found an IMG archive, we perform the same stuff for files inside of it.
                            CFileTranslator *outputRoot = nullptr;
                            CArchiveTranslator *outputRoot_archive = nullptr;

                            if ( info->reconstruct_archives )
                            {
                                // Grab the version of the source IMG file.
                                // We want to output rebuilt archives in the same version.
                                eIMGArchiveVersion imgVersion = srcIMGRoot->GetVersion();

                                // We copy the files into a new IMG archive tho.
                                CArchiveTranslator *newIMGRoot = nullptr;

                                if ( info->use_compressed_img_archives )
                                {
                                    newIMGRoot = fileSystem->CreateCompressedIMGArchive( info->buildRoot, relPathFromRoot, imgVersion );
                                }
                                else
                                {
                                    newIMGRoot = fileSystem->CreateIMGArchive( info->buildRoot, relPathFromRoot, imgVersion );
                                }

                                if ( newIMGRoot )
                                {
                                    outputRoot = newIMGRoot;
                                    outputRoot_archive = newIMGRoot;
                                }
                            }
                            else
                            {
                                // Create a new directory in place of the archive.
                                filePath srcPathToNewDir;

                                info->discHandle->GetRelativePathFromRoot( discFilePathAbs, false, srcPathToNewDir );

                                filePath pathToNewDir;

                                info->buildRoot->GetFullPathFromRoot( srcPathToNewDir, false, pathToNewDir );

                                pathToNewDir += fileName;
                                pathToNewDir += "_archive/";

                                bool createDirSuccess = info->buildRoot->CreateDir( pathToNewDir );

                                if ( createDirSuccess )
                                {
                                    CFileTranslator *newDirRoot = fileSystem->CreateTranslator( pathToNewDir );

                                    if ( newDirRoot )
                                    {
                                        outputRoot = newDirRoot;
                                    }
                                }
                            }

                            if ( outputRoot )
                            {
                                try
                                {
                                    // Run into shit.
                                    _discFileTraverse traverse;
                                    traverse.module = info->module;
                                    traverse.discHandle = srcIMGRoot;
                                    traverse.buildRoot = outputRoot;
                                    traverse.isInArchive = ( outputRoot_archive != nullptr ) || info->isInArchive;
                                    traverse.sentry = info->sentry;
                                    traverse.reconstruct_archives = info->reconstruct_archives;
                                    traverse.use_compressed_img_archives = info->use_compressed_img_archives;

                                    srcIMGRoot->ScanFilesInSerializationOrder( _discFileCallback, &traverse );

                                    if ( outputRoot_archive != nullptr )
                                    {
                                        if ( !traverse.anyWork )
                                        {
                                            module->OnMessage( module->TOKEN( "DirTools.WritingNoChange" ) );
                                        }
                                        else
                                        {
                                            module->OnMessage( module->TOKEN( "DirTools.Writing" ) );
                                        }

                                        outputRoot_archive->Save();

                                        module->OnMessage( module->TOKEN( "DirTools.WritingFinished" ) + L"\n\n" );

                                        anyWork = true;
                                    }
                                    else
                                    {
                                        module->OnMessage( module->TOKEN( "DirTools.ArchiveFinished" ) + L"\n\n" );

                                        anyWork = true;
                                    }

                                    hasPreprocessedFile = true;
                                }
                                catch( ... )
                                {
                                    delete outputRoot;

                                    throw;
                                }

                                delete outputRoot;
                            }
                            else
                            {
                                info->sentry->OnArchiveFail( fileName, extention );
                            }
                        }
                        catch( ... )
                        {
                            // On exception, we must clean up after ourselves :)
                            delete srcIMGRoot;

                            throw;
                        }

                        // Clean up.
                        delete srcIMGRoot;
                    }
                    else
                    {
                        module->OnMessage( module->TOKEN( "DirTools.NotAnIMG" ) + L"\n" );
                    }
                }
            }

            if ( !hasPreprocessedFile )
            {
                // Do special logic for certain files.
                // Copy all files into the build root.
                CFile *sourceStream = nullptr;
                {
                    sourceStream = info->discHandle->Open( discFilePathAbs, L"rb" );
                }

                if ( sourceStream )
                {
                    try
                    {
                        sourceStream = module->WrapStreamCodec( sourceStream );
                    }
                    catch( ... )
                    {
                        delete sourceStream;

                        throw;
                    }
                }

                if ( sourceStream )
                {
                    try
                    {
                        // Execute the sentry.
                        bool hasDoneAnyWork = info->sentry->OnSingletonFile( info->discHandle, buildRoot, relPathFromRoot, fileName, extention, sourceStream, info->isInArchive );

                        if ( hasDoneAnyWork )
                        {
                            anyWork = true;
                        }
                    }
                    catch( ... )
                    {
                        delete sourceStream;

                        throw;
                    }

                    delete sourceStream;
                }
            }
        }

        if ( anyWork )
        {
            info->anyWork = true;
        }
    }
};

inline bool obtainAbsolutePath( const wchar_t *path, CFileTranslator*& transOut, bool createDir, bool hasToBeDirectory = true )
{
    bool hasTranslator = false;

    bool newTranslator = false;
    CFileTranslator *translator = nullptr;
    filePath thePath;

    // Check whether the file path has a trailing slash.
    // If it has not, then append one.
    filePath inputPath( path );

    if ( hasToBeDirectory )
    {
        if ( !FileSystem::IsPathDirectory( inputPath ) )
        {
            inputPath += '/';
        }
    }

    if ( fileRoot->GetFullPath( inputPath, true, thePath ) )
    {
        translator = fileRoot;

        hasTranslator = true;
    }

    if ( !hasTranslator )
    {
        CFileTranslator *diskTranslator = fileSystem->CreateSystemMinimumAccessPoint( inputPath );

        if ( diskTranslator )
        {
            bool canResolve = diskTranslator->GetFullPath( inputPath, true, thePath );

            if ( canResolve )
            {
                newTranslator = true;
                translator = diskTranslator;

                hasTranslator = true;
            }

            if ( !hasTranslator )
            {
                delete diskTranslator;
            }
        }
    }

    bool success = false;

    if ( hasTranslator )
    {
        bool canCreateTranslator = false;

        if ( createDir )
        {
            bool createPath = translator->CreateDir( thePath );

            if ( createPath )
            {
                canCreateTranslator = true;
            }
        }
        else
        {
            bool existsPath = translator->Exists( thePath );

            if ( existsPath )
            {
                canCreateTranslator = true;
            }
        }

        if ( canCreateTranslator )
        {
            CFileTranslator *actualRoot = fileSystem->CreateTranslator( thePath );

            if ( actualRoot )
            {
                success = true;

                transOut = actualRoot;
            }
        }

        if ( newTranslator )
        {
            delete translator;
        }
    }

    return success;
}

inline bool isBuildRootConflict( CFileTranslator *inputRoot, CFileTranslator *outputRoot )
{
    filePath buildRootAbsolutePath;

    outputRoot->GetFullPathFromRoot( L"", false, buildRootAbsolutePath );

    // A build root conflict exists if the output root is inside the input root.
    // This means we cannot process all the files from the input root, because they could have been created by the output algorithm.
    filePath buildRootRelativeOfInput;

    bool isOutputInsideInput = inputRoot->GetRelativePathFromRoot( buildRootAbsolutePath, false, buildRootRelativeOfInput );

    return ( isOutputInsideInput == true );
}
