/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Vector.h
*  PURPOSE:     Optimized Vector implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// Once we had the native heap allocator done, we realized that we had quite some cool
// memory semantics that the standard memory allocators (even from OS side) do not support.
// So we decided to roll out our own SDK classes to optimize for those facts.
// It is also a good idea to consolidate the API; do not trust the STL too much!

#ifndef _EIR_VECTOR_HEADER_
#define _EIR_VECTOR_HEADER_

#include "Exceptions.h"
#include "eirutils.h"
#include "MacroUtils.h"
#include "MetaHelpers.h"

#include <algorithm>
#include <type_traits>
#include <assert.h>
#include <array>

namespace eir
{

// Evaluates to true if this value type can be used inside of a vector for insertion along with
// many such objects in arrays.
template <typename insertValueType, typename vectorStructType>
concept vector_multi_insert_compatible =
    ( ( eir::nothrow_move_assignable_with <insertValueType, vectorStructType> &&
        eir::nothrow_constructible_from <vectorStructType, insertValueType&&> ) ||
      ( eir::constructible_from <vectorStructType, const insertValueType&> &&
        eir::copy_assignable_with <vectorStructType, insertValueType> )
    );

// Helper for returning the memory size that a vector/array would need so that all items can be properly
// indexed while taking possible alignment rules into account.
// I guess since a lot of code is oblivious to alignment rules that is why MSVC made alignas play into
// the sizeof.
template <typename structType>
AINLINE constexpr size_t VectorMemSize( size_t itemCnt ) noexcept
{
    if ( itemCnt == 0 )
        return 0;

    size_t last_item_idx = ( itemCnt - 1 );

    return (size_t)( (structType*)0 + last_item_idx ) + sizeof(structType);
}

// Implementors comment: cannot use this concept because it requires structType to be a complete
// type at point of query. Imagine a vector inside class X that contains class X. That case should
// still work but it would not if we used this concept.
template <typename structType>
concept Vectorable = std::constructible_from <structType, structType&&>;

template <typename structType, MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager>
struct Vector
{
    // Make sure that templates are friends of each-other.
    template <typename, MemoryAllocator, typename> friend struct Vector;

    inline Vector( void ) noexcept
    {
        this->data.data_entries = nullptr;
        this->data.data_count = 0;
    }

private:
    template <std::convertible_to <structType> argStructType>
    static AINLINE structType* make_data_move_or_copy( Vector *refPtr, argStructType *right_data, size_t right_count )
    {
        size_t memRequestSize = VectorMemSize <structType> ( right_count );

        structType *data_entries = (structType*)refPtr->data.opt().Allocate( refPtr, memRequestSize, alignof(structType) );

        if ( data_entries == nullptr )
        {
            exceptMan::throw_oom( memRequestSize );
        }

        size_t cur_idx = 0;

        try
        {
            while ( cur_idx < right_count )
            {
                new ( data_entries + cur_idx ) structType( std::move( right_data[cur_idx] ) );

                cur_idx++;
            }
        }
        catch( ... )
        {
            if constexpr ( std::is_nothrow_constructible <structType, argStructType&&>::value == false )
            {
                // Destroy all constructed items again.
                while ( cur_idx > 0 )
                {
                    cur_idx--;

                    ( data_entries + cur_idx )->~structType();
                }

                refPtr->data.opt().Free( refPtr, data_entries );
            }

            throw;
        }

        return data_entries;
    }

    template <std::convertible_to <structType> argStructType>
    AINLINE void initialize_with( argStructType *data, size_t data_count )
    {
        structType *our_data_entries = nullptr;

        if ( data_count > 0 )
        {
            our_data_entries = make_data_move_or_copy( this, data, data_count );
        }

        this->data.data_entries = our_data_entries;
        this->data.data_count = data_count;
    }

public:
    template <typename... Args> requires ( std::copy_constructible <structType> && std::constructible_from <allocatorType, Args...> )
    inline Vector( const structType *data, size_t data_count, Args&&... allocArgs ) : data( std::tuple( fields() ), std::tuple( std::forward <Args> ( allocArgs )... ) )
    {
        initialize_with( data, data_count );
    }

    template <typename... Args> requires ( std::constructible_from <allocatorType, Args...> )
    inline Vector( constr_with_alloc, Args&&... allocArgs ) : data( std::tuple( fields() ), std::tuple( std::forward <Args> ( allocArgs )... ) )
    {
        this->data.data_entries = nullptr;
        this->data.data_count = 0;
    }

    template <std::convertible_to <structType> otherStructType, size_t N> requires ( std::convertible_to <otherStructType, structType> )
    inline Vector( std::array <otherStructType, N>&& entries )
    {
        initialize_with( entries.data(), N );
    }

    inline Vector( const Vector& right )
        : data( right.data )
    {
        size_t right_count = right.data.data_count;
        const structType *right_data = right.data.data_entries;

        initialize_with( right_data, right_count );
    }

    template <typename... otherVectorArgs> requires ( std::copy_constructible <structType> )
    inline Vector( const Vector <structType, otherVectorArgs...>& right )
    {
        size_t right_count = right.data.data_count;
        const structType *right_data = right.data.data_entries;

        initialize_with( right_data, right_count );
    }

