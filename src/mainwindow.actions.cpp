/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.actions.cpp
*  PURPOSE:     Parallelized action system.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

// Executes TXD file actions that can be performed by Magic.TXD.

#include "mainwindow.h"

#include <QtWidgets/QStatusBar>
#include <QtWidgets/QShortcut>

MagicActionSystem::MagicActionSystem( NativeExecutive::CExecutiveManager *natExec )
{
    this->nativeExec = natExec;

    this->lockActionQueue = natExec->CreateReadWriteLock();
    this->condHasActions = natExec->CreateConditionVariable();
    this->lockPostedActions = natExec->CreateReadWriteLock();
    this->lockCurrentTask = natExec->CreateReadWriteLock();
    this->lock_activeActions = natExec->CreateReadWriteLock();
    this->cond_activeActionsModified = natExec->CreateConditionVariable();
    this->lock_attached_token = natExec->CreateReadWriteLock();

    // Remember that it is okay to act like a spoiled brat inside of magic-txd and use
    // the lambda version of CreateThread. In realtime-critical code you must never do that
    // and instead allocate the runtime memory somewhere fixed.

    // Boot the sheduler.
    NativeExecutive::CExecThread *shedThread = NativeExecutive::CreateThreadL( natExec,
        [this, natExec]( NativeExecutive::CExecThread *theThread )
    {
        using namespace std::chrono;

        bool was_executing_task = false;

        while ( true )
        {
            // Make sure to terminate.
            natExec->CheckHazardCondition();

            // Wait for active tasks.
            MagicAction *action;
            {
                NativeExecutive::CReadWriteWriteContextSafe <> ctxFetchTask( this->lockActionQueue );

                while ( LIST_EMPTY( this->list_sheduledActions.root ) )
                {
                    if ( was_executing_task )
                    {
                        MGTXD_TaskProcessingEndEvent *evt = new MGTXD_TaskProcessingEndEvent;

                        QCoreApplication::postEvent( this, evt );

                        was_executing_task = false;
                    }

                    this->condHasActions->Wait( ctxFetchTask );
                }

                if ( !was_executing_task )
                {
                    MGTXD_TaskProcessingBeginEvent *evt = new MGTXD_TaskProcessingBeginEvent;

                    QCoreApplication::postEvent( this, evt );

                    was_executing_task = true;
                }

                action = LIST_GETITEM( MagicAction, this->list_sheduledActions.root.next, listNode );

                LIST_REMOVE( action->listNode );
            }

            // SET CURRENT TASK.
            {
                NativeExecutive::CReadWriteWriteContext <> ctx_startTask( this->lockCurrentTask );

                // Set the currently active task.
                this->running_task = action;

                // Notify the system.
                this->OnStartAction();
            }

            // Notify the runtime about the starting of an event, async.
            {
                MGTXD_StartTaskEvent *evt = new MGTXD_StartTaskEvent( action );

                QCoreApplication::postEvent( this, evt );
            }

            // Execute the code associated with it.
            bool hasThreadTerminated = false;
            std::exception_ptr termination_exception;

            // Actions are allowed to actually be post-only.
            if ( auto run_cb = action->run_cb )
            {
                try
                {
                    run_cb( this, action->run_ud );
                }
                catch( NativeExecutive::threadTerminationException& )
                {
                    // Must exit ASAP.
                    hasThreadTerminated = true;
                    termination_exception = std::current_exception();
                }
                catch( ... )
                {
                    // If some action threw any exception we kinda just ignore it.
                    // Especially we do catch the cancellation exception here.
                }
            }

            // Clean-up.
            if ( auto cleanupFunc = action->run_cleanup )
            {
                cleanupFunc( action->run_ud );
            }

            // UNSET CURRENT TASK.
            {
                NativeExecutive::CReadWriteWriteContext <> ctx_stopTask( this->lockCurrentTask );

                this->OnStopAction();

                // We have no more running task.
                this->running_task = nullptr;
            }

            if ( action->isCancelled )
            {
                // Make sure we are not cancelling anymore.
                // This resets any special state that was set by the runtime.
                theThread->SetThreadCancelling( false );
            }

            // Continue unwinding.
            if ( hasThreadTerminated )
            {
                std::rethrow_exception( termination_exception );
            }

            // Post the task.
            // We will finish execution as provided by the runtime.
            {
                NativeExecutive::CReadWriteWriteContext <> ctx_postAction( this->lockPostedActions );

                LIST_APPEND( this->list_postedActions.root, action->listNode );
            }
            {
                MGTXD_EndTaskEvent *evt = new MGTXD_EndTaskEvent( action );

                QCoreApplication::postEvent( this, evt );
            }
        }
    }, 0, "sngl-act-thr");

    assert( shedThread != nullptr );

    shedThread->Resume();

    this->shedulerThread = shedThread;
    this->running_task = nullptr;

    RegisterTextLocalizationItem( this );
}

