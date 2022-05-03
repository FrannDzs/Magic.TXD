/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MultiValueStore.h
*  PURPOSE:     Set-based container that keeps track of same-value insertions.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRSDK_MULTI_VALUE_STORE_HEADER_
#define _EIRSDK_MULTI_VALUE_STORE_HEADER_

#include "Allocation.h"
#include "Set.h"

namespace eir
{

#define MVS_TEMPLARGS \
template <typename valueType, MemoryAllocator allocatorType, typename exceptMan = DefaultExceptionManager>
#define MVS_TEMPLARGS_NODEF \
template <typename valueType, MemoryAllocator allocatorType, typename exceptMan>
#define MVS_TEMPLUSE \
<valueType, allocatorType, exceptMan>

MVS_TEMPLARGS
struct MultiValueStore
{
    inline MultiValueStore( void ) noexcept requires ( nothrow_constructible_from <allocatorType> )
    {
        return;
    }
    template <typename... allocArgs> requires ( nothrow_constructible_from <allocatorType, allocArgs&&...> )
    inline MultiValueStore( eir::constr_with_alloc _, allocArgs&&... args ) noexcept : allocMan( std::forward <allocArgs> ( args )... )
    {
        return;
    }
    template <typename subValueType, size_t N> requires ( std::convertible_to <subValueType, valueType> )
    inline MultiValueStore( std::array <subValueType, N> values )
    {
        for ( subValueType& value : values )
        {
            this->Insert( std::move( value ) );
        }
    }
    inline MultiValueStore( const MultiValueStore& ) = default;
    inline MultiValueStore( MultiValueStore&& ) = default;

    inline MultiValueStore& operator = ( const MultiValueStore& ) = default;
    inline MultiValueStore& operator = ( MultiValueStore&& ) = default;

    template <std::convertible_to <valueType> convValueType>
    inline void Insert( convValueType&& value )
    {
        auto *findNode = this->valrefs.Find( value );

        if ( findNode != nullptr )
        {
            const valinfo& info = findNode->GetValue();

            info.refcnt++;
        }
        else
        {
            valinfo newinfo( std::forward <convValueType> ( value ), 1 );

            this->valrefs.Insert( std::move( newinfo ) );
        }
    }

    // Removes a single instance of the given value. Returns true if successful.
    template <ThreeWayComparableValues <valueType> queryType>
    inline bool Remove( const queryType& value )
    {
        auto *findNode = this->valrefs.Find( value );

        if ( findNode == nullptr )
            return false;

        const valinfo& info = findNode->GetValue();

        size_t refcnt = ( info.refcnt - 1 );

        if ( refcnt > 0 )
        {
            info.refcnt = refcnt;
        }
        else
        {
            this->valrefs.RemoveNode( findNode );
        }
        return true;
    }
    // Removes all instances of the given value.
    template <ThreeWayComparableValues <valueType> queryType>
    inline bool RemoveAll( const queryType& value )
    {
        return this->valrefs.Remove( value );
    }

    // Returns true if the replacement was successfully performed, hence findval has existed and was replaced with
    // replaceby.
    template <ThreeWayComparableValues <valueType> queryType, std::convertible_to <valueType> convValueType>
    inline bool Replace( const queryType& findval, convValueType&& replaceby )
    {
        if ( auto *findNode = this->valrefs.Find( findval ) )
        {
            if ( auto *appendNode = this->valrefs.Find( replaceby ) )
            {
                const valinfo& vinfo = appendNode->GetValue();

                valinfo oldvinfo = this->valrefs.ExtractNode( findNode );

                vinfo.refcnt += oldvinfo.refcnt;
            }
            else
            {
                valinfo& vinfo = this->valrefs.BeginNodeUpdate( findNode );

                vinfo.value = std::forward <convValueType> ( replaceby );

                this->valrefs.EndNodeUpdate( findNode );
            }
            return true;
        }
        return false;
    }

    inline size_t GetValueCount( void ) const noexcept
    {
        return this->valrefs.GetValueCount();
    }

    template <ThreeWayComparableValues <valueType> queryType>
    inline size_t GetRefCount( const queryType& value ) const
    {
        auto *findNode = this->valrefs.Find( value );

        if ( findNode == nullptr )
            return 0;

        const valinfo& vinfo = findNode->GetValue();

        return vinfo.refcnt;
    }

    template <ThreeWayComparableValues <valueType> queryType>
    inline bool Find( const queryType& value ) const
    {
        return ( this->valrefs.Find( value ) != nullptr );
    }

private:
    struct valinfo
    {
        template <std::convertible_to <valueType> convValueType>
        inline valinfo( convValueType&& value, size_t refcnt = 0 ) noexcept : value( std::forward <convValueType> ( value ) ), refcnt( refcnt )
        {
            return;
        }
        inline valinfo( const valinfo& ) = default;
        inline valinfo( valinfo&& right ) noexcept : value( std::move( right.value ) )
        {
            this->refcnt = right.refcnt;

            right.refcnt = 0;
        }

        static inline eir::eCompResult compare_values( const valinfo& left, const valinfo& right )
        {
            return eir::DefaultValueCompare( left.value, right.value );
        }

        static inline eir::eCompResult compare_values( const valinfo& left, const valueType& right )
        {
            return eir::DefaultValueCompare( left.value, right );
        }

        static inline eir::eCompResult compare_values( const valueType& left, const valinfo& right )
        {
            return eir::DefaultValueCompare( left, right.value );
        }

        valueType value;
        mutable size_t refcnt = 0;
    };

    [[no_unique_address]] allocatorType allocMan;

    DEFINE_HEAP_REDIR_ALLOC_BYSTRUCT( valrefs_redirAlloc, allocatorType );

    eir::Set <valinfo, valrefs_redirAlloc, valinfo, exceptMan> valrefs;

public:
    typedef typename decltype(valrefs)::end_std_iterator end_std_iterator;

    struct std_iterator
    {
        inline std_iterator( void ) = default;
        inline std_iterator( typename decltype(valrefs)::std_iterator&& iter ) noexcept : iter( std::move( iter ) )
        {
            return;
        }
        inline std_iterator( const std_iterator& ) = default;
        inline std_iterator( std_iterator&& ) = default;

        inline std_iterator& operator = ( const std_iterator& ) = default;
        inline std_iterator& operator = ( std_iterator&& ) = default;

        inline bool operator != ( const end_std_iterator& right ) const noexcept
        {
            return ( iter != right );
        }

        inline std_iterator& operator ++ ( void ) noexcept
        {
            ++iter;
            return *this;
        }

        inline const valueType& operator * ( void ) const noexcept
        {
            return (*iter).value;
        }

    private:
        typename decltype(valrefs)::std_iterator iter;
    };

    AINLINE std_iterator begin( void ) const        { return std_iterator( this->valrefs.begin() ); }
    AINLINE end_std_iterator end( void ) const      { return this->valrefs.end(); }

    template <ThreeWayComparableValues <valueType> queryType>
    AINLINE std_iterator begin_from( const queryType& value ) const
    {
        return typename decltype(valrefs)::std_iterator( this->valrefs.Find( value ) );
    }

    template <ThreeWayComparableValues <valueType> queryType>
    AINLINE std_iterator begin_justAbove( const queryType& value ) const
    {
        return typename decltype(valrefs)::std_iterator( this->valrefs.FindJustAbove( value ) );
    }
};

IMPL_HEAP_REDIR_DYN_ALLOC_TEMPLATEBASE( MultiValueStore, MVS_TEMPLARGS_NODEF, MVS_TEMPLUSE, valrefs_redirAlloc, MultiValueStore, valrefs, allocMan, allocatorType );

// Traits.
template <typename>
struct is_multivaluestore : std::false_type
{};
template <typename... mvsArgs>
struct is_multivaluestore <MultiValueStore <mvsArgs...>> : std::true_type
{};
template <typename... mvsArgs>
struct is_multivaluestore <const MultiValueStore <mvsArgs...>> : std::true_type
{};
template <typename... mvsArgs>
struct is_multivaluestore <MultiValueStore <mvsArgs...>&> : std::true_type
{};
template <typename... mvsArgs>
struct is_multivaluestore <const MultiValueStore <mvsArgs...>&> : std::true_type
{};
template <typename... mvsArgs>
struct is_multivaluestore <MultiValueStore <mvsArgs...>&&> : std::true_type
{};
template <typename... mvsArgs>
struct is_multivaluestore <const MultiValueStore <mvsArgs...>&&> : std::true_type
{};

} // namespace eir

#endif //_EIRSDK_MULTI_VALUE_STORE_HEADER_