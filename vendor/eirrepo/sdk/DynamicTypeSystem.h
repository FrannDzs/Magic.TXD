/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/DynamicTypeSystem.h
*  PURPOSE:     Dynamic runtime type abstraction system.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _DYN_TYPE_ABSTRACTION_SYSTEM_
#define _DYN_TYPE_ABSTRACTION_SYSTEM_

// Memory layout documentation: https://eirrepo.osdn.jp/documentation/dts_memory_layout.pdf

#include "Exceptions.h"
#include "rwlist.hpp"
#include "PluginFactory.h"
#include "MemoryRaw.h"  // for ALIGN_SIZE.

#include "eirutils.h"
#include "String.h"
#include "MetaHelpers.h"
#include "AVLTree.h"

#include "UniChar.h"

#include <tuple>

// For C++ RTTI support.
#include <typeinfo>
#include <typeindex>
#include <optional>

// Type sentry struct of the dynamic type system.
// It notes the programmer that the struct has RTTI.
// Every type constructed through DynamicTypeSystem has this struct before it.
struct GenericRTTI
{
#ifdef _DEBUG
    void *typesys_ptr;      // pointer to the DynamicTypeSystem manager (for debugging)
#endif //_DEBUG
    void *type_meta;        // pointer to the struct that denotes the runtime type
};

// Assertions that can be thrown by the type system
#ifndef rtti_assert
#include <assert.h>

#define rtti_assert( x )    assert( x )
#endif //rtti_assert

// TODO: think about creating a concept that would guide the developers for proper
// DTS lock-provider design.
struct dtsDefaultLockProvider
{
    typedef void rwlock;

    inline rwlock* CreateLock( void ) const
    {
        return nullptr;
    }

    inline void CloseLock( rwlock *lock ) const
    {
        // noop.
    }

    inline void LockEnterRead( rwlock *lock ) const
    {
        // noop.
    }

    inline void LockLeaveRead( rwlock *lock ) const
    {
        // noop.
    }

    inline void LockEnterWrite( rwlock *lock ) const
    {
        // noop.
    }

    inline void LockLeaveWrite( rwlock *lock ) const
    {
        // noop.
    }
};

template <typename structType>
concept DTSUsableType =
    std::is_nothrow_destructible <structType>::value;

#define DTS_TEMPLARGS \
    template <eir::MemoryAllocator allocatorType, typename systemPointer, typename lockProvider_t = dtsDefaultLockProvider, PluginStructRegistryFlavor <GenericRTTI> structFlavorDispatchType = cachedMinimalStructRegistryFlavor <GenericRTTI>, typename exceptMan = eir::DefaultExceptionManager>
#define DTS_TEMPLARGS_NODEF \
    template <eir::MemoryAllocator allocatorType, typename systemPointer, typename lockProvider_t, PluginStructRegistryFlavor <GenericRTTI> structFlavorDispatchType, typename exceptMan>
#define DTS_TEMPLUSE \
    <allocatorType, systemPointer, lockProvider_t, structFlavorDispatchType, exceptMan>

// This class manages runtime type information.
// It allows for dynamic C++ class extension depending on runtime conditions.
// Main purpose for its creation are tight memory requirements.
DTS_TEMPLARGS
struct DynamicTypeSystem
{
    static constexpr unsigned int ANONYMOUS_PLUGIN_ID = 0xFFFFFFFF;

    // Export the system type so extensions can use it.
    typedef systemPointer systemPointer_t;

    // Exceptions thrown by this system.
    class abstraction_construction_exception    {};
    class type_name_conflict_exception          {};

    struct typeInfoBase;

private:
    [[no_unique_address]] allocatorType allocMan;

    // Lock provider for MT support.
    typedef typename lockProvider_t::rwlock rwlock;

    [[no_unique_address]] lockProvider_t lockProvider;

    // Lock used when using fields of the type system itself.
    rwlock *mainLock;

    // List of all registered types.
    RwList <typeInfoBase> registeredTypes;

    // Mapping to C++ language RTTI.
    struct langrtti_compare_dispatcher
    {
        static AINLINE eir::eCompResult CompareNodes( const AVLNode *left, const AVLNode *right ) noexcept
        {
            const typeInfoBase *left_ti = AVL_GETITEM( typeInfoBase, left, lang_type_node );
            const typeInfoBase *right_ti = AVL_GETITEM( typeInfoBase, right, lang_type_node );

            return eir::DefaultValueCompare( left_ti->lang_type_idx.value(), right_ti->lang_type_idx.value() );
        }

        static AINLINE eir::eCompResult CompareNodeWithValue( const AVLNode *left, const std::type_index& right ) noexcept
        {
            const typeInfoBase *left_ti = AVL_GETITEM( typeInfoBase, left, lang_type_node );

            return eir::DefaultValueCompare( left_ti->lang_type_idx.value(), right );
        }
    };
    AVLTree <langrtti_compare_dispatcher> lang_rtti_tree;

public:
    template <typename... lockProviderArgs, typename... allocArgs>
        requires ( eir::constructible_from <lockProvider_t, lockProviderArgs...> && eir::constructible_from <allocatorType, allocArgs...> )
    inline DynamicTypeSystem( std::tuple <lockProviderArgs...>&& lpArgs, std::tuple <allocArgs...>&& a_args = std::tuple() )
        noexcept(eir::nothrow_constructible_from <lockProvider_t, lockProviderArgs...> && eir::nothrow_constructible_from <allocatorType, allocArgs...>)
        : lockProvider( std::make_from_tuple <lockProvider_t> ( std::move( lpArgs ) ) ), allocMan( std::make_from_tuple <allocatorType> ( std::move( a_args ) ) )
    {
        this->mainLock = this->lockProvider.CreateLock();
    }
    inline DynamicTypeSystem( void ) noexcept requires ( eir::nothrow_constructible_from <lockProvider_t> && eir::nothrow_constructible_from <allocatorType> )
        : DynamicTypeSystem( std::tuple(), std::tuple() )
    {
        return;
    }
    template <typename... allocArgs>
        requires ( eir::nothrow_constructible_from <lockProvider_t> && eir::constructible_from <allocatorType, allocArgs...> )
    inline DynamicTypeSystem( eir::constr_with_alloc _, allocArgs&&... a_args )
        : DynamicTypeSystem( std::tuple(), std::tuple <allocArgs&&...> ( std::forward <allocArgs> ( a_args )... ) )
    {
        return;
    }
    inline DynamicTypeSystem( const DynamicTypeSystem& ) = delete;

private:
    AINLINE void _update_pointers( void ) noexcept
    {
        LIST_FOREACH_BEGIN( typeInfoBase, this->registeredTypes.root, node )
            item->typeSys = this;
        LIST_FOREACH_END
    }

public:
    inline DynamicTypeSystem( DynamicTypeSystem&& right ) noexcept
        requires ( eir::nothrow_constructible_from <lockProvider_t, lockProvider_t&&> )
        : allocMan( std::move( right.allocMan ) ), lockProvider( std::move( right.lockProvider ) ), mainLock( right.mainLock ),
          registeredTypes( std::move( right.registeredTypes ) ),
          lang_rtti_tree( std::move( right.lang_rtti_tree ) )
    {
        this->_update_pointers();

        right.mainLock = nullptr;
    }

    inline DynamicTypeSystem& operator = ( const DynamicTypeSystem& ) = delete;
    inline DynamicTypeSystem& operator = ( DynamicTypeSystem&& right ) noexcept
        requires ( std::is_nothrow_assignable <lockProvider_t&, lockProvider_t&&>::value )
    {
        this->allocMan = std::move( right.allocMan );
        this->lockProvider = std::move( right.lockProvider );
        this->mainLock = right.mainLock;
        this->registeredTypes = std::move( right.registeredTypes );
        this->lang_rtti_tree = std::move( right.lang_rtti_tree );

        this->_update_pointers();

        right.mainLock = nullptr;

        return *this;
    }

    inline void Shutdown( void )
    {
        // TODO: we need a no-lock variant of DeleteType.
        
        // Clean up our memory by deleting all types.
        while ( !LIST_EMPTY( registeredTypes.root ) )
        {
            typeInfoBase *info = LIST_GETITEM( typeInfoBase, registeredTypes.root.next, node );

            DeleteType( info );
        }

        // Remove our lock.
        if ( rwlock *sysLock = this->mainLock )
        {
            this->lockProvider.CloseLock( sysLock );

            this->mainLock = nullptr;
        }
    }

    inline ~DynamicTypeSystem( void )
    {
        Shutdown();
    }

    struct scoped_rwlock_read
    {
        scoped_rwlock_read( const scoped_rwlock_read& ) = delete;

        inline scoped_rwlock_read( const lockProvider_t& provider, rwlock *theLock ) : lockProvider( provider )
        {
            if ( theLock )
            {
                provider.LockEnterRead( theLock );
            }

            this->theLock = theLock;
        }

        inline ~scoped_rwlock_read( void )
        {
            if ( rwlock *theLock = this->theLock )
            {
                lockProvider.LockLeaveRead( theLock );

                this->theLock = nullptr;
            }
        }

    private:
        const lockProvider_t& lockProvider;
        rwlock *theLock;
    };

    struct scoped_rwlock_write
    {
        scoped_rwlock_write( const scoped_rwlock_write& ) = delete;

        inline scoped_rwlock_write( const lockProvider_t& provider, rwlock *theLock ) : lockProvider( provider )
        {
            if ( theLock )
            {
                provider.LockEnterWrite( theLock );
            }

            this->theLock = theLock;
        }

        inline ~scoped_rwlock_write( void )
        {
            if ( rwlock *theLock = this->theLock )
            {
                lockProvider.LockLeaveWrite( theLock );

                this->theLock = nullptr;
            }
        }

    private:
        const lockProvider_t& lockProvider;
        rwlock *theLock;
    };

public:
    // Interface for type lifetime management. All registered types expose this interface.
    // THIS INTERFACE MUST BE THREAD-SAFE! This means that it has to provide it's own locks, when necessary!
    struct typeInterface abstract
    {
        // Methods for dynamic non-RTTI lifetime management.
        virtual void Construct( void *mem, systemPointer_t *sysPtr, void *construct_params ) const = 0;
        virtual void CopyConstruct( void *mem, const void *srcMem ) const = 0;
        virtual void MoveConstruct( void *mem, void *srcMem ) const = 0;
        virtual void Destruct( void *mem ) const noexcept = 0;

        // Methods for data assignment, in case of dynamic non-RTTI operation.
        virtual void CopyAssign( void *mem, const void *srcMem ) const = 0;
        virtual void MoveAssign( void *mem, void *srcMem ) const = 0;

        // The type size of objects is assumed to be an IMMUTABLE property.
        // Changing the type size of an object leads to undefined behavior.
        virtual size_t GetTypeSize( systemPointer_t *sysPtr, void *construct_params ) const noexcept = 0;
        virtual size_t GetTypeSizeByObject( systemPointer_t *sysPtr, const void *mem ) const noexcept = 0;

        // Alignment has to be the same across the board because we need to have the ability to retrieve
        // the object from the type struct without knowing the object itself.
        virtual size_t GetTypeAlignment( systemPointer_t *sysPtr ) const noexcept = 0;
    };

    // THREAD-SAFE, because it is an IMMUTABLE struct.
    struct pluginDescriptor
    {
        friend struct DynamicTypeSystem;

        typedef ptrdiff_t pluginOffset_t;

        inline pluginDescriptor( void ) noexcept
        {
            this->pluginId = DynamicTypeSystem::ANONYMOUS_PLUGIN_ID;
            this->typeInfo = nullptr;
        }

        inline pluginDescriptor( typeInfoBase *typeInfo ) noexcept
        {
            this->pluginId = DynamicTypeSystem::ANONYMOUS_PLUGIN_ID;
            this->typeInfo = typeInfo;
        }

        inline pluginDescriptor( unsigned int id, typeInfoBase *typeInfo ) noexcept
        {
            this->pluginId = id;
            this->typeInfo = typeInfo;
        }

        inline pluginDescriptor( const pluginDescriptor& ) = default;

    private:
        // Plugin descriptors are immutable beyond construction.
        unsigned int pluginId;
        typeInfoBase *typeInfo;

    public:
        template <typename pluginStructType>
        AINLINE pluginStructType* RESOLVE_STRUCT( GenericRTTI *object, pluginOffset_t offset, systemPointer_t *sysPtr )
        {
            return DynamicTypeSystem::RESOLVE_STRUCT <pluginStructType> ( sysPtr, object, this->typeInfo, offset );
        }

