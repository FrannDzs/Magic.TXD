/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.plg.h
*  PURPOSE:     RenderWare plugin structures and helpers
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_PLUGINS_HEADER_
#define _RENDERWARE_PLUGINS_HEADER_

#include <sdk/PluginFactory.h>

// For ptrdiff_t.
#include <stddef.h>

namespace rw
{

// Offset into a plugin container as returned by the runtime.
typedef ptrdiff_t typePluginOffset_t;

// Resolving a plugin from information.
void* ResolveTypePlugin( void *obj, const char *typeName, typePluginOffset_t off );
const void* ResolveConstTypePlugin( const void *obj, const char *typeName, typePluginOffset_t off );
void* ResolveTypePluginSafe( Interface *rwEngine, void *obj, const char *typeName, typePluginOffset_t off );
const void* ResolveConstTypePluginSafe( Interface *rwEngine, const void *obj, const char *typeName, typePluginOffset_t off );
bool IsTypePluginOffsetValid( typePluginOffset_t off ) noexcept;
typePluginOffset_t GetInvalidTypePluginOffset( void ) noexcept;

// Plugin descriptor type. Immutable structure.
// Valid as long as the underlying type is valid.
struct typePluginDescriptor
{
    inline typePluginDescriptor( const char *typeName )
    {
        this->typeName = typeName;
    }

    inline const char* GetTypeName( void ) const
    {
        return this->typeName;
    }

private:
    const char *typeName;

public:
    template <typename structType>
    AINLINE structType* RESOLVE_STRUCT( void *obj, typePluginOffset_t off, Interface *rwEngine )
    {
        return (structType*)ResolveTypePluginSafe( rwEngine, obj, this->typeName, off );
    }
    template <typename structType>
    AINLINE const structType* RESOLVE_STRUCT( const void *obj, typePluginOffset_t off, Interface *rwEngine )
    {
        return (const structType*)ResolveConstTypePluginSafe( rwEngine, obj, this->typeName, off );
    }
};

// Global definition of a plugin interface that is used to register custom memory extensions to
// Rasters, TexDictionaries, etc. Most of the time you do not have to implement this but it
// gives you full control over the implementation.
struct typePluginInterface
{
    virtual ~typePluginInterface( void )            {}

    virtual bool OnPluginConstruct( void *obj, typePluginOffset_t pluginOffset, typePluginDescriptor pluginId, Interface *rwEngine )
    {
        // By default, construction of plugins should succeed.
        return true;
    }

    virtual void OnPluginDestruct( void *obj, typePluginOffset_t pluginOffset, typePluginDescriptor pluginId, Interface *rwEngine )
    {
        return;
    }

    virtual bool OnPluginAssign( void *dstObj, const void *srcObj, typePluginOffset_t pluginOffset, typePluginDescriptor pluginId, Interface *rwEngine )
    {
        // Assignment of data to another plugin struct is optional.
        return false;
    }

    virtual void DeleteOnUnregister( void )
    {
        // Overwrite this method if unregistering should delete this class.
        return;
    }
};

// Methods to find out about registration of internal types.
bool IsTypeRegistered( Interface *rwEngine, const char *typeName );
rwStaticString <char> GetFullTypeName( const void *obj );

// Methods to register plugins.
typePluginOffset_t RegisterTypePlugin( Interface *rwEngine, const char *typeName, size_t pluginSize, size_t pluginAlignment, typePluginInterface *pluginIntf );
void UnregisterTypePlugin( Interface *rwEngine, const char *typeName, typePluginOffset_t off );

//===========================================================
// *** Helpers for plugin registration and obtainment. ***
//===========================================================

// Required for the dispatcher so we can easily use dependant struct helpers, etc from the Eir SDK.
struct engineTypePluginSystemView
{
    typedef typePluginOffset_t pluginOffset_t;
    typedef typePluginInterface pluginInterface;

    inline engineTypePluginSystemView( Interface *rwEngine ) noexcept
    {
        this->rwEngine = rwEngine;
    }

    template <typename interfaceType, typename... constrArgs>
    inline typePluginOffset_t RegisterCustomPlugin( size_t pluginSize, size_t pluginAlignment, const typePluginDescriptor& pluginId, constrArgs... args )
    {
        struct interfaceProxyType : public interfaceType
        {
            inline interfaceProxyType( Interface *rwEngine ) noexcept
            {
                this->rwEngine = rwEngine;
            }

            void DeleteOnUnregister( void ) override
            {
                RwDynMemAllocator memAlloc( this->rwEngine );

                eir::dyn_del_struct <interfaceProxyType> ( memAlloc, nullptr, this );
            }

            Interface *rwEngine;
        };

        Interface *rwEngine = this->rwEngine;

        RwDynMemAllocator memAlloc( rwEngine );

        interfaceProxyType *intf = eir::dyn_new_struct <interfaceProxyType> ( memAlloc, nullptr, rwEngine );

        try
        {
            typePluginOffset_t off = RegisterTypePlugin( rwEngine, pluginId.GetTypeName(), pluginSize, pluginAlignment, intf );

            if ( IsTypePluginOffsetValid( off ) == false )
            {
                eir::dyn_del_struct <interfaceProxyType> ( memAlloc, nullptr, intf );
            }

            return off;
        }
        catch( ... )
        {
            eir::dyn_del_struct <interfaceProxyType> ( memAlloc, nullptr, intf );

            throw;
        }
    }

    static inline bool IsOffsetValid( typePluginOffset_t off )
    {
        return IsTypePluginOffsetValid( off );
    }

private:
    Interface *rwEngine;
};

// Registration helper for generic plugins.
template <typename structType, bool isDependantStruct = false>
struct typePluginStructRegister
{
    typedef CommonPluginSystemDispatch <void, engineTypePluginSystemView, typePluginDescriptor, Interface*> PluginDispatchType;

    inline typePluginStructRegister( void ) noexcept
    {
        this->rwEngine = nullptr;
        this->pluginOffset = GetInvalidTypePluginOffset();
        this->wasRegisteredInConstructor = false;
    }
    inline typePluginStructRegister( Interface *rwEngine, const char *typeName )
    {
        this->rwEngine = nullptr;
        this->pluginOffset = GetInvalidTypePluginOffset();
        this->wasRegisteredInConstructor = this->RegisterPlugin( rwEngine, typeName );
    }

    inline typePluginStructRegister( typePluginStructRegister&& right ) noexcept
    {
        this->rwEngine = right.rwEngine;
        this->pluginOffset = right.pluginOffset;
        this->wasRegisteredInConstructor = right.wasRegisteredInConstructor;

        right.rwEngine = nullptr;
    }
    inline typePluginStructRegister( const typePluginStructRegister& ) = delete;

    inline ~typePluginStructRegister( void )
    {
        if ( this->wasRegisteredInConstructor )
        {
            this->UnregisterPlugin();
        }
    }

    inline typePluginStructRegister& operator = ( typePluginStructRegister&& right ) noexcept
    {
        this->UnregisterPlugin();

        this->rwEngine = right.rwEngine;
        this->pluginOffset = right.pluginOffset;
        this->wasRegisteredInConstructor = right.wasRegisteredInConstructor;

        return *this;
    }
    inline typePluginStructRegister& operator = ( const typePluginStructRegister& ) = delete;

    inline bool RegisterPlugin( Interface *rwEngine, const char *typeName )
    {
        // If we are already registered then just ignore.
        if ( IsRegistered() )
            return true;

        typePluginDescriptor pluginId( typeName );

        engineTypePluginSystemView systemView( rwEngine );

        typePluginOffset_t pluginOff;

        if constexpr ( isDependantStruct )
        {
            pluginOff = PluginDispatchType( systemView ).RegisterDependantStructPlugin <structType> ( pluginId );
        }
        else
        {
            pluginOff = PluginDispatchType( systemView ).RegisterStructPlugin <structType> ( pluginId );
        }

        if ( IsTypePluginOffsetValid( pluginOff ) == false )
        {
            return false;
        }

        this->pluginOffset = pluginOff;
        this->rwEngine = rwEngine;
        this->typeName = typeName;

        return true;
    }

