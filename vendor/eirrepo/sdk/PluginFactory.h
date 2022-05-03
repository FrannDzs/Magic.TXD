/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/PluginFactory.h
*  PURPOSE:     Plugin factory templates
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_PLUGIN_FACTORY_HEADER_
#define _EIR_PLUGIN_FACTORY_HEADER_

#include "eirutils.h"
#include "MemoryUtils.h"
#include "MetaHelpers.h"

#include <atomic>
#include <algorithm>    // for std::min, std::max
#include <tuple>
#include <bit>   // for is-power-of-2
#include <type_traits>
#include <tuple>

// Prefer our own SDK types, because they are not tied to ridiculous allocator semantics.
#include "Vector.h"

// Include the helpers for actually handling plugin registrations and their validity.
#include "PluginFlavors.h"

// Class used to register anonymous structs that can be placed on top of a C++ type.
#define APSR_TEMPLARGS \
    template <typename abstractionType, typename pluginDescriptorType_meta, PluginStructRegistryFlavor <abstractionType> pluginAvailabilityDispatchType, eir::MemoryAllocator allocatorType, typename... AdditionalArgs>
#define APSR_TEMPLUSE \
    <abstractionType, pluginDescriptorType_meta, pluginAvailabilityDispatchType, allocatorType, AdditionalArgs...>

APSR_TEMPLARGS
struct AnonymousPluginStructRegistry
{
    // Export for DynamicTypeSystem (DTS).
    typedef pluginDescriptorType_meta pluginDescriptorType;

    // A plugin struct registry is based on an infinite range of memory that can be allocated on, like a heap.
    // The structure of this memory heap is then applied onto the underlying type.
    typedef InfiniteCollisionlessBlockAllocator <size_t> blockAlloc_t;

    typedef blockAlloc_t::allocSemanticsManager allocSemanticsManager;

    blockAlloc_t pluginRegions;

    typedef typename pluginDescriptorType::pluginOffset_t pluginOffset_t;

private:
    size_t requiredMemoryAlignment;

    // The "template" keyword makes sense because the specifier "pluginInterfaceBase" cannot be deduced from the global program graph.
    typedef typename pluginAvailabilityDispatchType::template pluginInterfaceBase <AnonymousPluginStructRegistry> pluginInterfaceBase;

public:
    // Virtual interface used to describe plugin classes.
    // The construction process must be immutable across the runtime.
    struct pluginInterface : public pluginInterfaceBase
    {
        virtual ~pluginInterface( void )            {}

        // Overwrite this method to construct the plugin memory.
        virtual bool OnPluginConstruct( abstractionType *object, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... )
        {
            // By default, construction of plugins should succeed.
            return true;
        }

        // Overwrite this method to destroy the plugin memory.
        virtual void OnPluginDestruct( abstractionType *object, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... ) noexcept
        {
            return;
        }

        // Overwrite this method to copy-assign the plugin memory.
        virtual bool OnPluginCopyAssign( abstractionType *dstObject, const abstractionType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... )
        {
            // Copy-assignment of data to another plugin struct is optional.
            return false;
        }

        // Overwrite this method to move-assign the plugin memory.
        virtual bool OnPluginMoveAssign( abstractionType *dstObject, abstractionType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... )
        {
            // Mpve-assignment of data to another plugin struct is optional.
            return false;
        }

        // Overwrite this method if unregistering should delete this class or other cleanup has to be performed.
        // This allows cleanup using custom allocators whose pointer-to is stored inside this interface itself.
        virtual void DeleteOnUnregister( void ) noexcept
        {
            return;
        }
    };

    // Struct that holds information about registered plugins.
    struct registered_plugin : public blockAlloc_t::block_t
    {
        friend struct AnonymousPluginStructRegistry;

        inline registered_plugin( void ) noexcept
        {
            this->pluginSize = 0;
        }
        inline registered_plugin( registered_plugin&& right ) noexcept :
            pluginAlignment( std::move( right.pluginAlignment ) ),
            pluginId( std::move( right.pluginId ) ),
            pluginOffset( std::move( right.pluginOffset ) ),
            descriptor( std::move( right.descriptor ) )
        {
            // NOTE THAT EVERY registered_plugin INSTANCE MUST BE ALLOCATED.

            // We know that invalid registered_plugin objects have a pluginSize of zero.
            size_t pluginSize = right.pluginSize;

            if ( pluginSize != 0 )
            {
                this->moveFrom( std::move( right ) );
            }

            this->pluginSize = pluginSize;

            right.pluginSize = 0;       // this means invalidation.
            right.pluginAlignment = 1;
            right.pluginId = pluginDescriptorType();    // TODO: probably set this to invalid id.
            right.pluginOffset = 0;
            right.descriptor = nullptr;
        }
        inline registered_plugin( const registered_plugin& right ) = delete;

        // When move assignment is happening, the object is expected to be properly constructed.
        inline registered_plugin& operator = ( registered_plugin&& right ) noexcept
        {
            // Is there even anything to deallocate?
            this->~registered_plugin();

            return *new (this) registered_plugin( std::move( right ) );
        }
        inline registered_plugin& operator = ( const registered_plugin& right ) = delete;

        inline size_t GetPluginSize( void ) const noexcept
        {
            return this->pluginSize;
        }

        inline size_t GetPluginAlignment( void ) const noexcept
        {
            return this->pluginAlignment;
        }

        inline const pluginDescriptorType& GetDescriptor( void ) const noexcept
        {
            return this->pluginId;
        }

        inline pluginOffset_t GetPluginOffset( void ) const noexcept
        {
            return this->pluginOffset;
        }

        inline pluginInterface* GetInterface( void ) const noexcept
        {
            return this->descriptor;
        }

    private:
        size_t pluginSize;
        size_t pluginAlignment;
        pluginDescriptorType pluginId;
        pluginOffset_t pluginOffset;
        pluginInterface *descriptor;
    };

private:
    DEFINE_HEAP_REDIR_ALLOC_BYSTRUCT( regPluginsRedirAlloc, allocatorType );