        template <typename pluginStructType>
        AINLINE const pluginStructType* RESOLVE_STRUCT( const GenericRTTI *object, pluginOffset_t offset, systemPointer_t *sysPtr )
        {
            return DynamicTypeSystem::RESOLVE_STRUCT <const pluginStructType> ( sysPtr, object, this->typeInfo, offset );
        }
    };

private:
    // Need to redirect the typeInfoBase structRegistry allocations to the DynamicTypeSystem allocator.
    DEFINE_HEAP_REDIR_ALLOC_BYSTRUCT( structRegRedirAlloc, allocatorType );

public:
    typedef AnonymousPluginStructRegistry <GenericRTTI, pluginDescriptor, structFlavorDispatchType, structRegRedirAlloc, systemPointer_t*> structRegistry_t;

    // Localize important struct details.
    typedef typename pluginDescriptor::pluginOffset_t pluginOffset_t;
    typedef typename structRegistry_t::pluginInterface pluginInterface;

    static constexpr pluginOffset_t INVALID_PLUGIN_OFFSET = (pluginOffset_t)-1;

    // THREAD-SAFE, only as long as it is used only through DynamicTypeSystem!
    struct typeInfoBase abstract
    {
        friend struct DynamicTypeSystem;
        
    private:
        inline typeInfoBase( void ) noexcept
        {
            // Initialization is performed inside SetupTypeInfoBase.
            return;
        }

        // We do not allow copies and moves.
        typeInfoBase( const typeInfoBase& ) = delete;
        typeInfoBase( typeInfoBase&& ) = delete;

    protected:
        virtual ~typeInfoBase( void )
        {
            rtti_assert( this->lang_type_idx.has_value() == false );
        }

    private:
        typeInfoBase& operator = ( const typeInfoBase& ) = delete;
        typeInfoBase& operator = ( typeInfoBase&& ) = delete;

    public:
        // Returns true if this type must not be changed.
        inline bool IsImmutable( void ) const noexcept
        {
            return ( this->refCount != 0 );
        }

        // Returns true if this type is not being inherited from.
        inline bool IsEndType( void ) const noexcept
        {
            return ( this->inheritanceCount == 0 );
        }

        // Returns the static zero-terminated-string that was assigned to the type at initialization.
        inline const char* GetInternalName( void ) const noexcept
        {
            return this->name;
        }

        // Returns the plugin struct registry of this type. Plugin information can
        // be used to perform additional tasks as described by the plugin interfaces.
        inline const structRegistry_t& GetPluginRegistry( void ) const noexcept
        {
            return this->structRegistry;
        }

        // Returns the type that we inherit from, otherwise nullptr.
        inline typeInfoBase* GetBaseType( void ) const noexcept
        {
            return this->inheritsFrom;
        }

    protected:
        // Helper for the Cleanup method.
        typedef allocatorType typeInfoAllocatorType;

        // Called whenever this type should be deleted.
        // Takes care of any destruction, even of this class itself.
        virtual void Cleanup( void ) noexcept = 0;

    private:
        DynamicTypeSystem *typeSys;

        const char *name;   // name of this type
        typeInterface *tInterface;  // type construction information

        unsigned long refCount;    // number of entities that use this type

        // WARNING: as long as a type is referenced, it MUST not change!!!
        unsigned long inheritanceCount; // number of types inheriting from this type

        // Special properties.
        bool isExclusive;       // can be used by the runtime to control the creation of objects.
        bool isAbstract;        // can this type be constructed (set internally)

        // Inheritance information.
        typeInfoBase *inheritsFrom; // type that this type inherits from

        // Plugin information.
        structRegistry_t structRegistry;

        // Lock used when accessing this type itself.
        rwlock *typeLock;

        RwListEntry <typeInfoBase> node;

        // C++ RTTI support.
        std::optional <std::type_index> lang_type_idx;
        AVLNode lang_type_node;
    };

    AINLINE static typeInfoBase* GetTypeInfoFromTypeStruct( const GenericRTTI *rtObj ) noexcept
    {
        return (typeInfoBase*)rtObj->type_meta;
    }

    AINLINE static DynamicTypeSystem* GetTypeSystemFromTypeInfo( typeInfoBase *typeInfo ) noexcept
    {
        return typeInfo->typeSys;
    }

    // Function to get the plugin memory offset on an object of a specific type.
    // The offset should be added to the GenericRTTI pointer to receive the memory buffer.
    // Undefined behaviour if there is no memory buffer for the plugins.
    AINLINE static pluginOffset_t GetTypeInfoStructOffset( systemPointer_t *sysPtr, GenericRTTI *rtObj, typeInfoBase *offsetInfo )
    {
        // This method is thread safe, because every operation is based on immutable data or is atomic already.

        size_t baseOffset = 0;

        // Get generic type information.
        typeInfoBase *subclassTypeInfo = GetTypeInfoFromTypeStruct( rtObj );

        baseOffset += sizeof( GenericRTTI );
        
        typeInterface *tInterface = subclassTypeInfo->tInterface;

        // Align the type memory properly.
        size_t baseStructAlignment = tInterface->GetTypeAlignment( sysPtr );

        baseOffset = ALIGN_SIZE( baseOffset, baseStructAlignment );

        // Get the pointer to the language object.
        void *langObj = GetObjectFromTypeStruct( rtObj, baseStructAlignment );

        // Add the dynamic size of the language object.
        baseOffset += tInterface->GetTypeSizeByObject( sysPtr, langObj );

        // Add the plugin offset.
        {
            DynamicTypeSystem *typeSys = subclassTypeInfo->typeSys;

            scoped_rwlock_read baseLock( typeSys->lockProvider, offsetInfo->typeLock );

            // Align by the plugin block.
            if ( typeInfoBase *inheritedType = offsetInfo->inheritsFrom )
            {
                size_t pluginAlignment_unused;
                AdvanceTypeStructPluginSize( inheritedType, rtObj, baseOffset, pluginAlignment_unused );
            }

            rtti_assert( offsetInfo->structRegistry.GetPluginSizeByObject( rtObj ) > 0 );

            // Now align.
            size_t lastPluginBlockAlignment = offsetInfo->structRegistry.GetPluginAlignment();

            baseOffset = ALIGN_SIZE( baseOffset, lastPluginBlockAlignment );
        }

        // Calculate the offset of rtObj to the allocation start.
        size_t allocStartPtr = (size_t)GetAllocationFromTypeStruct( rtObj, baseStructAlignment );
        size_t rtObj_allocOff = (size_t)rtObj - allocStartPtr;

        // Need to subtract it.
        baseOffset -= rtObj_allocOff;

        return (pluginOffset_t)baseOffset;
    }

    AINLINE static bool IsOffsetValid( pluginOffset_t offset ) noexcept
    {
        return ( offset != INVALID_PLUGIN_OFFSET );
    }

    // Struct resolution methods are thread safe.
    template <typename pluginStructType>
    AINLINE static pluginStructType* RESOLVE_STRUCT( systemPointer_t *sysPtr, GenericRTTI *rtObj, typeInfoBase *typeInfo, pluginOffset_t offset )
    {
        if ( !IsOffsetValid( offset ) )
            return nullptr;

        size_t baseOffset = GetTypeInfoStructOffset( sysPtr, rtObj, typeInfo );

        pluginOffset_t realOffset = GetTypeRegisteredPluginLocation( typeInfo, rtObj, offset );

        return (pluginStructType*)( (char*)rtObj + baseOffset + realOffset );
    }

    template <typename pluginStructType>
    AINLINE static const pluginStructType* RESOLVE_STRUCT( systemPointer_t *sysPtr, const GenericRTTI *rtObj, typeInfoBase *typeInfo, pluginOffset_t offset )
    {
        if ( !IsOffsetValid( offset ) )
            return nullptr;

        size_t baseOffset = GetTypeInfoStructOffset( sysPtr, (GenericRTTI*)rtObj, typeInfo );

        pluginOffset_t realOffset = GetTypeRegisteredPluginLocation( typeInfo, rtObj, offset );

        return (const pluginStructType*)( (char*)rtObj + baseOffset + realOffset );
    }

    // Function used to register a new plugin struct into the class.
    inline pluginOffset_t RegisterPlugin( size_t pluginSize, size_t pluginAlignment, const pluginDescriptor& descriptor, pluginInterface *plugInterface )
    {
        typeInfoBase *typeInfo = descriptor.typeInfo;

        scoped_rwlock_write lock( this->lockProvider, typeInfo->typeLock );

        rtti_assert( typeInfo->IsImmutable() == false );

        return typeInfo->structRegistry.RegisterPlugin( pluginSize, pluginAlignment, descriptor, plugInterface );
    }

    // Function used to register custom plugins whose lifetime is managed by DTS itself.
    template <typename structTypeInterface, typename... Args>
        requires ( std::is_nothrow_destructible <structTypeInterface>::value && eir::constructible_from <structTypeInterface, Args...> &&
                   std::is_base_of <pluginInterface, structTypeInterface>::value )
    inline pluginOffset_t RegisterCustomPlugin( size_t pluginSize, size_t pluginAlignment, const pluginDescriptor& descriptor, Args&&... constrArgs )
    {
        struct selfDestroyingInterface : public structTypeInterface
        {
            inline selfDestroyingInterface( DynamicTypeSystem *typeSys, Args&&... constrArgs ) : structTypeInterface( std::forward <Args> ( constrArgs )... )
            {
                this->typeSys = typeSys;
            }

            void DeleteOnUnregister( void ) noexcept override
            {
                DynamicTypeSystem *typeSys = this->typeSys;

                eir::dyn_del_struct <selfDestroyingInterface> ( typeSys->allocMan, typeSys, this );
            }

            DynamicTypeSystem *typeSys;
        };

        selfDestroyingInterface *pluginInfo = eir::dyn_new_struct <selfDestroyingInterface, exceptMan> ( this->allocMan, this, this, std::forward <Args> ( constrArgs )... );

        try
        {
            return RegisterPlugin( pluginSize, pluginAlignment, descriptor, pluginInfo );
        }
        catch( ... )
        {
            eir::dyn_del_struct <selfDestroyingInterface> ( this->allocMan, this, pluginInfo );

            throw;
        }
    }

    inline void UnregisterPlugin( typeInfoBase *typeInfo, pluginOffset_t pluginOffset )
    {
        scoped_rwlock_write lock( this->lockProvider, typeInfo->typeLock );

        rtti_assert( typeInfo->IsImmutable() == false );

        typeInfo->structRegistry.UnregisterPlugin( pluginOffset );
    }

    // Plugin registration functions.
    typedef CommonPluginSystemDispatch <GenericRTTI, DynamicTypeSystem, pluginDescriptor, systemPointer_t*> functoidHelper_t;

    template <PluginableStruct structType>
    inline pluginOffset_t RegisterStructPlugin( typeInfoBase *typeInfo, unsigned int pluginId = ANONYMOUS_PLUGIN_ID, size_t structSize = sizeof(structType), size_t alignment = alignof(structType) )
    {
        pluginDescriptor descriptor( pluginId, typeInfo );

        return functoidHelper_t( *this ).template RegisterStructPlugin <structType> ( descriptor, structSize, alignment );
    }

    template <PluginableStruct structType>
    inline pluginOffset_t RegisterDependantStructPlugin( typeInfoBase *typeInfo, unsigned int pluginId = ANONYMOUS_PLUGIN_ID, size_t structSize = sizeof(structType), size_t alignment = alignof(structType) )
    {
        pluginDescriptor descriptor( pluginId, typeInfo );

        return functoidHelper_t( *this ).template RegisterDependantStructPlugin <structType> ( descriptor, structSize, alignment );
    }

    // TODO: make this export conditional if the flavor is of conditional.
    typedef typename functoidHelper_t::conditionalPluginStructInterface conditionalPluginStructInterface;

