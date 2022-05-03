/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.menu.cpp
*  PURPOSE:     Editor menubar and it's actions.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include <mainwindow.h>

#include <QtWidgets/QMenuBar>
#include <QtGui/QDesktopServices>

#include "tools/dirtools.h"
#include "createtxddlg.h"
#include "texnamewindow.h"
#include "resizewindow.h"
#include "exportallwindow.h"
#include "renderpropwindow.h"
#include "rwversiondialog.h"
#include "optionsdialog.h"
#include "texadddialog.h"
#include "massbuild.h"
#include "massconvert.h"
#include "massexport.h"

struct mainWindowMenuEnv : public QObject, public magicThemeAwareItem
{
    inline void Initialize( MainWindow *mainWnd )
    {
        rw::Interface *rwEngine = mainWnd->GetEngine();

        // Required so that we can uncheck the theme checkboxes without actually triggering the handlers.
        this->recheckingThemeItem = false;

        /* --- Menu --- */
        QMenuBar *menu = new QMenuBar;

        fileMenu = menu->addMenu("");
        QAction *actionNew = CreateMnemonicActionL( "Main.File.New", this );
        actionNew->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_N ) );
        fileMenu->addAction(actionNew);

        this->actionNewTXD = actionNew;

        connect( actionNew, &QAction::triggered, this, &mainWindowMenuEnv::onCreateNewTXD );

        QAction *actionOpen = CreateMnemonicActionL( "Main.File.Open", this );
        actionOpen->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_O ) );
        fileMenu->addAction(actionOpen);

        this->actionOpenTXD = actionOpen;

        connect( actionOpen, &QAction::triggered, this, &mainWindowMenuEnv::onOpenFile );

        QAction *actionSave = CreateMnemonicActionL( "Main.File.Save", this );
        actionSave->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_S ) );
        fileMenu->addAction(actionSave);

        this->actionSaveTXD = actionSave;

        connect( actionSave, &QAction::triggered, this, &mainWindowMenuEnv::onRequestSaveTXD );

        QAction *actionSaveAs = CreateMnemonicActionL( "Main.File.SaveAs", this );
        actionSaveAs->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_A ) );
        fileMenu->addAction(actionSaveAs);

        this->actionSaveTXDAs = actionSaveAs;

        connect( actionSaveAs, &QAction::triggered, this, &mainWindowMenuEnv::onRequestSaveAsTXD );

        QAction *closeCurrent = CreateMnemonicActionL( "Main.File.Close", this );
        fileMenu->addAction(closeCurrent);
        fileMenu->addSeparator();

        this->actionCloseTXD = closeCurrent;

        connect( closeCurrent, &QAction::triggered, this, &mainWindowMenuEnv::onCloseCurrent );

        QAction *actionQuit = CreateMnemonicActionL( "Main.File.Quit", this );
        fileMenu->addAction(actionQuit);

        editMenu = menu->addMenu("");
        QAction *actionAdd = CreateMnemonicActionL( "Main.Edit.Add", this );
        actionAdd->setShortcut( Qt::Key_Insert );
        editMenu->addAction(actionAdd);

        this->actionAddTexture = actionAdd;

        connect( actionAdd, &QAction::triggered, this, &mainWindowMenuEnv::onAddTexture );

        QAction *actionReplace = CreateMnemonicActionL( "Main.Edit.Replace", this );
        actionReplace->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_R ) );
        editMenu->addAction(actionReplace);

        this->actionReplaceTexture = actionReplace;

        connect( actionReplace, &QAction::triggered, this, &mainWindowMenuEnv::onReplaceTexture );

        QAction *actionRemove = CreateMnemonicActionL( "Main.Edit.Remove", this );
        actionRemove->setShortcut( Qt::Key_Delete );
        editMenu->addAction(actionRemove);

        this->actionRemoveTexture = actionRemove;

        connect( actionRemove, &QAction::triggered, this, &mainWindowMenuEnv::onRemoveTexture );

        QAction *actionRename = CreateMnemonicActionL( "Main.Edit.Rename", this );
        actionRename->setShortcut( Qt::Key_F2 );
        editMenu->addAction(actionRename);

        this->actionRenameTexture = actionRename;

        connect( actionRename, &QAction::triggered, this, &mainWindowMenuEnv::onRenameTexture );

        QAction *actionResize = CreateMnemonicActionL( "Main.Edit.Resize", this );
        actionResize->setShortcut( QKeySequence( Qt::ALT | Qt::Key_S ) );
        editMenu->addAction(actionResize);

        this->actionResizeTexture = actionResize;

        connect( actionResize, &QAction::triggered, this, &mainWindowMenuEnv::onResizeTexture );

        QAction *actionManipulate = CreateMnemonicActionL( "Main.Edit.Modify", this );
        actionManipulate->setShortcut( Qt::Key_M );
        editMenu->addAction(actionManipulate);

        this->actionManipulateTexture = actionManipulate;

        connect( actionManipulate, &QAction::triggered, this, &mainWindowMenuEnv::onManipulateTexture );

        QAction *actionSetupMipLevels = CreateMnemonicActionL( "Main.Edit.SetupML", this );
        actionSetupMipLevels->setShortcut( Qt::CTRL | Qt::Key_M );
        editMenu->addAction(actionSetupMipLevels);

        this->actionSetupMipmaps = actionSetupMipLevels;

        connect( actionSetupMipLevels, &QAction::triggered, this, &mainWindowMenuEnv::onSetupMipmapLayers );

        QAction *actionClearMipLevels = CreateMnemonicActionL( "Main.Edit.ClearML", this );
        actionClearMipLevels->setShortcut( Qt::CTRL | Qt::Key_C );
        editMenu->addAction(actionClearMipLevels);

        this->actionClearMipmaps = actionClearMipLevels;

        connect( actionClearMipLevels, &QAction::triggered, this, &mainWindowMenuEnv::onClearMipmapLayers );

        QAction *actionSetupRenderingProperties = CreateMnemonicActionL( "Main.Edit.SetupRP", this );
        editMenu->addAction(actionSetupRenderingProperties);

        this->actionRenderProps = actionSetupRenderingProperties;

        connect( actionSetupRenderingProperties, &QAction::triggered, this, &mainWindowMenuEnv::onSetupRenderingProps );

#ifndef _FEATURES_NOT_IN_CURRENT_RELEASE

        editMenu->addSeparator();
        QAction *actionViewAllChanges = new QAction("&View all changes", this);
        editMenu->addAction(actionViewAllChanges);

        this->actionViewAllChanges = actionViewAllChanges;

        QAction *actionCancelAllChanges = new QAction("&Cancel all changes", this);
        editMenu->addAction(actionCancelAllChanges);

        this->actionCancelAllChanges = actionCancelAllChanges;

        editMenu->addSeparator();
        QAction *actionAllTextures = new QAction("&All textures", this);
        editMenu->addAction(actionAllTextures);

        this->actionAllTextures = actionAllTextures;

