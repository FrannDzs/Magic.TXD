/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/massconvert.cpp
*  PURPOSE:     UI for TxdGen.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "massconvert.h"

#include "qtinteroputils.hxx"

#include <QtCore/QCoreApplication>

#include "qtsharedlogic.h"

#include "guiserialization.hxx"

#include <sdk/PluginHelpers.h>

#include "toolshared.hxx"

#include "tools/txdgen.h"

#include "qtutils.h"
#include "languages.h"

// TODO for all tools:
// Make relative paths relate to app folder

using namespace toolshare;

struct massconvEnv : public magicSerializationProvider
{
    inline void Initialize( MainWindow *mainwnd )
    {
        LIST_CLEAR( this->openDialogs.root );

        RegisterMainWindowSerialization( mainwnd, MAGICSERIALIZE_MASSCONV, this );
    }

    void Shutdown( MainWindow *mainwnd )
    {
        UnregisterMainWindowSerialization( mainwnd, MAGICSERIALIZE_MASSCONV );

        // Make sure all open dialogs are closed.
        while ( !LIST_EMPTY( openDialogs.root ) )
        {
            MassConvertWindow *wnd = LIST_GETITEM( MassConvertWindow, openDialogs.root.next, node );

            delete wnd;
        }
    }

    struct txdgen_cfg_struct
    {
        endian::little_endian <rwkind::eTargetGame> c_gameType;
        endian::little_endian <rwkind::eTargetPlatform> c_targetPlatform;

        bool c_clearMipmaps;
        bool c_generateMipmaps;

        endian::little_endian <rw::eMipmapGenerationMode> c_mipGenMode;
        endian::little_endian <rw::uint32> c_mipGenMaxLevel;

        bool c_improveFiltering;
        bool compressTextures;

        endian::little_endian <rw::ePaletteRuntimeType> c_palRuntimeType;
        endian::little_endian <rw::eDXTCompressionMethod> c_dxtRuntimeType;

        bool c_reconstructIMGArchives;
        bool c_fixIncompatibleRasters;
        bool c_dxtPackedDecompression;
        bool c_imgArchivesCompressed;
        bool c_ignoreSerializationRegions;

        endian::little_endian <rw::float32> c_compressionQuality;

        bool c_outputDebug;

        endian::little_endian <rw::int32> c_warningLevel;

        bool c_ignoreSecureWarnings;
    };

    void Load( MainWindow *mainWnd, rw::BlockProvider& massconvBlock ) override
    {
        this->txdgenConfig.c_gameRoot = RwReadUnicodeString( massconvBlock );
        this->txdgenConfig.c_outputRoot = RwReadUnicodeString( massconvBlock );

        txdgen_cfg_struct cfgStruct;
        massconvBlock.readStruct( cfgStruct );

        txdgenConfig.c_gameType = cfgStruct.c_gameType;
        txdgenConfig.c_targetPlatform = cfgStruct.c_targetPlatform;
        txdgenConfig.c_clearMipmaps = cfgStruct.c_clearMipmaps;
        txdgenConfig.c_generateMipmaps = cfgStruct.c_generateMipmaps;
        txdgenConfig.c_mipGenMode = cfgStruct.c_mipGenMode;
        txdgenConfig.c_mipGenMaxLevel = cfgStruct.c_mipGenMaxLevel;
        txdgenConfig.c_improveFiltering = cfgStruct.c_improveFiltering;
        txdgenConfig.compressTextures = cfgStruct.compressTextures;
        txdgenConfig.c_palRuntimeType = cfgStruct.c_palRuntimeType;
        txdgenConfig.c_dxtRuntimeType = cfgStruct.c_dxtRuntimeType;
        txdgenConfig.c_reconstructIMGArchives = cfgStruct.c_reconstructIMGArchives;
        txdgenConfig.c_fixIncompatibleRasters = cfgStruct.c_fixIncompatibleRasters;
        txdgenConfig.c_dxtPackedDecompression = cfgStruct.c_dxtPackedDecompression;
        txdgenConfig.c_imgArchivesCompressed = cfgStruct.c_imgArchivesCompressed;
        txdgenConfig.c_ignoreSerializationRegions = cfgStruct.c_ignoreSerializationRegions;
        txdgenConfig.c_compressionQuality = cfgStruct.c_compressionQuality;
        txdgenConfig.c_outputDebug = cfgStruct.c_outputDebug;
        txdgenConfig.c_warningLevel = cfgStruct.c_warningLevel;
        txdgenConfig.c_ignoreSecureWarnings = cfgStruct.c_ignoreSecureWarnings;
    }