    template <PluginableStruct structType>
    inline pluginOffset_t RegisterDependantConditionalStructPlugin( typeInfoBase *typeInfo, unsigned int pluginId, conditionalPluginStructInterface *conditional, size_t structSize = sizeof(structType), size_t alignment = alignof(structType) )
        requires ( std::same_as <structFlavorDispatchType, conditionalStructRegistryFlavor <GenericRTTI>> )
    {
        pluginDescriptor descriptor( pluginId, typeInfo );

        return functoidHelper_t( *this ).template RegisterDependantConditionalStructPlugin <structType> ( descriptor, conditional, structSize, alignment );
    }

private:
    inline void SetupTypeInfoBase( typeInfoBase *tInfo, const char *typeName, typeInterface *tInterface, typeInfoBase *inheritsFrom )
    {
        scoped_rwlock_write lock( this->lockProvider, this->mainLock );

        // If we find a type info with this name already, throw an exception.
        {
            typeInfoBase *alreadyExisting = FindTypeInfoNolock( typeName, inheritsFrom );

            if ( alreadyExisting != nullptr )
            {
                exceptMan::throw_type_name_conflict();
            }
        }

        tInfo->typeSys = this;
        tInfo->name = typeName;
        tInfo->refCount = 0;
        tInfo->inheritanceCount = 0;
        tInfo->isExclusive = false;
        tInfo->isAbstract = false;
        tInfo->tInterface = tInterface;
        tInfo->inheritsFrom = nullptr;
        tInfo->typeLock = this->lockProvider.CreateLock();
        LIST_APPEND( this->registeredTypes.root, tInfo->node );

        // Set inheritance.
        try
        {
            this->SetTypeInfoInheritingClass( tInfo, inheritsFrom, false );
        }
        catch( ... )
        {
            if ( tInfo->typeLock )
            {
                lockProvider.CloseLock( tInfo->typeLock );
            }

            LIST_REMOVE( tInfo->node );

            throw;
        }
    }

public:
    // Already THREAD-SAFE, because memory allocation is THREAD-SAFE and type registration is THREAD-SAFE.
    inline typeInfoBase* RegisterType( const char *typeName, typeInterface *typeInterface, typeInfoBase *inheritsFrom = nullptr )
    {
        struct typeInfoGeneral final : public typeInfoBase
        {
            void Cleanup( void ) noexcept override
            {
                DynamicTypeSystem *typeSys = this->typeSys;

                // Terminate ourselves.
                eir::dyn_del_struct <typeInfoGeneral> ( typeSys->allocMan, typeSys, this );
            }
        };

        typeInfoGeneral *info = eir::dyn_new_struct <typeInfoGeneral, exceptMan> ( this->allocMan, this );

        try
        {
            SetupTypeInfoBase( info, typeName, typeInterface, inheritsFrom );
        }
        catch( ... )
        {
            eir::dyn_del_struct <typeInfoGeneral> ( this->allocMan, this, info );

            throw;
        }

        return info;
    }

    template <typename structTypeTypeInterface>
    struct commonTypeInfoStruct : public typeInfoBase
    {
        friend struct DynamicTypeSystem;
        
    private:
        template <typename... constrArgs> requires ( eir::constructible_from <structTypeTypeInterface, constrArgs...> )
        AINLINE commonTypeInfoStruct( constrArgs&&... args )
            : tInterface( std::forward <constrArgs> ( args )... )
        {
            return;
        }

    protected:
        void Cleanup( void ) noexcept final override
        {
            DynamicTypeSystem *typeSys = this->typeSys;

            eir::dyn_del_struct <commonTypeInfoStruct> ( typeSys->allocMan, typeSys, this );
        }

    public:
        structTypeTypeInterface tInterface;
    };

    // THREAD-SAFE because memory allocation is THREAD-SAFE and type registration is THREAD-SAFE.
    template <typename structTypeTypeInterface, typename... constrArgs>
        requires ( eir::constructible_from <structTypeTypeInterface, constrArgs...> )
    inline commonTypeInfoStruct <structTypeTypeInterface>* RegisterCommonTypeInterface( const char *typeName, typeInfoBase *inheritsFrom = nullptr, constrArgs&&... args )
    {
        struct typeInfoStruct final : commonTypeInfoStruct <structTypeTypeInterface>
        {
            AINLINE typeInfoStruct( constrArgs&&... args ) : commonTypeInfoStruct( std::forward <constrArgs> ( args )... )
            {
                return;
            }
        };

        typeInfoStruct *tInfo = eir::dyn_new_struct <typeInfoStruct, exceptMan> ( this->allocMan, this, std::forward <constrArgs> ( args )... );

        try
        {
            SetupTypeInfoBase( tInfo, typeName, &tInfo->tInterface, inheritsFrom );
        }
        catch( ... )
        {
            eir::dyn_del_struct <typeInfoStruct> ( this->allocMan, this, tInfo );

            throw;
        }

        return tInfo;
    }

private:
    template <typename langType>
    AINLINE void RegisterTypeToRTTI( typeInfoBase *typeInfo ) noexcept
    {
#ifdef _DEBUG
        rtti_assert( typeInfo->lang_type_idx.has_value() == false );
#endif //_DEBUG

        typeInfo->lang_type_idx = std::type_index( typeid( langType ) );

        this->lang_rtti_tree.Insert( &typeInfo->lang_type_node );
    }

public:
    template <typename structType>
    inline typeInfoBase* RegisterAbstractType( const char *typeName, typeInfoBase *inheritsFrom = nullptr )
    {
        struct structTypeInterface : public typeInterface
        {
            void Construct( void *mem, systemPointer_t *sysPtr, void *construct_params ) const override
            {
                exceptMan::throw_abstraction_construction();
            }

            void CopyConstruct( void *mem, const void *srcMem ) const override
            {
                exceptMan::throw_abstraction_construction();
            }

            void MoveConstruct( void *mem, void *srcMem ) const override
            {
                exceptMan::throw_abstraction_construction();
            }

            void Destruct( void *mem ) const noexcept override
            {
                return;
            }

            void CopyAssign( void *mem, const void *srcMem ) const override
            {
                exceptMan::throw_abstraction_construction();
            }

            void MoveAssign( void *mem, void *srcMem ) const override
            {
                exceptMan::throw_abstraction_construction();
            }

            size_t GetTypeSize( systemPointer_t *sysPtr, void *construct_params ) const noexcept override
            {
                return (size_t)0;
            }

            size_t GetTypeSizeByObject( systemPointer_t *sysPtr, const void *langObj ) const noexcept override
            {
                return (size_t)0;
            }

            size_t GetTypeAlignment( systemPointer_t *sysPtr ) const noexcept override
            {
                return 1;
            }
        };

        typeInfoBase *newTypeInfo = this->RegisterCommonTypeInterface <structTypeInterface> ( typeName, inheritsFrom );

        if ( newTypeInfo )
        {
            // WARNING: if you allow construction of types while types register themselves THIS IS A SECURITY ISSUE.
            // WE DO NOT DO THAT.
            newTypeInfo->isAbstract = true;

            this->RegisterTypeToRTTI <structType> ( newTypeInfo );
        }

        return newTypeInfo;
    }

private:
    // THREAD-SAFE Helper for construction.
    template <DTSUsableType structType>
    static AINLINE void try_construct( void *mem, systemPointer_t *sysPtr, void *construct_params )
    {
        if constexpr ( eir::constructible_from <structType, systemPointer_t*, void*> )
        {
            new (mem) structType( sysPtr, construct_params );
        }
        else if constexpr ( eir::constructible_from <structType> )
        {
            new (mem) structType;
        }
        else
        {
            exceptMan::throw_undefined_method( eir::eMethodType::DEFAULT_CONSTRUCTOR );
        }
    }

    // THREAD-SAFE Helper for copy construction.
    template <DTSUsableType structType>
    static AINLINE void try_copy_construct( void *dstMem, const structType& right )
    {
        if constexpr ( eir::constructible_from <structType, const structType&> )
        {
            new (dstMem) structType( right );
        }
        else
        {
            exceptMan::throw_undefined_method( eir::eMethodType::COPY_CONSTRUCTOR );
        }
    }

    // THREAD-SAFE Helper for move construction.
    template <DTSUsableType structType>
    static AINLINE void try_move_construct( void *dstMem, structType&& right )
    {
        if constexpr ( eir::constructible_from <structType, structType&&> )
        {
            new (dstMem) structType( std::move( right ) );
        }
        else
        {
            exceptMan::throw_undefined_method( eir::eMethodType::MOVE_CONSTRUCTOR );
        }
    }

    template <DTSUsableType structType>
    static AINLINE void try_copy_assign( structType& dst, const structType& right )
    {
        if constexpr ( std::is_assignable <structType&, const structType&>::value )
        {
            dst = right;
        }
        else
        {
            exceptMan::throw_undefined_method( eir::eMethodType::COPY_ASSIGNMENT );
        }
    }

    template <DTSUsableType structType>
    static AINLINE void try_move_assign( structType& dst, structType&& right )
    {
        if constexpr ( std::is_assignable <structType&, structType&&>::value )
        {
            dst = std::move( right );
        }
        else
        {
            exceptMan::throw_undefined_method( eir::eMethodType::MOVE_ASSIGNMENT );
        }
    }

public:
    // THREAD-SAFE, because memory allocation is THREAD-SAFE and type registration is THREAD-SAFE.
    template <DTSUsableType structType>
    inline typeInfoBase* RegisterStructType( const char *typeName, typeInfoBase *inheritsFrom = nullptr, size_t structSize = sizeof( structType ), size_t alignment = alignof( structType ) )
    {
        struct structTypeInterface : public typeInterface
        {
            AINLINE structTypeInterface( size_t structSize, size_t alignment ) noexcept
            {
                this->structSize = structSize;
                this->alignment = alignment;
            }

            void Construct( void *mem, systemPointer_t *sysPtr, void *construct_params ) const override
            {
                try_construct <structType> ( mem, sysPtr, construct_params );
            }

            void CopyConstruct( void *mem, const void *srcMem ) const override
            {
                try_copy_construct( mem, *(const structType*)srcMem );
            }

            void MoveConstruct( void *mem, void *srcMem ) const override
            {
                try_move_construct( mem, std::move( *(structType*)srcMem ) );
            }

            void Destruct( void *mem ) const noexcept override
            {
                ((structType*)mem)->~structType();
            }

            void CopyAssign( void *mem, const void *srcMem ) const override
            {
                try_copy_assign( *(structType*)mem, *(const structType*)srcMem );
            }

            void MoveAssign( void *mem, void *srcMem ) const override
            {
                try_move_assign( *(structType*)mem, std::move( *(structType*)srcMem ) );
            }

            size_t GetTypeSize( systemPointer_t *sysPtr, void *construct_params ) const noexcept override
            {
                return this->structSize;
            }

            size_t GetTypeSizeByObject( systemPointer_t *sysPtr, const void *langObj ) const noexcept override
            {
                return this->structSize;
            }

            size_t GetTypeAlignment( systemPointer_t *sysPtr ) const noexcept override
            {
                return this->alignment;
            }

            size_t structSize;
            size_t alignment;
        };

        typeInfoBase *typeInfo = this->RegisterCommonTypeInterface <structTypeInterface> ( typeName, inheritsFrom, structSize, alignment );

        if ( typeInfo )
        {
            this->RegisterTypeToRTTI <structType> ( typeInfo );
        }

        return typeInfo;
    }