    template <MemoryAllocator otherAllocType, typename... otherVectorArgs>
        requires ( nothrow_constructible_from <allocatorType, otherAllocType&&> )
    inline Vector( Vector <structType, otherAllocType, otherVectorArgs...>&& right ) noexcept
        : data( std::move( right.data.opt() ) )
    {
        this->data.data_entries = right.data.data_entries;
        this->data.data_count = right.data.data_count;

        right.data.data_entries = nullptr;
        right.data.data_count = 0;
    }

private:
    static AINLINE void release_data( Vector *refPtr, structType *data_entries, size_t data_count ) noexcept
    {
        if ( data_entries )
        {
            for ( size_t n = 0; n < data_count; n++ )
            {
                data_entries[n].~structType();
            }

            refPtr->data.opt().Free( refPtr, data_entries );
        }
    }

public:
    inline ~Vector( void )
    {
        release_data( this, this->data.data_entries, this->data.data_count );
    }

    inline Vector& operator = ( const Vector& right )
    {
        size_t right_count = right.data.data_count;
        structType *new_data_entries = nullptr;

        if ( right_count > 0 )
        {
            new_data_entries = make_data_move_or_copy <const structType> ( this, right.data.data_entries, right_count );
        }

        // Swap old with new.
        release_data( this, this->data.data_entries, this->data.data_count );

        this->data.data_entries = new_data_entries;
        this->data.data_count = right_count;

        return *this;
    }

    template <typename... otherVectorArgs> requires ( std::copy_constructible <structType> )
    inline Vector& operator = ( const Vector <structType, otherVectorArgs...>& right )
    {
        size_t right_count = right.data.data_count;
        structType *new_data_entries = nullptr;

        if ( right_count > 0 )
        {
            new_data_entries = make_data_move_or_copy <const structType> ( this, right.data.data_entries, right_count );
        }

        // Swap old with new.
        release_data( this, this->data.data_entries, this->data.data_count );

        this->data.data_entries = new_data_entries;
        this->data.data_count = right_count;

        return *this;
    }

    template <MemoryAllocator otherAllocType, typename... otherVectorArgs>
        requires ( std::is_nothrow_assignable <allocatorType&, otherAllocType&&>::value )
    inline Vector& operator = ( Vector <structType, otherAllocType, otherVectorArgs...>&& right ) noexcept
    {
        // Release our previous data.
        release_data( this, this->data.data_entries, this->data.data_count );

        // Move over allocator, if needed.
        this->data.opt() = std::move( right.data.opt() );

        this->data.data_entries = right.data.data_entries;
        this->data.data_count = right.data.data_count;

        right.data.data_entries = nullptr;
        right.data.data_count = 0;

        return *this;
    }

private:
    static constexpr bool is_vector_data_nothrow_arrangeable =
        std::is_nothrow_move_assignable <structType>::value &&
        std::is_nothrow_move_constructible <structType>::value;

    static constexpr bool is_vector_realloc_compatible =
        std::is_trivially_move_assignable <structType>::value &&
        std::is_trivially_copy_assignable <structType>::value;

    struct backing_storage_allocator
    {
        inline backing_storage_allocator(
            Vector *vec,
            structType *oldarr, size_t oldarrsize,
            size_t byte_size_of_target
        ) : vec( vec ), oldarr( oldarr ), oldarrsize( oldarrsize )
        {
            bool storage_grow_success = false;

            void *backing_storage = nullptr;
            bool is_storage_new;

            if constexpr ( is_vector_data_nothrow_arrangeable )
            {
                if ( oldarrsize > 0 )
                {
                    if constexpr ( ResizeMemoryAllocator <allocatorType> )
                    {
                        storage_grow_success = vec->data.opt().Resize( vec, oldarr, byte_size_of_target );

                        if ( storage_grow_success )
                        {
                            backing_storage = oldarr;
                            is_storage_new = false;
                        }
                    }
                    else if constexpr ( is_vector_realloc_compatible && ReallocMemoryAllocator <allocatorType> )
                    {
                        if ( storage_grow_success == false )
                        {
                            backing_storage = vec->data.opt().Realloc( vec, oldarr, byte_size_of_target, alignof(structType) );

                            if ( backing_storage == nullptr )
                            {
                                exceptMan::throw_oom( byte_size_of_target );
                            }

                            storage_grow_success = true;
                            if ( oldarr != backing_storage )
                            {
                                oldarr = (structType*)backing_storage;
                                this->oldarr = oldarr;
                                vec->data.data_entries = oldarr;
                            }
                            is_storage_new = false;
                        }
                    }
                }
            }

            if ( storage_grow_success == false )
            {
                backing_storage = vec->data.opt().Allocate( vec, byte_size_of_target, alignof(structType) );

                if ( backing_storage == nullptr )
                {
                    exceptMan::throw_oom( byte_size_of_target );
                }

                is_storage_new = true;
            }

            this->backing_storage = backing_storage;
            this->is_storage_new = is_storage_new;
        }

        inline backing_storage_allocator( const backing_storage_allocator& ) = delete;
        inline backing_storage_allocator( backing_storage_allocator&& ) = delete;