    // Container that holds plugin information.
    typedef eir::Vector <registered_plugin, regPluginsRedirAlloc> registeredPlugins_t;

    registeredPlugins_t regPlugins;

    // Holds things like the way to determine the size of all plugins.
    [[no_unique_address]] pluginAvailabilityDispatchType regBoundFlavor;
    
public:
    inline AnonymousPluginStructRegistry( void ) noexcept
    {
        this->requiredMemoryAlignment = 1;  // by default we do not need any special alignment.
    }
    inline AnonymousPluginStructRegistry( AnonymousPluginStructRegistry&& right ) noexcept
        : pluginRegions( std::move( right.pluginRegions ) ), requiredMemoryAlignment( std::move( right.requiredMemoryAlignment ) ),
          regPlugins( std::move( right.regPlugins ) ), regBoundFlavor( std::move( right.regBoundFlavor ) )
    {
        right.requiredMemoryAlignment = 1;
    }
    inline AnonymousPluginStructRegistry( const AnonymousPluginStructRegistry& ) = delete;

    inline ~AnonymousPluginStructRegistry( void )
    {
        // TODO: allow custom memory allocators.

        // The runtime is allowed to destroy with plugins still attached, so clean up.
        while ( this->regPlugins.GetCount() != 0 )
        {
            registered_plugin& thePlugin = this->regPlugins[ 0 ];

            thePlugin.descriptor->DeleteOnUnregister();

            this->regPlugins.RemoveByIndex( 0 );
        }
    }

    inline AnonymousPluginStructRegistry& operator = ( AnonymousPluginStructRegistry&& right ) noexcept
    {
        this->~AnonymousPluginStructRegistry();

        return *new (this) AnonymousPluginStructRegistry( std::move( right ) );
    }
    inline AnonymousPluginStructRegistry& operator = ( const AnonymousPluginStructRegistry& ) = delete;

    inline size_t GetPluginSizeByRuntime( void ) const
    {
        return this->regBoundFlavor.GetPluginAllocSize( this );
    }

    inline size_t GetPluginSizeByObject( const abstractionType *object ) const
    {
        return this->regBoundFlavor.GetPluginAllocSizeByObject( this, object );
    }

    inline size_t GetPluginAlignment( void ) const noexcept
    {
        return this->requiredMemoryAlignment;
    }

    inline const registeredPlugins_t& GetRegisteredPlugins( void ) const noexcept
    {
        return this->regPlugins;
    }

    inline pluginInterface* GetPluginInterfaceByPluginOffset( pluginOffset_t off ) const noexcept
    {
        for ( const registered_plugin& plg : this->regPlugins )
        {
            if ( plg.pluginOffset == off )
            {
                return plg.descriptor;
            }
        }

        return nullptr;
    }

    // Function used to register a new plugin struct into the class.
    inline pluginOffset_t RegisterPlugin( size_t pluginSize, size_t pluginAlignment, const pluginDescriptorType& pluginId, pluginInterface *plugInterface, size_t allocStart = 0 )
    {
        // Make sure we have got valid parameters passed.
        if ( pluginSize == 0 || plugInterface == nullptr )
            return 0;   // TODO: fix this crap, 0 is ambivalent since its a valid index!

        if ( std::has_single_bit( pluginAlignment ) == false )
            return 0;   // TODO: see above.

        // Determine the plugin offset that should be used for allocation.
        blockAlloc_t::allocInfo blockAllocInfo;

        bool hasSpace = pluginRegions.FindSpace( pluginSize, blockAllocInfo, pluginAlignment, allocStart );

        // Handle obscure errors.
        if ( hasSpace == false )
            return 0;

        // The beginning of the free space is where our plugin should be placed at.
        pluginOffset_t useOffset = blockAllocInfo.slice.GetSliceStartPoint();

        // Register ourselves.
        registered_plugin info;
        info.pluginSize = pluginSize;
        info.pluginAlignment = pluginAlignment;
        info.pluginId = pluginId;
        info.pluginOffset = useOffset;
        info.descriptor = plugInterface;

        // Register the pointer to the registered plugin.
        pluginRegions.PutBlock( &info, blockAllocInfo );

        regPlugins.AddToBack( std::move( info ) );

        // Do we have to update the required byte-alignment?
        // Since every alignment is a power-of-two, we can just take the biggest alignment as requirement.
        // This is possible because...
        // 1) every plugin struct is properly aligned into this set
        // 2) bigger power-of-two alignments imply smaller power-of-two alignments
        if ( this->requiredMemoryAlignment < pluginAlignment )
        {
            this->requiredMemoryAlignment = pluginAlignment;
        }

        // Notify the flavor to update.
        this->regBoundFlavor.UpdatePluginRegion( this );

        return useOffset;
    }

    inline bool UnregisterPlugin( pluginOffset_t pluginOffset )
    {
        bool hasDeleted = false;
        size_t deletedAlignment;

        // Get the plugin information that represents this plugin offset.
        size_t numPlugins = regPlugins.GetCount();

        for ( size_t n = 0; n < numPlugins; n++ )
        {
            registered_plugin& thePlugin = regPlugins[ n ];

            if ( thePlugin.pluginOffset == pluginOffset )
            {
                // We found it!
                // Now remove it and (probably) delete it.
                thePlugin.descriptor->DeleteOnUnregister();

                // Remember the alignment we have removed as we might have to update the value.
                deletedAlignment = thePlugin.pluginAlignment;

                pluginRegions.RemoveBlock( &thePlugin );

                regPlugins.RemoveByIndex( n );

                numPlugins--;

                hasDeleted = true;
                break;  // there can be only one.
            }
        }

        if ( hasDeleted )
        {
            // Recalculate the required byte-alignment, if it could have changed.
            if ( deletedAlignment == this->requiredMemoryAlignment )
            {
                size_t newAlignment = 1;

                for ( size_t n = 0; n < numPlugins; n++ )
                {
                    registered_plugin& thePlugin = regPlugins[ n ];

                    size_t this_align = thePlugin.pluginAlignment;

                    if ( newAlignment < this_align )
                    {
                        newAlignment = this_align;
                    }
                }

                this->requiredMemoryAlignment = newAlignment;
            }

            // Since we really have deleted, we need to readjust our struct memory size.
            // This is done by getting the span size of the plugin allocation region, which is a very fast operation!
            this->regBoundFlavor.UpdatePluginRegion( this );
        }

        return hasDeleted;
    }

private:
    typedef typename pluginAvailabilityDispatchType::template regionIterator <AnonymousPluginStructRegistry> regionIterator_t;

