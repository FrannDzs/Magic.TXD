/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.mutex.cpp
*  PURPOSE:     POSIX threads mutex implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "CExecutiveManager.pthread.hxx"

#include <errno.h>

using namespace NativeExecutive;

// *** MUTEX ATTRIBUTES
int _natexec_pthread_mutexattr_init( pthread_mutexattr_t *attr )
{
    __natexec_impl_pthread_mutexattr_t *impl_attr = get_internal_pthread_mutexattr( attr );

    impl_attr->mtx_type = PTHREAD_MUTEX_DEFAULT;

    return 0;
}
int pthread_mutexattr_init( pthread_mutexattr_t *attr )
{ return _natexec_pthread_mutexattr_init( attr ); }

int _natexec_pthread_mutexattr_destroy( pthread_mutexattr_t *attr )
{
    return 0;
}
int pthread_mutexattr_destroy( pthread_mutexattr_t *attr ) { return _natexec_pthread_mutexattr_destroy( attr ); }

int _natexec_pthread_mutexattr_getprioceiling( const pthread_mutexattr_t *attr, int *prioceiling )
{
    return ENOTSUP;
}
int pthread_mutexattr_getprioceiling( const pthread_mutexattr_t *attr, int *prioceiling )
{ return _natexec_pthread_mutexattr_getprioceiling( attr, prioceiling ); }

int _natexec_pthread_mutexattr_setprioceiling( pthread_mutexattr_t *attr, int prioceiling )
{
    return ENOTSUP;
}
int pthread_mutexattr_setprioceiling( pthread_mutexattr_t *attr, int prioceiling )
{ return _natexec_pthread_mutexattr_setprioceiling( attr, prioceiling ); }

int _natexec_pthread_mutexattr_getprotocol( const pthread_mutexattr_t *attr, int *protocol )
{
    *protocol = PTHREAD_PRIO_NONE;
    return 0;
}
int pthread_mutexattr_getprotocol( const pthread_mutexattr_t *attr, int *protocol )
{ return _natexec_pthread_mutexattr_getprotocol( attr, protocol ); }

int _natexec_pthread_mutexattr_setprotocol( pthread_mutexattr_t *attr, int protocol )
{
    return ENOTSUP;
}
int pthread_mutexattr_setprotocol( pthread_mutexattr_t *attr, int protocol )
{ return _natexec_pthread_mutexattr_setprotocol( attr, protocol ); }

int _natexec_pthread_mutexattr_getrobust( const pthread_mutexattr_t *attr, int *robust )
{
    *robust = PTHREAD_MUTEX_STALLED;
    return 0;
}
int pthread_mutexattr_getrobust( const pthread_mutexattr_t *attr, int *robust )
{ return _natexec_pthread_mutexattr_getrobust( attr, robust ); }

int _natexec_pthread_mutexattr_setrobust( pthread_mutexattr_t *attr, int robust )
{
    return ENOTSUP;
}
int pthread_mutexattr_setrobust( pthread_mutexattr_t *attr, int robust )
{ return _natexec_pthread_mutexattr_setrobust( attr, robust ); }

int _natexec_pthread_mutexattr_getrobust_np( const pthread_mutexattr_t *attr, int *robust )
{
    *robust = PTHREAD_MUTEX_STALLED_NP;
    return 0;
}
int pthread_mutexattr_getrobust_np( const pthread_mutexattr_t *attr, int *robust )
{ return _natexec_pthread_mutexattr_getrobust_np( attr, robust ); }

int _natexec_pthread_mutexattr_setrobust_np( pthread_mutexattr_t *attr, int robust )
{
    return ENOTSUP;
}
int pthread_mutexattr_setrobust_np( pthread_mutexattr_t *attr, int robust )
{ return _natexec_pthread_mutexattr_setrobust_np( attr, robust ); }

int _natexec_pthread_mutexattr_getpshared( const pthread_mutexattr_t *attr, int *pshared )
{
    *pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}
int pthread_mutexattr_getpshared( const pthread_mutexattr_t *attr, int *pshared )
{ return _natexec_pthread_mutexattr_getpshared( attr, pshared ); }

int _natexec_pthread_mutexattr_setpshared( pthread_mutexattr_t *attr, int pshared )
{
    return ENOTSUP;
}
int pthread_mutexattr_setpshared( pthread_mutexattr_t *attr, int pshared )
{ return _natexec_pthread_mutexattr_setpshared( attr, pshared ); }

int _natexec_pthread_mutexattr_gettype( const pthread_mutexattr_t *attr, int *type )
{
    const __natexec_impl_pthread_mutexattr_t *impl_attr = get_const_internal_pthread_mutexattr( attr );

    *type = impl_attr->mtx_type;
    return 0;
}
int pthread_mutexattr_gettype( const pthread_mutexattr_t *attr, int *type )
{ return _natexec_pthread_mutexattr_gettype( attr, type ); }

int _natexec_pthread_mutexattr_settype( pthread_mutexattr_t *attr, int type )
{
    __natexec_impl_pthread_mutexattr_t *impl_attr = get_internal_pthread_mutexattr( attr );

    if ( type == PTHREAD_MUTEX_NORMAL ||
         type == PTHREAD_MUTEX_RECURSIVE )
    {
        impl_attr->mtx_type = (unsigned int)type;
        return 0;
    }
    else if ( type == PTHREAD_MUTEX_ERRORCHECK ||
              type == PTHREAD_MUTEX_ADAPTIVE_NP )
    {
        return ENOTSUP;
    }

    return EINVAL;
}
int pthread_mutexattr_settype( pthread_mutexattr_t *attr, int type )
{ return _natexec_pthread_mutexattr_settype( attr, type ); }

// *** MUTEX

int _pthread_mutex_init_implicit( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr, bool ignoreObjectFields = false )
{
    auto& init_data = get_internal_pthread_mutex( mutex );

    return _pthread_once_lambda( init_data.init,
        [&]( void )
        {
            try
            {
                CExecutiveManager *manager = get_executive_manager();

                int mtx_type = PTHREAD_MUTEX_DEFAULT;

                if ( ignoreObjectFields == false )
                {
                    mtx_type = _NATEXEC_PTHREAD_GET_STATIC_MUTEX_KIND( mutex );
                }

                if ( attr != nullptr )
                {
                    const __natexec_impl_pthread_mutexattr_t *impl_attr = get_const_internal_pthread_mutexattr( attr );

                    mtx_type = impl_attr->mtx_type;
                }

                __natexec_impl_pthread_mutex_t *impl_mutex = eir::static_new_struct <__natexec_impl_pthread_mutex_t, NatExecGlobalStaticAlloc> ( nullptr );

                if ( mtx_type == PTHREAD_MUTEX_NORMAL )
                {
                    CUnfairMutex *mtx_normal = manager->CreateUnfairMutex();

                    if ( mtx_normal == nullptr )
                    {
                        eir::static_del_struct <__natexec_impl_pthread_mutex_t, NatExecGlobalStaticAlloc> ( nullptr, impl_mutex );
                        return ENOMEM;
                    }

                    impl_mutex->mtx_type = PTHREAD_MUTEX_NORMAL;
                    impl_mutex->mtx_normal = mtx_normal;
                }
                else if ( mtx_type == PTHREAD_MUTEX_RECURSIVE )
                {
                    CThreadReentrantReadWriteLock *mtx_reentrant = manager->CreateThreadReentrantReadWriteLock();

                    if ( mtx_reentrant == nullptr )
                    {
                        eir::static_del_struct <__natexec_impl_pthread_mutex_t, NatExecGlobalStaticAlloc> ( nullptr, impl_mutex );
                        return ENOMEM;
                    }

                    impl_mutex->mtx_type = PTHREAD_MUTEX_RECURSIVE;
                    impl_mutex->mtx_reentrant = mtx_reentrant;
                }
                else
                {
                    eir::static_del_struct <__natexec_impl_pthread_mutex_t, NatExecGlobalStaticAlloc> ( nullptr, impl_mutex );
                    return ENOTSUP;
                }

                init_data.objptr = impl_mutex;
                return 0;
            }
            catch( eir::eir_exception& )
            {
                return ENOMEM;
            }
        }, 0
    );
}

