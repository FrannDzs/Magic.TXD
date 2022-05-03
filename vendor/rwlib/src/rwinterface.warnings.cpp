/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwinterface.warnings.cpp
*  PURPOSE:     Warning dispatching and reporting.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

// Turns out warnings are a complicated topic that deserves its own source module.

#include "StdInc.h"

#include "rwinterface.hxx"

#include "rwthreading.hxx"

#include <sdk/Templates.h>

#ifdef RWLIB_ENABLE_THREADING

using namespace NativeExecutive;

#endif //RWLIB_ENABLE_THREADING

namespace rw
{

struct warningHandlerThreadEnv
{
    // The purpose of the warning handler stack is to fetch warning output requests and to reroute them
    // so that they make more sense.
    rwStaticVector <WarningHandler*> warningHandlerStack;
};

static optional_struct_space <PluginDependantStructRegister <perThreadDataRegister <warningHandlerThreadEnv, false>, RwInterfaceFactory_t>> warningHandlerPluginRegister;

void Interface::PushWarningToken( const wchar_t *token )
{
    EngineInterface *rwEngine = (EngineInterface*)this;

    // Just look up the localization and push it.
    this->PushWarningDynamic( GetLanguageItem( rwEngine, token ) );
}

void Interface::PushWarningDynamic( rwStaticString <wchar_t>&& localized_message )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    scoped_rwlock_writer <rwlock> lock( GetInterfaceReadWriteLock( engineInterface ) );

    const rwConfigBlock& cfgBlock = GetConstEnvironmentConfigBlock( engineInterface );

    if ( cfgBlock.GetWarningLevel() > 0 )
    {
        // If we have a warning handler, we redirect the message to it instead.
        // The warning handler is supposed to be an internal class that only the library has access to.
        WarningHandler *currentWarningHandler = nullptr;
        {
            auto *whandlerEnv = warningHandlerPluginRegister.get().GetPluginStruct( engineInterface );

            if ( whandlerEnv )
            {
                warningHandlerThreadEnv *threadEnv = whandlerEnv->GetCurrentPluginStruct();

                if ( threadEnv )
                {
                    if ( threadEnv->warningHandlerStack.GetCount() != 0 )
                    {
                        currentWarningHandler = threadEnv->warningHandlerStack.GetBack();
                    }
                }
            }
        }

        if ( currentWarningHandler )
        {
            // Give it the warning.
            currentWarningHandler->OnWarningMessage( std::move( localized_message ) );
        }
        else
        {
            // Else we just post the warning to the runtime.
            if ( WarningManagerInterface *warningMan = cfgBlock.GetWarningManager() )
            {
                warningMan->OnWarning( std::move( localized_message ) );
            }
        }
    }
}

void Interface::PushWarningSingleTemplate( const wchar_t *template_token, const wchar_t *tokenKey, const wchar_t *tokenValue )
{
    EngineInterface *rwEngine = (EngineInterface*)this;

    rwStaticString <wchar_t> template_string = GetLanguageItem( rwEngine, template_token );

    auto result_msg = eir::assign_template_callback <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> (
        template_string.GetConstString(), template_string.GetLength(),
        [&]( const wchar_t *key, size_t keyLen, rwStaticString <wchar_t>& append_out )
        {
            if ( BoundedStringEqual( key, keyLen, tokenKey, true ) )
            {
                append_out += tokenValue;
            }
        }
    );

    this->PushWarningDynamic( std::move( result_msg ) );
}

void Interface::PushWarningTemplate( const wchar_t *template_token, const languageTokenMap_t& tokenMap )
{
    EngineInterface *rwEngine = (EngineInterface*)this;

    rwStaticString <wchar_t> template_string = GetLanguageItem( rwEngine, template_token );

    this->PushWarningDynamic(
        eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( template_string.GetConstString(), template_string.GetLength(), tokenMap )
    );
}

