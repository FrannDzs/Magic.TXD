/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/mainwindow.asynclog.h
*  PURPOSE:     Header of the editor async log for threads.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef _MAINWINDOW_ASYNCLOG_HEADER_
#define _MAINWINDOW_ASYNCLOG_HEADER_

#include <QtCore/QEvent>

#include "txdlog.h"

struct MGTXDAsyncLogEvent : public QEvent
{
    inline MGTXDAsyncLogEvent( QString msg, eLogMessageType msgType ) : QEvent( (QEvent::Type)MGTXDCustomEvent::AsyncLogEvent ),
        msg( std::move( msg ) ), msgType( msgType )
    {
        return;
    }

    inline QString getMessage( void ) const
    {
        return this->msg;
    }

    inline eLogMessageType getMessageType( void ) const
    {
        return this->msgType;
    }

private:
    QString msg;
    eLogMessageType msgType;
};

#endif //_MAINWINDOW_ASYNCLOG_HEADER_