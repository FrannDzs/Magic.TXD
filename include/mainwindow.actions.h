/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/mainwindow.actions.h
*  PURPOSE:     Header of the editor action system.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <renderware.h>

#include <NativeExecutive/CExecutiveManager.h>

#include <condition_variable>

#include <QtCore/QEvent>
#include <QtCore/QObject>

#include "languages.h"

enum class eMagicTaskType
{
    UNSPECIFIED,
    NEW_TXD,
    LOAD_TXD,
    SAVE_TXD,
    CLOSE_TXD,
    ADD_TEXTURE,
    REPLACE_TEXTURE,
    DELETE_TEXTURE,
    RENAME_TEXTURE,
    RESIZE_TEXTURE,
    MANAGE_TEXTURE,
    GENERATE_MIPMAPS,
    CLEAR_MIPMAPS,
    SET_RENDER_PROPS,
    SET_TXD_VERSION,
    EXPORT_SINGLE,
    EXPORT_ALL
};

// Forward declarations.
struct MagicActionSystem;

struct MagicActionDesc
{
    eMagicTaskType taskType;
    const char *tokenFriendlyTitle;
};

// Forward declarations.
struct MGTXD_StartTaskEvent;
struct MGTXD_EndTaskEvent;

// Actions provider system.
// It allows for multiple tasks to be processed in a batch.
struct MagicActionSystem abstract : public QObject, public magicTextLocalizationItem
{
    MagicActionSystem( NativeExecutive::CExecutiveManager *natExec );
    ~MagicActionSystem( void );

    // Forward declaration.
    struct ActionToken;

private:
    struct MagicAction
    {
        typedef void (*actionRuntime_t)( MagicActionSystem *system, void *ud );
        typedef void (*cleanupFunc_t)( void *ud ) noexcept;

        MagicActionSystem *systemPtr;

        // Code to execute when running.
        actionRuntime_t run_cb;
        void *run_ud;
        cleanupFunc_t run_cleanup;

        // Code to execute at post.
        actionRuntime_t post_cb;
        void *post_ud;
        cleanupFunc_t post_cleanup;

        // Code to execute at abort.
        actionRuntime_t abort_cb;
        void *abort_ud;
        cleanupFunc_t abort_cleanup;

        // Code to execute at cleanup.
        actionRuntime_t finish_cb;
        void *finish_ud;
        cleanupFunc_t finish_cleanup;

        // Set to true if abort and/or finish have executed.
        bool hasExecutedPostExecHandlers;
        NativeExecutive::CReadWriteLock *lock_postExecHandlers;

        // Meta-information.
        MagicActionDesc desc;

        // Runtime state.
        bool isCancelled;

        // Token support.
        ActionToken *attached_token;

        RwListEntry <MagicAction> listNode;
        RwListEntry <MagicAction> activeNode;
    };

public:
    struct ActionToken
    {
        friend struct MagicActionSystem;

    private:
        ActionToken( MagicActionSystem *system, MagicAction *action ) noexcept;
    public:
        ActionToken( void ) noexcept;
        ActionToken( ActionToken&& right ) noexcept;
        ActionToken( const ActionToken& ) = delete;
        ~ActionToken( void );

        ActionToken& operator = ( ActionToken&& right ) noexcept;
        ActionToken& operator = ( const ActionToken& ) = delete;

        // Detach method is not possible because it would
        // incite an illegal system pattern that violates
        // the philosopher's fork grabbing rule.

        // Ability to cancel a task.
        void Cancel( bool directlyExecuteAbortHandlers = false );

    private:
        void deferredDetach( void ) noexcept;
        void moveFrom( ActionToken&& right ) noexcept;

        std::atomic <MagicActionSystem*> system;
        std::atomic <MagicAction*> action;
    };

