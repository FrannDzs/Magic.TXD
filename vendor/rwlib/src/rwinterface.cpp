/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwinterface.cpp
*  PURPOSE:     Interface-specific implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "pluginutil.hxx"

#include <atomic>

#include "rwinterface.hxx"

#include "rwthreading.hxx"

namespace rw
{

// Factory for interfaces.
optional_struct_space <RwInterfaceFactory_t> engineFactory;

RwObject::RwObject( Interface *engineInterface, void *construction_params )
{
    // Constructor that is called for creation.
    this->engineInterface = engineInterface;
    this->objVersion = engineInterface->GetVersion();   // when creating an object, we assign it the current version.
}

inline void SafeDeleteType( EngineInterface *engineInterface, RwTypeSystem::typeInfoBase*& theType )
{
    RwTypeSystem::typeInfoBase *theTypeVal = theType;

    if ( theTypeVal )
    {
        engineInterface->typeSystem.DeleteType( theTypeVal );

        theType = nullptr;
    }
}

struct rw_afterinit_t
{
    inline void Initialize( EngineInterface *engine )
    {
        engine->typeSystem.InitializeLockProvider();

        // Register the main RenderWare types.
        {
            engine->streamTypeInfo = engine->typeSystem.RegisterAbstractType <Stream> ( "stream" );
            engine->rwobjTypeInfo = engine->typeSystem.RegisterAbstractType <RwObject> ( "rwobj" );
            engine->textureTypeInfo = engine->typeSystem.RegisterStructType <TextureBase> ( "texture", engine->rwobjTypeInfo );
        }
    }

    inline void Shutdown( EngineInterface *engine )
    {
        // Unregister all types again.
        {
            SafeDeleteType( engine, engine->textureTypeInfo );
            SafeDeleteType( engine, engine->rwobjTypeInfo );
            SafeDeleteType( engine, engine->streamTypeInfo );
        }

        // We need to terminate the type system here.
        engine->typeSystem.Shutdown();
    }
};

static optional_struct_space <PluginDependantStructRegister <rw_afterinit_t, RwInterfaceFactory_t>> rw_afterinit_reg;

EngineInterface::EngineInterface( void ) :
    applicationName( eir::constr_with_alloc::DEFAULT, this ),
    applicationVersion( eir::constr_with_alloc::DEFAULT, this ),
    applicationDescription( eir::constr_with_alloc::DEFAULT, this )
{
    // Set up the type system.
    this->typeSystem.lockProvider.engineInterface = this;

    // Remember to wait with type creation until the threading environment has registered!
    this->rwobjTypeInfo = nullptr;
    this->streamTypeInfo = nullptr;
    this->textureTypeInfo = nullptr;
}

EngineInterface::~EngineInterface( void )
{
    return;
}

optional_struct_space <rwLockProvider_t> rwlockProvider;

void Interface::SetVersion( LibraryVersion version )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetVersion( version );
}

LibraryVersion Interface::GetVersion( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetVersion();
}

void Interface::SetApplicationInfo( const softwareMetaInfo& metaInfo )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    scoped_rwlock_writer <rwlock> lock( GetInterfaceReadWriteLock( engineInterface ) );

    if ( const char *appName = metaInfo.applicationName )
    {
        engineInterface->applicationName = appName;
    }
    else
    {
        engineInterface->applicationName.Clear();
    }

    if ( const char *appVersion = metaInfo.applicationVersion )
    {
        engineInterface->applicationVersion = appVersion;
    }
    else
    {
        engineInterface->applicationVersion.Clear();
    }

    if ( const char *desc = metaInfo.description )
    {
        engineInterface->applicationDescription = desc;
    }
    else
    {
        engineInterface->applicationDescription.Clear();
    }
}

void Interface::SetMetaDataTagging( bool enabled )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetMetaDataTagging( enabled );
}

bool Interface::GetMetaDataTagging( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetMetaDataTagging();
}

rwStaticString <char> GetRunningSoftwareInformation( EngineInterface *engineInterface, bool outputShort )
{
    rwStaticString <char> infoOut;

    {
        scoped_rwlock_reader <rwlock> lock( GetInterfaceReadWriteLock( engineInterface ) );

        const rwConfigBlock& cfgBlock = GetConstEnvironmentConfigBlock( engineInterface );

        // Only output anything if we enable meta data tagging.
        if ( cfgBlock.GetMetaDataTagging() )
        {
            // First put the software name.
            bool hasAppName = false;

            if ( engineInterface->applicationName.GetLength() != 0 )
            {
                infoOut += engineInterface->applicationName;

                hasAppName = true;
            }
            else
            {
                infoOut += "RenderWare (generic)";
            }

            infoOut += " [rwver: ";
            infoOut += engineInterface->GetVersion().toString();
            infoOut += "]";

            if ( hasAppName )
            {
                if ( engineInterface->applicationVersion.GetLength() != 0 )
                {
                    infoOut += " version: ";
                    infoOut += engineInterface->applicationVersion;
                }
            }

            if ( outputShort == false )
            {
                if ( engineInterface->applicationDescription.GetLength() != 0 )
                {
                    infoOut += " ";
                    infoOut += engineInterface->applicationDescription;
                }
            }
        }
    }

    return infoOut;
}

struct refCountPlugin
{
    std::atomic <unsigned long> refCount;

    inline void operator =( const refCountPlugin& right )
    {
        this->refCount.store( right.refCount );
    }

    inline void Initialize( GenericRTTI *obj )
    {
        this->refCount = 1; // we start off with one.
    }

    inline void Shutdown( GenericRTTI *obj )
    {
        // Has to be zeroed by the manager.
        assert( this->refCount == 0 );
    }

    inline void AddRef( void )
    {
        this->refCount++;
    }

    inline bool RemoveRef( void )
    {
        return ( this->refCount.fetch_sub( 1 ) == 1 );
    }
};

struct refCountManager
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        this->pluginOffset =
            engineInterface->typeSystem.RegisterDependantStructPlugin <refCountPlugin> ( engineInterface->rwobjTypeInfo, RwTypeSystem::ANONYMOUS_PLUGIN_ID );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        engineInterface->typeSystem.UnregisterPlugin( engineInterface->rwobjTypeInfo, this->pluginOffset );
    }

    inline refCountPlugin* GetPluginStruct( EngineInterface *engineInterface, RwObject *obj )
    {
        GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( obj );

        return RwTypeSystem::RESOLVE_STRUCT <refCountPlugin> ( engineInterface, rtObj, engineInterface->rwobjTypeInfo, this->pluginOffset );
    }

    RwTypeSystem::pluginOffset_t pluginOffset;
};

static optional_struct_space <PluginDependantStructRegister <refCountManager, RwInterfaceFactory_t>> refCountRegister;

// Acquisition routine for objects, so that reference counting is increased, if needed.
// Can return nullptr if the reference count could not be increased.
RwObject* AcquireObject( RwObject *obj )
{
    EngineInterface *engineInterface = (EngineInterface*)obj->engineInterface;

    // Increase the reference count.
    if ( refCountManager *refMan = refCountRegister.get().GetPluginStruct( engineInterface ) )
    {
        if ( refCountPlugin *refCount = refMan->GetPluginStruct( engineInterface, obj ) )
        {
            // TODO: make sure that incrementing is actually possible.
            // we cannot increment if we would overflow the number, for instance.

            refCount->AddRef();
        }
    }

    return obj;
}

void ReleaseObject( RwObject *obj )
{
    EngineInterface *engineInterface = (EngineInterface*)obj->engineInterface;

    if ( refCountManager *refMan = refCountRegister.get().GetPluginStruct( (EngineInterface*)engineInterface ) )
    {
        if ( refCountPlugin *refCount = refMan->GetPluginStruct( engineInterface, obj ) )
        {
            (void)refCount;

            // We just delete the object.
            engineInterface->DeleteRwObject( obj );
        }
    }
}

uint32 GetRefCount( RwObject *obj )
{
    uint32 refCountNum = 1;    // If we do not support reference counting, this is actually a valid value.

    EngineInterface *engineInterface = (EngineInterface*)obj->engineInterface;

    // Increase the reference count.
    if ( refCountManager *refMan = refCountRegister.get().GetPluginStruct( engineInterface ) )
    {
        if ( refCountPlugin *refCount = refMan->GetPluginStruct( engineInterface, obj ) )
        {
            // We just delete the object.
            refCountNum = refCount->refCount;
        }
    }

    return refCountNum;
}

RwObject* Interface::ConstructRwObject( const char *typeName )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    RwObject *newObj = nullptr;

    if ( RwTypeSystem::typeInfoBase *rwobjTypeInfo = engineInterface->rwobjTypeInfo )
    {
        // Try to find a type that inherits from RwObject with this name.
        RwTypeSystem::typeInfoBase *rwTypeInfo = engineInterface->typeSystem.FindTypeInfo( typeName, rwobjTypeInfo );

        if ( rwTypeInfo )
        {
            // Try to construct us.
            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, rwTypeInfo, nullptr );

            if ( rtObj )
            {
                // We are successful! Return the new object.
                newObj = (RwObject*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return newObj;
}

RwObject* Interface::CloneRwObject( const RwObject *srcObj )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    RwObject *newObj = nullptr;

    // We simply use our type system to do the job.
    const GenericRTTI *rttiObj = engineInterface->typeSystem.GetTypeStructFromConstAbstractObject( srcObj );

    if ( rttiObj )
    {
        GenericRTTI *newRtObj = engineInterface->typeSystem.Clone( engineInterface, rttiObj );

        if ( newRtObj )
        {
            newObj = (RwObject*)RwTypeSystem::GetObjectFromTypeStruct( newRtObj );
        }
    }

    return newObj;
}

void Interface::DeleteRwObject( RwObject *obj )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    // Delete it using the type system.
    GenericRTTI *rttiObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( obj );

    if ( rttiObj )
    {
        // By default, we can destroy.
        bool canDestroy = true;

        // If we have the refcount plugin, we want to handle things with it.
        if ( refCountManager *refMan = refCountRegister.get().GetPluginStruct( engineInterface ) )
        {
            if ( refCountPlugin *refCountObj = RwTypeSystem::RESOLVE_STRUCT <refCountPlugin> ( engineInterface, rttiObj, engineInterface->rwobjTypeInfo, refMan->pluginOffset ) )
            {
                canDestroy = refCountObj->RemoveRef();
            }
        }

        if ( canDestroy )
        {
            engineInterface->typeSystem.Destroy( engineInterface, rttiObj );
        }
    }
}

void Interface::SetWarningManager( WarningManagerInterface *warningMan )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetWarningManager( warningMan );
}

WarningManagerInterface* Interface::GetWarningManager( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetWarningManager();
}

void Interface::SetWarningLevel( int level )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetWarningLevel( level );
}

int Interface::GetWarningLevel( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetWarningLevel();
}

void Interface::SetIgnoreSecureWarnings( bool doIgnore )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetIgnoreSecureWarnings( doIgnore );
}

bool Interface::GetIgnoreSecureWarnings( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetIgnoreSecureWarnings();
}

bool Interface::SetPaletteRuntime( ePaletteRuntimeType palRunType )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    return GetEnvironmentConfigBlock( engineInterface ).SetPaletteRuntime( palRunType );
}

ePaletteRuntimeType Interface::GetPaletteRuntime( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetPaletteRuntime();
}

void Interface::SetDXTRuntime( eDXTCompressionMethod dxtRunType )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetDXTRuntime( dxtRunType );
}

eDXTCompressionMethod Interface::GetDXTRuntime( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetDXTRuntime();
}

void Interface::SetFixIncompatibleRasters( bool doFix )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetFixIncompatibleRasters( doFix );
}

bool Interface::GetFixIncompatibleRasters( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetFixIncompatibleRasters();
}

void Interface::SetCompatTransformNativeImaging( bool transfEnable )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetCompatTransformNativeImaging( transfEnable );
}

