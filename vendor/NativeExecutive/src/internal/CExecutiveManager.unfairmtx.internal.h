/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.unfairmtx.internal.h
*  PURPOSE:     Cross-platform native unfair mutex implementation that relies on OS thread sheduler.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

// The beauty of this implementation is that:
// * no runtime memory allocation besides object-space
// * OS threads compete in a race
// And because we do not allocate memory, like ever, we are perfect candidates for
// the memory alloocator!

#ifndef _NAT_EXEC_UNFAIR_MUTEX_IMPLEMENTATION_HEADER_
#define _NAT_EXEC_UNFAIR_MUTEX_IMPLEMENTATION_HEADER_

#include "CExecutiveManager.unfairmtx.h"

#include "CExecutiveManager.event.h"
#include "CExecutiveManager.spinlock.internal.h"

#include <sdk/MemoryRaw.h>

#include <chrono>

BEGIN_NATIVE_EXECUTIVE

struct CUnfairMutexImpl : public CUnfairMutex
{
    inline CUnfairMutexImpl( CEvent *evtWaiter ) noexcept
    {
        this->is_mutex_taken = false;
        this->evtWaiter = evtWaiter;
        
        // At first we allow taking the mutex.
        evtWaiter->Set( false );
    }
    inline CUnfairMutexImpl( const CUnfairMutexImpl& ) = delete;
    inline CUnfairMutexImpl( CUnfairMutexImpl&& ) = delete;

    inline ~CUnfairMutexImpl( void )
    {
        // The lock must not be taken anymore if it is deleted.
        // Common-sense anyway.
        assert( this->is_mutex_taken == false );
    }

    inline CUnfairMutexImpl& operator = ( const CUnfairMutexImpl& ) = delete;
    inline CUnfairMutexImpl& operator = ( CUnfairMutexImpl&& ) = delete;

    inline void lock( void ) noexcept
    {
        // Wait until we can take this mutex.
        // We could spin sometimes so this is like a conditional variable.
        CEvent *evtWaiter = this->evtWaiter;

        while ( true )
        {
            evtWaiter->Wait();

            this->lockAtomic.lock();

            bool canTakeLock = ( this->is_mutex_taken == false );

            if ( canTakeLock )
            {
                // The unfair mutex is thread-safe because we do this under a lock.
                this->is_mutex_taken = true;

                evtWaiter->Set( true );
            }

            this->lockAtomic.unlock();

            if ( canTakeLock )
            {
                break;
            }
        }
    }

    inline bool tryLock( void ) noexcept
    {
        CSpinLockContext ctxLock( this->lockAtomic );

        if ( this->is_mutex_taken == false )
        {
            this->is_mutex_taken = true;

            this->evtWaiter->Set( true );

            return true;
        }

        return false;
    }

    inline bool tryTimedLock( unsigned int time_ms ) noexcept
    {
        try
        {
            using namespace std::chrono;

            time_point <high_resolution_clock, milliseconds> end_time =
                time_point_cast <milliseconds> ( high_resolution_clock::now() ) + milliseconds( time_ms );

            CEvent *evtWaiter = this->evtWaiter;

            while ( true )
            {
                time_point <high_resolution_clock, milliseconds> cur_time =
                    time_point_cast <milliseconds> ( high_resolution_clock::now() );

                auto waitMS = ( end_time - cur_time ).count();

                if ( waitMS <= 0 )
                {
                    return false;
                }

                evtWaiter->WaitTimed( TRANSFORM_INT_CLAMP <unsigned int> ( waitMS ) );

                CSpinLockContext ctx_lock( this->lockAtomic );

                if ( this->is_mutex_taken == false )
                {
                    this->is_mutex_taken = true;

                    evtWaiter->Set( true );

                    break;
                }
            }

            return true;
        }
        catch( ... )
        {
            return false;
        }
    }

    inline void unlock( void ) noexcept
    {
        // It is important to take this lock so we prevent putting threads into
        // infinite spin-lock state.
        this->lockAtomic.lock();

        this->is_mutex_taken = false;

        this->evtWaiter->Set( false );

        this->lockAtomic.unlock();
    }

    inline void waitLock( void ) noexcept
    {
        // The lock is not taken anymore (for at least the tiniest time-fraction)
        // if the waiter event is released.
        this->evtWaiter->Wait();
    }

    inline CEvent* get_event( void )
    {
        return this->evtWaiter;
    }

private:
    // The waiting fence that every thread is waiting on, discarding timely-arrival advantage,
    // hence being an unfair lock.
    CEvent *evtWaiter;

    // If true then a thread is inside the lock.
    bool is_mutex_taken;

    // We need a spinlock to make the aquisition-of-mutex and release-of-mutex atomic.
    CSpinLockImpl lockAtomic;
};

END_NATIVE_EXECUTIVE

#endif //_NAT_EXEC_UNFAIR_MUTEX_IMPLEMENTATION_HEADER_