#endif //_FEATURES_NOT_IN_CURRENT_RELEASE

        editMenu->addSeparator();
        QAction *actionSetupTxdVersion = CreateMnemonicActionL( "Main.Edit.SetupTV", this );
        editMenu->addAction(actionSetupTxdVersion);

        this->actionSetupTXDVersion = actionSetupTxdVersion;

        connect(actionSetupTxdVersion, &QAction::triggered, mainWnd, &MainWindow::onSetupTxdVersion);

        editMenu->addSeparator();

        QAction *actionShowOptions = CreateMnemonicActionL( "Main.Edit.Options", this );
        editMenu->addAction(actionShowOptions);

        this->actionShowOptions = actionShowOptions;

        connect(actionShowOptions, &QAction::triggered, this, &mainWindowMenuEnv::onShowOptions);

        toolsMenu = menu->addMenu("");

        QAction *actionMassConvert = CreateMnemonicActionL( "Main.Tools.MassCnv", this );
        toolsMenu->addAction(actionMassConvert);

        connect( actionMassConvert, &QAction::triggered, this, &mainWindowMenuEnv::onRequestMassConvert );

        QAction *actionMassExport = CreateMnemonicActionL( "Main.Tools.MassExp", this );
        toolsMenu->addAction(actionMassExport);

        connect( actionMassExport, &QAction::triggered, this, &mainWindowMenuEnv::onRequestMassExport );

        QAction *actionMassBuild = CreateMnemonicActionL( "Main.Tools.MassBld", this );
        toolsMenu->addAction(actionMassBuild);

        connect( actionMassBuild, &QAction::triggered, this, &mainWindowMenuEnv::onRequestMassBuild );

        exportMenu = menu->addMenu("");

        // We should check if formats are available first :)
        if ( rw::IsImagingFormatAvailable( rwEngine, "PNG" ) )
        {
            this->addTextureFormatExportLinkToMenu( exportMenu, "PNG", "PNG", "Portable Network Graphics" );
        }

        // RWTEX should always be available. Otherwise there'd be no purpose in Magic.TXD :p
        this->addTextureFormatExportLinkToMenu( exportMenu, "RWTEX", "RWTEX", "RW Texture Chunk" );

        if ( rw::IsNativeImageFormatAvailable( rwEngine, "DDS" ) )
        {
            this->addTextureFormatExportLinkToMenu( exportMenu, "DDS", "DDS", "DirectDraw Surface" );
        }

        if ( rw::IsNativeImageFormatAvailable( rwEngine, "PVR" ) )
        {
            this->addTextureFormatExportLinkToMenu( exportMenu, "PVR", "PVR", "PowerVR Image" );
        }

        if ( rw::IsImagingFormatAvailable( rwEngine, "BMP" ) )
        {
            this->addTextureFormatExportLinkToMenu( exportMenu, "BMP", "BMP", "Raw Bitmap" );
        }

        // Add remaining formats that rwlib supports.
        {
            rw::registered_image_formats_t regFormats;

            rw::GetRegisteredImageFormats( rwEngine, regFormats );

            for ( const rw::registered_image_format& theFormat : regFormats )
            {
                rw::uint32 num_ext = theFormat.num_ext;
                const rw::imaging_filename_ext *ext_array = theFormat.ext_array;

                // Decide what the most friendly name of this format is.
                // The friendly name is the longest extension available.
                const char *displayName =
                    rw::GetLongImagingFormatExtension( num_ext, ext_array );

                const char *defaultExt = nullptr;

                bool gotDefaultExt = rw::GetDefaultImagingFormatExtension( theFormat.num_ext, theFormat.ext_array, defaultExt );

                if ( gotDefaultExt && displayName != nullptr )
                {
                    if ( !StringEqualToZero( defaultExt, "PNG", false ) &&
                         !StringEqualToZero( defaultExt, "DDS", false ) &&
                         !StringEqualToZero( defaultExt, "PVR", false ) &&
                         !StringEqualToZero( defaultExt, "BMP", false ) )
                    {
                        this->addTextureFormatExportLinkToMenu( exportMenu, displayName, defaultExt, theFormat.formatName );
                    }

                    // We want to cache the available formats.
                    registered_image_format imgformat;

                    imgformat.formatName = theFormat.formatName;
                    imgformat.defaultExt = defaultExt;

                    for ( rw::uint32 n = 0; n < theFormat.num_ext; n++ )
                    {
                        imgformat.ext_array.push_back( std::string( theFormat.ext_array[ n ].ext ) );
                    }

                    imgformat.isNativeFormat = false;

                    this->reg_img_formats.push_back( std::move( imgformat ) );
                }
            }

            // Also add image formats from native texture types.
            rw::registered_image_formats_t regNatImgTypes;

            rw::GetRegisteredNativeImageTypes( rwEngine, regNatImgTypes );

            for ( const rw::registered_image_format& info : regNatImgTypes )
            {
                const char *defaultExt = nullptr;

                if ( rw::GetDefaultImagingFormatExtension( info.num_ext, info.ext_array, defaultExt ) )
                {
                    registered_image_format imgformat;

                    imgformat.formatName = info.formatName;
                    imgformat.defaultExt = defaultExt;
                    imgformat.isNativeFormat = true;

                    // Add all extensions to the array.
                    {
                        size_t extCount = info.num_ext;

                        for ( size_t n = 0; n < extCount; n++ )
                        {
                            const char *extName = info.ext_array[ n ].ext;

                            imgformat.ext_array.push_back( extName );
                        }
                    }

                    this->reg_img_formats.push_back( std::move( imgformat ) );
                }
            }
        }

#ifndef _FEATURES_NOT_IN_CURRENT_RELEASE

        QAction *actionExportTTXD = new QAction("&Text-based TXD", this);
        exportMenu->addAction(actionExportTTXD);

        this->actionsExportImage.push_back( actionExportTTXD );

#endif //_FEATURES_NOT_IN_CURRENT_RELEASE

        exportMenu->addSeparator();
        QAction *actionExportAll = CreateMnemonicActionL( "Main.Export.ExpAll", this );
        exportMenu->addAction(actionExportAll);

        this->exportAllImages = actionExportAll;

        connect( actionExportAll, &QAction::triggered, this, &mainWindowMenuEnv::onExportAllTextures );

        viewMenu = menu->addMenu("");

        QAction *actionShowFullImage = CreateMnemonicActionL( "Main.View.FullImg", this);
        // actionBackground->setShortcut(Qt::Key_F4);
        actionShowFullImage->setCheckable(true);
        viewMenu->addAction(actionShowFullImage);

        connect(actionShowFullImage, &QAction::triggered, this, &mainWindowMenuEnv::onToggleShowFullImage);

        QAction *actionBackground = CreateMnemonicActionL( "Main.View.Backgr", this);
        actionBackground->setShortcut( Qt::Key_F5 );
        actionBackground->setCheckable(true);
        viewMenu->addAction(actionBackground);

        connect(actionBackground, &QAction::triggered, this, &mainWindowMenuEnv::onToggleShowBackground);

