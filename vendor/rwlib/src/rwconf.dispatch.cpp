/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwconf.dispatch.cpp
*  PURPOSE:     Threaded configuration block implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

// We want to register configuration blocks into the interface and into each thread.
#include "StdInc.h"

#include "rwconf.hxx"

#include "rwthreading.hxx"

#ifdef RWLIB_ENABLE_THREADING

using namespace NativeExecutive;

#endif //RWLIB_ENABLE_THREADING

namespace rw
{

struct rwConfigDispatchEnv
{ 
#ifdef RWLIB_ENABLE_THREADING
    struct perThreadConfigBlock : public threadPluginInterface
    {
        bool OnPluginConstruct( CExecThread *theThread, threadPluginOffset pluginOffset, threadPluginDescriptor pluginId ) override
        {
            void *objMem = theThread->ResolvePluginMemory( pluginOffset );

            if ( !objMem )
                return false;

            cfg_block_constructor constr( this->engineInterface );

            rwConfigBlock *cfgBlock = cfgEnv->configFactory.ConstructPlacementEx( objMem, constr );

            return ( cfgBlock != nullptr );
        }

        void OnPluginDestruct( CExecThread *theThread, threadPluginOffset pluginOffset, threadPluginDescriptor pluginId ) override
        {
            rwConfigBlock *cfgBlock = (rwConfigBlock*)theThread->ResolvePluginMemory( pluginOffset );
            
            if ( !cfgBlock )
                return;

            cfgEnv->configFactory.DestroyPlacement( cfgBlock );
        }

        bool OnPluginAssign( CExecThread *dstThread, const CExecThread *srcThread, threadPluginOffset pluginOffset, threadPluginDescriptor pluginId ) override
        {
            rwConfigBlock *dstBlock = (rwConfigBlock*)dstThread->ResolvePluginMemory( pluginOffset );
            const rwConfigBlock *srcBlock = (const rwConfigBlock*)srcThread->ResolvePluginMemory( pluginOffset );

            return cfgEnv->configFactory.Assign( dstBlock, srcBlock );
        }

        EngineInterface *engineInterface;
        rwConfigEnv *cfgEnv;
    };
    perThreadConfigBlock _perThreadPluginInterface;

    threadPluginOffset _perThreadPluginOffset;
#endif //RWLIB_ENABLE_THREADING

    inline void Initialize( EngineInterface *engineInterface )
    {
        rwConfigEnv *cfgEnv = rwConfigEnvRegister.get().GetPluginStruct( engineInterface );

        this->globalCfg = nullptr;

        if ( cfgEnv )
        {
            cfg_block_constructor constr( engineInterface );

            RwDynMemAllocator memAlloc( engineInterface );

            this->globalCfg = cfgEnv->configFactory.ConstructTemplate( memAlloc, constr );
        }

#ifdef RWLIB_ENABLE_THREADING
        // We want per-thread configuration states, too!
        _perThreadPluginInterface.engineInterface = engineInterface;
        _perThreadPluginInterface.cfgEnv = cfgEnv;

        if ( cfgEnv )
        {
            CExecutiveManager *manager = GetNativeExecutive( engineInterface );

            if ( manager )
            {
                size_t cfgBlockSize = cfgEnv->configFactory.GetClassSize();
                size_t cfgBlockAlign = cfgEnv->configFactory.GetClassAlignment();

                _perThreadPluginOffset =
                    manager->RegisterThreadPlugin( cfgBlockSize, cfgBlockAlign, &_perThreadPluginInterface );
            }
        }
#endif //RWLIB_ENABLE_THREADING
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        rwConfigEnv *cfgEnv = rwConfigEnvRegister.get().GetPluginStruct( engineInterface );

#ifdef RWLIB_ENABLE_THREADING
        // Unregister the per thread environment plugin.
        if ( CExecThread::IsPluginOffsetValid( _perThreadPluginOffset ) )
        {
            if ( cfgEnv )
            {
                CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

                if ( nativeMan )
                {
                    nativeMan->UnregisterThreadPlugin( _perThreadPluginOffset );
                }
            }
        }
#endif //RWLIB_ENABLE_THREADING

        // Destroy the global configuration.
        if ( cfgEnv )
        {
            if ( this->globalCfg )
            {
                RwDynMemAllocator memAlloc( engineInterface );

                cfgEnv->configFactory.Destroy( memAlloc, this->globalCfg );
            }
        }
    }

#ifdef RWLIB_ENABLE_THREADING
    inline rwConfigBlock* GetThreadConfig( CExecThread *theThread ) const
    {
        return (rwConfigBlock*)theThread->ResolvePluginMemory( this->_perThreadPluginOffset );
    }

    inline const rwConfigBlock* GetConstThreadConfig( const CExecThread *theThread ) const
    {
        return (const rwConfigBlock*)theThread->ResolvePluginMemory( this->_perThreadPluginOffset );
    }
#endif //RWLIB_ENABLE_THREADING

