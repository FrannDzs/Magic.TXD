/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.event.internal.h
*  PURPOSE:     Internal header of the (waitable) event object
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

// The problem with events is that there are many ways to implement them and also multiple
// ones on one platform, depending on OS version, with well-deserving upgrade coins.
// So basically we decide on the way during library boot and stick with it, attaching the data
// as plugin.

#ifndef _NAT_EXEC_EVENT_INTERNAL_HEADER_
#define _NAT_EXEC_EVENT_INTERNAL_HEADER_

#include "CExecutiveManager.event.h"

BEGIN_NATIVE_EXECUTIVE

// Export the public event API.
extern bool pubevent_is_available( void );
extern size_t pubevent_get_size( void );
extern size_t pubevent_get_alignment( void );
extern void pubevent_constructor( void *mem, bool shouldWait = false );
extern void pubevent_destructor( void *mem );
extern void pubevent_set( void *mem, bool shouldWait );
extern void pubevent_wait( void *mem );
extern void pubevent_wait_timed( void *mem, unsigned int msTimeout );

END_NATIVE_EXECUTIVE

#endif //_NAT_EXEC_EVENT_INTERNAL_HEADER_