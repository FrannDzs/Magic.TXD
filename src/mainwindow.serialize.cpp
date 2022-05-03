/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.serialize.cpp
*  PURPOSE:     Main editor disk serialization environment.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "guiserialization.hxx"

struct mainWindowSerializationEnv : public magicSerializationProvider
{
    inline void Initialize( MainWindow *mainWnd )
    {
        RegisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_MAINWINDOW, this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        UnregisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_MAINWINDOW );
    }

    enum eSelectedTheme
    {
        THEME_DARK,
        THEME_LIGHT,
        THEME_CUSTOM
    };

    struct mtxd_cfg_struct
    {
        bool addImageGenMipmaps;
        bool lockDownTXDPlatform;
        endian::little_endian <eSelectedTheme> selectedTheme;
        bool showLogOnWarning;
        bool showGameIcon;
        bool adjustTextureChunksOnImport;
        bool texaddViewportFill;
        bool texaddViewportScaled;
        bool texaddViewportBackground;
    };

    // We want to serialize RW config too.
    struct rwengine_cfg_struct
    {
        bool metaDataTagging;
        endian::little_endian <rw::int32> warning_level;
        bool ignoreSecureWarnings;
        bool fixIncompatibleRasters;
        bool compatTransformNativeImaging;
        bool preferPackedSampleExport;
        bool dxtPackedDecompression;
        bool ignoreBlockSerializationRegions;
        endian::little_endian <rw::eBlockAcquisitionMode> blockAcquisitionMode;
    };

    void Load( MainWindow *mainwnd, rw::BlockProvider& mtxdConfig ) override
    {
        // last directory we were in to save TXD file.
        mainwnd->lastTXDSaveDir = wide_to_qt( RwReadUnicodeString( mtxdConfig ) );

        // last directory we were in to add an image file.
        mainwnd->lastImageFileOpenDir = wide_to_qt( RwReadUnicodeString( mtxdConfig ) );

        mtxd_cfg_struct cfgStruct;
        mtxdConfig.readStruct( cfgStruct );

        mainwnd->addImageGenMipmaps = cfgStruct.addImageGenMipmaps;
        mainwnd->lockDownTXDPlatform = cfgStruct.lockDownTXDPlatform;

        // Select the appropriate theme.
        eSelectedTheme themeOption = cfgStruct.selectedTheme;

        bool hasCustomTheme = false;

        if ( themeOption == THEME_DARK )
        {
            mainwnd->setActiveTheme( L"dark" );
        }
        else if ( themeOption == THEME_LIGHT )
        {
            mainwnd->setActiveTheme( L"light" );
        }
        else if ( themeOption == THEME_CUSTOM )
        {
            hasCustomTheme = true;
        }

        mainwnd->showLogOnWarning = cfgStruct.showLogOnWarning;
        mainwnd->showGameIcon = cfgStruct.showGameIcon;
        mainwnd->adjustTextureChunksOnImport = cfgStruct.adjustTextureChunksOnImport;
        mainwnd->texaddViewportFill = cfgStruct.texaddViewportFill;
        mainwnd->texaddViewportScaled = cfgStruct.texaddViewportScaled;
        mainwnd->texaddViewportBackground = cfgStruct.texaddViewportBackground;

        // TXD log settings.
        {
            rw::BlockProvider logGeomBlock( &mtxdConfig, rw::CHUNK_STRUCT );

            int geomSize = (int)logGeomBlock.getBlockLength();

            QByteArray tmpArr( geomSize, 0 );

            logGeomBlock.read( tmpArr.data(), geomSize );

            // Restore geometry.
            mainwnd->txdLog->restoreGeometry( tmpArr );
        }

        // Read RenderWare settings.
        {
            rw::Interface *rwEngine = mainwnd->rwEngine;

            rw::BlockProvider rwsettingsBlock( &mtxdConfig );

            rwengine_cfg_struct rwcfg;
            rwsettingsBlock.readStruct( rwcfg );

            rwEngine->SetMetaDataTagging( rwcfg.metaDataTagging );
            rwEngine->SetWarningLevel( rwcfg.warning_level );
            rwEngine->SetIgnoreSecureWarnings( rwcfg.ignoreSecureWarnings );
            rwEngine->SetFixIncompatibleRasters( rwcfg.fixIncompatibleRasters );
            rwEngine->SetCompatTransformNativeImaging( rwcfg.compatTransformNativeImaging );
            rwEngine->SetPreferPackedSampleExport( rwcfg.preferPackedSampleExport );
            rwEngine->SetDXTPackedDecompression( rwcfg.dxtPackedDecompression );
            rwEngine->SetIgnoreSerializationBlockRegions( rwcfg.ignoreBlockSerializationRegions );
            rwEngine->SetBlockAcquisitionMode( rwcfg.blockAcquisitionMode );
        }

        if ( hasCustomTheme )
        {
            rw::rwStaticString <wchar_t> customThemeName = RwReadUnicodeString( mtxdConfig );
                
            mainwnd->setActiveTheme( customThemeName.GetConstString() );
        }

        // If we had valid configuration, we are not for the first time.
        mainwnd->isLaunchedForTheFirstTime = false;
    }

    void Save( const MainWindow *mainwnd, rw::BlockProvider& mtxdConfig ) const override
    {
        RwWriteUnicodeString( mtxdConfig, qt_to_widerw( mainwnd->lastTXDSaveDir ) );
        RwWriteUnicodeString( mtxdConfig, qt_to_widerw( mainwnd->lastImageFileOpenDir ) );

        mtxd_cfg_struct cfgStruct;
        cfgStruct.addImageGenMipmaps = mainwnd->addImageGenMipmaps;
        cfgStruct.lockDownTXDPlatform = mainwnd->lockDownTXDPlatform;

        // Write theme.
        eSelectedTheme themeOption = THEME_CUSTOM;
        filePath themeCurrentName = mainwnd->getActiveTheme();

        if ( themeCurrentName == L"dark" )
        {
            themeOption = THEME_DARK;
        }
        else if ( themeCurrentName == L"light" )
        {
            themeOption = THEME_LIGHT;
        }

        cfgStruct.selectedTheme = themeOption;
        cfgStruct.showLogOnWarning = mainwnd->showLogOnWarning;
        cfgStruct.showGameIcon = mainwnd->showGameIcon;
        cfgStruct.adjustTextureChunksOnImport = mainwnd->adjustTextureChunksOnImport;
        cfgStruct.texaddViewportFill = mainwnd->texaddViewportFill;
        cfgStruct.texaddViewportScaled = mainwnd->texaddViewportScaled;
        cfgStruct.texaddViewportBackground = mainwnd->texaddViewportBackground;

        mtxdConfig.writeStruct( cfgStruct );

        // TXD log properties.
        {
            QByteArray logGeom = mainwnd->txdLog->saveGeometry();

            rw::BlockProvider logGeomBlock( &mtxdConfig, rw::CHUNK_STRUCT );

            int geomSize = logGeom.size();

            logGeomBlock.write( logGeom.constData(), geomSize );
        }

        // RW engine properties.
        // Actually write those safely.
        {
            rw::Interface *rwEngine = mainwnd->rwEngine;

            rw::BlockProvider rwsettingsBlock( &mtxdConfig );
            
            rwengine_cfg_struct engineCfg;
            engineCfg.metaDataTagging = rwEngine->GetMetaDataTagging();
            engineCfg.warning_level = rwEngine->GetWarningLevel();
            engineCfg.ignoreSecureWarnings = rwEngine->GetIgnoreSecureWarnings();
            engineCfg.fixIncompatibleRasters = rwEngine->GetFixIncompatibleRasters();
            engineCfg.compatTransformNativeImaging = rwEngine->GetCompatTransformNativeImaging();
            engineCfg.preferPackedSampleExport = rwEngine->GetPreferPackedSampleExport();
            engineCfg.dxtPackedDecompression = rwEngine->GetDXTPackedDecompression();
            engineCfg.ignoreBlockSerializationRegions = rwEngine->GetIgnoreSerializationBlockRegions();
            engineCfg.blockAcquisitionMode = rwEngine->GetBlockAcquisitionMode();

            rwsettingsBlock.writeStruct( engineCfg );
        }

        if ( themeOption == THEME_CUSTOM )
        {
            RwWriteUnicodeString( mtxdConfig, themeCurrentName.convert_unicode <rw::RwStaticMemAllocator> () );
        }
    }
};

static optional_struct_space <PluginDependantStructRegister <mainWindowSerializationEnv, mainWindowFactory_t>> mainWindowSerializationEnvRegister;

void InitializeMainWindowSerializationBlock( void )
{
    mainWindowSerializationEnvRegister.Construct( mainWindowFactory );
}

void ShutdownMainWindowSerializationBlock( void )
{
    mainWindowSerializationEnvRegister.Destroy();
}