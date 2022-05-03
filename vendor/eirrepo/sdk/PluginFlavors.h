/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/PluginFlavors.h
*  PURPOSE:     Plugin factory flavors
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_PLUGIN_FACTORY_FLAVORS_HEADER_
#define _EIR_PLUGIN_FACTORY_FLAVORS_HEADER_

#include "eirutils.h"

#include <type_traits>

template <typename flavorType, typename abstractionType>
concept PluginStructRegistryFlavor =
    eir::constructible_from <flavorType> &&
    eir::nothrow_constructible_from <flavorType, flavorType&&> &&
    std::is_nothrow_assignable <flavorType&, flavorType&&>::value &&
    requires ( flavorType FT, size_t& S_OUT, const abstractionType *ATPTR, const void *MNGPTR ) {
        { FT.GetPluginStructOffset( MNGPTR, size_t(), S_OUT ) } -> std::same_as <bool>;
        { FT.GetPluginStructOffsetByObject( MNGPTR, ATPTR, size_t(), S_OUT ) } -> std::same_as <bool>;
        { FT.GetPluginAllocSize( MNGPTR ) } -> std::same_as <size_t>;
        { FT.GetPluginAllocSizeByObject( MNGPTR, ATPTR ) } -> std::same_as <size_t>;
        FT.UpdatePluginRegion( MNGPTR );
        typename decltype(FT)::template pluginInterfaceBase <void>;
        typename decltype(FT)::template regionIterator <void>;
    } &&
    std::is_invocable_r <bool, decltype(flavorType::DoesUseUnifiedPluginOffset)>::value;

// Struct registry flavor is used to fine-tune the performance of said registry by extending it with features.
// The idea is that we maximize performance by only giving it the features you really want it to have.
template <typename abstractionType, typename... pluginInterfaceBases> requires ( ( std::is_fundamental <pluginInterfaceBases>::value == false ) && ... )
struct cachedMinimalStructRegistryFlavor
{
    size_t pluginAllocSize;

    cachedMinimalStructRegistryFlavor( void ) noexcept
    {
        // Reset to zero as we have no plugins allocated.
        this->pluginAllocSize = 0;
    }

    inline bool GetPluginStructOffset( const void *_, size_t handleOffset, size_t& actualOffset ) const
    {
        actualOffset = handleOffset;
        return true;
    }

    inline bool GetPluginStructOffsetByObject( const void *_, const abstractionType *object, size_t handleOffset, size_t& actualOffset ) const
    {
        actualOffset = handleOffset;
        return true;
    }

    inline size_t GetPluginAllocSize( const void *_ ) const
    {
        return this->pluginAllocSize;
    }

    inline size_t GetPluginAllocSizeByObject( const void *_, const abstractionType *object ) const
    {
        return this->pluginAllocSize;
    }

    template <typename managerType>
    inline void UpdatePluginRegion( const managerType *manPtr )
    {
        // Update the overall class size.
        // It is determined by the end of this plugin struct.
        this->pluginAllocSize = manPtr->pluginRegions.GetSpanSize();    // often it will just be ( useOffset + pluginSize ), but we must not rely on that.
    }

    inline static bool DoesUseUnifiedPluginOffset( void )
    {
        return true;
    }

    template <typename managerType>
    struct pluginInterfaceBase : public pluginInterfaceBases...
    {
        // Nothing really.
    };

    template <typename managerType>
    struct regionIterator
    {
        typedef typename managerType::blockAlloc_t::block_t block_t;

    private:
        managerType *manPtr;

        RwListEntry <block_t> *iterator;

    public:
        inline regionIterator( managerType *manPtr )
        {
            this->manPtr = manPtr;

            this->iterator = manPtr->pluginRegions.blockList.root.next;
        }

        inline regionIterator( const regionIterator& right )
        {
            this->manPtr = right.manPtr;
        }

        inline void operator = ( const regionIterator& right )
        {
            this->manPtr = right.manPtr;
        }

        inline void Increment( void )
        {
            this->iterator = this->iterator->next;
        }

        inline void Decrement( void )
        {
            this->iterator = this->iterator->prev;
        }

        inline bool IsEnd( void ) const
        {
            return ( this->iterator == &this->manPtr->pluginRegions.blockList.root );
        }

