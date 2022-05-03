/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/tools/txdgen.cpp
*  PURPOSE:     Mass TXD conversion tool.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "txdgen.h"

#include <iostream>
#include <streambuf>
#include <gtaconfig/include.h>

#include "dirtools.h"

#include <sdk/NumericFormat.h>

using namespace rwkind;


static inline void ConvertRasterToPlatformEx( TxdGenModule *module, rw::TextureBase *theTexture, rw::Raster *texRaster, rwkind::eTargetPlatform targetPlatform, rwkind::eTargetGame targetGame )
{
    bool hasConversionSucceeded = rwkind::ConvertRasterToPlatform( texRaster, targetPlatform, targetGame );

    if ( hasConversionSucceeded == false )
    {
        module->OnMessage( templ_repl( module->TOKEN( "TxdGen.TexConvFail" ), L"texname", theTexture->GetName().GetConstString() ) + L'\n' );
    }
}

bool TxdGenModule::ProcessTXDArchive(
    CFileTranslator *srcRoot, CFile *srcStream, CFile *targetStream, eTargetPlatform targetPlatform, eTargetGame targetGame,
    bool clearMipmaps,
    bool generateMipmaps, rw::eMipmapGenerationMode mipGenMode, rw::uint32 mipGenMaxLevel,
    bool improveFiltering,
    bool doCompress, float compressionQuality,
    bool outputDebug, CFileTranslator *debugRoot,
    const rw::LibraryVersion& gameVersion,
    rw::rwStaticString <wchar_t>& errMsg
)
{
    rw::Interface *rwEngine = this->rwEngine;

    bool hasProcessed = false;

    // Optimize the texture archive.
    rw::StreamPtr txd_stream = RwStreamCreateTranslated( rwEngine, srcStream );

    try
    {
        rw::TexDictionary *txd = nullptr;

        if ( txd_stream.is_good() )
        {
            try
            {
                rw::RwObject *rwObj = rwEngine->Deserialize( txd_stream );

                txd = rw::ToTexDictionary( rwEngine, rwObj );

                if ( txd == nullptr )
                {
                    errMsg = templ_repl( TOKEN( "TxdGen.NotATXD" ), L"what", rwEngine->GetObjectTypeName( rwObj ) );

                    rwEngine->DeleteRwObject( rwObj );
                }
            }
            catch( rw::RwException& except )
            {
                errMsg = templ_repl( TOKEN( "TxdGen.TXDLoadError" ), L"msg", rw::DescribeException( rwEngine, except ) );

                throw;
            }
        }

        if ( txd )
        {
            // Update the version of this texture dictionary.
            txd->SetEngineVersion( gameVersion );

            try
            {
                // Process all textures.
                bool processSuccessful = true;

                try
                {
                    for ( rw::TexDictionary::texIter_t iter = txd->GetTextureIterator(); !iter.IsEnd(); iter.Increment() )
                    {
                        rw::TextureBase *theTexture = iter.Resolve();

                        // Update the version of this texture.
                        theTexture->SetEngineVersion( gameVersion );

                        // We need to modify the raster.
                        rw::Raster *texRaster = theTexture->GetRaster();

                        if ( texRaster )
                        {
                            // Decide whether to convert to target architecture beforehand or afterward.
                            bool shouldConvertBeforehand = ShouldRasterConvertBeforehand( texRaster, targetPlatform );

                            bool hasConvertedToTargetArchitecture = false;

                            if ( shouldConvertBeforehand == true )
                            {
                                ConvertRasterToPlatformEx( this, theTexture, texRaster, targetPlatform, targetGame );

                                hasConvertedToTargetArchitecture = true;
                            }

                            // Clear mipmaps if requested.
                            if ( clearMipmaps )
                            {
                                texRaster->clearMipmaps();

                                theTexture->fixFiltering();
                            }

                            // Generate mipmaps on demand.
                            if ( generateMipmaps )
                            {
                                // We generate as many mipmaps as we can.
                                texRaster->generateMipmaps( mipGenMaxLevel + 1, mipGenMode );

                                theTexture->fixFiltering();
                            }

                            // Output debug stuff.
                            if ( outputDebug && debugRoot != nullptr )
                            {
                                // We want to debug mipmap generation, so output debug textures only using mipmaps.
                                //if ( _meetsDebugCriteria( tex ) )
                                {
                                    auto srcPath = srcStream->GetPath().convert_unicode <rw::RwStaticMemAllocator> ();

                                    filePath relSrcPath;

                                    bool hasRelSrcPath = srcRoot->GetRelativePathFromRoot( srcPath.GetConstString(), true, relSrcPath );

                                    if ( hasRelSrcPath )
                                    {
                                        // Create a unique filename for this texture.
                                        filePath directoryPart;

                                        filePath fileNamePart = FileSystem::GetFileNameItem <FileSysCommonAllocator> ( relSrcPath.c_str(), false, &directoryPart, nullptr );

                                        if ( fileNamePart.size() != 0 )
                                        {
                                            filePath uniqueTextureNameTGA = directoryPart + fileNamePart + "_" + filePath( theTexture->GetName() ) + ".tga";

                                            CFile *debugOutputStream = debugRoot->Open( uniqueTextureNameTGA, "wb" );

                                            if ( debugOutputStream )
                                            {
                                                // Create a debug raster.
                                                rw::Raster *newRaster = rw::CreateRaster( rwEngine );

                                                if ( newRaster )
                                                {
                                                    try
                                                    {
                                                        newRaster->newNativeData( "Direct3D9" );

                                                        // Put the debug content into it.
                                                        {
                                                            rw::Bitmap debugTexContent( rwEngine );

                                                            debugTexContent.setBgColor( 1, 1, 1 );

                                                            bool gotDebugContent = rw::DebugDrawMipmaps( rwEngine, texRaster, debugTexContent );

                                                            if ( gotDebugContent )
                                                            {
                                                                newRaster->setImageData( debugTexContent );
                                                            }
                                                        }

                                                        if ( newRaster->getMipmapCount() > 0 )
                                                        {
                                                            // Write the debug texture to it.
                                                            rw::Stream *outputStream = RwStreamCreateTranslated( rwEngine, debugOutputStream );

                                                            if ( outputStream )
                                                            {
                                                                try
                                                                {
                                                                    newRaster->writeImage( outputStream, "TGA" );
                                                                }
                                                                catch( ... )
                                                                {
                                                                    rwEngine->DeleteStream( outputStream );

                                                                    throw;
                                                                }

                                                                rwEngine->DeleteStream( outputStream );
                                                            }
                                                        }
                                                    }
                                                    catch( ... )
                                                    {
                                                        rw::DeleteRaster( newRaster );

                                                        throw;
                                                    }

                                                    rw::DeleteRaster( newRaster );
                                                }

                                                // Free the stream handle.
                                                delete debugOutputStream;
                                            }
                                        }
                                    }
                                }
                            }

                            // Palettize the texture to save space.
                            if ( doCompress )
                            {
                                // If we are not target architecture already, make sure we are.
                                if ( hasConvertedToTargetArchitecture == false )
                                {
                                    ConvertRasterToPlatformEx( this, theTexture, texRaster, targetPlatform, targetGame );

                                    hasConvertedToTargetArchitecture = true;
                                }

                                if ( targetPlatform == PLATFORM_PS2 )
                                {
                                    texRaster->optimizeForLowEnd( compressionQuality );
                                }
                                else if ( targetPlatform == PLATFORM_XBOX || targetPlatform == PLATFORM_PC )
                                {
                                    // Compress if we are not already compressed.
                                    texRaster->compress( compressionQuality );
                                }
                            }

                            // Improve the filtering mode if the user wants us to.
                            if ( improveFiltering )
                            {
                                theTexture->improveFiltering();
                            }

                            // Convert it into the target platform.
                            if ( shouldConvertBeforehand == false )
                            {
                                if ( hasConvertedToTargetArchitecture == false )
                                {
                                    ConvertRasterToPlatformEx( this, theTexture, texRaster, targetPlatform, targetGame );

                                    hasConvertedToTargetArchitecture = true;
                                }
                            }
                        }
                    }
                }
                catch( rw::RwException& except )
                {
                    errMsg = templ_repl( TOKEN( "TxdGen.TexProcError" ), L"msg", rw::DescribeException( rwEngine, except ) );

                    throw;
                }

#if 0
                // Introduce this stub when dealing with memory leaks.
                static int ok_num = 0;

                if ( ok_num++ > 100 )
                {
                    throw termination_request();
                }
#endif

                // We do not perform any error checking for now.
                if ( processSuccessful )
                {
                    // Write the TXD into the target stream.
                    rw::StreamPtr rwTargetStream = RwStreamCreateTranslated( rwEngine, targetStream );

                    if ( rwTargetStream.is_good() )
                    {
                        try
                        {
                            rwEngine->Serialize( txd, rwTargetStream );
                        }
                        catch( rw::RwException& except )
                        {
                            errMsg = templ_repl( TOKEN( "TxdGen.TXDWriteError" ), L"msg", rw::DescribeException( rwEngine, except ) );

                            throw;
                        }

                        hasProcessed = true;
                    }
                }
            }
            catch( ... )
            {
                rwEngine->DeleteRwObject( txd );

                txd = nullptr;

                throw;
            }

            // Delete the TXD.
            rwEngine->DeleteRwObject( txd );
        }
    }
    catch( rw::RwException& )
    {
        hasProcessed = false;
    }
    catch( ... )
    {
        hasProcessed = false;

        throw;
    }

    return hasProcessed;
}

