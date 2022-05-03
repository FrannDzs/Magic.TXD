/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/tools/txdbuild.cpp
*  PURPOSE:     Mass TXD builder tool.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "dirtools.h"

#include "txdbuild.h"

#include "configtree.h"

#include <gtaconfig/include.h>

#include <regex>

#include "imagepipe.hxx"

static const std::regex gameVer_regex( "(\\w+),(\\w+)" );
static const std::regex game_regex( "(\\w+),(\\w+)" );

static const std::regex size_tuple( "(\\d+),(\\d+)" );

// Helper to decide the native name also with user configuration.
inline rw::rwStaticString <char> DecidePlatformString(
    rw::Interface *engineInterface,
    const ConfigNode& cfgNode,
    rwkind::eTargetGame targetGame, rwkind::eTargetPlatform targetPlatformType
)
{
    // Check if there is a target platform in the configuration system.
    // If there is, then just use that, if it is indeed a valid platform.
    {
        std::string targetPlatform;

        if ( cfgNode.GetString( "platform", targetPlatform ) )
        {
            // Check if it even is a valid platform.
            if ( rw::IsNativeTexture( engineInterface, targetPlatform.c_str() ) )
            {
                // Alright, should work.
                return rw::rwStaticString <char> ( targetPlatform.c_str(), targetPlatform.size() );
            }
        }
    }

    // Return the thing that is given by the runtime as default.
    return rwkind::GetTargetNativeFormatName( targetPlatformType, targetGame );
}

struct txdBuildImageImportMethods : public makeRasterImageImportMethods
{
    inline txdBuildImageImportMethods( rw::Interface *engineInterface, TxdBuildModule *module ) : makeRasterImageImportMethods( engineInterface )
    {
        this->module = module;

        // Set some standard default values.
        // Update those before loading.
        this->targetPlatform = rwkind::PLATFORM_PC;
        this->targetGame = rwkind::GAME_GTASA;

        this->cfgNode = nullptr;
    }

    void OnWarning( rw::rwStaticString <wchar_t>&& msg ) const override final
    {
        this->module->OnMessage( templ_repl( this->module->TOKEN( "TxdBuild.ImportWarning" ), L"msg", msg ) + L'\n' );
    }
    
    void OnError( rw::rwStaticString <wchar_t>&& msg ) const override final
    {
        this->module->OnMessage( templ_repl( this->module->TOKEN( "TxdBuild.ImportError" ), L"msg", msg ) + L'\n' );
    }

    void OnWarning( QString&& msg ) const override
    {
        this->OnWarning( qt_to_widerw( msg ) );
    }

    void OnError( QString&& msg ) const override
    {
        this->OnError( qt_to_widerw( msg ) );
    }

    // Properties for loading.
    rwkind::eTargetPlatform targetPlatform;
    rwkind::eTargetGame targetGame;
    const ConfigNode *cfgNode;

    rw::rwStaticString <char> GetNativeTextureName( void ) const override
    {
        return DecidePlatformString( this->engineInterface, *this->cfgNode, this->targetGame, this->targetPlatform );
    }

private:
    TxdBuildModule *module;
};

static rw::TextureBase* BuilderMakeTextureFromStream(
    rw::Interface *rwEngine, rw::Stream *imgStream, const filePath& extention,
    TxdBuildModule *module,
    rwkind::eTargetGame targetGame, rwkind::eTargetPlatform targetPlatform,
    const ConfigNode& cfgNode
)
{
    txdBuildImageImportMethods imgImporter( rwEngine, module );

    // Set things up.
    // TODO: cache the image importer somewhere and just set things up here.
    imgImporter.targetGame = targetGame;
    imgImporter.targetPlatform = targetPlatform;
    imgImporter.cfgNode = &cfgNode;

    return RwMakeTextureFromStream( rwEngine, imgStream, extention, imgImporter );
}

