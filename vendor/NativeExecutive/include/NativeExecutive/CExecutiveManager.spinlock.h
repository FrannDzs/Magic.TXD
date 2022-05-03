/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.spinlock.internal.h
*  PURPOSE:     Spin-lock implementation for low-level locking
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NAT_EXEC_SPIN_LOCK_OBJECT_HEADER_
#define _NAT_EXEC_SPIN_LOCK_OBJECT_HEADER_

#include <atomic>

#include <sdk/eirutils.h>

BEGIN_NATIVE_EXECUTIVE

// For use by low-level primitives in constant CPU time code regions.
// Use this primitive in better synchronization layers as stable foundation.
// But make sure that each region you use this in is constant CPU time and a small amount.
// This lock is very unfair because it relies on CPU instructions only and those
// do not allow for any queues of waiter-entries.
struct CSpinLock
{
    inline constexpr CSpinLock( void ) noexcept : isLockTaken( false )
    {
        return;
    }
    inline CSpinLock( const CSpinLock& ) = delete;
    inline CSpinLock( CSpinLock&& ) = delete;

    inline ~CSpinLock( void )
    {
#ifdef _DEBUG
        FATAL_ASSERT( this->isLockTaken == false );
#endif //_DEBUG
    }

    // We cannot more or copy a spin-lock because it absolutely requires
    // to stay on the same memory location.
    inline CSpinLock& operator = ( const CSpinLock& ) = delete;
    inline CSpinLock& operator = ( CSpinLock&& ) = delete;

    inline void lock( void ) noexcept
    {
        // Wait until the region of code is free.
        while ( true )
        {
            bool is_lock_taken = isLockTaken.exchange( true );

            if ( !is_lock_taken )
            {
                break;
            }
        }
    }

    inline bool tryLock( void ) noexcept
    {
        // Attempt to enter the lock.
        // Very important to have because sometimes you need to enter
        // two contexts intertwined, so the weaker entry must try-only.
        bool is_lock_taken = isLockTaken.exchange( true );

        return ( is_lock_taken == false );
    }

    inline void unlock( void ) noexcept
    {
        // Simply release the lock.
#ifdef _DEBUG
        bool wasLockTaken =
#endif //_DEBUG
        isLockTaken.exchange( false );

#ifdef _DEBUG
        FATAL_ASSERT( wasLockTaken == true );
#endif //_DEBUG
    }

    // After a state has finished mutating (safely), you know that every other execution expecting the previous state
    // has finished after this fence call.
    inline void fence( void ) noexcept
    {
        while ( isLockTaken );
    }

private:
    std::atomic <bool> isLockTaken;
};

END_NATIVE_EXECUTIVE

#endif //_NAT_EXEC_SPIN_LOCK_OBJECT_HEADER_