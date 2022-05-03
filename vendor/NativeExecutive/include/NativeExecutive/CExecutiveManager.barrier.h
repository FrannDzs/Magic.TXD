/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.barrier.h
*  PURPOSE:     Fence/barrier sync object header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_BARRIER_HEADER_
#define _NATEXEC_BARRIER_HEADER_

BEGIN_NATIVE_EXECUTIVE

// The purpose of a barrier object is to let a given amount of threads accumulate
// as waiters and then release them all-at-once once that count is reached.
struct CBarrier
{
    // Waits until the requested count of threads is satisfied.
    void Wait( void );
    // Wait for the specified amount of time for the requested amount of threads
    // to be satisfied. Returns true if the barrier was satisfied, false if
    // the time has elapsed without barrier trigger.
    bool WaitTimed( unsigned int time_ms );
};

END_NATIVE_EXECUTIVE

#endif //_NATEXEC_BARRIER_HEADER_