/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/massexport.cpp
*  PURPOSE:     UI for mass export tool.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include "massexport.h"

#include "qtsharedlogic.h"

#include "taskcompletionwindow.h"

#include "guiserialization.hxx"

#include <sdk/PluginHelpers.h>

#include "../src/tools/txdexport.h"

#include "qtutils.h"
#include "languages.h"

struct massexportEnv : public magicSerializationProvider
{
    inline void Initialize( MainWindow *mainWnd )
    {
        LIST_CLEAR( this->openDialogs.root );

        RegisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_MASSEXPORT, this );
    }

    void Shutdown( MainWindow *mainWnd )
    {
        UnregisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_MASSEXPORT );

        // Make sure all open dialogs are closed.
        while ( !LIST_EMPTY( openDialogs.root ) )
        {
            MassExportWindow *wnd = LIST_GETITEM( MassExportWindow, openDialogs.root.next, node );

            delete wnd;
        }
    }

    struct massexp_cfg_struct
    {
        endian::little_endian <MassExportModule::eOutputType> outputType;
    };

    void Load( MainWindow *mainWnd, rw::BlockProvider& massexportBlock ) override
    {
        this->config.gameRoot = RwReadUnicodeString( massexportBlock );
        this->config.outputRoot = RwReadUnicodeString( massexportBlock );
        this->config.recImgFormat = RwReadANSIString( massexportBlock );

        massexp_cfg_struct cfgStruct;
        massexportBlock.readStruct( cfgStruct );

        this->config.outputType = cfgStruct.outputType;
    }

    void Save( const MainWindow *mainWnd, rw::BlockProvider& massexportBlock ) const override
    {
        RwWriteUnicodeString( massexportBlock, this->config.gameRoot );
        RwWriteUnicodeString( massexportBlock, this->config.outputRoot );
        RwWriteANSIString( massexportBlock, this->config.recImgFormat );

        massexp_cfg_struct cfgStruct;
        cfgStruct.outputType = this->config.outputType;

        massexportBlock.writeStruct( cfgStruct );
    }

    MassExportModule::run_config config;

    RwList <MassExportWindow> openDialogs;
};

typedef PluginDependantStructRegister <massexportEnv, mainWindowFactory_t> massexportEnvRegister_t;

static optional_struct_space <massexportEnvRegister_t> massexportEnvRegister;

MassExportWindow::MassExportWindow( MainWindow *mainWnd ) : QDialog( mainWnd )
{
    // We want a dialog similar to the Mass Export one but it should be without a log.
    // Instead, we will launch a task completion window that will handle our task.

    this->mainWnd = mainWnd;

    massexportEnv *env = massexportEnvRegister.get().GetPluginStruct( mainWnd );

    rw::Interface *rwEngine = mainWnd->GetEngine();

    this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    this->setAttribute( Qt::WA_DeleteOnClose );

    // Just one pane with things to edit.
    // First we should put the paths to perform the extraction.
    MagicLayout<QVBoxLayout> layout(this);

    QFormLayout *pathRootForm = new QFormLayout();

    pathRootForm->addRow(
        CreateLabelL( "Tools.GameRt" ),
        qtshared::createPathSelectGroup( wide_to_qt( env->config.gameRoot ), this->editGameRoot )
    );

    pathRootForm->addRow(
        CreateLabelL( "Tools.Output" ),
        qtshared::createPathSelectGroup( wide_to_qt( env->config.outputRoot ), this->editOutputRoot )
   );

    layout.top->addLayout( pathRootForm );

    // The other most important thing is to select a target image format.
    // This is pretty easy.
    QHBoxLayout *imgFormatGroup = new QHBoxLayout();

    imgFormatGroup->setContentsMargins( 0, 0, 0, 10 );

    imgFormatGroup->addWidget(CreateLabelL("Tools.MassExp.ImgFmt"));

    QComboBox *boxRecomImageFormat = new QComboBox();
    {
        // We fill it with base formats.
        // Those formats are available to all native texture types.
        rw::registered_image_formats_t formats;
        rw::GetRegisteredImageFormats( rwEngine, formats );

        size_t numFormats = formats.GetCount();

        for ( size_t n = 0; n < numFormats; n++ )
        {
            const rw::registered_image_format& format = formats[n];

            const char *defaultExt = nullptr;

            bool gotDefaultExt = rw::GetDefaultImagingFormatExtension( format.num_ext, format.ext_array, defaultExt );

            if ( gotDefaultExt )
            {
                boxRecomImageFormat->addItem( QString( defaultExt ).toUpper() );
            }
        }

        // And our favourite format, the texture chunk!
        boxRecomImageFormat->addItem( "RWTEX" );
    }

    // If the remembered format exists in our combo box, select it.
    {
        int existFormatIndex = boxRecomImageFormat->findText( ansi_to_qt( env->config.recImgFormat ), Qt::MatchExactly );

        if ( existFormatIndex != -1 )
        {
            boxRecomImageFormat->setCurrentIndex( existFormatIndex );
        }
    }

    this->boxRecomImageFormat = boxRecomImageFormat;

    imgFormatGroup->addWidget( boxRecomImageFormat );

    layout.top->addLayout( imgFormatGroup );

    // Textures can be extracted in multiple modes, depending on how the user likes it best.
    // The plain mode extracts textures simply by their texture name into the location of the TXD.
    // The TXD name mode works differently: TXD name + _ + texture name; into the location of the TXD.
    // The folders mode extracts textures into folders that are called after the TXD, at the location of the TXD.

    MassExportModule::eOutputType outputType = env->config.outputType;

    QRadioButton *optionExportPlain = CreateRadioButtonL( "Tools.MassExp.TxNmOnl" );

    this->optionExportPlain = optionExportPlain;

    if ( outputType == MassExportModule::OUTPUT_PLAIN )
    {
        optionExportPlain->setChecked( true );
    }

    layout.top->addWidget( optionExportPlain );

    QRadioButton *optionExportTXDName = CreateRadioButtonL( "Tools.MassExp.PrTxdNm" );

    this->optionExportTXDName = optionExportTXDName;

    if ( outputType == MassExportModule::OUTPUT_TXDNAME )
    {
        optionExportTXDName->setChecked( true );
    }

    layout.top->addWidget( optionExportTXDName );

    QRadioButton *optionExportFolders = CreateRadioButtonL( "Tools.MassExp.SepFld" );

    this->optionExportFolders = optionExportFolders;

    if ( outputType == MassExportModule::OUTPUT_FOLDERS )
    {
        optionExportFolders->setChecked( true );
    }

    layout.top->addWidget( optionExportFolders );

    // The dialog is done, we finish off with the typical buttons.
    QPushButton *buttonExport = CreateButtonL( "Tools.MassExp.Export" );

    connect( buttonExport, &QPushButton::clicked, this, &MassExportWindow::OnRequestExport );

    layout.bottom->addWidget( buttonExport );

    QPushButton *buttonCancel = CreateButtonL( "Tools.MassExp.Cancel" );

    connect( buttonCancel, &QPushButton::clicked, this, &MassExportWindow::OnRequestCancel );

    layout.bottom->addWidget( buttonCancel );

    RegisterTextLocalizationItem( this );

    LIST_INSERT( env->openDialogs.root, this->node );

    // Finito.
}

