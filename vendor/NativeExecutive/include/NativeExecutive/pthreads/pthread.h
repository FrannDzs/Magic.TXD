/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/pthread.h
*  PURPOSE:     POSIX threads implementation header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef NATEXEC_PTHREADS_HEADER
#define NATEXEC_PTHREADS_HEADER

// For size_t.
#include <stddef.h>

// This header aims to be compatible with possible system headers (example for Linux).
// But you can include it to be sure that this header is available on all supported
// NativeExecutive platforms.

// To keep ABI compatibility with other implementations we definitely
// need to create headers for each known and supported implementation, like GLIBC.
// Then the structures metrics (size, used members, etc) have to be statically
// asserted in unittest files. This is possible because of a pthread mandate to
// have the struct allocatable by the runtime that use the API.

// Compatibility target: Linux GLIBC.

#ifdef __linux__
// We do want to conform to GLIBC ABI.
#include "internal/pthread.glibc.h"
typedef __natexec_glibc_pthread_t _natexec_pthread_t;
typedef __natexec_glibc_pthread_mutexattr_t _natexec_pthread_mutexattr_t;
typedef __natexec_glibc_pthread_condattr_t _natexec_pthread_condattr_t;
typedef __natexec_glibc_pthread_key_t _natexec_pthread_key_t;
typedef __natexec_glibc_pthread_once_t _natexec_pthread_once_t;
typedef __natexec_glibc_pthread_attr_t _natexec_pthread_attr_t;
typedef __natexec_glibc_pthread_mutex_t _natexec_pthread_mutex_t;
typedef __natexec_glibc_pthread_cond_t _natexec_pthread_cond_t;
typedef __natexec_glibc_pthread_rwlock_t _natexec_pthread_rwlock_t;
typedef __natexec_glibc_pthread_rwlockattr_t _natexec_pthread_rwlockattr_t;
typedef __natexec_glibc_pthread_spinlock_t _natexec_pthread_spinlock_t;
typedef __natexec_glibc_pthread_barrier_t _natexec_pthread_barrier_t;
typedef __natexec_glibc_pthread_barrierattr_t _natexec_pthread_barrierattr_t;
typedef __natexec_glibc_pthread_sem_t _natexec_sem_t;

typedef __natexec_glibc_pthread_timespec_t timespec;
typedef __natexec_glibc_pthread_clockid_t clockid_t;
typedef __natexec_glibc_pthread_sched_param_t sched_param;

#define _NATEXEC_PTHREAD_INHERIT_SCHED       __NATEXEC_GLIBC_PTHREAD_INHERIT_SCHED
#define _NATEXEC_PTHREAD_EXPLICIT_SCHED      __NATEXEC_GLIBC_PTHREAD_EXPLICIT_SCHED

#define _NATEXEC_SCHED_FIFO                  __NATEXEC_GLIBC_PTHREAD_SCHED_FIFO
#define _NATEXEC_SCHED_RR                    __NATEXEC_GLIBC_PTHREAD_SCHED_RR
#define _NATEXEC_SCHED_OTHER                 __NATEXEC_GLIBC_PTHREAD_SCHED_OTHER

#define _NATEXEC_PTHREAD_SCOPE_SYSTEM        __NATEXEC_GLIBC_PTHREAD_SCOPE_SYSTEM
#define _NATEXEC_PTHREAD_SCOPE_PROCESS       __NATEXEC_GLIBC_PTHREAD_SCOPE_PROCESS

#define _NATEXEC_PTHREAD_CREATE_JOINABLE     __NATEXEC_GLIBC_PTHREAD_CREATE_JOINABLE
#define _NATEXEC_PTHREAD_CREATE_DETACHED     __NATEXEC_GLIBC_PTHREAD_CREATE_DETACHED

