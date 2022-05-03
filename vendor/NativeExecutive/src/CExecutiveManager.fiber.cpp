/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.fiber.cpp
*  PURPOSE:     Executive manager fiber logic
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "internal/CExecutiveManager.fiber.internal.h"
#include "internal/CExecutiveManager.thread.internal.h"
#include "internal/CExecutiveManager.execgroup.internal.h"

#include "CExecutiveManager.thread.hxx"
#include "CExecutiveManager.fiber.hxx"

#ifdef _WIN32
#include <excpt.h>
#include <windows.h>

#include "CExecutiveManager.native.hxx"
#include "CExecutiveManager.hazards.hxx"

typedef struct _EXCEPTION_REGISTRATION
{
     struct _EXCEPTION_REGISTRATION* Next;
     void *Handler;
} EXCEPTION_REGISTRATION, *PEXCEPTION_REGISTRATION;

static EXCEPTION_DISPOSITION _defaultHandler( EXCEPTION_REGISTRATION *record, void *frame, CONTEXT *context, void *dispatcher )
{
    exit( 0x0DAB00B5 );
    return ExceptionContinueSearch;
}

static EXCEPTION_REGISTRATION _baseException =
{
    (EXCEPTION_REGISTRATION*)-1,
    _defaultHandler
};

#elif defined(__linux__)

#include <string.h>

static void FIBER_CALLCONV _linux_fiberEntryPoint( NativeExecutive::FiberStatus *userdata, NativeExecutive::FiberProcedure proc )
{
    proc( userdata );

    // Need to call the termination routine after yield.

    // Must set ourselves to TERMINATED state.
    userdata->status = NativeExecutive::FIBER_TERMINATED;

    // We have to leave to the previous environment.
    setcontext( &userdata->callee->linux_context );
}

#endif //CROSS PLATFORM CODE

BEGIN_NATIVE_EXECUTIVE

#ifdef _MSC_VER
#pragma warning(disable:4733)
#endif //_MSC_VER

// Stack manipulation routines.
template <typename stack_type, typename dataType>
inline void pushdata( stack_type& stack_cur, const dataType& data )
{
    *--((dataType*&)stack_cur) = data;
}

Fiber* ExecutiveFiber::newfiber( FiberStatus *userdata, void *fiber_ptr, void *stack_ptr, size_t stackSize, FiberProcedure proc, FiberStatus::termfunc_t termcb )
{
    Fiber *env = (Fiber*)fiber_ptr;

#if defined(_WIN32)

    const size_t stack_alignment = 16;

    void *stack_base = (char*)( (char*)stack_ptr + stackSize );

    // Properly align the stack base.
    stack_base = (char*)stack_base - ( (size_t)stack_base & ( stack_alignment - 1 ) );

    void *stack = stack_base;

#if defined(_M_IX86)
    // Once entering, the first argument should be the thread
    pushdata( stack, userdata );
    pushdata( stack, &exit );
    pushdata( stack, userdata );
    pushdata( stack, &_fiber86_retHandler );

    // We can enter the procedure directly.
    env->eip = (regType_t)proc;

#elif defined(_M_AMD64)
    // This is quite a complicated situation.
    pushdata( stack, userdata );
    pushdata( stack, &exit );      // if returning from start routine, exit.

    // The first argument is the rcx register.
    env->r12 = (regType_t)proc;
    env->r13 = (regType_t)userdata;

    // We should enter a custom fiber routine.
    env->eip = (regType_t)_fiber64_procStart;
#endif

    env->esp = (regType_t)stack;

#elif defined(__linux__)
    // Set up a basic machine context.
    int errctx = getcontext( &env->linux_context );

    if ( errctx != 0 )
    {
        return nullptr;
    }

    env->linux_context.uc_flags = 0;
    env->linux_context.uc_link = nullptr;

    env->linux_context.uc_stack.ss_size = stackSize;
    env->linux_context.uc_stack.ss_flags = 0;
    env->linux_context.uc_stack.ss_sp = (stack_t*)stack_ptr;

    // We pray that the native implementation does support passing machine-words through to the stack.
    // The linux version has a special statement that clarifies this.
    makecontext( &env->linux_context, (void (*)(void))_linux_fiberEntryPoint, 2, userdata, proc );

    // :)
#else
#error no newfiber implementation
#endif

    // Store stack properties.
    // We actually have to have valid stack properties here, since each implementation is
    // enforced to make its own stack space.
#ifdef _WIN32
    env->stack_base = stack_base;
    env->stack_limit = stack_ptr;
    env->except_info = &_baseException;
#elif defined(__linux__)
    env->stack_ptr = stack_ptr;
#endif //_WIN32

    // WARNING: has to be executed on a DIFFERENT stack than the fiber's!
    userdata->termcb = termcb;

    env->stackSize = stackSize;
    return env;
}