    void Save( const MainWindow *mainWnd, rw::BlockProvider& massconvEnvBlock ) const override
    {
        RwWriteUnicodeString( massconvEnvBlock, this->txdgenConfig.c_gameRoot );
        RwWriteUnicodeString( massconvEnvBlock, this->txdgenConfig.c_outputRoot );

        txdgen_cfg_struct cfgStruct;
        cfgStruct.c_gameType = this->txdgenConfig.c_gameType;
        cfgStruct.c_targetPlatform = this->txdgenConfig.c_targetPlatform;
        cfgStruct.c_clearMipmaps = this->txdgenConfig.c_clearMipmaps;
        cfgStruct.c_generateMipmaps = this->txdgenConfig.c_generateMipmaps;
        cfgStruct.c_mipGenMode = this->txdgenConfig.c_mipGenMode;
        cfgStruct.c_mipGenMaxLevel = this->txdgenConfig.c_mipGenMaxLevel;
        cfgStruct.c_improveFiltering = this->txdgenConfig.c_improveFiltering;
        cfgStruct.compressTextures = this->txdgenConfig.compressTextures;
        cfgStruct.c_palRuntimeType = this->txdgenConfig.c_palRuntimeType;
        cfgStruct.c_dxtRuntimeType = this->txdgenConfig.c_dxtRuntimeType;
        cfgStruct.c_reconstructIMGArchives = this->txdgenConfig.c_reconstructIMGArchives;
        cfgStruct.c_fixIncompatibleRasters = this->txdgenConfig.c_fixIncompatibleRasters;
        cfgStruct.c_dxtPackedDecompression = this->txdgenConfig.c_dxtPackedDecompression;
        cfgStruct.c_imgArchivesCompressed = this->txdgenConfig.c_imgArchivesCompressed;
        cfgStruct.c_ignoreSerializationRegions = this->txdgenConfig.c_ignoreSerializationRegions;
        cfgStruct.c_compressionQuality = this->txdgenConfig.c_compressionQuality;
        cfgStruct.c_outputDebug = this->txdgenConfig.c_outputDebug;
        cfgStruct.c_warningLevel = this->txdgenConfig.c_warningLevel;
        cfgStruct.c_ignoreSecureWarnings = this->txdgenConfig.c_ignoreSecureWarnings;

        massconvEnvBlock.writeStruct( cfgStruct );
    }

    TxdGenModule::run_config txdgenConfig;

    RwList <MassConvertWindow> openDialogs;
};

typedef PluginDependantStructRegister <massconvEnv, mainWindowFactory_t> massconvEnvRegister_t;

static optional_struct_space <massconvEnvRegister_t> massconvEnvRegister;

void InitializeMassconvToolEnvironment( void )
{
    massconvEnvRegister.Construct( mainWindowFactory );
}

void ShutdownMassconvToolEnvironment( void )
{
    massconvEnvRegister.Destroy();
}

