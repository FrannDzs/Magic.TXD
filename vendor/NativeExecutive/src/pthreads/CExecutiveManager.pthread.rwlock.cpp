/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.rwlock.cpp
*  PURPOSE:     POSIX threads rwlock implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "CExecutiveManager.pthread.hxx"

#include <sdk/Map.h>

#include <errno.h>

using namespace NativeExecutive;

enum class pthRwlockEnterMode
{
    NOT_ENTERED,
    WRITER,
    READER
};

struct _pthread_rwlock_enterinfo_t
{
    pthRwlockEnterMode entermode = pthRwlockEnterMode::NOT_ENTERED;
    unsigned int entercount = 0;
};

struct _pthread_rwlock_maintenance_t
{
    // We must keep track of which locks we entered how.
    eir::Map <CFairReadWriteLock*, _pthread_rwlock_enterinfo_t, NatExecGlobalStaticAlloc> locks_entered;
};

static constinit optional_struct_space <execThreadStructPluginRegister <_pthread_rwlock_maintenance_t>> _pthread_rwlock_maintenance_reg;

void _pthread_registerRwlockMaintenance( CExecutiveManager *execMan )
{
    _pthread_rwlock_maintenance_reg.Construct();
    _pthread_rwlock_maintenance_reg.get().RegisterPlugin( execMan );
}

void _pthread_unregisterRwlockMaintenance( void )
{
    _pthread_rwlock_maintenance_reg.get().UnregisterPlugin();
    _pthread_rwlock_maintenance_reg.Destroy();
}

inline static _pthread_rwlock_maintenance_t* get_current_rwlock_maintenance( void )
{
    return _pthread_rwlock_maintenance_reg.get().GetPluginStructCurrent();
}

// *** RWLOCK ATTR

int _natexec_pthread_rwlockattr_init( pthread_rwlockattr_t *attr )
{
    return 0;
}
int pthread_rwlockatr_init( pthread_rwlockattr_t *attr )    { return _natexec_pthread_rwlockattr_init( attr ); }

int _natexec_pthread_rwlockattr_destroy( pthread_rwlockattr_t *attr )
{
    return 0;
}
int pthread_rwlockattr_destroy( pthread_rwlockattr_t *attr )    { return _natexec_pthread_rwlockattr_destroy( attr ); }

int _natexec_pthread_rwlockattr_getpshared( const pthread_rwlockattr_t *attr, int *pshared )
{
    *pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}
int pthread_rwlockattr_getpshared( const pthread_rwlockattr_t *attr, int *pshared )
{ return _natexec_pthread_rwlockattr_getpshared( attr, pshared ); }

int _natexec_pthread_rwlockattr_setpshared( pthread_rwlockattr_t *attr, int pshared )
{
    return ENOTSUP;
}
int pthread_rwlockattr_setpshared( pthread_rwlockattr_t *attr, int pshared )
{ return _natexec_pthread_rwlockattr_setpshared( attr, pshared ); }

int _natexec_pthread_rwlockattr_getkind_np( const pthread_rwlockattr_t *attr, int *pref )
{
    // Not sure what to put here. Just go with the default.
    *pref = PTHREAD_RWLOCK_PREFER_READER_NP;
    return 0;
}
int pthread_rwlockattr_getkind_np( const pthread_rwlockattr_t *attr, int *pref )
{ return _natexec_pthread_rwlockattr_getkind_np( attr, pref ); }

int _natexec_pthread_rwlockattr_setkind_np( pthread_rwlockattr_t *attr, int pref )
{
    return ENOTSUP;
}
int pthread_rwlockattr_setkind_np( pthread_rwlockattr_t *attr, int pref )
{ return _natexec_pthread_rwlockattr_setkind_np( attr, pref ); }

// *** RWLOCK

static inline int _pthread_rwlock_init_implicit( pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr )
{
    (void)attr;

    auto& init_data = get_internal_pthread_rwlock( rwlock );

    return _pthread_once_lambda( init_data.init,
        [&]( void )
        {
            CExecutiveManager *manager = get_executive_manager();

            CFairReadWriteLock *impl_rwlock = manager->CreateFairReadWriteLock();

            if ( impl_rwlock == nullptr )
            {
                return ENOMEM;
            }

            init_data.objptr = impl_rwlock;
            return 0;
        }, 0
    );
}

int _natexec_pthread_rwlock_init( pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr )
{
    auto& init_data = get_internal_pthread_rwlock( rwlock );

    init_data.init = PTHREAD_ONCE_INIT;

    return _pthread_rwlock_init_implicit( rwlock, attr );
}
int pthread_rwlock_init( pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr )
{ return _natexec_pthread_rwlock_init( rwlock, attr ); }

