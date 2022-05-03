/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.barrier.cpp
*  PURPOSE:     POSIX threads barrier implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "CExecutiveManager.pthread.hxx"

#include <errno.h>

using namespace NativeExecutive;

// *** BARRIER ATTR

int _natexec_pthread_barrierattr_init( pthread_barrierattr_t *attr )
{
    return 0;
}
int pthread_barrierattr_init( pthread_barrierattr_t *attr ) { return _natexec_pthread_barrierattr_init( attr ); }

int _natexec_pthread_barrierattr_destroy( pthread_barrierattr_t *attr )
{
    return 0;
}
int pthread_barrierattr_destroy( pthread_barrierattr_t *attr ) { return _natexec_pthread_barrierattr_destroy( attr ); }

int _natexec_pthread_barrierattr_getpshared( pthread_barrierattr_t *attr, int *pshared )
{
    *pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}
int pthread_barrierattr_getpshared( pthread_barrierattr_t *attr, int *pshared )
{ return _natexec_pthread_barrierattr_getpshared( attr, pshared ); }

int _natexec_pthread_barrierattr_setpshared( pthread_barrierattr_t *attr, int pshared )
{
    return ENOTSUP;
}
int pthread_barrierattr_setpshared( pthread_barrierattr_t *attr, int pshared )
{ return _natexec_pthread_barrierattr_setpshared( attr, pshared ); }

// *** BARRIER

int _natexec_pthread_barrier_init( pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count )
{
    (void)attr;

    CExecutiveManager *manager = get_executive_manager();

    CBarrier *impl_barrier = manager->CreateBarrier( count );

    if ( impl_barrier == nullptr )
    {
        return ENOMEM;
    }

    set_internal_pthread_barrier( barrier, impl_barrier );
    return 0;
}
int pthread_barrier_init( pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count )
{ return _natexec_pthread_barrier_init( barrier, attr, count ); }

int _natexec_pthread_barrier_destroy( pthread_barrier_t *barrier )
{
    CExecutiveManager *manager = get_executive_manager();

    CBarrier *impl_barrier = get_internal_pthread_barrier( barrier );

    manager->CloseBarrier( impl_barrier );
    return 0;
}
int pthread_barrier_destroy( pthread_barrier_t *barrier )
{ return _natexec_pthread_barrier_destroy( barrier ); }

int _natexec_pthread_barrier_wait( pthread_barrier_t *barrier )
{
    CBarrier *impl_barrier = get_internal_pthread_barrier( barrier );

    impl_barrier->Wait();
    return 0;
}
int pthread_barrier_wait( pthread_barrier_t *barrier )
{ return _natexec_pthread_barrier_wait( barrier ); }

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
