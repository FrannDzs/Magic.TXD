/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.sem.cpp
*  PURPOSE:     POSIX threads semaphore implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "CExecutiveManager.pthread.hxx"

#include <errno.h>

using namespace NativeExecutive;

// *** SEMAPHORES (GNU)

int _natexec_sem_init( sem_t *sem, int pshared, unsigned int value )
{
    if ( pshared != PTHREAD_PROCESS_PRIVATE )
    {
        errno = ENOTSUP;
        return -1;
    }

    CExecutiveManager *manager = get_executive_manager();

    CSemaphore *impl_sem = manager->CreateSemaphore();

    for ( unsigned int n = 0; n < value; n++ )
    {
        impl_sem->Increment();
    }

    set_internal_pthread_semaphore( sem, impl_sem );
    return 0;
}
int sem_init( sem_t *sem, int pshared, unsigned int value )
{ return _natexec_sem_init( sem, pshared, value ); }

int _natexec_sem_destroy( sem_t *sem )
{
    CExecutiveManager *manager = get_executive_manager();

    CSemaphore *impl_sem = get_internal_pthread_semaphore( sem );

    manager->CloseSemaphore( impl_sem );
    return 0;
}
int sem_destroy( sem_t *sem )   { return _natexec_sem_destroy( sem ); }

// not implemented as internal routine.
sem_t* sem_open( const char *name, int oflag, ... )
{
    errno = ENOTSUP;
    return SEM_FAILED;
}

int _natexec_sem_close( sem_t *sem )
{
    errno = ENOTSUP;
    return -1;
}
int sem_close( sem_t *sem ) { return _natexec_sem_close( sem ); }

int _natexec_sem_unlink( const char *name )
{
    errno = ENOTSUP;
    return -1;
}
int sem_unlink( const char *name )  { return _natexec_sem_unlink( name ); }

int _natexec_sem_wait( sem_t *sem )
{
    CSemaphore *impl_sem = get_internal_pthread_semaphore( sem );

    impl_sem->Decrement();
    return 0;
}
int sem_wait( sem_t *sem )  { return _natexec_sem_wait( sem ); }

int _natexec_sem_timedwait( sem_t *sem, const timespec *abstime )
{
    unsigned int waitMS = _timespec_to_ms( abstime );

    CSemaphore *impl_sem = get_internal_pthread_semaphore( sem );

    bool decrement_success = impl_sem->TryTimedDecrement( waitMS );

    if ( decrement_success == false )
    {
        errno = ETIMEDOUT;
        return -1;
    }

    return 0;
}
int sem_timedwait( sem_t *sem, const timespec *abstime )
{ return _natexec_sem_timedwait( sem, abstime ); }

int _natexec_sem_trywait( sem_t *sem )
{
    CSemaphore *impl_sem = get_internal_pthread_semaphore( sem );

    bool decrement_success = impl_sem->TryDecrement();

    if ( decrement_success == false )
    {
        errno = EBUSY;
        return -1;
    }

    return 0;
}
int sem_trywait( sem_t *sem )   { return _natexec_sem_trywait( sem ); }

int _natexec_sem_post( sem_t *sem )
{
    CSemaphore *impl_sem = get_internal_pthread_semaphore( sem );

    try
    {
        impl_sem->Increment();
    }
    catch( ... )
    {
        errno = EINVAL;
        return -1;
    }

    return 0;
}
int sem_post( sem_t *sem )  { return _natexec_sem_post( sem ); }

int _natexec_sem_getvalue( sem_t *sem, int *sval )
{
    CSemaphore *impl_sem = get_internal_pthread_semaphore( sem );

    *sval = (int)impl_sem->GetValue();
    return 0;
}
int sem_getvalue( sem_t *sem, int *sval )
{ return _natexec_sem_getvalue( sem, sval ); }

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION