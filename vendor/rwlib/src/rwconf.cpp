/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwconf.cpp
*  PURPOSE:     Configuration block implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwconf.hxx"

namespace rw
{

// Implementation of the config block.
rwConfigBlock::rwConfigBlock( EngineInterface *intf )
{
    this->engineInterface = intf;

    // We default to the San Andreas engine.
    this->version = KnownVersions::getGameVersion( KnownVersions::SA );

    // Setup standard members.
    this->customFileInterface = nullptr;

    this->warningManager = nullptr;
    this->warningLevel = 3;
    this->ignoreSecureWarnings = true;

    // Only use the native toolchain.
    this->palRuntimeType = PALRUNTIME_NATIVE;

    // Prefer the native toolchain.
    this->dxtRuntimeType = DXTRUNTIME_NATIVE;

    this->fixIncompatibleRasters = true;
    this->dxtPackedDecompression = false;

    this->compatibilityTransformNativeImaging = false;
    this->preferPackedSampleExport = true;

    this->ignoreSerializationBlockRegions = false;
    this->blockAcquisitionMode = eBlockAcquisitionMode::EXPECTED;

    this->enableMetaDataTagging = true;

    // Set per-thread states.
    this->enableThreadedConfig = false;
}

rwlock* rwConfigBlock::GetConfigLock( void ) const
{
    const rwConfigEnv *env = rwConfigEnvRegister.get().GetConstPluginStruct( this->engineInterface );

    if ( env )
    {
        return env->GetConfigLock( this );
    }

    return nullptr;
}

rwConfigBlock::rwConfigBlock( const rwConfigBlock& right )
{
    scoped_rwlock_reader <rwlock> cfgConsistency( right.GetConfigLock() );

    this->engineInterface = right.engineInterface;
        
    this->version = right.version;

    this->customFileInterface = right.customFileInterface;

    this->warningManager = right.warningManager;

    this->palRuntimeType = right.palRuntimeType;
    this->dxtRuntimeType = right.dxtRuntimeType;

    this->warningLevel = right.warningLevel;
    this->ignoreSecureWarnings = right.ignoreSecureWarnings;

    this->fixIncompatibleRasters = right.fixIncompatibleRasters;
    this->dxtPackedDecompression = right.dxtPackedDecompression;

    this->compatibilityTransformNativeImaging = right.compatibilityTransformNativeImaging;
    this->preferPackedSampleExport = right.preferPackedSampleExport;

    this->ignoreSerializationBlockRegions = right.ignoreSerializationBlockRegions;
    this->blockAcquisitionMode = right.blockAcquisitionMode;

    this->enableMetaDataTagging = right.enableMetaDataTagging;

    // Copy per-thread states.
    this->enableThreadedConfig = right.enableThreadedConfig;
}

rwConfigBlock::~rwConfigBlock( void )
{
    return;
}

void rwConfigBlock::SetVersion( LibraryVersion ver )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    this->version = ver;
}

LibraryVersion rwConfigBlock::GetVersion( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->version;
}

void rwConfigBlock::SetMetaDataTagging( bool enable )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    // Meta data tagging is useful so that people will find you if they need to (debugging, etc).
    this->enableMetaDataTagging = enable;
}

bool rwConfigBlock::GetMetaDataTagging( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->enableMetaDataTagging;
}

void rwConfigBlock::SetWarningManager( WarningManagerInterface *intf )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    this->warningManager = intf;
}

WarningManagerInterface* rwConfigBlock::GetWarningManager( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->warningManager;
}

void rwConfigBlock::SetWarningLevel( int level )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    this->warningLevel = level;
}

int rwConfigBlock::GetWarningLevel( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->warningLevel;
}

void rwConfigBlock::SetIgnoreSecureWarnings( bool ignore )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    this->ignoreSecureWarnings = ignore;
}

bool rwConfigBlock::GetIgnoreSecureWarnings( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->ignoreSecureWarnings;
}

bool rwConfigBlock::SetPaletteRuntime( ePaletteRuntimeType palRunType )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    // Make sure we support this runtime.
    bool success = false;

    if ( palRunType == PALRUNTIME_NATIVE )
    {
        // We always support the native palette system.
        this->palRuntimeType = palRunType;

        success = true;
    }
#ifdef RWLIB_INCLUDE_LIBIMAGEQUANT
    else if ( palRunType == PALRUNTIME_PNGQUANT )
    {
        // Depends on whether we compiled with support for it.
        this->palRuntimeType = palRunType;

        success = true;
    }
#endif //RWLIB_INCLUDE_LIBIMAGEQUANT

    return success;
}

ePaletteRuntimeType rwConfigBlock::GetPaletteRuntime( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->palRuntimeType;
}

void rwConfigBlock::SetDXTRuntime( eDXTCompressionMethod method )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );
    
    this->dxtRuntimeType = method;
}

eDXTCompressionMethod rwConfigBlock::GetDXTRuntime( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->dxtRuntimeType;
}

void rwConfigBlock::SetFixIncompatibleRasters( bool enable )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    this->fixIncompatibleRasters = enable;
}

bool rwConfigBlock::GetFixIncompatibleRasters( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->fixIncompatibleRasters;
}

void rwConfigBlock::SetDXTPackedDecompression( bool enable )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    this->dxtPackedDecompression = enable;
}

bool rwConfigBlock::GetDXTPackedDecompression( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->dxtPackedDecompression;
}

void rwConfigBlock::SetCompatTransformNativeImaging( bool transfEnable )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    this->compatibilityTransformNativeImaging = transfEnable;
}

bool rwConfigBlock::GetCompatTransformNativeImaging( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->compatibilityTransformNativeImaging;
}

void rwConfigBlock::SetPreferPackedSampleExport( bool preferPacked )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    this->preferPackedSampleExport = preferPacked;
}

bool rwConfigBlock::GetPreferPackedSampleExport( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->preferPackedSampleExport;
}

void rwConfigBlock::SetIgnoreSerializationBlockRegions( bool ignore )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    this->ignoreSerializationBlockRegions = ignore;
}

bool rwConfigBlock::GetIgnoreSerializationBlockRegions( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->ignoreSerializationBlockRegions;
}

void rwConfigBlock::SetBlockAcquisitionMode( eBlockAcquisitionMode mode )
{
    scoped_rwlock_writer <rwlock> lock( GetConfigLock() );

    this->blockAcquisitionMode = mode;
}

eBlockAcquisitionMode rwConfigBlock::GetBlockAcquisitionMode( void ) const
{
    scoped_rwlock_reader <rwlock> lock( GetConfigLock() );

    return this->blockAcquisitionMode;
}

optional_struct_space <rwConfigEnvRegister_t> rwConfigEnvRegister;

void registerConfigurationEnvironment( void )
{
    rwConfigEnvRegister.Construct( engineFactory );
}

void unregisterConfigurationEnvironment( void )
{
    rwConfigEnvRegister.Destroy();
}

} // namespace rw