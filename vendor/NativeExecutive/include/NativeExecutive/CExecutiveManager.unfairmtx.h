/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.unfairmtx.internal.h
*  PURPOSE:     Unfair mutex implementation that relies on OS thread sheduler.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NAT_EXEC_UNFAIR_MUTEX_HEADER_
#define _NAT_EXEC_UNFAIR_MUTEX_HEADER_

BEGIN_NATIVE_EXECUTIVE

// This mutex does not allocate memory during runtime, unlike rwlock.
// It is a thing because memory allocators require exactly this trait.
// Fairness is a high price to pay, like occassional memory allocation.
class CUnfairMutex abstract
{
public:
    void lock( void ) noexcept;
    bool tryLock( void ) noexcept;
    bool tryTimedLock( unsigned int time_ms ) noexcept;
    void unlock( void ) noexcept;
    void wait_lock( void ) noexcept;
};

END_NATIVE_EXECUTIVE

#endif //_NAT_EXEC_UNFAIR_MUTEX_HEADER_