#ifndef _FEATURES_NOT_IN_CURRENT_RELEASE

        QAction *action3dView = new QAction("&3D View", this);
        action3dView->setCheckable(true);
        viewMenu->addAction(action3dView);

#endif //_FEATURES_NOT_IN_CURRENT_RELEASE

        QAction *actionShowMipLevels = CreateMnemonicActionL( "Main.View.DispML", this );
        actionShowMipLevels->setShortcut( Qt::Key_F6 );
        actionShowMipLevels->setCheckable(true);
        viewMenu->addAction(actionShowMipLevels);

        connect( actionShowMipLevels, &QAction::triggered, this, &mainWindowMenuEnv::onToggleShowMipmapLayers );

        QAction *actionShowLog = CreateMnemonicActionL( "Main.View.ShowLog", this );
        actionShowLog->setShortcut( Qt::Key_F7 );
        viewMenu->addAction(actionShowLog);

        connect( actionShowLog, &QAction::triggered, this, &mainWindowMenuEnv::onToggleShowLog );

        viewMenu->addSeparator();

        this->actionThemeDark = CreateMnemonicActionL( "Main.View.DarkThm", this );
        this->actionThemeDark->setCheckable(true);
        this->actionThemeLight = CreateMnemonicActionL( "Main.View.LightTm", this );
        this->actionThemeLight->setCheckable(true);

        // enable needed theme in menu before connecting a slot
        actionThemeDark->setChecked(true);

        connect(this->actionThemeDark, &QAction::triggered, this, &mainWindowMenuEnv::onToogleDarkTheme);
        connect(this->actionThemeLight, &QAction::triggered, this, &mainWindowMenuEnv::onToogleLightTheme);

        viewMenu->addAction(this->actionThemeDark);
        viewMenu->addAction(this->actionThemeLight);

        actionQuit->setShortcut(QKeySequence::Quit);
        connect(actionQuit, &QAction::triggered, mainWnd, &MainWindow::close);

        infoMenu = menu->addMenu("");

        QAction *actionOpenWebsite = CreateMnemonicActionL( "Main.Info.Website", this );
        infoMenu->addAction(actionOpenWebsite);

        connect( actionOpenWebsite, &QAction::triggered, this, &mainWindowMenuEnv::onRequestOpenWebsite );

        infoMenu->addSeparator();

        QAction *actionAbout = CreateMnemonicActionL( "Main.Info.About", this );
        infoMenu->addAction(actionAbout);

        connect( actionAbout, &QAction::triggered, this, &mainWindowMenuEnv::onAboutUs );

        // Initialize the GUI.
        mainWnd->UpdateAccessibility();

        // Set our menu.
        mainWnd->contentVerticalLayout->setMenuBar( menu );

        mainWnd->RegisterThemeItem( this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        mainWnd->UnregisterThemeItem( this );
    }

    void addTextureFormatExportLinkToMenu( QMenu *theMenu, const char *displayName, const char *defaultExt, const char *formatName )
    {
        TextureExportAction *formatActionExport = new TextureExportAction( defaultExt, displayName, QString( formatName ), this );
        theMenu->addAction( formatActionExport );

        this->actionsExportItems.push_back( formatActionExport );

        // Connect it to the export signal handler.
        connect( formatActionExport, &QAction::triggered, this, &mainWindowMenuEnv::onExportTexture );
    }

    void updateTheme( MainWindow *mainWnd ) override
    {
        filePath themeName = mainWnd->getActiveTheme();

        // Check the correct tick.
        if ( themeName == L"dark" )
        {
            this->actionThemeDark->setChecked( true );
            this->actionThemeLight->setChecked( false );
        }
        else if ( themeName == L"light" )
        {
            this->actionThemeLight->setChecked( true );
            this->actionThemeDark->setChecked( false );
        }
    }

public slots:
    void onCreateNewTXD(bool checked);
    void onOpenFile(bool checked);
    void onCloseCurrent(bool checked);

    void onToggleShowFullImage(bool checked);
    void onToggleShowMipmapLayers(bool checked);
    void onToggleShowBackground(bool checked);
    void onToggleShowLog(bool checked);
    void onSetupMipmapLayers(bool checked);
    void onClearMipmapLayers(bool checked);

    void onRequestSaveTXD(bool checked);
    void onRequestSaveAsTXD(bool checked);

    void onSetupRenderingProps(bool checked);
    void onShowOptions(bool checked);

    void onRequestMassConvert(bool checked);
    void onRequestMassExport(bool checked);
    void onRequestMassBuild(bool checked);

    void onToogleDarkTheme(bool checked);
    void onToogleLightTheme(bool checked);

    void onRequestOpenWebsite(bool checked);
    void onAboutUs(bool checked);

    // Edit menu events.
    void onAddTexture(bool checked);
    void onReplaceTexture(bool checked);
    void onRemoveTexture(bool checked);
    void onRenameTexture(bool checked);
    void onResizeTexture(bool checked);
    void onManipulateTexture(bool checked);
    void onExportTexture(bool checked);
    void onExportAllTextures(bool checked);

public:
    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *toolsMenu;
    QMenu *exportMenu;
    QMenu *viewMenu;
    QMenu *infoMenu;

    // Accessibility management variables (menu items).
    // FILE MENU.
    QAction *actionNewTXD;
    QAction *actionOpenTXD;
    QAction *actionSaveTXD;
    QAction *actionSaveTXDAs;
    QAction *actionCloseTXD;

    // EDIT MENU.
    QAction *actionAddTexture;
    QAction *actionReplaceTexture;
    QAction *actionRemoveTexture;
    QAction *actionRenameTexture;
    QAction *actionResizeTexture;
    QAction *actionManipulateTexture;
    QAction *actionSetupMipmaps;
    QAction *actionClearMipmaps;
    QAction *actionRenderProps;
#ifndef _FEATURES_NOT_IN_CURRENT_RELEASE
    QAction *actionViewAllChanges;
    QAction *actionCancelAllChanges;
    QAction *actionAllTextures;
#endif //_FEATURES_NOT_IN_CURRENT_RELEASE
    QAction *actionSetupTXDVersion;
    QAction *actionShowOptions;
    QAction *actionThemeDark;
    QAction *actionThemeLight;

    bool recheckingThemeItem;

    // EXPORT MENU.
    class TextureExportAction : public QAction
    {
    public:
        TextureExportAction( QString&& defaultExt, QString&& displayName, QString&& formatName, QObject *parent ) : QAction( QString( "&" ) + displayName, parent )
        {
            this->defaultExt = defaultExt;
            this->displayName = displayName;
            this->formatName = formatName;
        }

        QString defaultExt;
        QString displayName;
        QString formatName;
    };

    std::list <TextureExportAction*> actionsExportItems;
    QAction *exportAllImages;

    // Cache of registered image formats and their interfaces.
    struct registered_image_format
    {
        std::string formatName;
        std::string defaultExt;

        std::list <std::string> ext_array;

        bool isNativeFormat;
    };

    typedef std::list <registered_image_format> imageFormats_t;

    imageFormats_t reg_img_formats;
};

