/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.cpp
*  PURPOSE:     Central editor window implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include "texinfoitem.h"
#include <QtWidgets/QCommonStyle>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/qsplitter.h>
#include <QtGui/qmovie.h>
#include <QtWidgets/QFileDialog>
#include <QtCore/QDir>
#include <QtGui/QDesktopServices>
#include <QtGui/qdrag.h>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QDropEvent>
#include <QtCore/qmimedata.h>
#include <QtWidgets/QStatusBar>

#include "styles.h"
#include "rwversiondialog.h"
#include "texnamewindow.h"
#include "renderpropwindow.h"
#include "resizewindow.h"
#include "massconvert.h"
#include "exportallwindow.h"
#include "massexport.h"
#include "massbuild.h"
#include "optionsdialog.h"
#include "createtxddlg.h"
#include "texadddialog.h"
#include "languages.h"
//#include "platformselwindow.h"

#include "tools/txdgen.h"
#include "tools/imagepipe.hxx"

#include "qtrwutils.hxx"

#define MAIN_MIN_WIDTH 700
#define MAIN_WIDTH 800
#define MAIN_MIN_HEIGHT 300
#define MAIN_HEIGHT 560

MainWindow::MainWindow(QString appPath, rw::Interface *engineInterface, CFileSystem *fsHandle, QWidget *parent) :
    QMainWindow(parent),
    rwWarnMan( this ),
    rwEngine( engineInterface ),
    actionSystem( this ),
    taskSystem( (NativeExecutive::CExecutiveManager*)rw::GetThreadingNativeManager( engineInterface ) )
{
    m_appPath = appPath;
    m_appPathForStyleSheet = appPath;
    m_appPathForStyleSheet.replace('\\', '/');
    // Initialize variables.
    this->currentTXD = nullptr;
    this->txdNameLabel = nullptr;
    this->currentSelectedTexture = nullptr;
    this->txdLog = nullptr;
    this->verDlg = nullptr;

    this->texNameDlg = nullptr;
    this->renderPropDlg = nullptr;
    this->resizeDlg = nullptr;
    //this->platformDlg = nullptr;
    this->aboutDlg = nullptr;
    this->optionsDlg = nullptr;
    this->rwVersionButton = nullptr;

    this->recommendedTxdPlatform = "Direct3D9";

    // Initialize configuration to default.
    {
        this->lastTXDOpenDir = QDir::current().absolutePath();
        this->lastTXDSaveDir = this->lastTXDOpenDir;
        this->lastImageFileOpenDir = this->makeAppPath( "" );
        this->lastLanguageFileName = "";
        this->addImageGenMipmaps = true;
        this->lockDownTXDPlatform = true;
        this->adjustTextureChunksOnImport = true;
        this->texaddViewportFill = false;
        this->texaddViewportScaled = true;
        this->texaddViewportBackground = false;
        this->isLaunchedForTheFirstTime = true;
        this->showLogOnWarning = true;
        this->showGameIcon = true;
        this->lastUsedAllExportFormat = "PNG";
        this->lastAllExportTarget = qt_to_widerw( this->makeAppPath( "" ) );
    }

    this->wasTXDModified = false;

    this->drawMipmapLayers = false;

    this->hasOpenedTXDFileInfo = false;

    // Set-up the warning manager.
    this->rwEngine->SetWarningManager( &this->rwWarnMan );

    this->fileSystem = fsHandle;

    this->tempFileRoot = fileSystem->GenerateTempRepository();

    try
    {
        /* --- Window --- */
        updateWindowTitle();
        //setMinimumSize(620, 300);
        //resize(800, 600);

        // We do support drag and drop.
        this->setAcceptDrops( true );

        SetupWindowSize(this, MAIN_WIDTH, MAIN_HEIGHT, MAIN_MIN_WIDTH, MAIN_MIN_HEIGHT);

        /* --- Log --- */
        this->txdLog = new TxdLog(this, this->m_appPath, this);

        /* --- List --- */
        QListWidget *listWidget = new QListWidget();
        listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        //listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        listWidget->setMaximumWidth(350);
        //listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

        connect( listWidget, &QListWidget::currentItemChanged, this, &MainWindow::onTextureItemChanged );

        // We will store all our texture names in this.
        this->textureListWidget = listWidget;

        /* --- Viewport --- */
        imageView = new TexViewportWidget(this);
        imageView->setFrameShape(QFrame::NoFrame);
        imageView->setObjectName("textureViewBackground");

        /* --- Splitter --- */
        mainSplitter = new QSplitter;
        mainSplitter->addWidget(listWidget);
        mainSplitter->addWidget(imageView);
        QList<int> sizes;
        sizes.push_back(200);
        sizes.push_back(mainSplitter->size().width() - 200);
        mainSplitter->setSizes(sizes);
        mainSplitter->setChildrenCollapsible(false);

        /* --- Top panel --- */
        QWidget *txdNameBackground = new QWidget();
        txdNameBackground->setFixedHeight(60);
        txdNameBackground->setObjectName("background_0");
        QLabel *txdName = new QLabel();
        txdName->setObjectName("label36px");
        txdName->setAlignment(Qt::AlignCenter);

        this->txdNameLabel = txdName;

        QGridLayout *txdNameLayout = new QGridLayout();
        QLabel *starsBox = new QLabel;
        starsMovie = new QMovie;

        // set default theme movie
        starsMovie->setFileName(makeAppPath("resources/dark/stars.gif"));
        starsBox->setMovie(starsMovie);
        starsMovie->start();
        txdNameLayout->addWidget(starsBox, 0, 0);
        txdNameLayout->addWidget(txdName, 0, 0);
        txdNameLayout->setContentsMargins(0, 0, 0, 0);
        txdNameLayout->setMargin(0);
        txdNameLayout->setSpacing(0);
        txdNameBackground->setLayout(txdNameLayout);

        QWidget *txdOptionsBackground = new QWidget();
        txdOptionsBackground->setFixedHeight(54);
        txdOptionsBackground->setObjectName("background_1");

        QHBoxLayout *hlayout = new QHBoxLayout;
        txdOptionsBackground->setLayout(hlayout);

        this->contentVerticalLayout = hlayout;

        // Layout for rw version, with right-side alignment
        QHBoxLayout *rwVerLayout = new QHBoxLayout;
        rwVersionButton = new QPushButton;
        rwVersionButton->setObjectName("rwVersionButton");
        rwVersionButton->setMaximumWidth(100);
        rwVersionButton->hide();
        rwVerLayout->addWidget(rwVersionButton);
        rwVerLayout->setAlignment(Qt::AlignRight);

        connect( rwVersionButton, &QPushButton::clicked, this, &MainWindow::onSetupTxdVersion );

        // Layout to mix menu and rw version label/button
        QGridLayout *menuVerLayout = new QGridLayout();
        menuVerLayout->addWidget(txdOptionsBackground, 0, 0);
        menuVerLayout->addLayout(rwVerLayout, 0, 0, Qt::AlignRight);
        menuVerLayout->setContentsMargins(0, 0, 0, 0);
        menuVerLayout->setMargin(0);
        menuVerLayout->setSpacing(0);

        QWidget *hLineBackground = new QWidget();
        hLineBackground->setFixedHeight(1);
        hLineBackground->setObjectName("hLineBackground");

        QVBoxLayout *topLayout = new QVBoxLayout;
        topLayout->addWidget(txdNameBackground);
        topLayout->addLayout(menuVerLayout);
        topLayout->addWidget(hLineBackground);
        topLayout->setContentsMargins(0, 0, 0, 0);
        topLayout->setMargin(0);
        topLayout->setSpacing(0);

        /* --- Bottom panel --- */
        QWidget *hLineBackground2 = new QWidget;
        hLineBackground2->setFixedHeight(1);
        hLineBackground2->setObjectName("hLineBackground");
        QWidget *txdOptionsBackground2 = new QWidget;
        txdOptionsBackground2->setFixedHeight(59);
        txdOptionsBackground2->setObjectName("background_1");

        /* --- Friendly Icons --- */
        QHBoxLayout *friendlyIconRow = new QHBoxLayout();
        friendlyIconRow->setContentsMargins( 0, 0, 15, 0 );
        friendlyIconRow->setAlignment( Qt::AlignRight | Qt::AlignVCenter );

        this->friendlyIconRow = friendlyIconRow;

        QLabel *friendlyIconGame = new QLabel();
        friendlyIconGame->setObjectName("label25px_dim");
        friendlyIconGame->setVisible( false );

        this->friendlyIconGame = friendlyIconGame;

        friendlyIconRow->addWidget( friendlyIconGame );

        QWidget *friendlyIconSeparator = new QWidget();
        friendlyIconSeparator->setFixedWidth(1);
        friendlyIconSeparator->setObjectName( "friendlyIconSeparator" );
        friendlyIconSeparator->setVisible( false );

        this->friendlyIconSeparator = friendlyIconSeparator;

        friendlyIconRow->addWidget( friendlyIconSeparator );

        QLabel *friendlyIconPlatform = new QLabel();
        friendlyIconPlatform->setObjectName("label25px_dim");
        friendlyIconPlatform->setVisible( false );

        this->friendlyIconPlatform = friendlyIconPlatform;

        friendlyIconRow->addWidget( friendlyIconPlatform );

        txdOptionsBackground2->setLayout( friendlyIconRow );

        QVBoxLayout *bottomLayout = new QVBoxLayout;
        bottomLayout->addWidget(hLineBackground2);
        bottomLayout->addWidget(txdOptionsBackground2);
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setMargin(0);
        bottomLayout->setSpacing(0);

        // Add a status bar.
        QStatusBar *statusBar = new QStatusBar;
        statusBar->setSizeGripEnabled( false );

        this->setStatusBar( statusBar );

        /* --- Main layout --- */
        QVBoxLayout *mainLayout = new QVBoxLayout;
        mainLayout->addLayout(topLayout);
        mainLayout->addWidget(mainSplitter);
        mainLayout->addLayout(bottomLayout);

        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setMargin(0);
        mainLayout->setSpacing(0);

        QWidget *window = new QWidget();
        window->setLayout(mainLayout);

        window->setObjectName("background_0");
        setObjectName("background_0");

        setCentralWidget(window);

        // Read data files

        this->versionSets.readSetsFile(this->makeAppPath("data/versionsets.dat"));

        //

        RegisterTextLocalizationItem( this );
    }
    catch( ... )
    {
        rwEngine->SetWarningManager( nullptr );

        throw;
    }
}