void MagicActionSystem::TerminateSheduling( void )
{
    NativeExecutive::CExecutiveManager *nativeExec = this->nativeExec;

    NativeExecutive::CExecThread *shedThread = this->shedulerThread;

    shedThread->Terminate( true );

    nativeExec->CloseThread( shedThread );

    this->shedulerThread = nullptr;
}

void MagicActionSystem::RemoveActionTokenConnection( MagicAction *action )
{
    NativeExecutive::CReadWriteWriteContext <> ctx_removeTaskTokenConn( this->lock_attached_token );

    // Need to check this.
    // Requirement to have a solid system to circumvent limitations of a special
    // case of the philosopher's fork grabbing scenario.
    if ( action->systemPtr != this )
        return;

    if ( ActionToken *token = action->attached_token )
    {
        // Just clear the pointers.
        token->action = nullptr;
        action->attached_token = nullptr;
    }
}

void MagicActionSystem::CleanupAction( MagicAction *item, bool remove_node, bool cleanup_run ) noexcept
{
    // We silently do our job.
    if ( cleanup_run )
    {
        if ( auto cleanup = item->run_cleanup )
        {
            cleanup( item->run_ud );
        }
    }

    if ( auto cleanup = item->post_cleanup )
    {
        cleanup( item->post_ud );
    }

    if ( auto cleanup = item->abort_cleanup )
    {
        cleanup( item->abort_ud );
    }

    if ( auto cleanup = item->finish_cleanup )
    {
        cleanup( item->finish_ud );
    }


    if ( remove_node )
    {
        LIST_REMOVE( item->listNode );
    }

    // Remove from the active list.
    {
        NativeExecutive::CReadWriteWriteContext <> ctx_removeAction( this->lock_activeActions );

        LIST_REMOVE( item->activeNode );

        this->cond_activeActionsModified->Signal();
    }

    // Cleanup advanced resources.
    NativeExecutive::CExecutiveManager *execMan = this->nativeExec;

    execMan->CloseReadWriteLock( item->lock_postExecHandlers );

    delete item;
}

MagicActionSystem::~MagicActionSystem( void )
{
    NativeExecutive::CExecutiveManager *nativeExec = this->nativeExec;

    // The user must have terminated the runtime before us.
    assert( this->shedulerThread == nullptr );

    // If there are any sheduled items left that have not yet executed then clean them up.
    while ( !LIST_EMPTY( this->list_activeActions.root ) )
    {
        MagicAction *item = LIST_GETITEM( MagicAction, this->list_sheduledActions.root.next, listNode );

        this->RemoveActionTokenConnection( item );
        this->CleanupAction( item, true, true );
    }
    assert( LIST_EMPTY( this->list_sheduledActions.root ) == true );
    assert( LIST_EMPTY( this->list_postedActions.root ) == true );

    // Should be cleaned up by the thread itself.
    assert( this->running_task == nullptr );

    UnregisterTextLocalizationItem( this );

    // Since all activity has ceased we are safe to release all resources.
    nativeExec->CloseReadWriteLock( this->lock_attached_token );
    nativeExec->CloseConditionVariable( this->cond_activeActionsModified );
    nativeExec->CloseReadWriteLock( this->lock_activeActions );
    nativeExec->CloseReadWriteLock( this->lockCurrentTask );
    nativeExec->CloseReadWriteLock( this->lockPostedActions );
    nativeExec->CloseConditionVariable( this->condHasActions );
    nativeExec->CloseReadWriteLock( this->lockActionQueue );
}