int _natexec_pthread_mutex_init( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr )
{
    auto& init_data = get_internal_pthread_mutex( mutex );

    init_data.init = PTHREAD_ONCE_INIT;

    return _pthread_mutex_init_implicit( mutex, attr, true );
}
int pthread_mutex_init( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr )
{ return _natexec_pthread_mutex_init( mutex, attr ); }

int _natexec_pthread_mutex_destroy( pthread_mutex_t *mutex )
{
    CExecutiveManager *manager = get_executive_manager();

    auto& init_data = get_internal_pthread_mutex( mutex );

    __natexec_impl_pthread_mutex_t *impl_mutex = init_data.objptr;

    if ( init_data.init != PTHREAD_ONCE_INIT )
    {
        int mtx_type = impl_mutex->mtx_type;

        if ( mtx_type == PTHREAD_MUTEX_NORMAL )
        {
            manager->CloseUnfairMutex( impl_mutex->mtx_normal );
        }
        else if ( mtx_type == PTHREAD_MUTEX_RECURSIVE )
        {
            manager->CloseThreadReentrantReadWriteLock( impl_mutex->mtx_reentrant );
        }
        else
        {
            assert( 0 );
        }

        eir::static_del_struct <__natexec_impl_pthread_mutex_t, NatExecGlobalStaticAlloc> ( nullptr, impl_mutex );

        init_data.objptr = nullptr;
        init_data.init = PTHREAD_ONCE_INIT;
    }
    return 0;
}
int pthread_mutex_destroy( pthread_mutex_t *mutex )
{ return _natexec_pthread_mutex_destroy( mutex ); }

int _natexec_pthread_mutex_lock( pthread_mutex_t *mutex )
{
    if ( _pthread_mutex_init_implicit( mutex, nullptr ) != 0 )
    {
        return EINVAL;
    }

    __natexec_impl_pthread_mutex_t *impl_mutex = get_internal_pthread_mutex( mutex ).objptr;

#ifdef __linux__
    if ( impl_mutex == nullptr )
    {
        return EINVAL;
    }
#endif //__linux__

    int mtx_type = impl_mutex->mtx_type;

    if ( mtx_type == PTHREAD_MUTEX_NORMAL )
    {
        impl_mutex->mtx_normal->lock();
    }
    else if ( mtx_type == PTHREAD_MUTEX_RECURSIVE )
    {
        impl_mutex->mtx_reentrant->LockWrite();
    }
    else
    {
        return ENOTSUP;
    }

    return 0;
}
int pthread_mutex_lock( pthread_mutex_t *mutex )
{ return _natexec_pthread_mutex_lock( mutex ); }

int _natexec_pthread_mutex_trylock( pthread_mutex_t *mutex )
{
    if ( _pthread_mutex_init_implicit( mutex, nullptr ) != 0 )
    {
        return EINVAL;
    }

    __natexec_impl_pthread_mutex_t *impl_mutex = get_internal_pthread_mutex( mutex ).objptr;

#ifdef __linux__
    if ( impl_mutex == nullptr )
    {
        return EINVAL;
    }
#endif //__linux__

    int mtx_type = impl_mutex->mtx_type;

    bool could_lock;

    if ( mtx_type == PTHREAD_MUTEX_NORMAL )
    {
        could_lock = impl_mutex->mtx_normal->tryLock();
    }
    else if ( mtx_type == PTHREAD_MUTEX_RECURSIVE )
    {
        could_lock = impl_mutex->mtx_reentrant->TryLockWrite();
    }
    else
    {
        return ENOTSUP;
    }

    return ( could_lock ? 0 : EBUSY );
}
int pthread_mutex_trylock( pthread_mutex_t *mutex )
{ return _natexec_pthread_mutex_trylock( mutex ); }