template <typename objType>
inline void SafeDelete( objType *ptr )
{
    if ( ptr )
    {
        delete ptr;
    }
}

MainWindow::~MainWindow()
{
    UnregisterTextLocalizationItem( this );

    // If we have a loaded TXD, get rid of it.
    if ( this->currentTXD )
    {
        this->rwEngine->DeleteRwObject( this->currentTXD );

        this->currentTXD = nullptr;
    }

    // DELETE ALL SUB DIALOGS THAT DEPEND ON MAINWINDOW HERE.

    SafeDelete( txdLog );
    SafeDelete( verDlg );
    SafeDelete( texNameDlg );
    SafeDelete( renderPropDlg );
    SafeDelete( resizeDlg );
    //SafeDelete( platformDlg );
    SafeDelete( aboutDlg );
    SafeDelete( optionsDlg );

    // Kill any sub windows.
    // Less dangerous than killing by language context.
    {
        QObjectList children = this->children();

        for ( QObject *obj : children )
        {
            if ( QDialog *subDlg = dynamic_cast <QDialog*> ( obj ) )
            {
                // Kills off any remnants that could depend on the main window.
                delete subDlg;
            }
        }
    }

    // Cannot do this, because it is VERY dangerous.
#if 0
    // Kill off localization items that are dialogs and not the main window.
    {
        localizations_t culturalItems = GetTextLocalizationItems();

        for ( magicTextLocalizationItem *locale_obj : culturalItems )
        {
            if ( locale_obj != this )
            {
                if ( QDialog *dlg = dynamic_cast <QDialog*> ( locale_obj ) )
                {
                    delete dlg;
                }
            }
        }
    }
#endif

    // Remove the warning manager again.
    this->rwEngine->SetWarningManager( nullptr );

    // Delete our temporary repository.
    fileSystem->DeleteTempRepository( this->tempFileRoot );
}