bool Interface::GetCompatTransformNativeImaging( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetCompatTransformNativeImaging();
}

void Interface::SetPreferPackedSampleExport( bool preferPacked )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetPreferPackedSampleExport( preferPacked );
}

bool Interface::GetPreferPackedSampleExport( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetPreferPackedSampleExport();
}

void Interface::SetDXTPackedDecompression( bool packedDecompress )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetDXTPackedDecompression( packedDecompress );
}

bool Interface::GetDXTPackedDecompression( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetDXTPackedDecompression();
}

void Interface::SetIgnoreSerializationBlockRegions( bool doIgnore )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    GetEnvironmentConfigBlock( engineInterface ).SetIgnoreSerializationBlockRegions( doIgnore );
}

bool Interface::GetIgnoreSerializationBlockRegions( void ) const
{
    const EngineInterface *engineInterface = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( engineInterface ).GetIgnoreSerializationBlockRegions();
}

void Interface::SetBlockAcquisitionMode( eBlockAcquisitionMode mode )
{
    EngineInterface *rwEngine = (EngineInterface*)this;

    GetEnvironmentConfigBlock( rwEngine ).SetBlockAcquisitionMode( mode );
}

eBlockAcquisitionMode Interface::GetBlockAcquisitionMode( void ) const
{
    const EngineInterface *rwEngine = (const EngineInterface*)this;

    return GetConstEnvironmentConfigBlock( rwEngine ).GetBlockAcquisitionMode();
}

// Static library object that takes care of initializing the module dependencies properly.
extern void registerMemoryEnvironment( void );
extern void registerConfigurationEnvironment( void );
extern void registerThreadingEnvironment( void );
extern void registerLateInitialization( void );
extern void registerLocalizationEnvironment( void );
extern void registerWarningHandlerEnvironment( void );
extern void registerFileManagement( void );
extern void registerEventSystem( void );
extern void registerTXDPlugins( void );
extern void registerObjectExtensionsPlugins( void );
extern void registerSerializationPlugins( void );
extern void registerStreamGlobalPlugins( void );
extern void registerFileSystemDataRepository( void );
extern void registerImagingPlugin( void );
extern void registerNativeImagePluginEnvironment( void );
extern void registerWindowingSystem( void );
extern void registerDriverEnvironment( void );
extern void registerDrawingLayerEnvironment( void );
extern void registerConfigurationBlockDispatching( void );

// Need to also remove registrations again.
extern void unregisterMemoryEnvironment( void );
extern void unregisterConfigurationEnvironment( void );
extern void unregisterThreadingEnvironment( void );
extern void unregisterLateInitialization( void );
extern void unregisterLocalizationEnvironment( void );
extern void unregisterWarningHandlerEnvironment( void );
extern void unregisterFileManagement( void );
extern void unregisterEventSystem( void );
extern void unregisterTXDPlugins( void );
extern void unregisterObjectExtensionsPlugins( void );
extern void unregisterSerializationPlugins( void );
extern void unregisterStreamGlobalPlugins( void );
extern void unregisterFileSystemDataRepository( void );
extern void unregisterImagingPlugin( void );
extern void unregisterNativeImagePluginEnvironment( void );
extern void unregisterWindowingSystem( void );
extern void unregisterDriverEnvironment( void );
extern void unregisterDrawingLayerEnvironment( void );
extern void unregisterConfigurationBlockDispatching( void );

static unsigned int _init_ref_cnt = 0;

static bool VerifyLibraryIntegrity( void )
{
    // We need to standardize the number formats.
    // One way to check that is to make out their size, I guess.
    // Then there is also the problem of endianness, which we do not check here :(
    // For that we have to add special handling into the serialization environments.
    return
        ( sizeof(uint8) == 1 &&
          sizeof(uint16) == 2 &&
          sizeof(uint32) == 4 &&
          sizeof(uint64) == 8 &&
          sizeof(int8) == 1 &&
          sizeof(int16) == 2 &&
          sizeof(int32) == 4 &&
          sizeof(int64) == 8 &&
          sizeof(float32) == 4 );
}

// Module initialization and shutdown.
void ReferenceEngineEnvironment( void )
{
    if ( _init_ref_cnt == 0 )
    {
        // Verify data constants before we create a valid engine.
        if ( VerifyLibraryIntegrity() )
        {
            registerMemoryEnvironment();

            engineFactory.Construct();

            // Very important environment.
            // Initializes the memory subsystem aswell.
            registerThreadingEnvironment();

            registerLateInitialization();
            registerLocalizationEnvironment();

            // Configuration comes first.
            registerConfigurationEnvironment();

            // Safe to initialize the remainder of the engine now.
            rw_afterinit_reg.Construct( engineFactory );

            // Initialize our plugins first.
            refCountRegister.Construct( engineFactory );
            rwlockProvider.Construct( engineFactory );

            // Now do the main modules.
            registerWarningHandlerEnvironment();
            registerFileManagement();
            registerEventSystem();
            registerStreamGlobalPlugins();
            registerFileSystemDataRepository();
            registerSerializationPlugins();
            registerObjectExtensionsPlugins();
            registerTXDPlugins();
            registerImagingPlugin();
            registerNativeImagePluginEnvironment();
            registerWindowingSystem();
            registerDriverEnvironment();                // THREAD PLUGINS INCLUDED
            registerDrawingLayerEnvironment();

            // After all plugins registered themselves, we know that
            // each configuration entry is properly initialized.
            // Now we can create a configuration block in the interface!
            registerConfigurationBlockDispatching();    // THREAD PLUGIN INCLUDED

            _init_ref_cnt++;
        }
    }
    else
    {
        _init_ref_cnt++;
    }
}

void DereferenceEngineEnvironment( void )
{
    if ( --_init_ref_cnt == 0 )
    {
        unregisterConfigurationBlockDispatching();

        unregisterDrawingLayerEnvironment();
        unregisterDriverEnvironment();
        unregisterWindowingSystem();
        unregisterNativeImagePluginEnvironment();
        unregisterImagingPlugin();
        unregisterTXDPlugins();
        unregisterObjectExtensionsPlugins();
        unregisterSerializationPlugins();
        unregisterFileSystemDataRepository();
        unregisterStreamGlobalPlugins();
        unregisterEventSystem();
        unregisterFileManagement();
        unregisterWarningHandlerEnvironment();

        rwlockProvider.Destroy();
        refCountRegister.Destroy();

        rw_afterinit_reg.Destroy();

        unregisterConfigurationEnvironment();
        
        unregisterThreadingEnvironment();

        unregisterLocalizationEnvironment();
        unregisterLateInitialization();

        engineFactory.Destroy();

        unregisterMemoryEnvironment();
    }
}

// Interface creation for the RenderWare engine.
Interface* CreateEngine( LibraryVersion theVersion )
{
    // Has to be called prior to creating any engines.
    ReferenceEngineEnvironment();

    Interface *engineOut = nullptr;

    if ( _init_ref_cnt > 0 )
    {
        // Create a specialized engine depending on the version.
        RwStaticMemAllocator memAlloc;
        
        engineOut = engineFactory.get().ConstructArgs( memAlloc );

        if ( engineOut )
        {
            EngineInterface *rwEngine = (EngineInterface*)engineOut;

            rwEngine->SetVersion( theVersion );

            // Now run all late initializers.
            rwEngine->PerformLateInit();
        }
        else
        {
            DereferenceEngineEnvironment();
        }
    }

    return engineOut;
}

void DeleteEngine( Interface *theEngine )
{
    assert( _init_ref_cnt > 0 );

    EngineInterface *engineInterface = (EngineInterface*)theEngine;

    // Kill everything threading related, so we can terminate.
    ThreadingMarkAsTerminating( engineInterface );      // disallow creation of new threading objects
    //PurgeActiveThreadingObjects( engineInterface );     // destroy all available threading objects

    // Destroy the engine again.
    RwStaticMemAllocator memAlloc;

    engineFactory.get().Destroy( memAlloc, engineInterface );

    DereferenceEngineEnvironment();
}

};
