/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwthreading.hxx
*  PURPOSE:     RenderWare Threading shared include.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_THREADING_INTERNALS_
#define _RENDERWARE_THREADING_INTERNALS

#ifdef RWLIB_ENABLE_THREADING

#include <NativeExecutive/CExecutiveManager.h>

#endif //RWLIB_ENABLE_THREADING

#include "pluginutil.hxx"

namespace rw
{

#ifdef RWLIB_ENABLE_THREADING

struct threadingEnvironment
{
    inline void Initialize( Interface *engineInterface )
    {
        this->nativeMan = NativeExecutive::CExecutiveManager::Create();

        // Must not be optional.
        assert( this->nativeMan != nullptr );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        if ( NativeExecutive::CExecutiveManager *nativeMan = this->nativeMan )
        {
            NativeExecutive::CExecutiveManager::Delete( nativeMan );

            this->nativeMan = nullptr;
        }
    }

    NativeExecutive::CExecutiveManager *nativeMan;   // NativeExecutive library handle.
};

typedef PluginDependantStructRegister <threadingEnvironment, RwInterfaceFactory_t> threadingEnvRegister_t;

extern optional_struct_space <threadingEnvRegister_t> threadingEnv;

// Quick function to return the native executive.
inline NativeExecutive::CExecutiveManager* GetNativeExecutive( const EngineInterface *engineInterface )
{
    const threadingEnvironment *threadEnv = threadingEnv.get().GetConstPluginStruct( engineInterface );

    if ( threadEnv )
    {
        return threadEnv->nativeMan;
    }

    return nullptr;
}

#endif //RWLIB_ENABLE_THREADING

// Plugin struct for either per-thread or global data, depending on configuration of magic-rw.
template <typename structType, bool isDependantStruct = false>
struct perThreadDataRegister
{
    inline void Initialize( EngineInterface *engineInterface )
    {
#ifdef RWLIB_ENABLE_THREADING
        // Register the per-thread warning handler environment.
        NativeExecutive::CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

        if ( nativeMan )
        {
            // Since we are in initialization phase there must not be any meaningful thread created.
            // So blast away any dandling thread handles, ahoi.
            nativeMan->PurgeActiveThreads();

            pluginRegister.RegisterPlugin( nativeMan );
        }
#else
        if constexpr ( isDependantStruct )
        {
            globalData.Initialize( engineInterface );
        }
#endif //RWLIB_ENABLE_THREADING
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
#ifdef RWLIB_ENABLE_THREADING
        // Unregister the thread env, if registered.
        NativeExecutive::CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

        if ( nativeMan )
        {
            pluginRegister.UnregisterPlugin();
        }
#else
        if constexpr ( isDependantStruct )
        {
            globalData.Shutdown( engineInterface );
        }
#endif //RWLIB_ENABLE_THREADING
    }

    inline structType* GetCurrentPluginStruct( void )
    {
#ifdef RWLIB_ENABLE_THREADING
        return pluginRegister.GetPluginStructCurrent();
#else
        return &globalData;
#endif //RWLIB_ENABLE_THREADING
    }

private:
#ifdef RWLIB_ENABLE_THREADING
    NativeExecutive::execThreadStructPluginRegister <structType, isDependantStruct> pluginRegister;
#else
    structType globalData;
#endif //RWLIB_ENABLE_THREADING
};

// Private API.
void ThreadingMarkAsTerminating( EngineInterface *engineInterface );
void PurgeActiveThreadingObjects( EngineInterface *engineInterface );

}; // namespace rw

#endif //_RENDERWARE_THREADING_INTERNALS_