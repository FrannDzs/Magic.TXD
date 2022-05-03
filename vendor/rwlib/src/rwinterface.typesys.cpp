/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwinterface.typesys.cpp
*  PURPOSE:     Plugin and object type system implementations
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "pluginutil.hxx"

namespace rw
{

void Interface::GetObjectTypeNames( rwobjTypeNameList_t& listOut ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    if ( RwTypeSystem::typeInfoBase *rwobjTypeInfo = engineInterface->rwobjTypeInfo )
    {
        for ( RwTypeSystem::type_iterator iter = engineInterface->typeSystem.GetTypeIterator(); !iter.IsEnd(); iter.Increment() )
        {
            RwTypeSystem::typeInfoBase *item = iter.Resolve();

            if ( item != rwobjTypeInfo )
            {
                if ( engineInterface->typeSystem.IsTypeInheritingFrom( rwobjTypeInfo, item ) )
                {
                    listOut.AddToBack( item->name );
                }
            }
        }
    }
}

bool Interface::IsObjectRegistered( const char *typeName ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    if ( RwTypeSystem::typeInfoBase *rwobjTypeInfo = engineInterface->rwobjTypeInfo )
    {
        // Try to find a type that inherits from RwObject with this name.
        RwTypeSystem::typeInfoBase *rwTypeInfo = engineInterface->typeSystem.FindTypeInfo( typeName, rwobjTypeInfo );

        if ( rwTypeInfo )
        {
            return true;
        }
    }

    return false;
}

const char* Interface::GetObjectTypeName( const RwObject *rwObj ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    const char *typeName = "unknown";

    const GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromConstAbstractObject( rwObj );

    if ( rtObj )
    {
        RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

        // Return its type name.
        // This is an IMMUTABLE property, so we are safe.
        typeName = typeInfo->name;
    }

    return typeName;
}

// *** Plugin support ***

bool IsTypeRegistered( Interface *rwEngine, const char *typeName )
{
    EngineInterface *nativeEngine = (EngineInterface*)rwEngine;

    return ( nativeEngine->typeSystem.ResolveTypeInfo( typeName ) != nullptr );
}

rwStaticString <char> GetFullTypeName( const void *obj )
{
    const GenericRTTI *rttiObj = RwTypeSystem::GetTypeStructFromConstObject( obj );

    RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rttiObj );

    RwTypeSystem *typeSys = RwTypeSystem::GetTypeSystemFromTypeInfo( typeInfo );

    return typeSys->MakeTypeInfoFullName <RwStaticMemAllocator> ( typeInfo );
}

typePluginOffset_t RegisterTypePlugin( Interface *rwEngine, const char *typeName, size_t pluginSize, size_t pluginAlignment, typePluginInterface *pluginIntf )
{
    // Because types never really get added or removed after engine initialization we are thread-safe.

    EngineInterface *nativeEngine = (EngineInterface*)rwEngine;

    // Fetch the type that wants the plugin associated with.
    RwTypeSystem::typeInfoBase *typeInfo = nativeEngine->typeSystem.ResolveTypeInfo( typeName );

    if ( typeInfo == nullptr )
        return RwTypeSystem::INVALID_PLUGIN_OFFSET;

    // Wrapper for the pluginInterface type of DTS.
    struct internalPluginInterface : public RwTypeSystem::pluginInterface
    {
        inline internalPluginInterface( typePluginInterface *wrapped, const char *typeName )
        {
            this->wrapped = wrapped;
            this->typeName = typeName;
        }

        bool OnPluginConstruct( GenericRTTI *rtobj, RwTypeSystem::pluginOffset_t pluginOff, RwTypeSystem::pluginDescriptor pluginId, EngineInterface *rwEngine ) override
        {
            typePluginDescriptor desc( this->typeName );
            void *langobj = RwTypeSystem::GetObjectFromTypeStruct( rtobj );

            return this->wrapped->OnPluginConstruct( langobj, pluginOff, desc, rwEngine );
        }

        void OnPluginDestruct( GenericRTTI *rtobj, RwTypeSystem::pluginOffset_t pluginOff, RwTypeSystem::pluginDescriptor pluginId, EngineInterface *rwEngine ) override
        {
            typePluginDescriptor desc( this->typeName );
            void *langobj = RwTypeSystem::GetObjectFromTypeStruct( rtobj );

            this->wrapped->OnPluginDestruct( langobj, pluginOff, desc, rwEngine );
        }

        bool OnPluginAssign( GenericRTTI *dstRtobj, const GenericRTTI *srcRtobj, RwTypeSystem::pluginOffset_t pluginOff, RwTypeSystem::pluginDescriptor pluginId, EngineInterface *rwEngine ) override
        {
            typePluginDescriptor desc( this->typeName );
            void *dstLangobj = RwTypeSystem::GetObjectFromTypeStruct( dstRtobj );
            const void *srcLangobj = RwTypeSystem::GetConstObjectFromTypeStruct( srcRtobj );

            return this->wrapped->OnPluginAssign( dstLangobj, srcLangobj, pluginOff, desc, rwEngine );
        }

        void DeleteOnUnregister( void ) override
        {
            this->wrapped->DeleteOnUnregister();
        }

        typePluginInterface *wrapped;
        const char *typeName;
    };

    RwDynMemAllocator memAlloc( rwEngine );

    internalPluginInterface *internal_intf = eir::dyn_new_struct <internalPluginInterface> ( memAlloc, nullptr, pluginIntf, typeName );

    try
    {
        RwTypeSystem::pluginDescriptor desc( RwTypeSystem::ANONYMOUS_PLUGIN_ID, typeInfo );

        return nativeEngine->typeSystem.RegisterPlugin( pluginSize, pluginAlignment, desc, internal_intf );
    }
    catch( ... )
    {
        eir::dyn_del_struct <internalPluginInterface> ( memAlloc, nullptr, internal_intf );

        throw;
    }
}

