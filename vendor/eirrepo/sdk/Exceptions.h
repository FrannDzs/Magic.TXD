/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Exceptions.h
*  PURPOSE:     Shared exception definitions of the SDK.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_EXCEPTIONS_HEADER_
#define _EIR_EXCEPTIONS_HEADER_

#include "MacroUtils.h"

#include <stddef.h>

namespace eir
{

//===========================
// *** Exception classes ***
//===========================

enum class eExceptionType
{
    UNDEFINED,
    OUT_OF_MEMORY,
    INTERNAL_ERROR,
    TYPE_NAME_CONFLICT,
    ABSTRACTION_CONSTRUCTION,
    UNDEFINED_METHOD,
    NOT_INITIALIZED,
    MATH_VALUE_OUT_OF_VALID_RANGE,
    OPERATION_FAILED,
    CODEPOINT,
    INDEX_OUT_OF_BOUNDS,
    NOT_FOUND
};

// Base exception class for the entire SDK.
struct eir_exception
{
    inline eir_exception( void ) noexcept
    {
        this->type = eExceptionType::UNDEFINED;
    }
    inline eir_exception( eExceptionType type ) noexcept : type( type )
    {
        return;
    }

    inline eir_exception( eir_exception&& ) = default;
    inline eir_exception( const eir_exception& ) = default;

    inline eir_exception& operator = ( eir_exception&& ) = default;
    inline eir_exception& operator = ( const eir_exception& ) = default;

    inline eExceptionType GetType( void ) const noexcept
    {
        return this->type;
    }

private:
    eExceptionType type;
};

struct out_of_memory_exception : public eir_exception
{
    inline out_of_memory_exception( size_t lastFailedRequestSize ) noexcept : eir_exception( eExceptionType::OUT_OF_MEMORY )
    {
        this->lastFailedRequestSize = lastFailedRequestSize;
    }

    inline out_of_memory_exception( out_of_memory_exception&& ) = default;
    inline out_of_memory_exception( const out_of_memory_exception& ) = default;

    inline out_of_memory_exception& operator = ( out_of_memory_exception&& ) = default;
    inline out_of_memory_exception& operator = ( const out_of_memory_exception& ) = default;

    inline size_t GetLastFailedRequestSize( void ) const noexcept
    {
        return this->lastFailedRequestSize;
    }

private:
    size_t lastFailedRequestSize;
};

struct internal_error_exception : public eir_exception
{
    inline internal_error_exception( void ) noexcept : eir_exception( eExceptionType::INTERNAL_ERROR )
    {
        return;
    }
};

struct type_name_conflict_exception : public eir_exception
{
    inline type_name_conflict_exception( void ) noexcept : eir_exception( eExceptionType::TYPE_NAME_CONFLICT )
    {
        return;
    }
};

struct abstraction_construction_exception : public eir_exception
{
    inline abstraction_construction_exception( void ) noexcept : eir_exception( eExceptionType::ABSTRACTION_CONSTRUCTION )
    {
        return;
    }
};

enum class eMethodType
{
    DEFAULT_CONSTRUCTOR,
    MOVE_CONSTRUCTOR,
    COPY_CONSTRUCTOR,
    MOVE_ASSIGNMENT,
    COPY_ASSIGNMENT
};

struct undefined_method_exception : public eir_exception
{
    inline undefined_method_exception( eMethodType type ) noexcept : eir_exception( eExceptionType::UNDEFINED_METHOD )
    {
        this->method_type = type;
    }

    inline undefined_method_exception( undefined_method_exception&& ) = default;
    inline undefined_method_exception( const undefined_method_exception& ) = default;

    inline undefined_method_exception& operator = ( undefined_method_exception&& ) = default;
    inline undefined_method_exception& operator = ( const undefined_method_exception& ) = default;

    inline eMethodType GetMethodType( void ) const noexcept
    {
        return this->method_type;
    }

private:
    eMethodType method_type;
};

struct not_initialized_exception : public eir_exception
{
    inline not_initialized_exception( void ) noexcept : eir_exception( eExceptionType::NOT_INITIALIZED )
    {
        return;
    }
};

struct math_value_out_of_valid_range_exception : public eir_exception
{
    inline math_value_out_of_valid_range_exception( void ) noexcept : eir_exception( eExceptionType::MATH_VALUE_OUT_OF_VALID_RANGE )
    {
        return;
    }
};

enum class eOperationType
{
    READ,
    WRITE
};

struct operation_failed_exception : public eir_exception
{
    inline operation_failed_exception( eOperationType type ) noexcept : eir_exception( eExceptionType::OPERATION_FAILED )
    {
        this->op_type = type;
    }

    inline operation_failed_exception( operation_failed_exception&& ) = default;
    inline operation_failed_exception( const operation_failed_exception& ) = default;

    inline operation_failed_exception& operator = ( operation_failed_exception&& ) = default;
    inline operation_failed_exception& operator = ( const operation_failed_exception& ) = default;

private:
    eOperationType op_type;
};

struct codepoint_exception : public eir_exception
{
    inline codepoint_exception( void ) noexcept : eir_exception( eExceptionType::CODEPOINT )
    {
        this->msg = nullptr;
    }
    inline codepoint_exception( const wchar_t *msg ) noexcept : eir_exception( eExceptionType::CODEPOINT )
    {
        this->msg = msg;
    }

    inline codepoint_exception( codepoint_exception&& ) = default;
    inline codepoint_exception( const codepoint_exception& ) = default;

    inline codepoint_exception& operator = ( codepoint_exception&& ) = default;
    inline codepoint_exception& operator = ( const codepoint_exception& ) = default;

    inline const wchar_t* what( void ) const
    {
        return this->msg;
    }

private:
    const wchar_t *msg;
};

struct index_out_of_bounds_exception : public eir_exception
{
    inline index_out_of_bounds_exception( void ) noexcept : eir_exception( eExceptionType::INDEX_OUT_OF_BOUNDS )
    {
        return;
    }
};

struct not_found_exception : public eir_exception
{
    inline not_found_exception( void ) noexcept : eir_exception( eExceptionType::NOT_FOUND )
    {
        return;
    }
};

//=============================================================
// *** Default exception manager for all container types ***
//=============================================================

struct DefaultExceptionManager
{
    [[noreturn]] AINLINE static void throw_oom( size_t lastFailedRequestSize )
    {
        throw out_of_memory_exception( lastFailedRequestSize );
    }
    [[noreturn]] AINLINE static void throw_internal_error( void )
    {
        throw internal_error_exception();
    }
    [[noreturn]] AINLINE static void throw_type_name_conflict( void )
    {
        throw type_name_conflict_exception();
    }
    [[noreturn]] AINLINE static void throw_abstraction_construction( void )
    {
        throw abstraction_construction_exception();
    }
    [[noreturn]] AINLINE static void throw_undefined_method( eMethodType type )
    {
        throw undefined_method_exception( type );
    }
    [[noreturn]] AINLINE static void throw_not_initialized( void )
    {
        throw not_initialized_exception();
    }
    [[noreturn]] AINLINE static void throw_math_value_out_of_valid_range( void )
    {
        throw math_value_out_of_valid_range_exception();
    }
    [[noreturn]] AINLINE static void throw_operation_failed( eOperationType opType )
    {
        throw operation_failed_exception( opType );
    }
    [[noreturn]] AINLINE static void throw_codepoint( const wchar_t *msg = nullptr )
    {
        throw codepoint_exception( msg );
    }
    [[noreturn]] AINLINE static void throw_index_out_of_bounds( void )
    {
        throw index_out_of_bounds_exception();
    }
    [[noreturn]] AINLINE static void throw_not_found( void )
    {
        throw not_found_exception();
    }
};

} // namespace eir

#endif //_EIR_EXCEPTIONS_HEADER_
