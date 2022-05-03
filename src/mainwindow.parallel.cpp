/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.parallel.cpp
*  PURPOSE:     Multi-threading asynchronous task management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include <mainwindow.h>

// In contrast to the action system, the parallel tasks system allows for execution of many threads
// in parallel. This should be used for out-of-order execution of workloads.

MagicParallelTasks::MagicParallelTasks( NativeExecutive::CExecutiveManager *execMan )
{
    this->execMan = execMan;

    // Setup the synchronization objects.
    this->lock_sheduled_task_queue = execMan->CreateReadWriteLock();
    this->condWaitSheduledTask = execMan->CreateConditionVariable();
    this->lock_posted_task_list = execMan->CreateReadWriteLock();
    this->lock_running_tasks = execMan->CreateThreadReentrantReadWriteLock();
    this->lock_attached_token = execMan->CreateThreadReentrantReadWriteLock();

    // Determine the amount of threads that we should use to fully utilize the
    // user's machine.
    unsigned int recommendedAmountOfThreads = execMan->GetParallelCapability();

    for ( unsigned int n = 0; n < recommendedAmountOfThreads; n++ )
    {
        NativeExecutive::CExecThread *thread = NativeExecutive::CreateThreadL(
            execMan, [=, this] ( NativeExecutive::CExecThread *thread )
            {
                while ( true )
                {
                    // Explicit cancellation point for safety.
                    execMan->CheckHazardCondition();

                    // Wait until a task is available and grab it.
                    sheduled_task *task = nullptr;
                    {
                        NativeExecutive::CReadWriteWriteContextSafe <> ctx_grabTask( this->lock_sheduled_task_queue );

                        while ( LIST_EMPTY( this->sheduled_task_queue.root ) )
                        {
                            this->condWaitSheduledTask->Wait( ctx_grabTask );
                        }

                        task = LIST_GETITEM( sheduled_task, this->sheduled_task_queue.root.next, listNode );

                        LIST_REMOVE( task->listNode );
                    }

                    // ACTIVATE THE TASK.
                    {
                        NativeExecutive::CReadWriteWriteContext ctx_startTask( this->lock_running_tasks );

                        task->sheduledOnThread = thread;

                        LIST_APPEND( this->running_tasks.root, task->listNode );
                    }

                    // Run the task.
                    bool hasCancelledItself = false;

                    try
                    {
                        task->run_cb( task->run_ud );
                    }
                    catch( NativeExecutive::threadCancellationException& )
                    {
                        // Just continue on.
                    }
                    catch( NativeExecutive::threadTerminationException& )
                    {
                        // Just continue on.
                    }
                    catch( ... )
                    {
                        // If any exception was caught here then it means that our
                        // task has failed in some way, report it as self cancellation.
                        hasCancelledItself = true;
                    }

                    // Cleanup the data.
                    if ( auto cleanup = task->run_cleanup )
                    {
                        cleanup( task->run_ud );
                    }

                    // DEACTIVATE THE TASK.
                    {
                        NativeExecutive::CReadWriteWriteContext ctx_stopTask( this->lock_running_tasks );

                        task->sheduledOnThread = nullptr;

                        LIST_REMOVE( task->listNode );
                    }

                    // Report self cancellation.
                    if ( hasCancelledItself )
                    {
                        task->isCancelled = true;
                    }

                    if ( task->isCancelled )
                    {
                        // Need to reset cancellation mode again.
                        thread->SetThreadCancelling( false );
                    }

                    // If we caught a threadTerminationException earlier then we can just terminate here again.
                    // It is the same thing, easy to resume unwind!
                    // Good idea to skip post for terminated tasks.
                    // We exclude the simple thread cancelling exception because we disable it above.
                    execMan->CheckHazardCondition();

                    // Post the task.
                    {
                        NativeExecutive::CReadWriteWriteContext <> ctx_postTask( this->lock_posted_task_list );

                        LIST_APPEND( this->posted_task_list.root, task->listNode );
                    }
                    {
                        MGTXD_ParallelTaskPostEvent *evt = new MGTXD_ParallelTaskPostEvent( task );

                        QCoreApplication::postEvent( this, evt );
                    }
                }
            }, 0, "mult-act-thr"
        );

        // Start it up already.
        thread->Resume();

        executorThread_t threadInfo;
        threadInfo.threadHandle = thread;

        this->available_threads.AddToBack( std::move( threadInfo ) );
    }
}

void MagicParallelTasks::CleanupTask( sheduled_task *task, bool removeFromNode, bool cleanup_run ) noexcept
{
    if ( cleanup_run )
    {
        if ( auto cleanup = task->run_cleanup )
        {
            cleanup( task->run_ud );
        }
    }

    if ( auto cleanup = task->post_cleanup )
    {
        cleanup( task->post_ud );
    }

    if ( auto cleanup = task->abort_cleanup )
    {
        cleanup( task->abort_ud );
    }

    if ( removeFromNode )
    {
        LIST_REMOVE( task->listNode );
    }

    // Delete native resources.
    NativeExecutive::CExecutiveManager *execMan = this->execMan;

    execMan->CloseThreadReentrantReadWriteLock( task->lock_postExec );

    delete task;
}

