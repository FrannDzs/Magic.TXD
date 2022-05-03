/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/mainwindow.parallel.h
*  PURPOSE:     Header of the parallel tasks system.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef _MAINWINDOW_PARALLEL_ACTIVITY_HEADER_
#define _MAINWINDOW_PARALLEL_ACTIVITY_HEADER_

#include <atomic>

#include <QtCore/QObject>

#include <NativeExecutive/CExecutiveManager.h>

struct MagicParallelTasks : public QObject
{
    typedef void (*taskFunc_t)( void *ud );
    typedef void (*cleanupFunc_t)( void *ud ) noexcept;

    MagicParallelTasks( NativeExecutive::CExecutiveManager *execMan );
    ~MagicParallelTasks( void );

private:
    // Forward declaration.
    struct sheduled_task;

public:
    // Tokens which are handed out and point to sheduled, running or posting tasks.
    // The connection to the task is deleted as soon as it has posted.
    // For now there can only be one task token per sheduled task.
    // Please note that concurrent threads cannot access the same token (if not the
    // task system itself).
    struct TaskToken
    {
        friend struct MagicParallelTasks;

    private:
        TaskToken( MagicParallelTasks *system, sheduled_task *task ) noexcept;
    public:
        TaskToken( void ) noexcept;
        TaskToken( TaskToken&& right ) noexcept;
        TaskToken( const TaskToken& ) = delete;
        ~TaskToken( void );

        TaskToken& operator = ( TaskToken&& right ) noexcept;
        TaskToken& operator = ( const TaskToken& ) = delete;

        // Detach method is not possible because it would
        // incite an illegal system pattern that violates
        // the philosopher's fork grabbing rule.

        // Ability to cancel a task.
        void Cancel( bool directlyExecuteAbortHandler = false );

    private:
        void deferredDetach( void ) noexcept;
        void moveFrom( TaskToken&& right ) noexcept;

        std::atomic <MagicParallelTasks*> system;
        std::atomic <sheduled_task*> task;
    };

    // Sheduling of tasks.
    TaskToken LaunchTask(
        const char *task_key,
        taskFunc_t cb, void *ud, cleanupFunc_t cleanup,
        taskFunc_t abort_cb = nullptr, void *abort_ud = nullptr, cleanupFunc_t abort_cleanup = nullptr
    );
    void CancelTasksByKey( const char *task_key );

    // Set a special function to be executed on-post.
    void CurrentTaskSetPost( taskFunc_t cb, void *ud, cleanupFunc_t cleanup );

private:
    // *** LAMBDA HELPERS ***
    template <typename callbackType>
    static void _LambdaTaskRuntimeCB( void *ud )
    {
        callbackType *rem_cb = (callbackType*)ud;

        (*rem_cb)();
    }

    template <typename callbackType>
    static void _LambdaTaskCleanupCB( void *ud ) noexcept
    {
        callbackType *rem_cb = (callbackType*)ud;

        delete rem_cb;
    }

public:
    template <typename callbackType>
    inline TaskToken LaunchTaskL( const char *task_key, callbackType&& cb )
    {
        callbackType *rem_cb = new callbackType( std::move( cb ) );

        try
        {
            return this->LaunchTask( task_key,
                _LambdaTaskRuntimeCB <callbackType>,
                rem_cb,
                _LambdaTaskCleanupCB <callbackType>
            );
        }
        catch( ... )
        {
            delete rem_cb;

            throw;
        }
    }

    template <typename callbackType, typename abortCallbackType>
    inline TaskToken LaunchTaskL( const char *task_key, callbackType&& cb, abortCallbackType&& abort_cb )
    {
        callbackType *rem_cb = new callbackType( std::move( cb ) );

        try
        {
            abortCallbackType *rem_abort_cb = new abortCallbackType( std::move( abort_cb ) );

            try
            {
                return this->LaunchTask( task_key,
                    _LambdaTaskRuntimeCB <callbackType>,
                    rem_cb,
                    _LambdaTaskCleanupCB <callbackType>,
                    _LambdaTaskRuntimeCB <abortCallbackType>,
                    rem_abort_cb,
                    _LambdaTaskCleanupCB <abortCallbackType>
                );
            }
            catch( ... )
            {
                delete rem_abort_cb;

                throw;
            }
        }
        catch( ... )
        {
            delete rem_cb;

            throw;
        }
    }

    template <typename callbackType>
    inline void CurrentTaskSetPostL( callbackType&& cb )
    {
        callbackType *rem_cb = new callbackType( std::move( cb ) );

        try
        {
            this->CurrentTaskSetPost(
                _LambdaTaskRuntimeCB <callbackType>,
                rem_cb,
                _LambdaTaskCleanupCB <callbackType>
            );
        }
        catch( ... )
        {
            delete rem_cb;

            throw;
        }
    }

protected:
    void customEvent( QEvent *evt ) override;

private:
    void RemoveTaskTokenConnection( sheduled_task *task );
    void CleanupTask( sheduled_task *task, bool removeFromNode, bool cleanup_run ) noexcept;

    NativeExecutive::CExecutiveManager *execMan;

    // Allocated threads that are available for execution.
    struct executorThread_t
    {
        NativeExecutive::CExecThread *threadHandle;
    };

    rw::rwStaticVector <executorThread_t> available_threads;

    // Task queue that threads take from.
    struct sheduled_task
    {
        // Need-to-keep meta-data.
        MagicParallelTasks *systemPtr;

        // Sheduler runtime data.
        NativeExecutive::CExecThread *sheduledOnThread;
        const char *taskKey;
        bool isCancelled;

        // Processed when the task should run.
        taskFunc_t run_cb;
        void *run_ud;
        cleanupFunc_t run_cleanup;

        // Processed when the task should post.
        taskFunc_t post_cb;
        void *post_ud;
        cleanupFunc_t post_cleanup;

        // Processed after the task was cancelled.
        taskFunc_t abort_cb;
        void *abort_ud;
        cleanupFunc_t abort_cleanup;

        bool havePostExecHandlersExecuted;
        NativeExecutive::CThreadReentrantReadWriteLock *lock_postExec;

        RwListEntry <sheduled_task> listNode;

        // Token support.
        TaskToken *attached_token;
    };
    
    RwList <sheduled_task> sheduled_task_queue;
    NativeExecutive::CReadWriteLock *lock_sheduled_task_queue;
    NativeExecutive::CCondVar *condWaitSheduledTask;

    RwList <sheduled_task> posted_task_list;
    NativeExecutive::CReadWriteLock *lock_posted_task_list;

    // List of running tasks.
    RwList <sheduled_task> running_tasks;
    NativeExecutive::CThreadReentrantReadWriteLock *lock_running_tasks;

    // Token support.
    NativeExecutive::CThreadReentrantReadWriteLock *lock_attached_token;

    //=================
    // *** EVENTS ***
    //=================
    
    // Triggered when a parallel task of this task system should be posted.
    struct MGTXD_ParallelTaskPostEvent : public QEvent
    {
        inline MGTXD_ParallelTaskPostEvent( sheduled_task *task ) : QEvent( (QEvent::Type)MGTXDCustomEvent::ParallelTaskPost )
        {
            this->task = task;
        }

        inline sheduled_task* getTask( void ) const
        {
            return this->task;
        }

    private:
        sheduled_task *task;
    };
};

#endif //_MAINWINDOW_PARALLEL_ACTIVITY_HEADER_