#define _NATEXEC_PTHREAD_MUTEX_TIMED_NP      __NATEXEC_GLIBC_PTHREAD_MUTEX_TIMED_NP
#define _NATEXEC_PTHREAD_MUTEX_RECURSIVE_NP  __NATEXEC_GLIBC_PTHREAD_MUTEX_RECURSIVE_NP
#define _NATEXEC_PTHREAD_MUTEX_ERRORCHECK_NP __NATEXEC_GLIBC_PTHREAD_MUTEX_ERRORCHECK_NP
#define _NATEXEC_PTHREAD_MUTEX_ADAPTIVE_NP   __NATEXEC_GLIBC_PTHREAD_MUTEX_ADAPTIVE_NP
#define _NATEXEC_PTHREAD_MUTEX_NORMAL        __NATEXEC_GLIBC_PTHREAD_MUTEX_NORMAL
#define _NATEXEC_PTHREAD_MUTEX_RECURSIVE     __NATEXEC_GLIBC_PTHREAD_MUTEX_RECURSIVE
#define _NATEXEC_PTHREAD_MUTEX_ERRORCHECK    __NATEXEC_GLIBC_PTHREAD_MUTEX_ERRORCHECK
#define _NATEXEC_PTHREAD_MUTEX_DEFAULT       __NATEXEC_GLIBC_PTHREAD_MUTEX_DEFAULT
#define _NATEXEC_PTHREAD_MUTEX_FAST_NP       __NATEXEC_GLIBC_PTHREAD_MUTEX_FAST_NP
#define _NATEXEC_PTHREAD_MUTEX_STALLED       __NATEXEC_GLIBC_PTHREAD_MUTEX_STALLED
#define _NATEXEC_PTHREAD_MUTEX_STALLED_NP    __NATEXEC_GLIBC_PTHREAD_MUTEX_STALLED_NP
#define _NATEXEC_PTHREAD_MUTEX_ROBUST        __NATEXEC_GLIBC_PTHREAD_MUTEX_ROBUST
#define _NATEXEC_PTHREAD_MUTEX_ROBUST_NP     __NATEXEC_GLIBC_PTHREAD_MUTEX_ROBUST_NP

#define _NATEXEC_PTHREAD_PRIO_NONE           __NATEXEC_GLIBC_PTHREAD_PRIO_NONE
#define _NATEXEC_PTHREAD_PRIO_INHERIT        __NATEXEC_GLIBC_PTHREAD_PRIO_INHERIT
#define _NATEXEC_PTHREAD_PRIO_PROTECT        __NATEXEC_GLIBC_PTHREAD_PRIO_PROTECT

#define _NATEXEC_PTHREAD_PROCESS_PRIVATE     __NATEXEC_GLIBC_PTHREAD_PROCESS_PRIVATE
#define _NATEXEC_PTHREAD_PROCESS_SHARED      __NATEXEC_GLIBC_PTHREAD_PROCESS_SHARED

#define _NATEXEC_PTHREAD_CANCEL_ENABLE       __NATEXEC_GLIBC_PTHREAD_CANCEL_ENABLE
#define _NATEXEC_PTHREAD_CANCEL_DISABLE      __NATEXEC_GLIBC_PTHREAD_CANCEL_DISABLE
#define _NATEXEC_PTHREAD_CANCELED            __NATEXEC_GLIBC_PTHREAD_CANCELED

#define _NATEXEC_PTHREAD_RWLOCK_PREFER_READER_NP                 __NATEXEC_GLIBC_PTHREAD_RWLOCK_PREFER_READER_NP
#define _NATEXEC_PTHREAD_RWLOCK_PREFER_WRITER_NP                 __NATEXEC_GLIBC_PTHREAD_RWLOCK_PREFER_WRITER_NP
#define _NATEXEC_PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP    __NATEXEC_GLIBC_PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP
#define _NATEXEC_PTHREAD_RWLOCK_DEFAULT_NP                       __NATEXEC_GLIBC_PTHREAD_RWLOCK_DEFAULT_NP

#define _NATEXEC_PTHREAD_CANCEL_DEFERRED     __NATEXEC_GLIBC_PTHREAD_CANCEL_DEFERRED
#define _NATEXEC_PTHREAD_CANCEL_ASYNCHRONOUS __NATEXEC_GLIBC_PTHREAD_CANCEL_ASYNCHRONOUS

#define _NATEXEC_PTHREAD_ONCE_INIT           __NATEXEC_GLIBC_PTHREAD_ONCE_INIT

#define _NATEXEC_SEM_FAILED                  __NATEXEC_GLIBC_PTHREAD_SEM_FAILED

#define _NATEXEC_PTHREAD_COND_INITIALIZER    __NATEXEC_GLIBC_PTHREAD_COND_INITIALIZER
#define _NATEXEC_PTHREAD_RWLOCK_INITIALIZER  __NATEXEC_GLIBC_PTHREAD_RWLOCK_INITIALIZER

#define _NATEXEC_PTHREAD_MUTEX_INITIALIZER                  __NATEXEC_GLIBC_PTHREAD_MUTEX_INITIALIZER
#define _NATEXEC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP     __NATEXEC_GLIBC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define _NATEXEC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP    __NATEXEC_GLIBC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#define _NATEXEC_PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP      __NATEXEC_GLIBC_PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP

#define _NATEXEC_PTHREAD_GET_STATIC_MUTEX_KIND              __NATEXEC_GLIBC_PTHREAD_GET_STATIC_MUTEX_KIND
#else
// On platforms that have no system-wide definition of pthreads we
// can just include our own structures.
#include "internal/pthread.native.h"
typedef __natexec_native_pthread_attr_t _natexec_pthread_attr_t;
typedef __natexec_native_pthread_t _natexec_pthread_t;
typedef __natexec_native_pthread_mutexattr_t _natexec_pthread_mutexattr_t;
typedef __natexec_native_pthread_mutex_t _natexec_pthread_mutex_t;
typedef __natexec_native_pthread_condattr_t _natexec_pthread_condattr_t;
typedef __natexec_native_pthread_cond_t _natexec_pthread_cond_t;
typedef __natexec_native_pthread_spinlock_t _natexec_pthread_spinlock_t;
typedef __natexec_native_pthread_rwlockattr_t _natexec_pthread_rwlockattr_t;
typedef __natexec_native_pthread_rwlock_t _natexec_pthread_rwlock_t;
typedef __natexec_native_pthread_barrierattr_t _natexec_pthread_barrierattr_t;
typedef __natexec_native_pthread_barrier_t _natexec_pthread_barrier_t;
typedef __natexec_native_pthread_key_t _natexec_pthread_key_t;
typedef __natexec_native_pthread_once_t _natexec_pthread_once_t;
typedef __natexec_native_pthread_sem_t _natexec_sem_t;

typedef __natexec_native_pthread_timespec_t timespec;
typedef __natexec_native_pthread_clockid_t clockid_t;
typedef __natexec_native_pthread_sched_param_t sched_param;

#define _NATEXEC_PTHREAD_INHERIT_SCHED       __NATEXEC_NATIVE_PTHREAD_INHERIT_SCHED
#define _NATEXEC_PTHREAD_EXPLICIT_SCHED      __NATEXEC_NATIVE_PTHREAD_EXPLICIT_SCHED

#define _NATEXEC_SCHED_FIFO                  __NATEXEC_NATIVE_PTHREAD_SCHED_FIFO
#define _NATEXEC_SCHED_RR                    __NATEXEC_NATIVE_PTHREAD_SCHED_RR
#define _NATEXEC_SCHED_OTHER                 __NATEXEC_NATIVE_PTHREAD_SCHED_OTHER

#define _NATEXEC_PTHREAD_SCOPE_SYSTEM        __NATEXEC_NATIVE_PTHREAD_SCOPE_SYSTEM
#define _NATEXEC_PTHREAD_SCOPE_PROCESS       __NATEXEC_NATIVE_PTHREAD_SCOPE_PROCESS

#define _NATEXEC_PTHREAD_CREATE_DETACHED     __NATEXEC_NATIVE_PTHREAD_CREATE_DETACHED
#define _NATEXEC_PTHREAD_CREATE_JOINABLE     __NATEXEC_NATIVE_PTHREAD_CREATE_JOINABLE

#define _NATEXEC_PTHREAD_MUTEX_TIMED_NP      __NATEXEC_NATIVE_PTHREAD_MUTEX_TIMED_NP
#define _NATEXEC_PTHREAD_MUTEX_RECURSIVE_NP  __NATEXEC_NATIVE_PTHREAD_MUTEX_RECURSIVE_NP
#define _NATEXEC_PTHREAD_MUTEX_ERRORCHECK_NP __NATEXEC_NATIVE_PTHREAD_MUTEX_ERRORCHECK_NP
#define _NATEXEC_PTHREAD_MUTEX_ADAPTIVE_NP   __NATEXEC_NATIVE_PTHREAD_MUTEX_ADAPTIVE_NP
#define _NATEXEC_PTHREAD_MUTEX_NORMAL        __NATEXEC_NATIVE_PTHREAD_MUTEX_NORMAL
#define _NATEXEC_PTHREAD_MUTEX_RECURSIVE     __NATEXEC_NATIVE_PTHREAD_MUTEX_RECURSIVE
#define _NATEXEC_PTHREAD_MUTEX_ERRORCHECK    __NATEXEC_NATIVE_PTHREAD_MUTEX_ERRORCHECK
#define _NATEXEC_PTHREAD_MUTEX_DEFAULT       __NATEXEC_NATIVE_PTHREAD_MUTEX_DEFAULT
#define _NATEXEC_PTHREAD_MUTEX_FAST_NP       __NATEXEC_NATIVE_PTHREAD_MUTEX_FAST_NP
#define _NATEXEC_PTHREAD_MUTEX_STALLED       __NATEXEC_NATIVE_PTHREAD_MUTEX_STALLED
#define _NATEXEC_PTHREAD_MUTEX_STALLED_NP    __NATEXEC_NATIVE_PTHREAD_MUTEX_STALLED_NP
#define _NATEXEC_PTHREAD_MUTEX_ROBUST        __NATEXEC_NATIVE_PTHREAD_MUTEX_ROBUST
#define _NATEXEC_PTHREAD_MUTEX_ROBUST_NP     __NATEXEC_NATIVE_PTHREAD_MUTEX_ROBUST_NP

