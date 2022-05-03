/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/UniChar.casesense.h
*  PURPOSE:     Character environment case-sensitivity routines
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _UNICHAR_CASE_SENSITIVITY_
#define _UNICHAR_CASE_SENSITIVITY_

#include "MacroUtils.h"

// The main reason this file got created is because std::ctype <char32_t> was undefined under Linux.
// Hence I have to perform these things myself.

template <typename charType>
struct toupper_lookup
{
    // NOT IMPLEMENTED.
};

template <typename charType>
struct dyna_toupper_lookup
{
    AINLINE dyna_toupper_lookup( const std::locale& locale ) : facet( std::use_facet <std::ctype <charType>> ( locale ) )
    {
        return;
    }

    AINLINE charType toupper( charType val ) const
    {
        return facet.toupper( val );
    }

    AINLINE charType tolower( charType val ) const
    {
        return facet.tolower( val );
    }

    const std::ctype <charType>& facet;
};

template <>
struct toupper_lookup <char> : dyna_toupper_lookup <char>
{
    using dyna_toupper_lookup::dyna_toupper_lookup;
};
template <>
struct toupper_lookup <wchar_t> : dyna_toupper_lookup <wchar_t>
{
    using dyna_toupper_lookup::dyna_toupper_lookup;
};
template <>
struct toupper_lookup <char16_t> : dyna_toupper_lookup <char16_t>
{
    using dyna_toupper_lookup::dyna_toupper_lookup;
};

struct utf32_wchar_toupper_lookup
{
    AINLINE utf32_wchar_toupper_lookup( const std::locale& locale ) : _dyna_redir( locale )
    {
        return;
    }

    AINLINE char32_t toupper( char32_t val ) const
    {
        return (char32_t)_dyna_redir.toupper( (wchar_t)val );
    }

    AINLINE char32_t tolower( char32_t val ) const
    {
        return (char32_t)_dyna_redir.tolower( (wchar_t)val );
    }

    dyna_toupper_lookup <wchar_t> _dyna_redir;
};

typedef std::conditional <sizeof(wchar_t) == sizeof(char32_t), utf32_wchar_toupper_lookup, dyna_toupper_lookup <char32_t>>::type char32_toupper_baseclass;

template <>
struct toupper_lookup <char32_t> : public char32_toupper_baseclass
{
    using char32_toupper_baseclass::char32_toupper_baseclass;
};

template <>
struct toupper_lookup <char8_t>
{
    AINLINE toupper_lookup( const std::locale& locale ) : _dyna_redir( locale )
    {
        return;
    }

    AINLINE char8_t toupper( char8_t val ) const
    {
        return (char8_t)_dyna_redir.toupper( (char)val );
    }

    AINLINE char8_t tolower( char8_t val ) const
    {
        return (char8_t)_dyna_redir.tolower( (char)val );
    }

    dyna_toupper_lookup <char> _dyna_redir;
};

#endif //_UNICHAR_CASE_SENSITIVITY_