    rwConfigBlock *globalCfg;
};

static optional_struct_space <PluginDependantStructRegister <rwConfigDispatchEnv, RwInterfaceFactory_t>> rwConfigDispatchEnvRegister;

rwConfigBlock& GetEnvironmentConfigBlock( EngineInterface *engineInterface )
{
    rwConfigDispatchEnv *cfgEnv = rwConfigDispatchEnvRegister.get().GetPluginStruct( engineInterface );

    if ( !cfgEnv )
    {
        throw NotInitializedException( eSubsystemType::CONFIG, L"CFG_ENVIRONMENT" );
    }
    
#ifdef RWLIB_ENABLE_THREADING
    // Decide whether to return the per-thread state.
    CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

    if ( nativeMan )
    {
        CExecThread *curThread = nativeMan->GetCurrentThread( true );

        // If the current remote thread has no thread descriptor then it cannot have the threaded configuration enabled.
        if ( curThread )
        {
            rwConfigBlock *cfgBlock = cfgEnv->GetThreadConfig( curThread );

            if ( cfgBlock && cfgBlock->enableThreadedConfig )
            {
                return *cfgBlock;
            }
        }
    }
#endif //RWLIB_ENABLE_THREADING

    return *cfgEnv->globalCfg;
}

const rwConfigBlock& GetConstEnvironmentConfigBlock( const EngineInterface *engineInterface )
{
    const rwConfigDispatchEnv *cfgEnv = rwConfigDispatchEnvRegister.get().GetConstPluginStruct( engineInterface );

    if ( !cfgEnv )
    {
        throw NotInitializedException( eSubsystemType::CONFIG, L"CFG_ENVIRONMENT" );
    }

#ifdef RWLIB_ENABLE_THREADING
    // Decide whether to return the per-thread state.
    CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

    if ( nativeMan )
    {
        CExecThread *curThread = nativeMan->GetCurrentThread( true );
        
        // If the current remote thread has no thread descriptor then it cannot have the threaded configuration enabled.
        if ( curThread )
        {
            const rwConfigBlock *cfgBlock = cfgEnv->GetConstThreadConfig( curThread );

            if ( cfgBlock && cfgBlock->enableThreadedConfig )
            {
                return *cfgBlock;
            }
        }
    }
#endif //RWLIB_ENABLE_THREADING

    return *cfgEnv->globalCfg;
}

// Public API for setting states.
void AssignThreadedRuntimeConfig( Interface *intf )
{
#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *engineInterface = (EngineInterface*)intf;

    rwConfigEnv *cfgEnv = rwConfigEnvRegister.get().GetPluginStruct( engineInterface );

    if ( !cfgEnv )
        return;

    rwConfigDispatchEnv *cfgDispatch = rwConfigDispatchEnvRegister.get().GetPluginStruct( engineInterface );

    if ( !cfgDispatch )
        return;

    // We want to create a private copy of the global configuration and enable the per-thread state block.
    CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

    if ( !nativeMan )
        return;

    CExecThread *curThread = nativeMan->GetCurrentThread();
    
    if ( !curThread )
        return;
    
    rwConfigBlock *threadedCfg = cfgDispatch->GetThreadConfig( curThread );

    if ( !threadedCfg )
        return;

    if ( threadedCfg->enableThreadedConfig == false )
    {
        // First get us a private copy of the global configuration.
        bool couldSet = cfgEnv->configFactory.Assign( threadedCfg, cfgDispatch->globalCfg );

        if ( !couldSet )
        {
            throw UnsupportedOperationException( eSubsystemType::CONFIG, L"CFG_THREADEDCONFIG", L"CFG_REASON_PLGFAIL" );
        }

        // Enable our config.
        threadedCfg->enableThreadedConfig = true;
    }

    // Success!
#endif //RWLIB_ENABLE_THREADING
}

void ReleaseThreadedRuntimeConfig( Interface *intf )
{
#ifdef RWLIB_ENABLE_THREADING
    EngineInterface *engineInterface = (EngineInterface*)intf;

    rwConfigDispatchEnv *cfgDispatch = rwConfigDispatchEnvRegister.get().GetPluginStruct( engineInterface );

    if ( !cfgDispatch )
        return;

    // We simply want to disable our copy of the threaded configuration.
    CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

    if ( !nativeMan )
        return;

    CExecThread *curThread = nativeMan->GetCurrentThread( true );
    
    // If the current remote thread has no thread descriptor then it cannot have any threaded config applied.
    if ( !curThread )
        return;
    
    rwConfigBlock *threadedCfg = cfgDispatch->GetThreadConfig( curThread );

    if ( !threadedCfg )
        return;

    // Simply disable us.
    threadedCfg->enableThreadedConfig = false;

    // Success!
#endif //RWLIB_ENABLE_THREADING
}

void registerConfigurationBlockDispatching( void )
{
    rwConfigDispatchEnvRegister.Construct( engineFactory );
}

void unregisterConfigurationBlockDispatching( void )
{
    rwConfigDispatchEnvRegister.Destroy();
}

};