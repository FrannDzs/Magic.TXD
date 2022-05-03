/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwprivate.warnings.h
*  PURPOSE:     RenderWare private global include file about the warning management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_PRIVATE_WARNINGSYS_
#define _RENDERWARE_PRIVATE_WARNINGSYS_

namespace rw
{

// Internal warning dispatcher class.
struct WarningHandler abstract
{
    virtual void OnWarningMessage( rwStaticString <wchar_t>&& theMessage ) = 0;
};

// Neat little helper function.
rwStaticString <wchar_t> GetObjectNicePrefixString( EngineInterface *rwEngine, const RwObject *relevantObject );

void GlobalPushWarningHandler( EngineInterface *engineInterface, WarningHandler *theHandler );
void GlobalPopWarningHandler( EngineInterface *engineInterface );

};

#endif //_RENDERWARE_PRIVATE_WARNINGSYS_