void MainWindow::updateContent( MainWindow *_this )
{
    unsigned int menuLineWidth = updateMenuContent();

    RecalculateWindowSize(this, menuLineWidth, MAIN_MIN_WIDTH, MAIN_MIN_HEIGHT);
}

// We want to have some help tokens in the main window too.
struct mainWindowHelpEnv
{
    inline void Initialize( MainWindow *mainWnd )
    {
        RegisterHelperWidget( mainWnd, "mgbld_welcome", eHelperTextType::DIALOG_WITH_TICK, "Tools.MassBld.Welcome", true );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        UnregisterHelperWidget( mainWnd, "mgbld_welcome" );
    }
};

static optional_struct_space <PluginDependantStructRegister <mainWindowHelpEnv, mainWindowFactory_t>> mainWindowHelpEnvRegister;

void InitializeMainWindowHelpEnv( void )
{
    mainWindowHelpEnvRegister.Construct( mainWindowFactory );
}

void ShutdownMainWindowHelpEnv( void )
{
    mainWindowHelpEnvRegister.Destroy();
}

// I propose to seperate the TXD actions from this source file into a seperate file.
// Point is that the actions are entirely unrelated to Qt, just RenderWare. They
// are required to run entirely asynchronous. They need to trigger events back to
// the GUI, possibly inside RenderWare itself.
// But of course this is a very difficult task.

