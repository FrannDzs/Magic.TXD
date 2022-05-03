/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwprivate.locale.h
*  PURPOSE:     Translation support header.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_LOCALIZATION_PRIVATE_HEADER_
#define _RENDERWARE_LOCALIZATION_PRIVATE_HEADER_

namespace rw
{

rwStaticString <wchar_t> GetLanguageItem( EngineInterface *rwEngine, const wchar_t *key );

} // namespace rw

#endif //_RENDERWARE_LOCALIZATION_PRIVATE_HEADER_