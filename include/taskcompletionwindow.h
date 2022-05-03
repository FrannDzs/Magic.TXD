/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/taskcompletionwindow.h
*  PURPOSE:     Header for the thread task attachable window.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHBoxLayout>
#include <QtCore/QEvent>
#include <QtCore/QTimer>

#include <QtCore/QCoreApplication>

#include "progresslogedit.h"

struct TaskCompletionWindow abstract : public QDialog, public magicTextLocalizationItem
{
    friend struct taskCompletionWindowEnv;
private:
    struct status_msg_update : public QEvent
    {
        inline status_msg_update( QString newMsg ) : QEvent( QEvent::User )
        {
            this->msg = std::move( newMsg );
        }

        QString msg;
    };

    struct task_completion_event : public QEvent
    {
        inline task_completion_event( void ) : QEvent( QEvent::User )
        {
            return;
        }
    };

    static void waiterThread_runtime( rw::thread_t handle, rw::Interface *engineInterface, void *ud )
    {
        TaskCompletionWindow *wnd = (TaskCompletionWindow*)ud;

        rw::thread_t taskThreadHandle = wnd->taskThreadHandle;

        // Simply wait for the other task to finish.
        rw::JoinThread( engineInterface, taskThreadHandle );

        // We are done. Notify the window.
        task_completion_event *completeEvt = new task_completion_event();

        // We specify a priority of one to be above the typical event cast.
        // This makes it possible to trigger very quickly and the timer will be stopped properly.
        QCoreApplication::postEvent( wnd, completeEvt, 1 );
    }

public:
    TaskCompletionWindow( MainWindow *mainWnd, rw::thread_t taskHandle, const char *tokenTitleBar, int showDelayMS, bool isModal = false );
    virtual ~TaskCompletionWindow( void );

    // Access properties.
    inline void setCloseOnCompletion( bool enabled ) noexcept
    {
        this->closeOnCompletion = enabled;
    }

    inline bool getCloseOnCompletion( void ) const noexcept
    {
        return this->closeOnCompletion;
    }

    inline void updateStatusMessage( QString newMessage )
    {
        status_msg_update *evt = new status_msg_update( std::move( newMessage ) );

        QCoreApplication::postEvent( this, evt );
    }

    void customEvent( QEvent *evt ) override
    {
        if ( task_completion_event *completeEvt = dynamic_cast <task_completion_event*> ( evt ) )
        {
            (void)completeEvt;

            // Stop any show timer of the window.
            this->timer_show.stop();

            // We finished!
            if ( this->hasRequestedClosure || this->closeOnCompletion )
            {
                // This means that we can close ourselves.
                this->close();
            }

            // Remember that we completed.
            this->hasCompleted = true;

            return;
        }

        if ( status_msg_update *msgEvt = dynamic_cast <status_msg_update*> ( evt ) )
        {
            // Update our text.
            this->OnMessage( std::move( msgEvt->msg ) );

            return;
        }

        return;
    }

    inline MainWindow* getMainWindow( void ) const
    {
        return this->mainWnd;
    }

public slots:
    void OnRequestCancel( bool checked )
    {
        rw::Interface *rwEngine = this->mainWnd->GetEngine();

        // Attempt to accelerate the closing of the dialog by terminating the task thread.
        rw::TerminateThread( rwEngine, this->taskThreadHandle, false );

        // Make sure that we close if the thread has completed by now.
        this->hasRequestedClosure = true;

        // If we have completed already, we can close ourselves.
        if ( this->hasCompleted )
        {
            this->close();
        }
    }

protected:
    void updateContent( MainWindow *mainWnd ) override;

    virtual void OnMessage( QString msg ) = 0;

private:
    MainWindow *mainWnd;

    rw::thread_t taskThreadHandle;
    rw::thread_t waitThreadHandle;

    RwListEntry <TaskCompletionWindow> node;

    bool hasRequestedClosure;
    bool closeOnCompletion;
    bool hasCompleted;

    const char *tokenTitleBar;

    // Only show us after some time has passed.
    QTimer timer_show;

protected:
    QLayout *logAreaLayout;
};

struct LabelTaskCompletionWindow : public TaskCompletionWindow
{
    LabelTaskCompletionWindow( MainWindow *mainWnd, rw::thread_t taskHandle, const char *tokenTitleBar, const char *tokenStartMsg, int showDelayMS, bool isModal = false );
    ~LabelTaskCompletionWindow( void );

protected:
    void updateContent( MainWindow *mainWnd ) override;

    void OnMessage( QString msg ) override;

private:
    QLabel *statusMessageLabel;

    bool hasStartMessage;
    const char *tokenStartMsg;
};

struct LogTaskCompletionWindow : public TaskCompletionWindow
{
    LogTaskCompletionWindow( MainWindow *mainWnd, rw::thread_t taskHandle, const char *tokenTitleBar, QString statusMsg, int showDelayMS, bool isModal = false );
    ~LogTaskCompletionWindow( void );

protected:
    void OnMessage( QString msg ) override;

private:
    ProgressLogEdit logEditControl;
};