    ActionToken LaunchAction(
        eMagicTaskType taskType, const char *tokenFriendlyTitle,
        MagicAction::actionRuntime_t cb, void *ud, MagicAction::cleanupFunc_t cleanup,
        MagicAction::actionRuntime_t abort_cb, void *abort_ud, MagicAction::cleanupFunc_t abort_cleanup
    );
    ActionToken LaunchPostOnlyAction(
        eMagicTaskType taskType, const char *tokenFriendlyTitle,
        MagicAction::actionRuntime_t cb, void *ud, MagicAction::cleanupFunc_t cleanup,
        MagicAction::actionRuntime_t abort_cb, void *abort_ud, MagicAction::cleanupFunc_t abort_cleanup
    );

    void CurrentActionSetPost(
        MagicAction::actionRuntime_t cb, void *ud, MagicAction::cleanupFunc_t cleanup
    );
    void CurrentActionSetCleanup(
        MagicAction::actionRuntime_t cb, void *ud, MagicAction::cleanupFunc_t cleanup
    );
    bool CurrentActionCancel( void );

    void WaitForActionQueueCompletion( void );

protected:
    //==================
    // *** EVENTS ***
    //==================

    // Triggered when a task has started execution inside of the task management system.
    struct MGTXD_StartTaskEvent : public QEvent
    {
        inline MGTXD_StartTaskEvent( MagicAction *theAction )
            : QEvent( (QEvent::Type)MGTXDCustomEvent::StartTask ), theAction( theAction )
        {
            return;
        }

        inline MagicAction* getAction( void ) const
        {
            return this->theAction;
        }

    private:
        MagicAction *theAction;
    };

    // Triggered when a task has finished execution.
    struct MGTXD_EndTaskEvent : public QEvent
    {
        inline MGTXD_EndTaskEvent( MagicAction *theAction )
            : QEvent( (QEvent::Type)MGTXDCustomEvent::EndTask ), theAction( theAction )
        {
            return;
        }

        inline MagicAction* getAction( void ) const
        {
            return this->theAction;
        }

    private:
        MagicAction *theAction;
    };

    // Events needing plug-in by the runtime.
    void TaskProcessingBeginEvent( MGTXD_TaskProcessingBeginEvent *evt );
    void TaskProcessingEndEvent( MGTXD_TaskProcessingEndEvent *evt );
    void StartTaskEvent( MGTXD_StartTaskEvent *evt );
    void EndTaskEvent( MGTXD_EndTaskEvent *evt );

    void customEvent( QEvent *evt ) override;
    void updateContent( MainWindow *mainWnd ) override;

    // For clean termination.
    void TerminateSheduling( void );

private:
    void RemoveActionTokenConnection( MagicAction *action );
    void CleanupAction( MagicAction *action, bool remove_node, bool cleanup_run ) noexcept;

    template <typename callbackType>
    AINLINE static void LambdaExecuteActionCB( MagicActionSystem *sys, void *ud )
    {
        callbackType *rem_cb = (callbackType*)ud;

        (*rem_cb)();
    }

    template <typename callbackType>
    AINLINE static void LambdaExecuteCleanupCB( void *ud ) noexcept
    {
        callbackType *rem_cb = (callbackType*)ud;

        delete rem_cb;
    }

    void PublishAction( MagicAction *action );

public:
    template <typename callbackType>
    MagicActionSystem::ActionToken LaunchActionL( eMagicTaskType taskType, const char *tokenFriendlyTitle, callbackType&& cb )
    {
        callbackType *rem_cb = new callbackType( std::move( cb ) );

        try
        {
            return this->LaunchAction(
                taskType, std::move( tokenFriendlyTitle ),
                LambdaExecuteActionCB <callbackType>,
                rem_cb,
                LambdaExecuteCleanupCB <callbackType>,
                nullptr, nullptr, nullptr
            );
        }
        catch( ... )
        {
            delete rem_cb;

            throw;
        }
    }

