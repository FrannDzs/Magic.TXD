/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.cond.h
*  PURPOSE:     Hazard-safe conditional variable implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_CONDITIONAL_
#define _EXECUTIVE_MANAGER_CONDITIONAL_

#include "CExecutiveManager.qol.rwlock.h"

BEGIN_NATIVE_EXECUTIVE

struct CSpinLockContext;
struct CSpinLock;
struct CUnfairMutexContext;
class CUnfairMutex;
struct CReadWriteLock;

// Flood-gate style conditional variable.
// It comes with hazard-safety: if thread is asked to terminate then conditional variable will not wait.
struct CCondVar abstract
{
    void Wait( CReadWriteWriteContextSafe <>& ctxLock );
    void Wait( CSpinLockContext& ctxLock );
    void Wait( CUnfairMutexContext& ctxLock );
    // Returns true if the thread has been woken up by signal.
    // If the thread was woken up by time-out but the signal happened anyway then true is returned.
    bool WaitTimed( CReadWriteWriteContextSafe <>& ctxLock, unsigned int waitMS );
    bool WaitTimed( CSpinLockContext& ctxLock, unsigned int waitMS );
    bool WaitTimed( CUnfairMutexContext& ctxLock, unsigned int waitMS );
    // Methods that work directly on lock variables (you have to know what you are doing).
    // *** the rwlock must be taken in write-lock.
    void WaitByLock( CReadWriteLock *lock );
    void WaitByLock( CThreadReentrantReadWriteLock *lock );
    void WaitByLock( CSpinLock *lock );
    void WaitByLock( CUnfairMutex *lock );
    // *** the rwlock must be taken in write-lock.
    bool WaitTimedByLock( CReadWriteLock *lock, unsigned int waitMS );
    bool WaitTimedByLock( CThreadReentrantReadWriteLock *lock, unsigned int waitMS );
    bool WaitTimedByLock( CSpinLock *lock, unsigned int waitMS );
    bool WaitTimedByLock( CUnfairMutex *lock, unsigned int waitMS );
    // Wakes up all waiting threads and returns the amount of threads woken up.
    size_t Signal( void );
    // Wakes up at most maxWakeUpCount threads and returns the amount actually woken up.
    size_t SignalCount( size_t maxWakeUpCount );

    CExecutiveManager* GetManager( void );
};

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_CONDITIONAL_