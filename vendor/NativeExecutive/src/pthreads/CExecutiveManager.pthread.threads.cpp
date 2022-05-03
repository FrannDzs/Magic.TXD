/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.threads.cpp
*  PURPOSE:     POSIX threads thread implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "CExecutiveManager.pthread.hxx"

#include <errno.h>

using namespace NativeExecutive;

constinit CExecutiveManager *pth_executiveManager = nullptr;

struct pthread_internal_plugin
{
    void* (*start_routine)(void*) = nullptr;
    void *status = nullptr;
};

static constinit optional_struct_space <execThreadStructPluginRegister <pthread_internal_plugin>> pth_thread_plugin_reg;

// Plugins.
void _pthread_registerRwlockMaintenance( CExecutiveManager *execMan );
void _pthread_registerGeneric( CExecutiveManager *manager );

void _pthread_unregisterRwlockMaintenance( void );
void _pthread_unregisterGeneric( CExecutiveManager *manager );

CExecutiveManager* get_executive_manager( void )
{
    // TODO: until Microsoft actually does actually implement the consteval/constinit
    // keywords we are stuck with this hacky solution; spin-locks should
    // be compile-time initialized (EVEN in Debug builds)!
    // Otherwise this program is ill-formed, only works by chance.
    static constinit CSpinLock pth_spin_executiveManager;

    CSpinLockContext ctx_init( pth_spin_executiveManager );

    if ( pth_executiveManager == nullptr )
    {
        pth_executiveManager = CExecutiveManager::Create();

        pth_thread_plugin_reg.Construct();
        pth_thread_plugin_reg.get().RegisterPlugin( pth_executiveManager );

        _pthread_registerRwlockMaintenance( pth_executiveManager );
        _pthread_registerGeneric( pth_executiveManager );
    }

    return pth_executiveManager;
}

// *** THREAD ATTRIBUTES

int _natexec_pthread_attr_init( pthread_attr_t *attr )
{
    try
    {
        __natexec_impl_pthread_attr_t *impl_attr = eir::static_new_struct <__natexec_impl_pthread_attr_t, NatExecGlobalStaticAlloc> ( nullptr );

#ifdef NATEXEC_PTHREADS_DEFAULT_STACKSIZE
        impl_attr->stacksize = NATEXEC_PTHREADS_DEFAULT_STACKSIZE;
#else
        impl_attr->stacksize = ( 1 << 18 );     // a generous default.
#endif //NATEXEC_PTHREADS_DEFAULT_STACKSIZE
        impl_attr->create_joinable = true;

        set_internal_pthread_attr( attr, impl_attr );
        return 0;
    }
    catch( eir::eir_exception& )
    {
        return ENOMEM;
    }
}
int pthread_attr_init( pthread_attr_t *attr )   { return _natexec_pthread_attr_init( attr ); }

int _natexec_pthread_attr_destroy( pthread_attr_t *attr )
{
    __natexec_impl_pthread_attr_t *impl_attr = get_internal_pthread_attr( attr );

    eir::static_del_struct <__natexec_impl_pthread_attr_t, NatExecGlobalStaticAlloc> ( nullptr, impl_attr );
    return 0;
}
int pthread_attr_destroy( pthread_attr_t *attr )    { return _natexec_pthread_attr_destroy( attr ); }

int _natexec_pthread_attr_getinheritsched( const pthread_attr_t *attr, int *inheritsched )
{
    *inheritsched = PTHREAD_INHERIT_SCHED;
    return 0;
}
int pthread_attr_getinheritsched( const pthread_attr_t *attr, int *inheritsched )
{ return _natexec_pthread_attr_getinheritsched( attr, inheritsched ); }

int _natexec_pthread_attr_setinheritsched( pthread_attr_t *attr, int inheritsched )
{
    return ENOTSUP;
}
int pthread_attr_setinheritsched( pthread_attr_t *attr, int inheritsched )
{ return _natexec_pthread_attr_setinheritsched( attr, inheritsched ); }

int _natexec_pthread_attr_getschedparam( const pthread_attr_t *attr, sched_param *param )
{
    param->sched_priority = 0;
    return 0;
}
int pthread_attr_getschedparam( const pthread_attr_t *attr, sched_param *param )
{ return _natexec_pthread_attr_getschedparam( attr, param ); }

int _natexec_pthread_attr_setschedparam( pthread_attr_t *attr, const sched_param *param )
{
    return ENOTSUP;
}
int pthread_attr_setschedparam( pthread_attr_t *attr, const sched_param *param )
{ return _natexec_pthread_attr_setschedparam( attr, param ); }

int _natexec_pthread_attr_getschedpolicy( const pthread_attr_t *attr, int *policy )
{
    *policy = SCHED_OTHER;
    return 0;
}
int pthread_attr_getschedpolicy( const pthread_attr_t *attr, int *policy )
{ return _natexec_pthread_attr_getschedpolicy( attr, policy ); }