void UnregisterTypePlugin( Interface *rwEngine, const char *typeName, typePluginOffset_t off )
{
    EngineInterface *nativeEngine = (EngineInterface*)rwEngine;

    RwTypeSystem::typeInfoBase *typeInfo = nativeEngine->typeSystem.ResolveTypeInfo( typeName );

    if ( typeInfo )
    {
        nativeEngine->typeSystem.UnregisterPlugin( typeInfo, off );
    }
}

void* ResolveTypePlugin( void *obj, const char *typeName, typePluginOffset_t off )
{
    GenericRTTI *typeStruct = RwTypeSystem::GetTypeStructFromObject( obj );

    // TODO: somehow verify that obj is a valid RW object pointer by using the RTTI information.

    RwTypeSystem::typeInfoBase *objTypeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( typeStruct );

    RwTypeSystem *typeSys = RwTypeSystem::GetTypeSystemFromTypeInfo( objTypeInfo );

    EngineInterface *nativeEngine = LIST_GETITEM( EngineInterface, typeSys, typeSystem );

    RwTypeSystem::typeInfoBase *typeInfo = nativeEngine->typeSystem.ResolveTypeInfo( typeName );

    if ( typeInfo == nullptr )
        return nullptr;

#ifdef _DEBUG
    if ( nativeEngine->typeSystem.IsTypeInheritingFrom( typeInfo, objTypeInfo ) == false )
        return nullptr;
#endif //_DEBUG

    return RwTypeSystem::RESOLVE_STRUCT <void> ( nativeEngine, typeStruct, typeInfo, off );
}

const void* ResolveConstTypePlugin( const void *obj, const char *typeName, typePluginOffset_t off )
{
    const GenericRTTI *typeStruct = RwTypeSystem::GetTypeStructFromConstObject( obj );

    // TODO: somehow verify that obj is a valid RW object pointer by using the RTTI information.

    RwTypeSystem::typeInfoBase *objTypeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( typeStruct );

    RwTypeSystem *typeSys = RwTypeSystem::GetTypeSystemFromTypeInfo( objTypeInfo );

    EngineInterface *nativeEngine = LIST_GETITEM( EngineInterface, typeSys, typeSystem );

    RwTypeSystem::typeInfoBase *typeInfo = nativeEngine->typeSystem.ResolveTypeInfo( typeName );

    if ( typeInfo == nullptr )
        return nullptr;

#ifdef _DEBUG
    if ( nativeEngine->typeSystem.IsTypeInheritingFrom( typeInfo, objTypeInfo ) == false )
        return nullptr;
#endif //_DEBUG

    return RwTypeSystem::RESOLVE_STRUCT <void> ( nativeEngine, typeStruct, typeInfo, off );
}

void* ResolveTypePluginSafe( Interface *rwEngine, void *obj, const char *typeName, typePluginOffset_t off )
{
    EngineInterface *nativeEngine = (EngineInterface*)rwEngine;

    RwTypeSystem::typeInfoBase *pluginTypeInfo = nativeEngine->typeSystem.ResolveTypeInfo( typeName );

    if ( pluginTypeInfo == nullptr )
        return nullptr;

    GenericRTTI *rtObj = nativeEngine->typeSystem.GetTypeStructFromAbstractObject( obj );

#ifdef _DEBUG
    if ( rtObj == nullptr )
        return nullptr;
#endif //_DEBUG

    RwTypeSystem::typeInfoBase *actualObjType = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

#ifdef _DEBUG
    if ( nativeEngine->typeSystem.IsTypeInheritingFrom( pluginTypeInfo, actualObjType ) == false )
        return nullptr;
#endif //_DEBUG

    return RwTypeSystem::RESOLVE_STRUCT <void> ( nativeEngine, rtObj, actualObjType, off );
}

const void* ResolveConstTypePluginSafe( Interface *rwEngine, const void *obj, const char *typeName, typePluginOffset_t off )
{
    EngineInterface *nativeEngine = (EngineInterface*)rwEngine;

    RwTypeSystem::typeInfoBase *pluginTypeInfo = nativeEngine->typeSystem.ResolveTypeInfo( typeName );

    if ( pluginTypeInfo == nullptr )
        return nullptr;

    const GenericRTTI *rtObj = nativeEngine->typeSystem.GetTypeStructFromConstAbstractObject( obj );

#ifdef _DEBUG
    if ( rtObj == nullptr )
        return nullptr;
#endif //_DEBUG

    RwTypeSystem::typeInfoBase *actualObjType = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

#ifdef _DEBUG
    if ( nativeEngine->typeSystem.IsTypeInheritingFrom( pluginTypeInfo, actualObjType ) == false )
        return nullptr;
#endif //_DEBUG

    return RwTypeSystem::RESOLVE_STRUCT <void> ( nativeEngine, rtObj, actualObjType, off );
}

bool IsTypePluginOffsetValid( typePluginOffset_t off ) noexcept
{
    return RwTypeSystem::IsOffsetValid( off );
}

typePluginOffset_t GetInvalidTypePluginOffset( void ) noexcept
{
    return RwTypeSystem::INVALID_PLUGIN_OFFSET;
}

} // namespace rw