#define _NATEXEC_PTHREAD_PRIO_NONE           __NATEXEC_NATIVE_PTHREAD_PRIO_NONE
#define _NATEXEC_PTHREAD_PRIO_INHERIT        __NATEXEC_NATIVE_PTHREAD_PRIO_INHERIT
#define _NATEXEC_PTHREAD_PRIO_PROTECT        __NATEXEC_NATIVE_PTHREAD_PRIO_PROTECT

#define _NATEXEC_PTHREAD_PROCESS_PRIVATE     __NATEXEC_NATIVE_PTHREAD_PROCESS_PRIVATE
#define _NATEXEC_PTHREAD_PROCESS_SHARED      __NATEXEC_NATIVE_PTHREAD_PROCESS_SHARED

#define _NATEXEC_PTHREAD_CANCEL_DISABLE      __NATEXEC_NATIVE_PTHREAD_CANCEL_DISABLE
#define _NATEXEC_PTHREAD_CANCEL_ENABLE       __NATEXEC_NATIVE_PTHREAD_CANCEL_ENABLE
#define _NATEXEC_PTHREAD_CANCELED            __NATEXEC_NATIVE_PTHREAD_CANCELED

#define _NATEXEC_PTHREAD_RWLOCK_PREFER_READER_NP                 __NATEXEC_NATIVE_PTHREAD_RWLOCK_PREFER_READER_NP
#define _NATEXEC_PTHREAD_RWLOCK_PREFER_WRITER_NP                 __NATEXEC_NATIVE_PTHREAD_RWLOCK_PREFER_WRITER_NP
#define _NATEXEC_PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP    __NATEXEC_NATIVE_PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP
#define _NATEXEC_PTHREAD_RWLOCK_DEFAULT_NP                       __NATEXEC_NATIVE_PTHREAD_RWLOCK_DEFAULT_NP

#define _NATEXEC_PTHREAD_CANCEL_DEFERRED     __NATEXEC_NATIVE_PTHREAD_CANCEL_DEFERRED
#define _NATEXEC_PTHREAD_CANCEL_ASYNCHRONOUS __NATEXEC_NATIVE_PTHREAD_CANCEL_ASYNCHRONOUS

#define _NATEXEC_PTHREAD_ONCE_INIT           __NATEXEC_NATIVE_PTHREAD_ONCE_INIT

#define _NATEXEC_SEM_FAILED                  __NATEXEC_NATIVE_PTHREAD_SEM_FAILED

#define _NATEXEC_PTHREAD_COND_INITIALIZER    __NATEXEC_NATIVE_PTHREAD_COND_INITIALIZER
#define _NATEXEC_PTHREAD_RWLOCK_INITIALIZER  __NATEXEC_NATIVE_PTHREAD_RWLOCK_INITIALIZER

#define _NATEXEC_PTHREAD_MUTEX_INITIALIZER                  __NATEXEC_NATIVE_PTHREAD_MUTEX_INITIALIZER
#define _NATEXEC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP     __NATEXEC_NATIVE_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define _NATEXEC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP    __NATEXEC_NATIVE_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#define _NATEXEC_PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP      __NATEXEC_NATIVE_PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP

#define _NATEXEC_PTHREAD_GET_STATIC_MUTEX_KIND              __NATEXEC_NATIVE_PTHREAD_GET_STATIC_MUTEX_KIND
#endif //CROSS PLATFORM INCLUDE

#ifndef _PTHREAD_H

#ifdef __linux__
// Hack to prevent the system pthreads headers from being included.
// Our own copy of the pthread.h header should entirely suffice.
#define _PTHREAD_H
#define _BITS_PTHREADTYPES_COMMON_H
#define _SEMAPHORE_H
#endif //__linux__