void MagicActionSystem::PublishAction( MagicAction *action )
{
    NativeExecutive::CReadWriteWriteContext <> ctxPutAction( this->lockActionQueue );

    LIST_APPEND( this->list_sheduledActions.root, action->listNode );

    this->condHasActions->Signal();

    NativeExecutive::CReadWriteWriteContext <> ctxAliveAction( this->lock_activeActions );

    LIST_APPEND( this->list_activeActions.root, action->activeNode );

    this->cond_activeActionsModified->Signal();
}

MagicActionSystem::ActionToken MagicActionSystem::LaunchAction(
    eMagicTaskType taskType, const char *tokenFriendlyTitle,
    MagicAction::actionRuntime_t cb, void *ud, MagicAction::cleanupFunc_t cleanup,
    MagicAction::actionRuntime_t abort_cb, void *abort_ud, MagicAction::cleanupFunc_t abort_cleanup
)
{
    try
    {
        MagicAction *action = new MagicAction;
        action->systemPtr = this;
        action->run_cb = cb;
        action->run_ud = ud;
        action->run_cleanup = cleanup;
        action->abort_cb = abort_cb;
        action->abort_ud = abort_ud;
        action->abort_cleanup = abort_cleanup;
        action->post_cb = nullptr;
        action->post_ud = nullptr;
        action->post_cleanup = nullptr;
        action->finish_cb = nullptr;
        action->finish_ud = nullptr;
        action->finish_cleanup = nullptr;
        action->hasExecutedPostExecHandlers = false;
        action->lock_postExecHandlers = this->nativeExec->CreateReadWriteLock();
        action->desc.taskType = taskType;
        action->desc.tokenFriendlyTitle = std::move( tokenFriendlyTitle );
        action->isCancelled = false;
        action->attached_token = nullptr;

        ActionToken token( this, action );

        // Add the action to our system.
        PublishAction( action );

        return token;
    }
    catch( ... )
    {
        if ( cleanup )
        {
            cleanup( ud );
        }

        if ( abort_cleanup )
        {
            abort_cleanup( abort_ud );
        }

        throw;
    }
}

MagicActionSystem::ActionToken MagicActionSystem::LaunchPostOnlyAction(
    eMagicTaskType taskType, const char *tokenFriendlyTitle,
    MagicAction::actionRuntime_t cb, void *ud, MagicAction::cleanupFunc_t cleanup,
    MagicAction::actionRuntime_t abort_cb, void *abort_ud, MagicAction::cleanupFunc_t abort_cleanup
)
{
    try
    {
        MagicAction *action = new MagicAction;
        action->systemPtr = this;
        action->run_cb = nullptr;
        action->run_ud = nullptr;
        action->run_cleanup = nullptr;
        action->post_cb = cb;
        action->post_ud = ud;
        action->post_cleanup = cleanup;
        action->abort_cb = abort_cb;
        action->abort_ud = abort_ud;
        action->abort_cleanup = abort_cleanup;
        action->finish_cb = nullptr;
        action->finish_ud = nullptr;
        action->finish_cleanup = nullptr;
        action->hasExecutedPostExecHandlers = false;
        action->lock_postExecHandlers = this->nativeExec->CreateReadWriteLock();
        action->desc.taskType = taskType;
        action->desc.tokenFriendlyTitle = std::move( tokenFriendlyTitle );
        action->isCancelled = false;
        action->attached_token = nullptr;

        ActionToken token( this, action );

        // Add the action to our system.
        PublishAction( action );

        return token;
    }
    catch( ... )
    {
        if ( cleanup )
        {
            cleanup( ud );
        }

        if ( abort_cleanup )
        {
            abort_cleanup( abort_ud );
        }

        throw;
    }
}

void MagicActionSystem::CurrentActionSetPost(
    MagicAction::actionRuntime_t cb, void *ud, MagicAction::cleanupFunc_t cleanup
)
{
    NativeExecutive::CReadWriteWriteContext <> ctxSetPost( this->lockCurrentTask );

    MagicAction *currentTask = this->running_task;

    if ( currentTask == nullptr )
    {
        if ( cleanup )
        {
            cleanup( ud );
        }

        return;
    }

    currentTask->post_cb = cb;
    currentTask->post_ud = ud;
    currentTask->post_cleanup = cleanup;
}

