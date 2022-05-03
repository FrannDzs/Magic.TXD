/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.generic.cpp
*  PURPOSE:     POSIX threads generic API implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "CExecutiveManager.pthread.hxx"

#include <sdk/Map.h>
#include <sdk/rwlist.hpp>

#ifdef __linux__
#include <unistd.h>
#elif defined(_WIN32)
#include <Windows.h>
#endif //CROSS PLATFORM CODE

#include <errno.h>

using namespace NativeExecutive;

struct _pthread_impl_key_t
{
    void (*destructor)(void*);
};

struct _pthread_plugin_tsd;

struct _pthread_key_optional_storage : public cleanupInterface
{
    RwList <_pthread_plugin_tsd> thread_plugins;
    eir::Map <pthread_key_t, _pthread_impl_key_t, NatExecGlobalStaticAlloc> keys;

    void OnCleanup( CExecThread *thread ) noexcept override;
};

struct _pthread_plugin_tsd
{
    eir::Map <pthread_key_t, void*, NatExecGlobalStaticAlloc> tsd_map;
    RwListEntry <_pthread_plugin_tsd> tsd_node;

    void Initialize( CExecThread *thread );
    void Shutdown( CExecThread *thread );
};

static constinit optional_struct_space <_pthread_key_optional_storage> _pthread_tsd_data;
static constinit optional_struct_space <execThreadStructPluginRegister <_pthread_plugin_tsd, true>> _pthread_tsd_plugin_reg;
static constinit CReadWriteLock* _pthread_tsd_lock = nullptr;

void _pthread_plugin_tsd::Initialize( CExecThread *thread )
{
    LIST_INSERT( _pthread_tsd_data.get().thread_plugins.root, this->tsd_node );
}

void _pthread_plugin_tsd::Shutdown( CExecThread *thread )
{
    // Must not have any data anymore because the cleanup should have taken care of it.
}

void _pthread_key_optional_storage::OnCleanup( CExecThread *thread ) noexcept
{
    _pthread_plugin_tsd *tsdPlugin = _pthread_tsd_plugin_reg.get().GetPluginStruct( thread );

    assert( tsdPlugin != nullptr );

clear_again:
    bool has_found_non_nullptr = false;
    {
        pthread_key_t lastkey;
        bool has_last_key = false;

        while ( true )
        {
            // Try to fetch the next data.
            void *value;
            void (*destructor)(void*);
            {
                CReadWriteWriteContext <> ctx_clear_tsd( _pthread_tsd_lock );

                if ( has_last_key == false )
                {
                    auto *minNode = tsdPlugin->tsd_map.GetMinimumByKey();

                    if ( minNode == nullptr )
                    {
                        break;
                    }

                    lastkey = minNode->GetKey();
                    has_last_key = true;

                    value = minNode->GetValue();

                    minNode->GetValue() = nullptr;
                }
                else
                {
                    auto *nextNode = tsdPlugin->tsd_map.FindMinimumByCriteria(
                        [&]( const auto *compNode )
                        {
                            pthread_key_t keyNode = compNode->GetKey();

                            if ( keyNode <= lastkey )
                            {
                                return eir::eCompResult::LEFT_LESS;
                            }

                            return eir::eCompResult::EQUAL;
                        }
                    );

                    if ( nextNode == nullptr )
                    {
                        break;
                    }

                    lastkey = nextNode->GetKey();
                    has_last_key = true;

                    value = nextNode->GetValue();

                    nextNode->GetValue() = nullptr;
                }

                _pthread_impl_key_t& info = _pthread_tsd_data.get().keys[ lastkey ];

                destructor = info.destructor;
            }

            if ( value != nullptr && destructor != nullptr )
            {
                // For safety reasons we must ignore any kind of C++ exception that
                // is coming down here.
                try
                {
                    destructor( value );
                }
                catch( ... )
                {
                    // Continue chugging on!
                }
            }
        }
    }

    if ( has_found_non_nullptr )
    {
        goto clear_again;
    }

    CReadWriteWriteContext <> ctx_remove_node( _pthread_tsd_lock );

    LIST_REMOVE( tsdPlugin->tsd_node );
}

