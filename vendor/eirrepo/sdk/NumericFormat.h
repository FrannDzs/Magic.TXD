/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/NumericFormat.h
*  PURPOSE:     Numeric formatting with Eir SDK character strings
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// Usually the standard library is the provider for converting numbers into
// string representations. But since the algorithms behind that magic are
// rather simple, we want to provide them ourselves.

#ifndef _EIR_SDK_NUMERIC_FORMATTING_
#define _EIR_SDK_NUMERIC_FORMATTING_

#include "Exceptions.h"
#include "MacroUtils.h"
#include "String.h"
#include "UniChar.h"
#include "CharFundamentals.h"

#include <type_traits>

namespace eir
{

template <eir::CharacterType charType, MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager, typename numberType, typename... Args>
inline eir::String <charType, allocatorType, exceptMan> to_string( const numberType& value, unsigned int base = 10, bool upperCase = false, Args&&... theArgs )
{
    eir::String <charType, allocatorType, exceptMan> strOut( eir::constr_with_alloc::DEFAULT, std::forward <Args> ( theArgs )... );

    if ( base == 0 || base > 16 )
        return strOut;

    if constexpr ( std::is_integral <numberType>::value )
    {
        numberType bitStream = value;

        bool isSigned = false;

        typename std::make_unsigned <numberType>::type u_bitStream;

        if constexpr ( std::is_signed <numberType>::value )
        {
            isSigned = ( bitStream < 0 );

            if ( isSigned )
            {
                u_bitStream = (typename std::make_unsigned <numberType>::type)( -bitStream );
            }
            else
            {
                u_bitStream = (typename std::make_unsigned <numberType>::type)( bitStream );
            }
        }
        else
        {
            u_bitStream = bitStream;
        }

        if ( u_bitStream == 0 )
        {
            // Just print out the zero.
            strOut += GetCharacterForNumeric <charType, exceptMan> ( 0, upperCase );
        }
        else
        {
            // We build up the polynome in reverse order.
            // Then we use the eir::String reverse method to turn it around.
            while ( u_bitStream > 0 )
            {
                unsigned int polynome_coeff = ( u_bitStream % base );

                // Move down all polynome entries by one.
                u_bitStream /= base;

                // Add this polynome value.
                strOut += GetCharacterForNumeric <charType, exceptMan> ( polynome_coeff, upperCase );
            }
        }

        // Add a sign if required.
        if ( isSigned )
        {
            strOut += GetMinusSignCodepoint <charType, exceptMan> ();
        }

        // Reverse the (polynome) string.
        // This only works because each number is only one code point.
        strOut.reverse();
    }
    else
    {
        static_assert( std::is_integral <numberType>::value == false, "invalid numberType in to_string conversion" );
    }

    return strOut;
}

template <typename charType, MemoryAllocator allocatorType, size_t digitcnt, typename exceptMan = eir::DefaultExceptionManager, typename numberType, typename... Args>
inline eir::String <charType, allocatorType, exceptMan> to_string_digitfill( const numberType& value, unsigned int base = 10, bool upperCase = false, Args&&... theArgs )
{
    eir::String <charType, allocatorType, exceptMan> result = to_string <charType, allocatorType, exceptMan> ( value, base, upperCase, std::forward <Args> ( theArgs )... );

    size_t result_len = result.GetLength();

    if ( result_len < digitcnt )
    {
        charType nuldigit = GetCharacterForNumeric <charType> ( 0 );

        size_t digitreq = ( digitcnt - result_len );

        while ( digitreq )
        {
            result.Insert( 0, &nuldigit, 1 );

            digitreq--;
        }
    }

    return result;
}

template <typename numberType, typename exceptMan = eir::DefaultExceptionManager, typename charType>
inline numberType to_number_len( const charType *strnum, size_t strlen, unsigned int base = 10 )
{
    // TODO: add floating point support.

    if ( base == 0 || base > 16 )
        return 0;

    typedef typename character_env <charType>::ucp_t ucp_t;

    charenv_charprov_tocplen <charType> prov( strnum, strlen );
    character_env_iterator <charType, decltype(prov)> iter( strnum, std::move( prov ) );

    bool isSigned = false;

    (void)isSigned;

    if constexpr ( std::is_signed <numberType>::value )
    {
        if ( !iter.IsEnd() )
        {
            ucp_t ucp = iter.Resolve();

            if ( ucp == GetMinusSignCodepoint <ucp_t, exceptMan> () )
            {
                isSigned = true;

                iter.Increment();
            }
        }
    }

    numberType result = 0;

    while ( !iter.IsEnd() )
    {
        ucp_t ucp = iter.ResolveAndIncrement();

        unsigned int val;
        bool got_val = GetNumericForCharacter( ucp, val );

        if ( !got_val )
        {
            return 0;
        }

        if ( val >= base )
        {
            return 0;
        }

        result *= base;
        result += val;
    }

    if constexpr ( std::is_signed <numberType>::value )
    {
        return ( isSigned ? -result : result );
    }
    else
    {
        return result;
    }
}

template <typename numberType, typename exceptMan = eir::DefaultExceptionManager, typename charType>
inline numberType to_number( const charType *strnum, unsigned int base = 10 )
{
    return to_number_len <numberType, exceptMan> ( strnum, cplen_tozero( strnum ), base );
}

}

#endif //_EIR_SDK_NUMERIC_FORMATTING_