int _natexec_pthread_rwlock_destroy( pthread_rwlock_t *rwlock )
{
    CExecutiveManager *manager = get_executive_manager();

    auto& init_data = get_internal_pthread_rwlock( rwlock );

    CFairReadWriteLock *impl_rwlock = init_data.objptr;

    if ( init_data.init != PTHREAD_ONCE_INIT )
    {
        manager->CloseFairReadWriteLock( impl_rwlock );

        init_data.objptr = nullptr;
        init_data.init = PTHREAD_ONCE_INIT;
    }
    return 0;
}
int pthread_rwlock_destroy( pthread_rwlock_t *rwlock )  { return _natexec_pthread_rwlock_destroy( rwlock ); }

int _natexec_pthread_rwlock_rdlock( pthread_rwlock_t *rwlock )
{
    if ( _pthread_rwlock_init_implicit( rwlock, nullptr ) != 0 )
    {
        return EINVAL;
    }

    CFairReadWriteLock *impl_rwlock = get_internal_pthread_rwlock( rwlock ).objptr;

    _pthread_rwlock_maintenance_t *maintain = get_current_rwlock_maintenance();

    _pthread_rwlock_enterinfo_t& enterinfo = maintain->locks_entered[ impl_rwlock ];

    if ( enterinfo.entermode == pthRwlockEnterMode::WRITER )
    {
        return EDEADLK;
    }

    enterinfo.entermode = pthRwlockEnterMode::READER;
    enterinfo.entercount++;

    impl_rwlock->EnterCriticalReadRegion();

    return 0;
}
int pthread_rwlock_rdlock( pthread_rwlock_t *rwlock )   { return _natexec_pthread_rwlock_rdlock( rwlock ); }

static inline void _gc_enterinfo( _pthread_rwlock_maintenance_t *maintain, CFairReadWriteLock *lock, _pthread_rwlock_enterinfo_t& enterinfo )
{
    if ( enterinfo.entercount == 0 )
    {
        maintain->locks_entered.RemoveByKey( lock );
    }
}

int _natexec_pthread_rwlock_tryrdlock( pthread_rwlock_t *rwlock )
{
    if ( _pthread_rwlock_init_implicit( rwlock, nullptr ) != 0 )
    {
        return EINVAL;
    }

    CFairReadWriteLock *impl_rwlock = get_internal_pthread_rwlock( rwlock ).objptr;

    _pthread_rwlock_maintenance_t *maintain = get_current_rwlock_maintenance();

    _pthread_rwlock_enterinfo_t& enterinfo = maintain->locks_entered[ impl_rwlock ];

    if ( enterinfo.entermode == pthRwlockEnterMode::WRITER )
    {
        return EDEADLK;
    }

    bool couldEnter = impl_rwlock->TryEnterCriticalReadRegion();

    if ( couldEnter == false )
    {
        _gc_enterinfo( maintain, impl_rwlock, enterinfo );
        return EBUSY;
    }

    enterinfo.entermode = pthRwlockEnterMode::READER;
    enterinfo.entercount++;

    return 0;
}
int pthread_rwlock_tryrdlock( pthread_rwlock_t *rwlock )    { return _natexec_pthread_rwlock_tryrdlock( rwlock ); }

int _natexec_pthread_rwlock_timedrdlock( pthread_rwlock_t *rwlock, const timespec *abstime )
{
    if ( _pthread_rwlock_init_implicit( rwlock, nullptr ) != 0 )
    {
        return EINVAL;
    }

    unsigned int waitMS = _timespec_to_ms( abstime );

    CFairReadWriteLock *impl_rwlock = get_internal_pthread_rwlock( rwlock ).objptr;

    _pthread_rwlock_maintenance_t *maintain = get_current_rwlock_maintenance();

    _pthread_rwlock_enterinfo_t& enterinfo = maintain->locks_entered[ impl_rwlock ];

    if ( enterinfo.entermode == pthRwlockEnterMode::WRITER )
    {
        return EDEADLK;
    }

    bool couldEnter = impl_rwlock->TryTimedEnterCriticalReadRegion( waitMS );

    if ( couldEnter == false )
    {
        _gc_enterinfo( maintain, impl_rwlock, enterinfo );
        return ETIMEDOUT;
    }

    enterinfo.entermode = pthRwlockEnterMode::READER;
    enterinfo.entercount++;

    return 0;
}
int pthread_rwlock_timedrdlock( pthread_rwlock_t *rwlock, const timespec *abstime )
{ return _natexec_pthread_rwlock_timedrdlock( rwlock, abstime ); }