inline bool getFilterModeFromString( const char *string, rw::eRasterStageFilterMode& filter_out )
{
    if ( strieq( string, "point" ) )
    {
        filter_out = rw::RWFILTER_POINT;
        return true;
    }
    else if ( strieq( string, "linear" ) )
    {
        filter_out = rw::RWFILTER_LINEAR;
        return true;
    }
    else if ( strieq( string, "point_mip_point" ) ||
              strieq( string, "point_point" ) )
    {
        filter_out = rw::RWFILTER_POINT_POINT;
        return true;
    }
    else if ( strieq( string, "linear_mip_point" ) ||
              strieq( string, "linear_point" ) )
    {
        filter_out = rw::RWFILTER_LINEAR_POINT;
        return true;
    }
    else if ( strieq( string, "point_mip_linear" ) ||
              strieq( string, "point_linear" ) )
    {
        filter_out = rw::RWFILTER_POINT_LINEAR;
        return true;
    }
    else if ( strieq( string, "linear_mip_linear" ) ||
              strieq( string, "linear_linear" ) )
    {
        filter_out = rw::RWFILTER_LINEAR_LINEAR;
        return true;
    }

    return false;
}

inline rw::eRasterStageFilterMode GetConfigNodeFilterMode( const ConfigNode& cfgNode, const std::string& key, rw::eRasterStageFilterMode defaultValue )
{
    std::string strVal;

    bool gotConfig = cfgNode.GetString( key, strVal );

    if ( gotConfig )
    {
        getFilterModeFromString( strVal.c_str(), defaultValue );
    }

    return defaultValue;
}

inline bool getTexAddressFromString( const char *string, rw::eRasterStageAddressMode& address_out )
{
    if ( strieq( string, "wrap" ) )
    {
        address_out = rw::RWTEXADDRESS_WRAP;
        return true;
    }
    else if ( strieq( string, "clamp" ) )
    {
        address_out = rw::RWTEXADDRESS_CLAMP;
        return true;
    }
    else if ( strieq( string, "mirror" ) )
    {
        address_out = rw::RWTEXADDRESS_MIRROR;
        return true;
    }
    else if ( strieq( string, "border" ) )
    {
        address_out = rw::RWTEXADDRESS_BORDER;
        return true;
    }

    return false;
}

inline rw::eRasterStageAddressMode GetConfigNodeAddressMode( const ConfigNode& cfgNode, const std::string& key, rw::eRasterStageAddressMode defaultValue )
{
    std::string strVal;

    bool gotConfig = cfgNode.GetString( key, strVal );

    if ( gotConfig )
    {
        getTexAddressFromString( strVal.c_str(), defaultValue );
    }

    return defaultValue;
}

inline bool getPaletteTypeFromString( const char *string, rw::ePaletteType& palTypeOut )
{
    if ( strieq( string, "PAL8" ) ||
         strieq( string, "8BIT" ) )
    {
        palTypeOut = rw::PALETTE_8BIT;
        return true;
    }
    else if ( strieq( string, "PAL4" ) ||
              strieq( string, "4BIT" ) )
    {
        palTypeOut = rw::PALETTE_4BIT;
        return true;
    }

    return false;
}

inline bool ParseStringToVersion( const char *string, rw::LibraryVersion& verOut )
{
    unsigned int rwLibMajor, rwLibMinor, rwRevMajor, rwRevMinor;

    int parseCount = sscanf( string, "%u.%u.%u.%u", &rwLibMajor, &rwLibMinor, &rwRevMajor, &rwRevMinor );

    if ( parseCount == 4 )
    {
        if ( rwLibMajor == 3 && rwLibMinor <= 7 && rwRevMajor <= 15 && rwRevMinor <= 63 )
        {
            verOut.rwLibMajor = (rw::uint8)rwLibMajor;
            verOut.rwLibMinor = (rw::uint8)rwLibMinor;
            verOut.rwRevMajor = (rw::uint8)rwRevMajor;
            verOut.rwRevMinor = (rw::uint8)rwRevMinor;
            verOut.buildNumber = 0xFFFF;
            return true;
        }
    }

    return false;
}

inline void PutVersionOnObject( rw::RwObject *rwObj, rwkind::eTargetPlatform gui_targetPlatform, rwkind::eTargetGame gui_targetGame, const ConfigNode& cfgNode )
{
    // We want to get the actual version to put on this object.
    // This can come from the GUI or the .ini configuration.
    rw::LibraryVersion reqObjVer;
    bool hasVersion = false;

    if ( !hasVersion )
    {
        std::string strVer;

        if ( cfgNode.GetString( "rwver", strVer ) )
        {
            if ( ParseStringToVersion( strVer.c_str(), reqObjVer ) )
            {
                // We got a version.
                hasVersion = true;
            }
        }
    }

    // If we failed to get the version from ini config, we get it from GUI.
    if ( !hasVersion )
    {
        const char *_unusedVerName;

        bool gotGUIVersion = rwkind::GetTargetVersionFromPlatformAndGame( gui_targetPlatform, gui_targetGame, reqObjVer, _unusedVerName );

        if ( gotGUIVersion )
        {
            hasVersion = true;
        }
    }

    // We should always get a version here.
    if ( hasVersion )
    {
        rwObj->SetEngineVersion( reqObjVer );
    }
}

void BuildSingleTexture(
    rw::Interface *rwEngine, rw::ObjectPtr <rw::TexDictionary>& texDictPtr, const filePath& txdWritePath,
    const filePath& texturePath, rw::Stream *imgStream,
    TxdBuildModule *module, const TxdBuildModule::run_config& config, const filePath& extention,
    const ConfigNode& cfgParent
)
{
    rw::TextureBase *imgTex = BuilderMakeTextureFromStream( rwEngine, imgStream, extention, module, config.targetGame, config.targetPlatform, cfgParent );

    if ( imgTex )
    {
        try
        {
            // Put the texture into a correct version.
            PutVersionOnObject( imgTex, config.targetPlatform, config.targetGame, cfgParent );

            // Give the texture a name based on the original filename.
            filePath texName = FileSystem::GetFileNameItem( texturePath, false );

            auto ansiTexName = texName.convert_ansi <rw::RwStaticMemAllocator> ();

            // Tell the runtime that we process a texture.
            {
                module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.ProcessEntry" ), L"what", ansiTexName.GetConstString() ) + L"\n" );
            }

            imgTex->SetName( ansiTexName.GetConstString() );

            // Set some default rendering properties.
            imgTex->SetFilterMode(
                GetConfigNodeFilterMode( cfgParent, "filterMode", rw::RWFILTER_LINEAR )
            );
            imgTex->SetUAddressing(
                GetConfigNodeAddressMode( cfgParent, "uAddress", rw::RWTEXADDRESS_WRAP )
            );
            imgTex->SetVAddressing(
                GetConfigNodeAddressMode( cfgParent, "vAddress", rw::RWTEXADDRESS_WRAP )
            );

            // Scale the raster?
            {
                std::string strSize;

                if ( cfgParent.GetString( "size", strSize ) )
                {
                    // Try parsing a valid size tuple.
                    // If successful, resize things.
                    unsigned int width, height;

                    int parseCount = sscanf( strSize.c_str(), "%u,%u", &width, &height );

                    if ( parseCount == 2 )
                    {
                        // Do the resize with default filters.
                        rw::Raster *texRaster = imgTex->GetRaster();

                        if ( texRaster )
                        {
                            texRaster->resize( width, height );
                        }
                    }
                }
            }

            // Generate mipmaps?
            if ( GetConfigNodeBoolean( cfgParent, "genMipmaps", false ) )
            {
                rw::Raster *texRaster = imgTex->GetRaster();

                if ( texRaster )
                {
                    int genMipMaxLevel = GetConfigNodeInt( cfgParent, "genMipMaxLevel", 32 );

                    texRaster->generateMipmaps( genMipMaxLevel );
                }
            }

            // We want to palettize?
            if ( GetConfigNodeBoolean( cfgParent, "palettized", false ) )
            {
                rw::Raster *texRaster = imgTex->GetRaster();

                if ( texRaster )
                {
                    // Decide what palette format.
                    rw::ePaletteType paletteType = rw::PALETTE_8BIT;
                    {
                        std::string palName = GetConfigNodeString( cfgParent, "palType", "PAL8" );

                        getPaletteTypeFromString( palName.c_str(), paletteType );
                    }

                    texRaster->convertToPalette( paletteType, rw::RASTER_8888 );    // maximum palette quality.
                }
            }

            // Maybe this texture wants to be compressed.
            if ( GetConfigNodeBoolean( cfgParent, "compressed", false ) )
            {
                // Lets do it.
                rw::Raster *texRaster = imgTex->GetRaster();

                if ( texRaster )
                {
                    float comprQuality = (float)GetConfigNodeFloat( cfgParent, "comprQuality", 1.0 );

                    texRaster->compress( comprQuality );
                }
            }

            // ;)
            imgTex->fixFiltering();

            // Since we do have a texture we allocate a texture dictionary on demand.
            if ( texDictPtr.is_good() == false )
            {
                // Send a status message about our build process.
                module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.BuildMsg" ), L"what", txdWritePath.convert_unicode <rw::RwStaticMemAllocator> () ) + L'\n' );

                texDictPtr = rw::CreateTexDictionary( rwEngine );
            }

            // Add our texture to the dictionary!
            // Will throw an exception if the texture dictionary is nullptr.
            imgTex->AddToDictionary( texDictPtr );
        }
        catch( ... )
        {
            // In very rare cases we might have encountered an error.
            // This means that we decided against the texture, so delete it.
            rwEngine->DeleteRwObject( imgTex );

            throw;
        }
    }
}

