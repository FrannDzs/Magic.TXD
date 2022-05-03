/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.compat.glib.pthproto.cpp
*  PURPOSE:     Linux GCC libc compatibility layer pthreads prototype layer API
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef __linux__
#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "CExecutiveManager.thread.compat.glib.pthproto.hxx"

// In this file we want to provide API shims that will be used by glibc for interfacing
// with our threading functions. We must only use the internal-level symbols of our pthreads API
// (starting with _natexec_) to prevent global overriding support on platforms like Linux.

int _glibcshim_pthread_attr_getschedpolicy( const pthread_attr_t *attr, int *policy )
{
    return _natexec_pthread_attr_getschedpolicy( (const _natexec_pthread_attr_t*)attr, policy );
}

int _glibcshim_pthread_attr_setschedpolicy( pthread_attr_t *attr, int policy )
{
    return _natexec_pthread_attr_setschedpolicy( (_natexec_pthread_attr_t*)attr, policy );
}

int _glibcshim_pthread_attr_getscope( const pthread_attr_t *attr, int *scope )
{
    return _natexec_pthread_attr_getscope( (const _natexec_pthread_attr_t*)attr, scope );
}

int _glibcshim_pthread_attr_setscope( pthread_attr_t *attr, int scope )
{
    return _natexec_pthread_attr_setscope( (_natexec_pthread_attr_t*)attr, scope );
}

int _glibcshim_pthread_condattr_destroy( pthread_condattr_t *attr )
{
    return _natexec_pthread_condattr_destroy( (_natexec_pthread_condattr_t*)attr );
}

int _glibcshim_pthread_condattr_init( pthread_condattr_t *attr )
{
    return _natexec_pthread_condattr_init( (_natexec_pthread_condattr_t*)attr );
}

int _glibcshim_pthread_cond_broadcast( pthread_cond_t *cond )
{
    return _natexec_pthread_cond_broadcast( (_natexec_pthread_cond_t*)cond );
}

int _glibcshim_pthread_cond_destroy( pthread_cond_t *cond )
{
    return _natexec_pthread_cond_destroy( (_natexec_pthread_cond_t*)cond );
}

int _glibcshim_pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t *attr )
{
    return _natexec_pthread_cond_init( (_natexec_pthread_cond_t*)cond, (const _natexec_pthread_condattr_t*)attr );
}

int _glibcshim_pthread_cond_signal( pthread_cond_t *cond )
{
    return _natexec_pthread_cond_signal( (_natexec_pthread_cond_t*)cond );
}

int _glibcshim_pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex )
{
    return _natexec_pthread_cond_wait( (_natexec_pthread_cond_t*)cond, (_natexec_pthread_mutex_t*)mutex );
}

int _glibcshim_pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex, const timespec *abstime )
{
    return _natexec_pthread_cond_timedwait( (_natexec_pthread_cond_t*)cond, (_natexec_pthread_mutex_t*)mutex, abstime );
}

// TODO: actually implement the old-condvar support just like it is done in glibc.
int _glibcshim_pthread_cond_broadcast_2_0( pthread_cond_t *cond )
{
    return _natexec_pthread_cond_broadcast( (_natexec_pthread_cond_t*)cond );
}

int _glibcshim_pthread_cond_destroy_2_0( pthread_cond_t *cond )
{
    return _natexec_pthread_cond_destroy( (_natexec_pthread_cond_t*)cond );
}

int _glibcshim_pthread_cond_init_2_0( pthread_cond_t *cond, const pthread_condattr_t *attr )
{
    return _natexec_pthread_cond_init( (_natexec_pthread_cond_t*)cond, (const _natexec_pthread_condattr_t*)attr );
}

int _glibcshim_pthread_cond_signal_2_0( pthread_cond_t *cond )
{
    return _natexec_pthread_cond_signal( (_natexec_pthread_cond_t*)cond );
}

int _glibcshim_pthread_cond_wait_2_0( pthread_cond_t *cond, pthread_mutex_t *mutex )
{
    return _natexec_pthread_cond_wait( (_natexec_pthread_cond_t*)cond, (_natexec_pthread_mutex_t*)mutex );
}

