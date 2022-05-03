/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Templates.h
*  PURPOSE:     String template generation utilities.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_TEMPLATES_HEADER_
#define _EIR_TEMPLATES_HEADER_

#include "Exceptions.h"
#include "String.h"
#include "UniChar.h"
#include "Map.h"
#include "StringUtils.h"

namespace eir
{

template <typename dstCharType, MemoryAllocator dstAllocatorType, typename dstExceptMan = eir::DefaultExceptionManager, typename srcCharType, typename callbackType, typename... allocArgs>
AINLINE String <dstCharType, dstAllocatorType, dstExceptMan> assign_template_callback( const srcCharType *src, size_t srcLen, callbackType cb, const allocArgs&... args )
{
    String <dstCharType, dstAllocatorType, dstExceptMan> result( nullptr, 0, args... );

    charenv_charprov_tocplen <srcCharType> prov( src, srcLen );
    character_env_iterator <srcCharType, decltype(prov)> iter( src, std::move( prov ) );

    typedef typename character_env <srcCharType>::ucp_t ucp_t;

    while ( iter.IsEnd() == false )
    {
        ucp_t ucp = iter.ResolveAndIncrement();

        bool ignoreUCP = false;

        if ( ucp == '%' && iter.IsEnd() == false )
        {
            auto try_iter = iter;

            if ( try_iter.ResolveAndIncrement() == '(' )
            {
                bool hasEnded = false;
                bool hasLinebreak = false;
                const srcCharType *start_of_key = try_iter.GetPointer();
                const srcCharType *end_of_key = nullptr;

                while ( true )
                {
                    if ( try_iter.IsEnd() )
                    {
                        hasEnded = true;
                        break;
                    }

                    end_of_key = try_iter.GetPointer();

                    auto try_ucp = try_iter.ResolveAndIncrement();

                    if ( try_ucp == '\n' )
                    {
                        hasLinebreak = true;
                        break;
                    }

                    if ( try_ucp == ')' )
                    {
                        break;
                    }
                }

                if ( hasEnded == false && hasLinebreak == false )
                {
                    cb( start_of_key, end_of_key - start_of_key, result );

                    ignoreUCP = true;
                    iter = try_iter;
                }
            }
        }

        if ( ignoreUCP == false )
        {
            CharacterUtil::TranscodeCharacter <srcCharType> ( ucp, result );
        }
    }

    return result;
}

template <typename dstCharType, MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager, typename srcCharType, typename... mapArgs, typename... keyStringArgs, typename... valueStringArgs, typename... allocArgs>
inline String <dstCharType, allocatorType, exceptMan> assign_template( const srcCharType *src, size_t srcLen, const Map <String <srcCharType, keyStringArgs...>, String <srcCharType, valueStringArgs...>, mapArgs...>& params, const allocArgs&... args )
{
    // TODO: invent a FixedString type so we can use std::move on the args and only use them once without allocating a string for nothing
    // for each replacement in this function.

    return assign_template_callback <dstCharType, allocatorType, exceptMan> ( src, srcLen,
        [&]( const srcCharType *keyPtr, size_t keyLen, eir::String <dstCharType, allocatorType, exceptMan>& result_append )
        {
            String <srcCharType, allocatorType, exceptMan> key( keyPtr, keyLen, args... );

            auto valueNode = params.Find( key );

            if ( valueNode != nullptr )
            {
                // Append transformed codepoints.
                size_t valueLen = valueNode->GetValue().GetLength();
                const srcCharType *valuePtr = valueNode->GetValue().GetConstString();

                character_env_iterator <srcCharType, charenv_charprov_tocplen <srcCharType>> value_iter( valuePtr, { valuePtr, valueLen } );

                while ( !value_iter.IsEnd() )
                {
                    auto val_ucp = value_iter.ResolveAndIncrement();

                    CharacterUtil::TranscodeCharacter <srcCharType> ( val_ucp, result_append );
                }
            }
        }, args...
    );
}

} // namespace eir

#endif //_EIR_TEMPLATES_HEADER_