struct _discFileSentry_txdgen
{
    TxdGenModule *module;
    rwkind::eTargetPlatform targetPlatform;
    rwkind::eTargetGame targetGame;
    bool clearMipmaps;
    bool generateMipmaps;
    rw::eMipmapGenerationMode mipGenMode;
    rw::uint32 mipGenMaxLevel;
    bool improveFiltering;
    bool doCompress;
    float compressionQuality;
    rw::LibraryVersion gameVersion;
    bool outputDebug;
    CFileTranslator *debugTranslator;

    inline bool OnSingletonFile(
        CFileTranslator *sourceRoot, CFileTranslator *buildRoot, const filePath& relPathFromRoot,
        const filePath& fileName, const filePath& extention, CFile *sourceStream,
        bool isInArchive
    )
    {
        // If we are asked to terminate, just do it.
        rw::CheckThreadHazards( module->GetEngine() );

        // Decide whether we need a copy.
        bool requiresCopy = false;

        bool anyWork = false;

        if ( extention.equals( "TXD", false ) == true || isInArchive )
        {
            requiresCopy = true;
        }

        // Open the target stream.
        CFile *targetStream = nullptr;

        if ( requiresCopy )
        {
            targetStream = buildRoot->Open( relPathFromRoot, L"wb" );
        }

        if ( targetStream && sourceStream )
        {
            // Allow to perform custom logic on the source and target streams.
            bool hasCopiedFile = false;
            {
                if ( extention.equals( "TXD", false ) == true )
                {
                    module->OnMessage( templ_repl( module->TOKEN( "TxdGen.ProcItem" ), L"what", relPathFromRoot.convert_unicode <rw::RwStaticMemAllocator> () ) );

                    rw::rwStaticString <wchar_t> errorMessage;

                    bool couldProcessTXD = this->module->ProcessTXDArchive(
                        sourceRoot, sourceStream, targetStream, this->targetPlatform, this->targetGame,
                        this->clearMipmaps,
                        this->generateMipmaps, this->mipGenMode, this->mipGenMaxLevel,
                        this->improveFiltering,
                        this->doCompress, this->compressionQuality,
                        this->outputDebug, this->debugTranslator,
                        this->gameVersion,
                        errorMessage
                    );

                    if ( couldProcessTXD )
                    {
                        hasCopiedFile = true;

                        anyWork = true;

                        module->OnMessage( module->TOKEN( "TxdGen.OK" ) + L"\n" );
                    }
                    else
                    {
                        module->OnMessage( templ_repl( module->TOKEN( "TxdGen.Error" ), L"msg", errorMessage ) + L"\n" );
                    }

                    // Output any warnings.
                    module->_warningMan.Purge();
                }
            }

            if ( requiresCopy )
            {
                // If we have not yet created the new copy, we default to simple stream swap.
                if ( !hasCopiedFile && targetStream )
                {
                    // Make sure we copy from the beginning of the source stream.
                    sourceStream->Seek( 0, SEEK_SET );

                    // Copy the stream contents.
                    FileSystem::StreamCopy( *sourceStream, *targetStream );

                    hasCopiedFile = true;
                }
            }
        }

        if ( targetStream )
        {
            delete targetStream;
        }

        return anyWork;
    }