static optional_struct_space <PluginDependantStructRegister <mainWindowMenuEnv, mainWindowFactory_t>> mainWindowMenuEnvRegister;

void mainWindowMenuEnv::onCreateNewTXD( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    mainWnd->ModifiedStateBarrier( false,
        [=, this]( void )
    {
        CreateTxdDialog *createDlg = new CreateTxdDialog(mainWnd);
        createDlg->setVisible(true);
    });
}

void mainWindowMenuEnv::onOpenFile( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    mainWnd->ModifiedStateBarrier( false,
        [=, this]( void )
    {
        QString fileName = QFileDialog::getOpenFileName( mainWnd, MAGIC_TEXT( "Main.Open.Desc" ), mainWnd->lastTXDOpenDir, tr( "RW Texture Archive (*.txd);;Any File (*.*)" ) );

        if ( fileName.length() != 0 )
        {
            // Store the new dir
            mainWnd->lastTXDOpenDir = QFileInfo( fileName ).absoluteDir().absolutePath();

            mainWnd->actionSystem.LaunchActionL(
                eMagicTaskType::LOAD_TXD, "General.TXDLoad",
                [fileName = std::move(fileName), mainWnd, this] ( void ) mutable
                {
                    rw::ObjectPtr loadedTXD = mainWnd->expensiveTasks.loadTxdFile( fileName );

                    if ( loadedTXD.is_good() )
                    {
                        mainWnd->actionSystem.CurrentActionSetPostL(
                            [loadedTXD = std::move(loadedTXD), fileName = std::move(fileName), mainWnd, this] ( void ) mutable
                            {
                                // Okay, we got a new TXD.
                                // Set it as our current object in the editor.
                                mainWnd->setCurrentTXD( loadedTXD.detach() );

                                mainWnd->setCurrentFilePath( fileName );

                                mainWnd->updateFriendlyIcons();
                            }
                        );
                    }
                }
            );
        }
    });
}

void mainWindowMenuEnv::onCloseCurrent( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    mainWnd->ModifiedStateBarrier( false,
        [=, this]( void )
    {
        mainWnd->actionSystem.LaunchPostOnlyActionL(
            eMagicTaskType::CLOSE_TXD, "General.TXDClose",
            [mainWnd]
            {
                // Make sure we got no TXD active.
                mainWnd->setCurrentTXD( nullptr );

                mainWnd->updateWindowTitle();

                mainWnd->updateFriendlyIcons();
            }
        );
    });
}

void mainWindowMenuEnv::onToggleShowFullImage(bool checked)
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    mainWnd->imageView->setShowFullImage( checked );
}

void mainWindowMenuEnv::onToggleShowMipmapLayers( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    mainWnd->drawMipmapLayers = !( mainWnd->drawMipmapLayers );

    // Update the texture view.
    mainWnd->updateTextureView();
}

void mainWindowMenuEnv::onToggleShowBackground(bool checked)
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    mainWnd->imageView->setShowBackground( checked );
}

void mainWindowMenuEnv::onToggleShowLog( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    // Make sure the log is visible.
    mainWnd->txdLog->show();
}

void mainWindowMenuEnv::onSetupMipmapLayers( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    // We just generate up to the top mipmap level for now.
    if ( TexInfoWidget *texInfo = mainWnd->currentSelectedTexture )
    {
        rw::ObjectPtr texture = (rw::TextureBase*)rw::AcquireObject( texInfo->GetTextureHandle() );

        // Generate mipmaps of raster.
        rw::RasterPtr texRaster = rw::AcquireRaster( texture->GetRaster() );

        if ( texRaster.is_good() )
        {
            mainWnd->actionSystem.LaunchActionL(
                eMagicTaskType::GENERATE_MIPMAPS, "General.MipGen",
                [texture = std::move(texture), texRaster = std::move(texRaster), mainWnd] ( void ) mutable
            {
                mainWnd->actionSystem.CurrentActionSetCleanupL(
                    [mainWnd, texture]
                    {
                        // Make sure we update the info.
                        mainWnd->updateTextureMetaInfo( texture );

                        // Update the texture view.
                        mainWnd->updateTextureView();

                        // We have modified the TXD.
                        mainWnd->NotifyChange();
                    }
                );

                try
                {
                    texRaster->generateMipmaps( 32, rw::MIPMAPGEN_DEFAULT );

                    // Fix texture filtering modes.
                    texture->fixFiltering();
                }
                catch( rw::RwException& except )
                {
                    mainWnd->asyncLog(
                        MAGIC_TEXT("General.MipGenFail").arg(wide_to_qt( rw::DescribeException( texRaster->getEngine(), except ) )),
                        LOGMSG_ERROR
                    );
                    throw;
                }
            });
        }
    }
}

void mainWindowMenuEnv::onClearMipmapLayers( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    // Here is a quick way to clear mipmap layers from a texture.
    if ( TexInfoWidget *texInfo = mainWnd->currentSelectedTexture )
    {
        rw::ObjectPtr texture = (rw::TextureBase*)rw::AcquireObject( texInfo->GetTextureHandle() );

        // Clear the mipmaps from the raster.
        rw::RasterPtr texRaster = rw::AcquireRaster( texture->GetRaster() );

        if ( texRaster.is_good() )
        {
            mainWnd->actionSystem.LaunchActionL(
                eMagicTaskType::CLEAR_MIPMAPS, "General.MipClear",
                [texture = std::move( texture ), texRaster = std::move( texRaster ), mainWnd] ( void ) mutable
            {
                mainWnd->actionSystem.CurrentActionSetCleanupL(
                    [mainWnd, texture]
                    {
                        // Update the info.
                        mainWnd->updateTextureMetaInfo( texture );

                        // Update the texture view.
                        mainWnd->updateTextureView();

                        // We have modified the TXD.
                        mainWnd->NotifyChange();
                    }
                );

                try
                {
                    texRaster->clearMipmaps();

                    // Fix the filtering.
                    texture->fixFiltering();
                }
                catch( rw::RwException& except )
                {
                    mainWnd->asyncLog(
                        MAGIC_TEXT("General.MipClearFail").arg(wide_to_qt( rw::DescribeException( texRaster->getEngine(), except ) )),
                        LOGMSG_ERROR
                    );
                    throw;
                }
            });
        }
    }
}

MagicActionSystem::ActionToken MainWindow::performSaveTXD(
    std::function <void ( void )> success_cb,
    std::function <void ( void )> fail_cb
)
{
    if ( this->currentTXD != nullptr )
    {
        if ( this->hasOpenedTXDFileInfo )
        {
            QString txdFullPath = this->openedTXDFileInfo.absoluteFilePath();

            if ( txdFullPath.length() != 0 )
            {
                return this->saveCurrentTXDAt( txdFullPath, std::move( success_cb ), std::move( fail_cb ) );
            }
            else
            {
                fail_cb();
            }
        }
        else
        {
            return this->performSaveAsTXD( std::move( success_cb ), std::move( fail_cb ) );
        }
    }

    // Just return an empty token.
    return MagicActionSystem::ActionToken();
}

MagicActionSystem::ActionToken MainWindow::performSaveAsTXD(
    std::function <void ( void )> success_cb,
    std::function <void ( void )> fail_cb
)
{
    if ( this->currentTXD != nullptr )
    {
        QString txdSavePath;

        if (!(this->lastTXDSaveDir.isEmpty()) && this->currentTXD) {
            if (this->hasOpenedTXDFileInfo)
                txdSavePath = this->lastTXDSaveDir + "/" + this->openedTXDFileInfo.fileName();
            else
                txdSavePath = this->lastTXDSaveDir + "/" + this->newTxdName;
        }

        QString newSaveLocation = QFileDialog::getSaveFileName( this, MAGIC_TEXT("Main.SaveAs.Desc"), txdSavePath, "RW Texture Dictionary (*.txd)" );

        if ( newSaveLocation.length() != 0 )
        {
            // Save location.
            this->lastTXDSaveDir = QFileInfo( newSaveLocation ).absoluteDir().absolutePath();

            return this->saveCurrentTXDAt( newSaveLocation, std::move( success_cb ), std::move( fail_cb ) );
        }
        else
        {
            fail_cb();
        }
    }
    else
    {
        fail_cb();
    }

    // Return empty token.
    return MagicActionSystem::ActionToken();
}

void mainWindowMenuEnv::onRequestSaveTXD( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    mainWnd->performSaveTXD( []{}, []{} );
}

void mainWindowMenuEnv::onRequestSaveAsTXD( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    mainWnd->performSaveAsTXD( []{}, []{} );
}

QString MainWindow::requestValidImagePath( const QString *imageName )
{
    static const char *MULTI_ITEM_SEPERATOR = " ";

    mainWindowMenuEnv *menuEnv = mainWindowMenuEnvRegister.get().GetPluginStruct( this );

    // Get the name of a texture to add.
    // For that we want to construct a list of all possible image extensions.
    QString imgExtensionSelect;

    bool hasEntry = false;

    const mainWindowMenuEnv::imageFormats_t& avail_formats = menuEnv->reg_img_formats;

    // Add any image file.
    if ( hasEntry )
    {
        imgExtensionSelect += ";;";
    }

    imgExtensionSelect += "Image file (";

    bool hasExtEntry = false;

    for ( const mainWindowMenuEnv::registered_image_format& entry : avail_formats )
    {
        if ( hasExtEntry )
        {
            imgExtensionSelect += MULTI_ITEM_SEPERATOR;
        }

        bool needsExtSep = false;

        for ( const std::string& extName : entry.ext_array )
        {
            if ( needsExtSep )
            {
                imgExtensionSelect += MULTI_ITEM_SEPERATOR;
            }

            imgExtensionSelect += QString( "*." ) + ansi_to_qt( extName ).toLower();

            needsExtSep = true;
        }

        hasExtEntry = true;
    }

    // TEX CHUNK.
    {
        if ( hasExtEntry )
        {
            imgExtensionSelect += MULTI_ITEM_SEPERATOR;
        }

        imgExtensionSelect += QString( "*.rwtex" );
    }

    imgExtensionSelect += ")";

    hasEntry = true;

    for ( const mainWindowMenuEnv::registered_image_format& entry : avail_formats )
    {
        if ( hasEntry )
        {
            imgExtensionSelect += ";;";
        }

        imgExtensionSelect += ansi_to_qt( entry.formatName ) + QString( " (" );

        bool needsExtSep = false;

        for ( const std::string& extName : entry.ext_array )
        {
            if ( needsExtSep )
            {
                imgExtensionSelect += MULTI_ITEM_SEPERATOR;
            }

            imgExtensionSelect +=
                QString( "*." ) + ansi_to_qt( extName ).toLower();

            needsExtSep = true;
        }

        imgExtensionSelect += QString( ")" );

        hasEntry = true;
    }

    // Add any file.
    if ( hasEntry )
    {
        imgExtensionSelect += ";;";
    }

    imgExtensionSelect += "RW Texture Chunk (*.rwtex);;Any file (*.*)";

    hasEntry = true;

    // As a convenience feature the imageName parameter could be given so that we
    // check if in the currently selected directory a file of that name exists, under a known image extension.
    QString actualImageFileOpenPath = this->lastImageFileOpenDir;

    if ( imageName )
    {
        QString maybeImagePath = actualImageFileOpenPath;
        maybeImagePath += '/';
        maybeImagePath += *imageName;

        bool hasFoundKnownFile = false;
        {
            // We just check using image extensions we know and pick the first one we find.
            // Might improve this in the future by actually picking what image format the user likes the most first.
            for ( const mainWindowMenuEnv::registered_image_format& entry : avail_formats )
            {
                for ( const std::string& ext_name : entry.ext_array )
                {
                    // Check for this image file availability.
                    QString pathToImageFile = maybeImagePath + '.' + ansi_to_qt( ext_name ).toLower();

                    QFileInfo fileInfo( pathToImageFile );

                    if ( fileInfo.exists() && fileInfo.isFile() )
                    {
                        // Just pick this one.
                        actualImageFileOpenPath = std::move( pathToImageFile );

                        hasFoundKnownFile = true;
                        break;
                    }
                }

                // Check for .rwtex, too.
                if ( !hasFoundKnownFile )
                {
                    QString pathToImageFile = maybeImagePath + ".rwtex";

                    QFileInfo fileInfo( pathToImageFile );

                    if ( fileInfo.exists() && fileInfo.isFile() )
                    {
                        // We found a native texture, so pick that one.
                        actualImageFileOpenPath = std::move( pathToImageFile );

                        hasFoundKnownFile = true;
                    }
                }
            }
        }

        if ( !hasFoundKnownFile )
        {
            actualImageFileOpenPath = std::move( maybeImagePath );
        }
    }

    QString imagePath = QFileDialog::getOpenFileName( this, MAGIC_TEXT("Main.Edit.Add.Desc"), actualImageFileOpenPath, imgExtensionSelect );

    // Remember the directory.
    if ( imagePath.length() != 0 )
    {
        this->lastImageFileOpenDir = QFileInfo( imagePath ).absoluteDir().absolutePath();
    }

    return imagePath;
}

void mainWindowMenuEnv::onAddTexture( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    // Allow importing of a texture.
    rw::TexDictionary *currentTXD = mainWnd->currentTXD;

    if ( currentTXD != nullptr )
    {
        QString fileName = mainWnd->requestValidImagePath();

        if ( fileName.length() != 0 )
        {
            mainWnd->spawnTextureAddDialog( fileName );
        }
    }
}

