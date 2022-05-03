/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/tools/txdgen.h
*  PURPOSE:     Header of the mass TXD conversion tool.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef _TXDGEN_MODULE_
#define _TXDGEN_MODULE_

#include "shared.h"

class TxdGenModule : public MessageReceiver
{
public:
    inline TxdGenModule( rw::Interface *rwEngine )
    {
        this->rwEngine = rwEngine;
        this->_warningMan.module = this;
    }

    struct run_config
    {
        // By default, we create San Andreas files.
        rwkind::eTargetGame c_gameType = rwkind::GAME_GTASA;

        rw::rwStaticString <wchar_t> c_outputRoot = L"txdgen_out/";
        rw::rwStaticString <wchar_t> c_gameRoot = L"txdgen/";

        rwkind::eTargetPlatform c_targetPlatform = rwkind::PLATFORM_PC;

        bool c_clearMipmaps = false;

        bool c_generateMipmaps = false;

        rw::eMipmapGenerationMode c_mipGenMode = rw::MIPMAPGEN_DEFAULT;

        rw::uint32 c_mipGenMaxLevel = 0;

        bool c_improveFiltering = true;

        bool compressTextures = false;

        rw::ePaletteRuntimeType c_palRuntimeType = rw::PALRUNTIME_PNGQUANT;

        rw::eDXTCompressionMethod c_dxtRuntimeType = rw::DXTRUNTIME_SQUISH;

        bool c_reconstructIMGArchives = true;

        bool c_fixIncompatibleRasters = true;
        bool c_dxtPackedDecompression = false;

        bool c_imgArchivesCompressed = false;

        bool c_ignoreSerializationRegions = true;

        float c_compressionQuality = 1.0f;

        bool c_outputDebug = false;

        int c_warningLevel = 3;

        bool c_ignoreSecureWarnings = false;
    };

    run_config ParseConfig( CFileTranslator *root, const filePath& cfgPath ) const;

    bool ApplicationMain( const run_config& cfg );

    bool ProcessTXDArchive(
        CFileTranslator *srcRoot, CFile *srcStream, CFile *targetStream, rwkind::eTargetPlatform targetPlatform, rwkind::eTargetGame targetGame,
        bool clearMipmaps,
        bool generateMipmaps, rw::eMipmapGenerationMode mipGenMode, rw::uint32 mipGenMaxLevel,
        bool improveFiltering,
        bool doCompress, float compressionQuality,
        bool outputDebug, CFileTranslator *debugRoot,
        const rw::LibraryVersion& gameVersion,
        rw::rwStaticString <wchar_t>& errMsg
    );

    rw::Interface* GetEngine( void ) const
    {
        return this->rwEngine;
    }

    struct RwWarningBuffer : public rw::WarningManagerInterface
    {
        TxdGenModule *module;
        rw::rwStaticString <wchar_t> buffer;

        void Purge( void )
        {
            // Output the content to the stream.
            if ( buffer.GetLength() > 0 )
            {
                module->OnMessage( module->TOKEN( "TxdGen.HeaderWarnList" ) + L'\n' );

                buffer += L'\n';

                module->OnMessage( buffer );

                buffer.Clear();
            }
        }

        virtual void OnWarning( rw::rwStaticString <wchar_t>&& message ) override
        {
            if ( buffer.GetLength() > 0 )
            {
                buffer += L'\n';
            }

            buffer += message;
        }
    };

    RwWarningBuffer _warningMan;

private:
    rw::Interface *rwEngine;
};

#endif //_TXDGEN_MODULE_