int _natexec_pthread_attr_setschedpolicy( pthread_attr_t *attr, int policy )
{
    return ENOTSUP;
}
int pthread_attr_setschedpolicy( pthread_attr_t *attr, int policy )
{ return _natexec_pthread_attr_setschedpolicy( attr, policy ); }

int _natexec_pthread_attr_getscope( const pthread_attr_t *attr, int *scope )
{
    *scope = PTHREAD_SCOPE_SYSTEM;
    return 0;
}
int pthread_attr_getscope( const pthread_attr_t *attr, int *scope )
{ return _natexec_pthread_attr_getscope( attr, scope ); }

int _natexec_pthread_attr_setscope( pthread_attr_t *attr, int scope )
{
    return ENOTSUP;
}
int pthread_attr_setscope( pthread_attr_t *attr, int scope )
{ return _natexec_pthread_attr_setscope( attr, scope ); }

int _natexec_pthread_attr_getstackaddr( const pthread_attr_t *attr, void **stackaddr )
{
    return ENOTSUP;
}
int pthread_attr_getstackaddr( const pthread_attr_t *attr, void **stackaddr )
{ return _natexec_pthread_attr_getstackaddr( attr, stackaddr ); }

int _natexec_pthread_attr_setstackaddr( pthread_attr_t *attr, void *stackaddr )
{
    return ENOTSUP;
}
int pthread_attr_setstackaddr( pthread_attr_t *attr, void *stackaddr )
{ return _natexec_pthread_attr_setstackaddr( attr, stackaddr ); }

int _natexec_pthread_attr_getstack( const pthread_attr_t *attr, void **stackaddr, size_t *stacksize )
{
    return ENOTSUP;
}
int pthread_attr_getstack( const pthread_attr_t *attr, void **stackaddr, size_t *stacksize )
{ return _natexec_pthread_attr_getstack( attr, stackaddr, stacksize ); }

int _natexec_pthread_attr_setstack( pthread_attr_t *attr, void *stackaddr, size_t stacksize )
{
    return ENOTSUP;
}
int pthread_attr_setstack( pthread_attr_t *attr, void *stackaddr, size_t stacksize )
{ return _natexec_pthread_attr_setstack( attr, stackaddr, stacksize ); }

int _natexec_pthread_attr_getdetachstate( const pthread_attr_t *attr, int *detachstate )
{
    const __natexec_impl_pthread_attr_t *impl_attr = get_const_internal_pthread_attr( attr );

    *detachstate = ( impl_attr->create_joinable ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED );
    return 0;
}
int pthread_attr_getdetachstate( const pthread_attr_t *attr, int *detachstate )
{ return _natexec_pthread_attr_getdetachstate( attr, detachstate ); }

int _natexec_pthread_attr_setdetachstate( pthread_attr_t *attr, int detachstate )
{
    __natexec_impl_pthread_attr_t *impl_attr = get_internal_pthread_attr( attr );

    if ( detachstate == PTHREAD_CREATE_JOINABLE )
    {
        impl_attr->create_joinable = true;
        return 0;
    }
    else if ( detachstate == PTHREAD_CREATE_DETACHED )
    {
        impl_attr->create_joinable = false;
        return 0;
    }

    return EINVAL;
}
int pthread_attr_setdetachstate( pthread_attr_t *attr, int detachstate )
{ return _natexec_pthread_attr_setdetachstate( attr, detachstate ); }

int _natexec_pthread_attr_getguardsize( const pthread_attr_t *attr, size_t *guardsize )
{
    return ENOTSUP;
}
int pthread_attr_getguardsize( const pthread_attr_t *attr, size_t *guardsize )
{ return _natexec_pthread_attr_getguardsize( attr, guardsize ); }

int _natexec_pthread_attr_setguardsize( pthread_attr_t *attr, size_t guardsize )
{
    return ENOTSUP;
}
int pthread_attr_setguardsize( pthread_attr_t *attr, size_t guardsize )
{ return _natexec_pthread_attr_setguardsize( attr, guardsize ); }

int _natexec_pthread_attr_getstacksize( const pthread_attr_t *attr, size_t *stacksize )
{
    const __natexec_impl_pthread_attr_t *impl_attr = get_const_internal_pthread_attr( attr );

    *stacksize = impl_attr->stacksize;
    return 0;
}
int pthread_attr_getstacksize( const pthread_attr_t *attr, size_t *stacksize )
{ return _natexec_pthread_attr_getstacksize( attr, stacksize ); }

int _natexec_pthread_attr_setstacksize( pthread_attr_t *attr, size_t stacksize )
{
    if ( stacksize < PTHREAD_STACK_MIN )
    {
        return EINVAL;
    }

    __natexec_impl_pthread_attr_t *impl_attr = get_internal_pthread_attr( attr );

    impl_attr->stacksize = stacksize;
    return 0;
}
int pthread_attr_setstacksize( pthread_attr_t *attr, size_t stacksize )
{ return _natexec_pthread_attr_setstacksize( attr, stacksize ); }

