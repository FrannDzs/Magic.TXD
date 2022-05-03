/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.raster.nativetex.cpp
*  PURPOSE:     RenderWare Raster native texture specific logic.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.raster.hxx"

namespace rw
{

// Store our environment in the RW interface.
optional_struct_space <PluginDependantStructRegister <nativeTextureStreamPlugin, RwInterfaceFactory_t>> nativeTextureStreamStore;

// Texture native type registrations.
bool RegisterNativeTextureType( Interface *engineInterface, const char *nativeName, texNativeTypeProvider *typeProvider, size_t memSize, size_t memAlignment )
{
    bool success = false;

    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.get().GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        success = nativeTexEnv->RegisterNativeTextureType( engineInterface, nativeName, typeProvider, memSize, memAlignment );
    }

    return success;
}

bool UnregisterNativeTextureType( Interface *engineInterface, const char *nativeName )
{
    bool success = false;

    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.get().GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        success = nativeTexEnv->UnregisterNativeTextureType( engineInterface, nativeName );
    }

    return success;
}

void ExploreNativeTextureTypeProviders( Interface *intf, texNativeTypeProviderCallback_t cb, void *ud )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    // We want to iterate through all native texture type providers and return them to the callback.
    const nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.get().GetConstPluginStruct( engineInterface );

    if ( nativeTexEnv )
    {
        LIST_FOREACH_BEGIN( texNativeTypeProvider, nativeTexEnv->texNativeTypes.root, managerData.managerNode )

            cb( item, ud );

        LIST_FOREACH_END
    }
}

texNativeTypeProvider* GetNativeTextureTypeProvider( Interface *intf, void *objMem )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    texNativeTypeProvider *platformData = nullptr;

    // We first need the native texture type environment.
    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.get().GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        // Attempt to get the type info of the native data.
        GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( objMem );

        if ( rtObj )
        {
            RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

            // Check that we indeed are a native texture type.
            // This is guarranteed if we inherit from the native texture type info.
            if ( engineInterface->typeSystem.IsTypeInheritingFrom( nativeTexEnv->platformTexType, typeInfo ) )
            {
                // We assume that the type provider is of our native type.
                // For safety, do a dynamic cast.
                nativeTextureStreamPlugin::nativeTextureCustomTypeInterface *nativeTypeInterface =
                    dynamic_cast <nativeTextureStreamPlugin::nativeTextureCustomTypeInterface*> ( typeInfo->tInterface );

                if ( nativeTypeInterface )
                {
                    // Return the type provider.
                    platformData = nativeTypeInterface->texTypeProvider;
                }
            }
        }
    }

    return platformData;
}

texNativeTypeProvider* GetNativeTextureTypeProviderByName( Interface *engineInterface, const char *typeName )
{
    // Get the type that is associated with the given typeName.
    RwTypeSystem::typeInfoBase *theType = GetNativeTextureType( engineInterface, typeName );

    if ( theType )
    {
        // Ensure that we are a native texture type and get its manager.
        nativeTextureStreamPlugin::nativeTextureCustomTypeInterface *nativeIntf = dynamic_cast <nativeTextureStreamPlugin::nativeTextureCustomTypeInterface*> ( theType->tInterface );

        if ( nativeIntf )
        {
            // Get the interface pointer.
            return nativeIntf->texTypeProvider;
        }
    }

    return nullptr;
}

void* GetNativeTextureDriverInterface( Interface *engineInterface, const char *typeName )
{
    void *intf = nullptr;

    // Get the type that is associated with the given typeName.
    if ( texNativeTypeProvider *texProvider = GetNativeTextureTypeProviderByName( engineInterface, typeName ) )
    {
        // Get the interface pointer.
        intf = texProvider->GetDriverNativeInterface();
    }

    return intf;
}

platformTypeNameList_t GetAvailableNativeTextureTypes( Interface *intf )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    platformTypeNameList_t registeredTypes;

    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.get().GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        if ( RwTypeSystem::typeInfoBase *nativeTexType = nativeTexEnv->platformTexType )
        {
            // We need to iterate through all types and return the ones that directly inherit from our native tex type.
            for ( RwTypeSystem::type_iterator iter = engineInterface->typeSystem.GetTypeIterator(); iter.IsEnd() == false; iter.Increment() )
            {
                RwTypeSystem::typeInfoBase *theType = iter.Resolve();

                if ( theType->inheritsFrom == nativeTexType )
                {
                    registeredTypes.AddToBack( theType->name );
                }
            }
        }
    }

    return registeredTypes;
}

bool GetNativeTextureFormatInfo( Interface *intf, const char *nativeName, nativeRasterFormatInfo& infoOut )
{
    bool gotInfo = false;

    EngineInterface *engineInterface = (EngineInterface*)intf;

    if ( texNativeTypeProvider *texProvider = GetNativeTextureTypeProviderByName( engineInterface, nativeName ) )
    {
        // Alright, let us return info about it.
        storageCapabilities formatStoreCaps;
        texProvider->GetStorageCapabilities( formatStoreCaps );

        infoOut.isCompressedFormat = formatStoreCaps.isCompressedFormat;
        infoOut.supportsDXT1 = formatStoreCaps.pixelCaps.supportsDXT1;
        infoOut.supportsDXT2 = formatStoreCaps.pixelCaps.supportsDXT2;
        infoOut.supportsDXT3 = formatStoreCaps.pixelCaps.supportsDXT3;
        infoOut.supportsDXT4 = formatStoreCaps.pixelCaps.supportsDXT4;
        infoOut.supportsDXT5 = formatStoreCaps.pixelCaps.supportsDXT5;
        infoOut.supportsPalette = formatStoreCaps.pixelCaps.supportsPalette;

        gotInfo = true;
    }

    return gotInfo;
}

bool IsNativeTexture( rw::Interface *intf, const char *nativeName )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    // Get the type that is associated with the given typeName.
    RwTypeSystem::typeInfoBase *theType = GetNativeTextureType( engineInterface, nativeName );

    return ( theType != nullptr );
}

}
