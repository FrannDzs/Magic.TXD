/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/eirutils.h
*  PURPOSE:     Things that are commonly used but do not warrant for own header
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_COMMON_SDK_UTILITIES_
#define _EIR_COMMON_SDK_UTILITIES_

#include "Exceptions.h"

#include <algorithm>
#include <type_traits>

#include <cstddef>
#include <malloc.h>

#include <compare>

#include "DataUtil.h"
#include "MacroUtils.h"
#include "Allocation.h"

namespace eir
{

// Comparison results.
enum class eCompResult
{
    LEFT_LESS,
    EQUAL,
    LEFT_GREATER
};

static AINLINE std::weak_ordering cmpres_to_ordering( eCompResult res )
{
    switch( res )
    {
    case eCompResult::LEFT_LESS:
        return std::weak_ordering::less;
    case eCompResult::LEFT_GREATER:
        return std::weak_ordering::greater;
    default: break;
    }

    return std::weak_ordering::equivalent;
}

AINLINE eCompResult flip_comp_result( eCompResult res ) noexcept
{
    if ( res == eCompResult::LEFT_LESS )
    {
        return eCompResult::LEFT_GREATER;
    }
    else if ( res == eCompResult::LEFT_GREATER )
    {
        return eCompResult::LEFT_LESS;
    }

    return eCompResult::EQUAL;
}

#define EIR_VALCMP( left, right ) \
    { \
        auto cmpRes = eir::DefaultValueCompare( left, right ); \
        if ( cmpRes != eir::eCompResult::EQUAL ) \
        { \
            return cmpRes; \
        } \
    }
#define EIR_VALCMP_LT( _cmpVal ) \
    { \
        eir::eCompResult cmpVal = _cmpVal; \
        if ( cmpVal != eir::eCompResult::EQUAL ) \
        { \
            return ( cmpVal == eir::eCompResult::LEFT_LESS ); \
        } \
    }

template <typename LT, typename RT>
concept ThreeWayComparableValues = requires( LT l, RT r ) { l <=> r; };

template <typename LT, typename RT>
concept EqualityComparableValues = requires( LT l, RT r ) { { l == r } -> std::same_as <bool>; };

// Basic function.
template <typename leftType, typename rightType> requires ( ThreeWayComparableValues <leftType, rightType> )
AINLINE eCompResult DefaultValueCompare( const leftType& left, const rightType& right ) noexcept(noexcept(left <=> right))
{
    auto wo = ( left <=> right );

    if ( std::is_lt( wo ) )
    {
        return eCompResult::LEFT_LESS;
    }
    if ( std::is_gt( wo ) )
    {
        return eCompResult::LEFT_GREATER;
    }

    // TODO: think about the case of comparing floats of the "partial ordering" scenario (NaN, etc).

    return eCompResult::EQUAL;
}

template <typename structType, size_t N> requires requires ( structType T ) { T == T; }
AINLINE bool contains( std::array <structType, N> arr, const structType& val )
{
    for ( structType& arrval : arr )
    {
        if ( arrval == val )
            return true;
    }
    
    return false;
}

} // namespace eir

// Calculates the length of a string to the zero-terminator.
template <typename charType>
inline size_t cplen_tozero( const charType *chars )
{
    size_t len = 0;

    while ( *chars != (charType)0 )
    {
        len++;
        chars++;
    }

    return len;
}

#include <tuple>

template <typename structType, typename fieldsStruct>
struct empty_opt
{};

template <typename structType, typename fieldsStruct> requires ( std::is_empty <structType>::value == true )
struct empty_opt <structType, fieldsStruct> : public fieldsStruct
{
    AINLINE empty_opt( void ) = default;

    // Works due to guaranteed copy elision.
    template <typename... fieldsArgs, typename... structArgs>
    AINLINE empty_opt( std::tuple <fieldsArgs...>&& fields_args, std::tuple <structArgs...>&& struct_args )
        : fieldsStruct( std::make_from_tuple <fieldsStruct> ( std::move( fields_args ) ) )
    {
        return;
    }
    template <typename compatFieldsStruct> requires ( eir::constructible_from <fieldsStruct, compatFieldsStruct&&> )
    AINLINE empty_opt( compatFieldsStruct&& right_fields )
        : fieldsStruct( std::forward <compatFieldsStruct> ( right_fields ) )
    {
        return;
    }
    template <typename compatStructType> requires ( eir::constructible_from <structType, compatStructType&&> )
    AINLINE empty_opt( compatStructType&& right_data )
    {
        return;
    }
    AINLINE empty_opt( const empty_opt& ) = default;

    template <typename compatStructType, typename compatFieldsType>
        requires ( eir::constructible_from <structType, compatStructType&&> && eir::constructible_from <fieldsStruct, compatFieldsType&&> )
    AINLINE empty_opt( empty_opt <compatStructType, compatFieldsType>&& right )
        : fieldsStruct( std::forward <compatFieldsType> ( (compatFieldsType&)right ) )
    {}

    AINLINE empty_opt& operator = ( const empty_opt& ) = default;
    AINLINE empty_opt& operator = ( empty_opt&& ) = default;

    AINLINE structType& opt( void ) noexcept
    {
        return optvar;
    }

    AINLINE const structType& opt( void ) const noexcept
    {
        return optvar;
    }

private:
    inline static structType optvar;
};

template <typename structType, typename fieldsStruct> requires ( std::is_empty <structType>::value == false )
struct empty_opt <structType, fieldsStruct> : public fieldsStruct
{
    AINLINE empty_opt( void ) = default;

    template <typename... fieldsArgs, typename... structArgs>
    AINLINE empty_opt( std::tuple <fieldsArgs...>&& fields_args, std::tuple <structArgs...>&& struct_args )
        : fieldsStruct( std::make_from_tuple <fieldsStruct> ( std::move( fields_args ) ) ),
          optvar( std::make_from_tuple <structType> ( std::move( struct_args ) ) )
    {
        return;
    }
    template <typename compatFieldsStruct> requires ( eir::constructible_from <fieldsStruct, compatFieldsStruct&&> )
    AINLINE empty_opt( compatFieldsStruct&& right_fields )
        : fieldsStruct( std::forward <compatFieldsStruct> ( right_fields ) )
    {
        return;
    }
    template <typename compatStructType> requires ( eir::constructible_from <structType, compatStructType&&> )
    AINLINE empty_opt( compatStructType&& right_struct )
        : optvar( std::forward <compatStructType> ( right_struct ) )
    {
        return;
    }
    AINLINE empty_opt( const empty_opt& ) = default;

    template <typename compatStructType, typename compatFieldsType>
        requires ( eir::constructible_from <structType, compatStructType&&> && eir::constructible_from <fieldsStruct, compatFieldsType&&> )
    AINLINE empty_opt( empty_opt <compatStructType, compatFieldsType>&& right )
        : fieldsStruct( std::forward <compatFieldsType> ( (compatFieldsType&)right ) ), optvar( std::forward <compatStructType> ( right.optvar ) )
    {}

    AINLINE empty_opt& operator = ( const empty_opt& ) = default;
    AINLINE empty_opt& operator = ( empty_opt&& ) = default;

    AINLINE structType& opt( void ) noexcept
    {
        return this->optvar;
    }

    AINLINE const structType& opt( void ) const noexcept
    {
        return this->optvar;
    }

private:
    structType optvar;
};

// DEBUG checks.

[[noreturn]] inline static void fatal_abort_func( const char *filename, size_t linenum )
{
    *(char*)nullptr = 0;

    // This point is never reached but whatever.
    while ( true )  {}
}

#define FATAL_ABORT()   fatal_abort_func( __FILE__, __LINE__ )

// Safe assert that crashes the program on failure.
inline static void check_debug_condition( bool condition, const char *expression, const char *filename, size_t linenum )
{
    if ( !condition )
    {
        // Crash the program. A debugger should know what this is about.
        fatal_abort_func( filename, linenum );
    }
}

#define FATAL_ASSERT( expr )  check_debug_condition( expr, #expr, __FILE__, __LINE__ )

// A lot of objects used internally by NativeExecutive can only be constructed once the runtime has initialized for the first time.
// Thus we want to provide the space for them, but not initialize with program load.
template <typename structType>
struct alignas(alignof(structType)) optional_struct_space
{
    inline constexpr optional_struct_space( void ) noexcept = default;
    inline optional_struct_space( const optional_struct_space& ) = delete;
    inline optional_struct_space( optional_struct_space&& ) = delete;

    inline ~optional_struct_space( void )
    {
        // We cannot do this because components inside statics can depend on each-other
        // and we have no idea to fix this dependency-order.
#if 0
        // Added this to prevent assertions for when the memory-override-system of NativeExecutive
        // could find no time to properly shutdown memory manager globals but still work properly.
        if constexpr ( std::is_trivially_destructible <structType>::value == false )
        {
#ifdef _DEBUG
            FATAL_ASSERT( this->is_init == false );
#endif //_DEBUG
        }
#endif //0
    }

    inline optional_struct_space& operator = ( const optional_struct_space& ) = delete;
    inline optional_struct_space& operator = ( optional_struct_space&& ) = delete;

    template <typename... Args>
    AINLINE void Construct( Args&&... constrArgs ) noexcept(std::is_nothrow_constructible <structType, decltype(std::forward <Args> ( constrArgs ))...>::value)
    {
#ifdef _DEBUG
        FATAL_ASSERT( this->is_init == false );

        this->is_init = true;
#endif //_DEBUG

        new (struct_space) structType( std::forward <Args> ( constrArgs )... );
    }

    AINLINE void Destroy( void ) noexcept(std::is_nothrow_destructible <structType>::value)
    {
#ifdef _DEBUG
        FATAL_ASSERT( this->is_init == true );

        this->is_init = false;
#endif //_DEBUG

        ( *( (structType*)struct_space ) ).~structType();
    }

    AINLINE structType& get( void ) const noexcept
    {
#ifdef _DEBUG
        FATAL_ASSERT( this->is_init == true );
#endif //_DEBUG

        return *(structType*)struct_space;
    }

    AINLINE operator structType& ( void ) const noexcept
    {
#ifdef _DEBUG
        FATAL_ASSERT( this->is_init == true );
#endif //_DEBUG

        return get();
    }

private:
    alignas(alignof(structType)) char struct_space[ sizeof(structType) ] = {};

#ifdef _DEBUG
    bool is_init = false;
#endif //_DEBUG
};

// It is often required to construct an optional_struct_space as part of a running stack.
// Thus we need a wrapper that simplifies the cleanup of such activity.
template <typename structType, typename exceptMan = eir::DefaultExceptionManager>
struct optional_struct_space_init
{
    AINLINE optional_struct_space_init( void ) noexcept
    {
        this->varloc = nullptr;
    }

    template <typename... constrArgs>
    AINLINE optional_struct_space_init( optional_struct_space <structType>& varloc, constrArgs&&... args ) noexcept(noexcept(varloc.Construct( std::forward <constrArgs> ( args )... ))) : varloc( &varloc )
    {
        varloc.Construct( std::forward <constrArgs> ( args )... );
    }
    AINLINE optional_struct_space_init( const optional_struct_space_init& ) = delete;
    AINLINE optional_struct_space_init( optional_struct_space_init&& right ) noexcept
    {
        this->varloc = right.varloc;

        right.varloc = nullptr;
    }
    AINLINE ~optional_struct_space_init( void ) noexcept(noexcept(this->varloc->Destroy()))
    {
        if ( optional_struct_space <structType> *varloc = this->varloc )
        {
            varloc->Destroy();
        }
    }

    AINLINE optional_struct_space_init& operator = ( const optional_struct_space_init& ) = delete;
    AINLINE optional_struct_space_init& operator = ( optional_struct_space_init&& right ) noexcept(noexcept(this->varloc->Destroy()))
    {
        if ( optional_struct_space <structType> *varloc = this->varloc )
        {
            varloc->Destroy();
        }

        this->varloc = right.varloc;

        right.varloc = nullptr;

        return *this;
    }

    AINLINE structType& get( void ) const
    {
        optional_struct_space <structType> *varloc = this->varloc;

        if ( varloc == nullptr )
        {
            exceptMan::throw_not_initialized();
        }

        return varloc->get();
    }

private:
    optional_struct_space <structType> *varloc;
};

#endif //_EIR_COMMON_SDK_UTILITIES_
