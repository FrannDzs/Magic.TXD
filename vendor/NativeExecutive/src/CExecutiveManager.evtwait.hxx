/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.evtwait.cpp
*  PURPOSE:     Per-thread wait event registration
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NAT_EXEC_INTERNAL_WAIT_EVENT_REGISTRATION_
#define _NAT_EXEC_INTERNAL_WAIT_EVENT_REGISTRATION_

// Use the public event API to create an internal event.
#include "internal/CExecutiveManager.event.internal.h"

#include "CExecutiveManager.thread.hxx"
#include "PluginUtils.hxx"

BEGIN_NATIVE_EXECUTIVE

struct _threadEventWaiterPlugin
{
    inline void Initialize( CExecThreadImpl *thread )
    {
        pubevent_constructor( this );
    }

    inline void Shutdown( CExecThreadImpl *thread )
    {
        pubevent_destructor( this );
    }

    inline static size_t GetStructSize( CExecutiveManagerNative *natExec )
    {
        return pubevent_get_size();
    }
};

typedef PerThreadPluginRegister <_threadEventWaiterPlugin, true, true> eventWaiterEnvironment;

typedef PluginDependantStructRegister <eventWaiterEnvironment, executiveManagerFactory_t> eventWaiterEnvReg_t;

extern constinit optional_struct_space <eventWaiterEnvReg_t> eventWaiterEnvReg;

inline CEvent* GetCurrentThreadWaiterEvent( CExecutiveManagerNative *manager, CExecThreadImpl *currentThread )
{
    return (CEvent*)ResolveThreadPlugin( eventWaiterEnvReg.get(), manager, currentThread );
}

END_NATIVE_EXECUTIVE

#endif //_NAT_EXEC_INTERNAL_WAIT_EVENT_REGISTRATION_