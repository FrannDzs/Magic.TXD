/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/BitVector.h
*  PURPOSE:     Optimized BitVector implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_BITVECTOR_HEADER_
#define _EIR_BITVECTOR_HEADER_

#include "Allocation.h"
#include "Exceptions.h"
#include "MemoryRaw.h"
#include "eirutils.h"
#include "DataUtil.h"

#include <type_traits>
#include <algorithm>
#include <tuple>

#ifdef _DEBUG
#include <assert.h>
#endif //_DEBUG

namespace eir
{

template <MemoryAllocator allocatorType, typename exceptMan = DefaultExceptionManager>
struct BitVector
{
    // Make templates friends of each other.
    template <MemoryAllocator, typename> friend struct BitVector;

    template <typename... allocArgs> requires ( constructible_from <allocatorType, allocArgs...> )
    inline BitVector( eir::constr_with_alloc _, allocArgs&&... args ) : data( std::tuple(), std::tuple <allocArgs&&...> ( std::forward <allocArgs> ( args )... ) )
    {
        this->data.nonalloc_bitfield = 0;
        this->data.bitcount = 0;
    }
    inline BitVector( void ) noexcept requires ( constructible_from <allocatorType> )
        : BitVector( eir::constr_with_alloc::DEFAULT )
    {
        return;
    }

private:
    AINLINE void* _CreateAllocBitbufCopy( const void *srcbitbuf, size_t srcbytesize, size_t dstbytesize )
    {
#ifdef _DEBUG
        assert( dstbytesize > 0 );
#endif //_DEBUG

        void *new_bitbuf = this->data.opt().Allocate( this, dstbytesize, alignof(size_t) );

        if ( new_bitbuf == nullptr )
        {
            exceptMan::throw_oom( dstbytesize );
        }

        size_t copy_cnt = std::min( srcbytesize, dstbytesize );

        if ( copy_cnt > 0 )
        {
            const char *src_bitbuf = (const char*)srcbitbuf;

            FSDataUtil::copy_impl <char> ( src_bitbuf, src_bitbuf + copy_cnt, (char*)new_bitbuf );
        }

        return new_bitbuf;
    }

public:
    inline BitVector( const BitVector& right ) requires ( std::is_copy_constructible <allocatorType>::value )
        : data( std::tuple(), std::tuple <const allocatorType&> ( right.data.opt() ) )
    {
        size_t bitcount = right.data.bitcount;
        
        if ( bitcount <= 32 )
        {
            this->data.nonalloc_bitfield = right.data.nonalloc_bitfield;
        }
        else
        {
            size_t bitbuf_byteSize = GetByteSizeForBitCount( bitcount );

            this->data.alloc_bitfield = _CreateAllocBitbufCopy( right.data.alloc_bitfield, bitbuf_byteSize, bitbuf_byteSize );
        }
        this->data.bitcount = bitcount;
    }
    template <MemoryAllocator otherAllocType, typename... otherVecArgs>
        requires ( nothrow_constructible_from <allocatorType, otherAllocType&&> )
    inline BitVector( BitVector <otherAllocType, otherVecArgs...>&& right ) noexcept
        : data( std::move( right.data.opt() ) )
    {
        size_t bitcount = right.data.bitcount;

        if ( bitcount <= GetBittorBitcount() )
        {
            this->data.nonalloc_bitfield = right.data.nonalloc_bitfield;
        }
        else
        {
            this->data.alloc_bitfield = right.data.alloc_bitfield;
        }
        this->data.bitcount = right.data.bitcount;

        right.data.nonalloc_bitfield = 0;
        right.data.bitcount = 0;
    }
    inline ~BitVector( void )
    {
        size_t bitcount = this->data.bitcount;

        if ( bitcount > GetBittorBitcount() )
        {
            this->data.opt().Free( this, this->data.alloc_bitfield );
        }
    }

