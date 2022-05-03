/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.cpp
*  PURPOSE:     Thread abstraction layer for MTA
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef __linux__
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#ifdef NATEXEC_LINUX_THREAD_SELF_CLEANUP
#include <asm/prctl.h>
#endif //NATEXEC_LINUX_THREAD_SELF_CLEANUP
#include <linux/mman.h>
#include <linux/futex.h>
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include "CExecutiveManager.native.hxx"

#include <sdk/OSUtils.version.h>

// For now there is only glibc support.
#include "CExecutiveManager.thread.compat.glib.hxx"

#elif defined(_WIN32)
#include <process.h>
#endif //PLATFORM DEFINITIONS

#include "CExecutiveManager.hazards.hxx"
#include "CExecutiveManager.native.hxx"
#include "CExecutiveManager.eventplugin.hxx"
#include "PluginUtils.hxx"

#include <sdk/Vector.h>
#include <sdk/Map.h>
#include <sdk/MemoryRaw.h>

#include "CExecutiveManager.thread.hxx"
#include "CExecutiveManager.thread.activityreg.hxx"

#include "internal/CExecutiveManager.unfairmtx.internal.h"
#include "internal/CExecutiveManager.sem.internal.h"

BEGIN_NATIVE_EXECUTIVE

// We need some events for unfair mutexes.
static constinit optional_struct_space <EventPluginRegister> _runningThreadListEventRegister;
static constinit optional_struct_space <EventPluginRegister> _tlsThreadToNativeInfoLockEventRegister;

#ifdef __linux__

static constinit optional_struct_space <EventPluginRegister> _threadsToTermLockEventRegister;
static constinit optional_struct_space <EventPluginRegister> _threadsToTermSemEventRegister;

#endif //__linux__

// Events for shared stuff.
constinit optional_struct_space <EventPluginRegister> privateThreadEnvThreadReferenceLockEventRegister;
constinit optional_struct_space <EventPluginRegister> privateThreadEnvThreadPluginsLockEventRegister;

// The private thread environment that is public to the entire library.
constinit optional_struct_space <privateThreadEnvRegister_t> privateThreadEnv;

// We need a type for the thread ID.
#ifdef _WIN32
typedef DWORD threadIdType;
#elif defined(__linux__)
typedef pid_t threadIdType;
#else
#error Missing definition of the platform native thread id
#endif //CROSS PLATFORM CODE

struct nativeThreadPlugin
{
    // THESE FIELDS MUST NOT BE MODIFIED IN ANY WAY THAT BREAKS ASSEMBLER COMPATIBILITY.
    // (they are referenced from assembler)
#ifdef _WIN32
    Fiber *terminationReturn;   // if not nullptr, the thread yields to this state when it successfully terminated.
#elif defined(__linux__)
    void *userStack;
    size_t userStackSize;
#endif //CROSS PLATFORM ASSEMBLER DEPENDENCIES

    // You are free to modify from here.
    struct nativeThreadPluginInterface *manager;
    CExecThreadImpl *self;
    threadIdType codeThread;
#ifdef _WIN32
    HANDLE hThread;
#elif defined(__linux__)
    // True if the thread has been started; used to simulate the first Resume that is necessary.
    bool hasThreadStarted;
#else
#error No thread handle members implementation for this platform
#endif //CROSS PLATFORM CODE
    CUnfairMutexImpl mtxThreadLock;
    std::atomic <eThreadStatus> status;
    bool hasThreadBeenInitialized;
    bool isBeingCancelled;      // if true then no hazards can be entered, hazards are dismantled.

    CUnfairMutexImpl mtxVolatileHazardsLock;

    RwListEntry <nativeThreadPlugin> node;

    // Methods and stuff.
    nativeThreadPlugin( CExecThreadImpl *thread, CExecutiveManagerNative *manager );
};

struct thread_id_fetch
{
#ifdef _WIN32
    HANDLE hRunningThread = GetCurrentThread();
    threadIdType idRunningThread = GetThreadId( hRunningThread );
#elif defined(__linux__)
    threadIdType codeThread = _natexec_syscall_gettid();
#else
#error no thread identification fetch implementation
#endif //CROSS PLATFORM CODE

    AINLINE bool is_current( nativeThreadPlugin *thread ) const
    {
        return ( thread->codeThread == get_current_id() );
    }

    AINLINE threadIdType get_current_id( void ) const
    {
#ifdef _WIN32
        return this->idRunningThread;
#elif defined(__linux__)
        return this->codeThread;
#endif
    }
};

struct nativeThreadPluginInterface : public privateThreadEnvironment::threadPluginContainer_t::pluginInterface
{
    RwList <nativeThreadPlugin> runningThreads;
    CUnfairMutexImpl mtxRunningThreadList;

    // Need to have a per-thread mutex.
    DynamicEventPluginRegister <privateThreadEnvironment::threadPluginContainer_t> mtxThreadLockEventRegister;
    DynamicEventPluginRegister <privateThreadEnvironment::threadPluginContainer_t> mtxVolatileHazardsLockEventRegister;

    // Storage of native-thread to manager-struct relationship.
    eir::Map <threadIdType, nativeThreadPlugin*, NatExecStandardObjectAllocator> tlsThreadToNativeInfo;
    CUnfairMutexImpl mtxTLSThreadToNativeInfo;

#ifdef _WIN32
    // Nothing.
#elif defined(__linux__)
    CExecutiveManagerNative *self;
#ifndef NATEXEC_LINUX_THREAD_SELF_CLEANUP
    char freestackmem_threadStack[ NATEXEC_LINUX_FREE_STACKMEM_THREAD_STACKSIZE ];  // has to be HUGE because the stack aint a joke.
    pid_t freestackmem_procid;

    eir::Vector <nativeThreadPlugin*, NatExecStandardObjectAllocator> threadsToTerm;
    CUnfairMutexImpl mtxThreadsToTermLock;
    CSemaphoreImpl semThreadsToTerm;
#endif //not NATEXEC_LINUX_THREAD_SELF_CLEANUP

    size_t sysPageSize;

    // Events for certain thread things.
    DynamicEventPluginRegister <privateThreadEnvironment::threadPluginContainer_t> threadStartEventRegister;
    DynamicEventPluginRegister <privateThreadEnvironment::threadPluginContainer_t> threadRunningEventRegister;

    // Requiring version information for syscall specifics.
    NativeOSVersionInfo _verInfo;
#else
#error missing thread native TLS implementation
#endif //_WIN32

    bool isTerminating;

    // Safe runtime-reference releasing function.
    AINLINE void thread_end_of_life( CExecutiveManagerNative *manager, CExecThreadImpl *theThread, nativeThreadPlugin *nativeInfo )
    {
        // We must release the runtime lock at least after removing the runtime reference.
        // Thus we set thread to TERMINATED inside this function.
        manager->CloseThreadNative( theThread, true );
    }

#ifdef __linux__

    static void futex_wait_thread( pid_t *tid )
    {
        while ( true )
        {
            pid_t cur_tid = *tid;

            if ( cur_tid == 0 )
            {
                break;
            }

            long fut_error = _natexec_syscall_futex( tid, FUTEX_WAIT, cur_tid, nullptr, nullptr, 0 );
            (void)fut_error;
        }
    }

#ifndef NATEXEC_LINUX_THREAD_SELF_CLEANUP
    // On Linux we need a special signal thread for releasing stack memory after threads have terminated.
    // This is because stack memory is not handled by the OS itself, unlike in Windows.
    static int _linux_freeStackMem_thread( void *ud )
    {
        nativeThreadPluginInterface *nativeInfo = (nativeThreadPluginInterface*)ud;

        prctl( PR_SET_NAME, (unsigned long)"free-stack_mem", 0, 0, 0 );

        CExecutiveManagerNative *nativeMan = nativeInfo->self;

        while ( !nativeInfo->isTerminating )
        {
            nativeInfo->semThreadsToTerm.Decrement();

            // Check for any thread that needs cleanup.
            {
                CUnfairMutexContext ctxFetchTerm( nativeInfo->mtxThreadsToTermLock );

                for ( nativeThreadPlugin *termItem : nativeInfo->threadsToTerm )
                {
                    static_assert( sizeof(pid_t) == 4, "invalid machine pid_t word size" );

                    // Wait for the exit using our futex.
                    futex_wait_thread( &termItem->codeThread );

                    // Free the thread stack.
                    if ( void *stack = termItem->userStack )
                    {
                        int err_unmap = _natexec_syscall_munmap( stack, termItem->userStackSize );

                        assert( err_unmap == 0 );

                        termItem->userStack = nullptr;
                    }

                    // Release the thread "runtime reference".
                    nativeInfo->thread_end_of_life( nativeMan, termItem->self, termItem );
                }

                nativeInfo->threadsToTerm.Clear();
            }
        }

        return 0;
    }
#endif //not NATEXEC_LINUX_THREAD_SELF_CLEANUP

#endif //__linux__

