/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.errsys.ext.h
*  PURPOSE:     Delayed error system exception definitions.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_ERRORSYS_EXT_HEADER_
#define _RENDERWARE_ERRORSYS_EXT_HEADER_

#include "renderware.errsys.common.h"
#include "renderware.common.h"

namespace rw
{

//================================
// *** Main exception classes ***
//================================

// Thrown if a custom allocated string should be attached to the exception, as reported by the runtime.
// ANSI version.
// NOTE: please only use this very rarely/with a good reason because it does introduce localization issues.
struct UnlocalizedAInternalErrorException : public virtual RwException
{
    inline UnlocalizedAInternalErrorException( eSubsystemType subsys, const wchar_t *descTokenTemplate, rwStaticString <char>&& reason_msg ) noexcept
        : RwException( eErrorType::UNLOCALAINTERNERR, subsys ), descTokenTemplate( descTokenTemplate ), reason_msg( ::std::move( reason_msg ) )
    {
        return;
    }

    inline UnlocalizedAInternalErrorException( UnlocalizedAInternalErrorException&& ) = default;
    inline UnlocalizedAInternalErrorException( const UnlocalizedAInternalErrorException& ) = default;

    inline UnlocalizedAInternalErrorException& operator = ( UnlocalizedAInternalErrorException&& ) = default;
    inline UnlocalizedAInternalErrorException& operator = ( const UnlocalizedAInternalErrorException& ) = default;

    inline const wchar_t* getTemplateToken( void ) const noexcept
    {
        return this->descTokenTemplate;
    }

    inline const rwStaticString <char>& getReason( void ) const noexcept
    {
        return this->reason_msg;
    }

private:
    const wchar_t *descTokenTemplate;
    rwStaticString <char> reason_msg;
};

// Thrown if a custom allocated string should be attached to the exception, as reported by the runtime.
// Wide version.
// NOTE: please only use this very rarely/with a good reason because it does introduce localization issues.
struct UnlocalizedWInternalErrorException : public virtual RwException
{
    inline UnlocalizedWInternalErrorException( eSubsystemType subsys, const wchar_t *descTokenTemplate, rwStaticString <wchar_t>&& reason_msg ) noexcept
        : RwException( eErrorType::UNLOCALWINTERNERR, subsys ), descTokenTemplate( descTokenTemplate ), reason_msg( ::std::move( reason_msg ) )
    {
        return;
    }

    inline UnlocalizedWInternalErrorException( UnlocalizedWInternalErrorException&& ) = default;
    inline UnlocalizedWInternalErrorException( const UnlocalizedWInternalErrorException& ) = default;

    inline UnlocalizedWInternalErrorException& operator = ( UnlocalizedWInternalErrorException&& ) = default;
    inline UnlocalizedWInternalErrorException& operator = ( const UnlocalizedWInternalErrorException& ) = default;

    inline const wchar_t* getTemplateToken( void ) const noexcept
    {
        return this->descTokenTemplate;
    }

    inline const rwStaticString <wchar_t>& getReason( void ) const noexcept
    {
        return this->reason_msg;
    }

private:
    const wchar_t *descTokenTemplate;
    rwStaticString <wchar_t> reason_msg;
};

// Thrown if an unexpected chunk/block as encountered.
// Always originating from the Block API.
struct UnexpectedChunkException : public virtual RwException
{
    inline UnexpectedChunkException( eSubsystemType subsys, uint32 expectedChunkID, uint32 foundChunkID ) noexcept
        : RwException( eErrorType::UNEXPCHUNK, subsys ), expectedChunkID( expectedChunkID ), foundChunkID( foundChunkID )
    {
        return;
    }

    inline UnexpectedChunkException( UnexpectedChunkException&& ) = default;
    inline UnexpectedChunkException( const UnexpectedChunkException& ) = default;

    inline UnexpectedChunkException& operator = ( UnexpectedChunkException&& ) = default;
    inline UnexpectedChunkException& operator = ( const UnexpectedChunkException& ) = default;

    inline uint32 getExpectedChunkID( void ) const noexcept
    {
        return this->expectedChunkID;
    }

    inline uint32 getFoundChunkID( void ) const noexcept
    {
        return this->foundChunkID;
    }

private:
    uint32 expectedChunkID;
    uint32 foundChunkID;
};

//===============================================================
// *** Specialized exception helpers for context attachment ***
//===============================================================

#define RWEXCEPT_UNLOCALAINTERNERR_ARGUMENTS_DEFINE const wchar_t *uaie_descTokenTemplate, rwStaticString <char>&& uaie_reason
#define RWEXCEPT_UNLOCALAINTERNERR_ARGUMENTS_USE    uaie_descTokenTemplate, ::std::move( uaie_reason )
#define RWEXCEPT_UNLOCALWINTERNERR_ARGUMENTS_DEFINE const wchar_t *uwie_descTokenTemplate, rwStaticString <wchar_t>&& uwie_reason
#define RWEXCEPT_UNLOCALWINTERNERR_ARGUMENTS_USE    uwie_descTokenTemplate, ::std::move( uwie_reason )
#define RWEXCEPT_UNEXPCHUNK_ARGUMENTS_DEFINE        uint32 bapi_expectedChunkID, uint32 bapi_foundChunkID
#define RWEXCEPT_UNEXPCHUNK_ARGUMENTS_USE           bapi_expectedChunkID, bapi_foundChunkID

} // namespace rw

#endif //_RENDERWARE_ERRORSYS_EXT_HEADER_