        inline ~backing_storage_allocator( void )
        {
            if ( this->tripwire == false )
                return;

            Vector *vec = this->vec;
            size_t oldarrsize = this->oldarrsize;
            void *backing_storage = this->backing_storage;
            bool is_storage_new = this->is_storage_new;

            if ( is_storage_new )
            {
                // Do not forget to release the memory.
                vec->data.opt().Free( vec, backing_storage );
            }
            else
            {
                if constexpr ( ResizeMemoryAllocator <allocatorType> )
                {
                    // Need to shrink-back the memory.
                    size_t bytes_oldarrsize = VectorMemSize <structType> ( oldarrsize );

#ifdef _DEBUG
                    bool shrink_success =
#endif //_DEBUG
                        vec->data.opt().Resize( vec, backing_storage, bytes_oldarrsize );

#ifdef _DEBUG
                    FATAL_ASSERT( shrink_success == true );
#endif //_DEBUG
                }
                else if constexpr ( is_vector_realloc_compatible && ReallocMemoryAllocator <allocatorType> )
                {
                    size_t bytes_oldarrsize = VectorMemSize <structType> ( oldarrsize );

                    vec->data.data_entries =
                        (structType*)vec->data.opt().Realloc( vec, backing_storage, bytes_oldarrsize, alignof(structType) );
                }
            }
        }

        inline backing_storage_allocator& operator = ( const backing_storage_allocator& ) = delete;
        inline backing_storage_allocator& operator = ( backing_storage_allocator&& ) = delete;

        inline void Defuse( void )
        {
            this->tripwire = false;
        }

        inline void* GetBackingStorage( void ) noexcept
        {
            return this->backing_storage;
        }

        inline bool IsBackingStorageNew( void ) const noexcept
        {
            return this->is_storage_new;
        }

        inline constexpr bool CouldInvalidateOldarr( void ) const noexcept
        {
            return ( is_vector_realloc_compatible && ReallocMemoryAllocator <allocatorType> );
        }

        bool tripwire = true;
        Vector *vec;
    private:
        bool is_storage_new;
        void *backing_storage;

    public:
        structType *oldarr;
        size_t oldarrsize;
    };

    // An arranger that just establishes a clone of the source vector.
    struct clone_backing_storage_arranger : public backing_storage_allocator
    {
        inline clone_backing_storage_arranger(
            Vector *vec,
            structType *oldarr, size_t oldarrsize,
            size_t byte_size_of_target
        ) : backing_storage_allocator( vec, oldarr, oldarrsize, byte_size_of_target )
        {
            void *backing_storage = this->GetBackingStorage();
            bool is_storage_new = this->IsBackingStorageNew();

            if ( is_storage_new )
            {
                size_t cidx = 0;

                try
                {
                    while ( cidx < oldarrsize )
                    {
                        structType *source_obj = ( oldarr + cidx );
                        void *target_ptr = (structType*)backing_storage + cidx;

                        if constexpr ( is_vector_data_nothrow_arrangeable )
                        {
                            new (target_ptr) structType( std::move( *source_obj ) );
                        }
                        else
                        {
                            new (target_ptr) structType( *(const structType*)source_obj );
                        }

                        cidx++;
                    }
                }
                catch( ... )
                {
                    if constexpr ( is_vector_data_nothrow_arrangeable == false )
                    {
                        while ( cidx > 0 )
                        {
                            cidx--;

                            structType *target_obj = (structType*)backing_storage + cidx;

                            target_obj->~structType();
                        }
                    }
                    throw;
                }
            }
        }

        inline ~clone_backing_storage_arranger( void )
        {
            if ( this->tripwire == false )
                return;

            void *backing_storage = this->GetBackingStorage();
            bool is_storage_new = this->IsBackingStorageNew();

            structType *oldarr = this->oldarr;
            size_t oldarrsize = this->oldarrsize;

            if ( is_storage_new )
            {
                size_t cidx = oldarrsize;

                while ( cidx > 0 )
                {
                    cidx--;

                    structType *target_obj = (structType*)backing_storage + cidx;

                    if constexpr ( is_vector_data_nothrow_arrangeable )
                    {
                        structType *source_obj = ( oldarr + cidx );

                        *source_obj = std::move( *target_obj );
                    }

                    target_obj->~structType();
                }
            }
        }
    };

public:
    // Simple modification methods.
    template <std::convertible_to <structType> addStructType> requires ( Vectorable <structType> )
    inline void AddToBack( addStructType&& item )
    {
        size_t oldCount = ( this->data.data_count );
        size_t newCount = ( oldCount + 1 );
        size_t newRequiredSize = VectorMemSize <structType> ( newCount );

        clone_backing_storage_arranger bs_arrange(
            this,
            this->data.data_entries, oldCount,
            newRequiredSize
        );

        void *backing_storage = bs_arrange.GetBackingStorage();

        void *target_ptr = (structType*)backing_storage + oldCount;

        // This operation could throw an exception.
        new (target_ptr) structType( std::forward <addStructType> ( item ) );

        // No more exceptions possible from here on out.
        bs_arrange.Defuse();

        if ( bs_arrange.IsBackingStorageNew() )
        {
            release_data( this, bs_arrange.oldarr, oldCount );
        }

        this->data.data_entries = (structType*)backing_storage;
        this->data.data_count = newCount;
    }

private:
    struct insert_backing_storage_arranger : public backing_storage_allocator
    {
        inline insert_backing_storage_arranger(
            Vector *vec,
            structType *oldarr, size_t oldarrsize,
            size_t inspos, size_t size_of_target, size_t move_back_to, size_t move_back_cnt,
            size_t byte_size_of_target
        ) : backing_storage_allocator( vec, oldarr, oldarrsize, byte_size_of_target ), inspos( inspos ), move_back_to( move_back_to ), move_back_cnt( move_back_cnt )
        {
            // Might have changed in the arranger so let's re-read it.
            oldarr = this->oldarr;

            void *backing_storage = this->GetBackingStorage();
            bool is_storage_new = this->IsBackingStorageNew();

            size_t cidx = ( is_storage_new ? 0 : inspos );

            try
            {
                while ( cidx < oldarrsize )
                {
                    size_t source_idx;
                    size_t target_idx;

                    if ( cidx < inspos )
                    {
                        source_idx = cidx;
                        target_idx = cidx;
                    }
                    else
                    {
                        source_idx = inspos + ( move_back_cnt - 1 ) - ( cidx - inspos );
                        target_idx = ( move_back_to + ( move_back_cnt - 1 ) - ( cidx - inspos ) );
                    }

                    void *target_ptr = (structType*)backing_storage + target_idx;
                    structType *source_obj = ( oldarr + source_idx );

                    if constexpr ( is_vector_data_nothrow_arrangeable )
                    {
                        if ( is_storage_new == false && target_idx < oldarrsize )
                        {
                            structType *target_obj = (structType*)target_ptr;

                            *target_obj = std::move( *source_obj );
                        }
                        else
                        {
                            new (target_ptr) structType( std::move( *source_obj ) );
                        }
                    }
                    else
                    {
                        new (target_ptr) structType( *(const structType*)source_obj );
                    }

                    cidx++;
                }
            }
            catch( ... )
            {
                if constexpr ( is_vector_data_nothrow_arrangeable == false )
                {
                    while ( cidx > 0 )
                    {
                        cidx--;

                        size_t target_idx;

                        if ( cidx < inspos )
                        {
                            target_idx = cidx;
                        }
                        else
                        {
                            target_idx = ( move_back_to + ( move_back_cnt - 1 ) - ( cidx - inspos ) );
                        }

                        structType *target_obj = (structType*)backing_storage + target_idx;

                        target_obj->~structType();
                    }
                }
                throw;
            }
        }