    inline nativeThreadPluginInterface( CExecutiveManagerNative *nativeMan ) :
        mtxRunningThreadList( _runningThreadListEventRegister.get().GetEvent( nativeMan ) )
        , tlsThreadToNativeInfo( eir::constr_with_alloc::DEFAULT, nativeMan )
        , mtxTLSThreadToNativeInfo( _tlsThreadToNativeInfoLockEventRegister.get().GetEvent( nativeMan ) )
#ifdef __linux__
#ifndef NATEXEC_LINUX_THREAD_SELF_CLEANUP
        , threadsToTerm( eir::constr_with_alloc::DEFAULT, nativeMan )
        , mtxThreadsToTermLock( _threadsToTermLockEventRegister.get().GetEvent( nativeMan ) )
        , semThreadsToTerm( _threadsToTermSemEventRegister.get().GetEvent( nativeMan ) )
#endif //not NATEXEC_LINUX_THREAD_SELF_CLEANUP
#endif //__linux__
    {
        return;
    }

    inline void Initialize( CExecutiveManagerNative *nativeMan )
    {
        privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan );

        assert( threadEnv != nullptr );

        mtxThreadLockEventRegister.RegisterPlugin( threadEnv->threadPlugins );
        mtxVolatileHazardsLockEventRegister.RegisterPlugin( threadEnv->threadPlugins );

#ifdef _WIN32
        // Nothing.
#elif defined(__linux__)
        threadStartEventRegister.RegisterPlugin( threadEnv->threadPlugins );
        threadRunningEventRegister.RegisterPlugin( threadEnv->threadPlugins );
#else
#error no implementation for native thread plugin interface init
#endif //CROSS PLATFORM CODE

        this->isTerminating = false;

#ifdef __linux__
        long pageSize = sysconf(_SC_PAGESIZE);

        assert( pageSize > 0 );

        this->sysPageSize = (size_t)pageSize;
        this->self = nativeMan;

#define _LINUX_PROC_CLONE_PARAMS (CLONE_SIGHAND|CLONE_THREAD|CLONE_PARENT|CLONE_VM|CLONE_FILES|CLONE_FS|CLONE_SYSVSEM)

#ifndef NATEXEC_LINUX_THREAD_SELF_CLEANUP
        // Please note that the cleanup stack thread has very limited functionality, specially it does not support TLS activity.
        // We may change this in the future where we explicitly require CRT support to allow registration of TLS data in
        // other plugins aswell. We may also turn the stack of this thread into a plugin so that the number can be retrieved from
        // the CRT instead.

        int maintainThreadSucc =
            _natexec_syscall_clone(
                _linux_freeStackMem_thread, this->freestackmem_threadStack + countof( this->freestackmem_threadStack ),
                _LINUX_PROC_CLONE_PARAMS|CLONE_CHILD_CLEARTID,
                this, nullptr, nullptr, &this->freestackmem_procid
            );

        assert( maintainThreadSucc > 0 );

        this->freestackmem_procid = (pid_t)maintainThreadSucc;
#endif //not NATEXEC_LINUX_THREAD_SELF_CLEANUP
#endif //__linux__
    }

    inline void Shutdown( CExecutiveManagerNative *nativeMan )
    {
#ifdef _WIN32
        // Nothing.
#elif defined(__linux__)
#ifndef NATEXEC_LINUX_THREAD_SELF_CLEANUP
        // Wait for maintainer thread termination.
        this->semThreadsToTerm.Increment();

        futex_wait_thread( &this->freestackmem_procid );
#endif //not NATEXEC_LINUX_THREAD_SELF_CLEANUP

        // We simply forget the TLS mappings. No big deal.

        // Unregister thread runtime events.
        threadRunningEventRegister.UnregisterPlugin();
        threadStartEventRegister.UnregisterPlugin();
#else
#error no implementation for native thread plugin interface shutdown
#endif //CROSS PLATFORM CODE

        // Shutdown the per-thread plugins.
        mtxVolatileHazardsLockEventRegister.UnregisterPlugin();
        mtxThreadLockEventRegister.UnregisterPlugin();
    }

    inline void TlsSetCurrentThreadInfo( nativeThreadPlugin *info )
    {
        thread_id_fetch id;

        CUnfairMutexContext ctxSetTLSInfo( this->mtxTLSThreadToNativeInfo );

        if ( info == nullptr )
        {
            tlsThreadToNativeInfo.RemoveByKey( id.get_current_id() );
        }
        else
        {
            tlsThreadToNativeInfo[ id.get_current_id() ] = info;
        }
    }

    inline nativeThreadPlugin* TlsGetCurrentThreadInfo( void )
    {
        nativeThreadPlugin *plugin = nullptr;

        {
            thread_id_fetch id;

            CUnfairMutexContext ctxGetTLSInfo( this->mtxTLSThreadToNativeInfo );

            auto findIter = tlsThreadToNativeInfo.Find( id.get_current_id() );

            if ( findIter != nullptr )
            {
                plugin = findIter->GetValue();
            }
        }

        return plugin;
    }

    inline void TlsCleanupThreadInfo( nativeThreadPlugin *info )
    {
        tlsThreadToNativeInfo.RemoveByKey( info->codeThread );
    }

    static void _ThreadProcCPP( nativeThreadPlugin *info )
    {
        CExecThreadImpl *threadInfo = info->self;

        // Put our executing thread information into our TLS value.
        info->manager->TlsSetCurrentThreadInfo( info );

        // Make sure we intercept termination requests!
        try
        {
            {
                CUnfairMutexContext mtxThreadLock( info->mtxThreadLock );

                // We are properly initialized now.
                info->hasThreadBeenInitialized = true;
            }

            // Enter the routine.
            threadInfo->entryPoint( threadInfo, threadInfo->userdata );
        }
        catch( ... )
        {
            // We have to safely quit.
        }

        // We are terminating.
        {
            CUnfairMutexContext mtxThreadLock( info->mtxThreadLock );
            CUnfairMutexContext mtxThreadState( threadInfo->mtxThreadStatus );

            info->status = THREAD_TERMINATING;
        }

        // Leave this proto. The native implementation has the job to set us terminated.
    }

#ifdef _WIN32
    // This is a C++ proto. We must leave into a ASM proto to finish operation.
    static DWORD WINAPI _Win32_ThreadProcCPP( LPVOID param )
    {
        _ThreadProcCPP( (nativeThreadPlugin*)param );

        return ERROR_SUCCESS;
    }
#endif

    bool RtlThreadResumeSuspended( CExecThreadImpl *nativeThread, nativeThreadPlugin *info )
    {
        bool hasResumed = false;

#ifdef _WIN32
        BOOL success = ResumeThread( info->hThread );

        hasResumed = ( success == TRUE );
#elif defined(__linux__)
        // We want to support initial resumption of Linux threads.
        if ( !info->hasThreadStarted )
        {
            // Should be sufficient to have the threadLock.

            // Mark our thread to start running.
            CEvent *eventStart = this->threadStartEventRegister.GetEvent( nativeThread );
            eventStart->Set( false );

            info->hasThreadStarted = true;

            hasResumed = true;
        }
#else
#error No thread resume implementation
#endif

        return hasResumed;
    }

    // Make sure to lock mtxVolatileHazardsLock if necessary!
    static inline void RtlTerminateHazards( CExecutiveManagerNative *manager, CExecThreadImpl *theThread )
    {
        executiveHazardManagerEnv *hazardEnv = executiveHazardManagerEnvRegister.get().GetPluginStruct( manager );

        if ( hazardEnv )
        {
            hazardEnv->PurgeThreadHazards( theThread );
        }
    }

    // REQUIREMENT: WRITE ACCESS on lockThreadStatus of threadInfo handle
    void RtlTerminateThread( CExecutiveManager *manager, nativeThreadPlugin *threadInfo, CUnfairMutexContext& ctxLock, bool waitOnRemote )
    {
        CExecThreadImpl *theThread = threadInfo->self;

        assert( manager->IsRemoteThread( theThread ) == false );

        // If we are not the current thread, we must do certain precautions.
        bool isCurrentThread = theThread->IsCurrent();

        bool has_to_be_woken_up = false;

        // Set our status to terminating.
        // The moment we set this the thread starts terminating.
        {
            CUnfairMutexContext ctxThreadStatus( theThread->mtxThreadStatus );

            // Check whether we are suspended. Then we have to be woken up/started.
            has_to_be_woken_up = ( threadInfo->status == THREAD_SUSPENDED );

            threadInfo->status = THREAD_TERMINATING;
        }

        // Depends on whether we are the current thread or not.
        if ( isCurrentThread )
        {
            // Just do the termination.
            throw threadTerminationException( theThread );
        }
        else
        {
            // Is a wake-up required?
            if ( has_to_be_woken_up )
            {
                bool wakeup_success = RtlThreadResumeSuspended( theThread, threadInfo );
                (void)wakeup_success;

#ifdef _DEBUG
                assert( wakeup_success == true );
#endif //_DEBUG
            }

            // We do not need the lock anymore.
            ctxLock.release();

            // Terminate all possible hazards.
            // We cannot receive any new hazards because we are in the THREAD_TERMINATING state.
            // For the same reason we cannot suddenly lose any hazards.
            // Thus get rid of any existing hazards.
            // We must enter the mtxVolatileHazardsLock as both the thread termination and the
            // cancellation system can spawn hazard-termination activity. To exclude runtime
            // overlap we use that mutex.
            {
                CUnfairMutexContext mtx_mutualExclusion( threadInfo->mtxVolatileHazardsLock );

                RtlTerminateHazards( (CExecutiveManagerNative*)manager, theThread );
            }

            if ( waitOnRemote )
            {
#ifdef __linux__
                CEvent *evtRunning = this->threadRunningEventRegister.GetEvent( theThread );
#endif //__linux__

                // Wait for thread termination.
                while ( threadInfo->status != THREAD_TERMINATED )
                {
                    // Wait till the thread has really finished.
#ifdef _WIN32
                    WaitForSingleObject( threadInfo->hThread, INFINITE );
#elif defined(__linux__)
                    evtRunning->Wait();
#else
#error no thread wait for termination implementation
#endif //CROSS PLATFORM CODE
                }

                // If we return here, the thread must be terminated.
            }

            // TODO: allow safe termination of suspended threads.
        }

        // If we were the current thread, we cannot reach this point.
        assert( isCurrentThread == false );
    }

    bool OnPluginConstruct( CExecThreadImpl *thread, privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t pluginOffset, privateThreadEnvironment::threadPluginContainer_t::pluginDescriptor id ) override;
    void OnPluginDestruct( CExecThreadImpl *thread, privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t pluginOffset, privateThreadEnvironment::threadPluginContainer_t::pluginDescriptor id ) noexcept override;
};

#ifdef _WIN32
// Assembly routines for important thread events.
extern "C" DWORD WINAPI nativeThreadPluginInterface_ThreadProcCPP( LPVOID param )
{
    // This is an assembler compatible entry point.
    return nativeThreadPluginInterface::_Win32_ThreadProcCPP( param );
}

extern "C" void WINAPI nativeThreadPluginInterface_OnNativeThreadEnd( nativeThreadPlugin *nativeInfo )
{
    // The assembler finished using us, so do clean up work.
    CExecThreadImpl *theThread = nativeInfo->self;

    CExecutiveManagerNative *manager = theThread->manager;

    // NOTE: this is OKAY on Windows because we do not allocate the stack space ourselves!
    // On Linux for example we have to free the stack space using a different thread.

    // Officially terminated now.
    nativeInfo->manager->thread_end_of_life( manager, theThread, nativeInfo );
}
#elif defined(__linux__)

static int _linux_threadEntryPoint( void *in_ptr )
{
    nativeThreadPlugin *info = (nativeThreadPlugin*)in_ptr;

    CExecThreadImpl *nativeThread = info->self;

    // If we have a name hint, then set it right now.
    if ( const char *hint_thread_name = nativeThread->hint_thread_name )
    {
        prctl( PR_SET_NAME, (unsigned long)hint_thread_name, 0, 0, 0 );
    }

    // Initialize with the CRT.
    _threadsupp_glibc_enter_thread( nativeThread );

    nativeThreadPluginInterface *nativeMan = info->manager;

    // Wait for the real thread start event.
    {
        CEvent *eventStart = nativeMan->threadStartEventRegister.GetEvent( nativeThread );

        eventStart->Wait();
    }

    // Invoke thread runtime.
    {
        nativeThreadPluginInterface::_ThreadProcCPP( info );

        // There is a difference in implementation between Windows and Linux in that thread
        // runtime prematurely is reported finished using waiting-semantics under Linux.
        // This is not a problem for as long as things are thread-safe.
    }

#ifndef NATEXEC_LINUX_THREAD_SELF_CLEANUP
    // Cleanup CRT dependencies.
    _threadsupp_glibc_leave_thread( nativeThread );

    // We finished using the thread, so clean up.
    // This is done by notifying the termination runtime.
    {
        CUnfairMutexContext ctxPushTermThread( nativeMan->mtxThreadsToTermLock );

        nativeMan->threadsToTerm.AddToBack( info );

        nativeMan->semThreadsToTerm.Increment();
    }
#else
    // Remember stack information.
    void *stackaddr = info->userStack;
    size_t stacksize = info->userStackSize;

    // Just notify that the thread is no more.
    nativeMan->thread_end_of_life( nativeMan->self, nativeThread, info );

    // We have to call into assembler to finish off our thread here.
    // This function call will not return.
    _natexec_linux_end_thread( stackaddr, stacksize );
#endif //THREAD TERMINATION SEGMENT

    return 0;
}

#endif //CROSS PLATFORM CODE

bool nativeThreadPluginInterface::OnPluginConstruct( CExecThreadImpl *thread, privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t pluginOffset, privateThreadEnvironment::threadPluginContainer_t::pluginDescriptor id )
{
    // Cannot create threads if we are terminating!
    if ( this->isTerminating )
    {
        return false;
    }

    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)thread->manager;

    void *info_ptr = privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <void> ( thread, pluginOffset );

    nativeThreadPlugin *info = new (info_ptr) nativeThreadPlugin( thread, nativeMan );

    // Give ourselves a self reference pointer.
    info->self = thread;
    info->manager = this;

#ifdef _WIN32
    // If we are not a remote thread...
    HANDLE hOurThread = nullptr;

    if ( !thread->isLocallyRemoteThread )
    {
        // ... create a local thread!

#ifndef _WIN32_NO_THREAD_CRT_SUPPORT
        unsigned int threadIdOut;
        _beginthreadex_proc_type startRoutine = nullptr;

#if defined(_M_IX86)
        startRoutine = (_beginthreadex_proc_type)_thread86_procNative;
#elif defined(_M_AMD64)
        startRoutine = (_beginthreadex_proc_type)_thread64_procNative;
#else
#error missing startRoutine declaration for platform.
#endif
#else
        DWORD threadIdOut;
        LPTHREAD_START_ROUTINE startRoutine = nullptr;

#if defined(_M_IX86)
        startRoutine = (LPTHREAD_START_ROUTINE)_thread86_procNative;
#elif defined(_M_AMD64)
        startRoutine = (LPTHREAD_START_ROUTINE)_thread64_procNative;
#else
#error missing startRoutine declaration for platform.
#endif

#endif //not _WIN32_NO_THREAD_CRT_SUPPORT

        HANDLE hThread = nullptr;

#ifndef _WIN32_NO_THREAD_CRT_SUPPORT
        unsigned int stack_size = TRANSFORM_INT_CLAMP <unsigned int> ( thread->stackSize );

        // _endthreadex is automatically called in the callstack of the internal _beginthreadex startRoutine wrapper.
        hThread = (HANDLE)_beginthreadex( nullptr, stack_size, startRoutine, info, CREATE_SUSPENDED, &threadIdOut );
#else
        SIZE_T stack_size = TRANSFORM_INT_CLAMP <SIZE_T> ( thread->stackSize );

        hThread = ::CreateThread( nullptr, stack_size, startRoutine, info, CREATE_SUSPENDED, &threadIdOut );
#endif //not _WIN32_NO_THREAD_CRT_SUPPORT

        if ( hThread == nullptr )
            return false;

        hOurThread = hThread;

        info->hThread = hOurThread;
        info->codeThread = GetThreadId( hOurThread );
    }
    else
    {
        // This information is actually filled by the runtime that spawned this remote thread reference.
        info->hThread = INVALID_HANDLE_VALUE;
        info->codeThread = std::numeric_limits <decltype(info->codeThread)>::max();
    }