void MainWindow::setCurrentTXD( rw::TexDictionary *txdObj )
{
    if ( this->currentTXD == txdObj )
        return;

    if ( this->currentTXD != nullptr )
    {
        // We have no more opened TXD file, if any.
        this->hasOpenedTXDFileInfo = false;

        // Make sure we have no more texture in our viewport.
        clearViewImage();

        // Since we have no selected texture, we can hide the friendly icons.
        // this->hideFriendlyIcons();

        this->currentSelectedTexture = nullptr;

        this->rwEngine->DeleteRwObject( this->currentTXD );

        this->currentTXD = nullptr;

        this->ClearModifiedState();

        // Clear anything in the GUI that represented the previous TXD.
        this->textureListWidget->clear();
    }

    if ( txdObj != nullptr )
    {
        this->currentTXD = txdObj;

        this->updateTextureList(false);
    }

    // We should update how we let the user access the GUI.
    this->UpdateAccessibility();
}

void MainWindow::updateTextureList( bool selectLastItemInList )
{
    rw::TexDictionary *txdObj = this->currentTXD;

    QListWidget *listWidget = this->textureListWidget;

    listWidget->clear();

    // We have no more selected texture item.
    this->currentSelectedTexture = nullptr;

    // this->hideFriendlyIcons();

    if ( txdObj )
    {
        TexInfoWidget *texInfoToSelect = nullptr;

        for ( rw::TexDictionary::texIter_t iter( txdObj->GetTextureIterator() ); iter.IsEnd() == false; iter.Increment() )
        {
            rw::TextureBase *texItem = iter.Resolve();

            QListWidgetItem *item = new QListWidgetItem;
            listWidget->addItem(item);
            TexInfoWidget *texInfoWidget = new TexInfoWidget(item, texItem);
            listWidget->setItemWidget(item, texInfoWidget);
            item->setSizeHint(QSize(listWidget->sizeHintForColumn(0), 54));

            // Link things together properly.
            this->setTextureTexInfoLink( texItem, texInfoWidget );

            // select first or last item in a list
            if (!texInfoToSelect || selectLastItemInList)
                texInfoToSelect = texInfoWidget;
        }

        if (texInfoToSelect)
            listWidget->setCurrentItem(texInfoToSelect->listItem);
    }
}

