/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.cond.cpp
*  PURPOSE:     POSIX threads cond-var implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "CExecutiveManager.pthread.hxx"

#include <errno.h>

using namespace NativeExecutive;

// *** CONDITION ATTR

int _natexec_pthread_condattr_init( pthread_condattr_t *attr )
{
    return 0;
}
int pthread_condattr_init( pthread_condattr_t *attr )   { return _natexec_pthread_condattr_init( attr ); }

int _natexec_pthread_condattr_destroy( pthread_condattr_t *attr )
{
    return 0;
}
int pthread_condattr_destroy( pthread_condattr_t *attr )    { return _natexec_pthread_condattr_destroy( attr ); }

int _natexec_pthread_condattr_getclock( const pthread_condattr_t *attr, clockid_t *clockid )
{
    *clockid = 0;
    return 0;
}
int pthread_condattr_getclock( const pthread_condattr_t *attr, clockid_t *clockid )
{ return _natexec_pthread_condattr_getclock( attr, clockid ); }

int _natexec_pthread_condattr_setclock( pthread_condattr_t *attr, clockid_t clockid )
{
    return ENOTSUP;
}
int pthread_condattr_setclock( pthread_condattr_t *attr, clockid_t clockid )
{ return _natexec_pthread_condattr_setclock( attr, clockid ); }

int _natexec_pthread_condattr_getpshared( const pthread_condattr_t *attr, int *pshared )
{
    *pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}
int pthread_condattr_getpshared( const pthread_condattr_t *attr, int *pshared )
{ return _natexec_pthread_condattr_getpshared( attr, pshared ); }

int _natexec_pthread_condattr_setpshared( pthread_condattr_t *attr, int pshared )
{
    return ENOTSUP;
}
int pthread_condattr_setpshared( pthread_condattr_t *attr, int pshared )
{ return _natexec_pthread_condattr_setpshared( attr, pshared ); }

// *** CONDITION

static int _pthread_cond_init_implicit( pthread_cond_t *cond, const pthread_condattr_t *attr )
{
    (void)attr;

    auto& init_data = get_internal_pthread_cond( cond );

    return _pthread_once_lambda( init_data.init,
        [&]( void )
        {
            CExecutiveManager *manager = get_executive_manager();

            CCondVar *impl_cond = manager->CreateConditionVariable();

            if ( impl_cond == nullptr )
            {
                return ENOMEM;
            }

            init_data.objptr = impl_cond;
            return 0;
        }, 0
    );
}

int _natexec_pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t *attr )
{
    auto& init_data = get_internal_pthread_cond( cond );

    init_data.init = PTHREAD_ONCE_INIT;

    return _pthread_cond_init_implicit( cond, attr );
}
int pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t *attr )
{ return _natexec_pthread_cond_init( cond, attr ); }

int _natexec_pthread_cond_destroy( pthread_cond_t *cond )
{
    CExecutiveManager *manager = get_executive_manager();

    auto& init_data = get_internal_pthread_cond( cond );

    CCondVar *impl_cond = init_data.objptr;

    if ( init_data.init != PTHREAD_ONCE_INIT )
    {
        manager->CloseConditionVariable( impl_cond );
    
        init_data.objptr = nullptr;
        init_data.init = PTHREAD_ONCE_INIT;
    }
    return 0;
}
int pthread_cond_destroy( pthread_cond_t *cond )    { return _natexec_pthread_cond_destroy( cond ); }

int _natexec_pthread_cond_signal( pthread_cond_t *cond )
{
    if ( _pthread_cond_init_implicit( cond, nullptr ) != 0 )
    {
        return EINVAL;
    }

    CCondVar *impl_cond = get_internal_pthread_cond( cond ).objptr;

    impl_cond->SignalCount( 1 );
    return 0;
}
int pthread_cond_signal( pthread_cond_t *cond ) { return _natexec_pthread_cond_signal( cond ); }

int _natexec_pthread_cond_broadcast( pthread_cond_t *cond )
{
    if ( _pthread_cond_init_implicit( cond, nullptr ) != 0 )
    {
        return EINVAL;
    }

    CCondVar *impl_cond = get_internal_pthread_cond( cond ).objptr;

    impl_cond->Signal();
    return 0;
}
int pthread_cond_broadcast( pthread_cond_t *cond )  { return _natexec_pthread_cond_broadcast( cond ); }

// Implemented in the mutex file.
int _pthread_mutex_init_implicit( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr, bool ignoreObjectFields = false );

int _natexec_pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex )
{
    if ( _pthread_cond_init_implicit( cond, nullptr ) != 0 )
    {
        return EINVAL;
    }

    if ( _pthread_mutex_init_implicit( mutex, nullptr ) != 0 )
    {
        return EINVAL;
    }

    CCondVar *impl_cond = get_internal_pthread_cond( cond ).objptr;
    
    __natexec_impl_pthread_mutex_t *impl_mutex = get_internal_pthread_mutex( mutex ).objptr;

    int mtx_type = impl_mutex->mtx_type;

    if ( mtx_type == PTHREAD_MUTEX_NORMAL )
    {
        impl_cond->WaitByLock( impl_mutex->mtx_normal );
    }
    else if ( mtx_type == PTHREAD_MUTEX_RECURSIVE )
    {
        impl_cond->WaitByLock( impl_mutex->mtx_reentrant );
    }
    else
    {
        return ENOTSUP;
    }

    return 0;
}
int pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex )
{ return _natexec_pthread_cond_wait( cond, mutex ); }

int _natexec_pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex, const timespec *abstime )
{
    if ( _pthread_cond_init_implicit( cond, nullptr ) != 0 )
    {
        return EINVAL;
    }

    if ( _pthread_mutex_init_implicit( mutex, nullptr ) != 0 )
    {
        return EINVAL;
    }

    unsigned int waitMS = _timespec_to_ms( abstime );

    CCondVar *impl_cond = get_internal_pthread_cond( cond ).objptr;

    __natexec_impl_pthread_mutex_t *impl_mutex = get_internal_pthread_mutex( mutex ).objptr;

    int mtx_type = impl_mutex->mtx_type;

    bool wasInterrupted;

    if ( mtx_type == PTHREAD_MUTEX_NORMAL )
    {
        wasInterrupted = impl_cond->WaitTimedByLock( impl_mutex->mtx_normal, waitMS );
    }
    else if ( mtx_type == PTHREAD_MUTEX_RECURSIVE )
    {
        wasInterrupted = impl_cond->WaitTimedByLock( impl_mutex->mtx_reentrant, waitMS );
    }
    else
    {
        return ENOTSUP;
    }

    return ( wasInterrupted ? 0 : ETIMEDOUT );
}
int pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex, const timespec *abstime )
{ return _natexec_pthread_cond_timedwait( cond, mutex, abstime ); }

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION