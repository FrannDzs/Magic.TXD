/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Function.h
*  PURPOSE:     Dynamic function storage struct helper (i.e. lambdas)
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// Motivation for adding this type was that the STL does not support providing
// a custom allocator for std::function.

#ifndef _DYNAMIC_FUNCTION_STORAGE_
#define _DYNAMIC_FUNCTION_STORAGE_

#include "Exceptions.h"

#include <type_traits>

#include "MacroUtils.h"
#include "eirutils.h"

namespace eir
{

// TODO: add object-allocator support to eir::Function, refptr support, etc.

template <StaticMemoryAllocator allocatorType, typename exceptMan, typename returnType, typename... cbArgs>
struct Function
{
    AINLINE Function( void ) noexcept
    {
        this->func_mem = nullptr;
    }

private:
    template <typename callbackType> requires ( std::is_invocable_r <returnType, callbackType, cbArgs...>::value )
    AINLINE void assign_callback( callbackType&& cb )
    {
        struct lambda_function_storage final : public virtual_function_storage
        {
            AINLINE lambda_function_storage( Function *func, callbackType&& cb ) : cb( std::forward <callbackType> ( cb ) )
            {
                return;
            }

            AINLINE virtual_function_storage* clone( void ) override
            {
                if constexpr ( std::copy_constructible <callbackType> )
                {
                    return static_new_struct <lambda_function_storage, allocatorType, exceptMan> ( this->func, this->func, callbackType( cb ) );
                }
                else
                {
                    exceptMan::throw_undefined_method( eMethodType::COPY_CONSTRUCTOR );
                }
            }

            AINLINE returnType invoke( cbArgs&&... args )
            {
                return cb( std::forward <cbArgs> ( args )... );
            }

            Function *func;
            callbackType cb;
        };

        this->func_mem = static_new_struct <lambda_function_storage, allocatorType, exceptMan> ( this, this, std::forward <callbackType> ( cb ) );
    }

public:
    template <typename callbackType> requires ( std::is_invocable_r <returnType, callbackType, cbArgs...>::value )
    AINLINE Function( callbackType&& cb )
    {
        assign_callback( std::forward <callbackType> ( cb ) );
    }
    AINLINE Function( const Function& right )
    {
        if ( virtual_function_storage *right_func = right.func_mem )
        {
            this->func_mem = right_func->clone();
        }
        else
        {
            this->func_mem = nullptr;
        }
    }
    // allocators have to stay the same due to the virtualization.
    AINLINE Function( Function&& right ) noexcept
    {
        this->func_mem = right.func_mem;

        right.func_mem = nullptr;
    }

private:
    AINLINE void _clear_mem( void )
    {
        if ( virtual_function_storage *func_mem = this->func_mem )
        {
            static_del_struct <virtual_function_storage, allocatorType> ( this, func_mem );

            this->func_mem = nullptr;
        }
    }

public:
    AINLINE ~Function( void )
    {
        _clear_mem();
    }

    AINLINE Function& operator = ( const Function& right )
    {
        _clear_mem();

        if ( virtual_function_storage *right_func = right.func_mem )
        {
            this->func_mem = right_func->clone();
        }
        else
        {
            this->func_mem = nullptr;
        }

        return *this;
    }
    // allocators have to stay the same due to the virtualization.
    AINLINE Function& operator = ( Function&& right ) noexcept
    {
        _clear_mem();

        this->func_mem = right.func_mem;

        right.func_mem = nullptr;

        return *this;
    }
    template <typename callbackType> requires ( std::is_invocable_r <returnType, callbackType, cbArgs...>::value )
    AINLINE Function& operator = ( callbackType&& cb )
    {
        _clear_mem();

        this->assign_callback( std::forward <callbackType> ( cb ) );

        return *this;
    }

    AINLINE bool is_good( void ) const
    {
        return ( this->func_mem != nullptr );
    }

    AINLINE returnType operator () ( cbArgs&&... args )
    {
        virtual_function_storage *func_mem = this->func_mem;

        if ( func_mem == nullptr )
        {
            exceptMan::throw_not_initialized();
        }

        return func_mem->invoke( std::forward <cbArgs> ( args )... );
    }

private:
    struct virtual_function_storage
    {
        virtual ~virtual_function_storage( void )
        {
            return;
        }

        virtual virtual_function_storage* clone( void ) = 0;

        virtual returnType invoke( cbArgs&&... args ) = 0;
    };

    virtual_function_storage *func_mem;
};

}

#endif //_DYNAMIC_FUNCTION_STORAGE_
