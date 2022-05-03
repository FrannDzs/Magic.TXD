/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.eventplugin.hxx
*  PURPOSE:     Event plugin embedding into system struct
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NAT_EXEC_EVENT_OBJECT_EMBED_HELPER_HEADER_
#define _NAT_EXEC_EVENT_OBJECT_EMBED_HELPER_HEADER_

#include "internal/CExecutiveManager.event.internal.h"

#include <sdk/eirutils.h>

BEGIN_NATIVE_EXECUTIVE

template <typename factoryType>
struct DynamicEventPluginRegister
{
    inline DynamicEventPluginRegister( void )
    {
        return;
    }
    inline DynamicEventPluginRegister( factoryType& factory )
    {
        this->RegisterPlugin( factory );
    }
    inline ~DynamicEventPluginRegister( void )
    {
        this->UnregisterPlugin();
    }

    inline void RegisterPlugin( factoryType& factory )
    {
        this->pluginOff = factory.template RegisterStructPlugin <eventStructHelper> (
            factoryType::ANONYMOUS_PLUGIN_ID,
            pubevent_get_size()
        );

        FATAL_ASSERT( this->pluginOff != factoryType::INVALID_PLUGIN_OFFSET );

        this->regFact = &factory;
        this->isRegistered = true;
    }

    inline void UnregisterPlugin( void )
    {
        if ( factoryType *regFact = this->regFact )
        {
            regFact->UnregisterPlugin( this->pluginOff );
        }

        this->isRegistered = false;
    }

    CEvent* GetEvent( typename factoryType::hostType_t *nativeMan )
    {
        FATAL_ASSERT( this->isRegistered == true );

        return factoryType::template RESOLVE_STRUCT <CEvent> ( nativeMan, this->pluginOff );
    }

private:
    struct eventStructHelper
    {
        inline eventStructHelper( void )
        {
            pubevent_constructor( this );
        }

        inline ~eventStructHelper( void )
        {
            pubevent_destructor( this );
        }
    };

    bool isRegistered = false;

    factoryType *regFact = nullptr;

    typename factoryType::pluginOffset_t pluginOff = factoryType::INVALID_PLUGIN_OFFSET;
};

typedef DynamicEventPluginRegister <executiveManagerFactory_t> EventPluginRegister;

END_NATIVE_EXECUTIVE

#endif //_NAT_EXEC_EVENT_OBJECT_EMBED_HELPER_HEADER_