    inline void OnArchiveFail( const filePath& fileName, const filePath& extention )
    {
        module->OnMessage( module->TOKEN( "TxdGen.IMGArchiveFail" ) + L"\n" );
    }
};

inline bool isGoodEngine( const rw::Interface *engineInterface )
{
    if ( engineInterface->IsObjectRegistered( "texture" ) == false )
        return false;

    if ( engineInterface->IsObjectRegistered( "texture_dictionary" ) == false )
        return false;

    // We are ready to go.
    return true;
}

TxdGenModule::run_config TxdGenModule::ParseConfig( CFileTranslator *root, const filePath& path ) const
{
    run_config cfg;

    CFile *cfgStream = root->Open( path, "rb" );

    if ( cfgStream )
    {
        CINI *configFile = LoadINI( cfgStream );

        delete cfgStream;

        if ( configFile )
        {
            if ( CINI::Entry *mainEntry = configFile->GetEntry( "Main" ) )
            {
                // Output root.
                if ( const char *newOutputRoot = mainEntry->Get( "outputRoot" ) )
                {
                    cfg.c_outputRoot = CharacterUtil::ConvertStrings <char8_t, wchar_t, rw::RwStaticMemAllocator> ( (const char8_t*)newOutputRoot );
                }

                // Game root.
                if ( const char *newGameRoot = mainEntry->Get( "gameRoot" ) )
                {
                    cfg.c_gameRoot = CharacterUtil::ConvertStrings <char8_t, wchar_t, rw::RwStaticMemAllocator> ( (const char8_t*)newGameRoot );
                }

                // Target Platform.
                const char *targetPlatform = mainEntry->Get( "targetPlatform" );

                if ( targetPlatform )
                {
                    rwkind::GetTargetPlatformFromFriendlyString( targetPlatform, cfg.c_targetPlatform );
                }

                // Target game version.
                if ( const char *targetVersion = mainEntry->Get( "targetVersion" ) )
                {
                    rwkind::GetTargetGameFromFriendlyString( targetVersion, cfg.c_gameType );
                }

                // Mipmap clear flag.
                if ( mainEntry->Find( "clearMipmaps" ) )
                {
                    cfg.c_clearMipmaps = mainEntry->GetBool( "clearMipmaps" );
                }

                // Mipmap Generation enable.
                if ( mainEntry->Find( "generateMipmaps" ) )
                {
                    cfg.c_generateMipmaps = mainEntry->GetBool( "generateMipmaps" );
                }

                // Mipmap Generation Mode.
                if ( const char *mipGenMode = mainEntry->Get( "mipGenMode" ) )
                {
                    if ( strieq( mipGenMode, "default" ) ||
                         strieq( mipGenMode, "recommended" ) )
                    {
                        cfg.c_mipGenMode = rw::MIPMAPGEN_DEFAULT;
                    }
                    else if ( strieq( mipGenMode, "contrast" ) )
                    {
                        cfg.c_mipGenMode = rw::MIPMAPGEN_CONTRAST;
                    }
                    else if ( strieq( mipGenMode, "brighten" ) )
                    {
                        cfg.c_mipGenMode = rw::MIPMAPGEN_BRIGHTEN;
                    }
                    else if ( strieq( mipGenMode, "darken" ) )
                    {
                        cfg.c_mipGenMode = rw::MIPMAPGEN_DARKEN;
                    }
                    else if ( strieq( mipGenMode, "selectclose" ) )
                    {
                        cfg.c_mipGenMode = rw::MIPMAPGEN_SELECTCLOSE;
                    }
                }

                // Mipmap generation maximum level.
                if ( mainEntry->Find( "mipGenMaxLevel" ) )
                {
                    int mipGenMaxLevelInt = mainEntry->GetInt( "mipGenMaxLevel" );

                    if ( mipGenMaxLevelInt >= 0 )
                    {
                        cfg.c_mipGenMaxLevel = (rw::uint32)mipGenMaxLevelInt;
                    }
                }

                // Filter mode improvement.
                if ( mainEntry->Find( "improveFiltering" ) )
                {
                    cfg.c_improveFiltering = mainEntry->GetBool( "improveFiltering" );
                }

                // Compression.
                if ( mainEntry->Find( "compressTextures" ) )
                {
                    cfg.compressTextures = mainEntry->GetBool( "compressTextures" );
                }

                // Compression quality.
                if ( mainEntry->Find( "compressionQuality" ) )
                {
                    cfg.c_compressionQuality = (float)mainEntry->GetFloat( "compressionQuality", 0.0 );
                }

                // Palette runtime type.
                if ( const char *palRuntimeType = mainEntry->Get( "palRuntimeType" ) )
                {
                    if ( strieq( palRuntimeType, "native" ) )
                    {
                        cfg.c_palRuntimeType = rw::PALRUNTIME_NATIVE;
                    }
                    else if ( strieq( palRuntimeType, "pngquant" ) )
                    {
                        cfg.c_palRuntimeType = rw::PALRUNTIME_PNGQUANT;
                    }
                }

                // DXT compression method.
                if ( const char *dxtCompressionMethod = mainEntry->Get( "dxtRuntimeType" ) )
                {
                    if ( strieq( dxtCompressionMethod, "native" ) )
                    {
                        cfg.c_dxtRuntimeType = rw::DXTRUNTIME_NATIVE;
                    }
                    else if ( strieq( dxtCompressionMethod, "squish" ) ||
                              strieq( dxtCompressionMethod, "libsquish" ) ||
                              strieq( dxtCompressionMethod, "recommended" ) )
                    {
                        cfg.c_dxtRuntimeType = rw::DXTRUNTIME_SQUISH;
                    }
                }

                // Warning level.
                if ( mainEntry->Find( "warningLevel" ) )
                {
                    cfg.c_warningLevel = mainEntry->GetInt( "warningLevel" );
                }

                // Ignore secure warnings.
                if ( mainEntry->Find( "ignoreSecureWarnings" ) )
                {
                    cfg.c_ignoreSecureWarnings = mainEntry->GetBool( "ignoreSecureWarnings" );
                }

                // Reconstruct IMG Archives.
                if ( mainEntry->Find( "reconstructIMGArchives" ) )
                {
                    cfg.c_reconstructIMGArchives = mainEntry->GetBool( "reconstructIMGArchives" );
                }

                // Fix incompatible rasters.
                if ( mainEntry->Find( "fixIncompatibleRasters" ) )
                {
                    cfg.c_fixIncompatibleRasters = mainEntry->GetBool( "fixIncompatibleRasters" );
                }

                // DXT packed decompression.
                if ( mainEntry->Find( "dxtPackedDecompression" ) )
                {
                    cfg.c_dxtPackedDecompression = mainEntry->GetBool( "dxtPackedDecompression" );
                }

                // IMG archive compression
                if ( mainEntry->Find( "imgArchivesCompressed" ) )
                {
                    cfg.c_imgArchivesCompressed = mainEntry->GetBool( "imgArchivesCompressed" );
                }

                // Serialization compatibility setting.
                if ( mainEntry->Find( "ignoreSerializationRegions" ) )
                {
                    cfg.c_ignoreSerializationRegions = mainEntry->GetBool( "ignoreSerializationRegions" );
                }

                // Debug output flag.
                if ( mainEntry->Find( "outputDebug" ) )
                {
                    cfg.c_outputDebug = mainEntry->GetBool( "outputDebug" );
                }
            }

            // Kill the configuration.
            delete configFile;
        }
    }

    return cfg;
}

