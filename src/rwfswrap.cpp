/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwfswrap.cpp
*  PURPOSE:     Magic-RW filesystem wrapper with the editor.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

// Global plugins.
extern void registerQtFileSystemTranslators( void );
extern void registerQtFileSystem( void );

extern void unregisterQtFileSystemTranslators( void );
extern void unregisterQtFileSystem( void );

extern CFile* RawOpenGlobalFileEx( const filePath& path, const filesysOpenMode& mode );

CFile* RawOpenGlobalFile( const filePath& path, const filePath& mode )
{
    filesysOpenMode openMode;

    if ( !FileSystem::ParseOpenMode( mode, openMode ) )
    {
        return nullptr;
    }

    return RawOpenGlobalFileEx( path, openMode );
}

CFile* OpenGlobalFile( MainWindow *mainWnd, const filePath& path, const filePath& mode )
{
    CFile *theFile = RawOpenGlobalFile( path, mode );

    if ( theFile )
    {
        try
        {
            theFile = CreateDecompressedStream( mainWnd, theFile );
        }
        catch( ... )
        {
            delete theFile;

            throw;
        }
    }

    return theFile;
}

struct rwFileSystemStreamWrapEnv
{
    struct eirFileSystemMetaInfo
    {
        inline eirFileSystemMetaInfo( void )
        {
            this->theStream = nullptr;
        }

        inline ~eirFileSystemMetaInfo( void )
        {
            return;
        }

        CFile *theStream;
    };

    struct eirFileSystemWrapperProvider : public rw::customStreamInterface, public rw::FileInterface
    {
        // *** rw::customStreamInterface IMPL
        void OnConstruct( rw::eStreamMode streamMode, void *userdata, void *membuf, size_t memSize ) const override
        {
            eirFileSystemMetaInfo *meta = new (membuf) eirFileSystemMetaInfo;

            meta->theStream = (CFile*)userdata;
        }

        void OnDestruct( void *memBuf, size_t memSize ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            meta->~eirFileSystemMetaInfo();
        }

        size_t Read( void *memBuf, void *out_buf, size_t readCount ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            return meta->theStream->Read( out_buf, readCount );
        }

        size_t Write( void *memBuf, const void *in_buf, size_t writeCount ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            return meta->theStream->Write( in_buf, writeCount );
        }

        void Skip( void *memBuf, rw::int64 skipCount ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            meta->theStream->SeekNative( skipCount, SEEK_CUR );
        }

        rw::int64 Tell( const void *memBuf ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            return meta->theStream->TellNative();
        }

        void Seek( void *memBuf, rw::int64 stream_offset, rw::eSeekMode seek_mode ) const override
        {
            int ansi_seek = SEEK_SET;

            if ( seek_mode == rw::RWSEEK_BEG )
            {
                ansi_seek = SEEK_SET;
            }
            else if ( seek_mode == rw::RWSEEK_CUR )
            {
                ansi_seek = SEEK_CUR;
            }
            else if ( seek_mode == rw::RWSEEK_END )
            {
                ansi_seek = SEEK_END;
            }
            else
            {
                assert( 0 );
            }

            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            meta->theStream->SeekNative( stream_offset, ansi_seek );
        }

        rw::int64 Size( const void *memBuf ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            return meta->theStream->GetSizeNative();
        }

        bool SupportsSize( const void *memBuf ) const override
        {
            return true;
        }

        // *** rw::FileInterface IMPL.
        filePtr_t OpenStream( const char *streamPath, const char *mode ) override
        {
            return (filePtr_t)RawOpenGlobalFile( streamPath, mode );
        }

        void CloseStream( filePtr_t ptr ) override
        {
            CFile *outFile = (CFile*)ptr;

            delete outFile;
        }

        filePtr_t OpenStreamW( const wchar_t *streamPath, const wchar_t *mode ) override
        {
            return (filePtr_t)RawOpenGlobalFile( streamPath, mode );
        }

        size_t ReadStream( filePtr_t ptr, void *outBuf, size_t readCount ) override
        {
            CFile *theFile = (CFile*)ptr;

            return theFile->Read( outBuf, readCount );
        }

        size_t WriteStream( filePtr_t ptr, const void *outBuf, size_t writeCount ) override
        {
            CFile *theFile = (CFile*)ptr;

            return theFile->Write( outBuf, writeCount );
        }

