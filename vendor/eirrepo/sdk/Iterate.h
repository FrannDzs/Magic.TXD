/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Iterate.h
*  PURPOSE:     Iteration over lists and arguments.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRREPO_ITERATION_HEADER_
#define _EIRREPO_ITERATION_HEADER_

#include "MetaHelpers.h"
#include "Vector.h"
#include "Set.h"
#include "MultiValueStore.h"

#include <type_traits>

namespace eir
{

// Modification tie as helper.
template <typename containerType, typename addCallbackType, typename removeCallbackType>
struct containNotifyAddRemoveTie
{
    typedef containerType containerType;

    template <typename itemType>
    using is_about = std::bool_constant <
        std::is_invocable <addCallbackType, itemType>::value && std::is_invocable <removeCallbackType, itemType>::value
    >;
    
    AINLINE containNotifyAddRemoveTie( containerType& container, addCallbackType& add_cb, removeCallbackType& rem_cb ) noexcept
        : container( container ), add_cb( add_cb ), rem_cb( rem_cb )
    {
        return;
    }
    containNotifyAddRemoveTie( containNotifyAddRemoveTie&& ) = default;
    containNotifyAddRemoveTie( const containNotifyAddRemoveTie& ) = default;

    AINLINE containNotifyAddRemoveTie& operator = ( containNotifyAddRemoveTie&& ) = default;
    AINLINE containNotifyAddRemoveTie& operator = ( const containNotifyAddRemoveTie& ) = default;

    AINLINE containerType& GetContainer( void ) noexcept
    {
        return this->container;
    }
    AINLINE addCallbackType& GetAddCallback( void ) noexcept
    {
        return this->add_cb;
    }
    AINLINE removeCallbackType& GetRemoveCallback( void ) noexcept
    {
        return this->rem_cb;
    }

    AINLINE operator containerType& ( void ) noexcept
    {
        return this->container;
    }
    AINLINE operator const containerType& ( void ) const noexcept
    {
        return this->container;
    }

private:
    [[no_unique_address]] containerType& container;
    [[no_unique_address]] addCallbackType& add_cb;
    [[no_unique_address]] removeCallbackType& rem_cb;
};

// Deducation guides.
template <typename containerType, typename addCallbackType, typename removeCallbackType>
containNotifyAddRemoveTie( containerType& container, addCallbackType add_cb, removeCallbackType rem_cb ) -> containNotifyAddRemoveTie <containerType, const addCallbackType, const removeCallbackType>;

// Mandatory type-trait.
template <typename>
struct is_contain_notify_add_remove_tie : std::false_type
{};
template <typename... tieArgs>
struct is_contain_notify_add_remove_tie <containNotifyAddRemoveTie <tieArgs...>> : std::true_type
{};
template <typename... tieArgs>
struct is_contain_notify_add_remove_tie <const containNotifyAddRemoveTie <tieArgs...>> : std::true_type
{};
template <typename... tieArgs>
struct is_contain_notify_add_remove_tie <containNotifyAddRemoveTie <tieArgs...>&> : std::true_type
{};
template <typename... tieArgs>
struct is_contain_notify_add_remove_tie <const containNotifyAddRemoveTie <tieArgs...>&> : std::true_type
{};
template <typename... tieArgs>
struct is_contain_notify_add_remove_tie <containNotifyAddRemoveTie <tieArgs...>&&> : std::true_type
{};
template <typename... tieArgs>
struct is_contain_notify_add_remove_tie <const containNotifyAddRemoveTie <tieArgs...>&&> : std::true_type
{};

// A tie which stores just a removal callback.
template <typename containerType, typename removeCallbackType>
struct containNotifyRemoveTie
{
    typedef containerType containerType;

    template <typename itemType>
    using is_about = std::bool_constant <
        std::is_invocable <removeCallbackType, itemType>::value
    >;

    AINLINE containNotifyRemoveTie( containerType& container, removeCallbackType& rem_cb ) noexcept
        : container( container ), rem_cb( rem_cb )
    {
        return;
    }
    AINLINE containNotifyRemoveTie( containNotifyRemoveTie&& ) = default;
    AINLINE containNotifyRemoveTie( const containNotifyRemoveTie& ) = default;

    AINLINE containNotifyRemoveTie& operator = ( containNotifyRemoveTie&& ) = default;
    AINLINE containNotifyRemoveTie& operator = ( const containNotifyRemoveTie& ) = default;

    AINLINE containerType& GetContainer( void ) noexcept
    {
        return this->container;
    }
    AINLINE removeCallbackType& GetRemoveCallback( void ) noexcept
    {
        return this->rem_cb;
    }

