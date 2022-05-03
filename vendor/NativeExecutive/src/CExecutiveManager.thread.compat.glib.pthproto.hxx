/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.compat.glib.pthproto.hxx
*  PURPOSE:     Linux GCC libc compatibility layer shim prototypes
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_GLIBC_COMPAT_PTHREADS_PROTOTYPE_SHIMS_
#define _NATEXEC_GLIBC_COMPAT_PTHREADS_PROTOTYPE_SHIMS_

#ifdef __linux__
#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "pthreads/CExecutiveManager.pthread.internal.h"

// Version 2.31
int _glibcshim_pthread_attr_getschedpolicy( const pthread_attr_t *attr, int *policy );
int _glibcshim_pthread_attr_setschedpolicy( pthread_attr_t *attr, int policy );
int _glibcshim_pthread_attr_getscope( const pthread_attr_t *attr, int *scope );
int _glibcshim_pthread_attr_setscope( pthread_attr_t *attr, int scope );
int _glibcshim_pthread_condattr_destroy( pthread_condattr_t *attr );
int _glibcshim_pthread_condattr_init( pthread_condattr_t *attr );
int _glibcshim_pthread_cond_broadcast( pthread_cond_t *cond );
int _glibcshim_pthread_cond_destroy( pthread_cond_t *cond );
int _glibcshim_pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t *attr );
int _glibcshim_pthread_cond_signal( pthread_cond_t *cond );
int _glibcshim_pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex );
int _glibcshim_pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex, const timespec *abstime );
int _glibcshim_pthread_cond_broadcast_2_0( pthread_cond_t *cond );
int _glibcshim_pthread_cond_destroy_2_0( pthread_cond_t *cond );
int _glibcshim_pthread_cond_init_2_0( pthread_cond_t *cond, const pthread_condattr_t *attr );
int _glibcshim_pthread_cond_signal_2_0( pthread_cond_t *cond );
int _glibcshim_pthread_cond_wait_2_0( pthread_cond_t *cond, pthread_mutex_t *mutex );
int _glibcshim_pthread_cond_timedwait_2_0( pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime );
void _glibcshim_pthread_exit( void *status );
int _glibcshim_pthread_getschedparam( pthread_t thread, int *policy, struct sched_param *param );
int _glibcshim_pthread_setschedparam( pthread_t thread, int policy, const struct sched_param *param );
int _glibcshim_pthread_mutex_destroy( pthread_mutex_t *mutex );
int _glibcshim_pthread_mutex_init( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr );
int _glibcshim_pthread_mutex_lock( pthread_mutex_t *mutex );
int _glibcshim_pthread_mutex_unlock( pthread_mutex_t *mutex );
int _glibcshim_pthread_setcancelstate( int state, int *oldstate );
int _glibcshim_pthread_setcanceltype( int type, int *oldtype );
void _glibcshim_pthread_cleanup_upto( __jmp_buf buf, char *ref );
int _glibcshim_pthread_once( pthread_once_t *once_control, void (*init_routine)(void) );
int _glibcshim_pthread_rwlock_rdlock( pthread_rwlock_t *rwlock );
int _glibcshim_pthread_rwlock_wrlock( pthread_rwlock_t *rwlock );
int _glibcshim_pthread_rwlock_unlock( pthread_rwlock_t *rwlock );
int _glibcshim_pthread_key_create( pthread_key_t *key, void (*destructor)(void*) );
void* _glibcshim_pthread_getspecific( pthread_key_t key );
int _glibcshim_pthread_setspecific( pthread_key_t key, const void *value );
void _glibcshim_pthread_cleanup_push_defer( struct _pthread_cleanup_buffer *buf, void (*routine)(void*), void *arg );
void _glibcshim_pthread_cleanup_pop_restore( struct _pthread_cleanup_buffer *buf, int execute );
void _glibcshim_pthread_unwind( __pthread_unwind_buf_t* );
void _glibcshim_nptl_deallocate_tsd( void );
int _glibcshim_nptl_setxid( struct xid_command *info );
void _glibcshim_set_robust( struct pthread* );

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
#endif //__linux__

#endif //_NATEXEC_GLIBC_COMPAT_PTHREADS_PROTOTYPE_SHIMS_