int _natexec_pthread_mutex_timedlock( pthread_mutex_t *mutex, const timespec *abstime )
{
    if ( _pthread_mutex_init_implicit( mutex, nullptr ) != 0 )
    {
        return EINVAL;
    }

    unsigned int wait_ms = _timespec_to_ms( abstime );

    __natexec_impl_pthread_mutex_t *impl_mutex = get_internal_pthread_mutex( mutex ).objptr;

#ifdef __linux__
    if ( impl_mutex == nullptr )
    {
        return EINVAL;
    }
#endif //__linux__

    int mtx_type = impl_mutex->mtx_type;

    bool could_lock;

    if ( mtx_type == PTHREAD_MUTEX_NORMAL )
    {
        could_lock = impl_mutex->mtx_normal->tryTimedLock( wait_ms );
    }
    else if ( mtx_type == PTHREAD_MUTEX_RECURSIVE )
    {
        could_lock = impl_mutex->mtx_reentrant->TryTimedLockWrite( wait_ms );
    }
    else
    {
        return ENOTSUP;
    }

    return ( could_lock ? 0 : ETIMEDOUT );
}
int pthread_mutex_timedlock( pthread_mutex_t *mutex, const timespec *abstime )
{ return _natexec_pthread_mutex_timedlock( mutex, abstime ); }

int _natexec_pthread_mutex_unlock( pthread_mutex_t *mutex )
{
    if ( _pthread_mutex_init_implicit( mutex, nullptr ) != 0 )
    {
        return EINVAL;
    }

    __natexec_impl_pthread_mutex_t *impl_mutex = get_internal_pthread_mutex( mutex ).objptr;

#ifdef __linux__
    if ( impl_mutex == nullptr )
    {
        return EINVAL;
    }
#endif //__linux__

    int mtx_type = impl_mutex->mtx_type;

    if ( mtx_type == PTHREAD_MUTEX_NORMAL )
    {
        impl_mutex->mtx_normal->unlock();
    }
    else if ( mtx_type == PTHREAD_MUTEX_RECURSIVE )
    {
        impl_mutex->mtx_reentrant->UnlockWrite();
    }
    else
    {
        return ENOTSUP;
    }

    return 0;
}
int pthread_mutex_unlock( pthread_mutex_t *mutex )
{ return _natexec_pthread_mutex_unlock( mutex ); }

int _natexec_pthread_mutex_transfer_np( pthread_mutex_t *mutex, pthread_t tid )
{
    if ( _pthread_mutex_init_implicit( mutex, nullptr ) != 0 )
    {
        return EINVAL;
    }

    __natexec_impl_pthread_mutex_t *impl_mutex = get_internal_pthread_mutex( mutex ).objptr;

#ifdef __linux__
    if ( impl_mutex == nullptr )
    {
        return EINVAL;
    }
#endif //__linux__

    int mtx_type = impl_mutex->mtx_type;

    if ( mtx_type == PTHREAD_MUTEX_NORMAL )
    {
        return 0;
    }
    else if ( mtx_type == PTHREAD_MUTEX_RECURSIVE )
    {
        return ENOTSUP;
    }

    return ENOTSUP;
}
int pthread_mutex_transfer_np( pthread_mutex_t *mutex, pthread_t tid )
{ return _natexec_pthread_mutex_transfer_np( mutex, tid ); }

int _natexec_pthread_mutex_getprioceiling( const pthread_mutex_t *mutex, int *prioceiling )
{
    return ENOTSUP;
}
int pthread_mutex_getprioceiling( const pthread_mutex_t *mutex, int *prioceiling )
{ return _natexec_pthread_mutex_getprioceiling( mutex, prioceiling ); }

int _natexec_pthread_mutex_setprioceiling( pthread_mutex_t *mutex, int prio, int *oldprio )
{
    return ENOTSUP;
}
int pthread_mutex_setprioceiling( pthread_mutex_t *mutex, int prio, int *oldprio )
{ return _natexec_pthread_mutex_setprioceiling( mutex, prio, oldprio ); }

int _natexec_pthread_mutex_consistent( pthread_mutex_t *mutex )
{
    return ENOTSUP;
}
int pthread_mutex_consistent( pthread_mutex_t *mutex )
{ return _natexec_pthread_mutex_consistent( mutex ); }

int _natexec_pthread_mutex_consistent_np( pthread_mutex_t *mutex )
{
    return ENOTSUP;
}
int pthread_mutex_consistent_np( pthread_mutex_t *mutex )
{ return _natexec_pthread_mutex_consistent_np( mutex ); }

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
