/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/massbuild.cpp
*  PURPOSE:     UI for the mass builder tool.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include "massbuild.h"

#include "taskcompletionwindow.h"

#include "guiserialization.hxx"

#include "qtsharedlogic.h"

#include "toolshared.hxx"

#include "tools/txdbuild.h"

#include "qtutils.h"
#include "languages.h"

#include <sstream>

using namespace toolshare;

struct massbuildEnv : public magicSerializationProvider
{
    inline void Initialize( MainWindow *mainWnd )
    {
        LIST_CLEAR( this->windows.root );

        RegisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_MASSBUILD, this );

        // Initialize with some defaults.
        this->closeOnCompletion = true;
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        UnregisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_MASSBUILD );

        // Close all windows.
        while ( !LIST_EMPTY( this->windows.root ) )
        {
            MassBuildWindow *wnd = LIST_GETITEM( MassBuildWindow, this->windows.root.next, node );

            delete wnd;
        }
    }

    enum class eSerializedPaletteType
    {
        PAL4,
        PAL8
    };

    struct massbuild_cfg_struct
    {
        endian::little_endian <rwkind::eTargetGame> targetGame;
        endian::little_endian <rwkind::eTargetPlatform> targetPlatform;
        bool generateMipmaps;
        endian::little_endian <std::int32_t> genMipMaxLevel;
        bool closeOnCompletion;
        bool doCompress;
        endian::little_endian <float> compressionQuality;
        bool doPalettize;
        endian::little_endian <eSerializedPaletteType> paletteType;
    };

    void Load( MainWindow *mainWnd, rw::BlockProvider& cfgBlock ) override
    {
        // Load our state.
        this->config.gameRoot = RwReadUnicodeString( cfgBlock );
        this->config.outputRoot = RwReadUnicodeString( cfgBlock );

        massbuild_cfg_struct cfgStruct;
        cfgBlock.readStruct( cfgStruct );

        this->config.targetGame = cfgStruct.targetGame;
        this->config.targetPlatform = cfgStruct.targetPlatform;
        this->config.generateMipmaps = cfgStruct.generateMipmaps;
        this->config.curMipMaxLevel = cfgStruct.genMipMaxLevel;
        this->closeOnCompletion = cfgStruct.closeOnCompletion;

        // Compression.
        {
            this->config.doCompress = cfgStruct.doCompress;
            this->config.compressionQuality = cfgStruct.compressionQuality;
        }

        // Palettization.
        {
            this->config.doPalettize = cfgStruct.doPalettize;

            rw::ePaletteType runtimePaletteType = rw::PALETTE_8BIT;
            eSerializedPaletteType serPaletteType = cfgStruct.paletteType;

            if ( serPaletteType == eSerializedPaletteType::PAL4 )
            {
                runtimePaletteType = rw::PALETTE_4BIT;
            }
            else if ( serPaletteType == eSerializedPaletteType::PAL8 )
            {
                runtimePaletteType = rw::PALETTE_8BIT;
            }

            this->config.paletteType = runtimePaletteType;
        }
    }

    void Save( const MainWindow *mainWnd, rw::BlockProvider& cfgBlock ) const override
    {
        // Save our state.
        RwWriteUnicodeString( cfgBlock, this->config.gameRoot );
        RwWriteUnicodeString( cfgBlock, this->config.outputRoot );

        massbuild_cfg_struct cfgStruct;
        cfgStruct.targetGame = this->config.targetGame;
        cfgStruct.targetPlatform = this->config.targetPlatform;
        cfgStruct.generateMipmaps = this->config.generateMipmaps;
        cfgStruct.genMipMaxLevel = this->config.curMipMaxLevel;
        cfgStruct.closeOnCompletion = this->closeOnCompletion;

        // Compression.
        {
            cfgStruct.doCompress = this->config.doCompress;
            cfgStruct.compressionQuality = this->config.compressionQuality;
        }

        // Palettization.
        {
            cfgStruct.doPalettize = this->config.doPalettize;

            eSerializedPaletteType serPaletteType = eSerializedPaletteType::PAL8;
            rw::ePaletteType runtimePaletteType = this->config.paletteType;

            if ( runtimePaletteType == rw::PALETTE_4BIT || runtimePaletteType == rw::PALETTE_4BIT_LSB )
            {
                serPaletteType = eSerializedPaletteType::PAL4;
            }
            else if ( runtimePaletteType == rw::PALETTE_8BIT )
            {
                serPaletteType = eSerializedPaletteType::PAL8;
            }

            cfgStruct.paletteType = serPaletteType;
        }

        cfgBlock.writeStruct( cfgStruct );
    }

    TxdBuildModule::run_config config;
    bool closeOnCompletion;

    RwList <MassBuildWindow> windows;
};

static optional_struct_space <PluginDependantStructRegister <massbuildEnv, mainWindowFactory_t>> massbuildEnvRegister;

MassBuildWindow::MassBuildWindow( MainWindow *mainWnd ) : QDialog( mainWnd )
{
    // We want to create a dialog that spawns a task dialog with a build task with the configuration it set.
    // So you can create this dialog and spawn multiple build tasks from it.

    this->mainWnd = mainWnd;

    massbuildEnv *env = massbuildEnvRegister.get().GetPluginStruct( mainWnd );

    this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    this->setAttribute( Qt::WA_DeleteOnClose );

    // We want a dialog with important build configurations, so split it into two vertical panes.
    MagicLayout<QHBoxLayout> layout;

    QVBoxLayout *leftPaneLayout = new QVBoxLayout();

    // First the ability to select the paths.
    leftPaneLayout->addLayout(
        qtshared::createGameRootInputOutputForm(
            env->config.gameRoot,
            env->config.outputRoot,
            this->editGameRoot,
            this->editOutputRoot
        )
    );

    leftPaneLayout->addSpacing( 10 );

    // Then we should have a target configuration.
    createTargetConfigurationComponents(
        leftPaneLayout,
        env->config.targetPlatform,
        env->config.targetGame,
        this->selGameBox,
        this->selPlatformBox
    );

    leftPaneLayout->addSpacing( 10 );

    // Now some basic properties that the user might want to do globally.
    leftPaneLayout->addLayout(
        qtshared::createMipmapGenerationGroup(
            this,
            env->config.generateMipmaps,
            env->config.curMipMaxLevel,
            this->propGenMipmaps,
            this->propGenMipmapsMax
        )
    );

    // Meta-properties.
    QCheckBox *propCloseAfterComplete = CreateCheckBoxL( "Tools.MassBld.CloseOnCmplt" );

    propCloseAfterComplete->setChecked( env->closeOnCompletion );

    leftPaneLayout->addWidget( propCloseAfterComplete );

    this->propCloseAfterComplete = propCloseAfterComplete;

    leftPaneLayout->addSpacing( 15 );

    layout.top->addLayout( leftPaneLayout );

    // Continue on the right pane.
    QVBoxLayout *rightPaneLayout = new QVBoxLayout();

    rightPaneLayout->setAlignment( Qt::AlignTop );

    // We want more build properties.
    {
        QHBoxLayout *propCompressedGroup = new QHBoxLayout();

        QCheckBox *propCompressed = CreateCheckBoxL( "Tools.MassBld.Cmprs" );

        propCompressed->setChecked( env->config.doCompress );

        this->propCompressTextures = propCompressed;

        propCompressedGroup->addWidget( propCompressed, 0, Qt::AlignLeft );

        propCompressedGroup->addSpacing( 50 );

        QHBoxLayout *editGroup = new QHBoxLayout();

        editGroup->setAlignment( Qt::AlignRight );

        editGroup->addWidget( CreateLabelL( "Tools.MassBld.DescQual" ) );

        MagicLineEdit *editCompressionQuality = new MagicLineEdit();

        // Set the current compression quality into it.
        {
            std::stringstream strStream;

            strStream.precision( 2 );

            strStream << env->config.compressionQuality;

            editCompressionQuality->setText( QString::fromStdString( strStream.str() ) );
        }

        this->editCompressionQuality = editCompressionQuality;

        editCompressionQuality->setFixedWidth( 50 );

        editGroup->addWidget( editCompressionQuality );

        propCompressedGroup->addLayout( editGroup );

        rightPaneLayout->addLayout( propCompressedGroup );

        // Set up the connections.
        OnSelectCompressed( propCompressed->checkState() );

        connect( propCompressed, &QCheckBox::stateChanged, this, &MassBuildWindow::OnSelectCompressed );
    }

    // Palettization property.
    {
        QHBoxLayout *propPalettizedGroup = new QHBoxLayout();

        QCheckBox *propPalettized = CreateCheckBoxL( "Tools.MassBld.DoPal" );

        propPalettized->setChecked( env->config.doPalettize );

        this->propPalettizeTextures = propPalettized;

        propPalettizedGroup->addWidget( propPalettized, 0, Qt::AlignLeft );

        propPalettizedGroup->addSpacing( 50 );

        QHBoxLayout *editGroup = new QHBoxLayout();

        editGroup->setAlignment( Qt::AlignRight );

        editGroup->addWidget( CreateLabelL( "Tools.MassBld.DescPType" ) );

        QComboBox *selectPaletteType = new QComboBox();

        selectPaletteType->addItem( "PAL4" );
        selectPaletteType->addItem( "PAL8" );

        // Select the palette type in the dialog.
        {
            rw::ePaletteType currentPalType = env->config.paletteType;

            if ( currentPalType == rw::PALETTE_4BIT || currentPalType == rw::PALETTE_4BIT_LSB )
            {
                selectPaletteType->setCurrentText( "PAL4" );
            }
            else if ( currentPalType == rw::PALETTE_8BIT )
            {
                selectPaletteType->setCurrentText( "PAL8" );
            }
        }

        this->selectPaletteType = selectPaletteType;

        selectPaletteType->setFixedWidth( 80 );

        editGroup->addWidget( selectPaletteType );

        propPalettizedGroup->addLayout( editGroup );

        rightPaneLayout->addLayout( propPalettizedGroup );

        // Set up the connections.
        OnSelectPalettized( propPalettized->checkState() );

        connect( propPalettized, &QCheckBox::stateChanged, this, &MassBuildWindow::OnSelectPalettized );
    }

    layout.top->addLayout( rightPaneLayout );

    // Last thing is the typical button row.
    QPushButton *buttonBuild = CreateButtonL( "Tools.MassBld.Build" );

    connect( buttonBuild, &QPushButton::clicked, this, &MassBuildWindow::OnRequestBuild );

    layout.bottom->addWidget( buttonBuild );

    QPushButton *buttonCancel = CreateButtonL( "Tools.MassBld.Cancel" );

    connect( buttonCancel, &QPushButton::clicked, this, &MassBuildWindow::OnRequestCancel );

    layout.bottom->addWidget( buttonCancel );

    this->setLayout(layout.root);

    RegisterTextLocalizationItem( this );

    // We want to know about all active windows.
    LIST_INSERT( env->windows.root, this->node );
}

MassBuildWindow::~MassBuildWindow( void )
{
    // Remove us from the registry.
    LIST_REMOVE( this->node );

    UnregisterTextLocalizationItem( this );
}

void MassBuildWindow::updateContent( MainWindow *mainWnd )
{
    this->setWindowTitle( MAGIC_TEXT("Tools.MassBld.Desc") );
}

void MassBuildWindow::serialize( void )
{
    massbuildEnv *env = massbuildEnvRegister.get().GetPluginStruct( this->mainWnd );

    env->config.gameRoot = qt_to_widerw( this->editGameRoot->text() );
    env->config.outputRoot = qt_to_widerw( this->editOutputRoot->text() );

    platformToNaturalList.getCurrent( this->selPlatformBox, env->config.targetPlatform );
    gameToNaturalList.getCurrent( this->selGameBox, env->config.targetGame );

    env->config.generateMipmaps = this->propGenMipmaps->isChecked();
    env->config.curMipMaxLevel = this->propGenMipmapsMax->text().toInt();

    env->closeOnCompletion = this->propCloseAfterComplete->isChecked();

    // Compression.
    {
        env->config.doCompress = this->propCompressTextures->isChecked();
        env->config.compressionQuality = this->editCompressionQuality->text().toFloat();
    }

    // Palettization.
    {
        env->config.doPalettize = this->propPalettizeTextures->isChecked();

        // Get the palette type.
        {
            QString strPalType = this->selectPaletteType->currentText();

            if ( strPalType == "PAL4" )
            {
                env->config.paletteType = rw::PALETTE_4BIT;
            }
            else if ( strPalType == "PAL8" )
            {
                env->config.paletteType = rw::PALETTE_8BIT;
            }
        }
    }
}