    inline void DestroyPluginBlockInternal( abstractionType *pluginObj, regionIterator_t& iter, AdditionalArgs... addArgs ) noexcept
    {
        while ( true )
        {
            iter.Decrement();

            if ( iter.IsEnd() )
                break;

            const blockAlloc_t::block_t *blockItem = iter.ResolveBlock();

            const registered_plugin *constructedInfo = (const registered_plugin*)blockItem;

            // Destroy that plugin again.
            constructedInfo->descriptor->OnPluginDestruct(
                pluginObj,
                iter.ResolveOffset(),
                constructedInfo->pluginId,
                addArgs...
            );
        }
    }

public:
    inline bool ConstructPluginBlock( abstractionType *pluginObj, AdditionalArgs... addArgs ) noexcept
    {
        // Construct all plugins.
        bool pluginConstructionSuccessful = true;

        regionIterator_t iter( this );

        try
        {
            while ( !iter.IsEnd() )
            {
                const blockAlloc_t::block_t *blockItem = iter.ResolveBlock();

                const registered_plugin *pluginInfo = (const registered_plugin*)blockItem;

                // TODO: add dispatch based on the reg bound flavor.
                // it should say whether this plugin exists and where it is ultimatively located.
                // in the cached handler, this is an O(1) operation, in the conditional it is rather funky.
                // this is why you really should not use the conditional handler too often.

                bool success =
                    pluginInfo->descriptor->OnPluginConstruct(
                        pluginObj,
                        iter.ResolveOffset(),
                        pluginInfo->pluginId,
                        addArgs...
                    );

                if ( !success )
                {
                    pluginConstructionSuccessful = false;
                    break;
                }

                iter.Increment();
            }
        }
        catch( ... )
        {
            // There was an exception while trying to construct a plugin.
            // We do not let it pass and terminate here.
            pluginConstructionSuccessful = false;
        }

        if ( !pluginConstructionSuccessful )
        {
            // The plugin failed to construct, so destroy all plugins that
            // constructed up until that point.
            DestroyPluginBlockInternal( pluginObj, iter, addArgs... );
        }

        return pluginConstructionSuccessful;
    }

    inline bool CopyAssignPluginBlock( abstractionType *dstPluginObj, const abstractionType *srcPluginObj, AdditionalArgs... addArgs ) noexcept
    {
        // Call all assignment operators.
        bool cloneSuccess = true;

        regionIterator_t iter( this );

        try
        {
            for ( ; !iter.IsEnd(); iter.Increment() )
            {
                const blockAlloc_t::block_t *blockInfo = iter.ResolveBlock();

                const registered_plugin *pluginInfo = (const registered_plugin*)blockInfo;

                bool assignSuccess = pluginInfo->descriptor->OnPluginCopyAssign(
                    dstPluginObj, srcPluginObj,
                    iter.ResolveOffset(),
                    pluginInfo->pluginId,
                    addArgs...
                );

                if ( !assignSuccess )
                {
                    cloneSuccess = false;
                    break;
                }
            }
        }
        catch( ... )
        {
            // There was an exception while cloning plugin data.
            // We do not let it pass and terminate here.
            cloneSuccess = false;
        }

        return cloneSuccess;
    }

    inline bool MoveAssignPluginBlock( abstractionType *dstPluginObj, abstractionType *srcPluginObj, AdditionalArgs... addArgs ) noexcept
    {
        // Call all assignment operators.
        bool moveSuccess = true;

        regionIterator_t iter( this );

        try
        {
            for ( ; !iter.IsEnd(); iter.Increment() )
            {
                const blockAlloc_t::block_t *blockInfo = iter.ResolveBlock();

                const registered_plugin *pluginInfo = (const registered_plugin*)blockInfo;

                bool assignSuccess = pluginInfo->descriptor->OnPluginMoveAssign(
                    dstPluginObj, srcPluginObj,
                    iter.ResolveOffset(),
                    pluginInfo->pluginId,
                    addArgs...
                );

                if ( !assignSuccess )
                {
                    moveSuccess = false;
                    break;
                }
            }
        }
        catch( ... )
        {
            // There was an exception while moving plugin data.
            // We do not let it pass and terminate here.
            moveSuccess = false;
        }

        return moveSuccess;
    }

    inline void DestroyPluginBlock( abstractionType *pluginObj, AdditionalArgs... addArgs ) noexcept
    {
        // Call destructors of all registered plugins.
        regionIterator_t endIter( this );

#if 0
        // By decrementing this double-linked-list iterator by one, we get to the end.
        endIter.Decrement();
#else
        // We want to iterate to the end.
        while ( endIter.IsEnd() == false )
        {
            endIter.Increment();
        }
#endif

        DestroyPluginBlockInternal( pluginObj, endIter, addArgs... );
    }

    // Use this function whenever you receive a handle offset to a plugin struct.
    // It is optimized so that you cannot go wrong.
    inline pluginOffset_t ResolvePluginStructOffsetByRuntime( pluginOffset_t handleOffset ) const
    {
        size_t theActualOffset;

        bool gotOffset = this->regBoundFlavor.GetPluginStructOffset( this, handleOffset, theActualOffset );

        return ( gotOffset ? theActualOffset : 0 );
    }