MassConvertWindow::MassConvertWindow( MainWindow *mainwnd ) : QDialog( mainwnd ), logEditControl( this )
{
    massconvEnv *massconv = massconvEnvRegister.get().GetPluginStruct( mainwnd );

    this->mainwnd = mainwnd;

    this->setAttribute( Qt::WA_DeleteOnClose );

    this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    // Display lots of stuff.
    MagicLayout<QHBoxLayout> layout(this);

    QVBoxLayout *leftPanelLayout = new QVBoxLayout();

    layout.top->addLayout(leftPanelLayout);

    leftPanelLayout->addLayout(
        qtshared::createGameRootInputOutputForm(
            massconv->txdgenConfig.c_gameRoot,
            massconv->txdgenConfig.c_outputRoot,
            this->editGameRoot,
            this->editOutputRoot
        )
    );

    createTargetConfigurationComponents(
        leftPanelLayout,
        massconv->txdgenConfig.c_targetPlatform,
        massconv->txdgenConfig.c_gameType,
        this->selGameBox,
        this->selPlatformBox
    );

    // INVASION OF CHECKBOXES.
    QCheckBox *propClearMipmaps = CreateCheckBoxL( "Tools.MassCnv.ClearML" );

    propClearMipmaps->setChecked( massconv->txdgenConfig.c_clearMipmaps );

    this->propClearMipmaps = propClearMipmaps;

    leftPanelLayout->addWidget( propClearMipmaps );

    leftPanelLayout->addLayout(
        qtshared::createMipmapGenerationGroup(
            this,
            massconv->txdgenConfig.c_generateMipmaps,
            massconv->txdgenConfig.c_mipGenMaxLevel,
            this->propGenMipmaps,
            this->propGenMipmapsMax
        )
    );

    QCheckBox *propImproveFiltering = CreateCheckBoxL( "Tools.MassCnv.ImpFilt" );

    propImproveFiltering->setChecked( massconv->txdgenConfig.c_improveFiltering );

    this->propImproveFiltering = propImproveFiltering;

    leftPanelLayout->addWidget( propImproveFiltering );

    QCheckBox *propCompress = CreateCheckBoxL( "Tools.MassCnv.CompTex" );

    propCompress->setChecked( massconv->txdgenConfig.compressTextures );

    this->propCompressTextures = propCompress;

    leftPanelLayout->addWidget( propCompress );

    QCheckBox *propReconstructIMG = CreateCheckBoxL( "Tools.MassCnv.RecIMG" );

    propReconstructIMG->setChecked( massconv->txdgenConfig.c_reconstructIMGArchives );

    this->propReconstructIMG = propReconstructIMG;

    leftPanelLayout->addWidget( propReconstructIMG );

    QCheckBox *propCompressedIMG = CreateCheckBoxL( "Tools.MassCnv.CompIMG" );

    propCompressedIMG->setChecked( massconv->txdgenConfig.c_imgArchivesCompressed );

    this->propCompressedIMG = propCompressedIMG;

    leftPanelLayout->addWidget( propCompressedIMG );

    // Add a log.
    layout.top->addWidget( logEditControl.CreateLogWidget() );

    rw::Interface *rwEngine = mainwnd->GetEngine();

    this->convConsistencyLock = rw::CreateReadWriteLock( rwEngine );

    this->conversionThread = NULL;

    // buttons at the bottom
    QPushButton *buttonConvert = CreateButtonL("Tools.MassCnv.Convert");

    this->buttonConvert = buttonConvert;

    connect(buttonConvert, &QPushButton::clicked, this, &MassConvertWindow::OnRequestConvert);

    layout.bottom->addWidget(buttonConvert, 0, Qt::AlignCenter);

    QPushButton *buttonCancel = CreateButtonL("Tools.MassCnv.Cancel");

    connect(buttonCancel, &QPushButton::clicked, this, &MassConvertWindow::OnRequestCancel);

    layout.bottom->addWidget(buttonCancel, 0, Qt::AlignCenter);

    RegisterTextLocalizationItem( this );

    LIST_INSERT( massconv->openDialogs.root, this->node );
}

MassConvertWindow::~MassConvertWindow( void )
{
    LIST_REMOVE( this->node );

    UnregisterTextLocalizationItem( this );

    // Make sure we finished the thread.
    rw::Interface *rwEngine = this->mainwnd->GetEngine();

    bool hasLocked = true;

    this->convConsistencyLock->enter_read();

    if ( rw::thread_t convThread = this->conversionThread )
    {
        convThread = rw::AcquireThread( rwEngine, convThread );

        this->convConsistencyLock->leave_read();

        hasLocked = false;

        rw::TerminateThread( rwEngine, convThread );

        rw::CloseThread( rwEngine, convThread );
    }

    if ( hasLocked )
    {
        this->convConsistencyLock->leave_read();
    }

    // Kill the lock.
    rw::CloseReadWriteLock( rwEngine, this->convConsistencyLock );

    this->serialize();
}

void MassConvertWindow::updateContent( MainWindow *mainWnd )
{
    this->setWindowTitle( MAGIC_TEXT( "Tools.MassCnv.Desc" ) );
}

void MassConvertWindow::serialize( void )
{
    // We should serialize ourselves.
    massconvEnv *massconv = massconvEnvRegister.get().GetPluginStruct( this->mainwnd );

    // game version.
    gameToNaturalList.getCurrent( this->selGameBox, massconv->txdgenConfig.c_gameType );

    massconv->txdgenConfig.c_outputRoot = qt_to_widerw( this->editOutputRoot->text() );
    massconv->txdgenConfig.c_gameRoot = qt_to_widerw( this->editGameRoot->text() );

    platformToNaturalList.getCurrent( this->selPlatformBox, massconv->txdgenConfig.c_targetPlatform );

    massconv->txdgenConfig.c_clearMipmaps = this->propClearMipmaps->isChecked();
    massconv->txdgenConfig.c_generateMipmaps = this->propGenMipmaps->isChecked();
    massconv->txdgenConfig.c_mipGenMaxLevel = this->propGenMipmapsMax->text().toInt();
    massconv->txdgenConfig.c_improveFiltering = this->propImproveFiltering->isChecked();
    massconv->txdgenConfig.compressTextures = this->propCompressTextures->isChecked();
    massconv->txdgenConfig.c_reconstructIMGArchives = this->propReconstructIMG->isChecked();
    massconv->txdgenConfig.c_imgArchivesCompressed = this->propCompressedIMG->isChecked();
}