#elif defined(__linux__)
    // Need to initialize the state events.
    CEvent *eventStartThread = this->threadStartEventRegister.GetEvent( thread );
    eventStartThread->Set( false );
    CEvent *eventRunningThread = this->threadRunningEventRegister.GetEvent( thread );
    eventRunningThread->Set( false );

    pid_t our_thread_id = -1;
    void *our_userStack = nullptr;
    size_t our_userStackSize = 0;

    if ( !thread->isLocallyRemoteThread )
    {
        // On linux we use the native clone syscall to create a thread.
        size_t theStackSize = thread->stackSize;

        // If the user was undecided, then we just set it to some good value instead.
        if ( theStackSize == 0 )
        {
            theStackSize = 1 << 18;
        }

        // Make sure the stack size is aligned properly.
        size_t sysPageSize = this->sysPageSize;
        theStackSize = ALIGN( theStackSize, sysPageSize, sysPageSize );

        // Adjust memory flags depending on kernel version.
        int mmap_flags = MAP_PRIVATE|MAP_STACK|MAP_ANONYMOUS;

        if ( this->_verInfo.IsThirdPartyKernel() == false )
        {
            // Under non-MSFT kernels optional flags can be provided without erroring out.
            // Thus we say here that we do not care if we get dirty bytes through mmap.
            // It is advertised as performance optimization on the web, so yea.
            mmap_flags |= MAP_UNINITIALIZED;
        }

        void *stack_mem = _natexec_syscall_mmap( nullptr, theStackSize, PROT_READ|PROT_WRITE, mmap_flags, -1, 0 );

        if ( stack_mem == MAP_FAILED )
        {
            return false;
        }

        // Acquire TLS resources.
        void *tcb = _threadsupp_glibc_get_tcb( thread );

        // Initially take the runtime lock.
        // This is to prevent the thread from starting till the user wants to.
        eventStartThread->Set( true );
        eventRunningThread->Set( true );

        // We actually return the end of stack pointer, because we assume stack __always__ grows downwards.
        // TODO: this is not true all the time (mind obscure platforms)
        void *stack_beg_ptr = ( (char*)stack_mem + theStackSize );

        int clone_res =
            _natexec_syscall_clone(
                _linux_threadEntryPoint, stack_beg_ptr,
                /*SIGCHLD|CLONE_PTRACE|*/CLONE_CHILD_CLEARTID|CLONE_SETTLS|_LINUX_PROC_CLONE_PARAMS,
                info, nullptr, tcb, &info->codeThread
            );

        if ( clone_res == -1 )
        {
            eventStartThread->Set( false );
            eventRunningThread->Set( false );

            _natexec_syscall_munmap( stack_mem, theStackSize );
            return false;
        }

        our_thread_id = (pid_t)clone_res;
        our_userStack = stack_mem;
        our_userStackSize = theStackSize;

        info->hasThreadStarted = false;
    }
    else
    {
        // Since we do not control this thread we just return nothing.
        info->hasThreadStarted = true;
    }
    info->codeThread = our_thread_id;
    info->userStack = our_userStack;
    info->userStackSize = our_userStackSize;
#else
#error No thread creation implementation
#endif //CROSS PLATFORM CODE

    // NOTE: we initialize remote threads in the GetCurrentThread routine!

#ifdef _WIN32
    // This field is used by the runtime dispatcher to execute a "controlled return"
    // from different threads.
    info->terminationReturn = nullptr;
#endif //_WIN32

    info->hasThreadBeenInitialized = false;
    info->isBeingCancelled = false;

    // We must let the thread terminate itself.
    // So it is mandatory to give it a reference,
    // also called the "runtime reference".
    thread->refCount++;

    // We assume the thread is (always) running if its a remote thread.
    // Otherwise we know that it starts suspended.
    info->status = ( !thread->isLocallyRemoteThread ) ? THREAD_SUSPENDED : THREAD_RUNNING;

    // Add it to visibility.
    {
        CUnfairMutexContext ctxThreadList( this->mtxRunningThreadList );

        LIST_INSERT( runningThreads.root, info->node );
    }
    return true;
}

void nativeThreadPluginInterface::OnPluginDestruct( CExecThreadImpl *thread, privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t pluginOffset, privateThreadEnvironment::threadPluginContainer_t::pluginDescriptor id ) noexcept
{
    nativeThreadPlugin *info = privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <nativeThreadPlugin> ( thread, pluginOffset );

    // We must destroy the handle only if we are terminated.
    if ( !thread->isLocallyRemoteThread )
    {
        assert( info->status == THREAD_TERMINATED );
    }

    // Remove the thread from visibility.
    this->TlsCleanupThreadInfo( info );
    {
        CUnfairMutexContext ctxThreadList( this->mtxRunningThreadList );

        LIST_REMOVE( info->node );
    }

    // Close OS resources.
#ifdef _WIN32
    CloseHandle( info->hThread );
#elif defined(__linux__)
#ifndef NATEXEC_LINUX_THREAD_SELF_CLEANUP
    // We should have released our stack already.
    assert( info->userStack == nullptr );
#endif //not NATEXEC_LINUX_THREAD_SELF_CLEANUP
#else
#error No implementation for thread info shutdown
#endif //CROSS PLATFORM CODE

    // Destroy the plugin.
    info->~nativeThreadPlugin();
}

// todo: add other OSes too when it becomes necessary.

struct privateNativeThreadEnvironment
{
    nativeThreadPluginInterface _nativePluginInterface;

    privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t nativePluginOffset;

    inline privateNativeThreadEnvironment( CExecutiveManagerNative *natExec ) : _nativePluginInterface( natExec )
    {
        return;
    }

    inline void Initialize( CExecutiveManagerNative *manager )
    {
        privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( manager );

        assert( threadEnv != nullptr );

        _nativePluginInterface.Initialize( manager );

        this->nativePluginOffset =
            threadEnv->threadPlugins.RegisterPlugin( sizeof( nativeThreadPlugin ), alignof( nativeThreadPlugin ), THREAD_PLUGIN_NATIVE, &_nativePluginInterface );
    }

    inline void Shutdown( CExecutiveManagerNative *manager )
    {
        privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( manager );

        assert( threadEnv != nullptr );

        // Notify ourselves that we are terminating.
        _nativePluginInterface.isTerminating = true;

        // Shutdown all currently yet active threads.
        while ( !LIST_EMPTY( manager->threads.root ) )
        {
            CExecThreadImpl *thread = LIST_GETITEM( CExecThreadImpl, manager->threads.root.next, managerNode );

            manager->CloseThread( thread );
        }

        if ( privateThreadEnvironment::threadPluginContainer_t::IsOffsetValid( this->nativePluginOffset ) )
        {
            threadEnv->threadPlugins.UnregisterPlugin( this->nativePluginOffset );
        }

        _nativePluginInterface.Shutdown( manager );
    }
};

static constinit optional_struct_space <PluginDependantStructRegister <privateNativeThreadEnvironment, executiveManagerFactory_t>> privateNativeThreadEnvironmentRegister;

nativeThreadPlugin::nativeThreadPlugin( CExecThreadImpl *thread, CExecutiveManagerNative *natExec ) :
    mtxThreadLock( privateNativeThreadEnvironmentRegister.get().GetPluginStruct( natExec )->_nativePluginInterface.mtxThreadLockEventRegister.GetEvent( thread ) ),
    mtxVolatileHazardsLock( privateNativeThreadEnvironmentRegister.get().GetPluginStruct( natExec )->_nativePluginInterface.mtxVolatileHazardsLockEventRegister.GetEvent( thread ) )
{
    return;
}

inline nativeThreadPlugin* GetNativeThreadPlugin( CExecutiveManagerNative *manager, CExecThreadImpl *theThread )
{
    privateNativeThreadEnvironment *nativeThreadEnv = privateNativeThreadEnvironmentRegister.get().GetPluginStruct( manager );

    if ( nativeThreadEnv )
    {
        return privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <nativeThreadPlugin> ( theThread, nativeThreadEnv->nativePluginOffset );
    }

    return nullptr;
}

inline const nativeThreadPlugin* GetConstNativeThreadPlugin( const CExecutiveManager *manager, const CExecThreadImpl *theThread )
{
    const privateNativeThreadEnvironment *nativeThreadEnv = privateNativeThreadEnvironmentRegister.get().GetConstPluginStruct( (const CExecutiveManagerNative*)manager );

    if ( nativeThreadEnv )
    {
        return privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <nativeThreadPlugin> ( theThread, nativeThreadEnv->nativePluginOffset );
    }

    return nullptr;
}

// For the Windows.h header gunk.
#undef CreateEvent