    inline pluginOffset_t ResolvePluginStructOffsetByObject( const abstractionType *obj, pluginOffset_t handleOffset ) const
    {
        size_t theActualOffset;

        bool gotOffset = this->regBoundFlavor.GetPluginStructOffsetByObject( this, obj, handleOffset, theActualOffset );

        return ( gotOffset ? theActualOffset : 0 );
    }
};

IMPL_HEAP_REDIR_DIRECT_ALLOC_TEMPLATEBASE( AnonymousPluginStructRegistry, APSR_TEMPLARGS, APSR_TEMPLUSE, regPluginsRedirAlloc, AnonymousPluginStructRegistry, regPlugins, allocatorType );

template <typename structType>
concept PluginableStruct =
    std::is_nothrow_destructible <structType>::value;

template <typename structType, typename classType>
concept DependantInitializableStructType =
    std::is_invocable <decltype(&structType::Initialize), structType&, classType*>::value;
template <typename structType, typename classType>
concept DependantShutdownableStructType =
    std::is_invocable <decltype(&structType::Shutdown), structType&, classType*>::value;

// Helper struct for common plugin system functions.
// THREAD-SAFE because this class itself is immutable and the systemType is THREAD-SAFE.
template <typename classType, typename systemType, typename pluginDescriptorType, typename... AdditionalArgs>
struct CommonPluginSystemDispatch
{
    systemType& sysType;

    typedef typename systemType::pluginOffset_t pluginOffset_t;

    inline CommonPluginSystemDispatch( systemType& sysType ) : sysType( sysType )
    {
        return;
    }

    template <typename interfaceType, typename... Args>
    inline pluginOffset_t RegisterCommonPluginInterface( size_t structSize, size_t structAlignment, const pluginDescriptorType& pluginId, Args&&... constrArgs )
    {
        // Register our plugin!
        return sysType.template RegisterCustomPlugin <interfaceType> (
            structSize, structAlignment, pluginId,
            std::forward <Args> ( constrArgs )...
        );
    }

private:
    // Object construction helper.
    template <typename structType, typename... Args>
    static AINLINE structType* construct_helper( void *structMem, classType *obj, Args&&... theArgs )
    {
        if constexpr ( std::is_constructible <structType, classType*, Args...>::value )
        {
            return new (structMem) structType( obj, std::forward <Args> ( theArgs )... );
        }
        else if constexpr ( std::is_constructible <structType, classType*>::value )
        {
            return new (structMem) structType( obj );
        }
        else
        {
            return new (structMem) structType;
        }
    }

    // Since a lot of functionality could not support the copy assignment, we need this helper.
    template <typename subStructType>
    static AINLINE bool copy_assign( subStructType& left, const subStructType& right )
    {
        if constexpr ( std::is_copy_assignable <subStructType>::value )
        {
            left = right;
            return true;
        }
        else
        {
            return false;
        }
    }