typedef _natexec_pthread_attr_t pthread_attr_t;
typedef _natexec_pthread_t pthread_t;
typedef _natexec_pthread_mutexattr_t pthread_mutexattr_t;
typedef _natexec_pthread_mutex_t pthread_mutex_t;
typedef _natexec_pthread_condattr_t pthread_condattr_t;
typedef _natexec_pthread_cond_t pthread_cond_t;
typedef _natexec_pthread_spinlock_t pthread_spinlock_t;
typedef _natexec_pthread_rwlockattr_t pthread_rwlockattr_t;
typedef _natexec_pthread_rwlock_t pthread_rwlock_t;
typedef _natexec_pthread_barrierattr_t pthread_barrierattr_t;
typedef _natexec_pthread_barrier_t pthread_barrier_t;
typedef _natexec_pthread_key_t pthread_key_t;
typedef _natexec_pthread_once_t pthread_once_t;
typedef _natexec_sem_t sem_t;

#define PTHREAD_INHERIT_SCHED       _NATEXEC_PTHREAD_INHERIT_SCHED
#define PTHREAD_EXPLICIT_SCHED      _NATEXEC_PTHREAD_EXPLICIT_SCHED

#define SCHED_FIFO                  _NATEXEC_SCHED_FIFO
#define SCHED_RR                    _NATEXEC_SCHED_RR
#define SCHED_OTHER                 _NATEXEC_SCHED_OTHER

#define PTHREAD_SCOPE_SYSTEM        _NATEXEC_PTHREAD_SCOPE_SYSTEM
#define PTHREAD_SCOPE_PROCESS       _NATEXEC_PTHREAD_SCOPE_PROCESS

#define PTHREAD_CREATE_DETACHED     _NATEXEC_PTHREAD_CREATE_DETACHED
#define PTHREAD_CREATE_JOINABLE     _NATEXEC_PTHREAD_CREATE_JOINABLE

#define PTHREAD_MUTEX_TIMED_NP      _NATEXEC_PTHREAD_MUTEX_TIMED_NP
#define PTHREAD_MUTEX_RECURSIVE_NP  _NATEXEC_PTHREAD_MUTEX_RECURSIVE_NP
#define PTHREAD_MUTEX_ERRORCHECK_NP _NATEXEC_PTHREAD_MUTEX_ERRORCHECK_NP
#define PTHREAD_MUTEX_ADAPTIVE_NP   _NATEXEC_PTHREAD_MUTEX_ADAPTIVE_NP
#define PTHREAD_MUTEX_NORMAL        _NATEXEC_PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_RECURSIVE     _NATEXEC_PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_ERRORCHECK    _NATEXEC_PTHREAD_MUTEX_ERRORCHECK
#define PTHREAD_MUTEX_DEFAULT       _NATEXEC_PTHREAD_MUTEX_DEFAULT
#define PTHREAD_MUTEX_FAST_NP       _NATEXEC_PTHREAD_MUTEX_FAST_NP
#define PTHREAD_MUTEX_STALLED       _NATEXEC_PTHREAD_MUTEX_STALLED
#define PTHREAD_MUTEX_STALLED_NP    _NATEXEC_PTHREAD_MUTEX_STALLED_NP
#define PTHREAD_MUTEX_ROBUST        _NATEXEC_PTHREAD_MUTEX_ROBUST
#define PTHREAD_MUTEX_ROBUST_NP     _NATEXEC_PTHREAD_MUTEX_ROBUST_NP

#define PTHREAD_PRIO_NONE           _NATEXEC_PTHREAD_PRIO_NONE
#define PTHREAD_PRIO_INHERIT        _NATEXEC_PTHREAD_PRIO_INHERIT
#define PTHREAD_PRIO_PROTECT        _NATEXEC_PTHREAD_PRIO_PROTECT

#define PTHREAD_PROCESS_PRIVATE     _NATEXEC_PTHREAD_PROCESS_PRIVATE
#define PTHREAD_PROCESS_SHARED      _NATEXEC_PTHREAD_PROCESS_SHARED

#define PTHREAD_CANCEL_DISABLE      _NATEXEC_PTHREAD_CANCEL_DISABLE
#define PTHREAD_CANCEL_ENABLE       _NATEXEC_PTHREAD_CANCEL_ENABLE
#define PTHREAD_CANCELED            _NATEXEC_PTHREAD_CANCELED