    inline void UnregisterPlugin( void )
    {
        Interface *rwEngine = this->rwEngine;

        if ( rwEngine == nullptr )
            return;

        UnregisterTypePlugin( rwEngine, this->typeName, this->pluginOffset );

        this->rwEngine = nullptr;
        this->pluginOffset = GetInvalidTypePluginOffset();
        this->typeName = nullptr;
    }

    inline bool IsRegistered( void ) const
    {
        return ( this->rwEngine != nullptr );
    }

    AINLINE structType* GetPluginStruct( void *obj ) const
    {
        if ( IsRegistered() == false )
            return nullptr;

        return (structType*)ResolveTypePlugin( obj, this->typeName, this->pluginOffset );
    }

    AINLINE const structType* GetConstPluginStruct( const void *obj ) const
    {
        if ( IsRegistered() == false )
            return nullptr;

        return (const structType*)ResolveConstTypePlugin( obj, this->typeName, this->pluginOffset );
    }

private:
    Interface *rwEngine;
    typePluginOffset_t pluginOffset;
    const char *typeName;

    bool wasRegisteredInConstructor;
};

//===========================================================
// *** Direct RenderWare interface plugins. ***
//===========================================================

typedef ptrdiff_t interfacePluginOffset_t;

// Methods to resolve interface plugins.
void* ResolveInterfacePlugin( Interface *engine, interfacePluginOffset_t offset );
const void* ResolveConstInterfacePlugin( const Interface *engine, interfacePluginOffset_t offset );
bool IsInterfacePluginOffsetValid( interfacePluginOffset_t off ) noexcept;
interfacePluginOffset_t GetInvalidInterfacePluginOffset( void ) noexcept;

struct interfacePluginDescriptor
{
    template <typename structType>
    AINLINE static structType* RESOLVE_STRUCT( Interface *engine, interfacePluginOffset_t off )
    {
        return (structType*)ResolveInterfacePlugin( engine, off );
    }
    template <typename structType>
    AINLINE static const structType* RESOLVE_STRUCT( const Interface *engine, interfacePluginOffset_t off )
    {
        return (const structType*)ResolveConstInterfacePlugin( engine, off );
    }
};

struct interfacePluginInterface
{
    virtual ~interfacePluginInterface( void )            {}

    virtual bool OnPluginConstruct( Interface *rwEngine, interfacePluginOffset_t pluginOffset, interfacePluginDescriptor pluginId )
    {
        // By default, construction of plugins should succeed.
        return true;
    }

    virtual void OnPluginDestruct( Interface *rwEngine, interfacePluginOffset_t pluginOffset, interfacePluginDescriptor pluginId )
    {
        return;
    }

    virtual bool OnPluginAssign( Interface *dstEngine, const Interface *srcEngine, interfacePluginOffset_t pluginOffset, interfacePluginDescriptor pluginId )
    {
        // Assignment of data to another plugin struct is optional.
        return false;
    }

    virtual void DeleteOnUnregister( void )
    {
        // Overwrite this method if unregistering should delete this class.
        return;
    }
};

// Methods for plugin registration.
interfacePluginOffset_t RegisterInterfacePlugin( size_t pluginSize, size_t pluginAlignment, interfacePluginInterface *intf );
void UnregisterInterfacePlugin( interfacePluginOffset_t off );

//===========================================================
// *** Helpers for RenderWare interface plugins. ***
//===========================================================

// Very simple system view for engine plugin in the common dispatcher.
struct directEnginePluginSystemView
{
    typedef interfacePluginOffset_t pluginOffset_t;
    typedef interfacePluginInterface pluginInterface;