struct ConversionFinishEvent : public QEvent
{
    inline ConversionFinishEvent( void ) : QEvent( QEvent::User )
    {}
};

void MassConvertWindow::postLogMessage( QString msg )
{
    logEditControl.postLogMessage( std::move( msg ) );
}

struct MassConvertTxdGenModule : public TxdGenModule
{
    MassConvertWindow *massconvWnd;

    inline MassConvertTxdGenModule( MassConvertWindow *massconvWnd, rw::Interface *rwEngine ) : TxdGenModule( rwEngine )
    {
        this->massconvWnd = massconvWnd;
    }

    void OnMessage( const rw::rwStaticString <wchar_t>& msg ) override
    {
        massconvWnd->postLogMessage( wide_to_qt( msg ) );
    }

    rw::rwStaticString <wchar_t> TOKEN( const char *token ) override
    {
        return qt_to_widerw( MAGIC_TEXT( token ) );
    }

    CFile* WrapStreamCodec( CFile *compressed ) override
    {
        return CreateDecompressedStream( massconvWnd->mainwnd, compressed );
    }
};

static void convThreadEntryPoint( rw::thread_t threadHandle, rw::Interface *engineInterface, void *ud )
{
    MassConvertWindow *massconvWnd = (MassConvertWindow*)ud;

    // Get our configuration.
    TxdGenModule::run_config run_cfg;

    if ( massconvEnv *massconv = massconvEnvRegister.get().GetPluginStruct( massconvWnd->mainwnd ) )
    {
        run_cfg = massconv->txdgenConfig;
    }

    // Any RenderWare configuration that we set on this thread should count for this thread only.
    rw::AssignThreadedRuntimeConfig( engineInterface );

    try
    {
        massconvWnd->postLogMessage( MAGIC_TEXT("Tools.MassCnv.StartConv") + "\n\n" );

        MassConvertTxdGenModule module( massconvWnd, engineInterface );

        module.ApplicationMain( run_cfg );

        // Notify the application that we finished.
        {
            ConversionFinishEvent *evt = new ConversionFinishEvent();

            QCoreApplication::postEvent( massconvWnd, evt );
        }

        massconvWnd->postLogMessage( MAGIC_TEXT("Tools.MassCnv.EndConv") + "\n\n" );
    }
    catch( ... )
    {
        // We have to quit normally.
        massconvWnd->postLogMessage( MAGIC_TEXT("Tools.MassCnv.Term") + "\n" );
    }

    massconvWnd->convConsistencyLock->enter_write();

    massconvWnd->conversionThread = nullptr;

    // Close our handle.
    rw::CloseThread( engineInterface, threadHandle );

    massconvWnd->convConsistencyLock->leave_write();
}

void MassConvertWindow::OnRequestConvert( bool checked )
{
    if ( this->conversionThread )
        return;

    // Update configuration.
    this->serialize();

    // Disable the conversion button, since we cannot run two conversions at the same time in
    // the same window.
    this->buttonConvert->setDisabled( true );

    // Run some the conversion in a seperate thread.
    rw::Interface *rwEngine = this->mainwnd->GetEngine();

    rw::thread_t convThread = rw::MakeThread( rwEngine, convThreadEntryPoint, this );

    this->conversionThread = convThread;

    rw::ResumeThread( rwEngine, convThread );
}

void MassConvertWindow::OnRequestCancel( bool checked )
{
    this->close();
}

void MassConvertWindow::customEvent( QEvent *evt )
{
    // Handle events for the log.
    logEditControl.customEvent( evt );

    if ( ConversionFinishEvent *convEndEvt = dynamic_cast <ConversionFinishEvent*> ( evt ) )
    {
        (void)convEndEvt;

        // We can enable the conversion button again.
        this->buttonConvert->setDisabled( false );

        return;
    }

    QDialog::customEvent( evt );
}