    // THREAD-SAFEty cannot be guarranteed. Use with caution.
    template <DTSUsableType classType, typename staticRegistry>
    inline typename staticRegistry::pluginOffset_t StaticPluginRegistryRegisterTypeConstruction( staticRegistry& registry, typeInfoBase *typeInfo, systemPointer_t *sysPtr, void *construction_params = nullptr )
    {
        struct structPluginInterface : staticRegistry::pluginInterface
        {
            inline structPluginInterface( DynamicTypeSystem *typeSys, typeInfoBase *typeInfo, systemPointer_t *sysPtr, void *construction_params ) noexcept
                : typeSys( typeSys ), typeInfo( typeInfo ), sysPtr( sysPtr ), construction_params( construction_params )
            {
                return;
            }

            bool OnPluginConstruct( typename staticRegistry::hostType_t *obj, typename staticRegistry::pluginOffset_t pluginOffset, typename staticRegistry::pluginDescriptor pluginId ) override
            {
                void *structMem = pluginId.template RESOLVE_STRUCT <void> ( obj, pluginOffset );

                if ( structMem == nullptr )
                    return false;

                DynamicTypeSystem *typeSys = this->typeSys;
                systemPointer_t *sysPtr = this->sysPtr;
                typeInfoBase *typeInfo = this->typeInfo;
                void *construction_params = this->construction_params;

                // Construct the type.
                GenericRTTI *rtObj = typeSys->ConstructPlacement( sysPtr, structMem, typeInfo, construction_params );

                if constexpr ( DependantInitializableStructType <classType, typename staticRegistry::hostType_t> )
                {
                    if ( rtObj != nullptr )
                    {
                        size_t typeAlignment = typeInfo->tInterface->GetTypeAlignment( sysPtr );

                        void *langObj = DynamicTypeSystem::GetObjectFromTypeStruct( rtObj, typeAlignment );

                        try
                        {
                            ((classType*)langObj)->Initialize( obj );
                        }
                        catch( ... )
                        {
                            typeSys->DestroyPlacement( sysPtr, rtObj );

                            throw;
                        }
                    }
                }

                return ( rtObj != nullptr );
            }

            void OnPluginDestruct( typename staticRegistry::hostType_t *obj, typename staticRegistry::pluginOffset_t pluginOffset, typename staticRegistry::pluginDescriptor pluginId ) noexcept override
            {
                void *objMem = pluginId.template RESOLVE_STRUCT <void> ( obj, pluginOffset );

                if ( objMem == nullptr )
                    return;

                DynamicTypeSystem *typeSys = this->typeSys;
                systemPointer_t *sysPtr = this->sysPtr;
                typeInfoBase *typeInfo = this->typeInfo;

                size_t typeAlignment = typeInfo->tInterface->GetTypeAlignment( sysPtr );
                void *langObj = GetObjectMemoryFromAllocation( objMem, typeAlignment );
                GenericRTTI *rtObj = GetTypeStructFromObject( langObj );

                if constexpr ( DependantShutdownableStructType <classType, typename staticRegistry::hostType_t> )
                {
                    ((classType*)langObj)->Shutdown( obj );
                }

                // Destruct the type.
                typeSys->DestroyPlacement( sysPtr, rtObj );
            }

            bool OnPluginCopyAssign( typename staticRegistry::hostType_t *dstObject, const typename staticRegistry::hostType_t *srcObject, typename staticRegistry::pluginOffset_t pluginOffset, typename staticRegistry::pluginDescriptor pluginId ) override
            {
                void *dstStructMem = pluginId.template RESOLVE_STRUCT <void> ( dstObject, pluginOffset );

                if ( dstStructMem == nullptr )
                    return false;

                const void *srcStructMem = pluginId.template RESOLVE_STRUCT <void> ( srcObject, pluginOffset );

                if ( srcStructMem == nullptr )
                    return false;

                DynamicTypeSystem *typeSys = this->typeSys;
                systemPointer_t *sysPtr = this->sysPtr;
                typeInfoBase *typeInfo = this->typeInfo;

                size_t typeAlignment = typeInfo->tInterface->GetTypeAlignment( sysPtr );
                const void *srcLangObj = GetConstObjectMemoryFromAllocation( srcStructMem, typeAlignment );
                const GenericRTTI *srcStruct = GetTypeStructFromConstObject( srcLangObj );

                // Clone the type.
                GenericRTTI *cloned = typeSys->ClonePlacement( sysPtr, dstStructMem, srcStruct );

                return ( cloned != nullptr );
            }

            bool OnPluginMoveAssign( typename staticRegistry::hostType_t *dstObject, typename staticRegistry::hostType_t *srcObject, typename staticRegistry::pluginOffset_t pluginOffset, typename staticRegistry::pluginDescriptor pluginId ) override
            {
                void *dstStructMem = pluginId.template RESOLVE_STRUCT <void> ( dstObject, pluginOffset );

                if ( dstStructMem == nullptr )
                    return false;

                void *srcStructMem = pluginId.template RESOLVE_STRUCT <void> ( srcObject, pluginOffset );

                if ( srcStructMem == nullptr )
                    return false;

                DynamicTypeSystem *typeSys = this->typeSys;
                systemPointer_t *sysPtr = this->sysPtr;
                typeInfoBase *typeInfo = this->typeInfo;

                size_t typeAlignment = typeInfo->tInterface->GetTypeAlignment( sysPtr );
                void *srcLangObj = GetObjectMemoryFromAllocation( srcStructMem, typeAlignment );
                GenericRTTI *srcStruct = GetTypeStructFromObject( srcLangObj );

                // Move the type.
                GenericRTTI *moved = typeSys->MovePlacement( sysPtr, dstStructMem, srcStruct );

                return ( moved != nullptr );
            }

            void DeleteOnUnregister( void ) noexcept override
            {
                DynamicTypeSystem *typeSys = this->typeSys;

                eir::dyn_del_struct <structPluginInterface> ( typeSys->allocMan, typeSys, this );
            }

            DynamicTypeSystem *typeSys;
            typeInfoBase *typeInfo;
            systemPointer_t *sysPtr;
            void *construction_params;
        };

        typename staticRegistry::pluginOffset_t offset = 0;

        structPluginInterface *tInterface = eir::dyn_new_struct <structPluginInterface, exceptMan> ( this->allocMan, this, this, typeInfo, sysPtr, construction_params );

        try
        {
            size_t align;
            size_t objsize = this->GetTypeStructSize( sysPtr, typeInfo, construction_params, align );

            offset = registry.RegisterPlugin(
                objsize,
                align,
                typename staticRegistry::pluginDescriptor( staticRegistry::ANONYMOUS_PLUGIN_ID ),
                tInterface
            );

            if ( !staticRegistry::IsOffsetValid( offset ) )
            {
                eir::dyn_del_struct <structPluginInterface> ( this->allocMan, this, tInterface );
            }
        }
        catch( ... )
        {
            eir::dyn_del_struct <structPluginInterface> ( this->allocMan, this, tInterface );

            throw;
        }

        return offset;
    }

    // THREAD-SAFETY: this object is IMMUTABLE.
    struct structTypeMetaInfo abstract
    {
        virtual ~structTypeMetaInfo( void )     {}

        virtual size_t GetTypeSize( systemPointer_t *sysPtr, void *construct_params ) const noexcept = 0;
        virtual size_t GetTypeSizeByObject( systemPointer_t *sysPtr, const void *mem ) const noexcept = 0;

        virtual size_t GetTypeAlignment( systemPointer_t *sysPtr ) const noexcept = 0;
    };

    // THREAD-SAFE, because memory allocation is THREAD-SAFE and type registration is THREAD-SAFE.
    template <DTSUsableType structType>
    inline typeInfoBase* RegisterDynamicStructType( const char *typeName, structTypeMetaInfo *metaInfo, bool freeMetaInfo, typeInfoBase *inheritsFrom = nullptr )
    {
        struct structTypeInterface : public typeInterface
        {
            AINLINE structTypeInterface( structTypeMetaInfo *meta_info, bool freeMetaInfo ) noexcept
            {
                this->meta_info = meta_info;
                this->freeMetaInfo = freeMetaInfo;
            }

            inline ~structTypeInterface( void )
            {
                if ( this->freeMetaInfo )
                {
                    if ( structTypeMetaInfo *metaInfo = this->meta_info )
                    {
                        delete metaInfo;

                        this->meta_info = nullptr;
                    }
                }
            }

            void Construct( void *mem, systemPointer_t *sysPtr, void *construct_params ) const override
            {
                try_construct <structType> ( mem, sysPtr, construct_params );
            }

            void CopyConstruct( void *mem, const void *srcMem ) const override
            {
                try_copy_construct( mem, *(const structType*)srcMem );
            }

            void MoveConstruct( void *mem, void *srcMem ) const override
            {
                try_move_construct( mem, std::move( *(structType*)srcMem ) );
            }

            void Destruct( void *mem ) const noexcept override
            {
                ((structType*)mem)->~structType();
            }

            void CopyAssign( void *mem, const void *srcMem ) const override
            {
                try_copy_assign( *(structType*)mem, *(const structType*)srcMem );
            }

            void MoveAssign( void *mem, void *srcMem ) const override
            {
                try_move_assign( *(structType*)mem, std::move( *(structType*)srcMem ) );
            }

            size_t GetTypeSize( systemPointer_t *sysPtr, void *construct_params ) const noexcept override
            {
                return meta_info->GetTypeSize( sysPtr, construct_params );
            }

            size_t GetTypeSizeByObject( systemPointer_t *sysPtr, const void *obj ) const noexcept override
            {
                return meta_info->GetTypeSizeByObject( sysPtr, obj );
            }

            size_t GetTypeAlignment( systemPointer_t *sysPtr ) const noexcept override
            {
                return meta_info->GetTypeAlignment( sysPtr );
            }

            structTypeMetaInfo *meta_info;
            bool freeMetaInfo;
        };

        // No RTTI registration because the dynamic size and alignment of this type do not match the API usage.
        // Instead there should be a special constructor-function called if you'd absolutely need it (lambda, callable struct, etc).

        return this->RegisterCommonTypeInterface <structTypeInterface> ( typeName, inheritsFrom, metaInfo, freeMetaInfo );
    }

private:
    // THREAD-SAFETY: use from LOCKED CONTEXT only (at least READ ACCESS, locked by typeInfo)!
    static inline void AdvanceTypePluginSize( typeInfoBase *typeInfo, size_t& curOff, size_t& pluginAlignmentOut )
    {
        // In the DynamicTypeSystem environment, we do not introduce conditional registry plugin structs.
        // That would complicate things too much, but support can be added if truly required.
        // Development does not have to be hell.
        // Without conditional struct support, this operation stays considerable performance.

        // Add the plugin sizes of all inherited classes.
        size_t subAlignment = 1;

        if ( typeInfoBase *inheritedClass = typeInfo->inheritsFrom )
        {
            AdvanceTypePluginSize( inheritedClass, curOff, subAlignment );
        }

        size_t currentPluginSize = (size_t)typeInfo->structRegistry.GetPluginSizeByRuntime();

        size_t alignment;

        if ( currentPluginSize > 0 )
        {
            // Align the data.
            alignment = typeInfo->structRegistry.GetPluginAlignment();

            curOff = ALIGN_SIZE( curOff, alignment );
            curOff += currentPluginSize;

            // Update the returned alignment.
            if ( alignment < subAlignment )
            {
                alignment = subAlignment;
            }
        }
        else
        {
            alignment = subAlignment;
        }

        pluginAlignmentOut = alignment;
    }

    static inline size_t GetTypePluginAlignment( typeInfoBase *typeInfo ) noexcept
    {
        size_t alignment = typeInfo->structRegistry.GetPluginAlignment();

        if ( typeInfoBase *inheritedClass = typeInfo->inheritsFrom )
        {
            size_t subAlignment = GetTypePluginAlignment( inheritedClass );

            if ( alignment < subAlignment )
            {
                alignment = subAlignment;
            }
        }

        return alignment;
    }

    // THREAD-SAFETY: safe to call because lock is taken.
    static inline void AdvanceTypeStructPluginSize( typeInfoBase *typeInfo, const GenericRTTI *object, size_t& curOff, size_t& pluginAlignmentOut )
    {
        scoped_rwlock_read ctx_inheritance( typeInfo->typeSys->lockProvider, typeInfo->typeLock );

        // This method is made for conditional structing support of DTS.
        size_t subAlignment = 1;

        if ( typeInfoBase *inheritedClass = typeInfo->inheritsFrom )
        {
            AdvanceTypeStructPluginSize( inheritedClass, object, curOff, subAlignment );
        }

        size_t currentPluginSize = (size_t)typeInfo->structRegistry.GetPluginSizeByObject( object );

        size_t alignment;

        if ( currentPluginSize > 0 )
        {
            // Align the data.
            alignment = typeInfo->structRegistry.GetPluginAlignment();

            curOff = ALIGN_SIZE( curOff, alignment );
            curOff += currentPluginSize;

            // Update the returned alignment.
            if ( alignment < subAlignment )
            {
                alignment = subAlignment;
            }
        }
        else
        {
            alignment = subAlignment;
        }

        pluginAlignmentOut = alignment;
    }

    // THREAD-SAFETY: use from LOCKED CONTEXT only (at least READ ACCESS, locked by typeInfo)!
    static inline pluginOffset_t GetTypeRegisteredPluginLocation( typeInfoBase *typeInfo, const GenericRTTI *theObject, pluginOffset_t pluginOffDesc )
    {
        return typeInfo->structRegistry.ResolvePluginStructOffsetByObject( theObject, pluginOffDesc );
    }

    // THREAD-SAFETY: use from LOCKED CONTEXT only (at least READ ACCESS, locked by typeInfo)!
    // THREAD-SAFE, because called from locked context and construction of inherited plugins is THREAD-SAFE (by recursion)
    // and plugin destruction is THREAD-SAFE.
    inline bool ConstructPlugins( systemPointer_t *sysPtr, typeInfoBase *typeInfo, GenericRTTI *rtObj ) const noexcept
    {
        bool pluginConstructSuccess = false;

        // If we have no parents, it is success by default.
        bool parentConstructionSuccess = true;

        // First construct the parents.
        typeInfoBase *inheritedClass = typeInfo->inheritsFrom;

        if ( inheritedClass != nullptr )
        {
            parentConstructionSuccess = ConstructPlugins( sysPtr, inheritedClass, rtObj );
        }

        // If the parents have constructed properly, do ourselves.
        if ( parentConstructionSuccess )
        {
            bool thisConstructionSuccess = typeInfo->structRegistry.ConstructPluginBlock( rtObj, sysPtr );

            // If anything has failed, we destroy everything we already constructed.
            if ( !thisConstructionSuccess )
            {
                if ( inheritedClass != nullptr )
                {
                    DestructPlugins( sysPtr, inheritedClass, rtObj );
                }
            }
            else
            {
                pluginConstructSuccess = true;
            }
        }

        return pluginConstructSuccess;
    }