        bool SeekStream( filePtr_t ptr, long streamOffset, int type ) override
        {
            CFile *theFile = (CFile*)ptr;

            int iSeekMode;

            if ( type == rw::RWSEEK_BEG )
            {
                iSeekMode = SEEK_SET;
            }
            else if ( type == rw::RWSEEK_CUR )
            {
                iSeekMode = SEEK_CUR;
            }
            else if ( type == rw::RWSEEK_END )
            {
                iSeekMode = SEEK_END;
            }
            else
            {
                return false;
            }

            int seekSuccess = theFile->Seek( streamOffset, iSeekMode );

            return ( seekSuccess == 0 );
        }

        long TellStream( filePtr_t ptr ) override
        {
            CFile *theFile = (CFile*)ptr;

            return theFile->Tell();
        }

        bool IsEOFStream( filePtr_t ptr ) override
        {
            CFile *theFile = (CFile*)ptr;

            return theFile->IsEOF();
        }

        long SizeStream( filePtr_t ptr ) override
        {
            CFile *theFile = (CFile*)ptr;

            return theFile->GetSize();
        }

        void FlushStream( filePtr_t ptr ) override
        {
            CFile *theFile = (CFile*)ptr;

            return theFile->Flush();
        }

        CFileSystem *nativeFileSystem;
    };

    eirFileSystemWrapperProvider eirfs_file_wrap;

    inline void Initialize( rw::Interface *rwEngine )
    {
        NativeExecutive::CExecutiveManager *nativeExec = (NativeExecutive::CExecutiveManager*)rw::GetThreadingNativeManager( rwEngine );

        // Initialize the FileSystem module over here.
        fs_construction_params fsParams;
        fsParams.nativeExecMan = nativeExec;
        fsParams.fileRootPath = "//";
        fsParams.defaultPathProcessMode = filesysPathProcessMode::AMBIVALENT_FILE;

        CFileSystem *fileSys = CFileSystem::Create( fsParams );

        assert( fileSys != nullptr );

        // Set our application root translator to outbreak mode.
        // This is important because we need full access to the machine.
        fileRoot->SetOutbreakEnabled( true );

        // Initialize global plugins.
        registerQtFileSystemTranslators();

        // Create a translator whose root is placed in the application's directory.
        CFileTranslator *sysAppRoot = fileSystem->CreateTranslator( "//" );

        if ( sysAppRoot == nullptr )
        {
            throw std::exception();
        }

        ::sysAppRoot = sysAppRoot;

        if ( fileRoot == nullptr )
        {
            throw std::exception();
        }

        // Put our application path as first source of fresh files.
        register_file_translator( fileRoot );

        // Register embedded resources if present.
        // Those are queried if files in our folder are not present.
        initialize_embedded_resources();

        registerQtFileSystem();

        // Register the native file system wrapper type.
        eirfs_file_wrap.nativeFileSystem = fileSys;

        rwEngine->RegisterStream( "eirfs_file", sizeof( eirFileSystemMetaInfo ), alignof( eirFileSystemMetaInfo ), &eirfs_file_wrap );

        // For backwards compatibility with Windows XP we want to skip the CRT for all kinds of file operations
        // because the Visual Studio Windows XP compatibility layer is broken.
        rwEngine->SetFileInterface( &eirfs_file_wrap );
    }

    inline void Shutdown( rw::Interface *rwEngine )
    {
        rwEngine->SetFileInterface( nullptr );

        unregisterQtFileSystem();

        shutdown_embedded_resources();

        delete sysAppRoot;

        unregisterQtFileSystemTranslators();

        // Shutdown the FileSystem module.
        CFileSystem::Destroy( eirfs_file_wrap.nativeFileSystem );

        // Streams are unregistered automatically when the engine is destroyed.
        // TODO: could be dangerous. unregistering is way cleaner.
    }
};

rw::Stream* RwStreamCreateTranslated( rw::Interface *rwEngine, CFile *eirStream )
{
    rw::streamConstructionCustomParam_t customParam( "eirfs_file", eirStream );

    rw::Stream *result = rwEngine->CreateStream( rw::RWSTREAMTYPE_CUSTOM, rw::RWSTREAMMODE_READWRITE, &customParam );

    return result;
}

static optional_struct_space <rw::interfacePluginStructRegister <rwFileSystemStreamWrapEnv, true>> rwFileSystemStreamWrapEnvRegister;

void InitializeRWFileSystemWrap( void )
{
    rwFileSystemStreamWrapEnvRegister.Construct();
}

void ShutdownRWFileSystemWrap( void )
{
    rwFileSystemStreamWrapEnvRegister.Destroy();
}