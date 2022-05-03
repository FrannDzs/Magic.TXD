/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwinterface.lateinit.cpp
*  PURPOSE:     Initialization after engine construction for data-loading.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "pluginutil.hxx"

namespace rw
{

// Stores all late initializers that should be executed when the engine is completely constructed.
struct lateInitPlugin
{
    AINLINE lateInitPlugin( Interface *rwEngine ) : entries( eir::constr_with_alloc::DEFAULT, rwEngine )
    {
        return;
    }

    struct initEntry
    {
        lateInitializer_t init_cb;
        void *ud;
        lateInitializerCleanup_t cleanup_cb;
    };

    rwVector <initEntry> entries;
};

static optional_struct_space <PluginStructRegister <lateInitPlugin, RwInterfaceFactory_t>> lateInitPluginRegister;

void EngineInterface::PerformLateInit( void )
{
    lateInitPlugin *lateInit = lateInitPluginRegister.get().GetPluginStruct( this );

    for ( const lateInitPlugin::initEntry& entry : lateInit->entries )
    {
        void *ud = entry.ud;

        // Execute the initializer.
        if ( auto init_cb = entry.init_cb )
        {
            init_cb( this, ud );
        }

        // Cleanup the entry.
        if ( auto cleanup_cb = entry.cleanup_cb )
        {
            cleanup_cb( this, ud );
        }
    }

    // We do not need these entries anymore.
    lateInit->entries.Clear();
}

void RegisterLateInitializer( Interface *rwEngine, lateInitializer_t init_cb, void *ud, lateInitializerCleanup_t cleanup_cb )
{
    // This function should only be used during engine construction.
    // It is NOT thread-safe on purpose.

    EngineInterface *nativeEngine = (EngineInterface*)rwEngine;

    lateInitPlugin *lateInit = lateInitPluginRegister.get().GetPluginStruct( nativeEngine );

    lateInitPlugin::initEntry entry;
    entry.init_cb = init_cb;
    entry.ud = ud;
    entry.cleanup_cb = cleanup_cb;

    lateInit->entries.AddToBack( std::move( entry ) );
}

void registerLateInitialization( void )
{
    lateInitPluginRegister.Construct( engineFactory );
}

void unregisterLateInitialization( void )
{
    lateInitPluginRegister.Destroy();
}

} // namespace rw