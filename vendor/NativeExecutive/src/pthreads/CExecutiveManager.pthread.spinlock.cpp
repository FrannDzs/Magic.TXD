/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.spinlock.cpp
*  PURPOSE:     POSIX threads spinlock implementation.
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

typedef eir::Map <pthread_spinlock_t*, CSpinLock*, NatExecGlobalStaticAlloc> pth_spinlocks_t;

inline static pth_spinlocks_t& get_spin_lock_map( void )
{
    // IMPORTANT: wait for support of consteval so we can make this call thread-safe!
    static pth_spinlocks_t pth_spinlocks;

    return pth_spinlocks;
}

inline static CSpinLock& get_spin_lock_map_lock( void )
{
    // Same as above.
    static CSpinLock pth_spinlocks_lock;

    return pth_spinlocks_lock;
}

// *** SPIN LOCKS

int _natexec_pthread_spin_init( pthread_spinlock_t *lock, int pshared )
{
    if ( pshared != PTHREAD_PROCESS_PRIVATE )
    {
        return ENOTSUP;
    }

    CExecutiveManager *manager = get_executive_manager();

    pth_spinlocks_t& map = get_spin_lock_map();

    CSpinLockContext ctx_map( get_spin_lock_map_lock() );

    CSpinLock *spin = manager->CreateSpinLock();

    map.Set( lock, spin );
    return 0;
}
int pthread_spin_init( pthread_spinlock_t *lock, int pshared )
{ return _natexec_pthread_spin_init( lock, pshared ); }

int _natexec_pthread_spin_destroy( pthread_spinlock_t *lock )
{
    CExecutiveManager *manager = get_executive_manager();

    pth_spinlocks_t& map = get_spin_lock_map();

    CSpinLockContext ctx_map( get_spin_lock_map_lock() );

    auto *node = map.Find( lock );

    if ( node == nullptr )
    {
        return EINVAL;
    }

    manager->CloseSpinLock( node->GetValue() );

    map.RemoveNode( node );
    return 0;
}
int pthread_spin_destroy( pthread_spinlock_t *lock )    { return _natexec_pthread_spin_destroy( lock ); }

int _natexec_pthread_spin_lock( pthread_spinlock_t *lock )
{
    pth_spinlocks_t& map = get_spin_lock_map();

    CSpinLockContext ctx_map( get_spin_lock_map_lock() );

    CSpinLock *impl_lock = map[ lock ];

    impl_lock->lock();
    return 0;
}
int pthread_spin_lock( pthread_spinlock_t *lock )   { return _natexec_pthread_spin_lock( lock ); }

int _natexec_pthread_spin_trylock( pthread_spinlock_t *lock )
{
    pth_spinlocks_t& map = get_spin_lock_map();

    CSpinLockContext ctx_map( get_spin_lock_map_lock() );

    CSpinLock *impl_lock = map[ lock ];

    return ( impl_lock->tryLock() ? 0 : EBUSY );
}
int pthread_spin_trylock( pthread_spinlock_t *lock )    { return _natexec_pthread_spin_trylock( lock ); }

int _natexec_pthread_spin_unlock( pthread_spinlock_t *lock )
{
    pth_spinlocks_t& map = get_spin_lock_map();

    CSpinLockContext ctx_map( get_spin_lock_map_lock() );

    CSpinLock *impl_lock = map[ lock ];

    impl_lock->unlock();
    return 0;
}
int pthread_spin_unlock( pthread_spinlock_t *lock ) { return _natexec_pthread_spin_unlock( lock ); }

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