inline void InstrumentConfigKeys( rw::Interface *rwEngine, TxdBuildModule *module, ConfigNode& txdConfigNode, CINI::Entry *entry )
{
    entry->ForAllEntries(
        [&]( const CINI::Entry::Setting& cfg )
    {
        // Check all kinds of keys here and plant them into the configuration system properly.
        if ( strieq( cfg.key, "platform" ) )
        {
            // Is it even a platform?
            if ( rw::IsNativeTexture( rwEngine, cfg.value ) )
            {
                txdConfigNode.SetString( "platform", cfg.value );
            }
            else
            {
                module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.NotAPlatform" ), L"what", cfg.value ) + L'\n' );
            }
        }
        else if ( strieq( cfg.key, "rwversion" ) ||
                  strieq( cfg.key, "version" ) ||
                  strieq( cfg.key, "rw_version" ) ||
                  strieq( cfg.key, "rwver" ) )
        {
            txdConfigNode.SetString( "rwver", cfg.value );
        }
        else if ( strieq( cfg.key, "gameVer" ) ||
                  strieq( cfg.key, "gameVersion" ) )
        {
            // Process this and try to set a better RW version if we found a match.
            std::cmatch verMatch;

            bool didMatch = std::regex_match( cfg.value, verMatch, gameVer_regex );

            if ( didMatch && verMatch.size() == 3 )
            {
                // Do things.
                const std::string& gameName = verMatch[1];
                const std::string& gamePlat = verMatch[2];

                // Try to find a configuration that matches this description.
                rw::LibraryVersion libVer;
                bool hasVersion = false;
                {
                    // Try with built-in first.
                    rwkind::eTargetGame builtinGame;

                    if ( rwkind::GetTargetGameFromFriendlyString( gameName.c_str(), builtinGame ) )
                    {
                        rwkind::eTargetPlatform builtinPlat;

                        if ( rwkind::GetTargetPlatformFromFriendlyString( gamePlat.c_str(), builtinPlat ) )
                        {
                            // Get the config for this.
                            const char *unused;

                            hasVersion = rwkind::GetTargetVersionFromPlatformAndGame( builtinPlat, builtinGame, libVer, unused );
                        }
                    }

                    // TODO: check game registry if built-in was not successful.
                    if ( !hasVersion )
                    {
                        //
                    }
                }

                // If we have a version, set it as current.
                if ( hasVersion )
                {
                    auto verStr = libVer.toString( false );

                    txdConfigNode.SetString( "rwver", verStr.GetConstString() );
                }
                else
                {
                    module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.GameVerMapFail" ), L"what", cfg.value ) + L'\n' );
                }
            }
            else
            {
                module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.GameVerParseFail" ), L"what", cfg.value ) + L'\n' );
            }
        }
        else if ( strieq( cfg.key, "game" ) )
        {
            // We do very similar work to gameVer, just more.
            std::cmatch verMatch;

            bool didMatch = std::regex_match( cfg.value, verMatch, game_regex );

            if ( didMatch && verMatch.size() == 3 )
            {
                // Do things.
                const std::string& gameName = verMatch[1];
                const std::string& gamePlat = verMatch[2];

                // Try to find a configuration that matches this description.
                rw::LibraryVersion libVer;
                bool hasVersion = false;
                const char *targetNativeName = nullptr;
                {
                    // Try with built-in first.
                    rwkind::eTargetGame builtinGame;

                    if ( rwkind::GetTargetGameFromFriendlyString( gameName.c_str(), builtinGame ) )
                    {
                        rwkind::eTargetPlatform builtinPlat;

                        if ( rwkind::GetTargetPlatformFromFriendlyString( gamePlat.c_str(), builtinPlat ) )
                        {
                            // Get the config for this.
                            const char *unused;

                            hasVersion = rwkind::GetTargetVersionFromPlatformAndGame( builtinPlat, builtinGame, libVer, unused );

                            targetNativeName = rwkind::GetTargetNativeFormatName( builtinPlat, builtinGame );
                        }
                    }

                    // TODO: check game registry if built-in was not successful.
                    if ( !hasVersion )
                    {
                        //
                    }

                    if ( !targetNativeName )
                    {
                        //
                    }
                }

                // If we have a version, set it as current.
                if ( hasVersion )
                {
                    auto verStr = libVer.toString( false );

                    txdConfigNode.SetString( "rwver", verStr.GetConstString() );
                }

                if ( targetNativeName != nullptr )
                {
                    txdConfigNode.SetString( "platform", targetNativeName );
                }

                if ( !hasVersion && targetNativeName == nullptr )
                {
                    module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.GameMapFail" ), L"what", cfg.value ) + L'\n' );
                }
            }
            else
            {
                module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.GameParseFail" ), L"what", cfg.value ) + L'\n' );
            }
        }
        else if ( strieq( cfg.key, "size" ) )
        {
            // Ability to scale texture to a fixed size.
            txdConfigNode.SetString( "size", cfg.value );
        }
        else if ( strieq( cfg.key, "filterMode" ) )
        {
            rw::eRasterStageFilterMode filterOut;

            if ( getFilterModeFromString( cfg.value, filterOut ) )
            {
                txdConfigNode.SetString( "filterMode", cfg.value );
            }
            else
            {
                module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.NotAFilterMode" ), L"what", cfg.value ) + L'\n' );
            }
        }
        else if ( strieq( cfg.key, "uAddress" ) ||
                  strieq( cfg.key, "uAddressing" ) ||
                  strieq( cfg.key, "u_address" ) )
        {
            rw::eRasterStageAddressMode addressOut;

            if ( getTexAddressFromString( cfg.value, addressOut ) )
            {
                txdConfigNode.SetString( "uAddress", cfg.value );
            }
            else
            {
                module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.NotAUAddrMode" ), L"what", cfg.value ) + L'\n' );
            }
        }
        else if ( strieq( cfg.key, "vAddress" ) ||
                  strieq( cfg.key, "vAddressing" ) ||
                  strieq( cfg.key, "v_address" ) )
        {
            rw::eRasterStageAddressMode addressOut;

            if ( getTexAddressFromString( cfg.value, addressOut ) )
            {
                txdConfigNode.SetString( "vAddress", cfg.value );
            }
            else
            {
                module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.NotAVAddrMode" ), L"what", cfg.value ) + L'\n' );
            }
        }
        else if ( strieq( cfg.key, "compressed" ) )
        {
            txdConfigNode.SetBoolean( "compressed", entry->GetBool( cfg.key ) );
        }
        else if ( strieq( cfg.key, "comprQuality" ) ||
                  strieq( cfg.key, "compressionQuality" ) )
        {
            txdConfigNode.SetBoolean( "comprQuality", entry->GetInt( cfg.key ) );
        }
        else if ( strieq( cfg.key, "palettized" ) )
        {
            txdConfigNode.SetBoolean( "palettized", entry->GetBool( cfg.key ) );
        }
        else if ( strieq( cfg.key, "palType" ) ||
                  strieq( cfg.key, "paletteType" ) )
        {
            rw::ePaletteType palTypeOut;

            if ( getPaletteTypeFromString( cfg.value, palTypeOut ) )
            {
                txdConfigNode.SetString( "palType", cfg.value );
            }
            else
            {
                module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.NotAPalType" ), L"what", cfg.value ) + L'\n' );
            }
        }
        else if ( strieq( cfg.key, "genMipmaps" ) ||
                  strieq( cfg.key, "generateMipmaps" ) )
        {
            txdConfigNode.SetBoolean( "genMipmaps", entry->GetBool( cfg.key ) );
        }
        else if ( strieq( cfg.key, "genMipMaxLevel" ) ||
                  strieq( cfg.key, "maxMipLevel" ) )
        {
            txdConfigNode.SetInt( "genMipMaxLevel", entry->GetInt( cfg.key ) );
        }
    });
}

