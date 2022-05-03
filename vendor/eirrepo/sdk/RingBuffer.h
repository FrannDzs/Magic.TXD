/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/RingBuffer.h
*  PURPOSE:     Ring-buffer handling helpers
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _RING_BUFFER_HANDLERS_
#define _RING_BUFFER_HANDLERS_

#include "Exceptions.h"
#include "eirutils.h"

#include <algorithm>

namespace eir
{

template <typename numberType, typename exceptMan = eir::DefaultExceptionManager>
struct RingBufferProcessor
{
    AINLINE RingBufferProcessor( numberType iter, numberType end_iter, numberType size_plus_one )
        : iter( std::move( iter ) ), end_iter( std::move( end_iter ) ), size_plus_one( std::move( size_plus_one ) )
    {
        return;
    }
    AINLINE RingBufferProcessor( const RingBufferProcessor& ) = default;
    AINLINE RingBufferProcessor( RingBufferProcessor&& ) = default;

    AINLINE ~RingBufferProcessor( void )
    {
        return;
    }

    AINLINE RingBufferProcessor& operator = ( const RingBufferProcessor& ) = default;
    AINLINE RingBufferProcessor& operator = ( RingBufferProcessor&& ) = default;

    AINLINE numberType GetAvailableBytes( void ) const noexcept
    {
        numberType iter = this->iter;
        numberType end_iter = this->end_iter;

        if ( iter <= end_iter )
        {
            return ( end_iter - iter );
        }
        else
        {
            numberType first_left_write_count = ( this->size_plus_one - iter );
            numberType second_left_write_count = ( end_iter );

            return ( first_left_write_count + second_left_write_count );
        }
    }

    template <typename callbackType>
    AINLINE void PerformUpdate( numberType cnt, const callbackType& cb )
    {
        numberType iter = this->iter;
        numberType end_iter = this->end_iter;
        numberType size_plus_one = this->size_plus_one;

        if ( GetAvailableBytes() < cnt )
        {
            exceptMan::throw_internal_error();
        }

        if ( iter <= end_iter )
        {
            cb( iter, 0, cnt );

            iter += cnt;

            if ( iter == size_plus_one )
            {
                iter = 0;
            }
        }
        else
        {
            numberType first_left_write_count = ( size_plus_one - iter );
            numberType second_left_write_count = ( end_iter );

            numberType left_to_write = cnt;

            numberType first_write_count = std::min( cnt, first_left_write_count );

            if ( first_write_count > 0 )
            {
                cb( iter, 0, first_write_count );
                
                iter += first_write_count;

                if ( iter == size_plus_one )
                {
                    iter = 0;
                }
            }

            if ( left_to_write > first_write_count )
            {
                left_to_write -= first_write_count;

                std::uint32_t second_write_count = std::min( left_to_write, second_left_write_count );

                if ( second_write_count > 0 )
                {
                    cb( iter, first_write_count, second_write_count );

                    iter += second_write_count;
                }
            }
        }

        this->iter = iter;
    }

    AINLINE void Increment( numberType cnt )
    {
        this->iter = ( ( this->iter + cnt ) % this->size_plus_one );
    }

private:
    template <typename numberType>
    AINLINE static numberType minus_modulus( numberType value, numberType subtractBy, numberType modulus )
    {
        numberType real_subtractBy = ( subtractBy % modulus );

        if ( real_subtractBy > value )
        {
            numberType neg_value = ( real_subtractBy - value );

            value = ( modulus - neg_value );
        }
        else
        {
            value -= real_subtractBy;
        }

        return value;
    }

public:
    AINLINE void Decrement( numberType cnt )
    {
        this->iter = minus_modulus( this->iter, cnt, this->size_plus_one );
    }

    AINLINE void IncrementEnd( numberType cnt )
    {
        this->end_iter = ( ( this->end_iter + cnt ) % this->size_plus_one );
    }

    AINLINE void DecrementEnd( numberType cnt )
    {
        this->end_iter = minus_modulus( this->end_iter, cnt, this->size_plus_one );
    }

    AINLINE numberType GetOffset( numberType get_iter ) const
    {
        numberType iter = this->iter;
        numberType end_iter = this->end_iter;
        numberType size_plus_one = this->size_plus_one;

        if ( iter >= end_iter )
        {
            if ( get_iter >= iter && get_iter < size_plus_one )
            {
                return ( get_iter - iter );
            }

            if ( get_iter < end_iter )
            {
                return ( ( size_plus_one - iter ) + get_iter );
            }

            // Index out of bounds.
            exceptMan::throw_index_out_of_bounds();
        }
        else
        {
            if ( get_iter < iter || get_iter >= end_iter )
            {
                // Index out of bounds.
                exceptMan::throw_index_out_of_bounds();
            }

            return ( get_iter - iter );
        }
    }

    numberType iter;
    numberType end_iter;
    numberType size_plus_one;
};

} //namespace eir

#endif //_RING_BUFFER_HANDLERS_