        inline insert_backing_storage_arranger( const insert_backing_storage_arranger& ) = delete;
        inline insert_backing_storage_arranger( insert_backing_storage_arranger&& ) = delete;

        inline ~insert_backing_storage_arranger( void )
        {
            if ( this->tripwire == false )
                return;

            Vector *vec = this->vec;

            // Move any colliding items into their origin prior to nothrow-movement.
            size_t move_back_iter = 0;
            size_t move_back_cnt = this->move_back_cnt;
            size_t move_back_to = this->move_back_to;
            size_t inspos = this->inspos;
            structType *oldarr = this->oldarr;
            size_t oldarrsize = this->oldarrsize;

            void *backing_storage = this->GetBackingStorage();
            bool is_storage_new = this->IsBackingStorageNew();

            while ( move_back_iter < move_back_cnt )
            {
                // Now we move from target to source, basically reversing the operation.

                size_t target_idx = ( move_back_to + move_back_iter );
                size_t source_idx = ( inspos + move_back_iter );

                structType *target_obj = (structType*)backing_storage + target_idx;
                structType *source_obj = ( oldarr + source_idx );   // valid due to the guarantee of intersection.

                if constexpr ( is_vector_data_nothrow_arrangeable )
                {
                    // We don't have to move back if the original data
                    // was not touched anyway.
                    *source_obj = std::move( *target_obj );
                }

                if ( is_storage_new == true || target_idx >= oldarrsize )
                {
                    target_obj->~structType();
                }

                move_back_iter++;
            }

            // If the backing storage is new, then we also have to move back any items inside of it while
            // destroying any locations from the new storage.
            if ( is_storage_new )
            {
                size_t n = ( oldarrsize - move_back_cnt );

                while ( n > 0 )
                {
                    n--;

                    structType *target_obj = (structType*)backing_storage + n;
                    structType *source_obj = &oldarr[ n ];

                    if constexpr ( is_vector_data_nothrow_arrangeable )
                    {
                        // Just restore if the operation does not throw any
                        // exceptions in which case we have to restore.
                        *source_obj = std::move( *target_obj );
                    }

                    target_obj->~structType();
                }
            }
        }