struct MassBuildModule : public TxdBuildModule
{
    inline MassBuildModule( TaskCompletionWindow *taskWnd, rw::Interface *rwEngine ) : TxdBuildModule( rwEngine )
    {
        this->taskWnd = taskWnd;
    }

    void OnMessage( const rw::rwStaticString <wchar_t>& msg ) override
    {
        taskWnd->updateStatusMessage( wide_to_qt( msg ) );
    }

    rw::rwStaticString <wchar_t> TOKEN( const char *token ) override
    {
        return qt_to_widerw( MAGIC_TEXT(token) );
    }

    // Redirect every file access to the global stream function.
    CFile*  WrapStreamCodec( CFile *compressed ) override
    {
        return CreateDecompressedStream( taskWnd->getMainWindow(), compressed );
    }

    TaskCompletionWindow *taskWnd;
};

struct massbuild_task_params
{
    inline massbuild_task_params( const TxdBuildModule::run_config& config ) : config( config )
    {
        return;
    }

    TxdBuildModule::run_config config;
    TaskCompletionWindow *taskWnd;
    MainWindow *mainWnd;
};

static void massbuild_task_entry( rw::thread_t threadHandle, rw::Interface *engineInterface, void *ud )
{
    massbuild_task_params *params = (massbuild_task_params*)ud;

    try
    {
        // Run the mass build module.
        MassBuildModule module( params->taskWnd, engineInterface );

        module.RunApplication( params->config );
    }
    catch( ... )
    {
        delete params;

        throw;
    }

    // Make sure to free memory.
    delete params;
}

void MassBuildWindow::OnRequestBuild( bool checked )
{
    this->serialize();

    rw::Interface *rwEngine = this->mainWnd->GetEngine();

    MainWindow *mainWnd = this->mainWnd;

    massbuildEnv *env = massbuildEnvRegister.get().GetPluginStruct( mainWnd );

    // Create a copy of our configuration.
    massbuild_task_params *params = new massbuild_task_params( env->config );
    params->mainWnd = mainWnd;

    // Create the task.
    rw::thread_t taskThread = rw::MakeThread( rwEngine, massbuild_task_entry, params );

    // Communicate using a task window.
    TaskCompletionWindow *taskWnd = new LogTaskCompletionWindow( mainWnd, taskThread, "Tools.MassBld.TaskWndTitle", "Tools.MassBld.TaskWndInitialText", 0 );

    taskWnd->setCloseOnCompletion( env->closeOnCompletion );

    params->taskWnd = taskWnd;

    rw::ResumeThread( rwEngine, taskThread );

    this->close();
}

void MassBuildWindow::OnRequestCancel( bool checked )
{
    this->close();
}

void InitializeMassBuildEnvironment( void )
{
    massbuildEnvRegister.Construct( mainWindowFactory );
}

void ShutdownMassBuildEnvironment( void )
{
    massbuildEnvRegister.Destroy();
}
