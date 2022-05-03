/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/PluginUtils.hxx
*  PURPOSE:     Interesting plugin utilities, like per-thread plugins
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NAT_EXEC_PLUGIN_UTILITIES_INTERNAL_
#define _NAT_EXEC_PLUGIN_UTILITIES_INTERNAL_

#include "CExecutiveManager.thread.hxx"

BEGIN_NATIVE_EXECUTIVE

template <typename structType, bool isDependantStruct = true, bool hasDifficultSize = false, bool hasDifficultAlignment = false>
struct PerThreadPluginRegister
{
    inline void Initialize( CExecutiveManagerNative *nativeMan )
    {
        auto threadPluginOff = privateThreadEnvironment::threadPluginContainer_t::INVALID_PLUGIN_OFFSET;

        if ( privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan ) )
        {
            size_t structSize = sizeof(structType);

            if constexpr ( hasDifficultSize )
            {
                structSize = structType::GetStructSize( nativeMan );
            }

            size_t structAlignment = alignof(structType);

            if constexpr ( hasDifficultAlignment )
            {
                structAlignment = structType::GetStructAlignment( nativeMan );
            }

            if constexpr ( isDependantStruct )
            {
                threadPluginOff = threadEnv->threadPlugins.RegisterDependantStructPlugin <structType> (
                    privateThreadEnvironment::threadPluginContainer_t::ANONYMOUS_PLUGIN_ID, structSize, structAlignment
                );
            }
            else
            {
                threadPluginOff = threadEnv->threadPlugins.RegisterStructPlugin <structType> (
                    privateThreadEnvironment::threadPluginContainer_t::ANONYMOUS_PLUGIN_ID, structSize, structAlignment
                );
            }
        }

        this->threadPluginOff = threadPluginOff;
    }

    inline void Shutdown( CExecutiveManagerNative *nativeMan )
    {
        if ( privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan ) )
        {
            auto threadPluginOff = this->threadPluginOff;

            if ( privateThreadEnvironment::threadPluginContainer_t::IsOffsetValid( threadPluginOff ) )
            {
                threadEnv->threadPlugins.UnregisterPlugin( threadPluginOff );
            }
        }
    }

    inline structType* GetThreadPlugin( CExecThreadImpl *nativeThread )
    {
        return privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <structType> ( nativeThread, this->threadPluginOff );
    }

    inline CExecThreadImpl* BackResolveThread( structType *thePlugin )
    {
        return privateThreadEnvironment::threadPluginContainer_t::BACK_RESOLVE_STRUCT( thePlugin, this->threadPluginOff );
    }

    privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t threadPluginOff;
};

// Helper.
template <typename structType, bool isDependantStruct, bool hasDifficultSize>
inline structType* ResolveThreadPlugin( PluginDependantStructRegister <PerThreadPluginRegister <structType, isDependantStruct, hasDifficultSize>, executiveManagerFactory_t>& ptd_register, CExecutiveManagerNative *nativeMan, CExecThreadImpl *curThread )
{
    auto *ptd_env = ptd_register.GetPluginStruct( nativeMan );

    if ( ptd_env == nullptr )
        return nullptr;

    return ptd_env->GetThreadPlugin( curThread );
}

template <typename structType, bool isDependantStruct, bool hasDifficultSize>
inline CExecThreadImpl* BackResolveThreadPlugin( PluginDependantStructRegister <PerThreadPluginRegister <structType, isDependantStruct, hasDifficultSize>, executiveManagerFactory_t>& ptd_register, CExecutiveManagerNative *nativeMan, structType *thePlugin )
{
    auto *ptd_env = ptd_register.GetPluginStruct( nativeMan );

    if ( ptd_env == nullptr )
        return nullptr;

    return ptd_env->BackResolveThread( thePlugin );
}

END_NATIVE_EXECUTIVE

#endif //_NAT_EXEC_PLUGIN_UTILITIES_INTERNAL_
