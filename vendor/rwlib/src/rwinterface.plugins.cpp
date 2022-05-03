/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwinterface.plugins.cpp
*  PURPOSE:     Direct plugins to the interface struct
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwinterface.hxx"

namespace rw
{

void* ResolveInterfacePlugin( Interface *engine, interfacePluginOffset_t offset )
{
    EngineInterface *nativeEngine = (EngineInterface*)engine;

    return RwInterfaceFactory_t::RESOLVE_STRUCT <void> ( nativeEngine, offset );
}

const void* ResolveConstInterfacePlugin( const Interface *engine, interfacePluginOffset_t offset )
{
    EngineInterface *nativeEngine = (EngineInterface*)engine;

    return RwInterfaceFactory_t::RESOLVE_STRUCT <const void> ( nativeEngine, offset );
}

bool IsInterfacePluginOffsetValid( interfacePluginOffset_t off ) noexcept
{
    return RwInterfaceFactory_t::IsOffsetValid( off );
}

interfacePluginOffset_t GetInvalidInterfacePluginOffset( void ) noexcept
{
    return RwInterfaceFactory_t::INVALID_PLUGIN_OFFSET;
}

interfacePluginOffset_t RegisterInterfacePlugin( size_t pluginSize, size_t pluginAlignment, interfacePluginInterface *intf )
{
    // Wrapper for the public type.
    struct wrapperInterfaceType : public RwInterfaceFactory_t::pluginInterface
    {
        inline wrapperInterfaceType( interfacePluginInterface *wrapped ) : wrapped( wrapped )
        {
            return;
        }

        bool OnPluginConstruct( EngineInterface *engine, RwInterfacePluginOffset_t off, RwInterfaceFactory_t::pluginDescriptor desc ) override
        {
            return this->wrapped->OnPluginConstruct( engine, off, interfacePluginDescriptor() );
        }
        
        void OnPluginDestruct( EngineInterface *engine, RwInterfacePluginOffset_t off, RwInterfaceFactory_t::pluginDescriptor desc ) override
        {
            this->wrapped->OnPluginDestruct( engine, off, interfacePluginDescriptor() );
        }
        
        bool OnPluginAssign( EngineInterface *dstEngine, const EngineInterface *srcEngine, RwInterfacePluginOffset_t off, RwInterfaceFactory_t::pluginDescriptor desc ) override
        {
            return this->wrapped->OnPluginAssign( dstEngine, srcEngine, off, interfacePluginDescriptor() );
        }

        void DeleteOnUnregister( void ) override
        {
            eir::static_del_struct <wrapperInterfaceType, RwStaticMemAllocator> ( nullptr, this );
        }

    private:
        interfacePluginInterface *wrapped;
    };

    wrapperInterfaceType *native_intf = eir::static_new_struct <wrapperInterfaceType, RwStaticMemAllocator> ( nullptr, intf );

    try
    {
        RwInterfacePluginOffset_t off = engineFactory.get().RegisterPlugin( pluginSize, pluginAlignment, RwInterfaceFactory_t::ANONYMOUS_PLUGIN_ID, native_intf );

        if ( RwInterfaceFactory_t::IsOffsetValid( off ) == false )
        {
            eir::static_del_struct <wrapperInterfaceType, RwStaticMemAllocator> ( nullptr, native_intf );
        }

        return (interfacePluginOffset_t)off;
    }
    catch( ... )
    {
        eir::static_del_struct <wrapperInterfaceType, RwStaticMemAllocator> ( nullptr, native_intf );

        throw;
    }
}

void UnregisterInterfacePlugin( interfacePluginOffset_t off )
{
    engineFactory.get().UnregisterPlugin( off );
}

} // namespace rw