    template <typename callbackType, typename abortCallbackType>
    MagicActionSystem::ActionToken LaunchActionL( eMagicTaskType taskType, const char *tokenFriendlyTitle, callbackType&& cb, abortCallbackType&& abort_cb )
    {
        callbackType *rem_cb = new callbackType( std::move( cb ) );

        try
        {
            abortCallbackType *rem_abort_cb = new abortCallbackType( std::move( abort_cb ) );

            try
            {
                return this->LaunchAction(
                    taskType, std::move( tokenFriendlyTitle ),
                    LambdaExecuteActionCB <callbackType>,
                    rem_cb,
                    LambdaExecuteCleanupCB <callbackType>,
                    LambdaExecuteActionCB <abortCallbackType>,
                    rem_abort_cb,
                    LambdaExecuteCleanupCB <abortCallbackType>
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
    MagicActionSystem::ActionToken LaunchPostOnlyActionL( eMagicTaskType taskType, const char *tokenFriendlyTitle, callbackType&& cb )
    {
        callbackType *rem_cb = new callbackType( std::move( cb ) );

        try
        {
            return this->LaunchPostOnlyAction(
                taskType, std::move( tokenFriendlyTitle ),
                LambdaExecuteActionCB <callbackType>,
                rem_cb,
                LambdaExecuteCleanupCB <callbackType>,
                nullptr, nullptr, nullptr
            );
        }
        catch( ... )
        {
            delete rem_cb;

            throw;
        }
    }

    template <typename callbackType>
    void CurrentActionSetPostL( callbackType&& cb )
    {
        callbackType *rem_cb = new callbackType( std::move( cb ) );

        try
        {
            this->CurrentActionSetPost(
                LambdaExecuteActionCB <callbackType>,
                rem_cb,
                LambdaExecuteCleanupCB <callbackType>
            );
        }
        catch( ... )
        {
            delete rem_cb;

            throw;
        }
    }

    template <typename callbackType>
    void CurrentActionSetCleanupL( callbackType&& cb )
    {
        callbackType *rem_cb = new callbackType( std::move( cb ) );

        try
        {
            this->CurrentActionSetCleanup(
                LambdaExecuteActionCB <callbackType>,
                rem_cb,
                LambdaExecuteCleanupCB <callbackType>
            );
        }
        catch( ... )
        {
            delete rem_cb;

            throw;
        }
    }

protected:
    // Executed on the same thread.
    virtual void OnStartAction( void ) = 0;
    virtual void OnStopAction( void ) = 0;

    virtual void OnUpdateStatusMessage( const rw::rwStaticString <wchar_t>& statusString ) = 0;

    // Executed on the event dispatcher thread that created this class.
    virtual void OnAsyncStartAction( const MagicActionDesc& desc ) = 0;
    virtual void OnAsyncStopAction( const MagicActionDesc& desc ) = 0;
    virtual void OnAsyncTaskProcessingBegin( void ) = 0;
    virtual void OnAsyncTaskProcessingEnd( void ) = 0;

private:
    NativeExecutive::CExecutiveManager *nativeExec;

    // There is no point in using more than one thread in the task sheduling since
    // we have to obey a queue of tasks. Speedup of actions should be achieved
    // internally in magic-rw.
    NativeExecutive::CExecThread *shedulerThread;

    // List of tasks to be taken.
    NativeExecutive::CReadWriteLock *lockActionQueue;
    NativeExecutive::CCondVar *condHasActions;
    NativeExecutive::CReadWriteLock *lockPostedActions;
    NativeExecutive::CReadWriteLock *lockCurrentTask;

    // List of all alive actions.
    RwList <MagicAction> list_activeActions;
    NativeExecutive::CReadWriteLock *lock_activeActions;
    NativeExecutive::CCondVar *cond_activeActionsModified;

    RwList <MagicAction> list_sheduledActions;
    RwList <MagicAction> list_postedActions;
    MagicAction *running_task;      // pointer to the currently active task.

    // Token support.
    NativeExecutive::CReadWriteLock *lock_attached_token;
};