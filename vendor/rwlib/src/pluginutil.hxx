/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/pluginutil.hxx
*  PURPOSE:     Internal plugin helper definitions.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _PLUGIN_UTILITIES_
#define _PLUGIN_UTILITIES_

#include <sdk/PluginHelpers.h>

namespace rw
{

// Helper to place a RW lock into a StaticPluginClassFactory type.
template <typename factoryType, typename getFactCallbackType>
struct factLockProviderEnv
{
private:
    typedef typename factoryType::hostType_t hostType_t;

    struct dynamic_rwlock
    {
        inline void Initialize( hostType_t *host )
        {
            CreatePlacedReadWriteLock( host->engineInterface, this );
        }

        inline void Shutdown( hostType_t *host )
        {
            ClosePlacedReadWriteLock( host->engineInterface, (rwlock*)this );
        }

        inline void operator = ( const dynamic_rwlock& right )
        {
            // Nothing to do here.
            return;
        }
    };

public:
    inline void Initialize( EngineInterface *intf )
    {
        size_t rwlock_struct_size = GetReadWriteLockStructSize( intf );

        typename factoryType::pluginOffset_t lockPluginOffset = factoryType::INVALID_PLUGIN_OFFSET;

        factoryType *theFact = getFactCallbackType::getFactory( intf );

        if ( theFact )
        {
            lockPluginOffset =
                theFact->template RegisterDependantStructPlugin <dynamic_rwlock> ( factoryType::ANONYMOUS_PLUGIN_ID, rwlock_struct_size );
        }

        this->lockPluginOffset = lockPluginOffset;
    }

    inline void Shutdown( EngineInterface *intf )
    {
        if ( factoryType::IsOffsetValid( this->lockPluginOffset ) )
        {
            factoryType *theFact = getFactCallbackType::getFactory( intf );

            theFact->UnregisterPlugin( this->lockPluginOffset );
        }
    }

    inline rwlock* GetLock( const hostType_t *host ) const
    {
        return (rwlock*)factoryType::template RESOLVE_STRUCT <rwlock> ( host, this->lockPluginOffset );
    }

private:
    typename factoryType::pluginOffset_t lockPluginOffset;
};

// Helper to register a lock inside of a RenderWare dynamic type object (like RwObject).
template <typename typeFetchStructoid_t>
struct rwobjLockTypeRegister
{
private:
    struct custom_lock
    {
        inline void Initialize( GenericRTTI *rtObj )
        {
            RwObject *rwObj = (RwObject*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

            EngineInterface *engineInterface = (EngineInterface*)rwObj->engineInterface;

            CreatePlacedReadWriteLock( engineInterface, this );
        }

        inline void Shutdown( GenericRTTI *rtObj )
        {
            RwObject *rwObj = (RwObject*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

            EngineInterface *engineInterface = (EngineInterface*)rwObj->engineInterface;

            ClosePlacedReadWriteLock( engineInterface, (rwlock*)this );
        }

        inline void operator = ( const custom_lock& right )
        {
            // Nothing to do.
            return;
        }
    };

    RwInterfaceFactory_t::pluginOffset_t _genLockPlgOffset;

public:
    inline void Initialize( EngineInterface *engineInterface )
    {
        RwInterfaceFactory_t::pluginOffset_t offset = RwInterfaceFactory_t::INVALID_PLUGIN_OFFSET;

        RwTypeSystem::typeInfoBase *theType = typeFetchStructoid_t::resolveType( engineInterface );

        if ( theType )
        {
            size_t lockSize = GetReadWriteLockStructSize( engineInterface );

            offset = engineInterface->typeSystem.RegisterDependantStructPlugin <custom_lock> ( theType, RwTypeSystem::ANONYMOUS_PLUGIN_ID, lockSize );
        }

        this->_genLockPlgOffset = offset;
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        if ( RwTypeSystem::IsOffsetValid( this->_genLockPlgOffset ) )
        {
            RwTypeSystem::typeInfoBase *theType = typeFetchStructoid_t::resolveType( engineInterface );

            assert( theType != nullptr );

            engineInterface->typeSystem.UnregisterPlugin( theType, this->_genLockPlgOffset );
        }
    }

    inline rwlock* GetLock( EngineInterface *engineInterface, const RwObject *rwObj ) const
    {
        const GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromConstObject( rwObj );

        RwTypeSystem::typeInfoBase *theType = typeFetchStructoid_t::resolveType( engineInterface );

        return (rwlock*)RwTypeSystem::RESOLVE_STRUCT <rwlock> ( engineInterface, rtObj, theType, this->_genLockPlgOffset );
    }
};

inline void* GetObjectImplementationPointerForObject( void *headerMem, size_t headerSize, size_t objMemAlign )
{
    return (void*)ALIGN_SIZE( (size_t)headerMem + headerSize, objMemAlign );
}

inline size_t CalculateAlignedTypeSizeWithHeader( size_t headerSize, size_t objMemSize, size_t objMemAlign )
{
    size_t totalSize = headerSize;

    totalSize = ALIGN_SIZE( totalSize, objMemAlign );
    totalSize += objMemSize;

    return totalSize;
}

inline size_t CalculateAlignmentForObjectWithHeader( size_t headerAlign, size_t objMemAlign )
{
    return std::max( headerAlign, objMemAlign );
}


namespace rwFactRegPipes
{

struct rw_fact_pipeline_base
{
    AINLINE static RwTypeSystem& getTypeSystem( EngineInterface *intf )
    {
        return intf->typeSystem;
    }
};

template <typename constrType>
using rw_defconstr_fact_pipeline_base = factRegPipes::defconstr_fact_pipeline_base <EngineInterface, constrType>;

}

} // namespace rw

#endif //_PLUGIN_UTILITIES_
