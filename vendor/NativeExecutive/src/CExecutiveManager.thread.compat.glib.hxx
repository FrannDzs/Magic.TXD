/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.compat.glib.hxx
*  PURPOSE:     Linux GCC libc compatibility layer for threads
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _GLIBC_COMPAT_LAYER_
#define _GLIBC_COMPAT_LAYER_

#ifdef __linux__

#include "internal/CExecutiveManager.internal.h"

BEGIN_NATIVE_EXECUTIVE

void _threadsupp_glibc_enter_thread( CExecThreadImpl *current_thread );
void _threadsupp_glibc_leave_thread( CExecThreadImpl *current_thread );

void _threadsupp_glibc_register( void );
void _threadsupp_glibc_unregister( void );

void* _threadsupp_glibc_get_tcb( CExecThreadImpl *thread );

END_NATIVE_EXECUTIVE

#endif //__linux__

#endif //_GLIBC_COMPAT_LAYER_