CExecThreadImpl::CExecThreadImpl( CExecutiveManagerNative *manager, bool isLocallyRemoteThread, const char *hint_thread_name, void *userdata, size_t stackSize, threadEntryPoint_t entryPoint )
    : mtxThreadStatus( manager->CreateEvent() )
{
    this->manager = manager;
    this->isLocallyRemoteThread = isLocallyRemoteThread;
    this->hint_thread_name = hint_thread_name;
    this->userdata = userdata;
    this->stackSize = stackSize;
    this->entryPoint = entryPoint;

    // During construction we must not have a reference to ourselves.
    this->refCount = 0;
    this->isClosingDescriptor = false;

    LIST_INSERT( manager->threads.root, managerNode );
}

CExecThreadImpl::~CExecThreadImpl( void )
{
    //CExecutiveManagerNative *nativeMan = this->manager;

    FATAL_ASSERT( this->refCount == 0 );
    FATAL_ASSERT( this->isClosingDescriptor == true );

    LIST_REMOVE( managerNode );

    // Clean-up the event of the mutex.
    manager->CloseEvent( this->mtxThreadStatus.get_event() );
}

CExecutiveManager* CExecThread::GetManager( void )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)this;

    return nativeThread->manager;
}

eThreadStatus CExecThreadImpl::GetStatusNative( void ) const
{
    eThreadStatus status = THREAD_TERMINATED;

    const nativeThreadPlugin *info = GetConstNativeThreadPlugin( this->manager, this );

    if ( info )
    {
        status = info->status;
    }

    return status;
}

eThreadStatus CExecThread::GetStatus( void ) const
{
    const CExecThreadImpl *nativeThread = (const CExecThreadImpl*)this;

    return nativeThread->GetStatusNative();
}

// WARNING: terminating threads in general is very naughty and causes shit to go haywire!
// No matter what thread state, this function guarrantees to terminate a (non-remote) thread cleanly according to
// C++ stack unwinding logic!
// Termination of a thread is allowed to be executed by another thread (e.g. the "main"/initial thread).
// NOTE: logic has been changed to be secure. now proper terminating depends on a contract between runtime
// and the NativeExecutive library.
bool CExecThread::Terminate( bool waitOnRemote )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)this;

    bool returnVal = false;

    CExecutiveManagerNative *nativeMan = nativeThread->manager;

    nativeThreadPlugin *info = GetNativeThreadPlugin( nativeMan, nativeThread );

    if ( info && info->status != THREAD_TERMINATED )
    {
        // We cannot terminate a terminating thread.
        if ( info->status != THREAD_TERMINATING )
        {
            CUnfairMutexContext ctxThreadLock( info->mtxThreadLock );

            if ( info->status != THREAD_TERMINATING && info->status != THREAD_TERMINATED )
            {
                // Termination depends on what kind of thread we face.
                if ( nativeMan->IsRemoteThread( nativeThread ) )
                {
                    bool hasTerminated = false;

#ifdef _WIN32
                    // Remote threads must be killed just like that.
                    BOOL success = TerminateThread( info->hThread, ERROR_SUCCESS );

                    hasTerminated = ( success == TRUE );
#elif defined(__linux__)
                    // TODO. the docs say that this pid_t system sucks because it is suspect to
                    // ID-override causing kill of random threads/processes. Need to solve this
                    // somehow.
                    int success = _natexec_syscall_tkill( info->codeThread, SIGKILL );

                    hasTerminated = ( success == 0 );
#else
#error No implementation for thread kill
#endif

                    if ( hasTerminated )
                    {
                        // Put the status as terminated.
                        {
                            CUnfairMutexContext ctxThreadStatus( nativeThread->mtxThreadStatus );

                            info->status = THREAD_TERMINATED;
                        }

                        // Return true.
                        returnVal = true;
                    }
                }
                else
                {
                    privateNativeThreadEnvironment *nativeEnv = privateNativeThreadEnvironmentRegister.get().GetPluginStruct( (CExecutiveManagerNative*)nativeThread->manager );

                    if ( nativeEnv )
                    {
                        // User-mode threads have to be cleanly terminated.
                        // This means going down the exception stack.
                        nativeEnv->_nativePluginInterface.RtlTerminateThread( nativeThread->manager, info, ctxThreadLock, waitOnRemote );

                        // We may not actually get here!
                    }

                    // We have successfully terminated the thread.
                    returnVal = true;
                }
            }
        }
    }

    return returnVal;
}

void CExecThread::SetThreadCancelling( bool enableCancel )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)this;

    CExecutiveManagerNative *nativeMan = nativeThread->manager;

    nativeThreadPlugin *nativeInfo = GetNativeThreadPlugin( nativeMan, nativeThread );

    if ( nativeInfo )
    {
        CUnfairMutexContext mtx_cancelVolatility( nativeInfo->mtxVolatileHazardsLock );

        if ( nativeInfo->isBeingCancelled != enableCancel )
        {
            // Update the cancellation state.
            nativeInfo->isBeingCancelled = enableCancel;

            if ( enableCancel )
            {
                // Feel free to purge them hazards now.
                // Must not do it under the powerful thread lock because actions on the thread
                // itself should still be possible.
                nativeThreadPluginInterface::RtlTerminateHazards( nativeMan, nativeThread );
            }
        }
    }
}

bool CExecThread::IsThreadCancelling( void ) const
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)this;

    CExecutiveManagerNative *nativeMan = nativeThread->manager;

    nativeThreadPlugin *nativeInfo = GetNativeThreadPlugin( nativeMan, nativeThread );

    if ( nativeInfo )
    {
        return nativeInfo->isBeingCancelled;
    }

    return false;
}

void CExecThreadImpl::CheckTerminationRequest( void )
{
    // Must be performed on the current thread!

    // If we are terminating, we probably should do that.
    if ( this->GetStatusNative() == THREAD_TERMINATING )
    {
        // We just throw a thread termination exception.
        throw threadTerminationException( this );   // it is kind of not necessary to pass the thread handle, but okay.
    }

    // If we are cancelling then we should throw for this reason.
    if ( this->IsThreadCancelling() )
    {
        // Weaker exception than the termination case but fair enough.
        throw threadCancellationException( this );
    }
}

bool CExecThread::Suspend( void )
{
    bool returnVal = false;

#ifdef _WIN32
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)this;

    nativeThreadPlugin *info = GetNativeThreadPlugin( nativeThread->manager, nativeThread );

    // We cannot suspend a remote thread.
    if ( !nativeThread->isLocallyRemoteThread )
    {
        // Please note how we cannot suspend a terminating thread.
        // This is by design and expected behavior.
        if ( info && info->status == THREAD_RUNNING )
        {
            CUnfairMutexContext ctxThreadLock( info->mtxThreadLock );

            if ( info->status == THREAD_RUNNING )
            {
                DWORD susp_cnt = SuspendThread( info->hThread );

                if ( susp_cnt != (DWORD)-1 )
                {
                    CUnfairMutexContext ctxThreadStatus( nativeThread->mtxThreadStatus );

                    info->status = THREAD_SUSPENDED;

                    returnVal = true;
                }
            }
        }
    }
#elif defined(__linux__)
    // There is no thread suspension on Linux.
    returnVal = false;
#else
#error No thread suspend implementation
#endif

    return returnVal;
}

bool CExecThread::Resume( void )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)this;

    bool returnVal = false;

    CExecutiveManagerNative *nativeMan = nativeThread->manager;

    // We cannot resume a remote thread.
    if ( !nativeThread->isLocallyRemoteThread )
    {
        nativeThreadPlugin *info = GetNativeThreadPlugin( nativeMan, nativeThread );

        if ( info && info->status == THREAD_SUSPENDED )
        {
            CUnfairMutexContext ctxThreadLock( info->mtxThreadLock );

            if ( info->status == THREAD_SUSPENDED )
            {
                nativeThreadPluginInterface *nativeThreadMan = &privateNativeThreadEnvironmentRegister.get().GetPluginStruct( nativeMan )->_nativePluginInterface;

                bool hasResumed = nativeThreadMan->RtlThreadResumeSuspended( nativeThread, info );

                if ( hasResumed )
                {
                    CUnfairMutexContext ctxThreadStatus( nativeThread->mtxThreadStatus );

                    info->status = THREAD_RUNNING;

                    returnVal = true;
                }
            }
        }
    }

    return returnVal;
}

bool CExecThread::IsCurrent( void )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)this;

    return ( nativeThread->manager->IsCurrentThread( nativeThread ) );
}

void* CExecThread::ResolvePluginMemory( threadPluginOffset offset )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)this;

    return privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <void> ( nativeThread, offset );
}

const void* CExecThread::ResolvePluginMemory( threadPluginOffset offset ) const
{
    const CExecThreadImpl *nativeThread = (const CExecThreadImpl*)this;

    return privateThreadEnvironment::threadPluginContainer_t::RESOLVE_STRUCT <void> ( nativeThread, offset );
}

