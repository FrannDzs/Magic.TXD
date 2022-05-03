/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.lateinit.h
*  PURPOSE:     Initialization after engine construction for data-loading.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_LATE_INITIALIZATION_
#define _RENDERWARE_LATE_INITIALIZATION_

namespace rw
{

// Callback types for late initialization.
typedef void (*lateInitializer_t)( Interface *rwEngine, void *ud );
typedef void (*lateInitializerCleanup_t)( Interface *rwEngine, void *ud ) noexcept;

// Late initialization for the currently constructing engine.
void RegisterLateInitializer( Interface *rwEngine, lateInitializer_t init_cb, void *ud, lateInitializerCleanup_t cleanup_cb );

};

#endif //_RENDERWARE_LATE_INITIALIZATION_