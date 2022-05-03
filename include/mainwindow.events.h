/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/mainwindow.events.h
*  PURPOSE:     Header of the Qt event handling.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef _MAINWINDOW_CUSTOM_EVENTS_HEADER_
#define _MAINWINDOW_CUSTOM_EVENTS_HEADER_

#include <type_traits>

#include <renderware.h>

#include <QtCore/QEvent>

enum class MGTXDCustomEvent : std::underlying_type <QEvent::Type>::type
{
    AsyncLogEvent = QEvent::User + 1,
    TaskProcessingBegin,
    TaskProcessingEnd,
    StartTask,
    EndTask,
    ParallelTaskPost
};

// Triggered when the task management system has started execution, woken up from it's slumber.
struct MGTXD_TaskProcessingBeginEvent : public QEvent
{
    inline MGTXD_TaskProcessingBeginEvent( void ) : QEvent( (QEvent::Type)MGTXDCustomEvent::TaskProcessingBegin )
    {
        return;
    }
};

// Triggered when the task management system has finished execution of all currently
// sheduled tasks.
struct MGTXD_TaskProcessingEndEvent : public QEvent
{
    inline MGTXD_TaskProcessingEndEvent( void ) : QEvent( (QEvent::Type)MGTXDCustomEvent::TaskProcessingEnd )
    {
        return;
    }
};

#endif //_MAINWINDOW_CUSTOM_EVENTS_HEADER_