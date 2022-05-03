/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.qol.rwlock.h
*  PURPOSE:     Read/Write-lock based QoL helpers
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_QOL_RWLOCK_
#define _EXECUTIVE_MANAGER_QOL_RWLOCK_

BEGIN_NATIVE_EXECUTIVE

// Forward declarations.
struct natRWLock;
struct natOptRWLock;

//===================================================================================
// *** Generic helpers for entering read/write locks using the lock pointers ***
//===================================================================================

namespace rwlockutil
{

template <typename rwLockType>
AINLINE void LockEnterRead( rwLockType *lockPtr )
{
    if constexpr ( std::is_same <typename std::remove_const <rwLockType>::type, natRWLock>::value || std::is_same <typename std::remove_const <rwLockType>::type, natOptRWLock>::value )
    {
        (*lockPtr)->EnterCriticalReadRegion();
    }
    else if constexpr ( std::is_same <rwLockType, CThreadReentrantReadWriteLock>::value )
    {
        lockPtr->LockRead();
    }
    else
    {
        lockPtr->EnterCriticalReadRegion();
    }
}

template <typename rwLockType>
AINLINE void LockLeaveRead( rwLockType *lockPtr )
{
    if constexpr ( std::is_same <typename std::remove_const <rwLockType>::type, natRWLock>::value || std::is_same <typename std::remove_const <rwLockType>::type, natOptRWLock>::value )
    {
        (*lockPtr)->LeaveCriticalReadRegion();
    }
    else if constexpr ( std::is_same <rwLockType, CThreadReentrantReadWriteLock>::value )
    {
        lockPtr->UnlockRead();
    }
    else
    {
        lockPtr->LeaveCriticalReadRegion();
    }
}

template <typename rwLockType>
AINLINE void LockEnterWrite( rwLockType *lockPtr )
{
    if constexpr ( std::is_same <typename std::remove_const <rwLockType>::type, natRWLock>::value || std::is_same <typename std::remove_const <rwLockType>::type, natOptRWLock>::value )
    {
        (*lockPtr)->EnterCriticalWriteRegion();
    }
    else if constexpr ( std::is_same <rwLockType, CThreadReentrantReadWriteLock>::value )
    {
        lockPtr->LockWrite();
    }
    else
    {
        lockPtr->EnterCriticalWriteRegion();
    }
}

template <typename rwLockType>
AINLINE void LockLeaveWrite( rwLockType *lockPtr )
{
    if constexpr ( std::is_same <typename std::remove_const <rwLockType>::type, natRWLock>::value || std::is_same <typename std::remove_const <rwLockType>::type, natOptRWLock>::value )
    {
        (*lockPtr)->LeaveCriticalWriteRegion();
    }
    else if constexpr ( std::is_same <rwLockType, CThreadReentrantReadWriteLock>::value )
    {
        lockPtr->UnlockWrite();
    }
    else
    {
        lockPtr->LeaveCriticalWriteRegion();
    }
}

} // namespacw rwlockutil

//===================================================================================
// *** Lock context helpers for exception safe and correct code region marking. ***
//===================================================================================

template <typename rwLockType = CReadWriteLock>
struct CReadWriteReadContext
{
    inline CReadWriteReadContext( rwLockType *theLock )
    {
#ifdef _DEBUG
        assert( theLock != nullptr );
#endif //_DEBUG

        rwlockutil::LockEnterRead( theLock );

        this->theLock = theLock;
    }
    inline CReadWriteReadContext( rwLockType& theLock )
    {
        rwlockutil::LockEnterRead( &theLock );

        this->theLock = &theLock;
    }
    inline CReadWriteReadContext( const CReadWriteReadContext& ) = delete;
    inline CReadWriteReadContext( CReadWriteReadContext&& ) = delete;

    inline ~CReadWriteReadContext( void )
    {
        rwlockutil::LockLeaveRead( this->theLock );
    }

    inline CReadWriteReadContext& operator = ( const CReadWriteReadContext& ) = delete;
    inline CReadWriteReadContext& operator = ( CReadWriteReadContext&& ) = delete;

private:
    rwLockType *theLock;
};