    // THREAD-SAFETY: must be called from LOCKED CONTEXT (at least READ ACCESS)!
    // THREAD-SAFE, because it is called from locked context and assigning of plugins is THREAD-SAFE (by recursion).
    inline bool CopyAssignPlugins( systemPointer_t *sysPtr, typeInfoBase *typeInfo, GenericRTTI *dstRtObj, const GenericRTTI *srcRtObj ) const noexcept
    {
        // Assign plugins from the ground up.
        // If anything fails assignment, we just bail.
        // Failure in assignment does not imply an incorrect object state.
        bool assignmentSuccess = false;

        // First assign the parents.
        bool parentAssignmentSuccess = true;

        typeInfoBase *inheritedType = typeInfo->inheritsFrom;

        if ( inheritedType != nullptr )
        {
            parentAssignmentSuccess = CopyAssignPlugins( sysPtr, inheritedType, dstRtObj, srcRtObj );
        }

        // If all the parents successfully assigned, we can try assigning ourselves.
        if ( parentAssignmentSuccess )
        {
            bool thisAssignmentSuccess = typeInfo->structRegistry.CopyAssignPluginBlock( dstRtObj, srcRtObj, sysPtr );

            // If assignment was successful, we are happy.
            // Otherwise we bail the entire thing.
            if ( thisAssignmentSuccess )
            {
                assignmentSuccess = true;
            }
        }

        return assignmentSuccess;
    }

    // THREAD-SAFETY: must be called from LOCKED CONTEXT (at least READ ACCESS)!
    // THREAD-SAFE, because it is called from locked context and assigning of plugins is THREAD-SAFE (by recursion).
    // If this function fails, then it is unknown how many plugin movements have been successfully performed in interim.
    inline bool MoveAssignPlugins( systemPointer_t *sysPtr, typeInfoBase *typeInfo, GenericRTTI *dstRtObj, GenericRTTI *srcRtObj ) const noexcept
    {
        // Assign plugins from the ground up.
        // If anything fails assignment, we just bail.
        // Failure in assignment does not imply an incorrect object state.
        bool assignmentSuccess = false;

        // First assign the parents.
        bool parentAssignmentSuccess = true;

        typeInfoBase *inheritedType = typeInfo->inheritsFrom;

        if ( inheritedType != nullptr )
        {
            parentAssignmentSuccess = MoveAssignPlugins( sysPtr, inheritedType, dstRtObj, srcRtObj );
        }

        // If all the parents successfully assigned, we can try assigning ourselves.
        if ( parentAssignmentSuccess )
        {
            bool thisAssignmentSuccess = typeInfo->structRegistry.MoveAssignPluginBlock( dstRtObj, srcRtObj, sysPtr );

            // If assignment was successful, we are happy.
            // Otherwise we bail the entire thing.
            if ( thisAssignmentSuccess )
            {
                assignmentSuccess = true;
            }
        }

        return assignmentSuccess;
    }

    // THREAD-SAFETY: call from LOCKED CONTEXT only (at least READ ACCESS)!
    // THREAD-SAFE, because called from locked context and plugin destruction is THREAD-SAFE (by recursion).
    inline void DestructPlugins( systemPointer_t *sysPtr, typeInfoBase *typeInfo, GenericRTTI *rtObj ) const noexcept
    {
        typeInfo->structRegistry.DestroyPluginBlock( rtObj, sysPtr );

        if ( typeInfoBase *inheritedClass = typeInfo->inheritsFrom )
        {
            DestructPlugins( sysPtr, inheritedClass, rtObj );
        }
    }

    // THREAD-SAFE, because many operations are immutable and GetTypePluginSize is called from LOCKED READ CONTEXT.
    template <typename langType>
    inline size_t _GetTypeStructSizeByRTTI( systemPointer_t *sysPtr, typeInfoBase *typeInfo, size_t& alignmentOut ) const
    {
        // Attempt to get the memory the language object will take.
        size_t baseStructSize = sizeof( langType );
        size_t baseStructAlignment = alignof( langType );

        // Adjust that objMemSize so we can store meta information + plugins.
        size_t objMemSize = ALIGN_SIZE( sizeof( GenericRTTI ), baseStructAlignment );

        objMemSize += baseStructSize;

        scoped_rwlock_read typeLock( this->lockProvider, typeInfo->typeLock );

        // Calculate the memory that is required by all plugin structs.
        size_t pluginAlignment;
        AdvanceTypePluginSize( typeInfo, objMemSize, pluginAlignment );

        // Return the proper alignment.
        alignmentOut = std::max( std::max( alignof(GenericRTTI), baseStructAlignment ), pluginAlignment );

        return objMemSize;
    }

public:
    template <typename langType>
    inline size_t GetTypeStructSizeByRTTI( systemPointer_t *sysPtr, size_t& alignmentOut ) const
    {
        typeInfoBase *typeInfo = this->FindTypeInfoByRTTI <langType> ();

        if ( typeInfo == nullptr )
            return 0;

        return this->_GetTypeStructSizeByRTTI <langType> ( sysPtr, typeInfo, alignmentOut );
    }

    // THREAD-SAFE, because many operations are immutable and GetTypePluginSize is called from LOCKED READ CONTEXT.
    inline size_t GetTypeStructSize( systemPointer_t *sysPtr, typeInfoBase *typeInfo, void *construct_params, size_t& alignmentOut ) const
    {
        typeInterface *tInterface = typeInfo->tInterface;

        // Attempt to get the memory the language object will take.
        size_t baseStructSize = tInterface->GetTypeSize( sysPtr, construct_params );

        if ( baseStructSize == 0 )
            return 0;

        size_t baseStructAlignment = tInterface->GetTypeAlignment( sysPtr );

        // Adjust that objMemSize so we can store meta information + plugins.
        size_t objMemSize = ALIGN_SIZE( sizeof( GenericRTTI ), baseStructAlignment );

        objMemSize += baseStructSize;

        scoped_rwlock_read typeLock( this->lockProvider, typeInfo->typeLock );

        // Calculate the memory that is required by all plugin structs.
        size_t pluginAlignment;
        AdvanceTypePluginSize( typeInfo, objMemSize, pluginAlignment );

        // Return the proper alignment.
        alignmentOut = std::max( std::max( alignof(GenericRTTI), baseStructAlignment ), pluginAlignment );

        return objMemSize;
    }

    inline size_t GetTypeStructAlignment( systemPointer_t *sysPtr, typeInfoBase *typeInfo ) const noexcept
    {
        typeInterface *tInterface = typeInfo->tInterface;

        // We take advantage of the fact that alignments have to be power-of-two.

        size_t rtti_alignment = alignof(GenericRTTI);
        size_t baseStructAlignment = tInterface->GetTypeAlignment( sysPtr );
        size_t pluginAlignment = GetTypePluginAlignment( typeInfo );

        return std::max( std::max( rtti_alignment, baseStructAlignment ), pluginAlignment );
    }
    inline size_t GetTypeStructAlignment( systemPointer_t *sysPtr, const GenericRTTI *rtObj ) const noexcept
    {
        typeInfoBase *typeInfo = GetTypeInfoFromTypeStruct( rtObj );
        
        return GetTypeStructAlignment( sysPtr, typeInfo );
    }

    // THREAD-SAFE, because many operations are immutable and GetTypePluginSize is called from LOCKED READ CONTEXT.
    inline size_t GetTypeStructSize( systemPointer_t *sysPtr, const GenericRTTI *rtObj, size_t& alignmentOut ) const
    {
        typeInfoBase *typeInfo = GetTypeInfoFromTypeStruct( rtObj );
        typeInterface *tInterface = typeInfo->tInterface;

        size_t baseStructAlignment = tInterface->GetTypeAlignment( sysPtr );

        // Get the pointer to the object.
        const void *langObj = GetConstObjectFromTypeStruct( rtObj, baseStructAlignment );

        // Get the memory that is taken by the language object.
        size_t baseStructSize = tInterface->GetTypeSizeByObject( sysPtr, langObj );

        if ( baseStructSize == 0 )
            return 0;

        // Adjust that objMemSize so we can store meta information + plugins.
        size_t objMemSize = ALIGN_SIZE( sizeof( GenericRTTI ), baseStructAlignment );

        objMemSize += baseStructSize;

        // Calculate the memory that is required by all plugin structs.
        size_t pluginAlignment;
        AdvanceTypeStructPluginSize( typeInfo, rtObj, objMemSize, pluginAlignment );

        // Return the proper alignment.
        alignmentOut = std::max( std::max( alignof(GenericRTTI), baseStructAlignment ), pluginAlignment );

        return objMemSize;
    }

    inline bool DoesTypeInfoBelongTo( typeInfoBase *typeInfo ) const noexcept
    {
        return ( typeInfo->typeSys == this );
    }

    // THREAD-SAFE, because it establishes a write context.
    inline void ReferenceTypeInfo( typeInfoBase *typeInfo )
    {
        scoped_rwlock_write lock( this->lockProvider, typeInfo->typeLock );

        // This turns the typeInfo IMMUTABLE.
        typeInfo->refCount++;

        // For every type we inherit, we reference it as well.
        if ( typeInfoBase *inheritedClass = typeInfo->inheritsFrom )
        {
            ReferenceTypeInfo( inheritedClass );
        }
    }

    // THREAD-SAFE, because it establishes a write context.
    inline void DereferenceTypeInfo( typeInfoBase *typeInfo )
    {
        scoped_rwlock_write lock( this->lockProvider, typeInfo->typeLock );

        // For every type we inherit, we dereference it as well.
        if ( typeInfoBase *inheritedClass = typeInfo->inheritsFrom )
        {
            DereferenceTypeInfo( inheritedClass );
        }

        // This could turn the typeInfo NOT IMMUTABLE.
        typeInfo->refCount--;
    }

private:
    static inline void* GetObjectMemoryFromAllocation( void *allocMem, size_t baseStructAlignment ) noexcept
    {
        return (void*)ALIGN_SIZE( (size_t)allocMem + sizeof(GenericRTTI), baseStructAlignment );
    }

    static inline const void* GetConstObjectMemoryFromAllocation( const void *allocMem, size_t baseStructAlignment ) noexcept
    {
        return (void*)ALIGN_SIZE( (size_t)allocMem + sizeof(GenericRTTI), baseStructAlignment );
    }

    static inline void* GetAllocationFromTypeStruct( void *objMem, size_t baseStructAlignment ) noexcept
    {
        size_t align_alloc = std::max( baseStructAlignment, alignof(GenericRTTI) );

        return (void*)SCALE_DOWN( (size_t)objMem, align_alloc );
    }

public:
    // Returns any type info that is associated with this DTS by C++ type. If no
    // associated type is found then it returns nullptr.
    template <typename langType>
    inline typeInfoBase* FindTypeInfoByRTTI( void ) noexcept
    {
        std::type_index tidx = typeid( langType );

        AVLNode *findNode = this->lang_rtti_tree.FindNode( tidx );

        if ( findNode == nullptr )
            return nullptr;

        return AVL_GETITEM( typeInfoBase, findNode, lang_type_node );
    }

    // Returns the runtime object that will be allocated at objMem.
    // The returned pointer is not necessaringly equal to objMem itself if alignment is skewed.
    // THREAD-SAFE, because the typeInterface is THREAD-SAFE and plugin construction is THREAD-SAFE.
    inline GenericRTTI* ConstructPlacement( systemPointer_t *sysPtr, void *allocMem, typeInfoBase *typeInfo, void *construct_params )
    {
        GenericRTTI *objOut = nullptr;
        {
            // Reference the type info.
            ReferenceTypeInfo( typeInfo );

            // NOTE: TYPE INFOs do NOT CHANGE while they are referenced.
            // That is why we can loop through the plugin containers without referencing
            // the type infos!

            // Get the specialization interface.
            typeInterface *tInterface = typeInfo->tInterface;

            // Get a pointer to GenericRTTI and the object memory.
            size_t baseStructAlignment = tInterface->GetTypeAlignment( sysPtr );

            void *objMem = GetObjectMemoryFromAllocation( allocMem, baseStructAlignment );

            GenericRTTI *objTypeMeta = GetTypeStructFromObject( objMem );

            // Initialize the RTTI struct.
            objTypeMeta->type_meta = typeInfo;
#ifdef _DEBUG
            objTypeMeta->typesys_ptr = this;
#endif //_DEBUG

            // Initialize the language object.
            try
            {
                // Attempt to construct the language part.
                tInterface->Construct( objMem, sysPtr, construct_params );
            }
            catch( ... )
            {
                // We failed to construct the object struct, so it is invalid.
                objMem = nullptr;
            }

            if ( objMem )
            {
                // Only proceed if we have successfully constructed the object struct.
                // Now construct the plugins.
                bool pluginConstructSuccess = ConstructPlugins( sysPtr, typeInfo, objTypeMeta );

                if ( pluginConstructSuccess )
                {
                    // We are finished! Return the meta info.
                    objOut = objTypeMeta;
                }
                else
                {
                    // We failed, so destruct the class again.
                    tInterface->Destruct( objMem );
                }
            }

            if ( objOut == nullptr )
            {
                // Since we did not return a proper object, dereference again.
                DereferenceTypeInfo( typeInfo );
            }
        }
        return objOut;
    }