void* ExecutiveFiber::getstackspace( Fiber *fiber )
{
#ifdef _WIN32
    return ( fiber->stack_limit );
#elif defined(__linux__)
    return ( fiber->stack_ptr );
#else
#error missing fiber stack get implementation
#endif //CROSS PLATFORM CODE
}

// For use with resuming/jumping into fiber.
void ExecutiveFiber::eswitch( Fiber *from, Fiber *to )
{
#if defined(_M_IX86)
    _fiber86_eswitch( from, to );
#elif defined(_M_AMD64)
    _fiber64_eswitch( from, to );
#elif defined(__linux__)
    swapcontext( &from->linux_context, &to->linux_context );
#else
#error missing fiber eswitch implementation for platform (resume)
#endif
}

// For use with yielding
void ExecutiveFiber::qswitch( Fiber *from, Fiber *to )
{
#if defined(_M_IX86)
    _fiber86_qswitch( from, to );
#elif defined(_M_AMD64)
    _fiber64_qswitch( from, to );
#elif defined(__linux__)
    swapcontext( &from->linux_context, &to->linux_context );
#else
#error missing fiber qswitch implementation for platform (yield)
#endif
}

extern "C" void FIBER_CALLCONV nativeFiber_OnFiberTerminate( CFiber *fiber )
{
    throw fiberTerminationException( fiber );
}

void CFiberImpl::terminate_impl( void )
{
    // We assume that the fiber is suspended.

#if defined(_WIN32)
    // Throw an exception on the fiber
    Fiber *env = this->runtime;

#if defined(_M_IX86)
    pushdata( env->esp, this );
    pushdata( env->esp, &exit );
    env->eip = (regType_t)nativeFiber_OnFiberTerminate;
#elif defined(_M_AMD64)
    env->r12 = (regType_t)this;
    env->eip = (regType_t)_fiber64_term;
#endif //CROSS MACHINE CODE
#elif defined(__linux__)

    // On the linux architecture we do not support the throwing of exceptions across fibers directly.
    // Thus we have to emulate it by notifying the runtime it should terminate on next yield.
    // This is because throwing exceptions across unknown stack frames (inside swapcontext call) is
    // not possible.
    // * look into CExecutiveManager::TerminateFiber for the setting of FIBER_TERMINATING status.

#else
#error no fiber termination implementation
#endif

    // We want to eventually return back
    resume();
}

constinit optional_struct_space <privateFiberEnvironmentRegister_t> privateFiberEnvironmentRegister;

inline threadFiberStackPluginInfo* GetThreadFiberStackPlugin( CExecutiveManager *manager, CExecThreadImpl *theThread )
{
    privateFiberEnvironment *fiberEnv = privateFiberEnvironmentRegister.get().GetPluginStruct( (CExecutiveManagerNative*)manager );

    if ( fiberEnv )
    {
        return privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <threadFiberStackPluginInfo> ( theThread, fiberEnv->threadFiberStackPluginOffset );
    }

    return nullptr;
}

bool CFiber::is_running( void ) const
{
    CFiberImpl *nativeFiber = (CFiberImpl*)this;

    eFiberStatus state = nativeFiber->status;

    return ( state == FIBER_RUNNING || state == FIBER_TERMINATING );
}

bool CFiber::is_terminated( void ) const
{
    CFiberImpl *nativeFiber = (CFiberImpl*)this;

    return ( nativeFiber->status == FIBER_TERMINATED );
}

void CFiber::push_on_stack( void )
{
    CFiberImpl *nativeFiber = (CFiberImpl*)this;

    nativeFiber->manager->PushFiber( nativeFiber );
}

void CFiber::pop_from_stack( void )
{
    CFiberImpl *nativeFiber = (CFiberImpl*)this;

    nativeFiber->manager->PopFiber();
}

bool CFiber::is_current_on_stack( void ) const
{
    CFiberImpl *nativeFiber = (CFiberImpl*)this;

    return ( nativeFiber->manager->GetCurrentFiber() == this );
}