int _natexec_pthread_getattr_np( pthread_t thread, pthread_attr_t *attr )
{
    return ENOTSUP;
}
int pthread_getattr_np( pthread_t thread, pthread_attr_t *attr )
{ return _natexec_pthread_getattr_np( thread, attr ); }

// *** THREADS

static void _pth_thread_entry_point( CExecThread *thread, void *ud )
{
    pthread_internal_plugin *plug = pth_thread_plugin_reg.get().GetPluginStruct( thread );

    plug->status = plug->start_routine( ud );
}

int _natexec_pthread_create( pthread_t *thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void *arg )
{
    CExecutiveManager *execMan = get_executive_manager();

    size_t stacksize;
    bool create_joinable = true;

    if ( attr != nullptr )
    {
        const __natexec_impl_pthread_attr_t *impl_attr = get_const_internal_pthread_attr( attr );

        stacksize = impl_attr->stacksize;
        create_joinable = impl_attr->create_joinable;
    }
    else
    {
#ifdef NATEXEC_PTHREADS_DEFAULT_STACKSIZE
        stacksize = NATEXEC_PTHREADS_DEFAULT_STACKSIZE;
#else
        stacksize = 0;  // we pick the default from the NativeExecutive implementation.
#endif //NATEXEC_PTHREADS_DEFAULT_STACKSIZE
    }

    CExecThread *natexec_thread = execMan->CreateThread( _pth_thread_entry_point, arg, stacksize, "pthread" );

    if ( thread == nullptr )
    {
        return ENOMEM;
    }

    pthread_internal_plugin *plug = pth_thread_plugin_reg.get().GetPluginStruct( natexec_thread );

    plug->start_routine = start_routine;
    plug->status = nullptr;

    natexec_thread->Resume();

    if ( create_joinable == false )
    {
        // Let the thread free up resources on it's own.
        execMan->CloseThread( natexec_thread );
    }

    *thread = (pthread_t)natexec_thread;
    return 0;
}
int pthread_create( pthread_t *thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void *arg )
{ return _natexec_pthread_create( thread, attr, start_routine, arg ); }

int _natexec_pthread_exit( void *status )
{
    CExecutiveManager *execMan = get_executive_manager();

    CExecThread *curThread = execMan->GetCurrentThread( true );

    if ( curThread == nullptr )
    {
        return EINVAL;
    }

    if ( execMan->IsRemoteThread( curThread ) )
    {
        return EINVAL;
    }

    // Write the status.
    // We do this here because the Terminate request will not return if successful.
    {
        pthread_internal_plugin *plugin = pth_thread_plugin_reg.get().GetPluginStruct( curThread );

        if ( plugin )
        {
            plugin->status = status;
        }
    }

    bool termSucc = curThread->Terminate();

    if ( termSucc == false )
    {
        return EINVAL;
    }

    return 0;
}
int pthread_exit( void *status )
{ return _natexec_pthread_exit( status ); }

int _natexec_pthread_join( pthread_t thread, void **status )
{
    CExecutiveManager *execMan = get_executive_manager();

    CExecThread *natexec_thread = (CExecThread*)thread;

    if ( execMan->IsRemoteThread( natexec_thread ) )
    {
        return EINVAL;
    }

    execMan->JoinThread( natexec_thread );

    if ( status != nullptr )
    {
        pthread_internal_plugin *plug = pth_thread_plugin_reg.get().GetPluginStruct( natexec_thread );

        *status = plug->status;
    }

    execMan->CloseThread( natexec_thread );

    return 0;
}
int pthread_join( pthread_t thread, void **status )
{ return _natexec_pthread_join( thread, status ); }

int _natexec_pthread_detach( pthread_t thread )
{
    CExecutiveManager *execMan = get_executive_manager();

    CExecThread *natexec_thread = (CExecThread*)thread;

    if ( execMan->IsRemoteThread( natexec_thread ) )
    {
        return EINVAL;
    }

    execMan->CloseThread( natexec_thread );

    return 0;
}
int pthread_detach( pthread_t thread )
{ return _natexec_pthread_detach( thread ); }

int _natexec_pthread_equal( pthread_t thread1, pthread_t thread2 )
{
    return thread1 == thread2;
}
int pthread_equal( pthread_t thread1, pthread_t thread2 )
{ return _natexec_pthread_equal( thread1, thread2 ); }

pthread_t _natexec_pthread_self( void )
{
    CExecutiveManager *execMan = get_executive_manager();

    return (pthread_t)execMan->GetCurrentThread();
}
pthread_t pthread_self( void )
{ return _natexec_pthread_self(); }

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
