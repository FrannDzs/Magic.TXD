/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.compat.glib.ver2_31.cpp
*  PURPOSE:     Linux GCC libc compatibility layer for threads (GLIBC 2.31)
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef __linux__
#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "CExecutiveManager.thread.compat.glib.ver2_31.hxx"
#include "CExecutiveManager.thread.compat.glib.pthproto.hxx"

static unsigned int _decoy_nthreads = 2;

const _libc_pthread_functions_2_31 _libc_pthread_functions_2_31_data =
{
    .ptr_pthread_attr_getschedpolicy = _glibcshim_pthread_attr_getschedpolicy,
    .ptr_pthread_attr_setschedpolicy = _glibcshim_pthread_attr_setschedpolicy,
    .ptr_pthread_attr_getscope = _glibcshim_pthread_attr_getscope,
    .ptr_pthread_attr_setscope = _glibcshim_pthread_attr_setscope,
    .ptr_pthread_condattr_destroy = _glibcshim_pthread_condattr_destroy,
    .ptr_pthread_condattr_init = _glibcshim_pthread_condattr_init,
    .ptr___pthread_cond_broadcast = _glibcshim_pthread_cond_broadcast,
    .ptr___pthread_cond_destroy = _glibcshim_pthread_cond_destroy,
    .ptr___pthread_cond_init = _glibcshim_pthread_cond_init,
    .ptr___pthread_cond_signal = _glibcshim_pthread_cond_signal,
    .ptr___pthread_cond_wait = _glibcshim_pthread_cond_wait,
    .ptr___pthread_cond_timedwait = _glibcshim_pthread_cond_timedwait,
    .ptr___pthread_cond_broadcast_2_0 = _glibcshim_pthread_cond_broadcast_2_0,
    .ptr___pthread_cond_destroy_2_0 = _glibcshim_pthread_cond_destroy_2_0,
    .ptr___pthread_cond_init_2_0 = _glibcshim_pthread_cond_init_2_0,
    .ptr___pthread_cond_signal_2_0 = _glibcshim_pthread_cond_signal_2_0,
    .ptr___pthread_cond_wait_2_0 = _glibcshim_pthread_cond_wait_2_0,
    .ptr___pthread_cond_timedwait_2_0 = _glibcshim_pthread_cond_timedwait_2_0,
    .ptr___pthread_exit = _glibcshim_pthread_exit,
    .ptr_pthread_getschedparam = _glibcshim_pthread_getschedparam,
    .ptr_pthread_setschedparam = _glibcshim_pthread_setschedparam,
    .ptr_pthread_mutex_destroy = _glibcshim_pthread_mutex_destroy,
    .ptr_pthread_mutex_init = _glibcshim_pthread_mutex_init,
    .ptr_pthread_mutex_lock = _glibcshim_pthread_mutex_lock,
    .ptr_pthread_mutex_unlock = _glibcshim_pthread_mutex_unlock,
    .ptr___pthread_setcancelstate = _glibcshim_pthread_setcancelstate,
    .ptr_pthread_setcanceltype = _glibcshim_pthread_setcanceltype,
    .ptr___pthread_cleanup_upto = _glibcshim_pthread_cleanup_upto,
    .ptr___pthread_once = _glibcshim_pthread_once,
    .ptr___pthread_rwlock_rdlock = _glibcshim_pthread_rwlock_rdlock,
    .ptr___pthread_rwlock_wrlock = _glibcshim_pthread_rwlock_wrlock,
    .ptr___pthread_rwlock_unlock = _glibcshim_pthread_rwlock_unlock,
    .ptr___pthread_key_create = _glibcshim_pthread_key_create,
    .ptr___pthread_getspecific = _glibcshim_pthread_getspecific,
    .ptr___pthread_setspecific = _glibcshim_pthread_setspecific,
    .ptr__pthread_cleanup_push_defer = _glibcshim_pthread_cleanup_push_defer,
    .ptr__pthread_cleanup_pop_restore = _glibcshim_pthread_cleanup_pop_restore,
    .ptr_nthreads = &_decoy_nthreads,
    .ptr___pthread_unwind = _glibcshim_pthread_unwind,
    .ptr__nptl_deallocate_tsd = _glibcshim_nptl_deallocate_tsd,
    .ptr__nptl_setxid = _glibcshim_nptl_setxid,
    .ptr_set_robust = _glibcshim_set_robust
};

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
#endif //__linux__
