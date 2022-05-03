/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/streamcompress.mh2z.cpp
*  PURPOSE:     MH2Z compression plugin for the editor.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include <sdk/PluginHelpers.h>

#define MAGIC_NUM       'MH2Z'

// Meh. I made this just for fun. Manhunt 2 does not use RW TXD formats anyway.

struct mh2zCompressionEnv : public compressionManager
{
    inline void Initialize( MainWindow *mainWnd )
    {
        RegisterStreamCompressionManager( mainWnd, this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        UnregisterStreamCompressionManager( mainWnd, this );
    }

    struct mh2zHeader
    {
        char magic[4];
        endian::little_endian <std::uint32_t> decomp_size;
    };

    bool IsStreamCompressed( CFile *input ) const override
    {
        mh2zHeader header;

        if ( !input->ReadStruct( header ) )
        {
            return false;
        }

        if ( header.magic[0] != 'Z' || header.magic[1] != '2' || header.magic[2] != 'H' || header.magic[3] != 'M' )
        {
            return false;
        }

        // We trust that it is compressed.
        return true;
    }

    struct mh2zCompressionProvider final : public compressionProvider
    {
        bool Compress( CFile *input, CFile *output ) override
        {
            // Write the MH2Z header.
            mh2zHeader header;
            header.magic[0] = 'Z';
            header.magic[1] = '2';
            header.magic[2] = 'H';
            header.magic[3] = 'M';
            header.decomp_size = (size_t)( input->GetSizeNative() - input->TellNative() );

            output->WriteStruct( header );

            // Now the compression data.
            fileSystem->CompressZLIBStream( input, output, true );
            return true;
        }

        bool Decompress( CFile *input, CFile *output ) override
        {
            mh2zHeader header;

            if ( !input->ReadStruct( header ) )
            {
                return false;
            }

            if ( header.magic[0] != 'Z' || header.magic[1] != '2' || header.magic[2] != 'H' || header.magic[3] != 'M' )
            {
                return false;
            }

            // Decompress the remainder into the output.
            size_t dataSize = (size_t)( input->GetSizeNative() - input->TellNative() );

            fileSystem->DecompressZLIBStream( input, output, dataSize, true );
            return true;
        }
    };

    compressionProvider* CreateProvider( void ) override
    {
        mh2zCompressionProvider *prov = new mh2zCompressionProvider();

        return prov;
    }

    void DestroyProvider( compressionProvider *prov ) override
    {
        mh2zCompressionProvider *dprov = (mh2zCompressionProvider*)prov;

        delete dprov;
    }
};

static optional_struct_space <PluginDependantStructRegister <mh2zCompressionEnv, mainWindowFactory_t>> mh2zComprRegister;

void InitializeMH2ZCompressionEnv( void )
{
    mh2zComprRegister.Construct( mainWindowFactory );
}

void ShutdownMH2ZCompressionEnv( void )
{
    mh2zComprRegister.Destroy();
}