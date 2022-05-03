/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.hxx
*  PURPOSE:     Thread environment internals
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_THREAD_ENVIRONMENT_HEADER_
#define _NATEXEC_THREAD_ENVIRONMENT_HEADER_

#include <sdk/PluginHelpers.h>
#include <sdk/Vector.h>

#include "CExecutiveManager.eventplugin.hxx"
#include "internal/CExecutiveManager.unfairmtx.internal.h"
#include "internal/CExecutiveManager.thread.internal.h"

BEGIN_NATIVE_EXECUTIVE

extern constinit optional_struct_space <EventPluginRegister> privateThreadEnvThreadReferenceLockEventRegister;
extern constinit optional_struct_space <EventPluginRegister> privateThreadEnvThreadPluginsLockEventRegister;

struct privateThreadEnvironment
{
    inline privateThreadEnvironment( CExecutiveManagerNative *natExec ) :
        cleanups( nullptr, 0, natExec ),
        threadPlugins( eir::constr_with_alloc::DEFAULT, natExec ),
        mtxThreadReferenceLock( privateThreadEnvThreadReferenceLockEventRegister.get().GetEvent( natExec ) ),
        mtxThreadPluginsLock( privateThreadEnvThreadPluginsLockEventRegister.get().GetEvent( natExec ) )
    {
        return;
    }

    inline void Initialize( CExecutiveManagerNative *manager )
    {
        return;
    }

    inline void Shutdown( CExecutiveManagerNative *manager )
    {
        return;
    }

    typedef StaticPluginClassFactory <CExecThreadImpl, NatExecStandardObjectAllocator> threadPluginContainer_t;

    eir::Vector <cleanupInterface*, NatExecStandardObjectAllocator> cleanups;   // executed before thread descriptor destruction.

    threadPluginContainer_t threadPlugins;

    CUnfairMutexImpl mtxThreadReferenceLock;
    CUnfairMutexImpl mtxThreadPluginsLock;
};

typedef PluginDependantStructRegister <privateThreadEnvironment, executiveManagerFactory_t> privateThreadEnvRegister_t;

extern constinit optional_struct_space <privateThreadEnvRegister_t> privateThreadEnv;

END_NATIVE_EXECUTIVE

#endif //_NATEXEC_THREAD_ENVIRONMENT_HEADER_