void _pthread_registerGeneric( CExecutiveManager *manager )
{
    // *** THREAD SPECIFIC DATA (generic)
    _pthread_tsd_data.Construct();
    manager->RegisterThreadCleanup( &_pthread_tsd_data.get() );
    _pthread_tsd_plugin_reg.Construct();
    _pthread_tsd_plugin_reg.get().RegisterPlugin( manager );
    _pthread_tsd_lock = manager->CreateReadWriteLock();
    assert( _pthread_tsd_lock != nullptr );
}

void _pthread_unregisterGeneric( CExecutiveManager *manager )
{
    // *** THREAD SPECIFIC DATA (generic)
    manager->CloseReadWriteLock( _pthread_tsd_lock );
    _pthread_tsd_plugin_reg.get().UnregisterPlugin();
    _pthread_tsd_plugin_reg.Destroy();
    manager->UnregisterThreadCleanup( &_pthread_tsd_data.get() );
    _pthread_tsd_data.Destroy();
}

// *** THREAD SPECIFIC DATA (generic)

int _natexec_pthread_key_create( pthread_key_t *key, void (*destructor)(void*) )
{
    get_executive_manager();

    CReadWriteWriteContext <> ctx_new_key( _pthread_tsd_lock );

    // Find a free key id.
    pthread_key_t free_id = 0;

try_next_id:
    if ( _pthread_tsd_data.get().keys.Find( free_id ) != nullptr )
    {
        free_id++;
        goto try_next_id;
    }

    _pthread_impl_key_t keyinfo;
    keyinfo.destructor = destructor;

    _pthread_tsd_data.get().keys[ free_id ] = std::move( keyinfo );

    *key = free_id;
    return 0;
}
int pthread_key_create( pthread_key_t *key, void (*destructor)(void*) )
{ return _natexec_pthread_key_create( key, destructor ); }

int _natexec_pthread_key_delete( pthread_key_t key )
{
    get_executive_manager();

    CReadWriteWriteContext <> ctx_rem_key( _pthread_tsd_lock );

    auto *keyNode = _pthread_tsd_data.get().keys.Find( key );

    if ( keyNode == nullptr )
    {
        return EINVAL;
    }

    LIST_FOREACH_BEGIN( _pthread_plugin_tsd, _pthread_tsd_data.get().thread_plugins.root, tsd_node )

        item->tsd_map.RemoveByKey( key );

    LIST_FOREACH_END

    return 0;
}
int pthread_key_delete( pthread_key_t key ) { return _natexec_pthread_key_delete( key ); }

void* _natexec_pthread_getspecific( pthread_key_t key )
{
    get_executive_manager();

    _pthread_plugin_tsd *tsd_plugin = _pthread_tsd_plugin_reg.get().GetPluginStructCurrent( true );

    if ( tsd_plugin == nullptr )
    {
        return nullptr;
    }

    CReadWriteReadContext <> ctx_get_specific( _pthread_tsd_lock );

    auto *node = tsd_plugin->tsd_map.Find( key );

    if ( node == nullptr )
    {
        return nullptr;
    }

    return node->GetValue();
}
void* pthread_getspecific( pthread_key_t key )  { return _natexec_pthread_getspecific( key ); }

int _natexec_pthread_setspecific( pthread_key_t key, const void *value )
{
    get_executive_manager();

    _pthread_plugin_tsd *tsd_plugin = _pthread_tsd_plugin_reg.get().GetPluginStructCurrent( false );

    assert( tsd_plugin != nullptr );

    CReadWriteWriteContext <> ctx_set_specific( _pthread_tsd_lock );

    if ( _pthread_tsd_data.get().keys.Find( key ) == nullptr )
    {
        return EINVAL;
    }

    tsd_plugin->tsd_map[ key ] = (void*)value;
    return 0;
}
int pthread_setspecific( pthread_key_t key, const void *value )
{ return _natexec_pthread_setspecific( key, value ); }

// *** INIT (generic)