void MainWindow::updateWindowTitle( void )
{
    QString windowTitleString;

    if ( this->wasTXDModified )
    {
        windowTitleString += "* ";
    }

    windowTitleString += "Magic.TXD";

#if defined(_M_AMD64) || defined(__LP64__)
    windowTitleString += " x64";
#endif

#ifdef _DEBUG
    windowTitleString += " DEBUG";
#endif

    // Put a little version info.
    windowTitleString += " " MTXD_VERSION_STRING;

#ifdef MGTXD_BETA_BUILD
    windowTitleString += " Beta";
#endif //MGTXD_BETA_BUILD

    // If we are using a legacy OS, put that into the title.
    if ( this->fileSystem->IsInLegacyMode() )
    {
        windowTitleString += " (legacy)";
    }

#ifdef MGTXD_PERSONALIZED_DETAILS
    windowTitleString += " " MGTXD_PERSONALIZED_DETAILS;
#endif //MGTXD_PERSONALIED_DETAILS

    if ( this->currentTXD != nullptr )
    {
        if ( this->hasOpenedTXDFileInfo )
        {
            windowTitleString += " (" + QString( this->openedTXDFileInfo.absoluteFilePath() ) + ")";
        }
    }

    setWindowTitle( windowTitleString );

    // Also update the top label.
    if (this->txdNameLabel)
    {
        if ( this->currentTXD != nullptr )
        {
            QString topLabelDisplayString;

            if ( this->hasOpenedTXDFileInfo )
            {
                topLabelDisplayString = this->openedTXDFileInfo.fileName();
            }
            else
            {
                topLabelDisplayString = this->newTxdName;
            }

            this->txdNameLabel->setText( topLabelDisplayString );
        }
        else
        {
            this->txdNameLabel->clear();
        }
    }

    // Update version button
    if (this->rwVersionButton)
    {
        if (rw::TexDictionary *txd = this->currentTXD)
        {
            const rw::LibraryVersion& txdVersion = txd->GetEngineVersion();

            QString text =
                QString::number(txdVersion.rwLibMajor) + "." +
                QString::number(txdVersion.rwLibMinor) + "." +
                QString::number(txdVersion.rwRevMajor) + "." +
                QString::number(txdVersion.rwRevMinor);
            this->rwVersionButton->setText(text);
            this->rwVersionButton->show();
        }
        else
        {
            this->rwVersionButton->hide();
        }
    }
}

void MainWindow::updateTextureMetaInfo( rw::TextureBase *theTexture )
{
    if ( TexInfoWidget *infoWidget = this->getTextureTexInfoLink( theTexture ) )
    {
        // Update it.
        infoWidget->updateInfo();

        // We also want to update the exportability, as the format may have changed.
        this->UpdateExportAccessibility();
    }
}

void MainWindow::updateAllTextureMetaInfo( void )
{
    QListWidget *textureList = this->textureListWidget;

    int rowCount = textureList->count();

    for ( int row = 0; row < rowCount; row++ )
    {
        QListWidgetItem *item = textureList->item( row );

        TexInfoWidget *texInfo = dynamic_cast <TexInfoWidget*> ( textureList->itemWidget( item ) );

        if ( texInfo )
        {
            texInfo->updateInfo();
        }
    }

    // Make sure we update exportability.
    this->UpdateExportAccessibility();
}

void MainWindow::onTextureItemChanged(QListWidgetItem *listItem, QListWidgetItem *prevTexInfoItem)
{
    QListWidget *texListWidget = this->textureListWidget;

    QWidget *listItemWidget = texListWidget->itemWidget( listItem );

    TexInfoWidget *texItem = dynamic_cast <TexInfoWidget*> ( listItemWidget );

    this->currentSelectedTexture = texItem;

    this->updateTextureView();

    // Change what textures we can export to.
    this->UpdateExportAccessibility();
}

void MainWindow::adjustDimensionsByViewport( void )
{
    // If opening a TXD file, the editor window can be too small to view the entire image.
    // We should carefully increase the editor size so that everything is visible.

    // TODO.
}

void MainWindow::DefaultTextureAddAndPrepare( rw::TextureBase *newTexture, const char *name, const char *maskName )
{
    // We need to set default texture rendering properties.
    newTexture->SetFilterMode( rw::RWFILTER_LINEAR );
    newTexture->SetUAddressing( rw::RWTEXADDRESS_WRAP );
    newTexture->SetVAddressing( rw::RWTEXADDRESS_WRAP );

    // Actually adjust filtering based on its raster.
    newTexture->fixFiltering();

    // Give it a name.
    newTexture->SetName( name );
    newTexture->SetMaskName( maskName );

    // Now put it into the TXD.
    newTexture->AddToDictionary( currentTXD );

    // Update the texture list.
    this->updateTextureList(true);

    // We have modified the TXD.
    this->NotifyChange();
}

