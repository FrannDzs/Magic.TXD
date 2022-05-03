/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Endian.h
*  PURPOSE:     Endianness utilities header
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _ENDIAN_COMPAT_HEADER_
#define _ENDIAN_COMPAT_HEADER_

#include "MacroUtils.h"

// For the inline functions.
#include <stdlib.h>

#include <type_traits>
#include <bit>

namespace eir
{

// Endianness compatibility definitions.
namespace endian
{
    AINLINE static constexpr bool is_little_endian( void ) noexcept
    {
        // This does only make sense if the platform does specify endianness.
        static_assert( std::endian::native == std::endian::little || std::endian::native == std::endian::big );

        // Thanks to new C++ features we have this one in the bag!
        return ( std::endian::native == std::endian::little );
    }

    template <typename numberType>
    AINLINE numberType byte_swap_fast( const char *data ) noexcept
    {
        static_assert( std::is_trivially_move_constructible <numberType>::value );

        union
        {
            alignas(numberType) char swapped_data[ sizeof(numberType) ];
            numberType swapped_data_asval;
        };

        for ( unsigned int n = 0; n < sizeof(numberType); n++ )
        {
            swapped_data[ n ] = data[ ( sizeof(numberType) - 1 ) - n ];
        }

        return swapped_data_asval;
    }

#ifdef _MSC_VER
    template <> AINLINE short byte_swap_fast( const char *data ) noexcept               { return _byteswap_ushort( *(unsigned short*)data ); }
    template <> AINLINE unsigned short byte_swap_fast( const char *data ) noexcept      { return _byteswap_ushort( *(unsigned short*)data ); }
    template <> AINLINE int byte_swap_fast( const char *data ) noexcept                 { return _byteswap_ulong( *(unsigned int*)data ); }
    template <> AINLINE unsigned int byte_swap_fast( const char *data ) noexcept        { return _byteswap_ulong( *(unsigned int*)data ); }
    template <> AINLINE long byte_swap_fast( const char *data ) noexcept                { return _byteswap_ulong( *(unsigned long*)data ); }
    template <> AINLINE unsigned long byte_swap_fast( const char *data ) noexcept       { return _byteswap_ulong( *(unsigned long*)data ); }
    template <> AINLINE long long byte_swap_fast( const char *data ) noexcept           { return _byteswap_uint64( *(unsigned long long*)data ); }
    template <> AINLINE unsigned long long byte_swap_fast( const char *data ) noexcept  { return _byteswap_uint64( *(unsigned long long*)data ); }
#elif defined(__linux__)
    template <> AINLINE short byte_swap_fast( const char *data ) noexcept               { return __builtin_bswap16( *(unsigned short*)data ); }
    template <> AINLINE unsigned short byte_swap_fast( const char *data ) noexcept      { return __builtin_bswap16( *(unsigned short*)data ); }
    template <> AINLINE int byte_swap_fast( const char *data ) noexcept                 { return __builtin_bswap32( *(unsigned int*)data ); }
    template <> AINLINE unsigned int byte_swap_fast( const char *data ) noexcept        { return __builtin_bswap32( *(unsigned int*)data ); }
    template <> AINLINE long byte_swap_fast( const char *data ) noexcept                { return __builtin_bswap32( *(unsigned long*)data ); }
    template <> AINLINE unsigned long byte_swap_fast( const char *data ) noexcept       { return __builtin_bswap32( *(unsigned long*)data ); }
    template <> AINLINE long long byte_swap_fast( const char *data ) noexcept           { return __builtin_bswap64( *(unsigned long long*)data ); }
    template <> AINLINE unsigned long long byte_swap_fast( const char *data ) noexcept  { return __builtin_bswap64( *(unsigned long long*)data ); }
#endif

    // Aligned big_endian number type.
    template <typename numberType, bool isPacked = false>
    struct big_endian
    {
        static_assert( std::is_trivially_constructible <numberType>::value );

        // Required to be default for POD handling.
        inline big_endian( void ) = default;
        inline big_endian( const big_endian& ) = default;
        inline big_endian( big_endian&& ) = default;

    private:
        AINLINE void assign_data( const numberType& right ) noexcept(noexcept((numberType&)this->data = right))
        {
            if constexpr ( is_little_endian() )
            {
                (numberType&)data = byte_swap_fast <numberType> ( (const char*)&right );
            }
            else
            {
                (numberType&)data = right;
            }
        }

    public:
        inline big_endian( const numberType& right ) noexcept(noexcept(assign_data(right)))
        {
            assign_data( right );
        }

        inline operator numberType ( void ) const noexcept(std::is_nothrow_move_constructible <numberType>::value)
        {
            if constexpr ( is_little_endian() )
            {
                return byte_swap_fast <numberType> ( this->data );
            }
            else
            {
                return (numberType&)this->data;
            }
        }

        inline big_endian& operator = ( const numberType& right ) noexcept(noexcept(assign_data(right)))
        {
            assign_data( right );

            return *this;
        }

        inline big_endian& operator = ( const big_endian& ) = default;
        inline big_endian& operator = ( big_endian&& ) = default;

    private:
        alignas(typename std::conditional <!isPacked, numberType, char>::type) char data[ sizeof(numberType) ];
    };

    // Aligned little_endian number type.
    template <typename numberType, bool isPacked = false>
    struct little_endian
    {
        static_assert( std::is_trivially_constructible <numberType>::value );

        // Required to be default for POD handling.
        inline little_endian( void ) = default;
        inline little_endian( const little_endian& ) = default;
        inline little_endian( little_endian&& ) = default;

    private:
        AINLINE void assign_data( const numberType& right ) noexcept(noexcept((numberType&)this->data = right))
        {
            if constexpr ( !is_little_endian() )
            {
                (numberType&)data = byte_swap_fast <numberType> ( (const char*)&right );
            }
            else
            {
                (numberType&)data = right;
            }
        }

    public:
        inline little_endian( const numberType& right ) noexcept(noexcept(assign_data(right)))
        {
            assign_data( right );
        }

        inline operator numberType ( void ) const noexcept(noexcept(std::is_nothrow_move_constructible <numberType>::value))
        {
            if constexpr ( !is_little_endian() )
            {
                return byte_swap_fast <numberType> ( this->data );
            }
            else
            {
                return (numberType&)this->data;
            }
        }

        inline little_endian& operator = ( const numberType& right ) noexcept(noexcept(assign_data(right)))
        {
            assign_data( right );

            return *this;
        }

        inline little_endian& operator = ( const little_endian& ) = default;
        inline little_endian& operator = ( little_endian&& ) = default;

    private:
        alignas(typename std::conditional <!isPacked, numberType, char>::type) char data[ sizeof(numberType) ];
    };

    // Shortcuts for the packed endian structs for use in packed serialization structs.
    template <typename structType>
    using p_big_endian = big_endian <structType, true>;

    template <typename structType>
    using p_little_endian = little_endian <structType, true>;
};

}; // namespace eir

// For compatibility with older code.
namespace endian = eir::endian;

#endif //_ENDIAN_COMPAT_HEADER_