void MagicParallelTasks::RemoveTaskTokenConnection( sheduled_task *task )
{
    NativeExecutive::CReadWriteWriteContext ctx_removeTaskTokenConn( this->lock_attached_token );

    // Need to check this.
    // Requirement to have a solid system to circumvent limitations of a special
    // case of the philosopher's fork grabbing scenario.
    if ( task->systemPtr != this )
        return;

    if ( TaskToken *token = task->attached_token )
    {
        // Just clear the pointers.
        token->task = nullptr;
        task->attached_token = nullptr;
    }
}

MagicParallelTasks::~MagicParallelTasks( void )
{
    NativeExecutive::CExecutiveManager *execMan = this->execMan;

    // Wait for all our threads to finish execution.
    for ( executorThread_t& threadInfo : this->available_threads )
    {
        NativeExecutive::CExecThread *threadHandle = threadInfo.threadHandle;

        threadHandle->Terminate( true );

        execMan->CloseThread( threadHandle );
    }

    // Since execution has finished we can clean-up the items inside the queues and lists.
    while ( !LIST_EMPTY( this->sheduled_task_queue.root ) )
    {
        sheduled_task *task = LIST_GETITEM( sheduled_task, this->sheduled_task_queue.root.next, listNode );

        RemoveTaskTokenConnection( task );
        CleanupTask( task, true, true );
    }

    while ( !LIST_EMPTY( this->posted_task_list.root ) )
    {
        sheduled_task *task = LIST_GETITEM( sheduled_task, this->posted_task_list.root.next, listNode );

        RemoveTaskTokenConnection( task );
        CleanupTask( task, true, false );
    }

    // Every thread must cleanup their own running item.
    // So the list has to be empty!
    assert( LIST_EMPTY( this->running_tasks.root ) == true );

    // Since all activity has ceased we are safe to release all resources.
    execMan->CloseThreadReentrantReadWriteLock( this->lock_attached_token );
    execMan->CloseThreadReentrantReadWriteLock( this->lock_running_tasks );
    execMan->CloseReadWriteLock( this->lock_posted_task_list );
    execMan->CloseConditionVariable( this->condWaitSheduledTask );
    execMan->CloseReadWriteLock( this->lock_sheduled_task_queue );
}

