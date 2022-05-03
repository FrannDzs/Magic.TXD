/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.safety.cpp
*  PURPOSE:     Utilities to not lose data.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

// Code related to saving changes in critical situations.
// Here is the code for save-changes-before-closing, and stuff.

#include "mainwindow.h"

#include "taskcompletionwindow.h"

#include <QtCore/QEventLoop>

void MainWindow::ModifiedStateBarrier( bool blocking, modifiedEndCallback_t cb )
{
    rw::Interface *rwEngine = this->GetEngine();

    auto action_promptSave = [this, blocking, cb = std::move(cb)] ( void ) mutable
    {
        // If the current TXD was modified, we maybe want to save changes to
        // disk before proceeding.

        struct SaveChangesDialog : public QDialog, public magicTextLocalizationItem
        {
            inline SaveChangesDialog( MainWindow *mainWnd, modifiedEndCallback_t endCB ) : QDialog( mainWnd )
            {
                this->mainWnd = mainWnd;
                this->postCallback = std::move( endCB );

                // We are a modal dialog.
                setWindowModality( Qt::WindowModal );
                setWindowFlags( ( windowFlags() | Qt::WindowStaysOnTopHint ) & ~Qt::WindowContextHelpButtonHint );
                setAttribute( Qt::WA_DeleteOnClose );

                // This dialog consists of a warning message and selectable
                // options to the user, typically buttons.

                QVBoxLayout *rootLayout = new QVBoxLayout( this );

                rootLayout->addWidget( CreateLabelL( "Main.SavChange.Warn" ) );

                rootLayout->addSpacing( 15 );

                // Now for the button.
                QHBoxLayout *buttonRow = new QHBoxLayout( this );

                buttonRow->setAlignment( Qt::AlignCenter );

                QPushButton *saveAcceptButton = CreateButtonL( "Main.SavChange.Save" );

                saveAcceptButton->setDefault( true );

                this->buttonSave = saveAcceptButton;

                connect( saveAcceptButton, &QPushButton::clicked, this, &SaveChangesDialog::onRequestSave );

                buttonRow->addWidget( saveAcceptButton );

                QPushButton *notSaveButton = CreateButtonL( "Main.SavChange.Ignore" );

                this->buttonIgnore = notSaveButton;

                connect( notSaveButton, &QPushButton::clicked, this, &SaveChangesDialog::onRequestIgnore );

                buttonRow->addWidget( notSaveButton );

                QPushButton *cancelButton = CreateButtonL( "Main.SavChange.Cancel" );

                connect( cancelButton, &QPushButton::clicked, this, &SaveChangesDialog::onRequestCancel );

                buttonRow->addWidget( cancelButton );

                rootLayout->addLayout( buttonRow );

                this->setLayout( rootLayout );

                RegisterTextLocalizationItem( this );
            }

            ~SaveChangesDialog( void )
            {
                UnregisterTextLocalizationItem( this );

                // Make sure that any saving action has finished.
                // This is because the action could have pointers to our UI.
                this->token_action_save.Cancel( true );

                // TODO.
            }

            void updateContent( MainWindow *mainWnd ) override
            {
                setWindowTitle( MAGIC_TEXT( "Main.SavChange.Title" ) );
            }

            inline void terminate( void )
            {
                MainWindow::modifiedEndCallback_t stack_postCallback = std::move( this->postCallback );

                // Need to set ourselves invisible already, because Qt is event based and it wont quit here.
                this->setVisible( false );

                // Destroy ourselves.
                this->close();

                // Since we saved, we can just perform the callback now.
                stack_postCallback();
            }

        private:
            void onRequestSave( bool checked )
            {
                // Disable the controls because we are about to start saving.
                this->buttonSave->setDisabled( true );
                this->buttonIgnore->setDisabled( true );

                // If the user successfully saved the changes, we quit.
                this->token_action_save = mainWnd->performSaveTXD(
                    [this]
                    {
                        terminate();
                    },
                    [this]
                    {
                        // Saving has failed, re-enable the controls.
                        this->buttonSave->setDisabled( false );
                        this->buttonIgnore->setDisabled( false );
                    }
                );
            }

            void onRequestIgnore( bool checked )
            {
                // The user probably does not care anymore.
                this->mainWnd->ClearModifiedState();

                terminate();
            }

            void onRequestCancel( bool checked )
            {
                // We really do not want to do anything.
                this->close();
            }

            MainWindow *mainWnd;

            MainWindow::modifiedEndCallback_t postCallback;

            QPushButton *buttonSave;
            QPushButton *buttonIgnore;

            MagicActionSystem::ActionToken token_action_save;
        };

        bool hasPostponedExec = false;

        if ( this->currentTXD != nullptr )
        {
            if ( this->wasTXDModified )
            {
                // We want to postpone activity.
                SaveChangesDialog *saveDlg = new SaveChangesDialog( this, std::move( cb ) );

                if ( blocking )
                {
                    saveDlg->exec();
                }
                else
                {
                    saveDlg->setVisible( true );
                }

                hasPostponedExec = true;
            }
        }

        if ( !hasPostponedExec )
        {
            // Nothing has happened specially, so we just execute here.
            cb();
        }
    };

    // Make sure that all running actions have finished so far.
    rw::thread_t execThread = rw::MakeThreadL(
        rwEngine,
        [this, action_promptSave = std::move(action_promptSave)] ( rw::thread_t thread ) mutable
        {
            // First wait for all actions to finish.
            this->actionSystem.WaitForActionQueueCompletion();

            // Then just push an action with the save stuff.
            this->actionSystem.LaunchPostOnlyActionL(
                eMagicTaskType::UNSPECIFIED, nullptr,
                std::move( action_promptSave )
            );
        }
    );

    assert( execThread != nullptr );

    // Just kick it off already.
    rw::ResumeThread( rwEngine, execThread );

    LabelTaskCompletionWindow *waitWnd;

    try
    {
        waitWnd = new LabelTaskCompletionWindow( this, execThread, "ModifiedState.WaitingTitle", "ModifiedState.WaitingText", 150, true );
    }
    catch( ... )
    {
        rw::CloseThread( rwEngine, execThread );

        throw;
    }

    waitWnd->setCloseOnCompletion( true );

    if ( blocking )
    {
        QEventLoop loop;
        connect( waitWnd, &QDialog::destroyed, &loop, &QEventLoop::quit );

        loop.exec();
    }
}