void CFiber::resume( void )
{
    CFiberImpl *nativeFiber = (CFiberImpl*)this;

    eFiberStatus fiberStatus = nativeFiber->status;

    if ( fiberStatus == FIBER_SUSPENDED || fiberStatus == FIBER_TERMINATING )
    {
        // Save the time that we resumed from this.
        nativeFiber->resumeTimer = ExecutiveManager::GetPerformanceTimer();

        if ( fiberStatus == FIBER_SUSPENDED )
        {
            nativeFiber->status = ( fiberStatus = FIBER_RUNNING );
        }

        CExecutiveManagerNative *manager = nativeFiber->manager;

        privateFiberEnvironment *fiberEnv = privateFiberEnvironmentRegister.get().GetPluginStruct( manager );

        assert( fiberEnv != nullptr );

        // Get the callee context of the current runtime.
        Fiber *callee = nullptr;
        {
            CExecThreadImpl *curThread = (CExecThreadImpl*)manager->GetCurrentThread();

            assert( curThread != nullptr );

            // First check the fiber stack, if there is a current fiber already running.
            // If it is, then use it as callee.
            // Otherwise we assume we are not running on a fiber, so we use the thread root context.
            threadFiberStackPluginInfo *threadFiberStack = fiberEnv->GetThreadFiberStackPlugin( curThread );

            assert( threadFiberStack != nullptr );

            if ( CFiberImpl *curFiber = threadFiberStack->GetCurrentFiber() )
            {
                callee = curFiber->runtime;
            }
            else
            {
                callee = &threadFiberStack->context;
            }
        }

        // Properly set the callee for returning.
        nativeFiber->callee = callee;

        // Push the fiber on the current thread's executive stack.
        // We now become current fiber.
        this->push_on_stack();

        ExecutiveFiber::eswitch( callee, nativeFiber->runtime );

        // Check if we returned due to an exception.
        // Or we just are terminated for clean-up.
        if ( nativeFiber->status == FIBER_TERMINATED )
        {
#ifdef __linux__
            // Since we just terminated and we cannot resume a terminated fiber, we can
            // clean up the memory here and stuff.
            if ( NativeExecutive::FiberStatus::termfunc_t termfunc = nativeFiber->termcb )
            {
                termfunc( nativeFiber );
            }
#endif //__linux__

            if ( nativeFiber->has_term_unhandled_exception )
            {
                // Clear that we had an exception.
                nativeFiber->has_term_unhandled_exception = false;

#ifdef NATEXEC_EXCEPTION_COPYPUSH
                std::exception_ptr rethrow_except = std::move( nativeFiber->term_unhandled_exception );

                // We just forward the exception that we stored.
                std::rethrow_exception( std::move( rethrow_except ) );
#else //NATEXEC_EXCEPTION_COPYPUSH
                // Determine what type of exception was thrown.
                // Rethrow the appropriate one.
                CFiberImpl::eUnhandledExceptionType exceptType = nativeFiber->unhandled_exception_type;

                nativeFiber->unhandled_exception_type = CFiberImpl::eUnhandledExceptionType::UNKNOWN;

                if ( exceptType == CFiberImpl::eUnhandledExceptionType::FIBERTERM )
                {
                    throw fiberTerminationException( nativeFiber->unhandled_exception_args.fiber );
                }
                else
                {
                    // For memory management reasons we do not use unspecified-allocator C++ structures.
                    // So we just throw an usermode exception.
                    throw fiberUnhandledException( nativeFiber );
                }
#endif //NATEXEC_EXCEPTION_COPYPUSH
            }
        }
    }
}

void CFiber::yield_proc( void )
{
    CFiberImpl *nativeFiber = (CFiberImpl*)this;

    bool needYield = false;
    {
#ifdef _WIN32
        // We need very high precision math here.
        HighPrecisionMathWrap mathWrap;
#endif //_WIN32

        // Do some test with timing.
        double currentTimer = ExecutiveManager::GetPerformanceTimer();
        double perfMultiplier = nativeFiber->group->perfMultiplier;

        needYield =
            ( currentTimer - nativeFiber->resumeTimer )
            >
            ( nativeFiber->manager->GetFrameDuration() * perfMultiplier );
    }

    if ( needYield )
    {
        yield();
    }
}