        inline bool IsNextEnd( void ) const
        {
            return ( this->iterator->next == &this->manPtr->pluginRegions.blockList.root );
        }

        inline bool IsPrevEnd( void ) const
        {
            return ( this->iterator->prev == &this->manPtr->pluginRegions.blockList.root );
        }

        inline block_t* ResolveBlock( void ) const
        {
            return LIST_GETITEM( block_t, this->iterator, node );
        }

        inline size_t ResolveOffset( void ) const
        {
            block_t *curBlock = this->ResolveBlock();

            return ( curBlock->slice.GetSliceStartPoint() );
        }

        inline size_t ResolveOffsetAfter( void ) const
        {
            block_t *curBlock = this->ResolveBlock();

            return ( curBlock->slice.GetSliceEndPoint() + 1 );
        }
    };
};
static_assert( PluginStructRegistryFlavor <cachedMinimalStructRegistryFlavor <void>, void> == true );

template <typename pluginInterfaceBaseType, typename abstractionType, typename pluginDescriptorType>
concept PluginFlavorConditionalPluginInterface =
    std::is_invocable_r <bool, decltype(&pluginInterfaceBaseType::IsPluginAvailableDuringRuntime), const pluginInterfaceBaseType&, pluginDescriptorType>::value &&
    std::is_invocable_r <bool, decltype(&pluginInterfaceBaseType::IsPluginAvailableAtObject), const pluginInterfaceBaseType&, const abstractionType*, pluginDescriptorType>::value;

template <typename abstractionType>
struct conditionalStructRegistryFlavor
{
    conditionalStructRegistryFlavor( void ) noexcept
    {
        return;
    }

    template <typename managerType>
    struct runtimeBlockConditional
    {
    private:
        const managerType *manPtr;

        typedef typename managerType::allocSemanticsManager allocSemanticsManager;

    public:
        typedef ConditionalAllocationProcSemantics <size_t, allocSemanticsManager, runtimeBlockConditional> condSemantics;

        inline runtimeBlockConditional( const managerType *manPtr )
        {
            this->manPtr = manPtr;
        }

        inline bool DoIgnoreBlock( const void *theBlock ) const
        {
            // The given block is always a plugin registration, so cast it appropriately.
            const typename managerType::registered_plugin *pluginDesc = (const typename managerType::registered_plugin*)theBlock;

            pluginInterfaceBase <managerType> *pluginInterface = (pluginInterfaceBase <managerType>*)pluginDesc->GetInterface();

            // Lets just ask the vendor, whether he wants the block.
            bool isAvailable = pluginInterface->IsPluginAvailableDuringRuntime( pluginDesc->GetDescriptor() );

            // We ignore a block registration if it is not available.
            return ( isAvailable == false );
        }
    };

    template <typename managerType>
    struct regionIterator
    {
    private:
        typedef runtimeBlockConditional <managerType> usedConditional;

    public:
        typedef typename usedConditional::condSemantics condSemantics;

    private:
        usedConditional conditional;
        typename condSemantics::conditionalRegionIterator iterator;

    public:
        inline regionIterator( const managerType *manPtr ) : conditional( manPtr ), iterator( &manPtr->pluginRegions.allocSemMan, conditional )
        {
            return;
        }

        inline regionIterator( const regionIterator& right ) : conditional( right.conditional ), iterator( right.iterator )
        {
            return;
        }

        inline void operator = ( const regionIterator& right )
        {
            this->conditional = right.conditional;
            this->iterator = right.iterator;
        }

        inline void Increment( void )
        {
            this->iterator.Increment();
        }

        inline void Decrement( void )
        {
            this->iterator.Decrement();
        }

        inline bool IsEnd( void ) const
        {
            return this->iterator.IsEnd();
        }

        inline bool IsNextEnd( void ) const
        {
            return this->iterator.IsNextEnd();
        }

        inline bool IsPrevEnd( void ) const
        {
            return this->iterator.IsPrevEnd();
        }

        inline decltype(auto) ResolveBlock( void ) const
        {
            return this->iterator.ResolveBlock();
        }

        inline size_t ResolveOffset( void ) const
        {
            return this->iterator.ResolveOffset();
        }

