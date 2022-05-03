/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.compat.glib.ver2_32.hxx
*  PURPOSE:     Linux GCC libc compatibility layer for GLIBC version 2.32
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_GLIBC_COMPATIBILITY_VER_2_32_
#define _NATEXEC_GLIBC_COMPATIBILITY_VER_2_32_

#ifdef __linux__

// Define the table of available pthreads API.
// We have to use the functions gnu_get_libc_version and gnu_get_libc_release from the gnu/libc-version.h
// header to determine at runtime which structs we are allowed to use.
struct _libc_pthread_functions_2_32
{
    int (*ptr___pthread_cond_broadcast) (pthread_cond_t *);
    int (*ptr___pthread_cond_signal) (pthread_cond_t *);
    int (*ptr___pthread_cond_wait) (pthread_cond_t *, pthread_mutex_t *);
    int (*ptr___pthread_cond_timedwait) (pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
    int (*ptr___pthread_cond_broadcast_2_0) (pthread_cond_t *);
    int (*ptr___pthread_cond_signal_2_0) (pthread_cond_t *);
    int (*ptr___pthread_cond_wait_2_0) (pthread_cond_t *, pthread_mutex_t *);
    int (*ptr___pthread_cond_timedwait_2_0) (pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
    void (*ptr___pthread_exit) (void *) __attribute__ ((__noreturn__));
    int (*ptr_pthread_mutex_destroy) (pthread_mutex_t *);
    int (*ptr_pthread_mutex_init) (pthread_mutex_t *, const pthread_mutexattr_t *);
    int (*ptr_pthread_mutex_lock) (pthread_mutex_t *);
    int (*ptr_pthread_mutex_unlock) (pthread_mutex_t *);
    int (*ptr___pthread_setcancelstate) (int, int *);
    int (*ptr_pthread_setcanceltype) (int, int *);
    void (*ptr___pthread_cleanup_upto) (__jmp_buf, char *);
    int (*ptr___pthread_once) (pthread_once_t *, void (*) (void));
    int (*ptr___pthread_rwlock_rdlock) (pthread_rwlock_t *);
    int (*ptr___pthread_rwlock_wrlock) (pthread_rwlock_t *);
    int (*ptr___pthread_rwlock_unlock) (pthread_rwlock_t *);
    int (*ptr___pthread_key_create) (pthread_key_t *, void (*) (void *));
    void *(*ptr___pthread_getspecific) (pthread_key_t);
    int (*ptr___pthread_setspecific) (pthread_key_t, const void *);
    void (*ptr__pthread_cleanup_push_defer) (struct _pthread_cleanup_buffer *, void (*) (void *), void *);
    void (*ptr__pthread_cleanup_pop_restore) (struct _pthread_cleanup_buffer *, int);
    unsigned int *ptr_nthreads;
    void (*ptr___pthread_unwind) (__pthread_unwind_buf_t *) __attribute ((noreturn)) __cleanup_fct_attribute;
    void (*ptr__nptl_deallocate_tsd) (void);
    int (*ptr__nptl_setxid) (struct xid_command *);
    void (*ptr_set_robust) (struct pthread *);
};
extern const _libc_pthread_functions_2_32 _libc_pthread_functions_2_32_data;

#endif //__linux__

#endif //_NATEXEC_GLIBC_COMPATIBILITY_VER_2_32_
