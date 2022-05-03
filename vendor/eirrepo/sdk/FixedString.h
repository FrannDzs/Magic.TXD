/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/FixedString.h
*  PURPOSE:     Fixed-length String implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_SDK_FIXED_LENGTH_STRING_HEADER_
#define _EIR_SDK_FIXED_LENGTH_STRING_HEADER_

#include <stddef.h> // for size_t

#include <type_traits>

#include "eirutils.h"
#include "CharFundamentals.h"

namespace eir
{

template <CharacterType charType>
struct FixedString
{
    inline FixedString( void ) noexcept
    {
        this->chars = nullptr;
        this->len = 0;
    }
    inline FixedString( const charType *chars, size_t len ) noexcept
    {
        this->chars = chars;
        this->len = len;
    }
    inline FixedString( const charType *chars ) noexcept
    {
        this->chars = chars;
        this->len = cplen_tozero( chars );
    }
    inline FixedString( const FixedString& ) = default;
    inline FixedString( FixedString&& right ) noexcept
    {
        this->chars = right.chars;
        this->len = right.len;

        right.chars = nullptr;
        right.len = 0;
    }

    inline FixedString& operator = ( const FixedString& ) = default;
    inline FixedString& operator = ( FixedString&& right ) noexcept
    {
        this->chars = right.chars;
        this->len = right.len;

        right.chars = nullptr;
        right.len = 0;

        return *this;
    }

    inline const charType* GetConstString( void ) const noexcept
    {
        return this->chars;
    }

    inline size_t GetLength( void ) const noexcept
    {
        return this->len;
    }

    inline friend bool operator == ( const FixedString& left, const FixedString& right ) noexcept
    {
        size_t leftLen = left.len;

        if ( leftLen != right.len )
            return false;

        const charType *leftChars = left.chars;
        const charType *rightChars = right.chars;

        if ( leftChars == rightChars )
            return true;

        for ( size_t n = 0; n < leftLen; n++ )
        {
            charType left_c = leftChars[n];
            charType right_c = rightChars[n];

            if ( left_c != right_c )
            {
                return false;
            }
        }

        return true;
    }

private:
    const charType *chars;
    size_t len;
};

// Traits.
template <typename>
struct is_fixedstring : std::false_type
{};
template <typename... fsArgs>
struct is_fixedstring <FixedString <fsArgs...>> : std::true_type
{};
template <typename... fsArgs>
struct is_fixedstring <const FixedString <fsArgs...>> : std::true_type
{};
template <typename... fsArgs>
struct is_fixedstring <FixedString <fsArgs...>&> : std::true_type
{};
template <typename... fsArgs>
struct is_fixedstring <const FixedString <fsArgs...>&> : std::true_type
{};
template <typename... fsArgs>
struct is_fixedstring <FixedString <fsArgs...>&&> : std::true_type
{};
template <typename... fsArgs>
struct is_fixedstring <const FixedString <fsArgs...>&&> : std::true_type
{};

} // namespace eir

#endif //_EIR_SDK_FIXED_LENGTH_STRING_HEADER_