    inline BitVector& operator = ( const BitVector& right )
        requires ( std::is_nothrow_move_constructible <allocatorType>::value && std::is_copy_constructible <allocatorType>::value )
    {
        BitVector newvec( right );

        this->~BitVector();

        return *new (this) BitVector( std::move( newvec ) );
    }
    template <MemoryAllocator otherAllocType, typename... otherVecArgs>
        requires ( std::is_nothrow_assignable <allocatorType&, otherAllocType&&>::value )
    inline BitVector& operator = ( BitVector <otherAllocType, otherVecArgs...>&& right ) noexcept
    {
        this->~BitVector();

        return *new (this) BitVector( std::move( right ) );
    }

private:
    AINLINE void ResizeToBitcount( size_t newbitcount )
    {
        size_t oldbitcount = this->data.bitcount;

        if ( oldbitcount == newbitcount )
            return;

        const size_t bittor_bitcount = GetBittorBitcount();

        // Do we have to switch the storage mode?
        bool old_is_nonalloc = ( oldbitcount <= bittor_bitcount );
        bool new_is_nonalloc = ( newbitcount <= bittor_bitcount );

        size_t newReqByteSize;

        if ( new_is_nonalloc == false )
        {
            newReqByteSize = GetByteSizeForBitCount( newbitcount );
        }

        bool has_created_membuf = false;

        if ( old_is_nonalloc == false && new_is_nonalloc == true )
        {
            // Change the buffer to nonalloc.
            void *oldbitbuf = this->data.alloc_bitfield;

            size_t oldbits = *(size_t*)oldbitbuf;

            this->data.nonalloc_bitfield = oldbits;

            // Release the unnecessary memory.
            this->data.opt().Free( this, oldbitbuf );
        }
        else if ( old_is_nonalloc == true && new_is_nonalloc == false )
        {
            // Change the buffer to alloc.
            size_t oldbits = this->data.nonalloc_bitfield;

            void *new_bitbuf = this->data.opt().Allocate( this, newReqByteSize, alignof(size_t) );

            if ( new_bitbuf == nullptr )
            {
                exceptMan::throw_oom( newReqByteSize );
            }

            *(size_t*)new_bitbuf = oldbits;

            this->data.alloc_bitfield = new_bitbuf;

            has_created_membuf = true;
        }

        // If the memory buffer is allocation based and we have not created a new memory buffer, then we have to trim or
        // expand it.
        if ( new_is_nonalloc == false && has_created_membuf == false )
        {
            size_t old_membuf_byteSize = GetByteSizeForBitCount( oldbitcount );

            if ( old_membuf_byteSize > newReqByteSize )
            {
                // Just decrease the bitvector size.
                // It should not fail.
                if constexpr ( ResizeMemoryAllocator <allocatorType> )
                {
                    this->data.opt().Resize( this, this->data.alloc_bitfield, newReqByteSize );
                }
                else if constexpr ( ReallocMemoryAllocator <allocatorType> )
                {
                    // We are allowed to do this because bits are simple.
                    this->data.alloc_bitfield = this->data.opt().Realloc( this, this->data.alloc_bitfield, newReqByteSize, alignof(size_t) );
                }
                // else we just skip doing it; should not waste too much memory, huh?
                // using non-resize memory allocators is not recommended anyway.
            }
            else
            {
                // First try expanding with the resize-buf method.
                bool hasExpanded = false;

                void *bitbuf = this->data.alloc_bitfield;

                if constexpr ( ResizeMemoryAllocator <allocatorType> )
                {
                    hasExpanded = this->data.opt().Resize( this, bitbuf, newReqByteSize );
                }
                else if constexpr ( ReallocMemoryAllocator <allocatorType> )
                {
                    bitbuf = this->data.opt().Realloc( this, bitbuf, newReqByteSize, alignof(size_t) );

                    if ( bitbuf == nullptr )
                    {
                        exceptMan::throw_oom( newReqByteSize );
                    }

                    this->data.alloc_bitfield = bitbuf;

                    hasExpanded = true;
                }

                if ( hasExpanded == false )
                {
                    // Try creating a new memory array and copying the old contents into it.
                    void *newbuf = _CreateAllocBitbufCopy( bitbuf, old_membuf_byteSize, newReqByteSize );

                    this->data.opt().Free( this, bitbuf );

                    this->data.alloc_bitfield = newbuf;
                }
            }
        }

        // If we have increased the amount of bits, then we have to clear the new and additional bits to 0.
        if ( newbitcount > oldbitcount )
        {
            void *target_buf;

            if ( new_is_nonalloc )
            {
                target_buf = &this->data.nonalloc_bitfield;
            }
            else
            {
                target_buf = this->data.alloc_bitfield;
            }

            size_t dstbitcount = ( newbitcount - oldbitcount );

            eir::setBits( target_buf, false, oldbitcount, dstbitcount );
        }

        this->data.bitcount = newbitcount;
    }