void mainWindowMenuEnv::onReplaceTexture( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    // Replacing a texture means that we search for another texture on disc.
    // We prompt the user to input a replacement that has exactly the same texture properties
    // (name, addressing mode, etc) but different raster properties (maybe).

    // We need to have a texture selected to replace.
    if ( TexInfoWidget *curSelTexItem = mainWnd->currentSelectedTexture )
    {
        rw::ObjectPtr toBeReplaced = (rw::TextureBase*)rw::AcquireObject( curSelTexItem->GetTextureHandle() );

        QString overwriteTexName = ansi_to_qt( curSelTexItem->GetTextureHandle()->GetName() );

        QString replaceImagePath = mainWnd->requestValidImagePath( &overwriteTexName );

        if ( replaceImagePath.length() != 0 )
        {
            auto cb_lambda = [mainWnd, toBeReplaced = std::move(toBeReplaced)] ( const TexAddDialog::texAddOperation& params ) mutable
            {
                TexInfoWidget *texInfo = mainWnd->getTextureTexInfoLink( toBeReplaced );

                // If we have no more selected texture item then we bail.
                if ( texInfo == nullptr )
                    return;

                rw::Interface *rwEngine = mainWnd->GetEngine();

                // Replace stuff.
                TexAddDialog::texAddOperation::eAdditionType add_type = params.add_type;

                rw::TextureBase *updateTex = nullptr;

                if ( add_type == TexAddDialog::texAddOperation::ADD_TEXCHUNK )
                {
                    // We just take the texture and replace our existing texture with it.
                    // First remove the reference that was cast when adding it to the TexInfoWidget.
                    {
                        // Also remove the link because the texture could be deleted at a must later stage.
                        mainWnd->setTextureTexInfoLink( toBeReplaced, nullptr );

                        texInfo->SetTextureHandle( nullptr );

                        rwEngine->DeleteRwObject( toBeReplaced );
                    }

                    // Can now get rid of the texture.
                    toBeReplaced = nullptr;

                    rw::TextureBase *newTex = (rw::TextureBase*)rw::AcquireObject( params.add_texture.texHandle );

                    if ( newTex )
                    {
                        texInfo->SetTextureHandle( newTex );

                        // Make sure we link things together properly.
                        mainWnd->setTextureTexInfoLink( newTex, texInfo );

                        // Add the new texture to the dictionary.
                        newTex->AddToDictionary( mainWnd->currentTXD );

                        updateTex = newTex;
                    }
                }
                else if ( add_type == TexAddDialog::texAddOperation::ADD_RASTER )
                {
                    // We keep the texture but just assign a new raster to it.
                    TexAddDialog::RwTextureAssignNewRaster(
                        toBeReplaced, params.add_raster.raster,
                        params.add_raster.texName, params.add_raster.maskName
                    );

                    updateTex = toBeReplaced;
                }

                if ( updateTex )
                {
                    // Update info.
                    mainWnd->updateTextureMetaInfo( updateTex );
                }

                mainWnd->updateTextureView();

                // We have modified the TXD.
                mainWnd->NotifyChange();
            };

            TexAddDialog::dialogCreateParams params;
            params.actionName = "Modify.Replace";
            params.actionDesc = "Modify.Desc.Replace";
            params.type = TexAddDialog::CREATE_IMGPATH;
            params.img_path.imgPath = replaceImagePath;

            // Overwrite some properties.
            params.overwriteTexName = &overwriteTexName;

            TexAddDialog *texAddTask = new TexAddDialog( mainWnd, params, std::move( cb_lambda ) );

            texAddTask->move( 200, 250 );
            texAddTask->setVisible( true );
        }
    }
}

void mainWindowMenuEnv::onRemoveTexture( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    if ( TexInfoWidget *curSelTexItem = mainWnd->currentSelectedTexture )
    {
        rw::ObjectPtr toBeRemoved = (rw::TextureBase*)rw::AcquireObject( curSelTexItem->GetTextureHandle() );

        // Pretty simple. We get rid of the currently selected texture item.
        mainWnd->actionSystem.LaunchPostOnlyActionL(
            eMagicTaskType::DELETE_TEXTURE, "General.TexRemove",
            [mainWnd, toBeRemoved = std::move(toBeRemoved)] ( void ) mutable
            {
                if ( TexInfoWidget *texInfo = mainWnd->getTextureTexInfoLink( toBeRemoved ) )
                {
                    // If we have the current item selected then forget about this.
                    if ( mainWnd->currentSelectedTexture == texInfo )
                    {
                        mainWnd->currentSelectedTexture = nullptr;
                    }

                    // First delete this item from the list.
                    texInfo->remove();
                    mainWnd->setTextureTexInfoLink( toBeRemoved, nullptr );

                    // Now kill the texture.
                    mainWnd->rwEngine->DeleteRwObject( toBeRemoved );

                    // If we have no more items in the list widget, we should hide our texture view page.
                    if ( mainWnd->textureListWidget->selectedItems().count() == 0 )
                    {
                        mainWnd->clearViewImage();

                        // We should also hide the friendly icons, since they only should show if a texture is selected.
                        // this->hideFriendlyIcons();
                    }

                    // We have modified the TXD.
                    mainWnd->NotifyChange();
                }
            }
        );
    }
}

void mainWindowMenuEnv::onRenameTexture( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    // Change the name of the currently selected texture.

    if ( mainWnd->texNameDlg )
        return;

    if ( TexInfoWidget *texInfo = mainWnd->currentSelectedTexture )
    {
        rw::ObjectPtr toBeRenamed = (rw::TextureBase*)rw::AcquireObject( texInfo->GetTextureHandle() );

        TexNameWindow *texNameDlg = new TexNameWindow( mainWnd, std::move( toBeRenamed ) );

        texNameDlg->setVisible( true );
    }
}

void mainWindowMenuEnv::onResizeTexture( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    // Change the texture dimensions.

    if ( TexInfoWidget *texInfo = mainWnd->currentSelectedTexture )
    {
        if ( TexResizeWindow *curDlg = mainWnd->resizeDlg )
        {
            curDlg->setFocus();
        }
        else
        {
            rw::ObjectPtr toBeResized = (rw::TextureBase*)rw::AcquireObject( texInfo->GetTextureHandle() );

            TexResizeWindow *dialog = new TexResizeWindow( mainWnd, std::move( toBeResized ) );
            dialog->setVisible( true );
        }
    }
}

void mainWindowMenuEnv::onManipulateTexture( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    // Manipulating a raster is taking that raster and creating a new copy that is more beautiful.
    // We can easily reuse the texture add dialog for this task.

    // For that we need a selected texture.
    if ( TexInfoWidget *curSelTexItem = mainWnd->currentSelectedTexture )
    {
        rw::ObjectPtr toBeManipulated = (rw::TextureBase*)rw::AcquireObject( curSelTexItem->GetTextureHandle() );

        auto cb_lambda = [mainWnd, toBeManipulated = std::move(toBeManipulated)] ( const TexAddDialog::texAddOperation& params ) mutable
        {
            assert( params.add_type == TexAddDialog::texAddOperation::ADD_RASTER );

            // Update the stored raster.
            TexAddDialog::RwTextureAssignNewRaster(
                toBeManipulated, params.add_raster.raster,
                params.add_raster.texName, params.add_raster.maskName
            );

            // We have changed the TXD.
            mainWnd->NotifyChange();

            // Update info.
            if ( TexInfoWidget *texInfo = mainWnd->getTextureTexInfoLink( toBeManipulated ) )
            {
                texInfo->updateInfo();

                mainWnd->UpdateExportAccessibility();
            }

            mainWnd->updateTextureView();
        };

        TexAddDialog::dialogCreateParams params;
        params.actionName = "Modify.Modify";
        params.actionDesc = "Modify.Desc.Modify";
        params.type = TexAddDialog::CREATE_RASTER;
        params.orig_raster.tex = curSelTexItem->GetTextureHandle();

        TexAddDialog *texAddTask = new TexAddDialog( mainWnd, params, std::move( cb_lambda ) );

        texAddTask->move( 200, 250 );
        texAddTask->setVisible( true );
    }
}