        inline size_t ResolveOffsetAfter( void ) const
        {
            return this->iterator.ResolveOffsetAfter();
        }
    };

    template <typename managerType>
    inline size_t GetPluginAllocSize( managerType *manPtr ) const
    {
        typedef runtimeBlockConditional <managerType> usedConditional;

        usedConditional conditional( manPtr );

        return usedConditional::condSemantics::GetSpanSizeConditional( &manPtr->pluginRegions.allocSemMan, conditional );
    }

    template <typename managerType>
    struct objectBasedBlockConditional
    {
    private:
        managerType *manPtr;
        const abstractionType *aliveObject;

        typedef typename managerType::allocSemanticsManager allocSemanticsManager;

    public:
        typedef ConditionalAllocationProcSemantics <size_t, allocSemanticsManager, objectBasedBlockConditional> condSemantics;

        inline objectBasedBlockConditional( managerType *manPtr, const abstractionType *aliveObject )
        {
            this->manPtr = manPtr;
            this->aliveObject = aliveObject;
        }

        inline bool DoIgnoreBlock( const void *theBlock ) const
        {
            // The given block is always a plugin registration, so cast it appropriately.
            const typename managerType::registered_plugin *pluginDesc = (const typename managerType::registered_plugin*)theBlock;

            pluginInterfaceBase <managerType> *pluginInterface = (pluginInterfaceBase <managerType>*)pluginDesc->GetInterface();

            // Lets just ask the vendor, whether he wants the block.
            bool isAvailable = pluginInterface->IsPluginAvailableAtObject( this->aliveObject, pluginDesc->GetDescriptor() );

            // We ignore a block registration if it is not available.
            return ( isAvailable == false );
        }
    };

    template <typename managerType>
    inline size_t GetPluginAllocSizeByObject( managerType *manPtr, const abstractionType *object ) const
    {
        typedef objectBasedBlockConditional <managerType> usedConditional;

        usedConditional conditional( manPtr, object );

        return usedConditional::condSemantics::GetSpanSizeConditional( &manPtr->pluginRegions.allocSemMan, conditional );
    }

    template <typename managerType>
    inline bool GetPluginStructOffset( managerType *manPtr, size_t handleOffset, size_t& actualOffset ) const
    {
        typedef runtimeBlockConditional <managerType> usedConditional;

        usedConditional conditional( manPtr );

        return usedConditional::condSemantics::ResolveConditionalBlockOffset( &manPtr->pluginRegions.allocSemMan, handleOffset, actualOffset, conditional );
    }

    template <typename managerType>
    inline bool GetPluginStructOffsetByObject( managerType *manPtr, const abstractionType *object, size_t handleOffset, size_t& actualOffset ) const
    {
        typedef objectBasedBlockConditional <managerType> usedConditional;

        usedConditional conditional( manPtr, object );

        return usedConditional::condSemantics::ResolveConditionalBlockOffset( &manPtr->pluginRegions.allocSemMan, handleOffset, actualOffset, conditional );
    }

    template <typename managerType>
    inline void UpdatePluginRegion( managerType *manPtr )
    {
        // Whatever.
    }

    inline static bool DoesUseUnifiedPluginOffset( void )
    {
        return false;
    }

    template <typename managerType>
    struct pluginInterfaceBase
    {
        typedef typename managerType::pluginDescriptorType pluginDescriptorType;

        // Conditional interface. This is based on the state of the runtime.
        virtual bool IsPluginAvailableDuringRuntime( pluginDescriptorType pluginId ) const
        {
            return true;
        }

        virtual bool IsPluginAvailableAtObject( const abstractionType *object, pluginDescriptorType pluginId ) const
        {
            // Make sure that the functionality of IsPluginAvailableDuringRuntime and IsPluginAvailableAtObject logically match.
            // Basically, object must store a snapshot of the runtime state and keep it immutable, while the runtime state
            // (and by that, the type layout) may change at any time. This is best stored by some kind of bool variable.
            return true;
        }
    };
};
static_assert( PluginStructRegistryFlavor <conditionalStructRegistryFlavor <void>, void> == true );

#endif //_EIR_PLUGIN_FACTORY_FLAVORS_HEADER_