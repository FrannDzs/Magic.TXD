/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.internal.h
*  PURPOSE:     POSIX threads internal header for local definitions
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_PTHREADS_INTERNAL_HEADER_
#define _NATEXEC_PTHREADS_INTERNAL_HEADER_

#include "pthreads/pthread.h"

#ifdef __linux__
#define PTHPRIVATE __attribute__((visibility("hidden")))
#else
#define PTHPRIVATE
#endif

// *** THREAD ATTRIBUTES
PTHPRIVATE int _natexec_pthread_attr_init( _natexec_pthread_attr_t *attr );
PTHPRIVATE int _natexec_pthread_attr_destroy( _natexec_pthread_attr_t *attr );
PTHPRIVATE int _natexec_pthread_attr_getinheritsched( const _natexec_pthread_attr_t *attr, int *inheritsched );
PTHPRIVATE int _natexec_pthread_attr_setinheritsched( _natexec_pthread_attr_t *attr, int inheritshed );
PTHPRIVATE int _natexec_pthread_attr_getschedparam( const _natexec_pthread_attr_t *attr, sched_param *param );
PTHPRIVATE int _natexec_pthread_attr_setschedparam( _natexec_pthread_attr_t *attr, const sched_param *param );
PTHPRIVATE int _natexec_pthread_attr_getschedpolicy( const _natexec_pthread_attr_t *attr, int *policy );
PTHPRIVATE int _natexec_pthread_attr_setschedpolicy( _natexec_pthread_attr_t *attr, int policy );
PTHPRIVATE int _natexec_pthread_attr_getscope( const _natexec_pthread_attr_t *attr, int *scope );
PTHPRIVATE int _natexec_pthread_attr_setscope( _natexec_pthread_attr_t *attr, int scope );
PTHPRIVATE int _natexec_pthread_attr_getstackaddr( const _natexec_pthread_attr_t *attr, void **stackaddr );
PTHPRIVATE int _natexec_pthread_attr_setstackaddr( _natexec_pthread_attr_t *attr, void *stackaddr );
PTHPRIVATE int _natexec_pthread_attr_getstack( const _natexec_pthread_attr_t *attr, void **stackaddr, size_t *stacksize );
PTHPRIVATE int _natexec_pthread_attr_setstack( _natexec_pthread_attr_t *attr, void *stackattr, size_t stacksize );
PTHPRIVATE int _natexec_pthread_attr_getdetachstate( const _natexec_pthread_attr_t *attr, int *detachstate );
PTHPRIVATE int _natexec_pthread_attr_setdetachstate( _natexec_pthread_attr_t *attr, int detachstate );
PTHPRIVATE int _natexec_pthread_attr_getguardsize( const _natexec_pthread_attr_t *attr, size_t *guardsize );
PTHPRIVATE int _natexec_pthread_attr_setguardsize( _natexec_pthread_attr_t *attr, size_t guardsize );
PTHPRIVATE int _natexec_pthread_attr_getstacksize( const _natexec_pthread_attr_t *attr, size_t *stacksize );
PTHPRIVATE int _natexec_pthread_attr_setstacksize( _natexec_pthread_attr_t *attr, size_t stacksize );
PTHPRIVATE int _natexec_pthread_getattr_np( _natexec_pthread_t thread, _natexec_pthread_attr_t *attr );

// *** THREADS
PTHPRIVATE int _natexec_pthread_create( _natexec_pthread_t *thread, const _natexec_pthread_attr_t *attr, void* (*start_routine)(void*), void *arg );
PTHPRIVATE int _natexec_pthread_exit( void *status );
PTHPRIVATE int _natexec_pthread_join( _natexec_pthread_t thread, void **status );
PTHPRIVATE int _natexec_pthread_detach( _natexec_pthread_t thread );
PTHPRIVATE int _natexec_pthread_equal( _natexec_pthread_t thread1, _natexec_pthread_t thread2 );
PTHPRIVATE _natexec_pthread_t _natexec_pthread_self( void );

