/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.asynclog.cpp
*  PURPOSE:     Log messages posted from other threads.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

void MainWindow::asyncLogEvent( MGTXDAsyncLogEvent *evt )
{
    eLogMessageType msgType = evt->getMessageType();

    if ( msgType == LOGMSG_ERROR )
    {
        this->txdLog->showError( evt->getMessage() );
    }
    else
    {
        this->txdLog->addLogMessage( evt->getMessage(), msgType );

        if ( msgType == LOGMSG_WARNING )
        {
            this->txdLog->show();
        }
    }
}

void MainWindow::asyncLog( QString msg, eLogMessageType msgType )
{
    MGTXDAsyncLogEvent *evt = new MGTXDAsyncLogEvent( std::move( msg ), msgType );

    QCoreApplication::postEvent( this, evt );
}