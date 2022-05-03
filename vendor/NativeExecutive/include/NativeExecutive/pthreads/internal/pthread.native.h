/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/internal/pthread.native.h
*  PURPOSE:     POSIX threads custom implementation header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_PTHREADS_CUSTOM_HEADER_
#define _NATEXEC_PTHREADS_CUSTOM_HEADER_

// For time_t and timespec.
#include <time.h>

// Just create our own versions of the required structures.
// We define any of these as pointers because we would forward the
// logic to NativeExecutive itself, which has its own optimized memory
// allocators.
struct __natexec_native_pthread_objspace_t
{
    void *__pad1;
    unsigned int __pad2;
};

typedef void* __natexec_native_pthread_attr_t;
typedef void* __natexec_native_pthread_t;
typedef void* __natexec_native_pthread_mutexattr_t;
typedef void* __natexec_native_pthread_condattr_t;
typedef __natexec_native_pthread_objspace_t __natexec_native_pthread_cond_t;
typedef void* __natexec_native_pthread_spinlock_t;
typedef void* __natexec_native_pthread_rwlockattr_t;
typedef __natexec_native_pthread_objspace_t __natexec_native_pthread_rwlock_t;
typedef void* __natexec_native_pthread_barrierattr_t;
typedef void* __natexec_native_pthread_barrier_t;
typedef unsigned int __natexec_native_pthread_key_t;
typedef unsigned int __natexec_native_pthread_once_t;
typedef void* __natexec_native_pthread_sem_t;

#define __NATEXEC_NATIVE_PTHREAD_INHERIT_SCHED          0
#define __NATEXEC_NATIVE_PTHREAD_EXPLICIT_SCHED         1

#define __NATEXEC_NATIVE_PTHREAD_SCHED_FIFO             0
#define __NATEXEC_NATIVE_PTHREAD_SCHED_RR               1
#define __NATEXEC_NATIVE_PTHREAD_SCHED_OTHER            2

#define __NATEXEC_NATIVE_PTHREAD_SCOPE_SYSTEM           0
#define __NATEXEC_NATIVE_PTHREAD_SCOPE_PROCESS          1

#define __NATEXEC_NATIVE_PTHREAD_CREATE_DETACHED        0
#define __NATEXEC_NATIVE_PTHREAD_CREATE_JOINABLE        1

#define __NATEXEC_NATIVE_PTHREAD_MUTEX_TIMED_NP         0
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_RECURSIVE_NP     1
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_ERRORCHECK_NP    2
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_ADAPTIVE_NP      3
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_NORMAL           __NATEXEC_NATIVE_PTHREAD_MUTEX_TIMED_NP
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_RECURSIVE        __NATEXEC_NATIVE_PTHREAD_MUTEX_RECURSIVE_NP
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_ERRORCHECK       __NATEXEC_NATIVE_PTHREAD_MUTEX_ERRORCHECK_NP
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_DEFAULT          __NATEXEC_NATIVE_PTHREAD_MUTEX_NORMAL
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_FAST_NP          __NATEXEC_NATIVE_PTHREAD_MUTEX_TIMED_NP
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_STALLED          0
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_STALLED_NP       __NATEXEC_NATIVE_PTHREAD_MUTEX_STALLED
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_ROBUST           1
#define __NATEXEC_NATIVE_PTHREAD_MUTEX_ROBUST_NP        __NATEXEC_NATIVE_PTHREAD_MUTEX_ROBUST

#define __NATEXEC_NATIVE_PTHREAD_PRIO_NONE              0
#define __NATEXEC_NATIVE_PTHREAD_PRIO_INHERIT           1
#define __NATEXEC_NATIVE_PTHREAD_PRIO_PROTECT           2

#define __NATEXEC_NATIVE_PTHREAD_PROCESS_PRIVATE        0
#define __NATEXEC_NATIVE_PTHREAD_PROCESS_SHARED         1

#define __NATEXEC_NATIVE_PTHREAD_CANCEL_DISABLE         0
#define __NATEXEC_NATIVE_PTHREAD_CANCEL_ENABLE          1
#define __NATEXEC_NATIVE_PTHREAD_CANCELED               ((void*)-1)

#define __NATEXEC_NATIVE_PTHREAD_RWLOCK_PREFER_READER_NP                0
#define __NATEXEC_NATIVE_PTHREAD_RWLOCK_PREFER_WRITER_NP                1
#define __NATEXEC_NATIVE_PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP   2
#define __NATEXEC_NATIVE_PTHREAD_RWLOCK_DEFAULT_NP                      __NATEXEC_NATIVE_PTHREAD_RWLOCK_PREFER_READER_NP

#define __NATEXEC_NATIVE_PTHREAD_CANCEL_DEFERRED        0
#define __NATEXEC_NATIVE_PTHREAD_CANCEL_ASYNCHRONOUS    1

#define __NATEXEC_NATIVE_PTHREAD_ONCE_INIT              0

#define __NATEXEC_NATIVE_PTHREAD_SEM_FAILED             ((__natexec_native_pthread_sem_t*)0)

#define __NATEXEC_NATIVE_PTHREAD_COND_INITIALIZER       { 0 }
#define __NATEXEC_NATIVE_PTHREAD_RWLOCK_INITIALIZER     { 0 }


struct __natexec_native_pthread_mutexdata_t
{
    void *__pad1;
    unsigned int __pad2;
    int __kind;
};

typedef __natexec_native_pthread_mutexdata_t __natexec_native_pthread_mutex_t;

#define __NATEXEC_NATIVE_PTHREAD_MUTEX_INITIALIZER                  { .__pad1 = 0, .__pad2 = 0, .__kind = 0 }
#define __NATEXEC_NATIVE_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP     { .__pad1 = 0, .__pad2 = 0, .__kind = __NATEXEC_NATIVE_PTHREAD_MUTEX_RECURSIVE_NP }
#define __NATEXEC_NATIVE_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP    { .__pad1 = 0, .__pad2 = 0, .__kind = __NATEXEC_NATIVE_PTHREAD_MUTEX_ERRORCHECK_NP }
#define __NATEXEC_NATIVE_PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP      { .__pad1 = 0, .__pad2 = 0, .__kind = __NATEXEC_NATIVE_PTHREAD_MUTEX_ADAPTIVE_NP }

#define __NATEXEC_NATIVE_PTHREAD_GET_STATIC_MUTEX_KIND(mtx)         ( (mtx)->__kind )

typedef timespec __natexec_native_pthread_timespec_t;
typedef unsigned int __natexec_native_pthread_clockid_t;

struct __natexec_native_pthread_sched_param_t
{
    int sched_priority;
};

#endif //_NATEXEC_PTHREADS_CUSTOM_HEADER_