    template <typename subStructType>
    static AINLINE bool move_assign( subStructType& left, subStructType&& right )
    {
        if constexpr ( std::is_move_assignable <subStructType>::value )
        {
            left = std::move( right );
            return true;
        }
        else
        {
            return false;
        }
    }

public:
    // Helper functions used to create common plugin templates.
    template <PluginableStruct structType>
    inline pluginOffset_t RegisterStructPlugin( const pluginDescriptorType& pluginId, size_t structSize = sizeof(structType), size_t structAlignment = alignof(structType) )
    {
        struct structPluginInterface : systemType::pluginInterface
        {
            bool OnPluginConstruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
            {
                void *structMem = pluginId.template RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

                if ( structMem == nullptr )
                    return false;

                // Construct the struct!
                // We prefer giving the class object as argument to the default constructor.
                structType *theStruct = construct_helper <structType, AdditionalArgs...> ( structMem, obj, std::forward <AdditionalArgs> ( addArgs )... );

                return ( theStruct != nullptr );
            }

            void OnPluginDestruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) noexcept override
            {
                structType *theStruct = pluginId.template RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

                // Destruct the struct!
                theStruct->~structType();
            }

            bool OnPluginCopyAssign( classType *dstObject, const classType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
            {
                // Do a copy-assignment operation.
                structType *dstStruct = pluginId.template RESOLVE_STRUCT <structType> ( dstObject, pluginOffset, addArgs... );
                const structType *srcStruct = pluginId.template RESOLVE_STRUCT <structType> ( srcObject, pluginOffset, addArgs... );

                return copy_assign( *dstStruct, *srcStruct );
            }

            bool OnPluginMoveAssign( classType *dstObject, classType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
            {
                // Do a move-assignment operation.
                structType *dstStruct = pluginId.template RESOLVE_STRUCT <structType> ( dstObject, pluginOffset, addArgs... );
                structType *srcStruct = pluginId.template RESOLVE_STRUCT <structType> ( srcObject, pluginOffset, addArgs... );

                return move_assign( *dstStruct, std::move( *srcStruct ) );
            }
        };

        // Create the interface that should handle our plugin.
        return RegisterCommonPluginInterface <structPluginInterface> ( structSize, structAlignment, pluginId );
    }

    template <PluginableStruct structType, typename inheritFrom = systemType::pluginInterface>
    struct dependantStructPluginInterface : inheritFrom
    {
        bool OnPluginConstruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
        {
            void *structMem = pluginId.template RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

            if ( structMem == nullptr )
                return false;

            // Construct the struct!
            structType *theStruct = construct_helper <structType, AdditionalArgs...> ( structMem, obj, std::forward <AdditionalArgs> ( addArgs )... );

            if constexpr ( DependantInitializableStructType <structType, classType> )
            {
                if ( theStruct )
                {
                    try
                    {
                        // Initialize the manager.
                        theStruct->Initialize( obj );
                    }
                    catch( ... )
                    {
                        // We have to destroy our struct again.
                        theStruct->~structType();

                        throw;
                    }
                }
            }

            return ( theStruct != nullptr );
        }

        void OnPluginDestruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) noexcept override
        {
            structType *theStruct = pluginId.template RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

            if constexpr ( DependantShutdownableStructType <structType, classType> )
            {
                // Deinitialize the manager.
                theStruct->Shutdown( obj );
            }

            // Destruct the struct!
            theStruct->~structType();
        }

        bool OnPluginCopyAssign( classType *dstObject, const classType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
        {
            // Do a copy-assignment operation.
            structType *dstStruct = pluginId.template RESOLVE_STRUCT <structType> ( dstObject, pluginOffset, addArgs... );
            const structType *srcStruct = pluginId.template RESOLVE_STRUCT <structType> ( srcObject, pluginOffset, addArgs... );

            return copy_assign( *dstStruct, *srcStruct );
        }

        bool OnPluginMoveAssign( classType *dstObject, classType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
        {
            // Do a move-assignment operation.
            structType *dstStruct = pluginId.template RESOLVE_STRUCT <structType> ( dstObject, pluginOffset, addArgs... );
            structType *srcStruct = pluginId.template RESOLVE_STRUCT <structType> ( srcObject, pluginOffset, addArgs... );

            return move_assign( *dstStruct, std::move( *srcStruct ) );
        }
    };

    template <PluginableStruct structType>
    inline pluginOffset_t RegisterDependantStructPlugin( const pluginDescriptorType& pluginId, size_t structSize = sizeof(structType), size_t structAlignment = alignof(structType) )
    {
        typedef dependantStructPluginInterface <structType> structPluginInterface;

        // Create the interface that should handle our plugin.
        return RegisterCommonPluginInterface <structPluginInterface> ( structSize, structAlignment, pluginId );
    }

    struct conditionalPluginStructInterface abstract
    {
        // Conditional interface. This is based on the state of the runtime.
        // Both functions here have to match logically.
        virtual bool IsPluginAvailableDuringRuntime( pluginDescriptorType pluginId ) const = 0;
        virtual bool IsPluginAvailableAtObject( const classType *object, pluginDescriptorType pluginId ) const = 0;
    };

    // Helper to register a conditional type that acts as dependant struct.
    template <PluginableStruct structType>
        requires ( PluginFlavorConditionalPluginInterface <typename systemType::pluginInterface, classType, pluginDescriptorType> )
    inline pluginOffset_t RegisterDependantConditionalStructPlugin( const pluginDescriptorType& pluginId, conditionalPluginStructInterface *conditional, size_t structSize = sizeof(structType), size_t alignment = alignof(structType) )
    {
        struct structPluginInterface : public dependantStructPluginInterface <structType>
        {
            inline structPluginInterface( conditionalPluginStructInterface *conditional ) noexcept
            {
                this->conditional = conditional;
            }

            bool IsPluginAvailableDuringRuntime( pluginDescriptorType pluginId ) const override
            {
                return conditional->IsPluginAvailableDuringRuntime( pluginId );
            }

            bool IsPluginAvailableAtObject( const classType *object, pluginDescriptorType pluginId ) const override
            {
                return conditional->IsPluginAvailableAtObject( object, pluginId );
            }

            conditionalPluginStructInterface *conditional;
        };

        // Create the interface that should handle our plugin.
        return RegisterCommonPluginInterface <structPluginInterface> ( structSize, alignment, pluginId, conditional );
    }
};

template <typename classType>
concept SPCFClassType =
    std::is_nothrow_destructible <classType>::value;

// Static plugin system that constructs classes that can be extended at runtime.
// This one is inspired by the RenderWare plugin system.
// This container is NOT MULTI-THREAD SAFE.
// All operations are expected to be ATOMIC.
#define SPCF_TEMPLARGS \
    template <SPCFClassType classType, eir::MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager, PluginStructRegistryFlavor <classType> flavorType = cachedMinimalStructRegistryFlavor <classType>, typename... constrArgs>
#define SPCF_TEMPLARGS_NODEF \
    template <SPCFClassType classType, eir::MemoryAllocator allocatorType, typename exceptMan, PluginStructRegistryFlavor <classType> flavorType, typename... constrArgs>
#define SPCF_TEMPLUSE \
    <classType, allocatorType, exceptMan, flavorType, constrArgs...>

SPCF_TEMPLARGS
struct StaticPluginClassFactory
{
    typedef classType hostType_t;

    static constexpr unsigned int ANONYMOUS_PLUGIN_ID = std::numeric_limits <unsigned int>::max();

    inline StaticPluginClassFactory( void ) requires ( eir::constructible_from <allocatorType> )
    {
        this->data.aliveClasses = 0;
    }

    template <typename... Args> requires ( eir::constructible_from <allocatorType, Args...> )
    inline StaticPluginClassFactory( eir::constr_with_alloc _, Args&&... allocArgs )
        : data( std::tuple(), std::tuple( std::forward <Args> ( allocArgs )... ) )
    {
        this->data.aliveClasses = 0;
    }

    inline StaticPluginClassFactory( const StaticPluginClassFactory& right ) = delete;
    inline StaticPluginClassFactory( StaticPluginClassFactory&& right ) noexcept
        : structRegistry( std::move( right.structRegistry ) ), data( std::move( right.data ) )
    {
        return;
    }

    inline ~StaticPluginClassFactory( void )
    {
        assert( this->data.aliveClasses == 0 );
    }

    inline StaticPluginClassFactory& operator = ( const StaticPluginClassFactory& right ) = delete;
    inline StaticPluginClassFactory& operator = ( StaticPluginClassFactory&& right ) noexcept
    {
        this->structRegistry = std::move( right.structRegistry );
        this->data.aliveClasses = (unsigned int)right.data.aliveClasses;

        // Take over the allocator stuff.
        this->data.opt() = std::move( right.data.opt() );

        right.data.aliveClasses = 0;

        return *this;
    }

    inline unsigned int GetNumberOfAliveClasses( void ) const noexcept
    {
        return this->data.aliveClasses;
    }

    // Number type used to store the plugin offset.
    typedef ptrdiff_t pluginOffset_t;

    // Helper functoid.
    struct pluginDescriptor
    {
        typedef typename StaticPluginClassFactory::pluginOffset_t pluginOffset_t;

        inline pluginDescriptor( void ) noexcept
        {
            this->pluginId = StaticPluginClassFactory::ANONYMOUS_PLUGIN_ID;
        }

        inline pluginDescriptor( unsigned int pluginId ) noexcept
        {
            this->pluginId = pluginId;
        }

        operator const unsigned int& ( void ) const noexcept
        {
            return this->pluginId;
        }

        template <typename pluginStructType>
        AINLINE static pluginStructType* RESOLVE_STRUCT( classType *object, pluginOffset_t offset, constrArgs&&... args )
        {
            return StaticPluginClassFactory::RESOLVE_STRUCT <pluginStructType> ( object, offset );
        }

        template <typename pluginStructType>
        AINLINE static const pluginStructType* RESOLVE_STRUCT( const classType *object, pluginOffset_t offset, constrArgs&&... args )
        {
            return StaticPluginClassFactory::RESOLVE_STRUCT <const pluginStructType> ( object, offset );
        }

        unsigned int pluginId;
    };

private:
    DEFINE_HEAP_REDIR_ALLOC_BYSTRUCT( spcfAllocRedir, allocatorType );

public:
    typedef AnonymousPluginStructRegistry <classType, pluginDescriptor, flavorType, spcfAllocRedir, constrArgs...> structRegistry_t;

    // Localize certain plugin registry types.
    typedef typename structRegistry_t::pluginInterface pluginInterface;

    static constexpr pluginOffset_t INVALID_PLUGIN_OFFSET = (pluginOffset_t)-1;

    AINLINE static bool IsOffsetValid( pluginOffset_t offset ) noexcept
    {
        return ( offset != INVALID_PLUGIN_OFFSET );
    }

    template <typename pluginStructType>
    AINLINE static pluginStructType* RESOLVE_STRUCT( classType *object, pluginOffset_t offset )
    {
        if ( IsOffsetValid( offset ) == false )
            return nullptr;

        // TODO: properly resolve the plugin-struct offset taking the alignment of the plugin block into account!

        return (pluginStructType*)( (char*)object + offset );
    }

    template <typename pluginStructType>
    AINLINE static const pluginStructType* RESOLVE_STRUCT( const classType *object, pluginOffset_t offset )
    {
        if ( IsOffsetValid( offset ) == false )
            return nullptr;

        return (const pluginStructType*)( (const char*)object + offset );
    }

    // Just helpers, no requirement.
    AINLINE static classType* BACK_RESOLVE_STRUCT( void *pluginObj, pluginOffset_t offset )
    {
        if ( IsOffsetValid( offset ) == false )
            return nullptr;

        return (classType*)( (char*)pluginObj - offset );
    }

    AINLINE static const classType* BACK_RESOLVE_STRUCT( const void *pluginObj, pluginOffset_t offset )
    {
        if ( IsOffsetValid( offset ) == false )
            return nullptr;

        return (const classType*)( (const char*)pluginObj - offset );
    }

    // Function used to register a new plugin struct into the class.
    inline pluginOffset_t RegisterPlugin( size_t pluginSize, size_t pluginAlignment, unsigned int pluginId, pluginInterface *plugInterface )
    {
        assert( this->data.aliveClasses == 0 );

        return structRegistry.RegisterPlugin( pluginSize, pluginAlignment, pluginId, plugInterface, sizeof(classType) );
    }

    template <typename structPluginInterfaceType, typename... Args>
    inline pluginOffset_t RegisterCustomPlugin( size_t pluginSize, size_t pluginAlignment, unsigned int pluginId, Args&&... cArgs )
    {
        struct selfDeletingPlugin : public structPluginInterfaceType
        {
            inline selfDeletingPlugin( StaticPluginClassFactory *fact, Args&&... cArgs ) : structPluginInterfaceType( std::forward <Args> ( cArgs )... )
            {
                this->fact = fact;
            }

            void DeleteOnUnregister( void ) noexcept override
            {
                StaticPluginClassFactory *fact = this->fact;

                eir::dyn_del_struct <selfDeletingPlugin> ( fact->data.opt(), fact, this );
            }

            StaticPluginClassFactory *fact;
        };

        selfDeletingPlugin *pluginInfo = eir::dyn_new_struct <selfDeletingPlugin, exceptMan> ( this->data.opt(), this, this, std::forward <Args> ( cArgs )... );

        try
        {
            return RegisterPlugin( pluginSize, pluginAlignment, pluginId, pluginInfo );
        }
        catch( ... )
        {
            eir::dyn_del_struct <selfDeletingPlugin> ( this->data.opt(), this, pluginInfo );

            throw;
        }
    }

    inline void UnregisterPlugin( pluginOffset_t pluginOffset )
    {
        assert( this->data.aliveClasses == 0 );

        structRegistry.UnregisterPlugin( pluginOffset );
    }

    typedef CommonPluginSystemDispatch <classType, StaticPluginClassFactory, pluginDescriptor, constrArgs...> functoidHelper_t;

    // Helper functions used to create common plugin templates.
    template <PluginableStruct pluginStructType>
    inline pluginOffset_t RegisterStructPlugin( unsigned int pluginId = ANONYMOUS_PLUGIN_ID, size_t structSize = sizeof(pluginStructType), size_t structAlignment = alignof(pluginStructType) )
    {
        return functoidHelper_t( *this ).template RegisterStructPlugin <pluginStructType> ( pluginId, structSize, structAlignment );
    }

    template <PluginableStruct pluginStructType>
    inline pluginOffset_t RegisterDependantStructPlugin( unsigned int pluginId = ANONYMOUS_PLUGIN_ID, size_t structSize = sizeof(pluginStructType), size_t structAlignment = alignof(pluginStructType) )
    {
        return functoidHelper_t( *this ).template RegisterDependantStructPlugin <pluginStructType> ( pluginId, structSize, structAlignment );
    }

    template <PluginableStruct pluginStructType>
        requires ( std::same_as <flavorType, conditionalStructRegistryFlavor <classType>> )
    inline pluginOffset_t RegisterDependantConditionalStructPlugin( unsigned int pluginId, typename functoidHelper_t::conditionalPluginStructInterface *conditional, size_t structSize = sizeof(pluginStructType), size_t structAlignment = alignof(pluginStructType) )
    {
        return functoidHelper_t( *this ).template RegisterDependantConditionalStructPlugin <pluginStructType> ( pluginId, conditional, structSize, structAlignment );
    }

    // Note that this function only guarrantees to return an object size that is correct at this point in time.
    inline size_t GetClassSize( void ) const
    {
        size_t baseStructSize = sizeof( classType );
        size_t pluginBlockSize = this->structRegistry.GetPluginSizeByRuntime();

        return std::max( baseStructSize, pluginBlockSize );
    }

    inline size_t GetClassAlignment( void ) const
    {
        // We take advantage of the restriction that alignments are power-of-two only.

        size_t baseStructAlignment = alignof( classType );
        size_t pluginBlockAlignment = this->structRegistry.GetPluginAlignment();

        return std::max( baseStructAlignment, pluginBlockAlignment );
    }

    template <typename... baseConstrArgs>
        requires ( eir::constructible_from <classType, baseConstrArgs...> )
    inline classType* ConstructPlacementEx( void *classMem, std::tuple <baseConstrArgs...> baseArgs, constrArgs&&... args )
    {
        classType *resultObject = nullptr;
        {
            classType *intermediateClassObject = nullptr;

            try
            {
                intermediateClassObject =
                    new (classMem) classType( std::make_from_tuple <classType> ( std::move( baseArgs ) ) );
            }
            catch( ... )
            {
                // The base object failed to construct, so terminate here.
            }

            if ( intermediateClassObject )
            {
                bool pluginConstructionSuccessful = structRegistry.ConstructPluginBlock( intermediateClassObject, std::forward <constrArgs> ( args )... );

                if ( pluginConstructionSuccessful )
                {
                    // We succeeded, so return our pointer.
                    // We promote it to a real class object.
                    resultObject = intermediateClassObject;
                }
                else
                {
                    // Else we cannot keep the intermediate class object anymore.
                    intermediateClassObject->~classType();
                }
            }
        }

        if ( resultObject )
        {
            this->data.aliveClasses++;
        }

        return resultObject;
    }

    template <eir::MemoryAllocator subAllocatorType, typename... baseConstrArgs>
        requires ( eir::constructible_from <classType, baseConstrArgs...> )
    inline classType* ConstructWithAlloc( subAllocatorType& memAllocator, std::tuple <baseConstrArgs...> baseArgs, constrArgs&&... args )
    {
        // Attempt to allocate the necessary memory.
        const size_t wholeClassSize = this->GetClassSize();
        const size_t requiredAlignment = this->GetClassAlignment();

        void *classMem = memAllocator.Allocate( this, wholeClassSize, requiredAlignment );

        if ( !classMem )
            return nullptr;

        classType *resultObj = this->ConstructPlacementEx( classMem, std::move( baseArgs ), std::forward <constrArgs> ( args )... );

        if ( !resultObj )
        {
            // Clean up.
            memAllocator.Free( this, classMem );
        }

        return resultObj;
    }

    // Internalizes a previously non-plugin-based oldObj node into a plugin-based one.
    // The oldObj node has to reside on memory allocated by memAllocator.
    // The actual memory used by oldObj has to be sizeof(classType).
    // The alignment of oldObj memory has to be at-least alignof(classType).
    template <eir::MemoryAllocator subAllocatorType>
        requires ( eir::constructible_from <classType, classType&&> )
    inline classType* Repurpose( subAllocatorType& memAllocator, classType *oldObj, constrArgs&&... cargs )
    {
        size_t classType_size = sizeof(classType);
        size_t pluginObj_size = this->GetClassSize();

        size_t classType_alignof = alignof(classType);
        size_t pluginObj_alignof = this->GetClassAlignment();

#ifdef _DEBUG
        FATAL_ASSERT( classType_size <= pluginObj_size );
#endif //_DEBUG
        
        // We assume that all alignments are powers-of-two.
        // Check that the plugin object would be properly aligned on the same memory pointer.
        if ( pluginObj_alignof <= classType_alignof )
        {
            // Check that the plugin object can (tightly) fit at the provided memory location.
            bool can_tightly_fit = ( classType_size == pluginObj_size );

            if constexpr ( eir::ResizeMemoryAllocator <subAllocatorType> )
            {
                if ( can_tightly_fit == false )
                {
                    can_tightly_fit = memAllocator.Resize( this, oldObj, pluginObj_size );
                }
            }
            else if constexpr ( eir::trivially_moveable <classType> && eir::ReallocMemoryAllocator <subAllocatorType> )
            {
                if ( can_tightly_fit == false )
                {
                    void *realloc_result = memAllocator.Realloc( this, oldObj, pluginObj_size, pluginObj_alignof );

                    if ( realloc_result == nullptr )
                    {
                        return nullptr;
                    }

                    // We have just expanded the size so the regular data should be fine.
                    oldObj = (classType*)realloc_result;

                    can_tightly_fit = true;
                }
            }

            if ( can_tightly_fit )
            {
                bool pluginConstructSuccess = structRegistry.ConstructPluginBlock( oldObj, std::forward <constrArgs> ( cargs )... );

                if ( pluginConstructSuccess )
                {
                    // We have successfully internalized a new object.
                    this->data.aliveClasses++;

                    return oldObj;
                }
                
                // Have to clean up because we are unable to complete.
                eir::dyn_del_struct <classType> ( memAllocator, this, oldObj );
                return nullptr;
            }
        }

        // Fall back to the default.
        classType *result = this->ConstructWithAlloc( memAllocator, std::tuple <classType&&> ( std::move( *oldObj ) ), std::forward <constrArgs> ( cargs )... );

        eir::dyn_del_struct <classType> ( memAllocator, this, oldObj );

        return result;
    }

    template <eir::MemoryAllocator subAllocatorType, typename... Args>
        requires ( eir::constructible_from <classType, Args...> )
    inline classType* ConstructArgs( subAllocatorType& memAllocator, Args&&... theArgs )
    {
        return this->ConstructWithAlloc( memAllocator, std::tuple( std::forward <Args> ( theArgs )... ) );
    }

    inline classType* ClonePlacement( void *classMem, const classType *srcObject, constrArgs&&... args )
        requires ( eir::constructible_from <classType, const classType&> )
    {
        classType *clonedObject = nullptr;
        {
            // Construct a basic class where we assign stuff to.
            classType *dstObject = nullptr;

            try
            {
                dstObject = new (classMem) classType( *srcObject );
            }
            catch( ... )
            {
                dstObject = nullptr;
            }

            if ( dstObject )
            {
                bool pluginConstructionSuccessful = structRegistry.ConstructPluginBlock( dstObject, args... );

                if ( pluginConstructionSuccessful )
                {
                    bool cloneSuccess = structRegistry.CopyAssignPluginBlock( dstObject, srcObject, args... );

                    if ( cloneSuccess )
                    {
                        clonedObject = dstObject;
                    }

                    if ( clonedObject == nullptr )
                    {
                        structRegistry.DestroyPluginBlock( dstObject );
                    }
                }

                if ( clonedObject == nullptr )
                {
                    // Since cloning plugin data has not succeeded, we have to destroy the constructed base object again.
                    // Make sure that we do not throw exceptions.
                    dstObject->~classType();

                    dstObject = nullptr;
                }
            }
        }

        if ( clonedObject )
        {
            this->data.aliveClasses++;
        }

        return clonedObject;
    }

    template <eir::MemoryAllocator subAllocatorType>
        requires ( eir::constructible_from <classType, const classType&> )
    inline classType* Clone( subAllocatorType& memAllocator, const classType *srcObject, constrArgs&&... args )
    {
        // Attempt to allocate the necessary memory.
        const size_t wholeClassSize = this->GetClassSize();
        const size_t requiredAlignment = this->GetClassAlignment();

        void *classMem = memAllocator.Allocate( this, wholeClassSize, requiredAlignment );

        if ( !classMem )
            return nullptr;

        classType *clonedObject = ClonePlacement( classMem, srcObject, std::forward <constrArgs> ( args )... );

        if ( clonedObject == nullptr )
        {
            memAllocator.Free( this, classMem );
        }

        return clonedObject;
    }

    template <eir::MemoryAllocator subAllocatorType>
        requires ( eir::constructible_from <classType> )
    inline classType* Construct( subAllocatorType& memAllocator, constrArgs&&... args )
    {
        return ConstructWithAlloc( memAllocator, std::tuple(), std::forward <constrArgs> ( args )... );
    }

    inline classType* ConstructPlacement( void *memPtr, constrArgs&&... args ) requires ( eir::constructible_from <classType> )
    {
        return ConstructPlacementEx( memPtr, std::tuple(), std::forward <constrArgs> ( args )... );
    }

    // Performs copy-assignment of both the language object as well as the plugin blocks.
    inline bool CopyAssign( classType *dstObj, const classType *srcObj )
        requires ( std::is_copy_assignable <classType>::value )
    {
        // First we (copy-)assign the language object.
        // Not that hard.
        try
        {
            *dstObj = *srcObj;
        }
        catch( ... )
        {
            return false;
        }

        // Next we should assign the plugin blocks.
        return structRegistry.CopyAssignPluginBlock( dstObj, srcObj );
    }

    // Performs move-assignment of both the language object as well as the plugin blocks.
    inline bool MoveAssign( classType *dstObj, classType *srcObj )
        requires ( std::is_move_assignable <classType>::value )
    {
        // First we (move-)assign the language object.
        // Not that hard.
        try
        {
            *dstObj = std::move( *srcObj );
        }
        catch( ... )
        {
            return false;
        }

        // Next we should assign the plugin blocks.
        return structRegistry.MoveAssignPluginBlock( dstObj, srcObj );
    }

    inline void DestroyPlacement( classType *classObject ) noexcept
    {
        // Destroy plugin data first.
        structRegistry.DestroyPluginBlock( classObject );

        // Destroy the base class object.
        classObject->~classType();

        // Decrease the number of alive classes.
        this->data.aliveClasses--;
    }

    template <eir::MemoryAllocator subAllocatorType>
    inline void Destroy( subAllocatorType& memAllocator, classType *classObject ) noexcept
    {
        if ( classObject == nullptr )
            return;

        // Invalidate the memory that is in "classObject".
        DestroyPlacement( classObject );

        // Free our memory.
        void *classMem = classObject;

        memAllocator.Free( this, classMem );
    }

private:
    // Member fields that actually matter belong to the end of structs/classes.
    structRegistry_t structRegistry;

    struct semi_fields
    {
        semi_fields( void ) = default;
        semi_fields( const semi_fields& right ) noexcept : aliveClasses( (unsigned int)right.aliveClasses )
        {}
        semi_fields( semi_fields&& right ) noexcept : aliveClasses( (unsigned int)right.aliveClasses )
        {
            right.aliveClasses = 0u;
        }

        std::atomic <unsigned int> aliveClasses;
    };

    empty_opt <allocatorType, semi_fields> data;
};

IMPL_HEAP_REDIR_DYN_ALLOC_TEMPLATEBASE( StaticPluginClassFactory, SPCF_TEMPLARGS_NODEF, SPCF_TEMPLUSE, spcfAllocRedir, StaticPluginClassFactory, structRegistry, data.opt(), allocatorType );

#endif //_EIR_PLUGIN_FACTORY_HEADER_
