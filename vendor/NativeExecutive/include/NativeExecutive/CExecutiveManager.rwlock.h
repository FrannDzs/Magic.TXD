/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.rwlock.h
*  PURPOSE:     Read/Write lock synchronization object
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATIVE_EXECUTIVE_READ_WRITE_LOCK_
#define _NATIVE_EXECUTIVE_READ_WRITE_LOCK_

#include <type_traits>

BEGIN_NATIVE_EXECUTIVE

/*
    Synchronization object - the "Read/Write" lock

    Use this sync object if you have a data structure that requires consistency in a multi-threaded environment.
    Just like any other sync object it prevents instruction reordering where it changes the designed functionality.
    But the speciality of this object is that it allows two access modes.

    In typical data structure development, read operations do not change the state of an object. This allows
    multiple threads to run concurrently and still keep the logic of the data intact. This assumption is easily
    warded off if the data structure keeps shadow data for optimization purposes (mutable variables).

    Then there is the writing mode. In this mode threads want exclusive access to a data structure, as concurrent
    modification on a data structure is a daunting task and most often is impossible to solve fast and clean.

    By using this object to mark critical read and write regions in your code, you easily make it thread-safe.
    Thread-safety is the future, as silicon has reached its single-threaded performance peak.

    Please make sure that you use this object in an exception-safe way to prevent dead-locks! This structure does
    not support recursive acquisition, so be careful how you do things!
*/
struct CReadWriteLock abstract
{
    // Shared access to data structures.
    void EnterCriticalReadRegion( void );
    void LeaveCriticalReadRegion( void );

    // Exclusive access to data structures.
    void EnterCriticalWriteRegion( void );
    void LeaveCriticalWriteRegion( void );

    // Attempting to enter the lock while preventing a wait scenario.
    bool TryEnterCriticalReadRegion( void );
    bool TryEnterCriticalWriteRegion( void );

    // Attempting to enter the lock under timeout.
    // Returns false if not supported (support can be queried by manager).
    bool TryTimedEnterCriticalReadRegion( unsigned int time_ms );
    bool TryTimedEnterCriticalWriteRegion( unsigned int time_ms );
};

/*
    Synchronization object - the fair "Read/Write" lock

    This synchronization object is same as the regular read/write lock but with an additional promise:
    threads that enter this lock leave it in the same order as they entered it. Thus the lock is fair
    in a sense that it does not forget the order of timely arrivals.

    I admit that the inclusion of this lock type was promoted by the availability of an internal
    implementation.
*/
struct CFairReadWriteLock abstract
{
    // Shared access to data structures.
    void EnterCriticalReadRegion( void );
    void LeaveCriticalReadRegion( void );

    // Exclusive access to data structures.
    void EnterCriticalWriteRegion( void );
    void LeaveCriticalWriteRegion( void );

    // Attempting to enter the lock while preventing a wait scenario.
    bool TryEnterCriticalReadRegion( void );
    bool TryEnterCriticalWriteRegion( void );

    // Attempting to enter the lock under timeout.
    bool TryTimedEnterCriticalReadRegion( unsigned int time_ms );
    bool TryTimedEnterCriticalWriteRegion( unsigned int time_ms );
};

/*
    Synchronization object - the reentrant "Read/Write" lock

    This is a variant of CReadWriteLock but it is reentrant. This means that write contexts can be entered multiple times
    on one thread, leaving them the same amount of times to unlock again.

    Due to the reentrance feature this lock is slower than CReadWriteLock.
    It uses a context structure to remember recursive accesses.
*/
struct CReentrantReadWriteContext abstract
{
    unsigned long GetReadContextCount( void ) const;
    unsigned long GetWriteContextCount( void ) const;
};

struct CReentrantReadWriteLock abstract
{
    // Shared access to data structures.
    void LockRead( CReentrantReadWriteContext *ctx );
    void UnlockRead( CReentrantReadWriteContext *ctx );

    // Exclusive access to data structures.
    void LockWrite( CReentrantReadWriteContext *ctx );
    void UnlockWrite( CReentrantReadWriteContext *ctx );

    // Attempting to enter this lock without waiting.
    bool TryLockRead( CReentrantReadWriteContext *ctx );
    bool TryLockWrite( CReentrantReadWriteContext *ctx );

    // Attempting to enter this lock under timeout.
    bool TryTimedLockRead( CReentrantReadWriteContext *ctx, unsigned int time_ms );
    bool TryTimedLockWrite( CReentrantReadWriteContext *ctx, unsigned int time_ms );
};

/*
    Helper of the reentrant Read/Write lock which automatically uses the current thread context.
    Used quite often so we provide this out-of-the-box.
    You can query the thread-context manually if you want to use the generic lock instead.
*/
struct CThreadReentrantReadWriteLock abstract
{
    void LockRead( void );
    void UnlockRead( void );

    void LockWrite( void );
    void UnlockWrite( void );

    bool TryLockRead( void );
    bool TryLockWrite( void );

    bool TryTimedLockRead( unsigned int time_ms );
    bool TryTimedLockWrite( unsigned int time_ms );
};

END_NATIVE_EXECUTIVE

#endif //_NATIVE_EXECUTIVE_READ_WRITE_LOCK_