int _glibcshim_pthread_cond_timedwait_2_0( pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime )
{
    return _natexec_pthread_cond_timedwait( (_natexec_pthread_cond_t*)cond, (_natexec_pthread_mutex_t*)mutex, abstime );
}

void _glibcshim_pthread_exit( void *status )
{
    _natexec_pthread_exit( status );
}

int _glibcshim_pthread_getschedparam( pthread_t thread, int *policy, struct sched_param *param )
{
    return _natexec_pthread_getschedparam( (_natexec_pthread_t)thread, policy, param );
}

int _glibcshim_pthread_setschedparam( pthread_t thread, int policy, const struct sched_param *param )
{
    return _natexec_pthread_setschedparam( (_natexec_pthread_t)thread, policy, param );
}

int _glibcshim_pthread_mutex_destroy( pthread_mutex_t *mutex )
{
    return _natexec_pthread_mutex_destroy( (_natexec_pthread_mutex_t*)mutex );
}

int _glibcshim_pthread_mutex_init( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr )
{
    return _natexec_pthread_mutex_init( (_natexec_pthread_mutex_t*)mutex, (const _natexec_pthread_mutexattr_t*)attr );
}

int _glibcshim_pthread_mutex_lock( pthread_mutex_t *mutex )
{
    return _natexec_pthread_mutex_lock( (_natexec_pthread_mutex_t*)mutex );
}

int _glibcshim_pthread_mutex_unlock( pthread_mutex_t *mutex )
{
    return _natexec_pthread_mutex_unlock( (_natexec_pthread_mutex_t*)mutex );
}

int _glibcshim_pthread_setcancelstate( int state, int *oldstate )
{
    return _natexec_pthread_setcancelstate( state, oldstate );
}

int _glibcshim_pthread_setcanceltype( int type, int *oldtype )
{
    return _natexec_pthread_setcanceltype( type, oldtype );
}

void _glibcshim_pthread_cleanup_upto( __jmp_buf buf, char *ref )
{
    // TODO: implement this.
}

int _glibcshim_pthread_once( pthread_once_t *once_control, void (*init_routine)(void) )
{
    return _natexec_pthread_once( (_natexec_pthread_once_t*)once_control, init_routine );
}

int _glibcshim_pthread_rwlock_rdlock( pthread_rwlock_t *rwlock )
{
    return _natexec_pthread_rwlock_rdlock( (_natexec_pthread_rwlock_t*)rwlock );
}

int _glibcshim_pthread_rwlock_wrlock( pthread_rwlock_t *rwlock )
{
    return _natexec_pthread_rwlock_wrlock( (_natexec_pthread_rwlock_t*)rwlock );
}

int _glibcshim_pthread_rwlock_unlock( pthread_rwlock_t *rwlock )
{
    return _natexec_pthread_rwlock_unlock( (_natexec_pthread_rwlock_t*)rwlock );
}

int _glibcshim_pthread_key_create( pthread_key_t *key, void (*destructor)(void*) )
{
    return _natexec_pthread_key_create( (_natexec_pthread_key_t*)key, destructor );
}

void* _glibcshim_pthread_getspecific( pthread_key_t key )
{
    return _natexec_pthread_getspecific( (_natexec_pthread_key_t)key );
}

int _glibcshim_pthread_setspecific( pthread_key_t key, const void *value )
{
    return _natexec_pthread_setspecific( (_natexec_pthread_key_t)key, value );
}

void _glibcshim_pthread_cleanup_push_defer( struct _pthread_cleanup_buffer *buf, void (*routine)(void*), void *arg )
{
    _natexec_pthread_cleanup_push_defer_np( routine, arg );
}

void _glibcshim_pthread_cleanup_pop_restore( struct _pthread_cleanup_buffer *buf, int execute )
{
    _natexec_pthread_cleanup_pop_restore_np( execute );
}

void _glibcshim_pthread_unwind( __pthread_unwind_buf_t* )
{
    printf( "fatal error: unwind not supported because other exception system\n" );
    *(char*)0 = 0;
}

void _glibcshim_nptl_deallocate_tsd( void )
{
    // Nothing to do.
}

int _glibcshim_nptl_setxid( struct xid_command *info )
{
    // Not supported.
    return -1;
}

void _glibcshim_set_robust( struct pthread* )
{
    // Not supported.
}

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
#endif //__linux__
