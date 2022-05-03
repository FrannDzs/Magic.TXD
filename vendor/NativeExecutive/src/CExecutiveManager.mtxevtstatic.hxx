/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.mtxevtstatic.hxx
*  PURPOSE:     Global static-env mutex with event.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _NATEXEC_MUTEX_EVENT_STATIC_HEADER_
#define _NATEXEC_MUTEX_EVENT_STATIC_HEADER_

#include "internal/CExecutiveManager.internal.h"
#include "internal/CExecutiveManager.event.internal.h"

BEGIN_NATIVE_EXECUTIVE

// To use this struct you must first register the event subsystem.
struct mutexWithEventStatic
{
    inline void Initialize( void )
    {
        size_t event_start_off = GetEventStartOff();

        size_t event_size = pubevent_get_size();

        size_t requiredSize = ( event_start_off + event_size );

        FATAL_ASSERT( requiredSize <= sizeof(data) );

        void *evt_mem = data + event_start_off;

        pubevent_constructor( evt_mem );

        CEvent *evt = (CEvent*)evt_mem;

        new (data) CUnfairMutexImpl( evt );
    }

    inline void Shutdown( void )
    {
        CUnfairMutexImpl *mutex = (CUnfairMutexImpl*)this;

        mutex->~CUnfairMutexImpl();

        CEvent *evt = (CEvent*)( data + GetEventStartOff() );

        pubevent_destructor( evt );
    }

    inline CUnfairMutexImpl* GetMutex( void )
    {
        return (CUnfairMutexImpl*)this->data;
    }

private:
    static AINLINE size_t GetEventStartOff( void )
    {
        size_t event_alignment = pubevent_get_alignment();

        size_t event_start_off = ALIGN_SIZE( sizeof(CUnfairMutexImpl), event_alignment );

        return event_start_off;
    }

    // Opaque data for the unfair mutex + wait event.
    char data[ MAX_STATIC_SYNC_STRUCT_SIZE * 2 ] = {};
};

END_NATIVE_EXECUTIVE

#endif //_NATEXEC_MUTEX_EVENT_STATIC_HEADER_
