/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/progresslogedit.h
*  PURPOSE:     Multi-line edit that is capable of asynchronous input.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/qplaintextedit.h>

struct AppendConsoleMessageEvent : public QEvent
{
    inline AppendConsoleMessageEvent( QString msg ) : QEvent( QEvent::User )
    {
        this->msg = std::move( msg );
    }

    QString msg;
};

struct ProgressLogEdit
{
    ProgressLogEdit( QWidget *parent );
    ~ProgressLogEdit( void );

    QWidget* CreateLogWidget( void );

    void postLogMessage( QString msg );
    void directLogMessage( QString msg );   // call from GUI thread only.

    void customEvent( QEvent *evt );

private:
    QWidget *parent;

    QPlainTextEdit *logEdit;
};
