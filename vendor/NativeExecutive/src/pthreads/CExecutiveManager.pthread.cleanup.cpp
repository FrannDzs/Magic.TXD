/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/pthreads/CExecutiveManager.pthread.cleanup.cpp
*  PURPOSE:     POSIX threads cleanup implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include "CExecutiveManager.pthread.hxx"

#include <errno.h>

using namespace NativeExecutive;

// *** CANCELATION

struct _pthread_cancelation_hazard_handler : public hazardPreventionInterface
{
    inline _pthread_cancelation_hazard_handler( void (*routine)(void*), void *arg )
    {
        this->routine = routine;
        this->arg = arg;
    }

    void TerminateHazard( void ) noexcept override
    {
        try
        {
            this->routine( this->arg );
        }
        catch( ... )
        {
            // Ignore exceptions.
        }
    }

    void (*routine)(void*);
    void *arg;
};

void _natexec_pthread_cleanup_push( void (*routine)(void*), void *arg )
{
    CExecutiveManager *manager = get_executive_manager();

    _pthread_cancelation_hazard_handler *handler =
        eir::static_new_struct <_pthread_cancelation_hazard_handler, hazardPreventionInterface::dyn_alloc> ( nullptr, routine, arg );

    PushHazard( manager, handler, true );
}
void pthread_cleanup_push( void (*routine)(void*), void *arg )
{ return _natexec_pthread_cleanup_push( routine, arg ); }

void _natexec_pthread_cleanup_pop( int execute )
{
    CExecutiveManager *manager = get_executive_manager();

    PopHazard( manager, ( execute != 0 ) );
}
void pthread_cleanup_pop( int execute ) { return _natexec_pthread_cleanup_pop( execute ); }

int _natexec_pthread_setcancelstate( int state, int *oldstate )
{
    return ENOTSUP;
}
int pthread_setcancelstate( int state, int *oldstate )
{ return _natexec_pthread_setcancelstate( state, oldstate ); }

int _natexec_pthread_setcanceltype( int type, int *oldtype )
{
    return ENOTSUP;
}
int pthread_setcanceltype( int type, int *oldtype )
{ return _natexec_pthread_setcanceltype( type, oldtype ); }

int _natexec_pthread_cancel( pthread_t thread )
{
    CExecThread *impl_thread = (CExecThread*)thread;

    bool couldTerm = impl_thread->Terminate( false );

    return ( couldTerm ? 0 : EBUSY );
}
int pthread_cancel( pthread_t thread )  { return _natexec_pthread_cancel( thread ); }

void _natexec_pthread_testcancel( void )
{
    CExecutiveManager *manager = get_executive_manager();

    manager->CheckHazardCondition();
}
void pthread_testcancel( void ) { return _natexec_pthread_testcancel(); }

void _natexec_pthread_cleanup_push_defer_np( void (*routine)(void*), void *arg )
{
    // Since we do only support the deferred cancellation system this function call
    // is equivalent to _natexec_pthread_cleanup_push.
    _natexec_pthread_cleanup_push( routine, arg );
}
void pthread_cleanup_push_defer_np( void (*routine)(void*), void *arg )
{ return _natexec_pthread_cleanup_push_defer_np( routine, arg ); }

void _natexec_pthread_cleanup_pop_restore_np( int execute )
{
    // See above.
    _natexec_pthread_cleanup_pop( execute );
}
void pthread_cleanup_pop_restore_np( int execute )
{ return _natexec_pthread_cleanup_pop_restore_np( execute ); }

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION