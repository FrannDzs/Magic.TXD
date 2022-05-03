/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.cond.hxx
*  PURPOSE:     Conditional variable environment internals
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_CONDITIONAL_VARIABLE_ENV_HEADER_
#define _NATEXEC_CONDITIONAL_VARIABLE_ENV_HEADER_

#include "internal/CExecutiveManager.cond.internal.h"
#include "CExecutiveManager.hazards.h"

#include "CExecutiveManager.thread.hxx"

BEGIN_NATIVE_EXECUTIVE

struct condVarNativeEnv
{
    struct condVarThreadPlugin : public hazardPreventionInterface
    {
        // We need to be a information node for conditional variables to know
        // that we are waiting for signals. This is required in user-space to
        // cleanly serve termination logic.

        void Initialize( CExecThreadImpl *thread );
        void Shutdown( CExecThreadImpl *thread );

        void TerminateHazard( void ) noexcept override;

        // We use the threadState lock to prevent premature interruption of hazard init.
        // Initialization entails pushing the hazard on the stack and getting
        // into wait-state. Init has to be atomic.

        CCondVarImpl *waitingOnVar;
        perThreadCondVarRegistration condRegister;
    };

    executiveManagerFactory_t::pluginOffset_t _condVarThreadPluginOffset;

    inline void Initialize( CExecutiveManagerNative *nativeExec )
    {
        privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeExec );

        assert( threadEnv != nullptr );

        // Register a plugin for each thread that holds synchronization object maintenance.
        this->_condVarThreadPluginOffset = threadEnv->threadPlugins.RegisterDependantStructPlugin <condVarThreadPlugin> ();
    }

    inline void Shutdown( CExecutiveManagerNative *nativeExec )
    {
        privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeExec );

        assert( threadEnv != nullptr );

        // Terminate our plugins.
        if ( privateThreadEnvironment::threadPluginContainer_t::IsOffsetValid( this->_condVarThreadPluginOffset ) )
        {
            threadEnv->threadPlugins.UnregisterPlugin( this->_condVarThreadPluginOffset );

            this->_condVarThreadPluginOffset = privateThreadEnvironment::threadPluginContainer_t::INVALID_PLUGIN_OFFSET;
        }
    }

    inline condVarThreadPlugin* GetThreadCondEnv( CExecThreadImpl *thread ) const
    {
        return privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <condVarThreadPlugin> ( thread, this->_condVarThreadPluginOffset );
    }

    inline CExecThreadImpl* BackResolveThread( condVarThreadPlugin *pluginObj )
    {
        return privateThreadEnvironment::threadPluginContainer_t::BACK_RESOLVE_STRUCT( pluginObj, this->_condVarThreadPluginOffset );
    }
};

typedef PluginDependantStructRegister <condVarNativeEnv, executiveManagerFactory_t> condNativeEnvRegister_t;

extern constinit optional_struct_space <condNativeEnvRegister_t> condNativeEnvRegister;

END_NATIVE_EXECUTIVE

#endif //_NATEXEC_CONDITIONAL_VARIABLE_ENV_HEADER_