bool CExecThread::IsPluginOffsetValid( threadPluginOffset offset ) noexcept
{
    return privateThreadEnvironment::threadPluginContainer_t::IsOffsetValid( offset );
}

threadPluginOffset CExecThread::GetInvalidPluginOffset( void ) noexcept
{
    return privateThreadEnvironment::threadPluginContainer_t::INVALID_PLUGIN_OFFSET;
}

threadPluginOffset CExecutiveManager::RegisterThreadPlugin( size_t pluginSize, size_t pluginAlignment, threadPluginInterface *pluginInterface )
{
    struct threadPluginInterface_pipe : public privateThreadEnvironment::threadPluginContainer_t::pluginInterface
    {
        inline threadPluginInterface_pipe( CExecutiveManagerNative *nativeMan, ExecutiveManager::threadPluginInterface *publicIntf ) noexcept
        {
            this->nativeMan = nativeMan;
            this->publicIntf = publicIntf;
        }

        bool OnPluginConstruct( CExecThreadImpl *nativeThread, privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t pluginOff, privateThreadEnvironment::threadPluginContainer_t::pluginDescriptor pluginDesc ) override
        {
            return this->publicIntf->OnPluginConstruct( nativeThread, pluginOff, ExecutiveManager::threadPluginDescriptor( pluginDesc.pluginId ) );
        }

        void OnPluginDestruct( CExecThreadImpl *nativeThread, privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t pluginOff, privateThreadEnvironment::threadPluginContainer_t::pluginDescriptor pluginDesc ) noexcept override
        {
            this->publicIntf->OnPluginDestruct( nativeThread, pluginOff, ExecutiveManager::threadPluginDescriptor( pluginDesc.pluginId ) );
        }

        bool OnPluginCopyAssign( CExecThreadImpl *dstNativeThread, const CExecThreadImpl *srcNativeThread, privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t pluginOff, privateThreadEnvironment::threadPluginContainer_t::pluginDescriptor pluginDesc ) override
        {
            return this->publicIntf->OnPluginCopyAssign( dstNativeThread, srcNativeThread, pluginOff, ExecutiveManager::threadPluginDescriptor( pluginDesc.pluginId ) );
        }

        bool OnPluginMoveAssign( CExecThreadImpl *dstNativeThread, CExecThreadImpl *srcNativeThread, privateThreadEnvironment::threadPluginContainer_t::pluginOffset_t pluginOff, privateThreadEnvironment::threadPluginContainer_t::pluginDescriptor pluginDesc ) override
        {
            return this->publicIntf->OnPluginMoveAssign( dstNativeThread, srcNativeThread, pluginOff, ExecutiveManager::threadPluginDescriptor( pluginDesc.pluginId ) );
        }

        void DeleteOnUnregister( void ) noexcept override
        {
            NatExecStandardObjectAllocator memAlloc( this->nativeMan );
            
            this->publicIntf->DeleteOnUnregister();

            eir::dyn_del_struct <threadPluginInterface_pipe> ( memAlloc, nullptr, this );
        }

        CExecutiveManagerNative *nativeMan;
        ExecutiveManager::threadPluginInterface *publicIntf;
    };

    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan );

    if ( !threadEnv )
        return privateThreadEnvironment::threadPluginContainer_t::INVALID_PLUGIN_OFFSET;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    threadPluginInterface_pipe *threadIntf = eir::dyn_new_struct <threadPluginInterface_pipe> ( memAlloc, nullptr, nativeMan, pluginInterface );

    assert( threadIntf != nullptr );

    // Suspend the thread activities for a short while.
    stacked_thread_activity_suspend suspend_activity( nativeMan );

    // Kill all other thread handles.
    PurgeActiveThreads();

    return threadEnv->threadPlugins.RegisterPlugin( pluginSize, pluginAlignment, privateThreadEnvironment::threadPluginContainer_t::ANONYMOUS_PLUGIN_ID, threadIntf );
}

void CExecutiveManager::UnregisterThreadPlugin( threadPluginOffset offset )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan );

    if ( !threadEnv )
    {
        return;
    }

    // Suspend thread activity for a short while.
    stacked_thread_activity_suspend suspend_activity( nativeMan );

    // Kill all other thread handles.
    PurgeActiveThreads();

    threadEnv->threadPlugins.UnregisterPlugin( offset );
}

CExecThread* CExecutiveManager::CreateThread( CExecThread::threadEntryPoint_t entryPoint, void *userdata, size_t stackSize, const char *hint_thread_name )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    // We must not create new threads if the environment is terminating!
    if ( nativeMan->isTerminating )
    {
        return nullptr;
    }

    // Get the general thread environment.
    privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan );

    if ( !threadEnv )
    {
        return nullptr;
    }

    // No point in creating threads if we have no native implementation.
    if ( privateNativeThreadEnvironmentRegister.get().IsRegistered() == false )
        return nullptr;

    CExecThread *threadInfo = nullptr;

    // Construct the thread.
    {
        // We are about to reference a new thread, so lock here.
        CUnfairMutexContext ctxThreadCreate( threadEnv->mtxThreadReferenceLock );

        // Make sure we synchronize access to plugin containers!
        // This only has to happen when the API has to be thread-safe.
        CUnfairMutexContext ctxThreadPlugins( threadEnv->mtxThreadPluginsLock );

        try
        {
            NatExecStandardObjectAllocator memAlloc( nativeMan );

            CExecThreadImpl *nativeThread = threadEnv->threadPlugins.ConstructArgs( memAlloc, nativeMan, false, hint_thread_name, userdata, stackSize, entryPoint );

            if ( nativeThread )
            {
                // Give a referenced handle to the runtime.
                nativeThread->refCount++;

                threadInfo = nativeThread;
            }
        }
        catch( ... )
        {
            // TODO: add an exception that can be thrown if the construction of threads failed.
            threadInfo = nullptr;
        }
    }

    return threadInfo;
}

void CExecutiveManager::TerminateThread( CExecThread *thread, bool waitOnRemote )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)thread;

    nativeThread->Terminate( waitOnRemote );
}

void CExecutiveManager::JoinThread( CExecThread *thread )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)thread;

    CExecutiveManagerNative *nativeMan = nativeThread->manager;

    nativeThreadPlugin *info = GetNativeThreadPlugin( nativeMan, nativeThread );

    if ( info )
    {
#ifdef __linux__
        nativeThreadPluginInterface *nativeThreadMan = &privateNativeThreadEnvironmentRegister.get().GetPluginStruct( nativeMan )->_nativePluginInterface;

        CEvent *eventRunning = nativeThreadMan->threadRunningEventRegister.GetEvent( nativeThread );

        // We should wait till the lock of the thread runtime is taken and left.
        eventRunning->Wait();
#elif defined(_WIN32)
        // Wait for completion of the thread.
        WaitForSingleObject( info->hThread, INFINITE );
#else
#error No thread join wait-for implementation
#endif //CROSS PLATFORM CODE

        // Had to be set by the thread itself.
        assert( info->status == THREAD_TERMINATED );
    }
}

bool CExecutiveManager::IsCurrentThread( CExecThread *thread ) const
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    CExecThreadImpl *nativeThread = (CExecThreadImpl*)thread;

    if ( nativeMan->isTerminating == false )
    {
        // Really simple check actually.
        privateNativeThreadEnvironment *nativeEnv = privateNativeThreadEnvironmentRegister.get().GetPluginStruct( nativeMan );

        if ( nativeEnv )
        {
            nativeThreadPlugin *nativeInfo = GetNativeThreadPlugin( nativeMan, nativeThread );

            return thread_id_fetch().is_current( nativeInfo );
        }
    }

    return false;
}