    // Constructs a runtime object in allocated memory.
    // THREAD-SAFE, because memory allocation is THREAD-SAFE, GetTypeStructSize is THREAD-SAFE and ConstructPlacement is THREAD-SAFE.
    inline GenericRTTI* Construct( systemPointer_t *sysPtr, typeInfoBase *typeInfo, void *construct_params )
    {
#ifdef _DEBUG
        rtti_assert( typeInfo->typeSys == this );
#endif //_DEBUG

        GenericRTTI *objOut = nullptr;
        {
            // We must reference the type info to prevent the issue where the type struct can change
            // during construction.
            ReferenceTypeInfo( typeInfo );

            try
            {
                size_t objMemAlignment;
                size_t objMemSize = GetTypeStructSize( sysPtr, typeInfo, construct_params, objMemAlignment );

                if ( objMemSize != 0 )
                {
                    void *allocMem = this->allocMan.Allocate( this, objMemSize, objMemAlignment );

                    if ( allocMem )
                    {
                        try
                        {
                            // Attempt to construct the object on the memory.
                            objOut = ConstructPlacement( sysPtr, allocMem, typeInfo, construct_params );
                        }
                        catch( ... )
                        {
                            // Just to be on the safe side.
                            this->allocMan.Free( this, allocMem );

                            throw;
                        }

                        if ( !objOut )
                        {
                            // Deallocate the memory again, as we seem to have failed.
                            this->allocMan.Free( this, allocMem );
                        }
                    }
                }
            }
            catch( ... )
            {
                // Just to be on the safe side.
                DereferenceTypeInfo( typeInfo );

                throw;
            }

            // We can dereference the type info again.
            DereferenceTypeInfo( typeInfo );
        }
        return objOut;
    }

    // Constructs a copy of the provided runtime object at the provided allocMem, using any copy-constructor.
    // THREAD-SAFE, because many operations are IMMUTABLE, the type interface is THREAD-SAFE, plugin construction
    // is called from LOCKED READ CONTEXT and plugin assignment is called from LOCKED READ CONTEXT.
    inline GenericRTTI* ClonePlacement( systemPointer_t *sysPtr, void *allocMem, const GenericRTTI *toBeCloned )
    {
        DebugRTTIStruct( toBeCloned );

        GenericRTTI *objOut = nullptr;
        {
            // Grab the type of toBeCloned.
            typeInfoBase *typeInfo = GetTypeInfoFromTypeStruct( toBeCloned );

            // Reference the type info.
            ReferenceTypeInfo( typeInfo );

            try
            {
                // Get the specialization interface.
                const typeInterface *tInterface = typeInfo->tInterface;

                // Get a pointer to GenericRTTI and the object memory.
                size_t baseStructAlignment = tInterface->GetTypeAlignment( sysPtr );

                void *objMem = GetObjectMemoryFromAllocation( allocMem, baseStructAlignment );

                GenericRTTI *objTypeMeta = GetTypeStructFromObject( objMem );

                // Initialize the RTTI struct.
                objTypeMeta->type_meta = typeInfo;
#ifdef _DEBUG
                objTypeMeta->typesys_ptr = this;
#endif //_DEBUG

                // Initialize the language object.
                // Get the struct which we create from.
                const void *srcObjStruct = GetConstObjectFromTypeStruct( toBeCloned, baseStructAlignment );

                try
                {
                    // Attempt to copy-construct the language part.
                    tInterface->CopyConstruct( objMem, srcObjStruct );
                }
                catch( ... )
                {
                    // We failed to construct the object struct, so it is invalid.
                    objMem = nullptr;
                }

                if ( objMem )
                {
                    // Only proceed if we have successfully constructed the object struct.
                    // First we have to construct the plugins.
                    bool pluginSuccess = false;

                    bool pluginConstructSuccess = ConstructPlugins( sysPtr, typeInfo, objTypeMeta );

                    if ( pluginConstructSuccess )
                    {
                        // Now assign the plugins from the source object.
                        bool pluginAssignSuccess = CopyAssignPlugins( sysPtr, typeInfo, objTypeMeta, toBeCloned );

                        if ( pluginAssignSuccess )
                        {
                            // We are finished! Return the meta info.
                            pluginSuccess = true;
                        }
                        
                        if ( pluginSuccess == false )
                        {
                            // Have to clean up the plugins.
                            DestructPlugins( sysPtr, typeInfo, objTypeMeta );
                        }
                    }

                    if ( pluginSuccess )
                    {
                        objOut = objTypeMeta;
                    }
                    else
                    {
                        // We failed, so destruct the class again.
                        tInterface->Destruct( objMem );
                    }
                }
            }
            catch( ... )
            {
                // Just to be on the safe side.
                DereferenceTypeInfo( typeInfo );

                throw;
            }

            if ( objOut == nullptr )
            {
                // Since we did not return a proper object, dereference again.
                DereferenceTypeInfo( typeInfo );
            }
        }
        return objOut;
    }

    // Constructs a copy of the provided runtime object in allocated memory, using any copy-constructor.
    // THREAD-SAFE, because GetTypeStructSize is THREAD-SAFE, memory allocation is THREAD-SAFE
    // and ClonePlacement is THREAD-SAFE.
    inline GenericRTTI* Clone( systemPointer_t *sysPtr, const GenericRTTI *toBeCloned )
    {
        DebugRTTIStruct( toBeCloned );

        GenericRTTI *objOut = nullptr;
        {
            // Get the size toBeCloned currently takes.
            // This is an immutable property, because memory cannot magically expand.
            size_t allocMemAlignment;
            size_t allocMemSize = GetTypeStructSize( sysPtr, toBeCloned, allocMemAlignment );

            if ( allocMemSize != 0 )
            {
                void *allocMem = this->allocMan.Allocate( this, allocMemSize, allocMemAlignment );

                if ( allocMem )
                {
                    // Attempt to clone the object on the memory.
                    objOut = ClonePlacement( sysPtr, allocMem, toBeCloned );
                }

                if ( !objOut )
                {
                    // Deallocate the memory again, as we seem to have failed.
                    this->allocMan.Free( this, allocMem );
                }
            }
        }
        return objOut;
    }

    // Constructs a runtime object at the given allocMem location, using any move-constructor.
    // THREAD-SAFE, because many operations are IMMUTABLE, the type interface is THREAD-SAFE, plugin construction
    // is called from LOCKED READ CONTEXT and plugin assignment is called from LOCKED READ CONTEXT.
    inline GenericRTTI* MovePlacement( systemPointer_t *sysPtr, void *allocMem, GenericRTTI *toBeMoved )
    {
        DebugRTTIStruct( toBeMoved );

        GenericRTTI *objOut = nullptr;
        {
            // Grab the type of toBeMoved.
            typeInfoBase *typeInfo = GetTypeInfoFromTypeStruct( toBeMoved );

            // Reference the type info.
            ReferenceTypeInfo( typeInfo );

            try
            {
                // Get the specialization interface.
                const typeInterface *tInterface = typeInfo->tInterface;

                // Get a pointer to GenericRTTI and the object memory.
                size_t baseStructAlignment = tInterface->GetTypeAlignment( sysPtr );

                void *objMem = GetObjectMemoryFromAllocation( allocMem, baseStructAlignment );

                GenericRTTI *objTypeMeta = GetTypeStructFromObject( objMem );

                // Initialize the RTTI struct.
                objTypeMeta->type_meta = typeInfo;
#ifdef _DEBUG
                objTypeMeta->typesys_ptr = this;
#endif //_DEBUG

                // Initialize the language object.
                // Get the struct which we create from.
                void *srcObjStruct = GetObjectFromTypeStruct( toBeMoved, baseStructAlignment );

                try
                {
                    // Attempt to move-construct the language part.
                    tInterface->MoveConstruct( objMem, srcObjStruct );
                }
                catch( ... )
                {
                    // We failed to construct the object struct, so it is invalid.
                    objMem = nullptr;
                }

                if ( objMem )
                {
                    // Only proceed if we have successfully constructed the object struct.
                    // First we have to construct the plugins.
                    bool pluginSuccess = false;

                    bool pluginConstructSuccess = ConstructPlugins( sysPtr, typeInfo, objTypeMeta );

                    if ( pluginConstructSuccess )
                    {
                        // Now assign the plugins from the source object.
                        bool pluginAssignSuccess = MoveAssignPlugins( sysPtr, typeInfo, objTypeMeta, toBeMoved );

                        if ( pluginAssignSuccess )
                        {
                            // We are finished! Return the meta info.
                            pluginSuccess = true;
                        }
                        
                        if ( pluginSuccess == false )
                        {
                            // Have to clean up the plugins.
                            DestructPlugins( sysPtr, typeInfo, objTypeMeta );
                        }
                    }

                    if ( pluginSuccess )
                    {
                        objOut = objTypeMeta;
                    }
                    else
                    {
                        // We failed, so destruct the class again.
                        tInterface->Destruct( objMem );
                    }
                }
            }
            catch( ... )
            {
                // Just to be on the safe side.
                DereferenceTypeInfo( typeInfo );

                throw;
            }

            if ( objOut == nullptr )
            {
                // Since we did not return a proper object, dereference again.
                DereferenceTypeInfo( typeInfo );
            }
        }
        return objOut;
    }

    // *** RTTI CONSTRUCTION INTERFACE BEGIN

private:
    template <typename langType, typename... constrArgs> requires ( eir::constructible_from <langType, constrArgs...> )
    inline langType* _ConstructPlacementByRTTI( typeInfoBase *typeInfo, systemPointer_t *sysPtr, void *allocMem, constrArgs&&... cargs )
    {
        langType *objOut = nullptr;
        {
            // Reference the type info.
            ReferenceTypeInfo( typeInfo );

            // Get a pointer to GenericRTTI and the object memory.
            size_t baseStructAlignment = alignof( langType );

            void *objMem = GetObjectMemoryFromAllocation( allocMem, baseStructAlignment );

            GenericRTTI *objTypeMeta = GetTypeStructFromObject( objMem );

            // Initialize the RTTI struct.
            objTypeMeta->type_meta = typeInfo;
#ifdef _DEBUG
            objTypeMeta->typesys_ptr = this;
#endif //_DEBUG

            // Initialize the language object.
            langType *tmpObj;

            try
            {
                // Attempt to construct the language part.
                tmpObj = new (objMem) langType( std::forward <constrArgs> ( cargs )... );
            }
            catch( ... )
            {
                // We failed to construct the object struct, so it is invalid.
                tmpObj = nullptr;
            }

            if ( tmpObj )
            {
                // Only proceed if we have successfully constructed the object struct.
                // Now construct the plugins.
                bool pluginConstructSuccess = ConstructPlugins( sysPtr, typeInfo, objTypeMeta );

                if ( pluginConstructSuccess )
                {
                    // We are finished! Return the new language object.
                    objOut = tmpObj;
                }
                else
                {
                    // We failed, so destruct the class again.
                    tmpObj->~langType();
                }
            }

            if ( objOut == nullptr )
            {
                // Since we did not return a proper object, dereference again.
                DereferenceTypeInfo( typeInfo );
            }
        }
        return objOut;
    }

public:
    // Constructs a runtime object from the provided arguments at the specified memory location.
    // Copy or move construction does not automatically invoke the DTS plugin data assignment interface.
    // Objects should be destroyed by using the DTS DestroyPlacement method.
    template <typename langType, typename... constrArgs> requires ( eir::constructible_from <langType, constrArgs...> )
    inline langType* ConstructPlacementByRTTI( systemPointer_t *sysPtr, void *objMem, constrArgs&&... cargs )
    {
        typeInfoBase *typeInfo = this->FindTypeInfoByRTTI <langType> ();

        if ( typeInfo == nullptr )
            return nullptr;

        return this->_ConstructPlacementByRTTI( typeInfo, sysPtr, objMem, std::forward <constrArgs> ( cargs )... );
    }

