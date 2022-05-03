/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/StringUtils.h
*  PURPOSE:     Common string helpers
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _STRING_COMMON_SDK_UTILITIES_
#define _STRING_COMMON_SDK_UTILITIES_

#include "String.h"
#include "FixedString.h"
#include "MultiString.h"
#include "UniChar.h"

template <bool case_sensitive>
struct lexical_string_comparator
{
    template <typename charType, typename... stringArgs>
    AINLINE static eir::eCompResult compare_values( const charType *left, const eir::String <charType, stringArgs...>& right )
    {
        return eir::flip_comp_result( BoundedStringCompare( right.GetConstString(), right.GetLength(), left, case_sensitive ) );
    }

    template <typename charType, typename... stringArgs>
    AINLINE static eir::eCompResult compare_values( const eir::String <charType, stringArgs...>& left, const charType *right )
    {
        return ( BoundedStringCompare( left.GetConstString(), left.GetLength(), right, case_sensitive ) );
    }

    template <typename charType, typename... firstStringArgs, typename... secondStringArgs>
    AINLINE static eir::eCompResult compare_values( const eir::String <charType, firstStringArgs...>& left, const eir::String <charType, secondStringArgs...>& right )
    {
        return ( FixedStringCompare( left.GetConstString(), left.GetLength(), right.GetConstString(), right.GetLength(), case_sensitive ) );
    }

    // Stational multi-string.
    template <typename... firstStringArgs, typename... secondStringArgs>
    AINLINE static eir::eCompResult compare_values( const eir::MultiString <firstStringArgs...>& left, const eir::MultiString <secondStringArgs...>& right )
    {
        return left.compare( right, case_sensitive );
    }

    template <typename... stringArgs, typename charType>
    AINLINE static eir::eCompResult compare_values( const eir::MultiString <stringArgs...>& left, const charType *right )
    {
        return left.compare( right, case_sensitive );
    }

    template <typename... stringArgs, typename charType>
    AINLINE static eir::eCompResult compare_values( const charType *left, const eir::MultiString <stringArgs...>& right )
    {
        return eir::flip_comp_result( right.compare( left, case_sensitive ) );
    }

    template <typename... firstStringArgs, typename... secondStringArgs, typename charType>
    AINLINE static eir::eCompResult compare_values( const eir::MultiString <firstStringArgs...>& left, const eir::String <charType, secondStringArgs...>& right )
    {
        return left.compare( right, case_sensitive );
    }

    template <typename... firstStringArgs, typename... secondStringArgs, typename charType>
    AINLINE static eir::eCompResult compare_values( const eir::String <charType, firstStringArgs...>& left, const eir::MultiString <secondStringArgs...>& right )
    {
        return eir::flip_comp_result( right.compare( left, case_sensitive ) );
    }

    // For the fixed string.
    template <typename charType>
    AINLINE static eir::eCompResult compare_values( const charType *left, const eir::FixedString <charType>& right )
    {
        return eir::flip_comp_result( BoundedStringCompare( right.GetConstString(), right.GetLength(), left, case_sensitive ) );
    }

    template <typename charType>
    AINLINE static eir::eCompResult compare_values( const eir::FixedString <charType>& left, const charType *right )
    {
        return ( BoundedStringCompare( left.GetConstString(), left.GetLength(), right, case_sensitive ) );
    }

    template <typename charType>
    AINLINE static eir::eCompResult compare_values( const eir::FixedString <charType>& left, const eir::FixedString <charType>& right )
    {
        return ( FixedStringCompare( left.GetConstString(), left.GetLength(), right.GetConstString(), right.GetLength(), case_sensitive ) );
    }

    // ... with the dynamic string.
    template <typename charType, typename... stringArgs>
    AINLINE static eir::eCompResult compare_values( const eir::String <charType, stringArgs...>& left, const eir::FixedString <charType>& right )
    {
        return ( FixedStringCompare( left.GetConstString(), left.GetLength(), right.GetConstString(), right.GetLength(), case_sensitive ) );
    }

    template <typename charType, typename... stringArgs>
    AINLINE static eir::eCompResult compare_values( const eir::FixedString <charType>& left, const eir::String <charType, stringArgs...>& right )
    {
        return ( FixedStringCompare( left.GetConstString(), left.GetLength(), right.GetConstString(), right.GetLength(), case_sensitive ) );
    }

    // ... with the multi string.
    template <typename charType, typename... stringArgs>
    AINLINE static eir::eCompResult compare_values( const eir::MultiString <stringArgs...>& left, const eir::FixedString <charType>& right )
    {
        return left.compare( right, case_sensitive );
    }

    template <typename charType, typename... stringArgs>
    AINLINE static eir::eCompResult compare_values( const eir::FixedString <charType>& left, const eir::MultiString <stringArgs...>& right )
    {
        return eir::flip_comp_result( right.compare( left, case_sensitive ) );
    }
};

#endif //_STRING_COMMON_SDK_UTILITIES_