        size_t move_back_to, move_back_cnt, inspos;
    };

public:
    template <std::convertible_to <structType> insertValueType>
    inline void InsertMulti( size_t inspos, insertValueType *values, size_t vcnt )
        requires std::is_default_constructible <structType>::value &&
            ( is_vector_data_nothrow_arrangeable || std::is_copy_constructible <structType>::value ) &&
            vector_multi_insert_compatible <insertValueType, structType>
    {
        if ( vcnt == 0 )
            return;

        size_t oldsize = this->data.data_count;
        size_t move_back_count;

        if ( inspos < oldsize )
        {
            move_back_count = ( oldsize - inspos );
        }
        else
        {
            move_back_count = 0;
        }

        size_t move_back_to = inspos + vcnt;
        size_t size_of_target = ( move_back_to + move_back_count );

        insert_backing_storage_arranger bs_arrange(
            this,
            this->data.data_entries, oldsize,
            inspos, size_of_target, move_back_to, move_back_count,
            VectorMemSize <structType> ( size_of_target )
        );

        // TODO: if the entire following operations are nothrow then we can release any back-up memory at this point.

        void *backing_storage = bs_arrange.GetBackingStorage();
        bool is_backing_storage_new = bs_arrange.IsBackingStorageNew();

        // Default-construct any entries that are not yet constructed and before the inspos.
        size_t def_constr_cnt;
        size_t def_constr_idx;

        auto _cleanup_default_constr = [&]( void ) noexcept
        {
            if constexpr ( std::is_nothrow_default_constructible <structType>::value == false )
            {
                while ( def_constr_idx > 0 )
                {
                    def_constr_idx--;

                    structType *target_obj = (structType*)backing_storage + oldsize + def_constr_idx;

                    target_obj->~structType();
                }
            }
        };

        if ( inspos > oldsize )
        {
            def_constr_cnt = ( inspos - oldsize );
            def_constr_idx = 0;

            try
            {
                while ( def_constr_idx < def_constr_cnt )
                {
                    void *target_ptr = (structType*)backing_storage + oldsize + def_constr_idx;

                    new (target_ptr) structType();

                    def_constr_idx++;
                }
            }
            catch( ... )
            {
                _cleanup_default_constr();
                throw;
            }
        }

        constexpr bool allow_move_operation_for_sure =
            eir::nothrow_move_assignable_with <structType, insertValueType> &&
            eir::nothrow_constructible_from <structType, insertValueType>;

        // If a move is performed then it has to be done "atomically"/in one swipe.
        bool allow_move_operation =
            ( ( inspos >= oldsize || is_backing_storage_new == true || eir::nothrow_move_assignable_with <structType, insertValueType> ) &&
              ( inspos + vcnt <= oldsize || eir::nothrow_constructible_from <structType, insertValueType&&> ) ) ||
            ( vcnt == 1 ); // works because this is the last operation.

        try
        {
            // Put the data into the insertion slice.
            size_t insidx = 0;

            try
            {
                while ( insidx < vcnt )
                {
                    size_t target_idx = ( inspos + insidx );

                    insertValueType *source_obj = &values[ insidx ];
                    void *target_ptr = (structType*)backing_storage + target_idx;

                    if ( is_backing_storage_new == false && target_idx < oldsize )
                    {
                        structType *target_obj = (structType*)target_ptr;

                        if constexpr ( allow_move_operation_for_sure )
                        {
                            *target_obj = std::move( *source_obj );
                        }
                        else
                        {
                            if ( allow_move_operation )
                            {
                                *target_obj = std::move( *source_obj );
                            }
                            else
                            {
                                *target_obj = *(const insertValueType*)source_obj;
                            }
                        }
                    }
                    else
                    {
                        if constexpr ( allow_move_operation_for_sure )
                        {
                            new (target_ptr) structType( std::move( *source_obj ) );
                        }
                        else
                        {
                            if ( allow_move_operation )
                            {
                                new (target_ptr) structType( std::move( *source_obj ) );
                            }
                            else
                            {
                                new (target_ptr) structType( *(const insertValueType*)source_obj );
                            }
                        }
                    }

                    insidx++;
                }
            }
            catch( ... )
            {
                if constexpr ( allow_move_operation_for_sure == false )
                {
                    while ( insidx > 0 )
                    {
                        insidx--;

                        size_t target_idx = ( inspos + insidx );

                        structType *target_obj = (structType*)backing_storage + target_idx;

                        if ( is_backing_storage_new == false && target_idx < oldsize )
                        {
                            // There is nothing to do because we will reset these values using ones from the
                            // original array by clean-up of bs_arrange.
                        }
                        else
                        {
                            target_obj->~structType();
                        }
                    }
                }
                throw;
            }
        }
        catch( ... )
        {
            if ( inspos > oldsize )
            {
                _cleanup_default_constr();
            }
            throw;
        }

        // Cannot fail anymore, take over the new buffer, clear any unnecessary old.
        bs_arrange.Defuse();

        if ( is_backing_storage_new )
        {
            release_data( this, bs_arrange.oldarr, oldsize );
        }

        this->data.data_entries = (structType*)backing_storage;
        this->data.data_count = size_of_target;
    }

    template <std::convertible_to <structType> insertValueType>
    inline void Insert( size_t inspos, insertValueType&& insertItem )
        requires std::is_default_constructible <structType>::value &&
            ( is_vector_data_nothrow_arrangeable || std::is_copy_constructible <structType>::value )
    {
        size_t oldsize = this->data.data_count;
        size_t move_back_count;

        if ( inspos < oldsize )
        {
            move_back_count = 1;
        }
        else
        {
            move_back_count = 0;
        }

        size_t move_back_to = inspos + 1;
        size_t size_of_target = ( move_back_to + move_back_count );

        insert_backing_storage_arranger bs_arrange(
            this,
            this->data.data_entries, oldsize,
            inspos, size_of_target, move_back_to, move_back_count,
            VectorMemSize <structType> ( size_of_target )
        );

        // TODO: if the entire following operations are nothrow then we can release any back-up memory at this point.

        void *backing_storage = bs_arrange.GetBackingStorage();
        bool is_backing_storage_new = bs_arrange.IsBackingStorageNew();

        // Default-construct any entries that are not yet constructed and before the inspos.
        size_t def_constr_cnt;
        size_t def_constr_idx;

        auto _cleanup_default_constr = [&]( void ) noexcept
        {
            while ( def_constr_idx > 0 )
            {
                def_constr_idx--;

                structType *target_obj = (structType*)backing_storage + oldsize + def_constr_idx;

                target_obj->~structType();
            }
        };

        if ( inspos > oldsize )
        {
            def_constr_cnt = ( inspos - oldsize );
            def_constr_idx = 0;

            try
            {
                while ( def_constr_idx < def_constr_cnt )
                {
                    void *target_ptr = (structType*)backing_storage + oldsize + def_constr_idx;

                    new (target_ptr) structType();

                    def_constr_idx++;
                }
            }
            catch( ... )
            {
                _cleanup_default_constr();
                throw;
            }
        }

        try
        {
            // Put the data into the insertion spot.
            void *target_ptr = (structType*)backing_storage + inspos;

            if ( is_backing_storage_new == false && inspos < oldsize )
            {
                structType *target_obj = (structType*)target_ptr;

                *target_obj = std::forward <insertValueType> ( insertItem );
            }
            else
            {
                new (target_ptr) structType( std::forward <insertValueType> ( insertItem ) );
            }
        }
        catch( ... )
        {
            if ( inspos > oldsize )
            {
                _cleanup_default_constr();
            }
            throw;
        }

        // Cannot fail anymore, take over the new buffer, clear any unnecessary old.
        bs_arrange.Defuse();

        if ( is_backing_storage_new )
        {
            release_data( this, bs_arrange.oldarr, oldsize );
        }

        this->data.data_entries = (structType*)backing_storage;
        this->data.data_count = size_of_target;
    }