int _natexec_pthread_rwlock_wrlock( pthread_rwlock_t *rwlock )
{
    if ( _pthread_rwlock_init_implicit( rwlock, nullptr ) != 0 )
    {
        return EINVAL;
    }

    CFairReadWriteLock *impl_rwlock = get_internal_pthread_rwlock( rwlock ).objptr;

    _pthread_rwlock_maintenance_t *maintain = get_current_rwlock_maintenance();

    if ( maintain->locks_entered.Find( impl_rwlock ) != nullptr )
    {
        return EDEADLK;
    }

    _pthread_rwlock_enterinfo_t& enterinfo = maintain->locks_entered[ impl_rwlock ];
    enterinfo.entermode = pthRwlockEnterMode::WRITER;
    enterinfo.entercount = 1;

    impl_rwlock->EnterCriticalWriteRegion();

    return 0;
}
int pthread_rwlock_wrlock( pthread_rwlock_t *rwlock )   { return _natexec_pthread_rwlock_wrlock( rwlock ); }

int _natexec_pthread_rwlock_trywrlock( pthread_rwlock_t *rwlock )
{
    if ( _pthread_rwlock_init_implicit( rwlock, nullptr ) != 0 )
    {
        return EINVAL;
    }

    CFairReadWriteLock *impl_rwlock = get_internal_pthread_rwlock( rwlock ).objptr;

    _pthread_rwlock_maintenance_t *maintain = get_current_rwlock_maintenance();

    if ( maintain->locks_entered.Find( impl_rwlock ) != nullptr )
    {
        return EDEADLK;
    }

    _pthread_rwlock_enterinfo_t& enterinfo = maintain->locks_entered[ impl_rwlock ];

    bool couldEnter = impl_rwlock->TryEnterCriticalWriteRegion();

    if ( couldEnter == false )
    {
        _gc_enterinfo( maintain, impl_rwlock, enterinfo );
        return EBUSY;
    }

    enterinfo.entermode = pthRwlockEnterMode::WRITER;
    enterinfo.entercount = 1;

    return 0;
}
int pthread_rwlock_trywrlock( pthread_rwlock_t *rwlock )    { return _natexec_pthread_rwlock_trywrlock( rwlock ); }

int _natexec_pthread_rwlock_timedwrlock( pthread_rwlock_t *rwlock, const timespec *abstime )
{
    if ( _pthread_rwlock_init_implicit( rwlock, nullptr ) != 0 )
    {
        return EINVAL;
    }

    unsigned int waitMS = _timespec_to_ms( abstime );

    CFairReadWriteLock *impl_rwlock = get_internal_pthread_rwlock( rwlock ).objptr;

    _pthread_rwlock_maintenance_t *maintain = get_current_rwlock_maintenance();

    if ( maintain->locks_entered.Find( impl_rwlock ) != nullptr )
    {
        return EDEADLK;
    }

    _pthread_rwlock_enterinfo_t& enterinfo = maintain->locks_entered[ impl_rwlock ];

    bool couldEnter = impl_rwlock->TryTimedEnterCriticalWriteRegion( waitMS );

    if ( couldEnter == false )
    {
        _gc_enterinfo( maintain, impl_rwlock, enterinfo );
        return ETIMEDOUT;
    }

    enterinfo.entermode = pthRwlockEnterMode::WRITER;
    enterinfo.entercount = 1;

    return 0;
}
int pthread_rwlock_timedwrlock( pthread_rwlock_t *rwlock, const timespec *abstime )
{ return _natexec_pthread_rwlock_timedwrlock( rwlock, abstime ); }

int _natexec_pthread_rwlock_unlock( pthread_rwlock_t *rwlock )
{
    if ( _pthread_rwlock_init_implicit( rwlock, nullptr ) != 0 )
    {
        return EINVAL;
    }

    CFairReadWriteLock *impl_rwlock = get_internal_pthread_rwlock( rwlock ).objptr;

    _pthread_rwlock_maintenance_t *maintain = get_current_rwlock_maintenance();

    auto *node = maintain->locks_entered.Find( impl_rwlock );

    if ( node == nullptr )
    {
        return EPERM;
    }

    _pthread_rwlock_enterinfo_t& enterinfo = node->GetValue();

    if ( enterinfo.entercount == 0 )
    {
        return EPERM;
    }

    pthRwlockEnterMode entermode = enterinfo.entermode;

    if ( entermode == pthRwlockEnterMode::READER )
    {
        impl_rwlock->LeaveCriticalReadRegion();
    }
    else if ( entermode == pthRwlockEnterMode::WRITER )
    {
        impl_rwlock->LeaveCriticalWriteRegion();
    }
    else
    {
        return ENOTSUP;
    }

    enterinfo.entercount--;

    _gc_enterinfo( maintain, impl_rwlock, enterinfo );
    return 0;
}
int pthread_rwlock_unlock( pthread_rwlock_t *rwlock )   { return _natexec_pthread_rwlock_unlock( rwlock ); }

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