template <typename rwLockType = CReadWriteLock>
struct CReadWriteWriteContext
{
    inline CReadWriteWriteContext( rwLockType *theLock )
    {
#ifdef _DEBUG
        assert( theLock != nullptr );
#endif //_DEBUG

        rwlockutil::LockEnterWrite( theLock );

        this->theLock = theLock;
    }
    inline CReadWriteWriteContext( rwLockType& theLock )
    {
        rwlockutil::LockEnterWrite( &theLock );

        this->theLock = &theLock;
    }
    inline CReadWriteWriteContext( const CReadWriteWriteContext& ) = delete;
    inline CReadWriteWriteContext( CReadWriteWriteContext&& ) = delete;

    inline ~CReadWriteWriteContext( void )
    {
        rwlockutil::LockLeaveWrite( this->theLock );
    }

    inline CReadWriteWriteContext& operator = ( const CReadWriteWriteContext& ) = delete;
    inline CReadWriteWriteContext& operator = ( CReadWriteWriteContext&& ) = delete;

private:
    rwLockType *theLock;
};

// Variants of locking contexts that accept a NULL pointer.
template <typename rwLockType = CReadWriteLock>
struct CReadWriteReadContextSafe
{
    inline CReadWriteReadContextSafe( rwLockType *theLock )
    {
        if ( theLock )
        {
            rwlockutil::LockEnterRead( theLock );
        }

        this->theLock = theLock;
    }
    inline CReadWriteReadContextSafe( rwLockType& theLock )     // variant without nullptr possibility.
    {
        rwlockutil::LockEnterRead( &theLock );

        this->theLock = &theLock;
    }

    inline CReadWriteReadContextSafe( const CReadWriteReadContextSafe& ) = delete;  // rwlock type is not reentrant (unless otherwise specified)
    inline CReadWriteReadContextSafe( CReadWriteReadContextSafe&& right )
    {
        this->theLock = right.theLock;

        right.theLock = nullptr;
    }

    inline void Suspend( void )
    {
        if ( rwLockType *theLock = this->theLock )
        {
            rwlockutil::LockLeaveRead( theLock );

            this->theLock = nullptr;
        }
    }

    inline ~CReadWriteReadContextSafe( void )
    {
        this->Suspend();
    }

    inline CReadWriteReadContextSafe& operator = ( const CReadWriteReadContextSafe& ) = delete;
    inline CReadWriteReadContextSafe& operator = ( CReadWriteReadContextSafe&& right )
    {
        this->Suspend();

        this->theLock = right.theLock;

        right.theLock = nullptr;

        return *this;
    }

    inline rwLockType* GetCurrentLock( void )
    {
        return this->theLock;
    }

private:
    rwLockType *theLock;
};

template <typename rwLockType = CReadWriteLock>
struct CReadWriteWriteContextSafe
{
    inline CReadWriteWriteContextSafe( rwLockType *theLock )
    {
        if ( theLock )
        {
            rwlockutil::LockEnterWrite( theLock );
        }
        
        this->theLock = theLock;
    }
    inline CReadWriteWriteContextSafe( rwLockType& theLock )    // variant without nullptr possibility.
    {
        rwlockutil::LockEnterWrite( &theLock );

        this->theLock = &theLock;
    }

    inline CReadWriteWriteContextSafe( const CReadWriteWriteContextSafe& right ) = delete;
    inline CReadWriteWriteContextSafe( CReadWriteWriteContextSafe&& right )
    {
        this->theLock = right.theLock;

        right.theLock = nullptr;
    }

    inline void Suspend( void )
    {
        if ( rwLockType *theLock = this->theLock )
        {
            rwlockutil::LockLeaveWrite( theLock );

            this->theLock = nullptr;
        }
    }

    inline ~CReadWriteWriteContextSafe( void )
    {
        this->Suspend();
    }

    inline CReadWriteWriteContextSafe& operator = ( const CReadWriteWriteContextSafe& right ) = delete;
    inline CReadWriteWriteContextSafe& operator = ( CReadWriteWriteContextSafe&& right )
    {
        this->Suspend();

        this->theLock = right.theLock;

        right.theLock = nullptr;

        return *this;
    }

    inline rwLockType* GetCurrentLock( void )
    {
        return this->theLock;
    }

private:
    rwLockType *theLock;
};

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_QOL_RWLOCK_