void MagicActionSystem::CurrentActionSetCleanup(
    MagicAction::actionRuntime_t cb, void *ud, MagicAction::cleanupFunc_t cleanup
)
{
    NativeExecutive::CReadWriteWriteContext <> ctxSetCleanup( this->lockCurrentTask );

    MagicAction *currentTask = this->running_task;

    if ( currentTask == nullptr )
    {
        if ( cleanup )
        {
            cleanup( ud );
        }
        
        return;
    }

    currentTask->finish_cb = cb;
    currentTask->finish_ud = ud;
    currentTask->finish_cleanup = cleanup;
}

bool MagicActionSystem::CurrentActionCancel( void )
{
    NativeExecutive::CReadWriteWriteContext <> ctxSetPost( this->lockCurrentTask );

    MagicAction *currentTask = this->running_task;

    if ( currentTask == nullptr )
        return false;

    if ( currentTask->isCancelled )
        return false;

    // Notify the threading manager that we want to get out of our stack
    // as fast as possible.
    this->shedulerThread->SetThreadCancelling( true );

    // Reset the cancelling state as soon as we get out of the stack.
    currentTask->isCancelled = true;

    return true;
}

void MagicActionSystem::WaitForActionQueueCompletion( void )
{
    // Could work with a read context aswell.
    NativeExecutive::CReadWriteWriteContextSafe <> ctx_waitForEmptyActionQueue( this->lock_activeActions );

    while ( LIST_EMPTY( this->list_activeActions.root ) == false )
    {
        this->cond_activeActionsModified->Wait( ctx_waitForEmptyActionQueue );
    }
}

void MagicActionSystem::TaskProcessingBeginEvent( MGTXD_TaskProcessingBeginEvent *evt )
{
    // Notify the runtime.
    this->OnAsyncTaskProcessingBegin();
}

void MagicActionSystem::TaskProcessingEndEvent( MGTXD_TaskProcessingEndEvent *evt )
{
    // Notify the runtime.
    this->OnAsyncTaskProcessingEnd();
}

void MagicActionSystem::StartTaskEvent( MGTXD_StartTaskEvent *evt )
{
    MagicAction *action = evt->getAction();

    if ( const char *statusMsgToken = action->desc.tokenFriendlyTitle )
    {
        // We want to set the task text into the status bar.
        this->OnUpdateStatusMessage( qt_to_widerw(MAGIC_TEXT(statusMsgToken)) );
    }

    // Notify the runtime.
    this->OnAsyncStartAction( action->desc );
}

void MagicActionSystem::EndTaskEvent( MGTXD_EndTaskEvent *evt )
{
    MagicAction *action = evt->getAction();

    // Notify the runtime.
    this->OnAsyncStopAction( action->desc );

    // Enter post exec.
    {
        NativeExecutive::CReadWriteWriteContext <> ctx_postExec( action->lock_postExecHandlers );

        if ( action->hasExecutedPostExecHandlers == false )
        {
            try
            {
                if ( action->isCancelled == false )
                {
                    try
                    {
                        // Post the action.
                        if ( auto post_cb = action->post_cb )
                        {
                            post_cb( this, action->post_ud );
                        }
                    }
                    catch( ... )
                    {
                        // Request to abort.
                        if ( auto abort_cb = action->abort_cb )
                        {
                            abort_cb( this, action->abort_ud );
                        }

                        // ... and continue on.
                    }
                }
                else
                {
                    // Abort the action, normal.
                    if ( auto abort_cb = action->abort_cb )
                    {
                        abort_cb( this, action->abort_ud );
                    }
                }
            }
            catch( ... )
            {}

            // Execute any pending finish task.
            if ( auto finish_cb = action->finish_cb )
            {
                try
                {
                    finish_cb( this, action->finish_ud );
                }
                catch( ... )
                {
                    // Ignore exceptions.
                }
            }

            action->hasExecutedPostExecHandlers = true;
        }
    }

    // Make sure we remove any token connection.
    this->RemoveActionTokenConnection( action );

    // Remove the action from the post list.
    {
        NativeExecutive::CReadWriteWriteContext <> ctx_removePost( this->lockPostedActions );

        LIST_REMOVE( action->listNode );
    }

    // Release the memory of that action again.
    this->CleanupAction( action, false, false );
}

