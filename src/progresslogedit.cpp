/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/progresslogedit.cpp
*  PURPOSE:     Text box that acts as a log.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include "progresslogedit.h"

ProgressLogEdit::ProgressLogEdit( QWidget *parent )
{
    this->parent = parent;

    this->logEdit = nullptr;
}

ProgressLogEdit::~ProgressLogEdit( void )
{
    return;
}

QWidget* ProgressLogEdit::CreateLogWidget( void )
{
    // Create all the things.
    QPlainTextEdit *logEdit = new QPlainTextEdit();

    logEdit->setMinimumWidth( 400 );
    logEdit->setReadOnly( true );

    this->logEdit = logEdit;

    return logEdit;
}

void ProgressLogEdit::postLogMessage( QString msg )
{
    AppendConsoleMessageEvent *evt = new AppendConsoleMessageEvent( std::move( msg ) );

    QCoreApplication::postEvent( this->parent, evt );
}

void ProgressLogEdit::directLogMessage( QString msg )
{
    QPlainTextEdit *logEdit = this->logEdit;

    logEdit->moveCursor( QTextCursor::End );
    logEdit->insertPlainText( msg );
    logEdit->moveCursor( QTextCursor::End );
}

void ProgressLogEdit::customEvent( QEvent *evt )
{
    if ( AppendConsoleMessageEvent *appendMsgEvt = dynamic_cast <AppendConsoleMessageEvent*> ( evt ) )
    {
        this->directLogMessage( std::move( appendMsgEvt->msg ) );

        return;
    }
}