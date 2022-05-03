/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/internal/pthread.glibc.h
*  PURPOSE:     POSIX threads GLIBC ABI compatible structures
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_PTHREADS_GLIBC_COMPAT_
#define _NATEXEC_PTHREADS_GLIBC_COMPAT_

#ifdef __linux__
#include <bits/types/struct_sched_param.h>
#include <bits/types/struct_timespec.h>
#else
// We have no idea about those definitions on other platforms.
struct sched_param;
struct timespec;
#endif //__linux__

// Definitions adjusted to the GLIBC header contents.
#if defined(_WIN64) || defined(__x86_64__)
#define __NATEXEC_SIZEOF_PTHREAD_MUTEX_T            40
#define __NATEXEC_SIZEOF_PTHREAD_ATTR_T             56
#define __NATEXEC_SIZEOF_PTHREAD_RWLOCK_T           56
#define __NATEXEC_SIZEOF_PTHREAD_BARRIER_T          32
#define __NATEXEC_SIZEOF_SEM_T                      32
#else
#define __NATEXEC_SIZEOF_PTHREAD_MUTEX_T            32
#define __NATEXEC_SIZEOF_PTHREAD_ATTR_T             32
#define __NATEXEC_SIZEOF_PTHREAD_RWLOCK_T           44
#define __NATEXEC_SIZEOF_PTHREAD_BARRIER_T          20
#define __NATEXDC_SIZEOF_SEM_T                      16
#endif //64bit check

#define __NATEXEC_SIZEOF_PTHREAD_MUTEXATTR_T        4
#define __NATEXEC_SIZEOF_PTHREAD_COND_T             48
#define __NATEXEC_SIZEOF_PTHREAD_CONDATTR_T         4
#define __NATEXEC_SIZEOF_PTHREAD_RWLOCKATTR_T       8
#define __NATEXEC_SIZEOF_PTHREAD_BARRIERATTR_T      4

#define __NATEXEC_GLIBC_PTHREAD_INHERIT_SCHED       0
#define __NATEXEC_GLIBC_PTHREAD_EXPLICIT_SCHED      1

#define __NATEXEC_GLIBC_PTHREAD_SCHED_OTHER         0
#define __NATEXEC_GLIBC_PTHREAD_SCHED_FIFO          1
#define __NATEXEC_GLIBC_PTHREAD_SCHED_RR            2

#define __NATEXEC_GLIBC_PTHREAD_SCOPE_SYSTEM        0
#define __NATEXEC_GLIBC_PTHREAD_SCOPE_PROCESS       1

#define __NATEXEC_GLIBC_PTHREAD_CREATE_JOINABLE     0
#define __NATEXEC_GLIBC_PTHREAD_CREATE_DETACHED     1

#define __NATEXEC_GLIBC_PTHREAD_MUTEX_TIMED_NP      0
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_RECURSIVE_NP  1
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_ERRORCHECK_NP 2
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_ADAPTIVE_NP   3
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_NORMAL        __NATEXEC_GLIBC_PTHREAD_MUTEX_TIMED_NP
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_RECURSIVE     __NATEXEC_GLIBC_PTHREAD_MUTEX_RECURSIVE_NP
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_ERRORCHECK    __NATEXEC_GLIBC_PTHREAD_MUTEX_ERRORCHECK_NP
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_DEFAULT       __NATEXEC_GLIBC_PTHREAD_MUTEX_NORMAL
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_FAST_NP       __NATEXEC_GLIBC_PTHREAD_MUTEX_TIMED_NP
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_STALLED       0
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_STALLED_NP    __NATEXEC_GLIBC_PTHREAD_MUTEX_STALLED
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_ROBUST        1
#define __NATEXEC_GLIBC_PTHREAD_MUTEX_ROBUST_NP     __NATEXEC_GLIBC_PTHREAD_MUTEX_ROBUST

#define __NATEXEC_GLIBC_PTHREAD_PRIO_NONE           0
#define __NATEXEC_GLIBC_PTHREAD_PRIO_INHERIT        1
#define __NATEXEC_GLIBC_PTHREAD_PRIO_PROTECT        2

#define __NATEXEC_GLIBC_PTHREAD_PROCESS_PRIVATE     0
#define __NATEXEC_GLIBC_PTHREAD_PROCESS_SHARED      1

#define __NATEXEC_GLIBC_PTHREAD_CANCEL_ENABLE       0
#define __NATEXEC_GLIBC_PTHREAD_CANCEL_DISABLE      1
#define __NATEXEC_GLIBC_PTHREAD_CANCELED            ((void*)-1)

#define __NATEXEC_GLIBC_PTHREAD_RWLOCK_PREFER_READER_NP                 0
#define __NATEXEC_GLIBC_PTHREAD_RWLOCK_PREFER_WRITER_NP                 1
#define __NATEXEC_GLIBC_PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP    2
#define __NATEXEC_GLIBC_PTHREAD_RWLOCK_DEFAULT_NP                       __NATEXEC_GLIBC_PTHREAD_RWLOCK_PREFER_READER_NP