    // Insert a number of items into this vector, possibly with move-semantics.
    template <std::convertible_to <structType> insertValueType> requires requires ( Vector v, size_t a, insertValueType *b ) { v.InsertMulti( a, b, a ); }
    inline void InsertMove( size_t insertPos, insertValueType *insertItems, size_t insertCount )
    {
        this->InsertMulti( insertPos, insertItems, insertCount );
    }

    // Copy items into the vector.
    template <std::convertible_to <structType> insertValueType> requires requires ( Vector v, size_t a, const insertValueType *b ) { v.InsertMulti( a, b, a ); }
    inline void Insert( size_t insertPos, const insertValueType *insertItems, size_t insertCount )
    {
        this->InsertMulti( insertPos, insertItems, insertCount );
    }

    // Simple method for single insertion.
    template <std::convertible_to <structType> insertValueType> requires requires ( Vector v, insertValueType a, size_t s ) { v.Insert( s, a ); }
    inline void InsertMove( size_t insertPos, insertValueType&& value )
    {
        this->Insert( insertPos, std::forward <insertValueType> ( value ) );
    }

    // Replace entries in this vector.
    template <EqualityComparableValues <structType> queryType, std::convertible_to <structType> replType> requires ( Vectorable <structType> )
    inline void ReplaceSingle( const queryType& find, replType&& replace_with )
    {
        size_t cnt = this->data.data_count;
        structType *dataptr = this->data.data_entries;

        for ( size_t n = 0; n < cnt; n++ )
        {
            structType& entry = dataptr[ n ];

            if ( entry == find )
            {
                entry = std::forward <replType> ( replace_with );
                return;
            }
        }
    }
    // Replaces all entries of equal value in this vector. Returns true if any replacement was performed successfully.
    template <EqualityComparableValues <structType> queryType, std::convertible_to <structType> replType> requires ( Vectorable <structType> )
    inline bool ReplaceValues( const queryType& find, replType&& replace_with )
    {
        size_t cnt = this->data.data_count;
        structType *dataptr = this->data.data_entries;

        structType *prev_item = nullptr;

        bool didReplace = false;

        for ( size_t n = 0; n < cnt; n++ )
        {
            structType& entry = dataptr[ n ];

            if ( entry == find )
            {
                if ( prev_item != nullptr )
                {
                    *prev_item = ( replace_with );
                }

                prev_item = &entry;

                didReplace = true;
            }
        }

        if ( prev_item )
        {
            *prev_item = std::forward <replType> ( replace_with );

            didReplace = true;
        }

        return didReplace;
    }

