/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwconf.hxx
*  PURPOSE:
*       RenderWare configuration management.
*       We want to support different configurations for different runtimes that use an rw::Interface.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_CONFIG_INTERNALS_
#define _RENDERWARE_CONFIG_INTERNALS_

#include "pluginutil.hxx"

namespace rw
{

struct rwConfigBlock
{
    rwConfigBlock( EngineInterface *intf );
    rwConfigBlock( const rwConfigBlock& right );

    ~rwConfigBlock( void );

    rwlock* GetConfigLock( void ) const;

    // Thread-Safe access to this object.
    void                        SetVersion( LibraryVersion version );
    LibraryVersion              GetVersion( void ) const;

    void                        SetMetaDataTagging( bool enabled );
    bool                        GetMetaDataTagging( void ) const;

    void                        SetFileInterface( FileInterface *intf );
    FileInterface*              GetFileInterface( void ) const;

    void                        SetWarningManager( WarningManagerInterface *intf );
    WarningManagerInterface*    GetWarningManager( void ) const;

    void                        SetWarningLevel( int level );
    int                         GetWarningLevel( void ) const;

    void                        SetIgnoreSecureWarnings( bool doIgnore );
    bool                        GetIgnoreSecureWarnings( void ) const;

    bool                        SetPaletteRuntime( ePaletteRuntimeType type );
    ePaletteRuntimeType         GetPaletteRuntime( void ) const;

    void                        SetDXTRuntime( eDXTCompressionMethod method );
    eDXTCompressionMethod       GetDXTRuntime( void ) const;

    void                        SetFixIncompatibleRasters( bool doFix );
    bool                        GetFixIncompatibleRasters( void ) const;

    void                        SetDXTPackedDecompression( bool packed );
    bool                        GetDXTPackedDecompression( void ) const;

    void                        SetCompatTransformNativeImaging( bool transfEnable );
    bool                        GetCompatTransformNativeImaging( void ) const;

    void                        SetPreferPackedSampleExport( bool prefer );
    bool                        GetPreferPackedSampleExport( void ) const;

    void                        SetIgnoreSerializationBlockRegions( bool doIgnore );
    bool                        GetIgnoreSerializationBlockRegions( void ) const;

    void                        SetBlockAcquisitionMode( eBlockAcquisitionMode mode );
    eBlockAcquisitionMode       GetBlockAcquisitionMode( void ) const;

    EngineInterface *engineInterface;

private:
    LibraryVersion version;     // version of the output files (III, VC, SA, Manhunt, ...)

    FileInterface *customFileInterface;

    WarningManagerInterface *warningManager;

    ePaletteRuntimeType palRuntimeType;
    eDXTCompressionMethod dxtRuntimeType;
    
    int warningLevel;
    bool ignoreSecureWarnings;

    bool fixIncompatibleRasters;
    bool dxtPackedDecompression;

    bool compatibilityTransformNativeImaging;
    bool preferPackedSampleExport;

    bool ignoreSerializationBlockRegions;
    eBlockAcquisitionMode blockAcquisitionMode;

    bool enableMetaDataTagging;

public:
    // Per-Thread config states (only valid if accessed from thread).
    bool enableThreadedConfig;
};

struct cfg_block_constructor
{
    EngineInterface *intf;

    inline cfg_block_constructor( EngineInterface *intf )
    {
        this->intf = intf;
    }

    inline rwConfigBlock* Construct( void *mem ) const
    {
        return new (mem) rwConfigBlock( this->intf );
    }
};

typedef StaticPluginClassFactory <rwConfigBlock, RwDynMemAllocator, rwEirExceptionManager> rwConfigBlockFactory_t;

struct rwConfigEnv
{
    inline rwConfigEnv( EngineInterface *engineInterface ) : configFactory( eir::constr_with_alloc::DEFAULT, engineInterface )
    {
        return;
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        size_t rwlock_size = GetReadWriteLockStructSize( engineInterface );

        this->lockPluginOffset =
            configFactory.RegisterDependantStructPlugin <configLock> ( rwConfigBlockFactory_t::ANONYMOUS_PLUGIN_ID, rwlock_size );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        if ( rwConfigBlockFactory_t::pluginOffset_t offset = this->lockPluginOffset )
        {
            configFactory.UnregisterPlugin( offset );
        }
    }

    inline rwConfigEnv& operator = ( const rwConfigEnv& right )
    {
        throw InvalidOperationException( eSubsystemType::CONFIG, L"OPNAME_ASSIGN", nullptr );
    }

    struct configLock
    {
        inline void Initialize( rwConfigBlock *cfgBlock )
        {
            CreatePlacedReadWriteLock( cfgBlock->engineInterface, this );
        }

        inline void Shutdown( rwConfigBlock *cfgBlock )
        {
            ClosePlacedReadWriteLock( cfgBlock->engineInterface, (rwlock*)this );
        }

        inline void operator = ( const configLock& right )
        {
            // Assignment of locks is not possible/required.
            return;
        }
    };

    inline rwlock* GetConfigLock( const rwConfigBlock *block ) const
    {
        return (rwlock*)rwConfigBlockFactory_t::RESOLVE_STRUCT <rwlock> ( block, this->lockPluginOffset );
    }

    rwConfigBlockFactory_t::pluginOffset_t lockPluginOffset;

    rwConfigBlockFactory_t configFactory;
};

typedef PluginDependantStructRegister <rwConfigEnv, RwInterfaceFactory_t> rwConfigEnvRegister_t;

extern optional_struct_space <rwConfigEnvRegister_t> rwConfigEnvRegister;

// Functions to fetch configuration blocks of the current execution context.
rwConfigBlock& GetEnvironmentConfigBlock( EngineInterface *engineInterface );
const rwConfigBlock& GetConstEnvironmentConfigBlock( const EngineInterface *engineInterface );

} // namespace rw

#endif //_RENDERWARE_CONFIG_INTERNALS_