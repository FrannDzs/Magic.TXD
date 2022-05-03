/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/UniqueIterate.h
*  PURPOSE:     Unique iteration over a combination of lists
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRSDK_UNIQUE_ITER_HEADER_
#define _EIRSDK_UNIQUE_ITER_HEADER_

#include "Iterate.h"

#include <type_traits>
#include <optional>
#include <concepts>
#include <tuple>

namespace eir
{

template <typename containerType>
concept is_unique_container =
    is_multivaluestore <containerType>::value || is_set <containerType>::value;

template <typename itemType, typename containerType, typename callbackType, typename begIterType, typename endIterType>
    requires ( std::is_invocable <callbackType, itemType&&>::value )
AINLINE std::invoke_result_t <callbackType, itemType> _unique_iterate_single( begIterType& iter, endIterType& enditer, containerType&& container, callbackType&& cb )
{
    while ( iter != enditer )
    {
        itemType value = *iter;

        bool has_prev_instance = false;

        if constexpr ( is_unique_container <containerType> == false )
        {
            for ( auto begiter = std::begin( container ); begiter != iter; ++begiter )
            {
                if ( value == *begiter )
                {
                    has_prev_instance = true;
                    break;
                }
            }
        }

        if ( has_prev_instance == false )
        {
            if constexpr ( std::is_constructible <bool, std::invoke_result_t <callbackType, itemType>>::value )
            {
                auto result = cb( std::move( value ) );

                if ( bool(result) )
                {
                    return result;
                }
            }
            else
            {
                cb( std::move( value ) );
            }
        }

        ++iter;
    }

    if constexpr ( std::same_as <std::invoke_result_t <callbackType, itemType>, void> == false )
    {
        return {};
    }
}

// Iterates over a single container, returning each unique value inside exactly once.
template <typename itemType, typename containerType, typename callbackType>
inline std::invoke_result_t <callbackType, itemType> unique_iterate_single( containerType&& container, callbackType&& cb )
{
    if constexpr ( std::convertible_to <containerType, itemType> )
    {
        cb( std::forward <containerType> ( container ) );
    }
    else
    {
        auto iter = std::begin( container );
        auto enditer = std::end( container );

        return _unique_iterate_single <itemType> ( iter, enditer, std::forward <containerType> ( container ), std::forward <callbackType> ( cb ) );
    }
}

// Helper.
template <typename itemType, typename containerType>
inline decltype(auto) _begin_from( const containerType& container, const itemType& from )
{
    if constexpr ( is_set <containerType>::value )
    {
        auto *findNode = container.Find( from );

        return typename containerType::std_iterator( findNode );
    }
    else if constexpr ( is_multivaluestore <containerType>::value )
    {
        return container.begin_from( from );
    }
    else
    {
        auto iter = std::begin( container );
        auto end_iter = std::end( container );

        while ( iter != end_iter )
        {
            itemType value = *iter;

            if ( value == from )
            {
                return iter;
            }

            ++iter;
        }

        return iter;
    }
}

// Helper.
template <typename itemType, typename optionalStore>
AINLINE itemType _fetch_from_optional( const optionalStore& store )
{
    if constexpr ( std::same_as <optionalStore, std::optional <itemType>> )
    {
        return store.value();
    }
    else
    {
        return itemType(store);
    }
}

template <typename itemType, typename optionalStore = std::optional <itemType>, typename... containerTypes, typename... excludeContainerTypes>
    requires ( sizeof...( containerTypes ) > 0 )
AINLINE optionalStore uniqueFetchNextTuple( typename std::type_identity <optionalStore>::type prev, const std::tuple <containerTypes...>& containers, const std::tuple <excludeContainerTypes...>& exclude_containers = std::tuple() )
{
    size_t container_idx = 0;

    auto does_contain_to = [&]( size_t container_cnt, const itemType& item ) LAINLINE -> bool
    {
        if constexpr ( sizeof...( excludeContainerTypes ) > 0 )
        {
            bool is_excluded =
                std::apply( [&]( const excludeContainerTypes&... args ) LAINLINE { return ( containerContains( args, item ) || ... ); }, exclude_containers );

            if ( is_excluded )
            {
                return true;
            }
        }

        size_t findidx = 0;

        auto _container_find = [&]( const auto& container ) LAINLINE -> bool
        {
            if ( findidx >= container_cnt )
                return false;

            findidx++;

            return ( containerContains( container, item ) );
        };

        return std::apply( [&]( const containerTypes&... args ) LAINLINE { return ( _container_find( args ) || ... ); }, containers );
    };

    // This is important for default-value-initialization of POD types (typically a pointer).
    optionalStore fres = optionalStore();

    bool has_found_prev_yet = false;

    if ( bool(prev) == false )
    {
        has_found_prev_yet = true;
    }

    auto iterate_container = [&]( const auto& container ) LAINLINE -> bool
    {
        if constexpr ( std::is_assignable <itemType&, decltype(container)>::value )
        {
            if ( has_found_prev_yet == true )
            {
                if ( does_contain_to( container_idx, container ) == false )
                {
                    fres = container;
                    return true;
                }
            }
            else
            {
                if ( container == _fetch_from_optional <itemType> ( prev ) )
                {
                    has_found_prev_yet = true;
                }
            }
        }
        else
        {
            optional_struct_space <decltype(std::begin( container ))> _from_iter;
            optional_struct_space_init <decltype(std::begin( container ))> _from_iter_init;
            auto enditer = std::end( container );

            if ( has_found_prev_yet == false )
            {
                auto fiter = _begin_from <itemType> ( container, _fetch_from_optional <itemType> ( prev ) );
                if ( !( fiter != enditer ) )
                {
                    goto next;
                }
                ++fiter;
                _from_iter_init = { _from_iter, std::move( fiter ) };
                has_found_prev_yet = true;
            }
            else
            {
                _from_iter_init = { _from_iter, std::begin( container ) };
            }

            {
                optionalStore res = _unique_iterate_single <itemType> ( _from_iter.get(), enditer, container,
                    [&]( itemType&& item ) LAINLINE -> optionalStore
                    {
                        if ( does_contain_to( container_idx, item ) == false )
                        {
                            return std::move( item );
                        }
                        return optionalStore();
                    }
                );

                if ( bool(res) )
                {
                    fres = std::move( res );
                    return true;
                }
            }
        next:;
        }

        container_idx++;
        return false;
    };

    std::apply( [&] ( const containerTypes&... args ) LAINLINE { ( iterate_container( args ) || ... ); }, containers );

    return fres;
}

template <typename itemType, typename... containerTypes, typename... excludeContainerTypes>
    requires ( sizeof...( containerTypes ) > 0 && std::is_pointer <itemType>::value )
AINLINE itemType uniqueFetchNextPtrTuple( itemType prev, const std::tuple <containerTypes...>& containers, const std::tuple <excludeContainerTypes...>& exclude_containers  = std::tuple() )
{
    return uniqueFetchNextTuple <itemType, itemType> ( std::forward <itemType> ( prev ), containers, exclude_containers );
}

template <typename itemType, typename optionalStore = std::optional <itemType>, typename... containerTypes>
    requires ( sizeof...( containerTypes ) > 0 )
AINLINE optionalStore uniqueFetchNext( typename std::type_identity <optionalStore>::type prev, const containerTypes&... containers )
{
    return uniqueFetchNextTuple <itemType, optionalStore> ( std::forward <optionalStore> ( prev ), std::tuple <const containerTypes&...> ( containers... ) );
}

template <typename itemType, typename... containerTypes>
    requires ( sizeof...( containerTypes ) > 0 && std::is_pointer <itemType>::value )
AINLINE itemType uniqueFetchNextPtr( itemType prev, const containerTypes&... containers )
{
    return uniqueFetchNext <itemType, itemType> ( std::forward <itemType> ( prev ), containers... );
}

// The same concept as uniqueFetchNext but it does iterate using a callback, not requiring
// re-entering of containers, being better performance for local-iteration-purposes.
template <typename itemType, typename callbackType, typename... containerTypes, typename... excludeContainerTypes>
    requires ( sizeof...( containerTypes ) > 0 && std::is_invocable <callbackType, itemType&>::value )
AINLINE void uniqueIterateTuple( callbackType&& cb, std::tuple <containerTypes...> containers, const std::tuple <excludeContainerTypes...>& exclude_containers = std::tuple() )
{
    size_t container_idx = 0;

    auto does_contain_to = [&]( size_t container_cnt, const itemType& item ) LAINLINE -> bool
    {
        if constexpr ( sizeof...( excludeContainerTypes ) > 0 )
        {
            bool is_excluded =
                std::apply( [&]( const excludeContainerTypes&... args ) LAINLINE { return ( containerContains( args, item ) || ... ); }, exclude_containers );

            if ( is_excluded )
            {
                return true;
            }
        }

        size_t findidx = 0;

        auto _container_find = [&]( const auto& container ) LAINLINE -> bool
        {
            if ( findidx >= container_cnt )
                return false;

            findidx++;

            return ( containerContains( container, item ) );
        };

        return std::apply( [&]( const containerTypes&... args ) LAINLINE { return ( _container_find( args ) || ... ); }, containers );
    };

    auto iterate_container = [&]( const auto& container ) LAINLINE
    {
        unique_iterate_single <itemType> ( container,
            [&]( const itemType& item ) LAINLINE
            {
                if ( does_contain_to( container_idx, item ) == false )
                {
                    cb( (itemType&)item );
                }
            }
        );

        container_idx++;
    };

    std::apply( [&] ( const containerTypes&... args ) LAINLINE { ( iterate_container( args ), ... ); }, containers );
}

template <typename itemType, typename callbackType, typename... containerTypes>
    requires ( sizeof...( containerTypes ) > 0 && std::is_invocable <callbackType, itemType&>::value )
AINLINE void uniqueIterate( callbackType&& cb, const containerTypes&... containers )
{
    uniqueIterateTuple <itemType> ( std::forward <callbackType> ( cb ), std::tuple <const containerTypes&...> ( containers... ) );
}

} // namespace eir

#endif //_EIRSDK_UNIQUE_ITER_HEADER_