/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.barrier.internal.h
*  PURPOSE:     Fence/barrier sync object header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_BARRIER_IMPLEMENTATION_HEADER_
#define _NATEXEC_BARRIER_IMPLEMENTATION_HEADER_

// The name barrier was chosen over fence because the need for this synchronization
// object was first casted by the pthreads compatibility requirement.

#include "CExecutiveManager.barrier.h"

#include "CExecutiveManager.spinlock.internal.h"

#include <sdk/rwlist.hpp>

#include <assert.h>

BEGIN_NATIVE_EXECUTIVE

// The per-thread data.
struct _barrier_ptd;

struct CBarrierImpl : public CBarrier
{
    friend struct CBarrier;

    inline CBarrierImpl( CExecutiveManagerNative *nativeMan, unsigned int req_release_count )
    {
        this->nativeMan = nativeMan;
        this->waiting_count = 0;
        this->req_release_count = req_release_count;
    }
    CBarrierImpl( const CBarrierImpl& ) = delete;
    CBarrierImpl( CBarrierImpl&& ) = delete;

    ~CBarrierImpl( void )
    {
        // Make sure nobody is waiting on us if we get destroyed.
        assert( this->waiting_count == 0 );
    }

    // Methods are available through the public implementation.
    
    CExecutiveManagerNative *nativeMan;

    CSpinLockImpl lock_atomic;

    unsigned int waiting_count;
    unsigned int req_release_count;

    RwList <_barrier_ptd> list_waiting;
};

END_NATIVE_EXECUTIVE

#endif //_NATEXEC_BARRIER_IMPLEMENTATION_HEADER_