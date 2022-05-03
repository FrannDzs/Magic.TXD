/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/avlsetmaputil.h
*  PURPOSE:     Shared code between Set and Map objects
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _AVL_SET_AND_MAP_SHARED_HEADER_
#define _AVL_SET_AND_MAP_SHARED_HEADER_

#include "MacroUtils.h"
#include "AVLTree.h"

#include <type_traits>

#define MAKE_SETMAP_ITERATOR( iteratorName, hostType, nodeType, nodeRedirNode, treeMembPath, avlTreeType ) \
    struct iteratorName \
    { \
        AINLINE iteratorName( void ) = default; \
        AINLINE iteratorName( hostType& host ) noexcept : real_iter( host.treeMembPath ) \
        { \
            return; \
        } \
        AINLINE iteratorName( hostType *host ) noexcept : real_iter( host->treeMembPath ) \
        { \
            return; \
        } \
        AINLINE iteratorName( nodeType *iter ) noexcept : real_iter( iter ? (AVLNode*)&iter->nodeRedirNode : nullptr ) \
        { \
            return; \
        } \
        AINLINE iteratorName( iteratorName&& ) = default; \
        AINLINE iteratorName( const iteratorName& ) = default; \
        AINLINE ~iteratorName( void ) = default; \
        AINLINE bool IsEnd( void ) const noexcept \
        { \
            return real_iter.IsEnd(); \
        } \
        AINLINE void Increment( void ) noexcept \
        { \
            real_iter.Increment(); \
        } \
        AINLINE void Decrement( void ) noexcept \
        { \
            real_iter.Decrement(); \
        } \
        AINLINE nodeType* Resolve( void ) const noexcept \
        { \
            return AVL_GETITEM( nodeType, real_iter.Resolve(), nodeRedirNode ); \
        } \
    private: \
        typename avlTreeType::diff_node_iterator real_iter; \
    }

namespace eir
{

template <typename comparatorType, typename LT, typename RT>
concept CompareCompatibleRegularComparator =
    requires ( const LT& L, const RT& R ) { { comparatorType::compare_values( L, R ) } -> std::same_as <eCompResult>; };
template <typename comparatorType, typename LT, typename RT>
concept CompareCompatibleInverseComparator =
    requires ( const LT& R, const RT& L ) { { (eCompResult)comparatorType::compare_values( L, R ) } -> std::same_as <eCompResult>; };

template <typename comparatorType, typename LT, typename RT>
concept CompareCompatibleComparator =
    CompareCompatibleRegularComparator <comparatorType, LT, RT> ||
    CompareCompatibleInverseComparator <comparatorType, LT, RT>;

template <typename comparatorType, typename LT, typename RT>
concept NothrowCompareCompatibleRegularComparator =
    requires ( const LT& L, const RT& R ) { { comparatorType::compare_values( L, R ) } noexcept -> std::same_as <eCompResult>; };
template <typename comparatorType, typename LT, typename RT>
concept NothrowCompareCompatibleInverseComparator =
    requires ( const LT& R, const RT& L ) { { comparatorType::compare_values( L, R ) } noexcept -> std::same_as <eCompResult>; };

template <typename comparatorType, typename LT, typename RT>
concept NothrowCompareCompatibleComparator =
    ( CompareCompatibleRegularComparator <comparatorType, LT, RT> == false || NothrowCompareCompatibleRegularComparator <comparatorType, LT, RT> ) &&
    ( CompareCompatibleInverseComparator <comparatorType, LT, RT> == false || NothrowCompareCompatibleInverseComparator <comparatorType, LT, RT> );

// Default comparator for objects inside the Map/Set.
struct GenericDefaultComparator
{
    template <typename firstKeyType, typename secondKeyType> requires ( ThreeWayComparableValues <firstKeyType, secondKeyType> )
    static AINLINE eCompResult compare_values( const firstKeyType& left, const secondKeyType& right ) noexcept(noexcept(DefaultValueCompare( left, right )))
    {
        // We want to hide signed-ness problems, kinda.
        if constexpr ( std::is_integral <firstKeyType>::value == true && std::is_integral <secondKeyType>::value == true )
        {
            if constexpr ( std::is_signed <firstKeyType>::value == true && std::is_signed <secondKeyType>::value == false )
            {
                if ( left < 0 )
                {
                    return eCompResult::LEFT_LESS;
                }

                return ( DefaultValueCompare( (typename std::make_unsigned <firstKeyType>::type)left, right ) );
            }
            else if constexpr ( std::is_signed <firstKeyType>::value == false && std::is_signed <secondKeyType>::value == true )
            {
                if ( right < 0 )
                {
                    return eCompResult::LEFT_GREATER;
                }

                return ( DefaultValueCompare( left, (typename std::make_unsigned <secondKeyType>::type)right ) );
            }
            else
            {
                return DefaultValueCompare( left, right );
            }
        }
        else
        {
            // This is important because not all key types come with a usable "operator <" overload,
            // for example it is not really a good idea for eir::String because you ought to take
            // case-sensitivity into account!
            return DefaultValueCompare( left, right );
        }
    }
};

}

#endif //_AVL_SET_AND_MAP_SHARED_HEADER_