    // Constructs a runtime object from the provided arguments in allocator memory.
    // Objects should be destroyed by using the DTS Destroy method.
    template <typename langType, typename... constrArgs> requires ( eir::constructible_from <langType, constrArgs...> )
    inline langType* ConstructByRTTI( systemPointer_t *sysPtr, constrArgs&&... cargs )
    {
        typeInfoBase *typeInfo = this->FindTypeInfoByRTTI <langType> ();

        langType *objOut = nullptr;

        if ( typeInfo != nullptr )
        {
            // We must reference the type info to prevent the issue where the type struct can change
            // during construction.
            ReferenceTypeInfo( typeInfo );

            try
            {
                size_t objMemAlignment;
                size_t objMemSize = this->_GetTypeStructSizeByRTTI <langType> ( sysPtr, typeInfo, objMemAlignment );

                void *allocMem = this->allocMan.Allocate( this, objMemSize, objMemAlignment );

                if ( allocMem )
                {
                    try
                    {
                        // Attempt to construct the object on the memory.
                        objOut = this->_ConstructPlacementByRTTI <langType> ( typeInfo, sysPtr, allocMem, std::forward <constrArgs> ( cargs )... );
                    }
                    catch( ... )
                    {
                        // Just to be on the safe side.
                        this->allocMan.Free( this, allocMem );

                        throw;
                    }

                    if ( !objOut )
                    {
                        // Deallocate the memory again, as we seem to have failed.
                        this->allocMan.Free( this, allocMem );
                    }
                }
            }
            catch( ... )
            {
                // Just to be on the safe side.
                DereferenceTypeInfo( typeInfo );

                throw;
            }

            // We can dereference the type info again.
            DereferenceTypeInfo( typeInfo );
        }
        return objOut;
    }

    // Internalizes a given non-DTS object into this DTS structure so that it is turned into a regular runtime object
    // with attached GenericRTTI struct and plugins. The oldObj object memory has to have been allocated by the
    // same allocator as is assigned to this DTS. Returns nullptr if the internalization process has failed (for example
    // out-of-memory condition when forced to allocate new memory block, plugin construction failing, etc).
    // This method has optimal performance when it is able to reuse the memory pointed at by oldObj, moving around
    // the content in the process. In the worst case a new memory block is allocated which in itself is equal to
    // manually moving oldObj into a DTS runtime object.
    // The size of the object memory pointed at by oldObj has to be exactly sizeof(langType).
    template <typename langType> requires ( eir::nothrow_constructible_from <langType, langType&&> && std::is_nothrow_destructible <langType>::value )
    inline langType* RepurposeByRTTI( systemPointer_t *sysPtr, langType *oldObj )
    {
#ifdef _DEBUG
        rtti_assert( oldObj != nullptr );
#endif //_DEBUG

        typeInfoBase *typeInfo = this->FindTypeInfoByRTTI <langType> ();

        if ( typeInfo == nullptr )
        {
            goto failure;
        }

        {
            size_t langType_sizeof = sizeof(langType);
            size_t langType_alignof = alignof(langType);

            size_t rtobj_alignof;
            size_t rtobj_sizeof = this->_GetTypeStructSizeByRTTI <langType> ( sysPtr, typeInfo, rtobj_alignof );

            void *take_mem = nullptr;
        
            // We take advantage of the fact that alignments have to be powers-of-two.
            // Check that the memory pointed at by oldObj meets the alignment requirements.
            if ( langType_alignof >= rtobj_alignof )
            {
                // Check that the object memory can be extended to host the new type.
                bool couldBeExtended = false;

                if constexpr ( eir::ResizeMemoryAllocator <allocatorType> )
                {
                    couldBeExtended = this->allocMan.Resize( this, oldObj, rtobj_sizeof );
                }
                else if constexpr ( std::is_trivially_move_assignable <langType>::value &&
                                    std::is_trivially_move_constructible <langType>::value &&
                                    eir::ReallocMemoryAllocator <allocatorType> )
                {
                    void *realloc_result = this->allocMan.Realloc( this, oldObj, rtobj_sizeof, rtobj_alignof );

                    if ( realloc_result == nullptr )
                    {
                        goto failure;
                    }

                    oldObj = (langType*)realloc_result;

                    couldBeExtended = true;
                }

                if ( couldBeExtended )
                {
                    // Move the language object to the right spot and the construct the meta-information surrounding it.
                    take_mem = oldObj;
                }
            }

            // Go the default route.
            if ( take_mem == nullptr )
            {
                take_mem = this->allocMan.Allocate( this, rtobj_sizeof, rtobj_alignof );

                if ( take_mem == nullptr )
                {
                    goto failure;
                }
            }
        
            try
            {
                // Reference the type info.
                ReferenceTypeInfo( typeInfo );
            }
            catch( ... )
            {
                goto failure;
            }

            // Prevent uninitialized vars after jump.
            {
                // Get a pointer to GenericRTTI and the object memory.
                size_t baseStructAlignment = alignof( langType );

                void *objMem = GetObjectMemoryFromAllocation( take_mem, baseStructAlignment );

                GenericRTTI *objTypeMeta = GetTypeStructFromObject( objMem );

                // Initialize the RTTI struct.
                objTypeMeta->type_meta = typeInfo;
#ifdef _DEBUG
                objTypeMeta->typesys_ptr = this;
#endif //_DEBUG

                // Initialize the language object.
                langType *tmpObj;

                // TODO: we could optimize for non-intersection of memory buffers here.

                if ( take_mem == oldObj )
                {
                    langType tmp( std::move( *oldObj ) );

                    oldObj->~langType();

                    tmpObj = new (objMem) langType( std::move( tmp ) );
                }
                else
                {
                    // Attempt to construct the language part.
                    tmpObj = new (objMem) langType( std::move( *oldObj ) );

                    eir::dyn_del_struct <langType> ( this->allocMan, this, oldObj );
                }

                langType *objOut = nullptr;

                if ( tmpObj )
                {
                    // Only proceed if we have successfully constructed the object struct.
                    // Now construct the plugins.
                    bool pluginConstructSuccess = ConstructPlugins( sysPtr, typeInfo, objTypeMeta );

                    if ( pluginConstructSuccess )
                    {
                        // We are finished! Return the new language object.
                        objOut = tmpObj;
                    }
                    else
                    {
                        // We failed, so destroy the class again.
                        tmpObj->~langType();
                    }
                }

                if ( objOut == nullptr )
                {
                    // Since we did not return a proper object, dereference again.
                    DereferenceTypeInfo( typeInfo );

                    // Delete the allocated memory.
                    this->allocMan.Free( this, take_mem );
                }

                return objOut;
            }
        }

    failure:
        eir::dyn_del_struct <langType> ( this->allocMan, this, oldObj );
        return nullptr;
    }

    // *** RTTI CONSTRUCTION INTERFACE END

    // THREAD-SAFE, because single atomic operation.
    inline void SetTypeInfoExclusive( typeInfoBase *typeInfo, bool isExclusive ) noexcept
    {
        typeInfo->isExclusive = isExclusive;
    }

    // THREAD-SAFE, because single atomic operation.
    inline bool IsTypeInfoExclusive( typeInfoBase *typeInfo ) noexcept
    {
        return typeInfo->isExclusive;
    }

    // THREAD-SAFE, because single atomic operation.
    inline bool IsTypeInfoAbstract( typeInfoBase *typeInfo ) noexcept
    {
        return typeInfo->isAbstract;
    }

    // THREAD-SAFE, because we lock very hard to ensure consistent state.
    inline void SetTypeInfoInheritingClass( typeInfoBase *subClass, typeInfoBase *inheritedClass, bool requiresSystemLock = true )
    {
#ifdef _DEBUG
        rtti_assert( subClass->typeSys == this );
#endif //_DEBUG

        bool subClassImmutability = subClass->IsImmutable();

        rtti_assert( subClassImmutability == false );

        if ( subClassImmutability == false )
        {
            // We have to lock here, because this has to happen atomically on the whole type system.
            // Because inside of the whole type system, we assume there is not another type with the same resolution.
            scoped_rwlock_write sysLock( this->lockProvider, ( requiresSystemLock ? this->mainLock : nullptr ) );

            // Make sure we can even do that.
            // Verify that no other type with that name exists which inherits from said class.
            if ( inheritedClass != nullptr )
            {
#ifdef _DEBUG
                rtti_assert( inheritedClass->typeSys == this );
#endif //_DEBUG

                typeInfoBase *alreadyExisting = FindTypeInfoNolock( subClass->name, inheritedClass );

                if ( alreadyExisting && alreadyExisting != subClass )
                {
                    exceptMan::throw_type_name_conflict();
                }
            }

            // Alright, now we have confirmed the operation.
            // We want to change the state of the type, so we must write lock to ensure a consistent state.
            scoped_rwlock_write typeLock( this->lockProvider, subClass->typeLock );

            typeInfoBase *prevInherit = subClass->inheritsFrom;

            if ( prevInherit != inheritedClass )
            {
                scoped_rwlock_write inheritedLock( this->lockProvider, ( prevInherit ? prevInherit->typeLock : nullptr ) );
                scoped_rwlock_write newInheritLock( this->lockProvider, ( inheritedClass ? inheritedClass->typeLock : nullptr ) );

                if ( inheritedClass != nullptr )
                {
                    // Make sure that we NEVER do circular inheritance!
                    rtti_assert( IsTypeInheritingFromNolock( subClass, inheritedClass ) == false );
                }

                if ( prevInherit )
                {
                    prevInherit->inheritanceCount--;
                }

                subClass->inheritsFrom = inheritedClass;

                if ( inheritedClass )
                {
                    inheritedClass->inheritanceCount++;
                }
            }
        }
    }

private:
    // THREAD-SAFETY: has to be called from LOCKED READ CONTEXT (by subClass lock)!
    // THIS IS NOT A THREAD-SAFE ALGORITHM; USE WITH CAUTION.
    inline bool IsTypeInheritingFromNolock( typeInfoBase *baseClass, typeInfoBase *subClass ) const noexcept
    {
        if ( IsSameType( baseClass, subClass ) )
        {
            return true;
        }

        typeInfoBase *inheritedClass = subClass->inheritsFrom;

        if ( inheritedClass )
        {
            return IsTypeInheritingFromNolock( baseClass, inheritedClass );
        }

        return false;
    }

public:
    // THREAD-SAFE, because type equality is IMMUTABLE property, inherited class is
    // being verified under LOCKED READ CONTEXT and type inheritance check is THREAD-SAFE
    // (by recursion).
    inline bool IsTypeInheritingFrom( typeInfoBase *baseClass, typeInfoBase *subClass ) const
    {
#ifdef _DEBUG
        rtti_assert( baseClass->typeSys == this );
        rtti_assert( subClass->typeSys == this );
#endif //_DEBUG

        // We do not have to lock on this, because equality is an IMMUTABLE property of types.
        if ( IsSameType( baseClass, subClass ) )
        {
            return true;
        }

        scoped_rwlock_read subClassLock( this->lockProvider, subClass->typeLock );

        typeInfoBase *inheritedClass = subClass->inheritsFrom;

        if ( inheritedClass )
        {
            return IsTypeInheritingFrom( baseClass, inheritedClass );
        }

        return false;
    }

    // THREAD-SAFE, because single atomic operation.
    inline bool IsSameType( typeInfoBase *firstType, typeInfoBase *secondType ) const noexcept
    {
        return ( firstType == secondType );
    }

    // THREAD-SAFE, because local atomic operation.
    static inline void* GetObjectFromTypeStruct( GenericRTTI *rtObj, size_t objAlignment ) noexcept
    {
        return (void*)ALIGN_SIZE( (size_t)( rtObj + 1 ), objAlignment );
    }

    // THREAD-SAFE, because local atomic operation.
    static inline const void* GetConstObjectFromTypeStruct( const GenericRTTI *rtObj, size_t objAlignment ) noexcept
    {
        return (const void*)ALIGN_SIZE( (size_t)( rtObj + 1 ), objAlignment );
    }

    // THREAD-SAFE, because local atomic operation.
    static inline GenericRTTI* GetTypeStructFromObject( void *langObj ) noexcept
    {
        return (GenericRTTI*)SCALE_DOWN( (size_t)( (GenericRTTI*)langObj - 1 ), alignof(GenericRTTI) );
    }

    // THREAD-SAFE, because local atomic operation.
    static inline const GenericRTTI* GetTypeStructFromConstObject( const void *langObj ) noexcept
    {
        return (const GenericRTTI*)SCALE_DOWN( (size_t)( (const GenericRTTI*)langObj - 1 ), alignof(GenericRTTI) );
    }

protected:
    inline void DebugRTTIStruct( const GenericRTTI *typeInfo ) const
    {
#ifdef _DEBUG
        // If this assertion fails, the runtime may have mixed up times from different contexts (i.e. different configurations in Lua)
        // The problem is certainly found in the application itself.
        rtti_assert( typeInfo->typesys_ptr == this );
#endif //_DEBUG
    }

public:
    // THREAD-SAFE, because local atomic operation.
    inline GenericRTTI* GetTypeStructFromAbstractObject( void *langObj )
    {
        GenericRTTI *typeInfo = GetTypeStructFromObject( langObj );

        // Make sure it is valid.
        DebugRTTIStruct( typeInfo );

        return typeInfo;
    }