#define __NATEXEC_GLIBC_PTHREAD_CANCEL_DEFERRED     0
#define __NATEXEC_GLIBC_PTHREAD_CANCEL_ASYNCHRONOUS 1

#define __NATEXEC_GLIBC_PTHREAD_ONCE_INIT           0

#define __NATEXEC_GLIBC_PTHREAD_SEM_FAILED          ((__natexec_glibc_pthread_sem_t*)0)

#define __NATEXEC_GLIBC_PTHREAD_COND_INITIALIZER    { 0 }
#define __NATEXEC_GLIBC_PTHREAD_RWLOCK_INITIALIZER  { 0 }


typedef void* __natexec_glibc_pthread_t;

union __natexec_glibc_pthread_mutexattr_t
{
    char __size[__NATEXEC_SIZEOF_PTHREAD_MUTEXATTR_T];
    int __align;
};

union __natexec_glibc_pthread_condattr_t
{
    char __size[__NATEXEC_SIZEOF_PTHREAD_CONDATTR_T];
    int __align;
};

typedef unsigned int __natexec_glibc_pthread_key_t;

typedef int __natexec_glibc_pthread_once_t;

union __natexec_glibc_pthread_attr_t
{
    char __size[__NATEXEC_SIZEOF_PTHREAD_ATTR_T];
    void *__align;
};

struct __natexec_glibc_pthread_mutexdata_t
{
    int pad1;
    unsigned int pad2;
    int pad3;
    // if !__PTHREAD_MUTEX_NUSERS_AFTER_KIND
    unsigned int pad4;
    // end.
    int __kind;
    // End padding:
    char pad5[__NATEXEC_SIZEOF_PTHREAD_MUTEX_T - (sizeof(pad1) + sizeof(pad2) + sizeof(pad3) + sizeof(pad4) + sizeof(__kind))];
};

#define __NATEXEC_GLIBC_PTHREAD_MUTEX_INITIALIZER                   { .__data = { .pad1 = 0, .pad2 = 0, .pad3 = 0, .pad4 = 0, .__kind = 0, .pad5 = { 0 } } }
#define __NATEXEC_GLIBC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP      { .__data = { .pad1 = 0, .pad2 = 0, .pad3 = 0, .pad4 = 0, .__kind = __NATEXEC_GLIBC_PTHREAD_MUTEX_RECURSIVE_NP, .pad5 = { 0 } } }
#define __NATEXEC_GLIBC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP     { .__data = { .pad1 = 0, .pad2 = 0, .pad3 = 0, .pad4 = 0, .__kind = __NATEXEC_GLIBC_PTHREAD_MUTEX_ERRORCHECK_NP, .pad5 0 { 0 } } }
#define __NATEXEC_GLIBC_PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP       { .__data = { .pad1 = 0, .pad2 = 0, .pad3 = 0, .pad4 = 0, .__kind = __NATEXEC_GLIBC_PTHREAD_MUTEX_ADAPTIVE_NP, .pad5 = { 0 } } }

#define __NATEXEC_GLIBC_PTHREAD_GET_STATIC_MUTEX_KIND(mtx)          ( (mtx)->__data.__kind )

union __natexec_glibc_pthread_mutex_t
{
    __natexec_glibc_pthread_mutexdata_t __data;
    char __size[__NATEXEC_SIZEOF_PTHREAD_MUTEX_T];
    void *__align;
};

union __natexec_glibc_pthread_cond_t
{
    char __size[__NATEXEC_SIZEOF_PTHREAD_COND_T];
    long long int __align;
};

union __natexec_glibc_pthread_rwlock_t
{
    char __size[__NATEXEC_SIZEOF_PTHREAD_RWLOCK_T];
    void *__align;
};

union __natexec_glibc_pthread_rwlockattr_t
{
    char __size[__NATEXEC_SIZEOF_PTHREAD_RWLOCKATTR_T];
    void *__align;
};

typedef volatile int __natexec_glibc_pthread_spinlock_t;

union __natexec_glibc_pthread_barrier_t
{
    char __size[__NATEXEC_SIZEOF_PTHREAD_BARRIER_T];
    void *__align;
};

union __natexec_glibc_pthread_barrierattr_t
{
    char __size[__NATEXEC_SIZEOF_PTHREAD_BARRIERATTR_T];
    int __align;
};

union __natexec_glibc_pthread_sem_t
{
    char __size[__NATEXEC_SIZEOF_SEM_T];
    void *__align;
};

// Redirect system types.
typedef struct timespec __natexec_glibc_pthread_timespec_t;
typedef int __natexec_glibc_pthread_clockid_t;
typedef struct sched_param __natexec_glibc_pthread_sched_param_t;

#endif //_NATEXEC_PTHREADS_GLIBC_COMPAT_
