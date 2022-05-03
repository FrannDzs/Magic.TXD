/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.spinlock.cpp
*  PURPOSE:     Cross-platform native spin-lock implementation for low-level locking
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "internal/CExecutiveManager.spinlock.internal.h"

BEGIN_NATIVE_EXECUTIVE

#if 0

// Create the internal redirect for the public API.
void CSpinLock::lock( void )
{
    CSpinLockImpl *nativeLock = (CSpinLockImpl*)this;

    nativeLock->lock();
}

bool CSpinLock::tryLock( void )
{
    CSpinLockImpl *nativeLock = (CSpinLockImpl*)this;

    return nativeLock->tryLock();
}

void CSpinLock::unlock( void )
{
    CSpinLockImpl *nativeLock = (CSpinLockImpl*)this;

    nativeLock->unlock();
}

#endif //0

// Public creation API.
CSpinLock* CExecutiveManager::CreateSpinLock( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    return eir::dyn_new_struct <CSpinLockImpl> ( memAlloc, nullptr );
}

void CExecutiveManager::CloseSpinLock( CSpinLock *theLock )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    eir::dyn_del_struct <CSpinLockImpl> ( memAlloc, nullptr, (CSpinLockImpl*)theLock );
}

END_NATIVE_EXECUTIVE