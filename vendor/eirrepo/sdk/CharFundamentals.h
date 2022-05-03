/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/CharFundamentals.h
*  PURPOSE:     Character traits of the Eir SDK.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRSDK_CHARACTER_FUND_HEADER_
#define _EIRSDK_CHARACTER_FUND_HEADER_

#include "MacroUtils.h"

#include <type_traits>

namespace eir
{

template <typename charType>
concept CharacterType =
    std::is_same <typename std::remove_const <charType>::type, char>::value ||
    std::is_same <typename std::remove_const <charType>::type, wchar_t>::value ||
    std::is_same <typename std::remove_const <charType>::type, char8_t>::value ||
    std::is_same <typename std::remove_const <charType>::type, char16_t>::value ||
    std::is_same <typename std::remove_const <charType>::type, char32_t>::value;

// Important helper for many string functions.
template <eir::CharacterType subCharType>
static inline const subCharType (&GetEmptyString( void ) noexcept)[1]
{
    static const subCharType empty_string[] = { (subCharType)0 };
    return empty_string;
}

template <CharacterType ucpType>
AINLINE bool GetNumericForCharacter( ucpType cp, unsigned int& valueOut )
{
    if constexpr ( std::is_same <ucpType, char>::value )
    {
        switch( cp )
        {
        case '0': valueOut = 0; return true;
        case '1': valueOut = 1; return true;
        case '2': valueOut = 2; return true;
        case '3': valueOut = 3; return true;
        case '4': valueOut = 4; return true;
        case '5': valueOut = 5; return true;
        case '6': valueOut = 6; return true;
        case '7': valueOut = 7; return true;
        case '8': valueOut = 8; return true;
        case '9': valueOut = 9; return true;
        case 'a': case 'A': valueOut = 10; return true;
        case 'b': case 'B': valueOut = 11; return true;
        case 'c': case 'C': valueOut = 12; return true;
        case 'd': case 'D': valueOut = 13; return true;
        case 'e': case 'E': valueOut = 14; return true;
        case 'f': case 'F': valueOut = 15; return true;
        }

        return false;
    }
    else if constexpr ( std::is_same <ucpType, wchar_t>::value )
    {
        switch( cp )
        {
        case L'0': valueOut = 0; return true;
        case L'1': valueOut = 1; return true;
        case L'2': valueOut = 2; return true;
        case L'3': valueOut = 3; return true;
        case L'4': valueOut = 4; return true;
        case L'5': valueOut = 5; return true;
        case L'6': valueOut = 6; return true;
        case L'7': valueOut = 7; return true;
        case L'8': valueOut = 8; return true;
        case L'9': valueOut = 9; return true;
        case L'a': case L'A': valueOut = 10; return true;
        case L'b': case L'B': valueOut = 11; return true;
        case L'c': case L'C': valueOut = 12; return true;
        case L'd': case L'D': valueOut = 13; return true;
        case L'e': case L'E': valueOut = 14; return true;
        case L'f': case L'F': valueOut = 15; return true;
        }

        return false;
    }
    else if constexpr ( std::is_same <ucpType, char16_t>::value )
    {
        switch( cp )
        {
        case u'0': valueOut = 0; return true;
        case u'1': valueOut = 1; return true;
        case u'2': valueOut = 2; return true;
        case u'3': valueOut = 3; return true;
        case u'4': valueOut = 4; return true;
        case u'5': valueOut = 5; return true;
        case u'6': valueOut = 6; return true;
        case u'7': valueOut = 7; return true;
        case u'8': valueOut = 8; return true;
        case u'9': valueOut = 9; return true;
        case u'a': case u'A': valueOut = 10; return true;
        case u'b': case u'B': valueOut = 11; return true;
        case u'c': case u'C': valueOut = 12; return true;
        case u'd': case u'D': valueOut = 13; return true;
        case u'e': case u'E': valueOut = 14; return true;
        case u'f': case u'F': valueOut = 15; return true;
        }

        return false;
    }
    else if constexpr ( std::is_same <ucpType, char32_t>::value )
    {
        switch( cp )
        {
        case U'0': valueOut = 0; return true;
        case U'1': valueOut = 1; return true;
        case U'2': valueOut = 2; return true;
        case U'3': valueOut = 3; return true;
        case U'4': valueOut = 4; return true;
        case U'5': valueOut = 5; return true;
        case U'6': valueOut = 6; return true;
        case U'7': valueOut = 7; return true;
        case U'8': valueOut = 8; return true;
        case U'9': valueOut = 9; return true;
        case U'a': case U'A': valueOut = 10; return true;
        case U'b': case U'B': valueOut = 11; return true;
        case U'c': case U'C': valueOut = 12; return true;
        case U'd': case U'D': valueOut = 13; return true;
        case U'e': case U'E': valueOut = 14; return true;
        case U'f': case U'F': valueOut = 15; return true;
        }

        return false;
    }
    else if constexpr ( std::is_same <ucpType, char8_t>::value )
    {
        switch( cp )
        {
        case u8'0': valueOut = 0; return true;
        case u8'1': valueOut = 1; return true;
        case u8'2': valueOut = 2; return true;
        case u8'3': valueOut = 3; return true;
        case u8'4': valueOut = 4; return true;
        case u8'5': valueOut = 5; return true;
        case u8'6': valueOut = 6; return true;
        case u8'7': valueOut = 7; return true;
        case u8'8': valueOut = 8; return true;
        case u8'9': valueOut = 9; return true;
        case u8'a': case u8'A': valueOut = 10; return true;
        case u8'b': case u8'B': valueOut = 11; return true;
        case u8'c': case u8'C': valueOut = 12; return true;
        case u8'd': case u8'D': valueOut = 13; return true;
        case u8'e': case u8'E': valueOut = 14; return true;
        case u8'f': case u8'F': valueOut = 15; return true;
        }

        return false;
    }

    return false;
}

template <CharacterType charType, typename exceptMan = eir::DefaultExceptionManager>
AINLINE charType GetMinusSignCodepoint( void )
{
    if constexpr ( std::is_same <charType, char>::value )
    {
        return '-';
    }
    if constexpr ( std::is_same <charType, wchar_t>::value )
    {
        return L'-';
    }
    if constexpr ( std::is_same <charType, char32_t>::value )
    {
        return U'-';
    }
    if constexpr ( std::is_same <charType, char16_t>::value )
    {
        return u'-';
    }
    if constexpr ( std::is_same <charType, char8_t>::value )
    {
        return (char8_t)'-';
    }

    exceptMan::throw_codepoint( L"invalid character type in GetMinusSignCodepoint" );
}

template <CharacterType charType, typename exceptMan = eir::DefaultExceptionManager>
AINLINE constexpr charType GetCharacterForNumeric( unsigned int numeric, bool upperCase = false )
{
    if constexpr ( std::is_same <charType, char>::value )
    {
        switch( numeric )
        {
        case 0: return '0';
        case 1: return '1';
        case 2: return '2';
        case 3: return '3';
        case 4: return '4';
        case 5: return '5';
        case 6: return '6';
        case 7: return '7';
        case 8: return '8';
        case 9: return '9';
        case 10: return ( upperCase ? 'A' : 'a' );
        case 11: return ( upperCase ? 'B' : 'b' );
        case 12: return ( upperCase ? 'C' : 'c' );
        case 13: return ( upperCase ? 'D' : 'd' );
        case 14: return ( upperCase ? 'E' : 'e' );
        case 15: return ( upperCase ? 'F' : 'f' );
        }

        return '?';
    }
    if constexpr ( std::is_same <charType, wchar_t>::value )
    {
        switch( numeric )
        {
        case 0: return L'0';
        case 1: return L'1';
        case 2: return L'2';
        case 3: return L'3';
        case 4: return L'4';
        case 5: return L'5';
        case 6: return L'6';
        case 7: return L'7';
        case 8: return L'8';
        case 9: return L'9';
        case 10: return ( upperCase ? L'A' : L'a' );
        case 11: return ( upperCase ? L'B' : L'b' );
        case 12: return ( upperCase ? L'C' : L'c' );
        case 13: return ( upperCase ? L'D' : L'd' );
        case 14: return ( upperCase ? L'E' : L'e' );
        case 15: return ( upperCase ? L'F' : L'f' );
        }

        return L'?';
    }
    if constexpr ( std::is_same <charType, char32_t>::value )
    {
        switch( numeric )
        {
        case 0: return U'0';
        case 1: return U'1';
        case 2: return U'2';
        case 3: return U'3';
        case 4: return U'4';
        case 5: return U'5';
        case 6: return U'6';
        case 7: return U'7';
        case 8: return U'8';
        case 9: return U'9';
        case 10: return ( upperCase ? U'A' : U'a' );
        case 11: return ( upperCase ? U'B' : U'b' );
        case 12: return ( upperCase ? U'C' : U'c' );
        case 13: return ( upperCase ? U'D' : U'd' );
        case 14: return ( upperCase ? U'E' : U'e' );
        case 15: return ( upperCase ? U'F' : U'f' );
        }

        return U'?';
    }
    if constexpr ( std::is_same <charType, char16_t>::value )
    {
        switch( numeric )
        {
        case 0: return u'0';
        case 1: return u'1';
        case 2: return u'2';
        case 3: return u'3';
        case 4: return u'4';
        case 5: return u'5';
        case 6: return u'6';
        case 7: return u'7';
        case 8: return u'8';
        case 9: return u'9';
        case 10: return ( upperCase ? u'A' : u'a' );
        case 11: return ( upperCase ? u'B' : u'b' );
        case 12: return ( upperCase ? u'C' : u'c' );
        case 13: return ( upperCase ? u'D' : u'd' );
        case 14: return ( upperCase ? u'E' : u'e' );
        case 15: return ( upperCase ? u'F' : u'f' );
        }

        return u'?';
    }
    if constexpr ( std::is_same <charType, char8_t>::value )
    {
        switch( numeric )
        {
        case 0: return u8'0';
        case 1: return u8'1';
        case 2: return u8'2';
        case 3: return u8'3';
        case 4: return u8'4';
        case 5: return u8'5';
        case 6: return u8'6';
        case 7: return u8'7';
        case 8: return u8'8';
        case 9: return u8'9';
        case 10: return ( upperCase ? u8'A' : u8'a' );
        case 11: return ( upperCase ? u8'B' : u8'b' );
        case 12: return ( upperCase ? u8'C' : u8'c' );
        case 13: return ( upperCase ? u8'D' : u8'd' );
        case 14: return ( upperCase ? u8'E' : u8'e' );
        case 15: return ( upperCase ? u8'F' : u8'f' );
        }

        return u8'?';
    }

    exceptMan::throw_codepoint( L"invalid character type in GetCharacterForNumeric" );
}

template <CharacterType charType>
AINLINE constexpr charType GetSpaceCharacter( void )
{
    if constexpr ( std::is_same <charType, char>::value )
    {
        return ' ';
    }
    if constexpr ( std::is_same <charType, wchar_t>::value )
    {
        return L' ';
    }
    if constexpr ( std::is_same <charType, char8_t>::value )
    {
        return u8' ';
    }
    if constexpr ( std::is_same <charType, char16_t>::value )
    {
        return u' ';
    }
    if constexpr ( std::is_same <charType, char32_t>::value )
    {
        return U' ';
    }
}

template <CharacterType charType>
AINLINE constexpr charType GetTabulatorCharacter( void )
{
    if constexpr ( std::is_same <charType, char>::value )
    {
        return '\t';
    }
    if constexpr ( std::is_same <charType, wchar_t>::value )
    {
        return L'\t';
    }
    if constexpr ( std::is_same <charType, char8_t>::value )
    {
        return u8'\t';
    }
    if constexpr ( std::is_same <charType, char16_t>::value )
    {
        return u'\t';
    }
    if constexpr ( std::is_same <charType, char32_t>::value )
    {
        return U'\t';
    }
}

template <CharacterType charType>
AINLINE constexpr charType GetNewlineCharacter( void )
{
    if constexpr ( std::is_same <charType, char>::value )
    {
        return '\n';
    }
    if constexpr ( std::is_same <charType, wchar_t>::value )
    {
        return L'\n';
    }
    if constexpr ( std::is_same <charType, char8_t>::value )
    {
        return u8'\n';
    }
    if constexpr ( std::is_same <charType, char16_t>::value )
    {
        return u'\n';
    }
    if constexpr ( std::is_same <charType, char32_t>::value )
    {
        return U'\n';
    }
}

template <CharacterType charType>
AINLINE constexpr charType GetCarriageReturnCharacter( void )
{
    if constexpr ( std::is_same <charType, char>::value )
    {
        return '\r';
    }
    if constexpr ( std::is_same <charType, wchar_t>::value )
    {
        return L'\r';
    }
    if constexpr ( std::is_same <charType, char8_t>::value )
    {
        return u8'\r';
    }
    if constexpr ( std::is_same <charType, char16_t>::value )
    {
        return u'\r';
    }
    if constexpr ( std::is_same <charType, char32_t>::value )
    {
        return U'\r';
    }
}

template <CharacterType charType>
AINLINE constexpr charType GetDotCharacter( void )
{
    if constexpr ( std::is_same <charType, char>::value )
    {
        return '.';
    }
    if constexpr ( std::is_same <charType, wchar_t>::value )
    {
        return L'.';
    }
    if constexpr ( std::is_same <charType, char8_t>::value )
    {
        return u8'.';
    }
    if constexpr ( std::is_same <charType, char16_t>::value )
    {
        return u'.';
    }
    if constexpr ( std::is_same <charType, char32_t>::value )
    {
        return U'.';
    }
}

} // namespace eir

#endif //_EIRSDK_CHARACTER_FUND_HEADER_