// Returns a string which should nicely describe the character of the relevantObject.
rwStaticString <wchar_t> GetObjectNicePrefixString( EngineInterface *rwEngine, const RwObject *relevantObject )
{
    rwStaticString <wchar_t> objectmsg;
    {
        const GenericRTTI *rtObj = rwEngine->typeSystem.GetTypeStructFromConstAbstractObject( relevantObject );

        if ( rtObj )
        {
            RwTypeSystem::typeInfoBase *objTypeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );
            
            charenv_charprov_tozero <char> charprov( objTypeInfo->name );

            CharacterUtil::_TranscodeStringWithCharProv( objTypeInfo->name, std::move( charprov ), objectmsg );
        }
        else
        {
            objectmsg = L"unknown-obj";
        }
    }

    // Print some sort of name if available.
    if ( const TextureBase *texHandle = ToConstTexture( rwEngine, relevantObject ) )
    {
        const rwString <char>& texName = texHandle->GetName();

        if ( texName.IsEmpty() == false )
        {
            objectmsg += L" '";
            CharacterUtil::TranscodeStringLength( texName.GetConstString(), texName.GetLength(), objectmsg );
            objectmsg += L"'";
        }
    }

    return objectmsg;
}

void Interface::PushWarningObject( const RwObject *relevantObject, const wchar_t *token )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    rwStaticString <wchar_t> templ_objwarn = GetLanguageItem( engineInterface, L"TEMPL_WARN_OBJECT" );

    languageTokenMap_t templ_map;
    // Print the appropriate warning depending on object type.
    // We can use the object type information for that.
    templ_map[ L"objectmsg" ] = GetObjectNicePrefixString( engineInterface, relevantObject );
    templ_map[ L"message" ] = GetLanguageItem( engineInterface, token );

    // Give the message to the warning system.
    this->PushWarningDynamic(
        eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_objwarn.GetConstString(), templ_objwarn.GetLength(), templ_map )
    );
}

void Interface::PushWarningObjectSingleTemplate( const RwObject *relevantObject, const wchar_t *template_token, const wchar_t *tokenKey, const wchar_t *tokenValue )
{
    EngineInterface *rwEngine = (EngineInterface*)this;

    rwStaticString <wchar_t> template_string = GetLanguageItem( rwEngine, template_token );

    auto result_msg = eir::assign_template_callback <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> (
        template_string.GetConstString(), template_string.GetLength(),
        [&]( const wchar_t *keyStr, size_t keyLen, rwStaticString <wchar_t>& append_out )
        {
            if ( BoundedStringEqual( keyStr, keyLen, tokenKey, true ) )
            {
                append_out += tokenValue;
            }
        }
    );

    rwStaticString <wchar_t> templ_objwarn = GetLanguageItem( rwEngine, L"TEMPL_WARN_OBJECT" );

    languageTokenMap_t templ_map;
    templ_map[ L"objectmsg" ] = GetObjectNicePrefixString( rwEngine, relevantObject );
    templ_map[ L"message" ] = std::move( result_msg );

    this->PushWarningDynamic(
        eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_objwarn.GetConstString(), templ_objwarn.GetLength(), templ_map )
    );
}

void GlobalPushWarningHandler( EngineInterface *engineInterface, WarningHandler *theHandler )
{
    auto *whandlerEnv = warningHandlerPluginRegister.get().GetPluginStruct( engineInterface );

    if ( whandlerEnv )
    {
        warningHandlerThreadEnv *threadEnv = whandlerEnv->GetCurrentPluginStruct();

        if ( threadEnv )
        {
            threadEnv->warningHandlerStack.AddToBack( theHandler );
        }
    }
}

void GlobalPopWarningHandler( EngineInterface *engineInterface )
{
    auto *whandlerEnv = warningHandlerPluginRegister.get().GetPluginStruct( engineInterface );

    if ( whandlerEnv )
    {
        warningHandlerThreadEnv *threadEnv = whandlerEnv->GetCurrentPluginStruct();

        if ( threadEnv )
        {
            assert( threadEnv->warningHandlerStack.GetCount() != 0 );

            threadEnv->warningHandlerStack.RemoveFromBack();
        }
    }
}

void registerWarningHandlerEnvironment( void )
{
    warningHandlerPluginRegister.Construct( engineFactory );
}

void unregisterWarningHandlerEnvironment( void )
{
    warningHandlerPluginRegister.Destroy();
}

};