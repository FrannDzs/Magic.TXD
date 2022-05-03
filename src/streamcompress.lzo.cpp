/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/streamcompress.lzo.cpp
*  PURPOSE:     LZO compression plugin for the editor.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include <sdk/PluginHelpers.h>

struct lzoStreamCompressionManager final : public compressionManager
{
    inline void Initialize( MainWindow *mainWnd )
    {
        this->mainWnd = mainWnd;

        RegisterStreamCompressionManager( mainWnd, this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        UnregisterStreamCompressionManager( mainWnd, this );
    }

    bool IsStreamCompressed( CFile *stream ) const override
    {
        try
        {
            return mainWnd->fileSystem->IsStreamLZOCompressed( stream );
        }
        catch( FileSystem::filesystem_exception& )
        {
            return false;
        }
    }

    struct fsysProviderWrap final : public compressionProvider
    {
        inline fsysProviderWrap( CIMGArchiveCompressionHandler *handler )
        {
            this->prov = handler;
        }

        bool Compress( CFile *input, CFile *output ) override
        {
            return prov->Compress( input, output );
        }

        bool Decompress( CFile *input, CFile *output ) override
        {
            return prov->Decompress( input, output );
        }

        CIMGArchiveCompressionHandler *prov;
    };

    compressionProvider* CreateProvider( void ) override
    {
        try
        {
            CIMGArchiveCompressionHandler *lzo = mainWnd->fileSystem->CreateLZOCompressor();

            if ( lzo )
            {
                return new fsysProviderWrap( lzo );
            }

            return nullptr;
        }
        catch( FileSystem::filesystem_exception& )
        {
            return nullptr;
        }
    }

    void DestroyProvider( compressionProvider *prov ) override
    {
        fsysProviderWrap *wrap = (fsysProviderWrap*)prov;

        CIMGArchiveCompressionHandler *fsysHandler = wrap->prov;

        delete wrap;

        mainWnd->fileSystem->DestroyLZOCompressor( fsysHandler );
    }

    MainWindow *mainWnd;
};

static optional_struct_space <PluginDependantStructRegister <lzoStreamCompressionManager, mainWindowFactory_t>> lzoStreamCompressionRegister;

void InitializeLZOStreamCompression( void )
{
    lzoStreamCompressionRegister.Construct( mainWindowFactory );
}

void ShutdownLZOStreamCompression( void )
{
    lzoStreamCompressionRegister.Destroy();
}