    // THREAD-SAFE, because local atomic operation.
    inline const GenericRTTI* GetTypeStructFromConstAbstractObject( const void *langObj ) const
    {
        const GenericRTTI *typeInfo = GetTypeStructFromConstObject( langObj );

        // Make sure it is valid.
        DebugRTTIStruct( typeInfo );

        return typeInfo;
    }

    // THREAD-SAFE, because DestructPlugins is called from LOCKED READ CONTEXT and type interface
    // is THREAD-SAFE.
    inline void DestroyPlacement( systemPointer_t *sysPtr, GenericRTTI *typeStruct )
    {
        DebugRTTIStruct( typeStruct );

        typeInfoBase *typeInfo = GetTypeInfoFromTypeStruct( typeStruct );
        typeInterface *tInterface = typeInfo->tInterface;

        // Destroy all object plugins.
        DestructPlugins( sysPtr, typeInfo, typeStruct );

        // Pointer to the language object.
        size_t typeAlignment = tInterface->GetTypeAlignment( sysPtr );

        void *langObj = GetObjectFromTypeStruct( typeStruct, typeAlignment );

        // Destroy the actual object.
        tInterface->Destruct( langObj );

        // Dereference the type info since we do not require it anymore.
        DereferenceTypeInfo( typeInfo );
    }

    // THREAD-SAFE, because typeStruct memory size if an IMMUTABLE property,
    // DestroyPlacement is THREAD-SAFE and memory allocation is THREAD-SAFE.
    inline void Destroy( systemPointer_t *sysPtr, GenericRTTI *typeStruct )
    {
        DebugRTTIStruct( typeStruct );

        typeInfoBase *subclassTypeInfo = GetTypeInfoFromTypeStruct( typeStruct );
        typeInterface *tInterface = subclassTypeInfo->tInterface;

        // Delete the object from the memory.
        DestroyPlacement( sysPtr, typeStruct );

        // Free the memory.
        size_t baseStructAlignment = tInterface->GetTypeAlignment( sysPtr );

        void *allocMem = GetAllocationFromTypeStruct( typeStruct, baseStructAlignment );

        this->allocMan.Free( this, allocMem );
    }

    // Calling this method is only permitted on types that YOU KNOW
    // ARE NOT USED ANYMORE. In a multi-threaded environment this is
    // VERY DANGEROUS. The runtime itself must ensure that typeInfo
    // cannot be addressed by any logic!
    // Under the above assumption, this function is THREAD-SAFE.
    // THREAD-SAFE, because typeInfo is not used anymore and a global
    // system lock is used when handling the type environment.
    inline void DeleteType( typeInfoBase *typeInfo )
    {
#ifdef _DEBUG
        rtti_assert( typeInfo->typeSys == this );
#endif //_DEBUG

        // Make sure we do not inherit from anything anymore.
        if ( typeInfo->inheritsFrom != nullptr )
        {
            scoped_rwlock_write sysLock( this->lockProvider, this->mainLock );

            typeInfoBase *inheritsFrom = typeInfo->inheritsFrom;

            if ( inheritsFrom )
            {
                scoped_rwlock_write inheritedLock( this->lockProvider, inheritsFrom->typeLock );

                typeInfo->inheritsFrom = nullptr;

                inheritsFrom->inheritanceCount--;
            }
        }

        // Make sure all classes that inherit from us do not do that anymore.
        {
            scoped_rwlock_write typeEnvironmentConsistencyLock( this->lockProvider, this->mainLock );

            LIST_FOREACH_BEGIN( typeInfoBase, this->registeredTypes.root, node )

                if ( item->inheritsFrom == typeInfo )
                {
                    SetTypeInfoInheritingClass( item, nullptr, false );
                }

            LIST_FOREACH_END
        }

        if ( typeInfo->typeLock )
        {
            // The lock provider is assumed to be THREAD-SAFE itself.
            this->lockProvider.CloseLock( typeInfo->typeLock );
        }

        // Remove the type from this manager.
        {
            scoped_rwlock_write sysLock( this->lockProvider, this->mainLock );

            LIST_REMOVE( typeInfo->node );
        }

        // If the type is registered under C++ RTTI, then remove it from that list.
        if ( typeInfo->lang_type_idx.has_value() )
        {
            this->lang_rtti_tree.RemoveByNodeFast( &typeInfo->lang_type_node );

            typeInfo->lang_type_idx.reset();
        }

        typeInfo->Cleanup();
    }

    // THREAD-SAFE, because it uses GLOBAL SYSTEM READ LOCK when iterating through the type nodes.
    struct type_iterator
    {
    private:
        DynamicTypeSystem& typeSys;

        // When iterating through types, we must hold a lock.
        scoped_rwlock_read typeConsistencyLock;

        const RwList <typeInfoBase>& listRoot;
        RwListEntry <typeInfoBase> *curNode;

    public:
        inline type_iterator( DynamicTypeSystem& typeSys )
            : typeSys( typeSys ),
              typeConsistencyLock( typeSys.lockProvider, typeSys.mainLock ),    // WE MUST GET THE LOCK BEFORE WE ACQUIRE THE LIST ROOT, for nicety :3
              listRoot( typeSys.registeredTypes )
        {
            this->curNode = listRoot.root.next;
        }

        inline type_iterator( const type_iterator& right )
            : typeSys( right.typeSys ),
              typeConsistencyLock( typeSys.lockProvider, typeSys.mainLock ),
              listRoot( right.listRoot )
        {
            this->curNode = right.curNode;
        }

        inline type_iterator( type_iterator&& right )
            : typeSys( right.typeSys ),
              typeConsistencyLock( typeSys.lockProvider, typeSys.mainLock ),
              listRoot( right.listRoot )
        {
            this->curNode = right.curNode;
        }

        inline bool IsEnd( void ) const noexcept
        {
            return ( &listRoot.root == curNode );
        }

        inline typeInfoBase* Resolve( void ) const noexcept
        {
            return LIST_GETITEM( typeInfoBase, curNode, node );
        }

        inline void Increment( void ) noexcept
        {
            this->curNode = this->curNode->next;
        }
    };

    // THREAD-SAFE, because it returns THREAD-SAFE type_iterator object.
    inline type_iterator GetTypeIterator( void ) const
    {
        // Just hack around, meow.
        return type_iterator( *(DynamicTypeSystem*)this );
    }

private:
    struct dtsAllocLink
    {
        inline dtsAllocLink( DynamicTypeSystem *refMem ) noexcept
        {
            this->refMem = refMem;
        }

        inline dtsAllocLink( const dtsAllocLink& right ) = default;

        inline void* Allocate( void *_, size_t memSize, size_t alignment ) noexcept
        {
            DynamicTypeSystem *refMem = this->refMem;
            
            return refMem->allocMan.Allocate( refMem, memSize, alignment );
        }

        inline bool Resize( void *_, void *memPtr, size_t reqSize ) noexcept requires( eir::ResizeMemoryAllocator <allocatorType> )
        {
            DynamicTypeSystem *refMem = this->refMem;

            return refMem->allocMan.Resize( refMem, memPtr, reqSize );
        }

        inline void* Realloc( void *_, void *memPtr, size_t reqSize, size_t reqAlignment ) noexcept requires( eir::ReallocMemoryAllocator <allocatorType> )
        {
            DynamicTypeSystem *refMem = this->refMem;

            return refMem->allocMan.Realloc( refMem, memPtr, reqSize, reqAlignment );
        }

        inline void Free( void *_, void *memPtr ) noexcept
        {
            DynamicTypeSystem *refMem = this->refMem;

            refMem->allocMan.Free( refMem, memPtr );
        }

        DynamicTypeSystem *refMem;
    };

public:
    typedef eir::String <char, dtsAllocLink, exceptMan> dtsString;

    // THREAD-SAFE, because it only consists of THREAD-SAFE local operations.
    struct type_resolution_iterator
    {
    private:
        DynamicTypeSystem *typeSys;
        const char *typePath;
        const char *type_iter_ptr;
        size_t tokenLen;

    public:
        inline type_resolution_iterator( DynamicTypeSystem *typeSys, const char *typePath )
        {
            this->typeSys = typeSys;
            this->typePath = typePath;
            this->type_iter_ptr = typePath;
            this->tokenLen = 0;

            this->Increment();
        }

        inline dtsString Resolve( void ) const
        {
            const char *returnedPathToken = this->typePath;

            size_t nameLength = this->tokenLen;

            return dtsString( returnedPathToken, nameLength, this->typeSys );
        }

        inline void Increment( void )
        {
            this->typePath = this->type_iter_ptr;

            size_t currentTokenLen = 0;

            while ( true )
            {
                const char *curPtr = this->type_iter_ptr;

                char c = *curPtr;

                if ( c == 0 )
                {
                    currentTokenLen = ( curPtr - this->typePath );

                    break;
                }

                if ( *curPtr == ':' && *(curPtr+1) == ':' )
                {
                    currentTokenLen = ( curPtr - this->typePath );

                    this->type_iter_ptr = curPtr + 2;

                    break;
                }

                this->type_iter_ptr++;
            }

            // OK.
            this->tokenLen = currentTokenLen;
        }

        inline bool IsEnd( void ) const noexcept
        {
            return ( this->typePath == this->type_iter_ptr );
        }
    };

private:
    // THREAD-SAFETY: call from GLOBAL LOCKED READ CONTEXT only!
    // THREAD-SAFE, because called from global LOCKED READ CONTEXT.
    inline typeInfoBase* FindTypeInfoNolock( const char *typeName, typeInfoBase *baseType ) const
    {
        LIST_FOREACH_BEGIN( typeInfoBase, this->registeredTypes.root, node )

            bool isInterestingType = true;

            if ( baseType != item->inheritsFrom )
            {
                isInterestingType = false;
            }

            if ( isInterestingType && StringEqualToZero( item->name, typeName, true ) )
            {
                return item;
            }

        LIST_FOREACH_END

        return nullptr;
    }

public:
    // THREAD-SAFE, because it calls FindTypeInfoNolock using GLOBAL LOCKED READ CONTEXT.
    inline typeInfoBase* FindTypeInfo( const char *typeName, typeInfoBase *baseType ) const
    {
        scoped_rwlock_read lock( this->lockProvider, this->mainLock );

        return FindTypeInfoNolock( typeName, baseType );
    }

    // Type resolution based on type descriptors.
    // THREAD-SAFE, because it uses THREAD-SAFE type_resolution_iterator object and the
    // FindTypeInfo function is THREAD-SAFE.
    inline typeInfoBase* ResolveTypeInfo( const char *typePath, typeInfoBase *baseTypeInfo = nullptr )
    {
        typeInfoBase *returnedTypeInfo = nullptr;
        {
            typeInfoBase *currentType = baseTypeInfo;

            // Find a base type.
            type_resolution_iterator type_path_iter( this, typePath );

            while ( type_path_iter.IsEnd() == false )
            {
                dtsString curToken = type_path_iter.Resolve();

                currentType = FindTypeInfo( curToken.GetConstString(), currentType );

                if ( currentType == nullptr )
                {
                    break;
                }

                type_path_iter.Increment();
            }

            returnedTypeInfo = currentType;
        }
        return returnedTypeInfo;
    }

private:
    template <typename... stringArgs>
    void _AppendTypeInfoName( eir::String <char, stringArgs...>& appendTo, typeInfoBase *typeInfo )
    {
        scoped_rwlock_read ctx_typeInheritance( this->lockProvider, typeInfo->typeLock );

        if ( typeInfoBase *inheritsFrom = typeInfo->inheritsFrom )
        {
            _AppendTypeInfoName( appendTo, inheritsFrom );

            appendTo += "::";
        }

        appendTo += typeInfo->name;
    }

public:
    template <typename subAllocatorType, typename subExceptMan = exceptMan, typename... allocArgs>
    inline eir::String <char, subAllocatorType> MakeTypeInfoFullName( typeInfoBase *typeInfo, allocArgs&&... args )
    {
        scoped_rwlock_read ctx_globalTypeConsistency( this->lockProvider, this->mainLock );

        eir::String <char, subAllocatorType, subExceptMan> result( eir::constr_with_alloc::DEFAULT, std::forward <allocArgs> ( args )... );
        _AppendTypeInfoName( result, typeInfo );

        return result;
    }
};

// Redirect from structRegistry to typeInfoBase.
IMPL_HEAP_REDIR_DYNRPTR_ALLOC_TEMPLATEBASE( DynamicTypeSystem, DTS_TEMPLARGS_NODEF, DTS_TEMPLUSE, structRegRedirAlloc, typeInfoBase, structRegistry, typeSys, allocMan, allocatorType );

#endif //_DYN_TYPE_ABSTRACTION_SYSTEM_