void ReadConfigurationBlock(
    rw::Interface *rwEngine,
    CFileTranslator *gameRoot, const filePath& path,
    ConfigNode& cfgNode,
    TxdBuildModule *module
)
{
    if ( CFile *iniStream = gameRoot->Open( path, "rb" ) )
    {
        CINI *iniConfig = LoadINI( iniStream );

        delete iniStream;

        if ( iniConfig )
        {
            // Just read what is available.
            if ( CINI::Entry *mainEntry = iniConfig->GetEntry( "main" ) )
            {
                InstrumentConfigKeys( rwEngine, module, cfgNode, mainEntry );
            }

            delete iniConfig;
        }
    }
}

void BuildTXDArchives(
    rw::Interface *rwEngine,
    TxdBuildModule *module, CFileTranslator *gameRoot, CFileTranslator *outputRoot,
    const TxdBuildModule::run_config& config, const ConfigNode& cfgNode
)
{
    // Process things.
    auto dir_callback = [&]( const filePath& dirPath )
    {
        try
        {
            // Prepare the TXD write path.
            filePath txdRelativeDirPath;

            bool hasTXDWritePath = gameRoot->GetRelativePathFromRoot( dirPath, false, txdRelativeDirPath );

            filePath txdWritePath;

            if ( hasTXDWritePath )
            {
                // Make a copy but use it to create the relative TXD output path.
                txdWritePath = txdRelativeDirPath;

                // Trimm off the slash, if it exists.
                {
                    size_t outPathLen = txdWritePath.size();

                    if ( outPathLen > 0 )
                    {
                        txdWritePath.resize( outPathLen - 1 );  // Here cannot be encoding issues as long as the character is a traditional slash.
                    }
                }

                txdWritePath += L".txd";
            }

            // We can only continue if we actually have a valid location to write our TXD to.
            if ( hasTXDWritePath )
            {
                // Load configuration for this TXD.
                ConfigNode txdConfigNode;
                txdConfigNode.SetParent( &cfgNode );
                {
                    filePath iniPath = dirPath + L"_build.ini";

                    ReadConfigurationBlock(
                        rwEngine,
                        gameRoot, std::move( iniPath ),
                        txdConfigNode,
                        module
                    );
                }

                // Only create the TXD on demand.
                rw::ObjectPtr <rw::TexDictionary> texDict;

                // Add all textures to this TXD.
                {
                    auto per_dir_file_cb = [&]( const filePath& texturePath )
                    {
                        // We first have to establish a stream to the file.
                        CFile *fsImgStream = gameRoot->Open( texturePath, L"rb" );

                        if ( fsImgStream )
                        {
                            try
                            {
                                // Decompress if we find compressed things. ;)
                                fsImgStream = module->WrapStreamCodec( fsImgStream );
                            }
                            catch( ... )
                            {
                                delete fsImgStream;

                                throw;
                            }
                        }

                        if ( fsImgStream )
                        {
                            try
                            {
                                // Try to turn this file into a texture.
                                try
                                {
                                    rw::StreamPtr imgStream = RwStreamCreateTranslated( rwEngine, fsImgStream );

                                    if ( imgStream.is_good() )
                                    {
                                        // We have to parse the path to this texture.
                                        filePath pathToTexture;

                                        bool gotPath = gameRoot->GetRelativePathFromRoot( texturePath, false, pathToTexture );

                                        if ( gotPath )
                                        {
                                            filePath extOut;

                                            filePath fileNameItem = FileSystem::GetFileNameItem <FileSysCommonAllocator> ( texturePath, false, nullptr, &extOut );

                                            // Ignore some extensions.
                                            // Those are used for meta-properties of textures.
                                            if ( extOut != L"ini" )
                                            {
                                                // Alright, this is a candidate for a valid texture!
                                                // Let process this entry.

                                                // Load configuration for this texture.
                                                ConfigNode textureCfgNode;
                                                textureCfgNode.SetParent( &txdConfigNode );
                                                {
                                                    filePath texIniPath = ( pathToTexture + fileNameItem + L".ini" );

                                                    ReadConfigurationBlock(
                                                        rwEngine,
                                                        gameRoot, std::move( texIniPath ),
                                                        textureCfgNode,
                                                        module
                                                    );
                                                }

                                                // We got all streams prepared!
                                                // Try turning it into a texture now.
                                                BuildSingleTexture(
                                                    rwEngine, texDict, txdWritePath,
                                                    texturePath, imgStream,
                                                    module, config, extOut,
                                                    textureCfgNode
                                                );
                                            }
                                        }
                                    }
                                }
                                catch( rw::RwException& except )
                                {
                                    // Tell the runtime about any errors.
                                    module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.TexBuildFail" ), L"why", rw::DescribeException( rwEngine, except ) ) + L'\n' );

                                    // Continue. This is just one of many textures.
                                }
                            }
                            catch( ... )
                            {
                                delete fsImgStream;

                                throw;
                            }

                            delete fsImgStream;
                        }
                        else
                        {
                            module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.TexOpenFail" ), L"path", texturePath.convert_unicode <rw::RwStaticMemAllocator> () ) + L'\n' );
                        }

                        // Allow termination per texture.
                        rw::CheckThreadHazards( rwEngine );
                    };

                    gameRoot->ScanDirectory( dirPath, "*", false, nullptr, std::move( per_dir_file_cb ), nullptr );
                }

                // If we have at least one texture in this texture dictionary, we can initialize it and write away.
                if ( texDict.is_good() == false )
                {
                    module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.NoTexturesInDir" ), L"dir", txdRelativeDirPath.convert_unicode <rw::RwStaticMemAllocator> () ) + L'\n' );
                }
                else if ( texDict->GetTextureCount() != 0 )
                {
                    // We give this TXD the version of the first texture inside, for good measure.
                    rw::TextureBase *firstTex = texDict->GetTextureIterator().Resolve();

                    texDict->SetEngineVersion( firstTex->GetEngineVersion() );

                    // Maybe the config has a better version.
                    PutVersionOnObject( texDict, config.targetPlatform, config.targetGame, txdConfigNode );

                    // Now write it to disk.
                    // We want to write it with the same name as the directory had.
                    // Here we can use a trick: trimm of the last character of the directory path, always a slash, and replace it with ".txd" !
                    // The path has to be relative, as we want to write it into the output root.

                    // Now establish the stream and push it!
                    CFile *fsTXDStream = outputRoot->Open( txdWritePath, L"wb" );

                    if ( fsTXDStream )
                    {
                        try
                        {
                            rw::Stream *txdStream = RwStreamCreateTranslated( rwEngine, fsTXDStream );

                            if ( txdStream )
                            {
                                try
                                {
                                    // Finally, get to write this thing.
                                    rwEngine->Serialize( texDict, txdStream );
                                }
                                catch( ... )
                                {
                                    rwEngine->DeleteStream( txdStream );

                                    throw;
                                }

                                rwEngine->DeleteStream( txdStream );
                            }
                        }
                        catch( ... )
                        {
                            delete fsTXDStream;

                            throw;
                        }

                        delete fsTXDStream;
                    }
                    else
                    {
                        module->OnMessage( module->TOKEN( "TxdBuild.TXDOpenWriteFail") + L"\n" );
                    }
                }
            }

            // Allow termination per TXD archive.
            rw::CheckThreadHazards( rwEngine );
        }
        catch( rw::RwException& except )
        {
            // Ignore any errors we encounter at processing a TXD, so other TXDs can try processing.
            module->OnMessage( templ_repl( module->TOKEN( "TxdBuild.TXDBuildFail" ), L"why", rw::DescribeException( rwEngine, except ) ) + L'\n' );
        }
    };

    // Let us use the kickass C++11 lambdas :)
    gameRoot->ScanDirectory( "//", "*", true, std::move( dir_callback ), nullptr, nullptr );
}