#define PTHREAD_RWLOCK_PREFER_READER_NP                 _NATEXEC_PTHREAD_RWLOCK_PREFER_READER_NP
#define PTHREAD_RWLOCK_PREFER_WRITER_NP                 _NATEXEC_PTHREAD_RWLOCK_PREFER_WRITER_NP
#define PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP    _NATEXEC_PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP
#define PTHREAD_RWLOCK_DEFAULT_NP                       _NATEXEC_PTHREAD_RWLOCK_DEFAULT_NP

#define PTHREAD_CANCEL_DEFERRED     _NATEXEC_PTHREAD_CANCEL_DEFERRED
#define PTHREAD_CANCEL_ASYNCHRONOUS _NATEXEC_PTHREAD_CANCEL_ASYNCHRONOUS

#define PTHREAD_ONCE_INIT           _NATEXEC_PTHREAD_ONCE_INIT

#define SEM_FAILED                  _NATEXEC_SEM_FAILED

#define PTHREAD_COND_INITIALIZER    _NATEXEC_PTHREAD_COND_INITIALIZER
#define PTHREAD_RWLOCK_INITIALIZER  _NATEXEC_PTHREAD_RWLOCK_INITIALIZER

#define PTHREAD_MUTEX_INITIALIZER                   _NATEXEC_PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP      _NATEXEC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP     _NATEXEC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#define PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP       _NATEXEC_PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP

#define PTHREAD_STACK_MIN       16384

#define PTHAPI extern "C"

// Now come the functions that we have to implement.
// *** THREAD ATTRIBUTES
PTHAPI int pthread_attr_init( pthread_attr_t *attr );
PTHAPI int pthread_attr_destroy( pthread_attr_t *attr );
PTHAPI int pthread_attr_getinheritsched( const pthread_attr_t *attr, int *inheritsched );
PTHAPI int pthread_attr_setinheritsched( pthread_attr_t *attr, int inheritshed );
PTHAPI int pthread_attr_getschedparam( const pthread_attr_t *attr, sched_param *param );
PTHAPI int pthread_attr_setschedparam( pthread_attr_t *attr, const sched_param *param );
PTHAPI int pthread_attr_getschedpolicy( const pthread_attr_t *attr, int *policy );
PTHAPI int pthread_attr_setschedpolicy( pthread_attr_t *attr, int policy );
PTHAPI int pthread_attr_getscope( const pthread_attr_t *attr, int *scope );
PTHAPI int pthread_attr_setscope( pthread_attr_t *attr, int scope );
PTHAPI int pthread_attr_getstackaddr( const pthread_attr_t *attr, void **stackaddr );
PTHAPI int pthread_attr_setstackaddr( pthread_attr_t *attr, void *stackaddr );
PTHAPI int pthread_attr_getstack( const pthread_attr_t *attr, void **stackaddr, size_t *stacksize );
PTHAPI int pthread_attr_setstack( pthread_attr_t *attr, void *stackattr, size_t stacksize );
PTHAPI int pthread_attr_getdetachstate( const pthread_attr_t *attr, int *detachstate );
PTHAPI int pthread_attr_setdetachstate( pthread_attr_t *attr, int detachstate );
PTHAPI int pthread_attr_getguardsize( const pthread_attr_t *attr, size_t *guardsize );
PTHAPI int pthread_attr_setguardsize( pthread_attr_t *attr, size_t guardsize );
PTHAPI int pthread_attr_getstacksize( const pthread_attr_t *attr, size_t *stacksize );
PTHAPI int pthread_attr_setstacksize( pthread_attr_t *attr, size_t stacksize );
PTHAPI int pthread_getattr_np( pthread_t thread, pthread_attr_t *attr );

// *** THREADS
PTHAPI int pthread_create( pthread_t *thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void *arg );
PTHAPI int pthread_exit( void *status );
PTHAPI int pthread_join( pthread_t thread, void **status );
PTHAPI int pthread_detach( pthread_t thread );
PTHAPI int pthread_equal( pthread_t thread1, pthread_t thread2 );
PTHAPI pthread_t pthread_self( void );