    AINLINE operator containerType& ( void ) noexcept
    {
        return this->container;
    }
    AINLINE operator const containerType& ( void ) const noexcept
    {
        return this->container;
    }

private:
    [[no_unique_address]] containerType& container;
    [[no_unique_address]] removeCallbackType& rem_cb;
};

// Deducation guides.
template <typename containerType, typename removeCallbackType>
containNotifyRemoveTie( containerType&, removeCallbackType ) -> containNotifyRemoveTie <containerType, const removeCallbackType>;

// Mandatory type-trait.
template <typename>
struct is_contain_notify_remove_tie : std::false_type
{};
template <typename... tieArgs>
struct is_contain_notify_remove_tie <containNotifyRemoveTie <tieArgs...>> : std::true_type
{};
template <typename... tieArgs>
struct is_contain_notify_remove_tie <const containNotifyRemoveTie <tieArgs...>> : std::true_type
{};
template <typename... tieArgs>
struct is_contain_notify_remove_tie <containNotifyRemoveTie <tieArgs...>&> : std::true_type
{};
template <typename... tieArgs>
struct is_contain_notify_remove_tie <const containNotifyRemoveTie <tieArgs...>&> : std::true_type
{};
template <typename... tieArgs>
struct is_contain_notify_remove_tie <containNotifyRemoveTie <tieArgs...>&&> : std::true_type
{};
template <typename... tieArgs>
struct is_contain_notify_remove_tie <const containNotifyRemoveTie <tieArgs...>&&> : std::true_type
{};

// Iterates over a single item/container.
template <typename itemType, typename containerType, typename callbackType>
    requires ( eir::constructible_from <itemType, containerType&&> && std::is_invocable <callbackType, itemType&&>::value ||
               std::is_invocable <callbackType, itemType&>::value )
AINLINE void iterateOver( containerType&& container, callbackType&& cb )
{
    if constexpr ( eir::constructible_from <itemType, containerType&&> )
    {
        cb( std::forward <containerType> ( container ) );
    }
    else
    {
        for ( auto& item : container )
        {
            cb( item );
        }
    }
}

// Iterates over multiple containers, non-uniquely.
template <typename itemType, typename... containerTypes, typename callbackType>
    requires ( sizeof...( containerTypes ) > 0 && std::is_invocable <callbackType, itemType&>::value )
AINLINE void iterateOverMulti( callbackType&& cb, containerTypes&&... containers )
{
    ( iterateOver <itemType> ( containers, cb ), ... );
}

// Queries a single container and sees if the data exist in that container.
template <typename containerType, typename itemType>
AINLINE bool containerContains( const containerType& container, const itemType& item )
{
    if constexpr ( is_set <containerType>::value || is_vector <containerType>::value || is_multivaluestore <containerType>::value )
    {
        return ( container.Find( item ) );
    }
    else if constexpr ( EqualityComparableValues <containerType, itemType> )
    {
        return ( container == item );
    }
    else
    {
        for ( const itemType& ai : container )
        {
            if ( ai == item )
            {
                return true;
            }
        }
        return false;
    }
}

// Quick helper for checking multiple containers related to item presence.
template <typename itemType, typename... containerTypes>
    requires ( sizeof...( containerTypes ) > 0 )
AINLINE bool isInContainer( const itemType& item, const containerTypes&... containers )
{
    return ( containerContains( containers, item ) || ... );
}

template <typename findItemType, typename putItemType, typename... containerTypes>
    requires ( sizeof...( containerTypes ) > 0 && ( std::is_const <containerTypes>::value || ... ) == false )
AINLINE void replaceInContainer( const findItemType& oldval, putItemType&& newval, containerTypes&&... containers )
{
    auto _replace_lambda = [&]( auto& container ) LAINLINE
    {
        // I have to separate the two code-graphs because there is no compile-time splicing of variable names
        // possible using simple notation. In that case I prefer to have full control in shaping the possibly-
        // incompatible future.
        if constexpr ( is_contain_notify_add_remove_tie <decltype(container)>::value )
        {
            auto& real_container = container.GetContainer();

            if constexpr ( is_set <decltype(real_container)>::value )
            {
                if ( auto *foundNode = real_container.Find( oldval ) )
                {
                    auto& upd = real_container.BeginNodeUpdate( foundNode );

                    container.GetRemoveCallback()( upd );

                    upd = std::forward <putItemType> ( newval );

                    container.GetAddCallback()( upd );

                    real_container.EndNodeUpdate( foundNode );
                }
            }
            else if constexpr ( is_multivaluestore <decltype(real_container)>::value )
            {
                bool replaced = real_container.Replace( oldval, newval );

                if ( replaced )
                {
                    container.GetRemoveCallback()( oldval );
                    container.GetAddCallback()( newval );
                }
            }
            else if constexpr ( is_vector <decltype(real_container)>::value )
            {
                bool replaced = real_container.ReplaceValues( oldval, newval );

                if ( replaced )
                {
                    container.GetRemoveCallback()( oldval );
                    container.GetAddCallback()( newval );
                }
            }
            else if constexpr (
                EqualityComparableValues <findItemType, decltype(real_container)> &&
                std::is_assignable <decltype(real_container), putItemType>::value
                )
            {
                if ( real_container == oldval )
                {
                    container.GetRemoveCallback()( real_container );

                    real_container = std::forward <putItemType> ( newval );

                    container.GetAddCallback()( real_container );
                }
            }
            else
            {
                bool replaced = false;

                for ( auto& ival : real_container )
                {
                    if ( ival == oldval )
                    {
                        ival = newval;

                        replaced = true;
                    }
                }

                if ( replaced )
                {
                    container.GetRemoveCallback()( oldval );
                    container.GetAddCallback()( newval );
                }
            }
        }
        else
        {
            if constexpr ( is_set <decltype(container)>::value )
            {
                if ( auto *foundNode = container.Find( oldval ) )
                {
                    auto& upd = container.BeginNodeUpdate( foundNode );

                    upd = std::forward <putItemType> ( newval );

                    container.EndNodeUpdate( foundNode );
                }
            }
            else if constexpr ( is_multivaluestore <decltype(container)>::value )
            {
                container.Replace( oldval, std::forward <putItemType> ( newval ) );
            }
            // if eir::Vector, then (we could just) fall-through to default handler.
            else if constexpr ( is_vector <decltype(container)>::value )
            {
                container.ReplaceValues( oldval, std::forward <putItemType> ( newval ) );
            }
            else if constexpr (
                EqualityComparableValues <findItemType, decltype(container)> &&
                std::is_assignable <decltype(container), putItemType>::value
                )
            {
                if ( container == oldval )
                {
                    container = std::forward <putItemType> ( newval );
                }
            }
            else
            {
                for ( auto& ival : container )
                {
                    if ( ival == oldval )
                    {
                        ival = newval;
                    }
                }
            }
        }
    };

    ( _replace_lambda( containers ), ... );
}

// The word "unique" here is actually redundant.
enum class eUniqueVectorRemovalPolicy
{
    REMOVE,
    REPLACE_WITH_DEFAULT
};

template <typename itemType, eUniqueVectorRemovalPolicy vecRemPolicy = eUniqueVectorRemovalPolicy::REMOVE, typename... containerTypes>
    requires ( sizeof...( containerTypes ) > 0 && std::is_const <itemType>::value == false && ( std::is_const <containerTypes>::value || ... ) == false )
AINLINE void removeFromContainer( const itemType& findval, containerTypes&&... containers )
{
    auto _remove_lambda = [&]( auto& container ) LAINLINE
    {
        // Code-path splicing necessary for the same reason as in replaceInContainer.
        if constexpr ( is_contain_notify_add_remove_tie <decltype(container)>::value || is_contain_notify_remove_tie <decltype(container)>::value )
        {
            auto& real_container = container.GetContainer();

            if constexpr ( is_set <decltype(real_container)>::value )
            {
                if ( auto *findNode = real_container.Find( findval ) )
                {
                    container.GetRemoveCallback()( findNode->GetValue() );

                    real_container.RemoveNode( findNode );
                }
            }
            else if constexpr ( is_multivaluestore <decltype(real_container)>::value )
            {
                bool removed = real_container.RemoveAll( findval );

                if ( removed )
                {
                    container.GetRemoveCallback()( findval );
                }
            }
            else if constexpr ( is_vector <decltype(real_container)>::value && vecRemPolicy == eUniqueVectorRemovalPolicy::REMOVE )
            {
                // If the policy is not equal to REMOVE, then we fall down to the replacement logic below.
                bool removed = real_container.RemoveByValue( findval );

                if ( removed )
                {
                    container.GetRemoveCallback()( findval );
                }
            }
            else if constexpr ( std::same_as <decltype(real_container), itemType&> )
            {
                if ( findval != itemType() && real_container == findval )
                {
                    container.GetRemoveCallback()( real_container );

                    real_container = {};
                }
            }
            else
            {
                if ( findval != itemType() )
                {
                    bool didRemove = false;

                    for ( auto& ival : real_container )
                    {
                        if ( ival == findval )
                        {
                            ival = {};

                            didRemove = true;
                        }
                    }

                    if ( didRemove )
                    {
                        container.GetRemoveCallback()( findval );
                    }
                }
            }
        }
        else
        {
            if constexpr ( is_set <decltype(container)>::value )
            {
                container.Remove( findval );
            }
            else if constexpr ( is_multivaluestore <decltype(container)>::value )
            {
                container.RemoveAll( findval );
            }
            else if constexpr ( is_vector <decltype(container)>::value && vecRemPolicy == eUniqueVectorRemovalPolicy::REMOVE )
            {
                // If the policy is not equal to REMOVE, then we fall down to the replacement logic below.
                container.RemoveByValue( findval );
            }
            else if constexpr ( std::same_as <decltype(container), itemType&> )
            {
                if ( findval != itemType() && container == findval )
                {
                    container = {};
                }
            }
            else
            {
                if ( findval != itemType() )
                {
                    for ( auto& ival : container )
                    {
                        if ( ival == findval )
                        {
                            ival = {};
                        }
                    }
                }
            }
        }
    };

    ( _remove_lambda( containers ), ... );
}

} // namespace eir

#endif //_EIRREPO_ITERATION_HEADER_