    template <typename interfaceType, typename... constrArgs>
    inline interfacePluginOffset_t RegisterCustomPlugin( size_t pluginSize, size_t pluginAlignment, const interfacePluginDescriptor& pluginId, constrArgs... args )
    {
        // Need a simple wrapper.
        struct wrapperInterface : public interfaceType
        {
            void DeleteOnUnregister( void ) override
            {
                eir::static_del_struct <wrapperInterface, RwStaticMemAllocator> ( nullptr, this );
            }
        };

        wrapperInterface *intf = eir::static_new_struct <wrapperInterface, RwStaticMemAllocator> ( nullptr );

        try
        {
            interfacePluginOffset_t off = RegisterInterfacePlugin( pluginSize, pluginAlignment, intf );

            if ( IsInterfacePluginOffsetValid( off ) == false )
            {
                eir::static_del_struct <wrapperInterface, RwStaticMemAllocator> ( nullptr, intf );
            }

            return off;
        }
        catch( ... )
        {
            eir::static_del_struct <wrapperInterface, RwStaticMemAllocator> ( nullptr, intf );

            throw;
        }
    }

    static inline bool IsOffsetValid( interfacePluginOffset_t off ) noexcept
    {
        return IsInterfacePluginOffsetValid( off );
    }
};

// Now for the type that can be put into static memory and initialize itself.
template <typename structType, bool isDependantStruct = false>
struct interfacePluginStructRegister
{
    typedef CommonPluginSystemDispatch <Interface, directEnginePluginSystemView, interfacePluginDescriptor> PluginDispatchType;

    inline interfacePluginStructRegister( void )
    {
        this->offset = GetInvalidInterfacePluginOffset();

        this->RegisterPlugin();
    }
    
    inline interfacePluginStructRegister( interfacePluginStructRegister&& right ) noexcept
    {
        this->offset = right.offset;

        right.offset = GetInvalidInterfacePluginOffset();
    }
    inline interfacePluginStructRegister( const interfacePluginStructRegister& ) = delete;

    inline ~interfacePluginStructRegister( void )
    {
        this->UnregisterPlugin();
    }

    inline bool RegisterPlugin( void )
    {
        if ( this->IsRegistered() )
            return true;

        interfacePluginDescriptor desc;

        directEnginePluginSystemView systemView;

        // Boot up engine essentials.
        ReferenceEngineEnvironment();

        interfacePluginOffset_t off;

        if constexpr ( isDependantStruct )
        {
            off = PluginDispatchType( systemView ).RegisterDependantStructPlugin <structType> ( desc );
        }
        else
        {
            off = PluginDispatchType( systemView ).RegisterStructPlugin <structType> ( desc );
        }

        if ( IsInterfacePluginOffsetValid( off ) == false )
        {
            // Do not need engine essentials anymore.
            DereferenceEngineEnvironment();
            return false;
        }

        this->offset = off;
        return true;
    }

    inline void UnregisterPlugin( void )
    {
        if ( IsRegistered() )
        {
            UnregisterInterfacePlugin( this->offset );

            // Clean up.
            DereferenceEngineEnvironment();
        }
    }

    inline bool IsRegistered( void )
    {
        return ( IsInterfacePluginOffsetValid( this->offset ) );
    }

    inline structType* GetPluginStruct( Interface *engine )
    {
        if ( IsRegistered() == false )
            return nullptr;

        return (structType*)ResolveInterfacePlugin( engine, this->offset );
    }

    inline const structType* GetConstPluginStruct( Interface *engine )
    {
        if ( IsRegistered() == false )
            return nullptr;

        return (const structType*)ResolveConstInterfacePlugin( engine, this->offset );
    }

private:
    interfacePluginOffset_t offset;
};

} // namespace rw

#endif //_RENDERWARE_PLUGINS_HEADER_