bool TxdBuildModule::RunApplication( const run_config& config )
{
    rw::Interface *rwEngine = this->rwEngine;

    try
    {
        // Isolate us.
        rw::AssignThreadedRuntimeConfig( rwEngine );

        try
        {
            // Give some start message.
            this->OnMessage( L"\n" + this->TOKEN( "TxdBuild.StartProc" ) + L"\n\n" );

            // Intercept warnings and send them to our system.
            rwEngine->SetWarningLevel( 4 );
            rwEngine->SetWarningManager( this );

            // The main configuration node.
            // Initialize it.
            ConfigNode rootNode;
            rootNode.SetBoolean( "genMipmaps", config.generateMipmaps );

            if ( config.generateMipmaps )
            {
                rootNode.SetInt( "genMipMaxLevel", config.curMipMaxLevel );
            }

            rootNode.SetBoolean( "compressed", config.doCompress );

            if ( config.doCompress )
            {
                rootNode.SetFloat( "comprQuality", config.compressionQuality );
            }

            rootNode.SetBoolean( "palettized", config.doPalettize );

            if ( config.doPalettize )
            {
                rw::ePaletteType paletteType = config.paletteType;

                if ( paletteType == rw::PALETTE_4BIT || paletteType == rw::PALETTE_4BIT_LSB )
                {
                    rootNode.SetString( "palType", "PAL4" );
                }
                else if ( paletteType == rw::PALETTE_8BIT )
                {
                    rootNode.SetString( "palType", "PAL8" );
                }
            }

            // Get handles to the input and output directories.
            CFileTranslator *gameRootTranslator = nullptr;

            bool hasGameRoot = obtainAbsolutePath( config.gameRoot.GetConstString(), gameRootTranslator, false );

            if ( hasGameRoot )
            {
                try
                {
                    CFileTranslator *outputRootTranslator = nullptr;

                    bool hasOutputRoot = obtainAbsolutePath( config.outputRoot.GetConstString(), outputRootTranslator, true );

                    if ( hasOutputRoot )
                    {
                        try
                        {
                            if ( hasGameRoot && hasOutputRoot )
                            {
                                BuildTXDArchives( this->rwEngine, this, gameRootTranslator, outputRootTranslator, config, rootNode );
                            }
                        }
                        catch( ... )
                        {
                            delete outputRootTranslator;

                            throw;
                        }

                        delete outputRootTranslator;
                    }
                    else
                    {
                        this->OnMessage( this->TOKEN( "TxdBuild.DestDirFail" ) + L"\n" );
                    }
                }
                catch( ... )
                {
                    delete gameRootTranslator;

                    throw;
                }

                delete gameRootTranslator;
            }
            else
            {
                this->OnMessage( this->TOKEN( "TxdBuild.SrcDirFail" ) + L"\n" );
            }

            // Give a nice finish message.
            this->OnMessage( L"\n" + this->TOKEN( "TxdBuild.Finished" ) );
        }
        catch( ... )
        {
            rw::ReleaseThreadedRuntimeConfig( rwEngine );

            throw;
        }

        rw::ReleaseThreadedRuntimeConfig( rwEngine );
    }
    catch( ... )
    {
        this->OnMessage( L"\n\n" + this->TOKEN( "TxdBuild.UnexpectedTerm" ) );

        throw;
    }

    return true;
}

void TxdBuildModule::OnWarning( rw::rwStaticString <wchar_t>&& msg )
{
    // Forward things to the management.
    this->OnMessage( templ_repl( this->TOKEN( "TxdBuild.Warn" ), L"msg", msg ) + L'\n' );
}