bool TxdGenModule::ApplicationMain( const run_config& cfg )
{
    this->OnMessage(
        L"RenderWare TXD generator tool. Compiled on " __DATE__ L"\n\n"
    );

    bool successful = true;

    if ( isGoodEngine( rwEngine ) )
    {
        // Set up the warning buffer.
        rwEngine->SetWarningManager( &_warningMan );

        // Set some configuration.
        rwEngine->SetPaletteRuntime( cfg.c_palRuntimeType );
        rwEngine->SetDXTRuntime( cfg.c_dxtRuntimeType );

        // We inherit certain properties from Magic.TXD, so we do not want to set them here anymore.
#if 0
        rwEngine->SetDXTPackedDecompression( cfg.c_dxtPackedDecompression );

        rwEngine->SetFixIncompatibleRasters( cfg.c_fixIncompatibleRasters );

        rwEngine->SetIgnoreSerializationBlockRegions( cfg.c_ignoreSerializationRegions );

        rwEngine->SetWarningLevel( cfg.c_warningLevel );

        rwEngine->SetIgnoreSecureWarnings( cfg.c_ignoreSecureWarnings );
#endif

        // Output some debug info.
        this->OnMessage( this->TOKEN( "TxdGen.ConfigHeader" ) + L'\n' );

        this->OnMessage(
            L"* outputRoot: " + cfg.c_outputRoot + L"\n" \
            L"* gameRoot: " + cfg.c_gameRoot + L"\n"
        );

        eTargetGame targetGame = cfg.c_gameType;

        const char *strTargetVersion = "unknown";

        rw::LibraryVersion targetVersion;
        {
            // Determine the real target version.
            bool getVer = rwkind::GetTargetVersionFromPlatformAndGame( cfg.c_targetPlatform, targetGame, targetVersion, strTargetVersion );

            // Just pick a default if we could not get the version.
            if ( getVer == false )
            {
                targetVersion = rw::KnownVersions::getGameVersion( rw::KnownVersions::SA );

                strTargetVersion = "San Andreas (default)";
            }
        }

        auto ansi_msg = [&]( const rw::rwStaticString <char>& msg )
        {
            this->OnMessage( CharacterUtil::ConvertStrings <char, wchar_t> ( msg ) );
        };

        ansi_msg(
            rw::rwStaticString <char> ( "* targetVersion: " ) + strTargetVersion + " [rwver: " + targetVersion.toString() + "]\n"
        );

        const char *strTargetPlatform = "unknown";

        if ( cfg.c_targetPlatform == PLATFORM_PC )
        {
            strTargetPlatform = "PC";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_PS2 )
        {
            strTargetPlatform = "PS2";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_PSP )
        {
            strTargetPlatform = "PSP";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_XBOX )
        {
            strTargetPlatform = "XBOX";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_DXT_MOBILE )
        {
            strTargetPlatform = "S3TC [mobile]";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_PVR )
        {
            strTargetPlatform = "PowerVR [mobile]";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_ATC )
        {
            strTargetPlatform = "AMD [mobile]";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_UNC_MOBILE )
        {
            strTargetPlatform = "uncompressed [mobile]";
        }

        ansi_msg(
            rw::rwStaticString <char> ( "* targetPlatform: " ) + strTargetPlatform + "\n"
        );

        ansi_msg(
            rw::rwStaticString <char> ( "* clearMipmaps: " ) + ( cfg.c_clearMipmaps ? "true" : "false" ) + "\n"
        );

        ansi_msg(
            rw::rwStaticString <char> ( "* generateMipmaps: " ) + ( cfg.c_generateMipmaps ? "true" : "false" ) + "\n"
        );

        const char *mipGenModeString = "unknown";

        if ( cfg.c_mipGenMode == rw::MIPMAPGEN_DEFAULT )
        {
            mipGenModeString = "default";
        }
        else if ( cfg.c_mipGenMode == rw::MIPMAPGEN_CONTRAST )
        {
            mipGenModeString = "contrast";
        }
        else if ( cfg.c_mipGenMode == rw::MIPMAPGEN_BRIGHTEN )
        {
            mipGenModeString = "brighten";
        }
        else if ( cfg.c_mipGenMode == rw::MIPMAPGEN_DARKEN )
        {
            mipGenModeString = "darken";
        }
        else if ( cfg.c_mipGenMode == rw::MIPMAPGEN_SELECTCLOSE )
        {
            mipGenModeString = "selectclose";
        }

        ansi_msg(
            rw::rwStaticString <char> ( "* mipGenMode: " ) + mipGenModeString + "\n"
        );

        ansi_msg(
            "* mipGenMaxLevel: " + eir::to_string <char, rw::RwStaticMemAllocator> ( cfg.c_mipGenMaxLevel ) + "\n"
        );

        ansi_msg(
            rw::rwStaticString <char> ( "* improveFiltering: " ) + ( cfg.c_improveFiltering ? "true" : "false" ) + "\n"
        );

        ansi_msg(
            rw::rwStaticString <char> ( "* compressTextures: " ) + ( cfg.compressTextures ? "true" : "false" ) + "\n"
        );

        // TODO.
#if 0
        ansi_msg(
            "* compressionQuality: " + eir::to_string <char, rw::RwStaticMemAllocator> ( cfg.c_compressionQuality ) + "\n"
        );
#endif 

        const char *strPalRuntimeType = "unknown";

        rw::ePaletteRuntimeType actualPalRuntimeType = rwEngine->GetPaletteRuntime();

        if ( actualPalRuntimeType == rw::PALRUNTIME_NATIVE )
        {
            strPalRuntimeType = "native";
        }
        else if ( actualPalRuntimeType == rw::PALRUNTIME_PNGQUANT )
        {
            strPalRuntimeType = "pngquant";
        }

        ansi_msg(
            rw::rwStaticString <char> ( "* palRuntimeType: " ) + strPalRuntimeType + "\n"
        );

        rw::eDXTCompressionMethod actualDXTRuntimeType = rwEngine->GetDXTRuntime();

        const char *strDXTRuntimeType = "unknown";

        if ( actualDXTRuntimeType == rw::DXTRUNTIME_NATIVE )
        {
            strDXTRuntimeType = "native";
        }
        else if ( actualDXTRuntimeType == rw::DXTRUNTIME_SQUISH )
        {
            strDXTRuntimeType = "squish";
        }

        ansi_msg(
            rw::rwStaticString <char> ( "* dxtRuntimeType: " ) + strDXTRuntimeType + "\n"
        );

        ansi_msg(
            "* warningLevel: " + eir::to_string <char, rw::RwStaticMemAllocator> ( rwEngine->GetWarningLevel() ) + "\n"
        );

        ansi_msg(
            rw::rwStaticString <char> ( "* ignoreSecureWarnings: " ) + ( rwEngine->GetIgnoreSecureWarnings() ? "true" : "false" ) + "\n"
        );

        ansi_msg(
            rw::rwStaticString <char> ( "* reconstructIMGArchives: " ) + ( cfg.c_reconstructIMGArchives ? "true" : "false" ) + "\n"
        );

        ansi_msg(
            rw::rwStaticString <char> ( "* fixIncompatibleRasters: " ) + ( rwEngine->GetFixIncompatibleRasters() ? "true" : "false" ) + "\n"
        );

        ansi_msg(
            rw::rwStaticString <char> ( "* dxtPackedDecompression: " ) + ( rwEngine->GetDXTPackedDecompression() ? "true" : "false" ) + "\n"
        );

        ansi_msg(
            rw::rwStaticString <char> ( "* imgArchivesCompressed: " ) + ( cfg.c_imgArchivesCompressed ? "true" : "false" ) + "\n"
        );

        ansi_msg(
            rw::rwStaticString <char> ( "* ignoreSerializationRegions: " ) + ( rwEngine->GetIgnoreSerializationBlockRegions() ? "true" : "false" ) + "\n"
        );

        // Finish with a newline.
        this->OnMessage( L"\n" );

        // Do the conversion!
        {
            CFileTranslator *absGameRootTranslator = nullptr;
            CFileTranslator *absOutputRootTranslator = nullptr;

            bool hasGameRoot = obtainAbsolutePath( cfg.c_gameRoot.GetConstString(), absGameRootTranslator, false, true );
            bool hasOutputRoot = obtainAbsolutePath( cfg.c_outputRoot.GetConstString(), absOutputRootTranslator, true, true );

            // Create a debug directory if we want to output debug.
            CFileTranslator *absDebugOutputTranslator = nullptr;

            bool hasDebugRoot = false;

            if ( cfg.c_outputDebug )
            {
                hasDebugRoot = obtainAbsolutePath( L"debug_output/", absDebugOutputTranslator, true, true );
            }

            if ( hasGameRoot && hasOutputRoot )
            {
                try
                {
                    // Check for build root conflicts.
                    if ( isBuildRootConflict( absGameRootTranslator, absOutputRootTranslator ) )
                    {
                        this->OnMessage( this->TOKEN( "TxdGen.BuildRootConflict" ) + L"\n\n" );
                    }

                    // File roots are prepared.
                    // We can start processing files.
                    gtaFileProcessor <_discFileSentry_txdgen> fileProc( this );

                    fileProc.setArchiveReconstruction( cfg.c_reconstructIMGArchives );

                    fileProc.setUseCompressedIMGArchives( cfg.c_imgArchivesCompressed );

                    _discFileSentry_txdgen sentry;
                    sentry.module = this;
                    sentry.targetPlatform = cfg.c_targetPlatform;
                    sentry.targetGame = cfg.c_gameType;
                    sentry.clearMipmaps = cfg.c_clearMipmaps;
                    sentry.generateMipmaps = cfg.c_generateMipmaps;
                    sentry.mipGenMode = cfg.c_mipGenMode;
                    sentry.mipGenMaxLevel = cfg.c_mipGenMaxLevel;
                    sentry.improveFiltering = cfg.c_improveFiltering;
                    sentry.doCompress = cfg.compressTextures;
                    sentry.compressionQuality = cfg.c_compressionQuality;
                    sentry.gameVersion = targetVersion;
                    sentry.outputDebug = cfg.c_outputDebug;
                    sentry.debugTranslator = absDebugOutputTranslator;

                    fileProc.process( &sentry, absGameRootTranslator, absOutputRootTranslator );

                    // Output any warnings.
                    _warningMan.Purge();
                }
                catch( ... )
                {
                    // OK.
                    this->OnMessage(
                        this->TOKEN( "TxdGen.Termination" ) + L"\n"
                    );

                    successful = false;
                }
            }
            else
            {
                if ( !hasGameRoot )
                {
                    this->OnMessage( this->TOKEN( "TxdGen.GameRootFail" ) + L"\n" );
                }

                if ( !hasOutputRoot )
                {
                    this->OnMessage( this->TOKEN( "TxdGen.OutputRootFail" ) + L"\n" );
                }
            }

            // Clean up resources.
            if ( hasDebugRoot )
            {
                delete absDebugOutputTranslator;
            }

            if ( hasGameRoot )
            {
                delete absGameRootTranslator;
            }

            if ( hasOutputRoot )
            {
                delete absOutputRootTranslator;
            }
        }
    }
    else
    {
        this->OnMessage( this->TOKEN( "TxdGen.InternErr.Config" ) + L"\n" );
    }

    return successful;
}