// *** MUTEX ATTRIBUTES
PTHPRIVATE int _natexec_pthread_mutexattr_init( _natexec_pthread_mutexattr_t *attr );
PTHPRIVATE int _natexec_pthread_mutexattr_destroy( _natexec_pthread_mutexattr_t *attr );
PTHPRIVATE int _natexec_pthread_mutexattr_getprioceiling( const _natexec_pthread_mutexattr_t *attr, int *prioceling );
PTHPRIVATE int _natexec_pthread_mutexattr_setprioceiling( _natexec_pthread_mutexattr_t *attr, int prioceiling );
PTHPRIVATE int _natexec_pthread_mutexattr_getprotocol( const _natexec_pthread_mutexattr_t *attr, int *protocol );
PTHPRIVATE int _natexec_pthread_mutexattr_setprotocol( _natexec_pthread_mutexattr_t *attr, int protocol );
PTHPRIVATE int _natexec_pthread_mutexattr_getrobust( const _natexec_pthread_mutexattr_t *attr, int *robustness );
PTHPRIVATE int _natexec_pthread_mutexattr_getrobust_np( const _natexec_pthread_mutexattr_t *attr, int *robustness );
PTHPRIVATE int _natexec_pthread_mutexattr_setrobust( _natexec_pthread_mutexattr_t *attr, int robustness );
PTHPRIVATE int _natexec_pthread_mutexattr_setrobust_np( _natexec_pthread_mutexattr_t *attr, int robustness );
PTHPRIVATE int _natexec_pthread_mutexattr_getpshared( const _natexec_pthread_mutexattr_t *attr, int *pshared );
PTHPRIVATE int _natexec_pthread_mutexattr_setpshared( _natexec_pthread_mutexattr_t *attr, int pshared );
PTHPRIVATE int _natexec_pthread_mutexattr_gettype( const _natexec_pthread_mutexattr_t *attr, int *type );
PTHPRIVATE int _natexec_pthread_mutexattr_settype( _natexec_pthread_mutexattr_t *attr, int type );

// *** MUTEX
PTHPRIVATE int _natexec_pthread_mutex_init( _natexec_pthread_mutex_t *mutex, const _natexec_pthread_mutexattr_t *attr );
PTHPRIVATE int _natexec_pthread_mutex_destroy( _natexec_pthread_mutex_t *mutex );
PTHPRIVATE int _natexec_pthread_mutex_lock( _natexec_pthread_mutex_t *mutex );
PTHPRIVATE int _natexec_pthread_mutex_trylock( _natexec_pthread_mutex_t *mutex );
PTHPRIVATE int _natexec_pthread_mutex_timedlock( _natexec_pthread_mutex_t *mutex, const timespec *abstime );
PTHPRIVATE int _natexec_pthread_mutex_unlock( _natexec_pthread_mutex_t *mutex );
PTHPRIVATE int _natexec_pthread_mutex_transfer_np( _natexec_pthread_mutex_t *mutex, _natexec_pthread_t tid );
PTHPRIVATE int _natexec_pthread_mutex_getprioceiling( const _natexec_pthread_mutex_t *mutex, int *prioceiling );
PTHPRIVATE int _natexec_pthread_mutex_setprioceiling( _natexec_pthread_mutex_t *mutex, int prio, int *oldprio );
PTHPRIVATE int _natexec_pthread_mutex_consistent( _natexec_pthread_mutex_t *mutex );
PTHPRIVATE int _natexec_pthread_mutex_consistent_np( _natexec_pthread_mutex_t *mutex );

// *** CONDITION ATTR
PTHPRIVATE int _natexec_pthread_condattr_init( _natexec_pthread_condattr_t *attr );
PTHPRIVATE int _natexec_pthread_condattr_destroy( _natexec_pthread_condattr_t *attr );
PTHPRIVATE int _natexec_pthread_condattr_getclock( const _natexec_pthread_condattr_t *attr, clockid_t *clockid );
PTHPRIVATE int _natexec_pthread_condattr_setclock( _natexec_pthread_condattr_t *attr, clockid_t clockid );
PTHPRIVATE int _natexec_pthread_condattr_getpshared( const _natexec_pthread_condattr_t *attr, int *pshared );
PTHPRIVATE int _natexec_pthread_condattr_setpshared( _natexec_pthread_condattr_t *attr, int pshared );