void MainWindow::spawnTextureAddDialog( QString fileName )
{
    auto cb_lambda = [=, this] ( const TexAddDialog::texAddOperation& params )
    {
        TexAddDialog::texAddOperation::eAdditionType add_type = params.add_type;

        bool hadEmptyTXD = ( this->currentTXD->GetTextureCount() == 0 );

        if ( add_type == TexAddDialog::texAddOperation::ADD_TEXCHUNK )
        {
            // This is just adding the texture chunk to our TXD.
            rw::TextureBase *texHandle = (rw::TextureBase*)rw::AcquireObject( params.add_texture.texHandle );

            texHandle->AddToDictionary( this->currentTXD );

            // Update the texture list.
            this->updateTextureList(true);

            this->NotifyChange();
        }
        else if ( add_type == TexAddDialog::texAddOperation::ADD_RASTER )
        {
            rw::Raster *newRaster = params.add_raster.raster;

            if ( newRaster )
            {
                try
                {
                    // We want to create a texture and put it into our TXD.
                    rw::TextureBase *newTexture = rw::CreateTexture( this->rwEngine, newRaster );

                    if ( newTexture )
                    {
                        try
                        {
                            DefaultTextureAddAndPrepare(
                                newTexture,
                                params.add_raster.texName.GetConstString(),
                                params.add_raster.maskName.GetConstString()
                            );
                        }
                        catch( ... )
                        {
                            // We kinda should get rid of this texture.
                            this->rwEngine->DeleteRwObject( newTexture );

                            throw;
                        }
                    }
                    else
                    {
                        this->txdLog->showError( MAGIC_TEXT("General.TexCreateFail") );
                    }
                }
                catch( rw::RwException& except )
                {
                    this->txdLog->showError( MAGIC_TEXT("General.TexAddFail").arg(wide_to_qt( rw::DescribeException( this->rwEngine, except ) )) );

                    // Just continue.
                }
            }
        }

        // Update the friendly icons, since if TXD was empty, platform was set by giving first texture to TXD.
        if ( hadEmptyTXD )
        {
            this->updateFriendlyIcons();
        }
    };

    TexAddDialog::dialogCreateParams params;
    params.actionName = "Modify.Add";
    params.actionDesc = "Modify.Desc.Add";
    params.type = TexAddDialog::CREATE_IMGPATH;
    params.img_path.imgPath = fileName;

    TexAddDialog *texAddTask = new TexAddDialog( this, params, std::move( cb_lambda ) );

    //texAddTask->move( 200, 250 );
    texAddTask->setVisible( true );
}

void MainWindow::clearViewImage()
{
    // Disable any pending loading animation.
    tasktoken_updatePreview.Cancel( true );

    this->imageView->clearContent();
    this->imageView->setViewportMode( eViewportMode::DISABLED );
}

void MainWindow::NotifyChange( void )
{
    // Call this function if there has been a change in the currently open TXD.
    if ( this->currentTXD == nullptr )
        return;

    bool isTXDChanged = this->wasTXDModified;

    if ( isTXDChanged )
    {
        // No need to notify twice.
        return;
    }

    this->wasTXDModified = true;

    // Update the editor title.
    this->updateWindowTitle();
}

void MainWindow::ClearModifiedState( void )
{
    // Call this function if the change is not important anymore.
    bool isTXDChanged = this->wasTXDModified;

    if ( !isTXDChanged )
        return;

    this->wasTXDModified = false;

    // Time to remove hints that the TXD was changed.
    this->updateWindowTitle();
}

// Set txd plaform when we open
// or create new txd
// Creating a txd with different platform textures doesn't make any sense.

QString MainWindow::GetCurrentPlatform()
{
    // Attempt to get the platform from the current TXD, if present.
    if ( rw::TexDictionary *currentTXD = this->currentTXD )
    {
        if ( const char *txdPlatName = this->GetTXDPlatform( currentTXD ) )
        {
            return txdPlatName;
        }
    }

    // If we cannot get it from the actual TXD, the user's choice is just as important.
    return this->recommendedTxdPlatform;
}

void MainWindow::SetRecommendedPlatform(QString platform)
{
    // Please use this function only to set the user's preference.
    // User selects something in GUI that should be honored by the editor.
    this->recommendedTxdPlatform = platform;
}

const char* MainWindow::GetTXDPlatform(rw::TexDictionary *txd)
{
    if (txd->GetTextureCount() > 0) {
        for (rw::TexDictionary::texIter_t iter(txd->GetTextureIterator()); !iter.IsEnd(); iter.Increment())
        {
            rw::TextureBase *texHandle = iter.Resolve();

            rw::Raster *texRaster = texHandle->GetRaster();

            if (texRaster)
            {
                return texRaster->getNativeDataTypeName();
            }
        }
    }
    return nullptr;
}

QString MainWindow::makeAppPath(QString subPath)
{
    return m_appPath + "/" + subPath;
}