// *** MUTEX ATTRIBUTES
PTHAPI int pthread_mutexattr_init( pthread_mutexattr_t *attr );
PTHAPI int pthread_mutexattr_destroy( pthread_mutexattr_t *attr );
PTHAPI int pthread_mutexattr_getprioceiling( const pthread_mutexattr_t *attr, int *prioceling );
PTHAPI int pthread_mutexattr_setprioceiling( pthread_mutexattr_t *attr, int prioceiling );
PTHAPI int pthread_mutexattr_getprotocol( const pthread_mutexattr_t *attr, int *protocol );
PTHAPI int pthread_mutexattr_setprotocol( pthread_mutexattr_t *attr, int protocol );
PTHAPI int pthread_mutexattr_getrobust( const pthread_mutexattr_t *attr, int *robustness );
PTHAPI int pthread_mutexattr_getrobust_np( const pthread_mutexattr_t *attr, int *robustness );
PTHAPI int pthread_mutexattr_setrobust( pthread_mutexattr_t *attr, int robustness );
PTHAPI int pthread_mutexattr_setrobust_np( pthread_mutexattr_t *attr, int robustness );
PTHAPI int pthread_mutexattr_getpshared( const pthread_mutexattr_t *attr, int *pshared );
PTHAPI int pthread_mutexattr_setpshared( pthread_mutexattr_t *attr, int pshared );
PTHAPI int pthread_mutexattr_gettype( const pthread_mutexattr_t *attr, int *type );
PTHAPI int pthread_mutexattr_settype( pthread_mutexattr_t *attr, int type );

// *** MUTEX
PTHAPI int pthread_mutex_init( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr );
PTHAPI int pthread_mutex_destroy( pthread_mutex_t *mutex );
PTHAPI int pthread_mutex_lock( pthread_mutex_t *mutex );
PTHAPI int pthread_mutex_trylock( pthread_mutex_t *mutex );
PTHAPI int pthread_mutex_timedlock( pthread_mutex_t *mutex, const timespec *abstime );
PTHAPI int pthread_mutex_unlock( pthread_mutex_t *mutex );
PTHAPI int pthread_mutex_transfer_np( pthread_mutex_t *mutex, pthread_t tid );
PTHAPI int pthread_mutex_getprioceiling( const pthread_mutex_t *mutex, int *prioceiling );
PTHAPI int pthread_mutex_setprioceiling( pthread_mutex_t *mutex, int prio, int *oldprio );
PTHAPI int pthread_mutex_consistent( pthread_mutex_t *mutex );
PTHAPI int pthread_mutex_consistent_np( pthread_mutex_t *mutex );

// *** CONDITION ATTR
PTHAPI int pthread_condattr_init( pthread_condattr_t *attr );
PTHAPI int pthread_condattr_destroy( pthread_condattr_t *attr );
PTHAPI int pthread_condattr_getclock( const pthread_condattr_t *attr, clockid_t *clockid );
PTHAPI int pthread_condattr_setclock( pthread_condattr_t *attr, clockid_t clockid );
PTHAPI int pthread_condattr_getpshared( const pthread_condattr_t *attr, int *pshared );
PTHAPI int pthread_condattr_setpshared( pthread_condattr_t *attr, int pshared );

// *** CONDITION
PTHAPI int pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t *attr );
PTHAPI int pthread_cond_destroy( pthread_cond_t *cond );
PTHAPI int pthread_cond_signal( pthread_cond_t *cond );
PTHAPI int pthread_cond_broadcast( pthread_cond_t *cond );
PTHAPI int pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex );
PTHAPI int pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex, const timespec *abstime );

// *** SPIN LOCKS
PTHAPI int pthread_spin_init( pthread_spinlock_t *lock, int pshared );
PTHAPI int pthread_spin_destroy( pthread_spinlock_t *lock );
PTHAPI int pthread_spin_lock( pthread_spinlock_t *lock );
PTHAPI int pthread_spin_trylock( pthread_spinlock_t *lock );
PTHAPI int pthread_spin_unlock( pthread_spinlock_t *lock );

// *** RWLOCK ATTR
PTHAPI int pthread_rwlockattr_init( pthread_rwlockattr_t *attr );
PTHAPI int pthread_rwlockattr_destroy( pthread_rwlockattr_t *attr );
PTHAPI int pthread_rwlockattr_getpshared( const pthread_rwlockattr_t *attr, int *pshared );
PTHAPI int pthread_rwlockattr_setpshared( pthread_rwlockattr_t *attr, int pshared );
PTHAPI int pthread_rwlockattr_getkind_np( const pthread_rwlockattr_t *attr, int *pref );
PTHAPI int pthread_rwlockattr_setkind_np( pthread_rwlockattr_t *attr, int pref );