CExecThread* CExecutiveManager::GetCurrentThread( bool onlyIfCached )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    CExecThreadImpl *currentThread = nullptr;

    // Only allow retrieval if the environment is not terminating.
    if ( nativeMan->isTerminating == false )
    {
        // Get our native interface (if available).
        privateNativeThreadEnvironment *nativeEnv = privateNativeThreadEnvironmentRegister.get().GetPluginStruct( nativeMan );

        if ( nativeEnv )
        {
            thread_id_fetch helper;

            // If we have an accelerated TLS slot, try to get the handle from it.
            if ( nativeThreadPlugin *tlsInfo = nativeEnv->_nativePluginInterface.TlsGetCurrentThreadInfo() )
            {
                currentThread = tlsInfo->self;
            }
            else
            {
                CUnfairMutexContext ctxRunningThreadList( nativeEnv->_nativePluginInterface.mtxRunningThreadList );

                // Else we have to go the slow way by checking every running thread information in existance.
                LIST_FOREACH_BEGIN( nativeThreadPlugin, nativeEnv->_nativePluginInterface.runningThreads.root, node )
                    if ( helper.is_current( item ) )
                    {
                        currentThread = item->self;
                        break;
                    }
                LIST_FOREACH_END
            }

            // Are we even allowed to create new thread references for remote threads? (onlyIfCached is false if allowed)
            // This is useful to turn off if you are trying access thread variables but you know what default variables a remote thread would have.

            // If we have not found a thread handle representing this native thread, we should create one.
            // This creates a so called "remote thread" reference which points to a thread not spawned by NativeExecutive.
            if ( onlyIfCached == false && currentThread == nullptr &&
                 nativeEnv->_nativePluginInterface.isTerminating == false && nativeMan->isTerminating == false )
            {
                // Need to fetch the general thread environment.
                privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan );

                if ( threadEnv != nullptr )
                {
                    // Create the thread.
                    CExecThreadImpl *newThreadInfo = nullptr;
                    {
                        // Since we are about to create a new thread reference, we must lock.
                        // We can later think about how to optimize this.
                        CUnfairMutexContext ctxThreadCreate( threadEnv->mtxThreadReferenceLock );

                        CUnfairMutexContext ctxThreadPlugins( threadEnv->mtxThreadPluginsLock );

                        try
                        {
                            NatExecStandardObjectAllocator memAlloc( nativeMan );

                            newThreadInfo = threadEnv->threadPlugins.ConstructArgs(
                                memAlloc,
                                nativeMan,
                                true,   // is a remote thread.
                                "remote-thread",
                                nullptr, 0, nullptr
                            );
                        }
                        catch( ... )
                        {
                            newThreadInfo = nullptr;
                        }
                    }

                    if ( newThreadInfo )
                    {
                        bool successPluginCreation = false;

                        // Our plugin must have been successfully intialized to continue.
                        if ( nativeThreadPlugin *plugInfo = GetNativeThreadPlugin( nativeMan, newThreadInfo ) )
                        {
                            bool gotIdentificationSuccess = false;

#ifdef _WIN32
                            // Open another thread handle and put it into our native plugin.
                            HANDLE newHandle = nullptr;

                            BOOL successClone = DuplicateHandle(
                                GetCurrentProcess(), helper.hRunningThread,
                                GetCurrentProcess(), &newHandle,
                                0, FALSE, DUPLICATE_SAME_ACCESS
                            );

                            gotIdentificationSuccess = ( successClone == TRUE );
#elif defined(__linux__)
                            // Since thread ids are not reference counted on Linux, we do not care and simply succeed.
                            gotIdentificationSuccess = true;
#else
#error no thread remote handle fetch implementation
#endif //CROSS PLATFORM CODE

                            if ( gotIdentificationSuccess )
                            {
                                // Always remember the thread id.
                                plugInfo->codeThread = helper.get_current_id();

#ifdef _WIN32
                                // Put the new handle into our plugin structure.
                                plugInfo->hThread = newHandle;
#elif defined(__linux__)
                                // Nothing.
#else
#error no thread identification store implementation
#endif //CROSS PLATFORM CODE

                                // Set our plugin information into our Tls slot (if available).
                                nativeEnv->_nativePluginInterface.TlsSetCurrentThreadInfo( plugInfo );

                                // Return it.
                                currentThread = newThreadInfo;

                                successPluginCreation = true;
                            }
                        }

                        if ( successPluginCreation == false )
                        {
                            // Delete the thread object again.
                            CloseThread( newThreadInfo );
                        }
                    }
                }
            }
        }
    }

    return currentThread;
}

bool CExecutiveManager::IsRemoteThread( CExecThread *thread ) const noexcept
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    CExecThreadImpl *nativeThread = (CExecThreadImpl*)thread;

    if ( nativeThread->isLocallyRemoteThread )
    {
        return true;
    }

    if ( nativeThread->manager != nativeMan )
    {
        return true;
    }

    return false;
}

CExecThread* CExecutiveManager::AcquireThread( CExecThread *thread )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)thread;

    CSpinLockContext ctx_addRef( nativeThread->lock_refCountChange );

    // If the descriptor is closing then we must not do that.
    if ( nativeThread->isClosingDescriptor )
    {
        throw eir::internal_error_exception();
    }

    // Add a reference and return a new handle to the thread.

    // TODO: make sure that we do not overflow the refCount.

    unsigned long prevRefCount = nativeThread->refCount++;

    FATAL_ASSERT( prevRefCount != 0 );

    // We have a new handle.
    return thread;
}

// Cleanup interface management.
// Should be used by thread plugins for additional runtime reference cleaning.
void CExecutiveManager::RegisterThreadCleanup( cleanupInterface *handler )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan );

    if ( threadEnv )
    {
        // To prevent unnecessary locking we do not want to have threads created while you can assign
        // cleanups. If you still want such functionality then you should definately make your own
        // cleanup wrapper that allows this behaviour.
        assert( threadEnv->threadPlugins.GetNumberOfAliveClasses() == 0 );

        threadEnv->cleanups.AddToBack( handler );
    }
}

void CExecutiveManager::UnregisterThreadCleanup( cleanupInterface *handler ) noexcept
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan );

    if ( threadEnv )
    {
        // See the registration function for clarification on this assumption.
        assert( threadEnv->threadPlugins.GetNumberOfAliveClasses() == 0 );

        threadEnv->cleanups.RemoveByValue( handler );
    }
}

void CExecutiveManagerNative::CloseThreadNative( CExecThreadImpl *nativeThread, bool isEndOfLocalThread )
{
    // Get the general thread environment.
    privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( this );

    if ( !threadEnv )
        return;

    // Changing thread reference count is unsafe so we lock here.
    unsigned long prevRefCount;
    {
        CUnfairMutexContext ctxThreadClose_prepare( threadEnv->mtxThreadReferenceLock );
        CSpinLockContext ctx_delRef_prepare( nativeThread->lock_refCountChange );

        // If we are closing already then this is a very bad idea.
        if ( nativeThread->isClosingDescriptor )
        {
            throw eir::internal_error_exception();
        }

        // Check the reference count
        prevRefCount = nativeThread->refCount;

        if ( prevRefCount == 1 )
        {
            // We can skip notifying the runtime because we are the only ones that care.

            // We are entering clean-up phase so notify the runtime.
            // From here on out the reference count has turned immutable for everyone else.
            nativeThread->isClosingDescriptor = true;

            // If we are a remote thread then we absolutely must switch to terminating state over here.
            // Even if we are not actually terminatingthe thread stack, the thread descriptor itself surely
            // is being terminated thus it makes sense.
            // This prevents any hazard from registering themselves as well as correctness-checks for cleanup.
            if ( nativeThread->isLocallyRemoteThread )
            {
                NativeExecutive::nativeThreadPlugin *nativeInfo = GetNativeThreadPlugin( this, nativeThread );

                if ( nativeInfo )
                {
                    CUnfairMutexContext ctx_set_terminating( nativeThread->mtxThreadStatus );

                    nativeInfo->status = THREAD_TERMINATING;
                }
            }
        }
        else
        {
            // We can safely update the reference count here.
            nativeThread->refCount = ( prevRefCount - 1 );
        }

        if ( isEndOfLocalThread )
        {
            // Set our thread as terminated, since we have reached end-of-the-line.
            // Have to set state before notify the runtime of completion.
            NativeExecutive::nativeThreadPlugin *nativeInfo = GetNativeThreadPlugin( this, nativeThread );

            if ( nativeInfo )
            {
                nativeInfo->status = THREAD_TERMINATED;

#ifdef _DEBUG
                assert( nativeThread->isLocallyRemoteThread == false );
#endif //_DEBUG
            }
        }

        if ( prevRefCount > 1 )
        {
            // Are we supposed to notify the runtime about end-of-thread execution?
            // Since the runtime reference has been released we can safely do that here.
            if ( isEndOfLocalThread )
            {
#ifdef __linux__
                // Report end of runtime using the event.
                {
                    privateNativeThreadEnvironment *nativeEnv = privateNativeThreadEnvironmentRegister.get().GetPluginStruct( this );

                    CEvent *eventRunning = nativeEnv->_nativePluginInterface.threadRunningEventRegister.GetEvent( nativeThread );

                    eventRunning->Set( false );
                }
#endif //__linux__
            }
        }
    }

    if ( prevRefCount == 1 )
    {
        // Are there any cleanups to execute?
        // Note that we assume that we cannot assign any cleanups while threads are potentially created already.
        for ( cleanupInterface *cleanup : threadEnv->cleanups )
        {
            cleanup->OnCleanup( nativeThread );
        }
    }

    if ( isEndOfLocalThread )
    {
        // At this point we do not execute any more user code.
        // Thus it is the perfect time to get rid of runtime linkage by CRT, etc.

#ifdef __linux__
#ifdef NATEXEC_LINUX_THREAD_SELF_CLEANUP

        // Cleanup CRT dependencies.
        _threadsupp_glibc_leave_thread( nativeThread );

#ifndef NATEXEC_LINUX_TSC_USE_DUMMY_TLD
        // We should make sure that we are not using the TLS pointer anymore.
        _natexec_syscall_arch_prctl( ARCH_SET_FS, nullptr );
#endif //NATEXEC_LINUX_TSC_USE_DUMMY_TLD

#endif //NATEXEC_LINUX_THREAD_SELF_CLEANUP
#endif //__linux__
    }

    if ( prevRefCount == 1 )
    {
        // Enter last phase of cleanup.
        CUnfairMutexContext ctxThreadClose_destroy( threadEnv->mtxThreadReferenceLock );
        // don't have to take the spinlock anymore because the thread is definitely closing after the above section
        // and then we cannot change the count anymore anyway.

        // Update the reference count.
        nativeThread->refCount = 0;

        // Kill the thread.
        CUnfairMutexContext ctxThreadPlugins( threadEnv->mtxThreadPluginsLock );

        NatExecStandardObjectAllocator memAlloc( this );

        threadEnv->threadPlugins.Destroy( memAlloc, nativeThread );
    }
}