void MagicActionSystem::customEvent( QEvent *evt )
{
    MGTXDCustomEvent evtType = (MGTXDCustomEvent)evt->type();

    if ( evtType == MGTXDCustomEvent::TaskProcessingBegin )
    {
        MGTXD_TaskProcessingBeginEvent *cevt = (MGTXD_TaskProcessingBeginEvent*)evt;

        this->TaskProcessingBeginEvent( cevt );
        return;
    }
    else if ( evtType == MGTXDCustomEvent::TaskProcessingEnd )
    {
        MGTXD_TaskProcessingEndEvent *cevt = (MGTXD_TaskProcessingEndEvent*)evt;

        this->TaskProcessingEndEvent( cevt );
        return;
    }
    else if ( evtType == MGTXDCustomEvent::StartTask )
    {
        MGTXD_StartTaskEvent *cevt = (MGTXD_StartTaskEvent*)evt;

        this->StartTaskEvent( cevt );
        return;
    }
    else if ( evtType == MGTXDCustomEvent::EndTask )
    {
        MGTXD_EndTaskEvent *cevt = (MGTXD_EndTaskEvent*)evt;

        this->EndTaskEvent( cevt );
        return;
    }
}

void MagicActionSystem::updateContent( MainWindow *mainWnd )
{
    NativeExecutive::CReadWriteReadContext <> ctx_runningTask( this->lockCurrentTask );

    // If there is a task running then update the status bar.
    if ( MagicAction *currentAction = this->running_task )
    {
        if ( const char *statusMsgToken = currentAction->desc.tokenFriendlyTitle )
        {
            this->OnUpdateStatusMessage( qt_to_widerw(MAGIC_TEXT(statusMsgToken)) );
        }
    }
}

// Specialization for MainWindow.
MainWindow::EditorActionSystem::EditorActionSystem( MainWindow *mainWnd )
    : MagicActionSystem( (NativeExecutive::CExecutiveManager*)rw::GetThreadingNativeManager( mainWnd->GetEngine() ) )
{
    this->wnd = mainWnd;

    // Install a shortcut to cancel activity (ESC key).
    QShortcut *shortcutCancelActivity = new QShortcut( QKeySequence("ESC"), mainWnd );
    connect( shortcutCancelActivity, &QShortcut::activated,
        [this, mainWnd]( void )
        {
            bool hasCancelled = this->CurrentActionCancel();

            if ( hasCancelled )
            {
                mainWnd->txdLog->addLogMessage( MAGIC_TEXT("Tasks.Cancelled"), LOGMSG_INFO );
            }
        }
    );
}

MainWindow::EditorActionSystem::~EditorActionSystem( void )
{
    // Don't forget to terminate sheduling.
    this->TerminateSheduling();
}

void MainWindow::EditorActionSystem::rwAsyncWarningManager::OnWarning( rw::rwStaticString <wchar_t>&& msg )
{
    // Need to pipe all RenderWare warnings to the asynchronous message line.
    EditorActionSystem *actionSys = LIST_GETITEM( EditorActionSystem, this, asyncWarn );
    MainWindow *wnd = actionSys->wnd;

    wnd->asyncLog( wide_to_qt( msg ), LOGMSG_WARNING );
}

void MainWindow::EditorActionSystem::OnStartAction( void )
{
    rw::Interface *rwEngine = this->wnd->rwEngine;

    rw::AssignThreadedRuntimeConfig( rwEngine );

    // Install asynchronous warning handling.
    rwEngine->SetWarningManager( &this->asyncWarn );
}

void MainWindow::EditorActionSystem::OnStopAction( void )
{
    rw::Interface *rwEngine = this->wnd->rwEngine;

    rw::ReleaseThreadedRuntimeConfig( rwEngine );
}

void MainWindow::EditorActionSystem::OnUpdateStatusMessage( const rw::rwStaticString <wchar_t>& msg )
{
    MainWindow *mainWnd = this->wnd;

    if ( QStatusBar *statusBar = mainWnd->statusBar() )
    {
        statusBar->showMessage( wide_to_qt( msg ) );
    }

    mainWnd->progressStatusLoadingAnimation();
}

void MainWindow::EditorActionSystem::OnAsyncStartAction( const MagicActionDesc& desc )
{
    return;
}

void MainWindow::EditorActionSystem::OnAsyncStopAction( const MagicActionDesc& desc )
{
    return;
}

void MainWindow::EditorActionSystem::OnAsyncTaskProcessingBegin( void )
{
    MainWindow *mainWnd = this->wnd;

    mainWnd->startStatusLoadingAnimation();
}

void MainWindow::EditorActionSystem::OnAsyncTaskProcessingEnd( void )
{
    MainWindow *mainWnd = this->wnd;

    // Clear status bar text.
    if ( QStatusBar *statusBar = mainWnd->statusBar() )
    {
        statusBar->clearMessage();
    }

    mainWnd->stopStatusLoadingAnimation();
}

//===========================
//      Token support.
//===========================

MagicActionSystem::ActionToken::ActionToken( MagicActionSystem *system, MagicAction *action ) noexcept
{
    this->system = system;
    this->action = action;

    action->attached_token = this;
}

MagicActionSystem::ActionToken::ActionToken( void ) noexcept
{
    this->system = nullptr;
    this->action = nullptr;
}

void MagicActionSystem::ActionToken::moveFrom( ActionToken&& right ) noexcept
{
    MagicActionSystem *system = right.system;

    if ( system == nullptr )
    {
        // if system is nullptr then so is task.
        this->system = nullptr;
        this->action = nullptr;
    }
    else
    {
        NativeExecutive::CReadWriteWriteContext <> ctx_updateTokenPtr( system->lock_attached_token );

        MagicAction *action = right.action;

        action->attached_token = this;

        this->system = system;
        this->action = action;

        right.action = nullptr;
    }
}

MagicActionSystem::ActionToken::ActionToken( ActionToken&& right ) noexcept
{
    this->moveFrom( std::move( right ) );
}

void MagicActionSystem::ActionToken::deferredDetach( void ) noexcept
{
    MagicActionSystem *system = this->system;
    MagicAction *action = this->action;

    if ( system != nullptr && action != nullptr )
    {
        // It is the responsbility of the system to check whether it still
        // is the system of the token inside this call.
        system->RemoveActionTokenConnection( action );
    }
}

MagicActionSystem::ActionToken::~ActionToken( void )
{
    this->deferredDetach();
}

MagicActionSystem::ActionToken& MagicActionSystem::ActionToken::operator = ( ActionToken&& right ) noexcept
{
    this->deferredDetach();
    this->moveFrom( std::move( right ) );

    return *this;
}

void MagicActionSystem::ActionToken::Cancel( bool directlyExecuteAbortHandlers )
{
    MagicActionSystem *system = this->system;

    if ( system == nullptr )
        return;

    NativeExecutive::CReadWriteWriteContext <> ctx_cancelActionOfToken( system->lock_attached_token );
    NativeExecutive::CReadWriteReadContext <> ctx_runningAction( system->lockCurrentTask );

    if ( MagicAction *action = this->action )
    {
        if ( action->systemPtr == system )
        {
            // We want to enter postExec state.
            // In this state we guarrantee that the action does not execute it's post exec handlers while we do.
            // If the post exec handlers have already executed then a cancellation does nothing, but we still register it.

            if ( action->isCancelled == false )
            {
                // Is the system running this task right now?
                if ( system->running_task == action )
                {
                    // Ask the sheduler thread to cancel the stack.
                    system->shedulerThread->SetThreadCancelling( true );
                }

                action->isCancelled = true;
            }

            if ( directlyExecuteAbortHandlers && action->hasExecutedPostExecHandlers == false )
            {
                if ( auto abort_cb = action->abort_cb )
                {
                    abort_cb( system, action->abort_ud );
                }

                if ( auto finish_cb = action->finish_cb )
                {
                    finish_cb( system, action->finish_ud );
                }

                action->hasExecutedPostExecHandlers = true;
            }
        }
    }
}