MassExportWindow::~MassExportWindow( void )
{
    LIST_REMOVE( this->node );

    UnregisterTextLocalizationItem( this );
}

void MassExportWindow::updateContent( MainWindow *mainWnd )
{
    this->setWindowTitle( MAGIC_TEXT("Tools.MassExp.Desc") );
}

struct MagicMassExportModule : public MassExportModule
{
    inline MagicMassExportModule( rw::Interface *rwEngine, TaskCompletionWindow *wnd ) : MassExportModule( rwEngine )
    {
        this->wnd = wnd;
    }

    void OnProcessingFile( const std::wstring& fileName ) override
    {
        wnd->updateStatusMessage( MAGIC_TEXT("Tools.MassExp.Proc").arg(QString::fromStdWString( fileName )) );
    }

    CFile* WrapStreamCodec( CFile *stream ) override
    {
        return CreateDecompressedStream( wnd->getMainWindow(), stream );
    }

private:
    TaskCompletionWindow *wnd;
};

struct exporttask_params
{
    TaskCompletionWindow *taskWnd;
    MassExportModule::run_config config;
};

static void exporttask_runtime( rw::thread_t handle, rw::Interface *engineInterface, void *ud )
{
    exporttask_params *params = (exporttask_params*)ud;

    try
    {
        // We want our own configuration.
        rw::AssignThreadedRuntimeConfig( engineInterface );

        // Warnings should be ignored.
        engineInterface->SetWarningLevel( 0 );
        engineInterface->SetWarningManager( NULL );

        // Run the application.
        MagicMassExportModule module( engineInterface, params->taskWnd );

        module.ApplicationMain( params->config );
    }
    catch( ... )
    {
        delete params;

        throw;
    }

    // Release the configuration that has been given to us.
    delete params;
}

void MassExportWindow::OnRequestExport( bool checked )
{
    // Store our configuration.
    this->serialize();

    // Launch the task.
    {
        rw::Interface *engineInterface = this->mainWnd->GetEngine();

        // Allocate a private configuration for the task.
        const massexportEnv *env = massexportEnvRegister.get().GetConstPluginStruct( mainWnd );

        exporttask_params *params = new exporttask_params();
        params->config = env->config;
        params->taskWnd = nullptr;

        rw::thread_t taskHandle = rw::MakeThread( engineInterface, exporttask_runtime, params );

        // Create a window that is responsible of it.
        TaskCompletionWindow *taskWnd = new LabelTaskCompletionWindow( this->mainWnd, taskHandle, "Tools.MassExp.TaskWndTitle", "Tools.MassExp.TsakWndInitialText", 0 );

        params->taskWnd = taskWnd;

        rw::ResumeThread( engineInterface, taskHandle );
    }

    this->close();
}

void MassExportWindow::OnRequestCancel( bool checked )
{
    this->close();
}

void MassExportWindow::serialize( void )
{
    massexportEnv *env = massexportEnvRegister.get().GetPluginStruct( this->mainWnd );

    env->config.gameRoot = qt_to_widerw( this->editGameRoot->text() );
    env->config.outputRoot = qt_to_widerw( this->editOutputRoot->text() );
    env->config.recImgFormat = qt_to_ansirw( this->boxRecomImageFormat->currentText() );

    MassExportModule::eOutputType outputType = MassExportModule::OUTPUT_TXDNAME;

    if ( this->optionExportPlain->isChecked() )
    {
        outputType = MassExportModule::OUTPUT_PLAIN;
    }
    else if ( this->optionExportTXDName->isChecked() )
    {
        outputType = MassExportModule::OUTPUT_TXDNAME;
    }
    else if ( this->optionExportFolders->isChecked() )
    {
        outputType = MassExportModule::OUTPUT_FOLDERS;
    }

    env->config.outputType = outputType;
}

void InitializeMassExportToolEnvironment( void )
{
    massexportEnvRegister.Construct( mainWindowFactory );
}

void ShutdownMassExportToolEnvironment( void )
{
    massexportEnvRegister.Destroy();
}