void CExecutiveManager::CloseThread( CExecThread *thread )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    CExecThreadImpl *nativeThread = (CExecThreadImpl*)thread;

    // RULES FOR USER RUNTIMES:
    // 1) calling this function more than more than the amount of references granted/acquired is illegal.
    // 2) calling this function for remote threads is illegal, if not obtained by GetCurrentThread or
    //    preceeded by a previous call of CloseThread on the same handle obtained by single invocation of
    //    GetCurrentThread (LEGACY SUPPORT).

    if ( nativeThread->refCount == 1 )
    {
        // Only allow this from the current thread if we are a remote thread.
        // This is actually not a supported use-case but we do it for compatibility's sake.
        if ( IsCurrentThread( nativeThread ) )
        {
            if ( !nativeMan->IsRemoteThread( nativeThread ) )
            {
                // TODO: handle this more gracefully.
                FATAL_ASSERT( 0 );
            }
        }
    }

    nativeMan->CloseThreadNative( nativeThread, false );
}

void CExecutiveManager::CleanupRemoteThread( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    // This function must be called to release NativeExecutive resources from a remote thread that is
    // currently running. After a successful call, the runtime notifies NativeExecutive that the thread
    // is not required anymore and can be cleaned-up at the earliest time possible. All associated
    // runtime data is forgotten, like lock-states and thread-local variables. You must not call this
    // function if the remote thread could still rely on thread-local data.
    // The only valid point to call this function is after any thread specific operation of the remote
    // thread has finished.
    // * calling cleanup too early does risk undefined behaviour due to lock contexts, etc.
    // * forgetting to call cleanup does risk memory leakage or even polluting new threads with resources
    //   from a long-dead thread (undefined behaviour).

    // NOTE: the initial thread is of course a remote thread, because it was not started by NativeExecutive.
    // Thus if the initial thread does exit but the program continues to run, then you have to clean-up the
    // resources of the initial thread, too! But many applications do destroy the NativeExecutive
    // manager as part of stack unwinding in the initial thread thus an explicit cleanup is often not required.

    CExecThreadImpl *currentThread = (CExecThreadImpl*)this->GetCurrentThread( true );

    if ( currentThread->isLocallyRemoteThread == false )
    {
        return;
    }

    nativeMan->CloseThreadNative( currentThread, false );
}

static eir::Vector <CExecThreadImpl*, NatExecStandardObjectAllocator> get_active_threads( CExecutiveManagerNative *nativeMan )
{
    eir::Vector <CExecThreadImpl*, NatExecStandardObjectAllocator> threadList( nullptr, 0, nativeMan );

    privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan );

    if ( threadEnv )
    {
        // We need a hard lock on global all-thread status change here.
        // No threads can be added or closed if we hold this lock.
        CUnfairMutexContext ctxThreadPurge( threadEnv->mtxThreadReferenceLock );

        LIST_FOREACH_BEGIN( CExecThreadImpl, nativeMan->threads.root, managerNode )

            // Ignore any threads that are closing; we must not reference them anymore anyway.
            if ( item->isClosingDescriptor == false )
            {
                CExecThreadImpl *thread = (CExecThreadImpl*)nativeMan->AcquireThread( item );

                if ( thread )
                {
                    threadList.AddToBack( thread );
                }
            }

        LIST_FOREACH_END
    }

    return threadList;
}

// You must not be using any threads anymore when calling this function because
// it cleans up their references.
void CExecutiveManager::PurgeActiveThreads( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    privateNativeThreadEnvironment *natThreadEnv = privateNativeThreadEnvironmentRegister.get().GetPluginStruct( nativeMan );

    if ( natThreadEnv == nullptr )
    {
        return;
    }

    auto threadList = get_active_threads( nativeMan );

    // Destroy all the threads.
    threadList.Walk(
        [&]( size_t idx, CExecThreadImpl *thread )
    {
        // Is it our thread?
        if ( thread->isLocallyRemoteThread == false )
        {
            // Make sure to signal termination.
            thread->Terminate( false );

            // Wait till the thread has absolutely finished running by joining it.
            this->JoinThread( thread );
        }

        // JoinThread ensures that runtime reference is released. Thus we do not need to lock here anymore.

        unsigned long refsToRelease = ( thread->refCount - 1 );

        for ( unsigned long n = 0; n < refsToRelease; n++ )
        {
            CloseThread( thread );
        }

        // Remove our own reference aswell.
        CloseThread( thread );

        // We could performance-improve this process in the future.
    });
}

unsigned int CExecutiveManager::GetParallelCapability( void ) const
{
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo( &sysInfo );

    return sysInfo.dwNumberOfProcessors;
#elif defined(__linux__)
    int num_processors = sysconf(_SC_NPROCESSORS_CONF);

    assert( num_processors > 0 );

    return (unsigned int)num_processors;
#else
    // TODO: add support for more systems.
    // (1 because we always have at least one processor)
    return 1;
#endif
}

void CExecutiveManager::CheckHazardCondition( void )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)GetCurrentThread( true );

    // There is no hazard if NativeExecutive is terminating.
    // Since remote threads cannot be cleanly terminated by us we do not care about them.
    if ( nativeThread == nullptr )
    {
        return;
    }

    nativeThread->CheckTerminationRequest();
}

void registerThreadPlugin( void )
{
    // Register the events that are required for the mutexes.
    _runningThreadListEventRegister.Construct( executiveManagerFactory );
    _tlsThreadToNativeInfoLockEventRegister.Construct( executiveManagerFactory );

#ifdef __linux__

    _threadsToTermLockEventRegister.Construct( executiveManagerFactory );
    _threadsToTermSemEventRegister.Construct( executiveManagerFactory );

#endif //__linux__

    // Register shared events.
    privateThreadEnvThreadReferenceLockEventRegister.Construct( executiveManagerFactory );
    privateThreadEnvThreadPluginsLockEventRegister.Construct( executiveManagerFactory );

    // Register the general thread environment and ...
    privateThreadEnv.Construct( executiveManagerFactory );

    // Register any CRT threading support dependencies.
    // Do note that thread plugins can be registered in these.
#ifdef __linux__
    _threadsupp_glibc_register();
#endif //__linux__

    // ... the native thread environment.
    privateNativeThreadEnvironmentRegister.Construct( executiveManagerFactory );
}

void unregisterThreadPlugin( void )
{
    // Must unregister plugins in-order.
    privateNativeThreadEnvironmentRegister.Destroy();

    // Now unregister any CRT threading support stuff.
#ifdef __linux__
    _threadsupp_glibc_unregister();
#endif //__linux__

    privateThreadEnv.Destroy();

    // Unregister shared stuff.
    privateThreadEnvThreadPluginsLockEventRegister.Destroy();
    privateThreadEnvThreadReferenceLockEventRegister.Destroy();

    // Unregister the events.
#ifdef __linux__

    _threadsToTermSemEventRegister.Destroy();
    _threadsToTermLockEventRegister.Destroy();

#endif //__linux__

    _tlsThreadToNativeInfoLockEventRegister.Destroy();
    _runningThreadListEventRegister.Destroy();
}

END_NATIVE_EXECUTIVE
