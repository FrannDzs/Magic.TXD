/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.threadplugins.h
*  PURPOSE:     Thread plugin helpers
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_THREAD_PLUGINS_HEADER_
#define _EXECUTIVE_MANAGER_THREAD_PLUGINS_HEADER_

// This header was created because the plugin logic has to be inserted after the CExecutiveManager
// class has been specified.

#include <type_traits>

BEGIN_NATIVE_EXECUTIVE

namespace ThreadPlugins
{

// Plugin helpers.

// System view for the executive manager as plugin provider.
struct executiveManagerPluginSystemView
{
    typedef threadPluginOffset pluginOffset_t;
    typedef threadPluginInterface pluginInterface;

    CExecutiveManager *execMan;

    inline executiveManagerPluginSystemView( CExecutiveManager *execMan ) noexcept
    {
        this->execMan = execMan;
    }

    template <typename interfaceType, typename... constrArgs>
    inline threadPluginOffset RegisterCustomPlugin( size_t pluginSize, size_t pluginAlignment, const ExecutiveManager::threadPluginDescriptor& pluginId, constrArgs&&... args )
    {
        struct interfaceProxyType final : public interfaceType
        {
            inline interfaceProxyType( CExecutiveManager *manager, constrArgs... args ) : interfaceType( std::forward <constrArgs> ( args )... )
            {
                this->manager = manager;
            }

            void DeleteOnUnregister( void ) noexcept override
            {
                CExecutiveManager *nativeMan = this->manager;

                NatExecStandardObjectAllocator memAlloc( nativeMan );

                eir::dyn_del_struct <interfaceProxyType> ( memAlloc, nullptr, this );
            }

            CExecutiveManager *manager;
        };

        // We do not use the plugin descriptor.

        CExecutiveManager *execMan = this->execMan;

        NatExecStandardObjectAllocator memAlloc( execMan );

        interfaceProxyType *pluginInterface = eir::dyn_new_struct <interfaceProxyType> ( memAlloc, nullptr, execMan, std::forward <constrArgs> ( args )... );

        try
        {
            threadPluginOffset off = execMan->RegisterThreadPlugin( pluginSize, pluginAlignment, pluginInterface );

            if ( CExecThread::IsPluginOffsetValid( off ) == false )
            {
                eir::dyn_del_struct( memAlloc, nullptr, pluginInterface );
                return off;
            }

            return off;
        }
        catch( ... )
        {
            eir::dyn_del_struct <interfaceProxyType> ( memAlloc, nullptr, pluginInterface );

            throw;
        }
    }

    static AINLINE bool IsOffsetValid( threadPluginOffset offset ) noexcept
    {
        return CExecThread::IsPluginOffsetValid( offset );
    }
};

};

// Common thread plugin types.
template <typename structType, bool isDependantStruct = false>
struct execThreadStructPluginRegister
{
    inline execThreadStructPluginRegister( void ) noexcept
    {
        this->execMan = nullptr;
        this->pluginOffset = CExecThread::GetInvalidPluginOffset();
    }

    inline ~execThreadStructPluginRegister( void )
    {
        assert( this->pluginOffset == CExecThread::GetInvalidPluginOffset() );
    }

    typedef CommonPluginSystemDispatch <CExecThread, ThreadPlugins::executiveManagerPluginSystemView, ExecutiveManager::threadPluginDescriptor> PluginDispatchType;

    inline bool RegisterPlugin( CExecutiveManager *execMan )
    {
        if ( execMan == nullptr )
        {
            return false;
        }

        if ( this->pluginOffset != CExecThread::GetInvalidPluginOffset() )
        {
            return true;
        }

        ThreadPlugins::executiveManagerPluginSystemView sysView( execMan );

        threadPluginOffset offset;

        // The_GTA: actually very darn useful to reduce code size.
        // might think about using enums aswell.
        if constexpr ( isDependantStruct )
        {
            offset = PluginDispatchType( sysView ).RegisterDependantStructPlugin <structType> ( ExecutiveManager::threadPluginDescriptor( 0 ), sizeof(structType) );
        }
        else
        {
            offset = PluginDispatchType( sysView ).RegisterStructPlugin <structType> ( ExecutiveManager::threadPluginDescriptor( 0 ) );
        }

        if ( offset == CExecThread::GetInvalidPluginOffset() )
        {
            return false;
        }

        // Remember the plugin offset so we can unregister it later.
        this->execMan = execMan;
        this->pluginOffset = offset;

        return true;
    }

    inline void UnregisterPlugin( void )
    {
        threadPluginOffset offset = this->pluginOffset;

        if ( offset == CExecThread::GetInvalidPluginOffset() )
            return;

        CExecutiveManager *execMan = this->execMan;

        if ( execMan == nullptr )
            return;

        execMan->UnregisterThreadPlugin( offset );

        this->pluginOffset = CExecThread::GetInvalidPluginOffset();
        this->execMan = nullptr;
    }

    inline structType* GetPluginStruct( CExecThread *thread ) const
    {
        threadPluginOffset offset = this->pluginOffset;

        if ( offset == CExecThread::GetInvalidPluginOffset() )
            return nullptr;

        return (structType*)thread->ResolvePluginMemory( offset );
    }

    inline const structType* GetPluginStruct( const CExecThread *thread ) const
    {
        threadPluginOffset offset = this->pluginOffset;

        if ( offset == CExecThread::GetInvalidPluginOffset() )
            return nullptr;

        return (const structType*)thread->ResolvePluginMemory( offset );
    }

    inline structType* GetPluginStructCurrent( bool onlyIfCached = false ) const
    {
        CExecutiveManager *execMan = this->execMan;

        if ( execMan == nullptr )
            return nullptr;

        CExecThread *currentThread = this->execMan->GetCurrentThread( onlyIfCached );

        if ( currentThread == nullptr )
            return nullptr;

        return GetPluginStruct( currentThread );
    }

private:
    CExecutiveManager *execMan;
    threadPluginOffset pluginOffset;
};

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_THREAD_PLUGINS_HEADER_