    // Writes the contents of one array into this array at a certain offset.
    AINLINE void WriteVectorIntoCount( size_t writeOff, Vector&& srcData, size_t srcWriteCount, size_t srcStartOff = 0 )
    {
#ifdef _DEBUG
        assert( srcWriteCount + srcStartOff <= srcData.data.data_count );
#endif //_DEBUG

        if ( srcWriteCount == 0 )
            return;

        size_t oldCurSize = this->data.data_count;

        if ( writeOff == 0 && srcStartOff == 0 && oldCurSize <= srcWriteCount )
        {
            release_data( this, this->data.data_entries, oldCurSize );

            size_t actualSourceCount = srcData.data.data_count;

            this->data.data_entries = srcData.data.data_entries;
            this->data.data_count = actualSourceCount;

            srcData.data.data_entries = nullptr;
            srcData.data.data_count = 0;

            // Do we have to trim?
            if ( actualSourceCount > srcWriteCount )
            {
                this->Resize( srcWriteCount );
            }
        }
        else
        {
            // Make sure there is enough space to write our entries.
            size_t writeEndOff = ( writeOff + srcWriteCount );

            if ( oldCurSize < writeEndOff )
            {
                // TODO: optimize? would be possible by not constructing some entries...
                //  but maybe way too complicated.
                this->Resize( writeEndOff );
            }

            const structType *srcDataPtr = srcData.data.data_entries;
            structType *dstDataPtr = this->data.data_entries;

            for ( size_t n = 0; n < srcWriteCount; n++ )
            {
                dstDataPtr[ writeOff + n ] = std::move( srcDataPtr[ srcStartOff + n ] );
            }
        }
    }

private:
    AINLINE void shrink_backed_memory( structType *use_data, size_t newCount ) noexcept
    {
        if ( newCount == 0 )
        {
            this->data.opt().Free( this, use_data );

            // Gotta do this.
            this->data.data_entries = nullptr;
        }
        else
        {
            if constexpr ( ResizeMemoryAllocator <allocatorType> )
            {
                bool resizeSuccess = this->data.opt().Resize( this, use_data, VectorMemSize <structType> ( newCount ) );
                (void)resizeSuccess;

                // TODO: take care of optionally-resizable memory allocators, so that we can
                // iron-out this check properly.
                //FATAL_ASSERT( resizeSuccess == true );
            }
            else if constexpr ( is_vector_realloc_compatible && ReallocMemoryAllocator <allocatorType> )
            {
                // Since the data is allowed to be moved to a different memory location by the call
                // to realloc, we have to allow such behaviour transparently.
                // This is only possible if is_vector_realloc_compatible == true.

                this->data.data_entries = (structType*)this->data.opt().Realloc( this, use_data, VectorMemSize <structType> ( newCount ), alignof(structType) );
            }
        }

        this->data.data_count = newCount;
    }

public:
    // Removes multiple items from the vector.
    inline void RemoveMultipleByIndex( size_t removeIdx, size_t removeCnt )
    {
        if ( removeCnt == 0 )
            return;

        size_t oldCount = this->data.data_count;

        if ( removeIdx >= oldCount )
            return;

        size_t realRemoveCnt = std::min( removeCnt, oldCount - removeIdx );

        if ( realRemoveCnt == 0 )
            return;

        size_t newCount = ( oldCount - realRemoveCnt );

        // We can always shrink memory, so go for it.
        structType *use_data = this->data.data_entries;

        // We move down all items to squash the items that were removed.
        for ( size_t n = removeIdx; n < newCount; n++ )
        {
            *( use_data + n ) = std::move( *( use_data + n + realRemoveCnt ) );
        }

        // No point in keeping the last entries anymore, so destroy them.
        for ( size_t n = 0; n < realRemoveCnt; n++ )
        {
            use_data[ newCount + n ].~structType();
        }

        // Squish stuff.
        this->shrink_backed_memory( use_data, newCount );
    }

    inline void RemoveByIndex( size_t removeIdx )
    {
        RemoveMultipleByIndex( removeIdx, 1 );
    }

    inline void RemoveFromBack( void )
    {
        size_t oldCount = this->data.data_count;

        if ( oldCount == 0 )
            return;

        size_t newCount = ( oldCount - 1 );

        // We can always shrink memory, so go for it.
        structType *use_data = this->data.data_entries;

        use_data[ newCount ].~structType();

        this->shrink_backed_memory( use_data, newCount );
    }

    // Easy helper to remove all items of a certain value.
    template <EqualityComparableValues <structType> cmpValueType> requires ( Vectorable <structType> )
    inline bool RemoveByValue( const cmpValueType& theValue )
    {
        size_t curCount = this->data.data_count;
        size_t cur_idx = 0;

        bool removed = false;

        while ( cur_idx < curCount )
        {
            bool removeCurrent = false;
            {
                const structType& curItem = this->data.data_entries[ cur_idx ];

                if ( curItem == theValue )
                {
                    removeCurrent = true;
                }
            }

            if ( removeCurrent )
            {
                this->RemoveByIndex( cur_idx );

                removed = true;

                curCount--;
            }
            else
            {
                cur_idx++;
            }
        }

        return removed;
    }

    // Returns a reference to a slot on this vector. If indices up to and including this
    // slot do not exist, then they are standard-constructed.
    // Useful for map-operating-mode.
    inline structType& ObtainItem( size_t idx )
    {
        if ( idx >= this->data.data_count )
        {
            this->Resize( idx + 1 );
        }

        return this->data.data_entries[ idx ];
    }

    // Puts an item into the vector on a certain index. If there is not enough space on
    // this vector, then it is resized to fit automatically.
    // The new slots that are allocated will all be standard-constructed.
    // Useful for map-operating-mode.
    inline void SetItem( size_t idx, structType&& item )
    {
        this->ObtainItem( idx ) = std::move( item );
    }

    inline void SetItem( size_t idx, const structType& item )
    {
        this->ObtainItem( idx ) = item;
    }

    // Removes all items from this array by releasing the back-end memory.
    inline void Clear( void )
    {
        release_data( this, this->data.data_entries, this->data.data_count );

        this->data.data_entries = nullptr;
        this->data.data_count = 0;
    }

    inline size_t GetCount( void ) const
    {
        return this->data.data_count;
    }

    // Sets the array size, creating default items on new slots.
    inline void Resize( size_t newCount )
    {
        size_t oldCount = this->data.data_count;

        if ( oldCount == newCount )
            return;

        if ( oldCount > newCount )
        {
            // Also handles the case when memory is just being removed.

            structType *useData = this->data.data_entries;

            // We assume that we can always shrink.
            // First delete the stuff.
            for ( size_t iter = newCount; iter < oldCount; iter++ )
            {
                ( useData + iter )->~structType();
            }

            shrink_backed_memory( useData, newCount );
        }
        else
        {
            // Basically the same as Append but we default initialize the end.

            clone_backing_storage_arranger bs_arrange(
                this,
                this->data.data_entries, oldCount,
                VectorMemSize <structType> ( newCount )
            );

            void *backing_storage = bs_arrange.GetBackingStorage();

            size_t cidx = oldCount;

            try
            {
                while ( cidx < newCount )
                {
                    void *target_ptr = (structType*)backing_storage + cidx;

                    new (target_ptr) structType();

                    cidx++;
                }
            }
            catch( ... )
            {
                if constexpr ( std::is_nothrow_default_constructible <structType>::value == false )
                {
                    while ( cidx > oldCount )
                    {
                        cidx--;

                        structType *target_obj = (structType*)backing_storage + cidx;

                        target_obj->~structType();
                    }
                }
                throw;
            }

            // Accept the changes now.
            bs_arrange.Defuse();

            if ( bs_arrange.IsBackingStorageNew() )
            {
                release_data( this, bs_arrange.oldarr, oldCount );
            }

            this->data.data_entries = (structType*)backing_storage;
            this->data.data_count = newCount;
        }
    }