static void serializeRaster( rw::Stream *outputStream, rw::Raster *texRaster, const char *method )
{
    // TODO: add DDS file writer functionality, by checking method for "DDS"

    // Serialize it.
    texRaster->writeImage( outputStream, method );
}

void mainWindowMenuEnv::onExportTexture( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    // We are always sent by a QAction object.
    TextureExportAction *senderAction = (TextureExportAction*)this->sender();

    // Make sure we have selected a texture in the texture list.
    // Fetch it.
    TexInfoWidget *selectedTexture = mainWnd->currentSelectedTexture;

    if ( selectedTexture != nullptr )
    {
        rw::ObjectPtr texHandle = (rw::TextureBase*)rw::AcquireObject( selectedTexture->GetTextureHandle() );

        if ( texHandle.is_good() )
        {
            const QString& defaultExt = senderAction->defaultExt;
            const QString& exportFunction = senderAction->displayName;
            const QString& formatName = senderAction->formatName;

            rw::rwStaticString <char> ansiExportFunction = qt_to_ansirw( exportFunction );

            const QString actualExt = defaultExt.toLower();

            // Construct a default filename for the object.
            QString defaultFileName = ansi_to_qt( texHandle->GetName() ) + "." + actualExt;

            // Request a filename and do the export.
            QString caption;
            bool found = false;
            QString captionFormat = MAGIC_TEXT_CHECK_AVAILABLE("Main.Export.Desc", &found);

            if ( found )
                caption = QString(captionFormat).arg(exportFunction);
            else
                caption = QString("Save ") + exportFunction + QString(" as...");


            QString finalFilePath = QFileDialog::getSaveFileName(mainWnd, caption, defaultFileName, formatName + " (*." + actualExt + ");;Any (*.*)");

            if ( finalFilePath.length() != 0 )
            {
                mainWnd->actionSystem.LaunchActionL(
                    eMagicTaskType::EXPORT_SINGLE, "General.ExportTex",
                    [mainWnd, ansiExportFunction = std::move(ansiExportFunction), finalFilePath = std::move(finalFilePath), texHandle = std::move(texHandle)]
                    {
                        rw::Interface *rwEngine = mainWnd->GetEngine();

                        try
                        {
                            // Try to open that file for writing.
                            rw::rwStaticString <wchar_t> unicodeImagePath = qt_to_widerw( finalFilePath );

                            rw::streamConstructionFileParamW_t fileParam( unicodeImagePath.GetConstString() );

                            rw::StreamPtr imageStream = rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_CREATE, &fileParam );

                            if ( imageStream.is_good() )
                            {
                                try
                                {
                                    // Directly write us.
                                    if ( StringEqualToZero( ansiExportFunction.GetConstString(), "RWTEX", false ) )
                                    {
                                        rwEngine->Serialize( texHandle, imageStream );
                                    }
                                    else
                                    {
                                        rw::Raster *texRaster = texHandle->GetRaster();

                                        if ( texRaster )
                                        {
                                            serializeRaster( imageStream, texRaster, ansiExportFunction.GetConstString() );
                                        }
                                    }
                                }
                                catch( ... )
                                {
                                    // Release the lock on the file.
                                    imageStream = nullptr;

                                    // Since we failed, we do not want that image stream anymore.
                                    fileRoot->Delete( unicodeImagePath.GetConstString(), filesysPathOperatingMode::FILE_ONLY );

                                    throw;
                                }
                            }
                        }
                        catch( rw::RwException& except )
                        {
                            mainWnd->txdLog->showError( MAGIC_TEXT("General.ExportTexFail").arg(wide_to_qt( rw::DescribeException( rwEngine, except ) )) );
                            throw;
                        }
                    }
                );
            }
        }
    }
}

void mainWindowMenuEnv::onExportAllTextures( bool checked )
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    rw::ObjectPtr texDict = (rw::TexDictionary*)rw::AcquireObject( mainWnd->currentTXD );

    if ( texDict.is_good() )
    {
        // No point in exporting empty TXD.
        if ( texDict->GetTextureCount() != 0 )
        {
            ExportAllWindow *curDlg = new ExportAllWindow( mainWnd, texDict );

            curDlg->setVisible( true );
        }
    }
}

void mainWindowMenuEnv::onToogleDarkTheme(bool checked)
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    if (checked && !this->recheckingThemeItem) {
        mainWnd->setActiveTheme(L"dark");
    }
    else {
        this->recheckingThemeItem = true;
        this->actionThemeDark->setChecked(true);
        this->recheckingThemeItem = false;
    }
}

void mainWindowMenuEnv::onToogleLightTheme(bool checked)
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    if (checked && !this->recheckingThemeItem) {
        mainWnd->setActiveTheme(L"light");
    }
    else {
        this->recheckingThemeItem = true;
        this->actionThemeLight->setChecked(true);
        this->recheckingThemeItem = false;
    }
}

void mainWindowMenuEnv::onSetupRenderingProps( bool checked )
{
    if ( checked == true )
        return;

    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    if ( TexInfoWidget *texInfo = mainWnd->currentSelectedTexture )
    {
        if ( RenderPropWindow *curDlg = mainWnd->renderPropDlg )
        {
            curDlg->setFocus();
        }
        else
        {
            rw::ObjectPtr toBeEdited = (rw::TextureBase*)rw::AcquireObject( texInfo->GetTextureHandle() );

            RenderPropWindow *dialog = new RenderPropWindow( mainWnd, std::move( toBeEdited ) );
            dialog->setVisible( true );
        }
    }
}

void MainWindow::onSetupTxdVersion(bool checked)
{
    if ( checked == true )
        return;

    if ( RwVersionDialog *curDlg = this->verDlg )
    {
        curDlg->setFocus();
    }
    else
    {
        RwVersionDialog *dialog = new RwVersionDialog( this );

        dialog->setVisible( true );

        this->verDlg = dialog;
    }

    this->verDlg->updateVersionConfig();
}

void mainWindowMenuEnv::onShowOptions(bool checked)
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    if ( QDialog *curDlg = mainWnd->optionsDlg )
    {
        curDlg->setFocus();
    }
    else
    {
        OptionsDialog *optionsDlg = new OptionsDialog( mainWnd );

        optionsDlg->setVisible( true );
    }
}

