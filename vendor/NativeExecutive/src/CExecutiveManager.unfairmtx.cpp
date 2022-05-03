/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.unfairmtx.cpp
*  PURPOSE:     Cross-platform native unfair mutex implementation that relies on OS thread sheduler.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "internal/CExecutiveManager.unfairmtx.internal.h"

BEGIN_NATIVE_EXECUTIVE

void CUnfairMutex::lock( void ) noexcept
{
    CUnfairMutexImpl *nativeMutex = (CUnfairMutexImpl*)this;

    nativeMutex->lock();
}

void CUnfairMutex::unlock( void ) noexcept
{
    CUnfairMutexImpl *nativeMutex = (CUnfairMutexImpl*)this;

    nativeMutex->unlock();
}

bool CUnfairMutex::tryLock( void ) noexcept
{
    CUnfairMutexImpl *nativeMutex = (CUnfairMutexImpl*)this;

    return nativeMutex->tryLock();
}

bool CUnfairMutex::tryTimedLock( unsigned int time_ms ) noexcept
{
    CUnfairMutexImpl *nativeMutex = (CUnfairMutexImpl*)this;

    return nativeMutex->tryTimedLock( time_ms );
}

void CUnfairMutex::wait_lock( void ) noexcept
{
    CUnfairMutexImpl *nativeMutex = (CUnfairMutexImpl*)this;

    return nativeMutex->waitLock();
}

CUnfairMutex* CExecutiveManager::CreateUnfairMutex( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    CEvent *evtWaiter = nativeMan->CreateEvent();

    if ( !evtWaiter )
        return nullptr;
    
    try
    {
        return eir::dyn_new_struct <CUnfairMutexImpl> ( memAlloc, nullptr, evtWaiter );
    }
    catch( ... )
    {
        nativeMan->CloseEvent( evtWaiter );

        throw;
    }
}

void CExecutiveManager::CloseUnfairMutex( CUnfairMutex *mtx )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    CUnfairMutexImpl *nativeMutex = (CUnfairMutexImpl*)mtx;

    CEvent *evtWaiter = nativeMutex->get_event();
    
    // First destroy the mutex.
    eir::dyn_del_struct <CUnfairMutexImpl> ( memAlloc, nullptr, nativeMutex );

    // Clean-up the event.
    nativeMan->CloseEvent( evtWaiter );
}

size_t CExecutiveManager::GetUnfairMutexStructSize( void )
{
    return sizeof(CUnfairMutexImpl);
}

size_t CExecutiveManager::GetUnfairMutexAlignment( void )
{
    return alignof(CUnfairMutexImpl);
}

CUnfairMutex* CExecutiveManager::CreatePlacedUnfairMutex( void *mem )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    CEvent *evt = nativeMan->CreateEvent();

    if ( evt == nullptr )
    {
        return nullptr;
    }
    
    try
    {
        return new (mem) CUnfairMutexImpl( evt );
    }
    catch( ... )
    {
        nativeMan->CloseEvent( evt );

        throw;
    }
}

void CExecutiveManager::ClosePlacedUnfairMutex( CUnfairMutex *mtx )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    CUnfairMutexImpl *nativeMtx = (CUnfairMutexImpl*)mtx;

    CEvent *evtWaiter = nativeMtx->get_event();

    nativeMtx->~CUnfairMutexImpl();

    nativeMan->CloseEvent( evtWaiter );
}

END_NATIVE_EXECUTIVE