    // Returns true if an item that is equal to findItem already exists inside this vector.
    template <EqualityComparableValues <structType> findValueType>
    inline bool Find( const findValueType& findItem, size_t *idxOut = nullptr ) const
    {
        structType *data = this->data.data_entries;
        size_t numData = this->data.data_count;

        for ( size_t n = 0; n < numData; n++ )
        {
            if ( data[n] == findItem )
            {
                if ( idxOut )
                {
                    *idxOut = n;
                }

                return true;
            }
        }

        return false;
    }

    // Walks all items of this object.
    template <typename callbackType>
    AINLINE void Walk( const callbackType& cb )
    {
        structType *data = this->data.data_entries;
        size_t num_data = this->data.data_count;

        for ( size_t n = 0; n < num_data; n++ )
        {
            cb( n, data[ n ] );
        }
    }

    // Walks all items of this object, constant.
    template <typename callbackType>
    AINLINE void Walk( const callbackType& cb ) const
    {
        const structType *data = this->data.data_entries;
        size_t num_data = this->data.data_count;

        for ( size_t n = 0; n < num_data; n++ )
        {
            cb( n, data[ n ] );
        }
    }

    // To support the C++11 range-based for loop.
    AINLINE structType* begin( void )                   { return ( this->data.data_entries ); }
    AINLINE structType* end( void )                     { return ( this->data.data_entries + this->data.data_count ); }

    AINLINE const structType* begin( void ) const       { return ( this->data.data_entries ); }
    AINLINE const structType* end( void ) const         { return ( this->data.data_entries + this->data.data_count ); }

    // Indexing operators.
    inline structType& operator [] ( size_t idx )
    {
        if ( idx >= this->data.data_count )
        {
            // Index out of range.
            exceptMan::throw_index_out_of_bounds();
        }

        return this->data.data_entries[ idx ];
    }

    inline const structType& operator [] ( size_t idx ) const
    {
        if ( idx >= this->data.data_count )
        {
            // Index out of range.
            exceptMan::throw_index_out_of_bounds();
        }

        return this->data.data_entries[ idx ];
    }

    inline structType* GetData( void )
    {
        return this->data.data_entries;
    }

    inline const structType* GetData( void ) const
    {
        return this->data.data_entries;
    }

    inline structType& GetBack( void )
    {
        size_t curCount = this->data.data_count;

        if ( curCount == 0 )
        {
            // Not found.
            exceptMan::throw_not_found();
        }

        return this->data.data_entries[ curCount - 1 ];
    }

    inline structType& GetBack( void ) const
    {
        size_t curCount = this->data.data_count;

        if ( curCount == 0 )
        {
            // Not found.
            exceptMan::throw_not_found();
        }

        return this->data.data_entries[ curCount - 1 ];
    }

    // Implement some comparison operators.
    template <typename... otherVectorArgs>
    inline bool operator == ( const Vector <structType, otherVectorArgs...>& right ) const
    {
        size_t leftCount = this->data.data_count;
        size_t rightCount = right.data.data_count;

        if ( leftCount != rightCount )
            return false;

        const structType *leftData = this->data.data_entries;
        const structType *rightData = right.data.data_entries;

        for ( size_t n = 0; n < leftCount; n++ )
        {
            const structType& leftDataItem = leftData[ n ];
            const structType& rightDataItem = rightData[ n ];

            // Actually use the item comparator.
            if ( leftDataItem != rightDataItem )
            {
                return false;
            }
        }

        return true;
    }

    template <typename... otherVectorArgs>
    inline bool operator != ( const Vector <structType, otherVectorArgs...>& right ) const
    {
        return !( operator == ( right ) );
    }

private:
    // Need this thing as long as there is no static_if.
    // Better alternative to static_if: requires-clause on (member|stack) variables.
    struct fields
    {
        structType *data_entries;
        size_t data_count;
    };

    empty_opt <allocatorType, fields> data;
};

// Traits.
template <typename>
struct is_vector : std::false_type
{};
template <typename... vecArgs>
struct is_vector <Vector <vecArgs...>> : std::true_type
{};
template <typename... vecArgs>
struct is_vector <const Vector <vecArgs...>> : std::true_type
{};
template <typename... vecArgs>
struct is_vector <Vector <vecArgs...>&> : std::true_type
{};
template <typename... vecArgs>
struct is_vector <Vector <vecArgs...>&&> : std::true_type
{};
template <typename... vecArgs>
struct is_vector <const Vector <vecArgs...>&> : std::true_type
{};
template <typename... vecArgs>
struct is_vector <const Vector <vecArgs...>&&> : std::true_type
{};

} // namespace eir

#endif //_EIR_VECTOR_HEADER_