void CFiber::yield( void )
{
    CFiberImpl *nativeFiber = (CFiberImpl*)this;

    // Care about the status.
    {
        eFiberStatus status = nativeFiber->status;

        if ( status == FIBER_TERMINATING )
        {
            nativeFiber_OnFiberTerminate( nativeFiber );
        }

        assert( status == FIBER_RUNNING );
    }
    assert( this->is_current_on_stack() );

    // WARNING: only call this function from the fiber stack!
    // WARNING: do not call this function if the fiber is terminating!
    nativeFiber->status = FIBER_SUSPENDED;

    // Pop the fiber from the current active executive stack.
    this->pop_from_stack();

    ExecutiveFiber::qswitch( nativeFiber->runtime, nativeFiber->callee );

    // Check the status again.
    {
        eFiberStatus status = nativeFiber->status;

        if ( status == FIBER_TERMINATING )
        {
            nativeFiber_OnFiberTerminate( nativeFiber );
        }
    }
}

void CFiber::check_termination( void )
{
    CFiberImpl *nativeFiber = (CFiberImpl*)this;

    if ( nativeFiber->status == FIBER_TERMINATING )
    {
        // We should continue termination.
        throw fiberTerminationException( nativeFiber );
    }
}

void CExecutiveManagerNative::PushFiber( CFiberImpl *currentFiber )
{
    CExecThreadImpl *currentThread = (CExecThreadImpl*)this->GetCurrentThread();

    assert( currentThread != nullptr );

    if ( threadFiberStackPluginInfo *info = GetThreadFiberStackPlugin( this, currentThread ) )
    {
        info->fiberStack.AddToBack( currentFiber );
    }
}

void CExecutiveManagerNative::PopFiber( void )
{
    CExecThreadImpl *currentThread = (CExecThreadImpl*)this->GetCurrentThread();

    assert( currentThread != nullptr );

    if ( threadFiberStackPluginInfo *info = GetThreadFiberStackPlugin( this, currentThread ) )
    {
        info->fiberStack.RemoveFromBack();
    }
}

threadFiberStackIterator::threadFiberStackIterator( CExecThreadImpl *thread )
{
    this->thread = thread;
    this->iter = 0;
}

threadFiberStackIterator::~threadFiberStackIterator( void )
{
    return;
}

bool threadFiberStackIterator::IsEnd( void ) const
{
    threadFiberStackPluginInfo *fiberObj = GetThreadFiberStackPlugin( this->thread->manager, this->thread );

    if ( fiberObj )
    {
        return ( fiberObj->fiberStack.GetCount() <= this->iter );
    }

    return true;
}

void threadFiberStackIterator::Increment( void )
{
    this->iter++;
}

CFiberImpl* threadFiberStackIterator::Resolve( void ) const
{
    threadFiberStackPluginInfo *fiberObj = GetThreadFiberStackPlugin( this->thread->manager, this->thread );

    if ( fiberObj )
    {
        return fiberObj->fiberStack[ this->iter ];
    }

    return nullptr;
}

CFiber* CExecThread::GetCurrentFiber( void )
{
    // One big rule is that this function is only called on the same thread that it is running on.
    // Then we have no problems.

    CExecThreadImpl *nativeThread = (CExecThreadImpl*)this;

    if ( threadFiberStackPluginInfo *info = GetThreadFiberStackPlugin( nativeThread->manager, nativeThread ) )
    {
        return info->GetCurrentFiber();
    }

    return nullptr;
}

bool CExecThread::IsFiberRunningHere( CFiber *fiber )
{
    CFiberImpl *nativeFiber = (CFiberImpl*)fiber;

    // One big rule is that this function is only called on the same thread that it is running on.
    // Then we have no problems.

    CExecThreadImpl *nativeThread = (CExecThreadImpl*)this;

    if ( threadFiberStackPluginInfo *info = GetThreadFiberStackPlugin( nativeThread->manager, nativeThread ) )
    {
        return info->HasFiber( nativeFiber );
    }

    return false;
}

CFiber* CExecutiveManager::GetCurrentFiber( void )
{
    CExecThread *currentThread = GetCurrentThread( true );

    // If there is no thread then there also is no current fiber.
    // Could happen if the environment is terminating.
    // If the current remote thread has no thread descriptor then it cannot have any running fiber.
    if ( currentThread == nullptr )
    {
        return nullptr;
    }

    return currentThread->GetCurrentFiber();
}

void registerFiberPlugin( void )
{
    privateFiberEnvironmentRegister.Construct( executiveManagerFactory );
}

void unregisterFiberPlugin( void )
{
    privateFiberEnvironmentRegister.Destroy();
}

END_NATIVE_EXECUTIVE
