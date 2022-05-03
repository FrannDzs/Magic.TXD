/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwfile.system.cpp
*  PURPOSE:     File system management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwfile.system.hxx"

#include "pluginutil.hxx"

// Decide on what directory separator we want to use.
#ifdef _WIN32
#define PATH_SEP    L"\\"
#elif defined(__linux__)
#define PATH_SEP    L"/"
#else
#error no path separator defined for platform
#endif

namespace rw
{

struct dataRepositoryEnv
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        // We just keep a pointer to the translator.
        this->fileTranslator = nullptr;
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // nothing to do here.
    }

    // The FileTranslator class is used to access the data directories on the host's device.
    // We expect it to support input as standard path trees like "prim/sec/file.dat" and then
    // it can do its own transformations into OS path format.
    FileTranslator *fileTranslator;
};

static optional_struct_space <PluginDependantStructRegister <dataRepositoryEnv, RwInterfaceFactory_t>> dataRepositoryEnvRegister;

namespace fs
{

Stream* OpenDataStream( EngineInterface *engineInterface, const wchar_t *filePath, eStreamMode mode )
{
    dataRepositoryEnv *repoEnv = dataRepositoryEnvRegister.get().GetPluginStruct( engineInterface );

    // If we have a data environment and a file translator, we pass all data requests through it.
    // Otherwise we just give the requests to the main file interface.
    if ( repoEnv )
    {
        FileTranslator *fileTrans = repoEnv->fileTranslator;

        if ( fileTrans )
        {
            rw::Stream *transLocalStream = nullptr;

            rwStaticString <wchar_t> sysDataFilePath;

            bool couldGetValidDir = fileTrans->GetBasedDirectory( filePath, sysDataFilePath );

            if ( couldGetValidDir )
            {
                // Let's access it :)
                rw::streamConstructionFileParamW_t wFileParam( sysDataFilePath.GetConstString() );

                transLocalStream = engineInterface->CreateStream( rw::RWSTREAMTYPE_FILE_W, mode, &wFileParam );
            }

            return transLocalStream;
        }
    }

    // This is the fallback, which passes the file request _directly_ to the system.
    // We do prepend a directory tho.
    rwStaticString <wchar_t> prepend_main_dir =
#ifdef RWLIB_DEFAULT_REPOSITORY_DIR
        RWLIB_DEFAULT_REPOSITORY_DIR
#else
        L"magic-rw"
#endif
        PATH_SEP;
    prepend_main_dir += filePath;

    rw::streamConstructionFileParamW_t wFileParam( prepend_main_dir.GetConstString() );

    return engineInterface->CreateStream( rw::RWSTREAMTYPE_FILE_W, mode, &wFileParam );
}

FileTranslator::fileList_t GetDirectoryListing( EngineInterface *rwEngine, const wchar_t *dir, const wchar_t *file_pattern, bool recursive )
{
    dataRepositoryEnv *repoEnv = dataRepositoryEnvRegister.get().GetPluginStruct( rwEngine );

    if ( repoEnv )
    {
        FileTranslator *fileTrans = repoEnv->fileTranslator;

        if ( fileTrans )
        {
            return fileTrans->GetDirectoryListing( dir, file_pattern, recursive );
        }
    }

    // If anything failed we can just pass nothing.
    return FileTranslator::fileList_t();
}

} // namespace fs

void SetDataDirectoryTranslator( Interface *intf, FileTranslator *trans )
{
    // Sets the currently active data repository access parser (translator).

    EngineInterface *engineInterface = (EngineInterface*)intf;

    dataRepositoryEnv *repoEnv = dataRepositoryEnvRegister.get().GetPluginStruct( engineInterface );

    if ( repoEnv )
    {
        repoEnv->fileTranslator = trans;
    }
}

void registerFileSystemDataRepository( void )
{
    dataRepositoryEnvRegister.Construct( engineFactory );
}

void unregisterFileSystemDataRepository( void )
{
    dataRepositoryEnvRegister.Destroy();
}

};