/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/taskcompletionwindow.cpp
*  PURPOSE:     Window attached to thread tasks.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include "taskcompletionwindow.h"

#include <sdk/PluginHelpers.h>

struct taskCompletionWindowEnv
{
    inline void Initialize( MainWindow *mainWnd )
    {
        LIST_CLEAR( this->windows.root );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        while ( !LIST_EMPTY( this->windows.root ) )
        {
            TaskCompletionWindow *wnd = LIST_GETITEM( TaskCompletionWindow, this->windows.root.next, node );

            delete wnd;
        }
    }

    RwList <TaskCompletionWindow> windows;
};

static optional_struct_space <PluginDependantStructRegister <taskCompletionWindowEnv, mainWindowFactory_t>> taskCompletionWindowEnvRegister;

TaskCompletionWindow::TaskCompletionWindow( MainWindow *mainWnd, rw::thread_t taskHandle, const char *tokenTitleBar, int showDelayMS, bool isModal ) : QDialog( mainWnd )
{
    taskCompletionWindowEnv *env = taskCompletionWindowEnvRegister.get().GetPluginStruct( mainWnd );

    rw::Interface *rwEngine = mainWnd->GetEngine();

    this->setWindowFlags( this->windowFlags() & ( ~Qt::WindowContextHelpButtonHint & ~Qt::WindowCloseButtonHint ) );

    if ( isModal )
    {
        this->setWindowModality( Qt::WindowModal );
    }

    this->setAttribute( Qt::WA_DeleteOnClose );

    this->mainWnd = mainWnd;

    this->taskThreadHandle = taskHandle;

    // Initialize general properties.
    this->hasRequestedClosure = false;
    this->hasCompleted = false;
    this->closeOnCompletion = true;

    this->tokenTitleBar = tokenTitleBar;

    // This dialog should consist of a status message and a cancel button.
    QVBoxLayout *rootLayout = new QVBoxLayout();

    QVBoxLayout *logWidgetLayout = new QVBoxLayout();

    this->logAreaLayout = logWidgetLayout;
    
    rootLayout->addLayout( logWidgetLayout );

    QHBoxLayout *buttonRow = new QHBoxLayout();

    buttonRow->setAlignment( Qt::AlignCenter );

    QPushButton *buttonCancel = CreateButtonL( "TaskCompl.Cancel" );

    buttonCancel->setMaximumWidth( 90 );

    connect( buttonCancel, &QPushButton::clicked, this, &TaskCompletionWindow::OnRequestCancel );

    buttonRow->addWidget( buttonCancel );

    rootLayout->addLayout( buttonRow );

    this->setLayout( rootLayout );

    this->setMinimumWidth( 350 );

    LIST_INSERT( env->windows.root, this->node );

    RegisterTextLocalizationItem( this );

    if ( showDelayMS <= 0 )
    {
        this->setVisible( true );
    }
    else
    {
        // Show us after some delay.
        this->timer_show.setSingleShot( true );
        this->timer_show.setInterval( showDelayMS );

        connect( &this->timer_show, &QTimer::timeout,
            [this]
            {
                this->setVisible( true );
            }
        );

        this->timer_show.start();
    }

    // We need a waiter thread that will notify us of the task completion.
    this->waitThreadHandle = rw::MakeThread( rwEngine, waiterThread_runtime, this );

    rw::ResumeThread( rwEngine, this->waitThreadHandle );
}

TaskCompletionWindow::~TaskCompletionWindow( void )
{
    UnregisterTextLocalizationItem( this );

    // Remove us from the registry.
    LIST_REMOVE( this->node );

    rw::Interface *rwEngine = this->mainWnd->GetEngine();

    // Terminate the task.
    rw::TerminateThread( rwEngine, this->taskThreadHandle, false );

    // We cannot close the window until the task has finished.
    rw::JoinThread( rwEngine, this->waitThreadHandle );

    rw::CloseThread( rwEngine, this->waitThreadHandle );
        
    // We can safely get rid of the task thread handle.
    rw::CloseThread( rwEngine, this->taskThreadHandle );
}

void TaskCompletionWindow::updateContent( MainWindow *mainWnd )
{
    this->setWindowTitle( MAGIC_TEXT( this->tokenTitleBar ) );
}

void InitializeTaskCompletionWindowEnv( void )
{
    taskCompletionWindowEnvRegister.Construct( mainWindowFactory );
}

void ShutdownTaskCompletionWindowEnv( void )
{
    taskCompletionWindowEnvRegister.Destroy();
}

LabelTaskCompletionWindow::LabelTaskCompletionWindow( MainWindow *mainWnd, rw::thread_t taskHandle, const char *tokenTitleBar, const char *tokenStartMsg, int showDelayMS, bool isModal )
    : TaskCompletionWindow( mainWnd, taskHandle, tokenTitleBar, showDelayMS, isModal )
{
    QLabel *statusMessageLabel = new QLabel();

    statusMessageLabel->setAlignment( Qt::AlignCenter );

    this->statusMessageLabel = statusMessageLabel;

    this->logAreaLayout->addWidget( statusMessageLabel );

    this->tokenStartMsg = tokenStartMsg;
    this->hasStartMessage = true;

    // Initial update of the label.
    this->statusMessageLabel->setText( MAGIC_TEXT( tokenStartMsg ) );
}

LabelTaskCompletionWindow::~LabelTaskCompletionWindow( void )
{
    return;
}

void LabelTaskCompletionWindow::updateContent( MainWindow *mainWnd )
{
    if ( this->hasStartMessage )
    {
        this->statusMessageLabel->setText( MAGIC_TEXT( this->tokenStartMsg ) );
    }

    // Also update the parent.
    TaskCompletionWindow::updateContent( mainWnd );
}

void LabelTaskCompletionWindow::OnMessage( QString statusMsg )
{
    // We have no more starting label token.
    this->hasStartMessage = false;

    // Just update the label.
    this->statusMessageLabel->setText( statusMsg );
}

LogTaskCompletionWindow::LogTaskCompletionWindow( MainWindow *mainWnd, rw::thread_t taskHandle, const char *tokenTitleBar, QString statusMsg, int showDelayMS, bool isModal )
    : TaskCompletionWindow( mainWnd, taskHandle, tokenTitleBar, showDelayMS, isModal ), logEditControl( this )
{
    QWidget *logWidget = logEditControl.CreateLogWidget();

    logEditControl.directLogMessage( std::move( statusMsg ) );

    this->logAreaLayout->addWidget( logWidget );
}

LogTaskCompletionWindow::~LogTaskCompletionWindow( void )
{
    return;
}

void LogTaskCompletionWindow::OnMessage( QString statusMsg )
{
    logEditControl.directLogMessage( std::move( statusMsg ) );
}