    static AINLINE void AssignBitIntoBitfield( size_t& bitfield, size_t localbitidx, bool value )
    {
        size_t targetbitfield = ( (size_t)1 << localbitidx );

        if ( value )
        {
            bitfield |= targetbitfield;
        }
        else
        {
            bitfield &= ~targetbitfield;
        }
    }

public:
    inline void SetBit( size_t bitidx, bool value )
    {
        size_t curbitcount = this->data.bitcount;

        if ( curbitcount <= bitidx )
        {
            if ( value == false )
            {
                // Nothing to do.
                return;
            }
            else
            {
                size_t newbitcount = ( bitidx + 1 );

                ResizeToBitcount( bitidx + 1 );

                curbitcount = newbitcount;
            }
        }

        if ( curbitcount <= GetBittorBitcount() )
        {
            AssignBitIntoBitfield( this->data.nonalloc_bitfield, bitidx, value );
        }
        else
        {
            size_t bittor_bitsize = GetBittorBitcount();

            size_t bittor_idx = ( bitidx / bittor_bitsize );
            size_t localbitidx = ( bitidx % bittor_bitsize );

            size_t *bittor_buf = (size_t*)this->data.alloc_bitfield;

            size_t& bittor = *( bittor_buf + bittor_idx );

            AssignBitIntoBitfield( bittor, localbitidx, value );
        }

        if ( value == false )
        {
            // Find the highest bit index with a value of 1 and shrink the vector to it.
            size_t highest_bit_enable_idx;
            bool has_enable_bit = false;

            if ( curbitcount > 0 )
            {
                size_t iter = curbitcount - 1;
            
                while ( true )
                {
                    if ( GetBit( iter ) == true )
                    {
                        highest_bit_enable_idx = iter;
                        has_enable_bit = true;
                        break;
                    }

                    if ( iter == 0 )
                        break;

                    iter--;
                }
            }

            size_t shrink_to_size = ( has_enable_bit ? highest_bit_enable_idx + 1 : 0 );

            if ( shrink_to_size < curbitcount )
            {
                ResizeToBitcount( shrink_to_size );
            }
        }
    }

    inline bool GetBit( size_t bitidx ) const
    {
        size_t bitcount = this->data.bitcount;

        if ( bitidx >= bitcount )
        {
            return false;
        }

        bool bitval;

        if ( bitcount <= GetBittorBitcount() )
        {
            bitval = (bool)( ( this->data.nonalloc_bitfield >> bitidx ) & 1u );
        }
        else
        {
            size_t bittor_bitsize = GetBittorBitcount();

            size_t bittor_idx = ( bitidx / bittor_bitsize );
            size_t localbitidx = ( bitidx % bittor_bitsize );

            const size_t *bittor_buf = (size_t*)this->data.alloc_bitfield;

            const size_t& bittor = *( bittor_buf + bittor_idx );

            bitval = (bool)( ( bittor >> localbitidx ) & 1u );
        }

        return bitval;
    }

    inline size_t GetBitCount( void ) const noexcept
    {
        return this->data.bitcount;
    }

    inline void Clear( void )
    {
        // Just screw all them bits!
        ResizeToBitcount( 0 );
    }

private:
    static AINLINE size_t GetBittorBitcount( void ) noexcept
    {
        return ( sizeof(size_t) * 8u );
    }

    static AINLINE size_t GetByteSizeForBitCount( size_t bitcount ) noexcept
    {
        return CEIL_DIV( bitcount, GetBittorBitcount() ) * sizeof(size_t);
    }

    struct fields
    {
        union
        {
            size_t nonalloc_bitfield;   // used if bitcount <= 32
            void *alloc_bitfield;       // used if bitcount > 32
        };
        size_t bitcount;
    };
    empty_opt <allocatorType, fields> data;
};

} // namespace eir

#endif //_EIR_BITVECTOR_HEADER_