// *** CONDITION
PTHPRIVATE int _natexec_pthread_cond_init( _natexec_pthread_cond_t *cond, const _natexec_pthread_condattr_t *attr );
PTHPRIVATE int _natexec_pthread_cond_destroy( _natexec_pthread_cond_t *cond );
PTHPRIVATE int _natexec_pthread_cond_signal( _natexec_pthread_cond_t *cond );
PTHPRIVATE int _natexec_pthread_cond_broadcast( _natexec_pthread_cond_t *cond );
PTHPRIVATE int _natexec_pthread_cond_wait( _natexec_pthread_cond_t *cond, _natexec_pthread_mutex_t *mutex );
PTHPRIVATE int _natexec_pthread_cond_timedwait( _natexec_pthread_cond_t *cond, _natexec_pthread_mutex_t *mutex, const timespec *abstime );

// *** SPIN LOCKS
PTHPRIVATE int _natexec_pthread_spin_init( _natexec_pthread_spinlock_t *lock, int pshared );
PTHPRIVATE int _natexec_pthread_spin_destroy( _natexec_pthread_spinlock_t *lock );
PTHPRIVATE int _natexec_pthread_spin_lock( _natexec_pthread_spinlock_t *lock );
PTHPRIVATE int _natexec_pthread_spin_trylock( _natexec_pthread_spinlock_t *lock );
PTHPRIVATE int _natexec_pthread_spin_unlock( _natexec_pthread_spinlock_t *lock );

// *** RWLOCK ATTR
PTHPRIVATE int _natexec_pthread_rwlockattr_init( _natexec_pthread_rwlockattr_t *attr );
PTHPRIVATE int _natexec_pthread_rwlockattr_destroy( _natexec_pthread_rwlockattr_t *attr );
PTHPRIVATE int _natexec_pthread_rwlockattr_getpshared( const _natexec_pthread_rwlockattr_t *attr, int *pshared );
PTHPRIVATE int _natexec_pthread_rwlockattr_setpshared( _natexec_pthread_rwlockattr_t *attr, int pshared );
PTHPRIVATE int _natexec_pthread_rwlockattr_getkind_np( const _natexec_pthread_rwlockattr_t *attr, int *pref );
PTHPRIVATE int _natexec_pthread_rwlockattr_setkind_np( _natexec_pthread_rwlockattr_t *attr, int pref );

// *** RWLOCK
PTHPRIVATE int _natexec_pthread_rwlock_init( _natexec_pthread_rwlock_t *rwlock, const _natexec_pthread_rwlockattr_t *attr );
PTHPRIVATE int _natexec_pthread_rwlock_destroy( _natexec_pthread_rwlock_t *rwlock );
PTHPRIVATE int _natexec_pthread_rwlock_rdlock( _natexec_pthread_rwlock_t *rwlock );
PTHPRIVATE int _natexec_pthread_rwlock_tryrdlock( _natexec_pthread_rwlock_t *rwlock );
PTHPRIVATE int _natexec_pthread_rwlock_timedrdlock( _natexec_pthread_rwlock_t *rwlock, const timespec *abstime );
PTHPRIVATE int _natexec_pthread_rwlock_wrlock( _natexec_pthread_rwlock_t *rwlock );
PTHPRIVATE int _natexec_pthread_rwlock_trywrlock( _natexec_pthread_rwlock_t *rwlock );
PTHPRIVATE int _natexec_pthread_rwlock_timedwrlock( _natexec_pthread_rwlock_t *rwlock, const timespec *abstime );
PTHPRIVATE int _natexec_pthread_rwlock_unlock( _natexec_pthread_rwlock_t *rwlock );

// *** CANCELATION
PTHPRIVATE void _natexec_pthread_cleanup_push( void (*routine)(void*), void *arg );
PTHPRIVATE void _natexec_pthread_cleanup_pop( int execute );
PTHPRIVATE int _natexec_pthread_setcancelstate( int state, int *oldstate );
PTHPRIVATE int _natexec_pthread_setcanceltype( int type, int *oldtype );
PTHPRIVATE int _natexec_pthread_cancel( _natexec_pthread_t thread );
PTHPRIVATE void _natexec_pthread_testcancel( void );
PTHPRIVATE void _natexec_pthread_cleanup_push_defer_np( void (*routine)(void*), void *arg );
PTHPRIVATE void _natexec_pthread_cleanup_pop_restore_np( int execute );

// *** BARRIER ATTR
PTHPRIVATE int _natexec_pthread_barrierattr_init( _natexec_pthread_barrierattr_t *attr );
PTHPRIVATE int _natexec_pthread_barrierattr_destroy( _natexec_pthread_barrierattr_t *attr );
PTHPRIVATE int _natexec_pthread_barrierattr_getpshared( const _natexec_pthread_barrierattr_t *attr, int *pshared );
PTHPRIVATE int _natexec_pthread_barrierattr_setpshared( _natexec_pthread_barrierattr_t *attr, int pshared );

// *** BARRIER
PTHPRIVATE int _natexec_pthread_barrier_init( _natexec_pthread_barrier_t *barrier, const _natexec_pthread_barrierattr_t *attr, unsigned int count );
PTHPRIVATE int _natexec_pthread_barrier_destroy( _natexec_pthread_barrier_t *barrier );
PTHPRIVATE int _natexec_pthread_barrier_wait( _natexec_pthread_barrier_t *barrier );

// *** THREAD SPECIFIC DATA (generic)
PTHPRIVATE int _natexec_pthread_key_create( _natexec_pthread_key_t *key, void (*destructor)(void*) );
PTHPRIVATE int _natexec_pthread_key_delete( _natexec_pthread_key_t key );
PTHPRIVATE void* _natexec_pthread_getspecific( _natexec_pthread_key_t key );
PTHPRIVATE int _natexec_pthread_setspecific( _natexec_pthread_key_t key, const void *value );

// *** INIT (generic)
PTHPRIVATE int _natexec_pthread_once( _natexec_pthread_once_t *once_control, void (*init_routine)(void) );
PTHPRIVATE int _natexec_pthread_once_ex_np( _natexec_pthread_once_t *once_control, void (*init_routine)(void*), void *arg );

// *** CONCURRENCY (generic)
PTHPRIVATE int _natexec_pthread_setconcurrency( int new_level );
PTHPRIVATE int _natexec_pthread_getconcurrency( void );

// *** FORKING (generic)
PTHPRIVATE int _natexec_pthread_atfork( void (*prepare)(void), void (*parent)(void), void (*child)(void) );

// *** SIGNALS (generic)
PTHPRIVATE int _natexec_pthread_kill( _natexec_pthread_t thread, int signo );

// *** TIME (generic)
PTHPRIVATE int _natexec_pthread_getcpuclockid( _natexec_pthread_t thread, clockid_t *clockid );

// *** SCHEDULING (generic)
PTHPRIVATE int _natexec_pthread_getschedparam( _natexec_pthread_t thread, int *policy, sched_param *param );
PTHPRIVATE int _natexec_pthread_setschedparam( _natexec_pthread_t thread, int policy, const sched_param *param );
PTHPRIVATE int _natexec_pthread_setschedprio( _natexec_pthread_t thread, int prio );
PTHPRIVATE int _natexec_pthread_yield( void );

// *** GNU extensions.
PTHPRIVATE int _natexec_sched_yield( void );
// TODO: add all the required API.

// *** SEMAPHORES (GNU)
PTHPRIVATE int _natexec_sem_init( _natexec_sem_t *sem, int pshared, unsigned int value );
PTHPRIVATE int _natexec_sem_destroy( _natexec_sem_t *sem );
// sem_open not implemented internally.
PTHPRIVATE int _natexec_sem_close( _natexec_sem_t *sem );
PTHPRIVATE int _natexec_sem_unlink( const char *name );
PTHPRIVATE int _natexec_sem_wait( _natexec_sem_t *sem );
PTHPRIVATE int _natexec_sem_timedwait( _natexec_sem_t *sem, const timespec *abstime );
PTHPRIVATE int _natexec_sem_trywait( _natexec_sem_t *sem );
PTHPRIVATE int _natexec_sem_post( _natexec_sem_t *sem );
PTHPRIVATE int _natexec_sem_getvalue( _natexec_sem_t *sem, int *sval );

#endif //_NATEXEC_PTHREADS_INTERNAL_HEADER_