void mainWindowMenuEnv::onRequestMassConvert(bool checked)
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    MassConvertWindow *massconv = new MassConvertWindow( mainWnd );

    massconv->setVisible( true );
}

void mainWindowMenuEnv::onRequestMassExport(bool checked)
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    MassExportWindow *massexport = new MassExportWindow( mainWnd );

    massexport->setVisible( true );
}

void mainWindowMenuEnv::onRequestMassBuild(bool checked)
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    MassBuildWindow *massbuild = new MassBuildWindow( mainWnd );

    massbuild->setVisible( true );

    TriggerHelperWidget( mainWnd, "mgbld_welcome", massbuild );
}

void mainWindowMenuEnv::onRequestOpenWebsite(bool checked)
{
    QDesktopServices::openUrl( QUrl( "http://www.gtamodding.com/wiki/Magic.TXD" ) );
}

void mainWindowMenuEnv::onAboutUs(bool checked)
{
    MainWindow *mainWnd = mainWindowMenuEnvRegister.get().GetBackResolve( this );

    if ( QDialog *curDlg = mainWnd->aboutDlg )
    {
        curDlg->setFocus();
    }
    else
    {
        AboutDialog *aboutDlg = new AboutDialog( mainWnd );

        aboutDlg->setVisible( true );
    }
}

#define FONT_SIZE_MENU_PX 26

unsigned int MainWindow::updateMenuContent( void )
{
    mainWindowMenuEnv *menuEnv = mainWindowMenuEnvRegister.get().GetPluginStruct( this );

    unsigned int menuLineWidth = 0;

    QString sFileMenu = MAGIC_TEXT("Main.File");
    menuLineWidth += GetTextWidthInPixels(sFileMenu, FONT_SIZE_MENU_PX);

    menuEnv->fileMenu->setTitle( "&" + sFileMenu );

    QString sEditMenu = MAGIC_TEXT("Main.Edit");
    menuLineWidth += GetTextWidthInPixels(sEditMenu, FONT_SIZE_MENU_PX);

    menuEnv->editMenu->setTitle( "&" + sEditMenu );

    QString sToolsMenu = MAGIC_TEXT("Main.Tools");
    menuLineWidth += GetTextWidthInPixels(sToolsMenu, FONT_SIZE_MENU_PX);

    menuEnv->toolsMenu->setTitle( "&" + sToolsMenu );

    QString sExportMenu = MAGIC_TEXT("Main.Export");
    menuLineWidth += GetTextWidthInPixels(sExportMenu, FONT_SIZE_MENU_PX);

    menuEnv->exportMenu->setTitle( sExportMenu );

    QString sViewMenu = MAGIC_TEXT("Main.View");
    menuLineWidth += GetTextWidthInPixels(sViewMenu, FONT_SIZE_MENU_PX);

    menuEnv->viewMenu->setTitle( sViewMenu );

    QString sInfoMenu = MAGIC_TEXT("Main.Info");
    menuLineWidth += GetTextWidthInPixels(sInfoMenu, FONT_SIZE_MENU_PX);

    menuEnv->infoMenu->setTitle( sInfoMenu );

    menuLineWidth += 240; // space between menu items ( 5 * 40 ) + 20 + 20
    menuLineWidth += 100;  // buttons size

    return menuLineWidth;
}

void MainWindow::UpdateExportAccessibility( void )
{
    mainWindowMenuEnv *menuEnv = mainWindowMenuEnvRegister.get().GetPluginStruct( this );

    // Export options are available depending on what texture has been selected.
    bool has_txd = ( this->currentTXD != nullptr );

    for ( mainWindowMenuEnv::TextureExportAction *exportAction : menuEnv->actionsExportItems )
    {
        bool shouldEnable = has_txd;

        if ( shouldEnable )
        {
            // We should only enable if the currently selected texture actually supports us.
            bool hasSupport = false;

            if ( !hasSupport )
            {
                try
                {
                    if ( TexInfoWidget *curSelTex = this->currentSelectedTexture )
                    {
                        if ( rw::Raster *texRaster = curSelTex->GetTextureHandle()->GetRaster() )
                        {
                            std::string ansiMethodName = qt_to_ansi( exportAction->displayName );

                            if ( StringEqualToZero( ansiMethodName.c_str(), "RWTEX", false ) )
                            {
                                hasSupport = true;
                            }
                            else
                            {
                                hasSupport = texRaster->supportsImageMethod( ansiMethodName.c_str() );
                            }
                        }
                    }
                }
                catch( rw::RwException& )
                {
                    // If we failed to request support capability, we just abort.
                    hasSupport = false;
                }
            }

            if ( !hasSupport )
            {
                // No texture item selected means we cannot export anyway.
                shouldEnable = false;
            }
        }

        exportAction->setDisabled( !shouldEnable );
    }

    menuEnv->exportAllImages->setDisabled( !has_txd );
}

void MainWindow::UpdateAccessibility( void )
{
    mainWindowMenuEnv *menuEnv = mainWindowMenuEnvRegister.get().GetPluginStruct( this );

    // If we have no TXD available, we should not allow the user to pick TXD related options.
    bool has_txd = ( this->currentTXD != nullptr );

    menuEnv->actionSaveTXD->setDisabled( !has_txd );
    menuEnv->actionSaveTXDAs->setDisabled( !has_txd );
    menuEnv->actionCloseTXD->setDisabled( !has_txd );
    menuEnv->actionAddTexture->setDisabled( !has_txd );
    menuEnv->actionReplaceTexture->setDisabled( !has_txd );
    menuEnv->actionRemoveTexture->setDisabled( !has_txd );
    menuEnv->actionRenameTexture->setDisabled( !has_txd );
    menuEnv->actionResizeTexture->setDisabled( !has_txd );
    menuEnv->actionManipulateTexture->setDisabled( !has_txd );
    menuEnv->actionSetupMipmaps->setDisabled( !has_txd );
    menuEnv->actionClearMipmaps->setDisabled( !has_txd );
    menuEnv->actionRenderProps->setDisabled( !has_txd );
#ifndef _FEATURES_NOT_IN_CURRENT_RELEASE
    menuEnv->actionViewAllChanges->setDisabled( !has_txd );
    menuEnv->actionCancelAllChanges->setDisabled( !has_txd );
    menuEnv->actionAllTextures->setDisabled( !has_txd );
#endif //_FEATURES_NOT_IN_CURRENT_RELEASE
    menuEnv->actionSetupTXDVersion->setDisabled( !has_txd );

    this->UpdateExportAccessibility();
}

void MainWindow::launchDetails( void )
{
    mainWindowMenuEnv *menuEnv = mainWindowMenuEnvRegister.get().GetPluginStruct( this );

    if ( this->isLaunchedForTheFirstTime )
    {
        // We should make ourselves known.
        menuEnv->onAboutUs( false );
    }
}

// Module initialization and shutdown.
void InitializeMainWindowMenu( void )
{
    mainWindowMenuEnvRegister.Construct( mainWindowFactory );
}

void ShutdownMainWindowMenu( void )
{
    mainWindowMenuEnvRegister.Destroy();
}
