/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.sem.internal.h
*  PURPOSE:     Cross-platform native semaphore implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_SEMAPHORE_INTERNAL_HEADER_
#define _NATEXEC_SEMAPHORE_INTERNAL_HEADER_

#include "CExecutiveManager.sem.h"

#include "CExecutiveManager.spinlock.internal.h"
#include "CExecutiveManager.event.internal.h"

#include <sdk/rwlist.hpp>

BEGIN_NATIVE_EXECUTIVE

struct CSemaphoreImpl : public CSemaphore
{
    inline CSemaphoreImpl( CEvent *evtWaiter ) : cur_count( 0 ), evtWaiter( evtWaiter )
    {
        // At first we are 0 so nobody can decrement from us.
        evtWaiter->Set( true );
    }

    size_t cur_count;
    CEvent *evtWaiter;
    CSpinLockImpl lockAtomic;
};

END_NATIVE_EXECUTIVE

#endif //_NATEXEC_SEMAPHORE_INTERNAL_HEADER_