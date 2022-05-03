/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.hxx
*  PURPOSE:     POSIX threads thread implementation internal header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_PTHREAD_INTERNAL_HEADER_
#define _NATEXEC_PTHREAD_INTERNAL_HEADER_

#include "CExecutiveManager.pthread.internal.h"

#include "CExecutiveManager.h"

struct __natexec_impl_pthread_attr_t
{
    size_t stacksize;
    bool create_joinable;
};

// We store one pointer inside of the pthread_attr_t struct because it should be
// big enough on every implementation.
static_assert( sizeof(pthread_attr_t) >= sizeof(void*), "cannot store pointer inside pthread_attr_t" );

inline __natexec_impl_pthread_attr_t*& get_internal_pthread_attr( pthread_attr_t *attr )
{
    return *(__natexec_impl_pthread_attr_t**)attr;
}

inline const __natexec_impl_pthread_attr_t* get_const_internal_pthread_attr( const pthread_attr_t *attr )
{
    return *(const __natexec_impl_pthread_attr_t**)attr;
}

inline void set_internal_pthread_attr( pthread_attr_t *attr, __natexec_impl_pthread_attr_t *impl_attr )
{
    get_internal_pthread_attr( attr ) = impl_attr;
}

static_assert( sizeof(pthread_t) >= sizeof(void*), "cannot store pointer inside pthread_t" );

struct __natexec_impl_pthread_mutexattr_t
{
    unsigned char mtx_type;
};

static_assert( sizeof(pthread_mutexattr_t) >= sizeof(__natexec_impl_pthread_mutexattr_t), "cannot store mutex attributes inside pthread_mutexattr_t" );

inline __natexec_impl_pthread_mutexattr_t* get_internal_pthread_mutexattr( pthread_mutexattr_t *attr )
{
    return (__natexec_impl_pthread_mutexattr_t*)attr;
}

inline const __natexec_impl_pthread_mutexattr_t* get_const_internal_pthread_mutexattr( const pthread_mutexattr_t *attr )
{
    return (const __natexec_impl_pthread_mutexattr_t*)attr;
}

template <typename structType>
struct __natexec_impl_static_initializable_t
{
    structType *objptr;
    pthread_once_t init;
};

struct __natexec_impl_pthread_mutex_t
{
    unsigned char mtx_type;
    union
    {
        NativeExecutive::CUnfairMutex *mtx_normal;
        NativeExecutive::CThreadReentrantReadWriteLock *mtx_reentrant;
    };
};

static_assert(
    sizeof(pthread_mutex_t) >= sizeof(__natexec_impl_static_initializable_t <__natexec_impl_pthread_mutex_t>),
    "cannot store maintenance data inside pthread_mutex_t"
);

inline __natexec_impl_static_initializable_t <__natexec_impl_pthread_mutex_t>& get_internal_pthread_mutex( pthread_mutex_t *mutex )
{
    return *(__natexec_impl_static_initializable_t <__natexec_impl_pthread_mutex_t>*)mutex;
}

inline const __natexec_impl_static_initializable_t <__natexec_impl_pthread_mutex_t>& get_const_internal_pthread_mutex( const pthread_mutex_t *mutex )
{
    return *(const __natexec_impl_static_initializable_t <__natexec_impl_pthread_mutex_t>*)mutex;
}

static_assert(
    sizeof(pthread_cond_t) >= sizeof(__natexec_impl_static_initializable_t <NativeExecutive::CCondVar>),
    "cannot store maintenance data inside pthread_cond_t"
);

inline __natexec_impl_static_initializable_t <NativeExecutive::CCondVar>& get_internal_pthread_cond( pthread_cond_t *cond )
{
    return *(__natexec_impl_static_initializable_t <NativeExecutive::CCondVar>*)cond;
}

static_assert(
    sizeof(pthread_rwlock_t) >= sizeof(__natexec_impl_static_initializable_t <NativeExecutive::CFairReadWriteLock>),
    "cannot store pointer inside pthread_rwlock_t"
);

inline __natexec_impl_static_initializable_t <NativeExecutive::CFairReadWriteLock>& get_internal_pthread_rwlock( pthread_rwlock_t *rwlock )
{
    return *(__natexec_impl_static_initializable_t <NativeExecutive::CFairReadWriteLock>*)rwlock;
}

static_assert( sizeof(pthread_barrier_t) >= sizeof(void*), "cannot store pointer inside pthread_barrier_t" );

inline NativeExecutive::CBarrier*& get_internal_pthread_barrier( pthread_barrier_t *barrier )
{
    return *(NativeExecutive::CBarrier**)barrier;
}

inline void set_internal_pthread_barrier( pthread_barrier_t *barrier, NativeExecutive::CBarrier *impl_barrier )
{
    get_internal_pthread_barrier( barrier ) = impl_barrier;
}

static_assert( sizeof(sem_t) >= sizeof(void*), "cannot store pointer inside sem_t" );

inline NativeExecutive::CSemaphore*& get_internal_pthread_semaphore( sem_t *sem )
{
    return *(NativeExecutive::CSemaphore**)sem;
}

inline void set_internal_pthread_semaphore( sem_t *sem, NativeExecutive::CSemaphore *impl_sem )
{
    get_internal_pthread_semaphore( sem ) = impl_sem;
}

NativeExecutive::CExecutiveManager* get_executive_manager( void );

// Helpers.
inline unsigned int _timespec_to_ms( const timespec *abstime )
{
    unsigned int ms_out = 0;
    ms_out += (unsigned int)( abstime->tv_sec * 1000 );
    ms_out += (unsigned int)( abstime->tv_nsec / 1000000 );
    return ms_out;
}

template <typename callbackType, typename returnType>
inline returnType _pthread_once_lambda( pthread_once_t& once_control, const callbackType& cb, returnType default_val )
{
    struct control_data
    {
        returnType *return_ptr;
        const callbackType *cb;
    };

    auto static_cb = []( void *param )
    {
        control_data *ctl = (control_data*)param;

        *ctl->return_ptr = (*ctl->cb)();
    };

    control_data data;
    data.return_ptr = &default_val;
    data.cb = &cb;

    pthread_once_ex_np( &once_control, static_cb, &data );

    return default_val;
}

#endif //_NATEXEC_PTHREAD_INTERNAL_HEADER_