// *** RWLOCK
PTHAPI int pthread_rwlock_init( pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr );
PTHAPI int pthread_rwlock_destroy( pthread_rwlock_t *rwlock );
PTHAPI int pthread_rwlock_rdlock( pthread_rwlock_t *rwlock );
PTHAPI int pthread_rwlock_tryrdlock( pthread_rwlock_t *rwlock );
PTHAPI int pthread_rwlock_timedrdlock( pthread_rwlock_t *rwlock, const timespec *abstime );
PTHAPI int pthread_rwlock_wrlock( pthread_rwlock_t *rwlock );
PTHAPI int pthread_rwlock_trywrlock( pthread_rwlock_t *rwlock );
PTHAPI int pthread_rwlock_timedwrlock( pthread_rwlock_t *rwlock, const timespec *abstime );
PTHAPI int pthread_rwlock_unlock( pthread_rwlock_t *rwlock );

// *** CANCELATION
PTHAPI void pthread_cleanup_push( void (*routine)(void*), void *arg );
PTHAPI void pthread_cleanup_pop( int execute );
PTHAPI int pthread_setcancelstate( int state, int *oldstate );
PTHAPI int pthread_setcanceltype( int type, int *oldtype );
PTHAPI int pthread_cancel( pthread_t thread );
PTHAPI void pthread_testcancel( void );
PTHAPI void pthread_cleanup_push_defer_np( void (*routine)(void*), void *arg );
PTHAPI void pthread_cleanup_pop_restore_np( int execute );

// *** BARRIER ATTR
PTHAPI int pthread_barrierattr_init( pthread_barrierattr_t *attr );
PTHAPI int pthread_barrierattr_destroy( pthread_barrierattr_t *attr );
PTHAPI int pthread_barrierattr_getpshared( const pthread_barrierattr_t *attr, int *pshared );
PTHAPI int pthread_barrierattr_setpshared( pthread_barrierattr_t *attr, int pshared );

// *** BARRIER
PTHAPI int pthread_barrier_init( pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count );
PTHAPI int pthread_barrier_destroy( pthread_barrier_t *barrier );
PTHAPI int pthread_barrier_wait( pthread_barrier_t *barrier );

// *** THREAD SPECIFIC DATA (generic)
PTHAPI int pthread_key_create( pthread_key_t *key, void (*destructor)(void*) );
PTHAPI int pthread_key_delete( pthread_key_t key );
PTHAPI void* pthread_getspecific( pthread_key_t key );
PTHAPI int pthread_setspecific( pthread_key_t key, const void *value );

// *** INIT (generic)
PTHAPI int pthread_once( pthread_once_t *once_control, void (*init_routine)(void) );
PTHAPI int pthread_once_ex_np( pthread_once_t *once_control, void (*init_routine)(void*), void *arg );

// *** CONCURRENCY (generic)
PTHAPI int pthread_setconcurrency( int new_level );
PTHAPI int pthread_getconcurrency( void );

// *** FORKING (generic)
PTHAPI int pthread_atfork( void (*prepare)(void), void (*parent)(void), void (*child)(void) );

// *** SIGNALS (generic)
PTHAPI int pthread_kill( pthread_t thread, int signo );

// *** TIME (generic)
PTHAPI int pthread_getcpuclockid( pthread_t thread, clockid_t *clockid );

// *** SCHEDULING (generic)
PTHAPI int pthread_getschedparam( pthread_t thread, int *policy, sched_param *param );
PTHAPI int pthread_setschedparam( pthread_t thread, int policy, const sched_param *param );
PTHAPI int pthread_setschedprio( pthread_t thread, int prio );
PTHAPI int pthread_yield( void );

// *** GNU extensions.
PTHAPI int sched_yield( void );
// TODO: add all the required API.

// *** SEMAPHORES (GNU)
PTHAPI int sem_init( sem_t *sem, int pshared, unsigned int value );
PTHAPI int sem_destroy( sem_t *sem );
PTHAPI sem_t* sem_open( const char *name, int oflag, ... );
PTHAPI int sem_close( sem_t *sem );
PTHAPI int sem_unlink( const char *name );
PTHAPI int sem_wait( sem_t *sem );
PTHAPI int sem_timedwait( sem_t *sem, const timespec *abstime );
PTHAPI int sem_trywait( sem_t *sem );
PTHAPI int sem_post( sem_t *sem );
PTHAPI int sem_getvalue( sem_t *sem, int *sval );

#endif //_PTHREAD_H

#endif //NATEXEC_PTHREADS_HEADER
