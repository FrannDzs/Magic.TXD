/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/EmptyVector.h
*  PURPOSE:     Vector implementation with std::empty values (optimization)
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRREPO_EMPTY_VECTOR_HEADER_
#define _EIRREPO_EMPTY_VECTOR_HEADER_

#include "Exceptions.h"

#include <type_traits>
#include <concepts>

namespace eir
{

/*
* This vector type was made as a template use-case where code is made to use the regular Eir Vector struct
* but the values are what the C++ standard considers "empty". In such a case we can discard the unique-address
* requirement and just offer a vector type that returns the same object for all indices that it created.
* Not all methods from the regular Eir Vector type are taken over, most importantly ones that do not make
* sense due to either complexity of implementation or feature-set that should be optimized away by the user.
*/

template <typename structType, typename exceptMan = DefaultExceptionManager>
    requires ( std::is_empty <structType>::value )
struct EmptyVector
{
    inline EmptyVector( void ) noexcept
    {
        this->item_count = 0;
    }
    inline EmptyVector( const EmptyVector& right ) noexcept : _empty_value( right._empty_value ), item_count( right.item_count )
    {
        return;
    }
    inline EmptyVector( EmptyVector&& right ) noexcept : _empty_value( std::move( right._empty_value ) ), item_count( right.item_count )
    {
        right.item_count = 0;
    }

    inline ~EmptyVector( void )
    {
        return;
    }

    inline EmptyVector& operator = ( const EmptyVector& right ) = default;
    inline EmptyVector& operator = ( EmptyVector& right ) noexcept
    {
        this->_empty_value = std::move( right._empty_value );
        this->item_count = right.item_count;

        right.item_count = 0;
        return *this;
    }

    template <std::convertible_to <structType> valueType>
    inline void AddToBack( valueType&& value )
    {
        // Just to keep the user's expectations alive.
        // This will be discarded because it is empty.
        {
            structType empty_value( std::forward <valueType> ( value ) );
            (void)empty_value;
        }

        this->item_count++;
    }

    inline void RemoveFromBack( void ) noexcept
    {
        size_t item_count = this->item_count;

        if ( item_count == 0 )
            return;

        this->item_count = ( item_count - 1 );
    }

    inline void Clear( void ) noexcept
    {
        this->item_count = 0;
    }

    inline void Resize( size_t cnt ) noexcept
    {
        this->item_count = cnt;
    }

    inline size_t GetCount( void ) const noexcept
    {
        return this->item_count;
    }

    inline structType& GetBack( void )
    {
        if ( this->item_count == 0 )
        {
            exceptMan::throw_not_found();
        }

        return this->_empty_value;
    }
    inline const structType& GetBack( void ) const
    {
        if ( this->item_count == 0 )
        {
            exceptMan::throw_not_found();
        }

        return this->_empty_value;
    }

    inline structType& operator [] ( size_t idx )
    {
        if ( idx >= this->item_count )
        {
            exceptMan::throw_index_out_of_bounds();
        }

        return this->_empty_value;
    }
    inline const structType& operator [] ( size_t idx ) const
    {
        if ( idx >= this->item_count )
        {
            exceptMan::throw_index_out_of_bounds();
        }

        return this->_empty_value;
    }

private:
    template <typename vecType> requires ( std::same_as <vecType, EmptyVector> || std::same_as <vecType, const EmptyVector> )
    struct std_iterator
    {
        inline std_iterator( vecType *vec ) noexcept : vec( vec )
        {
            return;
        }
        inline std_iterator( const std_iterator& ) = default;
        inline std_iterator( std_iterator&& ) = default;

        inline std_iterator& operator ++ ( void )
        {
            this->idx++;
            return *this;
        }

        inline structType& operator * ( void ) const
        {
            return vec->_empty_value;
        }

        size_t idx = 0;
        vecType *vec;
    };

    template <typename vecType> requires ( std::same_as <vecType, EmptyVector> || std::same_as <vecType, const EmptyVector> )
    struct std_iterator_end
    {
        inline bool operator == ( const std_iterator <vecType>& right ) const
        {
            vecType *vec = right.vec;

            return ( right.idx == vec->item_count );
        }
    };

public:
    inline std_iterator <EmptyVector> begin( void )
    {
        return std_iterator( this );
    }
    inline std_iterator <const EmptyVector> begin( void ) const
    {
        return std_iterator( this );
    }

    inline std_iterator_end <EmptyVector> end( void )
    {
        return std_iterator_end <EmptyVector> ();
    }
    inline std_iterator_end <const EmptyVector> end( void ) const
    {
        return std_iterator_end <const EmptyVector> ();
    }

private:
    [[no_unique_address]] structType _empty_value;
    size_t item_count;
};

} // namespace eir

#endif //_EIRREPO_EMPTY_VECTOR_HEADER_