/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/streamcompress.cpp
*  PURPOSE:     Stream compression codec management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include <sdk/PluginHelpers.h>

struct streamCompressionEnv
{
    inline void Initialize( MainWindow *mainWnd )
    {
        // Only establish the temporary root on demand.
        this->tmpRoot = nullptr;

        this->lockRootConsistency = rw::CreateReadWriteLock( mainWnd->GetEngine() );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        rw::CloseReadWriteLock( mainWnd->GetEngine(), this->lockRootConsistency );

        // If we have a temporary repository, destroy it.
        if ( this->tmpRoot )
        {
            mainWnd->fileSystem->DeleteTempRepository( this->tmpRoot );
        }
    }

    inline CFileTranslator* GetRepository( MainWindow *mainWnd )
    {
        if ( !this->tmpRoot )
        {
            rw::scoped_rwlock_writer <> consistency( this->lockRootConsistency );

            if ( !this->tmpRoot )
            {
                // We need to establish a temporary root for writing files into that we decompressed.
                this->tmpRoot = mainWnd->fileSystem->GenerateTempRepository();
            }
        }

        return this->tmpRoot;
    }

    rw::rwlock *lockRootConsistency;

    typedef std::vector <compressionManager*> compressors_t;

    compressors_t compressors;

    CFileTranslator *volatile tmpRoot;
};

static optional_struct_space <PluginDependantStructRegister <streamCompressionEnv, mainWindowFactory_t>> streamCompressionEnvRegister;

struct CTemporaryFile : public CFile
{
    AINLINE CTemporaryFile( CFileTranslator *sourceTrans, CFile *wrapped )
    {
        this->sourceTrans = sourceTrans;
        this->actualFile = wrapped;
    }

    AINLINE ~CTemporaryFile( void )
    {
        filePath pathOfFile = actualFile->GetPath();

        delete actualFile;

        sourceTrans->Delete( pathOfFile );
    }

    size_t Read( void *buffer, size_t readCount ) override
    {
        return actualFile->Read( buffer, readCount );
    }

    size_t Write( const void *buffer, size_t writeCount ) override
    {
        return actualFile->Write( buffer, writeCount );
    }

    int Seek( long iOffset, int iType ) override
    {
        return actualFile->Seek( iOffset, iType );
    }

    int SeekNative( fsOffsetNumber_t iOffset, int iType ) override
    {
        return actualFile->SeekNative( iOffset, iType );
    }

    long Tell( void ) const noexcept override
    {
        return actualFile->Tell();
    }

    fsOffsetNumber_t TellNative( void ) const noexcept override
    {
        return actualFile->TellNative();
    }

    bool IsEOF( void ) const noexcept override
    {
        return actualFile->IsEOF();
    }

    bool QueryStats( filesysStats& statsOut ) const noexcept override
    {
        return actualFile->QueryStats( statsOut );
    }

    void SetFileTimes( time_t atime, time_t ctime, time_t mtime ) override
    {
        actualFile->SetFileTimes( atime, ctime, mtime );
    }

    void SetSeekEnd( void ) override
    {
        actualFile->SetSeekEnd();
    }

    size_t GetSize( void ) const noexcept override
    {
        return actualFile->GetSize();
    }

    fsOffsetNumber_t GetSizeNative( void ) const noexcept override
    {
        return actualFile->GetSizeNative();
    }

    void Flush( void ) override
    {
        actualFile->Flush();
    }

    CFileMappingProvider* CreateMapping( void ) override
    {
        return nullptr;
    }

    filePath GetPath( void ) const override
    {
        return actualFile->GetPath();
    }

    bool IsReadable( void ) const noexcept override
    {
        return actualFile->IsReadable();
    }

    bool IsWriteable( void ) const noexcept override
    {
        return actualFile->IsWriteable();
    }

    CFileTranslator *sourceTrans;
    CFile *actualFile;
};