int _natexec_pthread_once_ex_np( pthread_once_t *once_control, void (*init_routine)(void*), void *arg )
{
    // Once again this depends on the compiler actually supporting "consteval" keyword
    // for class constructors. Else there is a race condition.
    static CSpinLock once_lock;

    once_lock.lock();

    while ( *once_control == PTHREAD_ONCE_INIT + 1 )
    {
        once_lock.unlock();

#ifdef __linux__
        usleep( 1000 );
#elif defined(_WIN32)
        Sleep( 1 );
#else
#error missing delay implementation
#endif //

        once_lock.lock();
    }

    if ( *once_control == PTHREAD_ONCE_INIT )
    {
        *once_control = PTHREAD_ONCE_INIT + 1;

        once_lock.unlock();

        init_routine( arg );

        once_lock.lock();

        *once_control = PTHREAD_ONCE_INIT + 2;

        once_lock.unlock();
        return 0;
    }

    bool already_initialized = ( *once_control == PTHREAD_ONCE_INIT + 2 );

    once_lock.unlock();

    return ( already_initialized ? 0 : EINVAL );
}
int pthread_once_ex_np( pthread_once_t *once_control, void (*init_routine)(void*), void *arg )
{ return _natexec_pthread_once_ex_np( once_control, init_routine, arg ); }

int _natexec_pthread_once( pthread_once_t *once_control, void (*init_routine)(void) )
{
    auto ctl_func = []( void *param )
    {
        auto init_routine = ((void (*)(void))param);

        init_routine();
    };

    return pthread_once_ex_np( once_control, ctl_func, (void*)init_routine );
}
int pthread_once( pthread_once_t *once_control, void (*init_routine)(void) )
{ return _natexec_pthread_once( once_control, init_routine ); }

// *** CONCURRENCY (generic)

int _natexec_pthread_setconcurrency( int new_level )
{
    return ENOTSUP;
}
int pthread_setconcurrency( int new_level ) { return _natexec_pthread_setconcurrency( new_level ); }

int _natexec_pthread_getconcurrency( void )
{
    return 0;
}
int pthread_getconcurrency( void )  { return _natexec_pthread_getconcurrency(); }

// *** FORKING (generic)

int _natexec_pthread_atfork( void (*prepare)(void), void (*parent)(void), void (*child)(void) )
{
    return ENOTSUP;
}
int pthread_atfork( void (*prepare)(void), void (*parent)(void), void (*child)(void) )
{ return _natexec_pthread_atfork( prepare, parent, child ); }

// *** SIGNALS (generic)

int _natexec_pthread_kill( pthread_t thread, int signo )
{
    return ENOTSUP;
}
int pthread_kill( pthread_t thread, int signo )
{ return _natexec_pthread_kill( thread, signo ); }

// *** TIME (generic)

int _natexec_pthread_getcpuclockid( pthread_t thread, clockid_t *clockid )
{
    return ENOTSUP;
}
int pthread_getcpuclockid( pthread_t thread, clockid_t *clockid )
{ return _natexec_pthread_getcpuclockid( thread, clockid ); }

// *** SCHEDULING (generic)

int _natexec_pthread_getschedparam( pthread_t thread, int *policy, sched_param *param )
{
    return ENOTSUP;
}
int pthread_getschedparam( pthread_t thread, int *policy, sched_param *param )
{ return _natexec_pthread_getschedparam( thread, policy, param ); }

int _natexec_pthread_setschedparam( pthread_t thread, int policy, const sched_param *param )
{
    return ENOTSUP;
}
int pthread_setschedparam( pthread_t thread, int policy, const sched_param *param )
{ return _natexec_pthread_setschedparam( thread, policy, param ); }

int _natexec_pthread_setschedprio( pthread_t thread, int prio )
{
    return ENOTSUP;
}
int pthread_setschedprio( pthread_t thread, int prio )
{ return _natexec_pthread_setschedprio( thread, prio ); }

int _natexec_pthread_yield( void )
{
    return 0;
}
int pthread_yield( void )   { return _natexec_pthread_yield(); }

// GNU extensions.

int _natexec_sched_yield( void )
{
    return 0;
}
int sched_yield( void ) { return _natexec_sched_yield(); }

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