MagicParallelTasks::TaskToken MagicParallelTasks::LaunchTask(
    const char *task_key,
    taskFunc_t cb, void *ud, cleanupFunc_t cleanup,
    taskFunc_t abort_cb, void *abort_ud, cleanupFunc_t abort_cleanup
)
{
    NativeExecutive::CExecutiveManager *execMan = this->execMan;
    
    try
    {
        sheduled_task *task = new sheduled_task;

        try
        {
            task->systemPtr = this;
            task->sheduledOnThread = nullptr;
            task->taskKey = task_key;
            task->isCancelled = false;
            task->run_cb = cb;
            task->run_ud = ud;
            task->run_cleanup = cleanup;
            task->post_cb = nullptr;
            task->post_ud = nullptr;
            task->post_cleanup = nullptr;
            task->abort_cb = abort_cb;
            task->abort_ud = abort_ud;
            task->abort_cleanup = abort_cleanup;
            task->havePostExecHandlersExecuted = false;
            task->lock_postExec = execMan->CreateThreadReentrantReadWriteLock();
            task->attached_token = nullptr;

            TaskToken token( this, task );

            NativeExecutive::CReadWriteWriteContext <> ctx_sheduledTask( this->lock_sheduled_task_queue );

            LIST_APPEND( this->sheduled_task_queue.root, task->listNode );

            this->condWaitSheduledTask->SignalCount( 1 );

            return token;
        }
        catch( ... )
        {
            delete task;

            throw;
        }
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

void MagicParallelTasks::CancelTasksByKey( const char *task_key )
{
    // We ignore the sheduled tasks because we consider those to be taken in a whim.

    // Do the running tasks.
    {
        NativeExecutive::CReadWriteWriteContext ctx_killRunningTasks( this->lock_running_tasks );

        LIST_FOREACH_BEGIN( sheduled_task, this->running_tasks.root, listNode )

            if ( StringEqualToZero( item->taskKey, task_key, true ) )
            {
                item->sheduledOnThread->SetThreadCancelling( true );

                item->isCancelled = true;
            }

        LIST_FOREACH_END
    }

    // Also check the tasks that are to be posted.
    {
        NativeExecutive::CReadWriteWriteContext <> ctx_killPostTasks( this->lock_posted_task_list );

        LIST_FOREACH_BEGIN( sheduled_task, this->posted_task_list.root, listNode )

            if ( StringEqualToZero( item->taskKey, task_key, true ) )
            {
                item->isCancelled = true;
            }

        LIST_FOREACH_END
    }
}

void MagicParallelTasks::CurrentTaskSetPost( taskFunc_t cb, void *ud, cleanupFunc_t cleanup )
{
    NativeExecutive::CExecThread *currentThread = this->execMan->GetCurrentThread();

    NativeExecutive::CReadWriteWriteContext ctx_setPostTask( this->lock_running_tasks );

    LIST_FOREACH_BEGIN( sheduled_task, this->running_tasks.root, listNode )

        if ( item->sheduledOnThread == currentThread )
        {
            item->post_cb = cb;
            item->post_ud = ud;
            item->post_cleanup = cleanup;
            break;
        }

    LIST_FOREACH_END
}

void MagicParallelTasks::customEvent( QEvent *evt )
{
    MGTXDCustomEvent evtType = (MGTXDCustomEvent)evt->type();

    if ( evtType == MGTXDCustomEvent::ParallelTaskPost )
    {
        MGTXD_ParallelTaskPostEvent *cevt = (MGTXD_ParallelTaskPostEvent*)evt;
        sheduled_task *task = cevt->getTask();

        // Enter post execution.
        {
            NativeExecutive::CReadWriteWriteContext ctx_postExec( task->lock_postExec );

            if ( task->havePostExecHandlersExecuted == false )
            {
                // Post the task.
                if ( task->isCancelled == false )
                {
                    if ( auto post_cb = task->post_cb )
                    {
                        post_cb( task->post_ud );
                    }
                }
                else
                {
                    if ( auto abort_cb = task->abort_cb )
                    {
                        abort_cb( task->abort_ud );
                    }
                }

                task->havePostExecHandlersExecuted = true;
            }
        }

        // Remove any pointers to our task.
        RemoveTaskTokenConnection( task );

        // Remove the post status.
        {
            NativeExecutive::CReadWriteWriteContext <> ctx_removeFromPost( this->lock_posted_task_list );

            LIST_REMOVE( task->listNode );
        }

        // Simply clean up the task.
        CleanupTask( task, false, false );

        return;
    }
}

// *** TOKEN IMPLEMENTATION ***

MagicParallelTasks::TaskToken::TaskToken( MagicParallelTasks *system, sheduled_task *task ) noexcept
{
    this->system = system;
    this->task = task;

    task->attached_token = this;
}

MagicParallelTasks::TaskToken::TaskToken( void ) noexcept
{
    this->system = nullptr;
    this->task = nullptr;
}

void MagicParallelTasks::TaskToken::moveFrom( TaskToken&& right ) noexcept
{
    MagicParallelTasks *system = right.system;

    if ( system == nullptr )
    {
        // if system is nullptr then so is task.
        this->system = nullptr;
        this->task = nullptr;
    }
    else
    {
        NativeExecutive::CReadWriteWriteContext ctx_updateTokenPtr( system->lock_attached_token );

        sheduled_task *task = right.task;

        task->attached_token = this;

        this->system = system;
        this->task = task;

        right.task = nullptr;
    }
}

MagicParallelTasks::TaskToken::TaskToken( TaskToken&& right ) noexcept
{
    this->moveFrom( std::move( right ) );
}

void MagicParallelTasks::TaskToken::deferredDetach( void ) noexcept
{
    MagicParallelTasks *system = this->system;
    sheduled_task *task = this->task;

    if ( system != nullptr && task != nullptr )
    {
        // It is the responsbility of the system to check whether it still
        // is the system of the token inside this call.
        system->RemoveTaskTokenConnection( task );
    }
}

MagicParallelTasks::TaskToken::~TaskToken( void )
{
    this->deferredDetach();
}

MagicParallelTasks::TaskToken& MagicParallelTasks::TaskToken::operator = ( TaskToken&& right ) noexcept
{
    this->deferredDetach();
    this->moveFrom( std::move( right ) );

    return *this;
}

void MagicParallelTasks::TaskToken::Cancel( bool directlyExecuteAbortHandler )
{
    MagicParallelTasks *system = this->system;

    if ( system == nullptr )
        return;

    NativeExecutive::CReadWriteWriteContext ctx_cancelTaskOfToken( system->lock_attached_token );
    NativeExecutive::CReadWriteReadContext ctx_runningTaskList( system->lock_running_tasks );

    if ( sheduled_task *task = this->task )
    {
        if ( task->systemPtr == system )
        {
            // Enter post execution.
            // This is to prevent any token fetching while the task post/abort handlers could still be running.
            NativeExecutive::CReadWriteWriteContext ctx_postExec( task->lock_postExec );

            if ( task->isCancelled == false )
            {
                if ( NativeExecutive::CExecThread *shedThread = task->sheduledOnThread )
                {
                    shedThread->SetThreadCancelling( true );
                }

                task->isCancelled = true;
            }

            // We know that prior to being posted we have not executed the abort handler.
            // But we want to allow execution of this routine just for cleanup purposes.
            if ( directlyExecuteAbortHandler && task->havePostExecHandlersExecuted == false )
            {
                // Make sure that when we try to post execute in another call to Cancel that it won't work.
                // This is the curse of having recursive locks.
                task->havePostExecHandlersExecuted = true;

                if ( auto abort_cb = task->abort_cb )
                {
                    abort_cb( task->abort_ud );
                }

                // Cleanup is performed later.
            }
        }
    }
}