CFile* CreateDecompressedStream( MainWindow *mainWnd, CFile *compressed )
{
    // We want to pipe the stream if we find out that it really is compressed.
    // For those we will create a special stream that will point to the new stream.

    CFile *resultFile = compressed;

    if ( streamCompressionEnv *env = streamCompressionEnvRegister.get().GetPluginStruct( mainWnd ) )
    {
        bool needsStreamReset = false;

        compressionManager *theManager = nullptr;

        for ( compressionManager *manager : env->compressors )
        {
            if ( needsStreamReset )
            {
                compressed->Seek( 0, SEEK_SET );

                needsStreamReset = false;
            }

            bool isCorrectFormat = manager->IsStreamCompressed( compressed );

            needsStreamReset = true;

            if ( isCorrectFormat )
            {
                theManager = manager;
                break;
            }
        }

        if ( needsStreamReset )
        {
            compressed->Seek( 0, SEEK_SET );

            needsStreamReset = false;
        }

        // If we found a compressed format...
        if ( theManager )
        {
            // ... we want to create a random file and decompress into it.
            CFileTranslator *repo = env->GetRepository( mainWnd );

            if ( repo )
            {
                CFile *decFile = mainWnd->fileSystem->GenerateRandomFile( repo );

                if ( decFile )
                {
                    try
                    {
                        // Create a compression provider we will use.
                        compressionProvider *compressor = theManager->CreateProvider();

                        if ( compressor )
                        {
                            try
                            {
                                // Decompress!
                                bool couldDecompress = compressor->Decompress( compressed, decFile );

                                if ( couldDecompress )
                                {
                                    // Simply return the decompressed file.
                                    decFile->Seek( 0, SEEK_SET );

                                    CTemporaryFile *tmpFile = new CTemporaryFile( repo, decFile );

                                    resultFile = tmpFile;

                                    // We can free the other handle.
                                    delete compressed;

                                    compressed = nullptr;
                                }
                                else
                                {
                                    // We kinda failed. Just return the original stream.
                                    compressed->Seek( 0, SEEK_SET );
                                }
                            }
                            catch( ... )
                            {
                                theManager->DestroyProvider( compressor );

                                throw;
                            }

                            theManager->DestroyProvider( compressor );
                        }
                    }
                    catch( ... )
                    {
                        delete decFile;

                        throw;
                    }

                    if ( resultFile == compressed )
                    {
                        delete decFile;
                    }
                }
            }
        }
    }

    return resultFile;
}

bool RegisterStreamCompressionManager( MainWindow *mainWnd, compressionManager *manager )
{
    bool success = false;

    if ( streamCompressionEnv *env = streamCompressionEnvRegister.get().GetPluginStruct( mainWnd ) )
    {
        env->compressors.push_back( manager );

        success = true;
    }

    return success;
}

bool UnregisterStreamCompressionManager( MainWindow *mainWnd, compressionManager *manager )
{
    bool success = false;

    if ( streamCompressionEnv *env = streamCompressionEnvRegister.get().GetPluginStruct( mainWnd ) )
    {
        streamCompressionEnv::compressors_t::iterator iter =
            std::find( env->compressors.begin(), env->compressors.end(), manager );

        if ( iter != env->compressors.end() )
        {
            env->compressors.erase( iter );

            success = true;
        }
    }

    return success;
}

// Sub modules.
extern void InitializeLZOStreamCompression( void );
extern void InitializeMH2ZCompressionEnv( void );

extern void ShutdownLZOStreamCompression( void );
extern void ShutdownMH2ZCompressionEnv( void );

void InitializeStreamCompressionEnvironment( void )
{
    streamCompressionEnvRegister.Construct( mainWindowFactory );

    // Register sub-modules.
    InitializeLZOStreamCompression();
    InitializeMH2ZCompressionEnv();
}

void ShutdownStreamCompressionEnvironment( void )
{
    // Unregister sub-modules.
    ShutdownMH2ZCompressionEnv();
    ShutdownLZOStreamCompression();

    streamCompressionEnvRegister.Destroy();
}