/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.cpp
*  PURPOSE:     MTA thread and fiber execution manager for workload smoothing
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include <chrono>

#include "internal/CExecutiveManager.execgroup.internal.h"
#include "internal/CExecutiveManager.thread.internal.h"
#include "internal/CExecutiveManager.fiber.internal.h"

#include "CExecutiveManager.thread.hxx"
#include "CExecutiveManager.fiber.hxx"
#include "CExecutiveManager.hazards.hxx"

#include "CExecutiveManager.memory.hxx"

BEGIN_NATIVE_EXECUTIVE

void CExecutiveManagerNative::NativeFiberTerm( FiberStatus *status )
{
    CFiberImpl *nativeFiber = (CFiberImpl*)status;

    Fiber *env = nativeFiber->runtime;

    if ( !env )
        return;

    CExecutiveManagerNative *nativeMan = nativeFiber->manager;

    // We are not running anymore, so remove our fiber from the fiber stack.
    nativeMan->PopFiber();

    // Execute cleanup of hazards.
    {
        auto *hazardEnv = executiveHazardManagerEnvRegister.get().GetPluginStruct( nativeMan );

        if ( hazardEnv )
        {
            hazardEnv->PurgeFiberHazards( nativeFiber );
        }
    }

	// Fibers clean their environments after themselves
    {
        nativeMan->MemFree( ExecutiveFiber::getstackspace( env ) );
        nativeMan->MemFree( env );
    }

	nativeFiber->runtime = nullptr;
}

static void FIBER_CALLCONV _FiberProc( FiberStatus *status )
{
    CFiberImpl *fiber = (CFiberImpl*)status;

    try
    {
        fiber->yield();

        fiber->callback( fiber, fiber->userdata );
    }
    catch( fiberTerminationException& except )
    {
        // The runtime requested the fiber to terminate.
        // We must only break if it indeed is our fiber tho.
        CFiber *except_fib = except.fiber;

        if ( except_fib != fiber )
        {
            // Just rethrow it, after we yielded.
            fiber->has_term_unhandled_exception = true;
#ifdef NATEXEC_EXCEPTION_COPYPUSH
            // Store our exception.
            fiber->term_unhandled_exception = std::current_exception();
#else
            fiber->unhandled_exception_type = CFiberImpl::eUnhandledExceptionType::FIBERTERM;
            fiber->unhandled_exception_args.fiber = except_fib;
#endif //NATEXEC_EXCEPTION_COPYPUSH
        }

        // Since we execute fibers on the stack, possibly recursively, this should work.
    }
    catch( ... )
    {
        // An unexpected error was triggered.
        // We should pass it on to the runtime.
        fiber->has_term_unhandled_exception = true;
#ifdef NATEXEC_EXCEPTION_COPYPUSH
        fiber->term_unhandled_exception = std::current_exception();
#else
        fiber->unhandled_exception_type = CFiberImpl::eUnhandledExceptionType::UNKNOWN;
#endif //NATEXEC_EXCEPTION_COPYPUSH
    }
}

// Factory API.
constinit optional_struct_space <executiveManagerFactory_t> executiveManagerFactory;

// Memory allocator for static data, because it actually is required.
static constinit optional_struct_space <NatExecHeapAllocator> _staticHeapAlloc;

void* nativeExecutiveAllocator::Allocate( void *refPtr, size_t memSize, size_t alignment ) noexcept
{
    return _staticHeapAlloc.get().Allocate( memSize, alignment );
}

bool nativeExecutiveAllocator::Resize( void *refPtr, void *memPtr, size_t reqSize ) noexcept
{
    return _staticHeapAlloc.get().SetAllocationSize( memPtr, reqSize );
}

void nativeExecutiveAllocator::Free( void *refPtr, void *memPtr ) noexcept
{
    _staticHeapAlloc.get().Free( memPtr );
}

// Make sure we register all plugins.
extern void registerReadWriteLockEnvironment( void );
extern void registerEventManagement( void );
extern void registerMemoryManager( void );
extern void registerThreadPlugin( void );
extern void registerFiberPlugin( void );
extern void registerStackHazardManagement( void );
extern void registerBarriers( void );
extern void registerConditionalVariables( void );
extern void registerTaskPlugin( void );
extern void registerEventWaiterEnvironment( void );
extern void registerReadWriteLockPTD( void );
extern void registerReentrantReadWriteLockEnvironment( void );
extern void registerThreadActivityEnvironment( void );

extern void unregisterReadWriteLockEnvironment( void );
extern void unregisterEventManagement( void );
extern void unregisterMemoryManager( void );
extern void unregisterThreadPlugin( void );
extern void unregisterFiberPlugin( void );
extern void unregisterStackHazardManagement( void );
extern void unregisterBarriers( void );
extern void unregisterConditionalVariables( void );
extern void unregisterTaskPlugin( void );
extern void unregisterEventWaiterEnvironment( void );
extern void unregisterReadWriteLockPTD( void );
extern void unregisterReentrantReadWriteLockEnvironment( void );
extern void unregisterThreadActivityEnvironment( void );

// Not really thread-safe yet but ok for now.
// Would require a wait for the count to be 1, with a lock.
static unsigned int _initRefCount( 0 );

void reference_library( void )
{
    if ( _initRefCount++ == 0 )
    {
        // First we need our allocator.
        _staticHeapAlloc.Construct();

        // Construct all important factories.
        executiveManagerFactory.Construct();

        // TODO: since rwlock env does use the thread environment, we kind of have to solve a dependency error here.

        // Register all plugins.
        registerReadWriteLockEnvironment();
        registerEventManagement();
        registerMemoryManager();
        registerThreadPlugin();
        registerFiberPlugin();
        registerStackHazardManagement();
        registerBarriers();
        registerConditionalVariables();
        registerTaskPlugin();
        registerEventWaiterEnvironment();
        registerReadWriteLockPTD();
        registerReentrantReadWriteLockEnvironment();
        registerThreadActivityEnvironment();
    }
}

void dereference_library( void )
{
    if ( --_initRefCount == 0 )
    {
        // Unregister all plugins.
        unregisterThreadActivityEnvironment();
        unregisterReentrantReadWriteLockEnvironment();
        unregisterReadWriteLockPTD();
        unregisterEventWaiterEnvironment();
        unregisterTaskPlugin();
        unregisterConditionalVariables();
        unregisterBarriers();
        unregisterStackHazardManagement();
        unregisterThreadPlugin();
        unregisterFiberPlugin();
        unregisterMemoryManager();
        unregisterEventManagement();
        unregisterReadWriteLockEnvironment();

        // Destroy all important factories.
        executiveManagerFactory.Destroy();

#ifdef _DEBUG
        {
            // Kinda have to make sure we use memory properly.
            // Since we get rid of the memory anyway, this is just for helper.
            NatExecHeapAllocator& heapAlloc = _staticHeapAlloc.get();

            NatExecHeapAllocator::heapStats stats = heapAlloc.GetStatistics();

            assert( stats.usedBytes == 0 );
        }
#endif //_DEBUG

        // Now get rid of all the remaining memory.
        _staticHeapAlloc.Destroy();
    }
}

CExecutiveManager* CExecutiveManager::Create( void )
{
    reference_library();

    nativeExecutiveAllocator memAlloc;

    CExecutiveManagerNative *resultObj = executiveManagerFactory.get().Construct( memAlloc );

    if ( resultObj == nullptr )
    {
        dereference_library();
    }
    else
    {
        // We have successfully initialized.
        resultObj->isFullyInitialized = true;
    }

    return resultObj;
}

void CExecutiveManager::Delete( CExecutiveManager *manager )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)manager;

    // Mark the runtime objects as terminating.
    // If it has not been marked already of course.
    nativeMan->isRuntimeTerminating = true;

    // First we terminate all fibers and executive groups.
    nativeMan->PurgeActiveRuntimes();

    // Mark this module so that is spawns no more new objects.
    nativeMan->isTerminating = true;

    // Kill all remaining handles.
    // This is required to properly shut ourselves down.
    nativeMan->PurgeActiveThreads();

    // We are not fully initialized anymore, because we quit.
    nativeMan->isFullyInitialized = false;

    nativeExecutiveAllocator memAlloc;

    executiveManagerFactory.get().Destroy( memAlloc, nativeMan );

    // Shutdown our reference.
    dereference_library();
}

CExecutiveManagerNative::CExecutiveManagerNative( void )
{
    this->isTerminating = false;            // we are ready to accept connections!
    this->isRuntimeTerminating = false;
    this->isFullyInitialized = false;       // must wait till thread plugins are completely started up, etc.

    // For now we have no memory allocator.
    this->memoryIntf = nullptr;

    // Initialization things belong into plugins.
}

CExecutiveManagerNative::~CExecutiveManagerNative( void )
{
    // Make sure we host no more objects.
    assert( LIST_EMPTY( this->groups.root ) == true );
    assert( LIST_EMPTY( this->fibers.root ) == true );
    assert( LIST_EMPTY( this->threads.root ) == true );
}

void CExecutiveManager::PurgeActiveRuntimes( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    // Calling this function makes most sense if the environment is terminating.

    // Destroy all groups.
    while ( !LIST_EMPTY( nativeMan->groups.root ) )
    {
        CExecutiveGroupImpl *group = LIST_GETITEM( CExecutiveGroupImpl, nativeMan->groups.root.next, managerNode );

        CloseGroup( group );
    }

    // Destroy all executive runtimes.
    while ( !LIST_EMPTY( nativeMan->fibers.root ) )
    {
        CFiberImpl *fiber = LIST_GETITEM( CFiberImpl, nativeMan->fibers.root.next, node );

        CloseFiber( fiber );
    }
}

void CExecutiveManager::MarkAsTerminating( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    // If we are terminating then no more threading objects are allowed to be created.
    // Should be made if you cleanly want to destroy thread plugins and stuff.
    nativeMan->isRuntimeTerminating = true;
}

CFiber* CExecutiveManager::CreateFiber( CFiber::fiberexec_t proc, void *userdata, size_t stackSize )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    if ( nativeMan->isTerminating || nativeMan->isRuntimeTerminating )
    {
        return nullptr;
    }

    // Get the fiber environment.
    privateFiberEnvironment *fiberEnv = privateFiberEnvironmentRegister.get().GetPluginStruct( (CExecutiveManagerNative*)this );

    if ( !fiberEnv )
    {
        return nullptr;
    }

    NatExecStandardObjectAllocator objMemAlloc( nativeMan );

    CFiberImpl *fiber = fiberEnv->fiberFact.ConstructArgs( objMemAlloc, nativeMan, nullptr, nullptr );

    if ( !fiber )
    {
        return nullptr;
    }

    // Make sure we have an appropriate stack size.
    if ( stackSize != 0 )
    {
        // At least two hundred bytes.
        stackSize = std::max( (size_t)200, stackSize );
    }
    else
    {
        stackSize = ( 1 << 17 );
    }

    // Try to allocate the required memory.
    void *stackMem = nativeMan->MemAlloc( stackSize, ExecutiveFiber::STACK_ALIGNMENT );

    if ( stackMem )
    {
        void *fiberMem = nativeMan->MemAlloc( sizeof(Fiber), alignof(Fiber) );

        if ( fiberMem )
        {
            Fiber *runtime = ExecutiveFiber::newfiber( fiber, fiberMem, stackMem, stackSize, _FiberProc, CExecutiveManagerNative::NativeFiberTerm );

            // Set first step into it, so the fiber can set itself up.
            fiber->runtime = runtime;
            fiber->resume();

            // Set the function that should be executed next cycle.
            fiber->callback = proc;
            fiber->userdata = userdata;

            // Add it to our manager list.
            LIST_APPEND( nativeMan->fibers.root, fiber->node );

            // Add it to the default fiber group.
            // The user can add it to another if he wants.
            fiberEnv->defGroup->AddFiberNative( fiber );

            // Return it.
            return fiber;
        }

        nativeMan->MemFree( stackMem );
    }

    // Remove the fiber again.
    fiberEnv->fiberFact.Destroy( objMemAlloc, fiber );

    return nullptr;
}

void CExecutiveManager::TerminateFiber( CFiber *fiber )
{
    CFiberImpl *nativeFiber = (CFiberImpl*)fiber;

    eFiberStatus fiberStatus = nativeFiber->status;

    // If we are terminating already, then we could probably mess up a lot of shit
    // when we would throw another exception.
    if ( fiberStatus == FIBER_TERMINATED || fiberStatus == FIBER_TERMINATING )
    {
        return;
    }

    CExecThread *curThread = this->GetCurrentThread( true );

    // If it does not exist then there is no fiber to remove, rite?
    // If the current remote thread has no thread descriptor then there cannot be any fiber executing on it.
    if ( curThread == nullptr )
    {
        return;
    }

    // If we are the fiber that is currently running, we just have to throw an termination exception.
    if ( curThread->IsFiberRunningHere( nativeFiber ) )
    {
        // Need to set it to terminating state.
        // This is a new state that is helping user-friendlyness (as well as nice cross-fiber exceptions support).
        nativeFiber->status = FIBER_TERMINATING;

        throw fiberTerminationException( fiber );
    }

    // If we are running, then this call is an API violation because we must not terminate fibers across threads.
    if ( fiberStatus == FIBER_RUNNING )
    {
        assert( 0 );
        return;
    }

    // If we are suspended, then it must mean that we are a remote fiber waiting to be terminated.
    // In that case just continue here.
    if ( fiberStatus == FIBER_SUSPENDED )
    {
        nativeFiber->status = FIBER_TERMINATING;

        nativeFiber->terminate_impl();
    }
}

void CExecutiveManager::CloseFiber( CFiber *fiber )
{
    CFiberImpl *nativeFiber = (CFiberImpl*)fiber;

    // Get the fiber environment.
    privateFiberEnvironment *fiberEnv = privateFiberEnvironmentRegister.get().GetPluginStruct( (CExecutiveManagerNative*)this );

    if ( !fiberEnv )
        return;

    TerminateFiber( fiber );

    LIST_REMOVE( nativeFiber->node );
    LIST_REMOVE( nativeFiber->groupNode );

    NatExecStandardObjectAllocator memAlloc( this );

    fiberEnv->fiberFact.Destroy( memAlloc, nativeFiber );
}

CExecutiveGroup* CExecutiveManager::CreateGroup( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    if ( nativeMan->isTerminating || nativeMan->isRuntimeTerminating )
    {
        return nullptr;
    }

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    CExecutiveGroup *group = eir::dyn_new_struct <CExecutiveGroupImpl> ( memAlloc, nullptr, nativeMan );

    return group;
}

void CExecutiveManager::CloseGroup( CExecutiveGroup *group )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    CExecutiveGroupImpl *nativeGroup = (CExecutiveGroupImpl*)group;

    eir::dyn_del_struct <CExecutiveGroupImpl> ( memAlloc, nullptr, nativeGroup );
}

void CExecutiveManager::DoPulse( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    // Update performance stats.
    {
#ifdef _WIN32
        HighPrecisionMathWrap mathWrap;
#endif //_WIN32

        // Update frame timer.
        double timeNow = ExecutiveManager::GetPerformanceTimer();

        nativeMan->frameDuration = timeNow - nativeMan->frameTime;
        nativeMan->frameTime = timeNow;
    }

    // Update all groups so they use new performance stats.
    LIST_FOREACH_BEGIN( CExecutiveGroupImpl, nativeMan->groups.root, managerNode )
        item->DoPulse();
    LIST_FOREACH_END
}

double ExecutiveManager::GetPerformanceTimer( void )
{
    auto now_time = std::chrono::high_resolution_clock::now();

    return std::chrono::duration <double, std::chrono::seconds::period> ( now_time.time_since_epoch() ).count();

    // TODO: test this.
#if 0
    LONGLONG counterFrequency, currentCount;

    QueryPerformanceFrequency( (LARGE_INTEGER*)&counterFrequency );
    QueryPerformanceCounter( (LARGE_INTEGER*)&currentCount );

    return (long double)currentCount / (long double)counterFrequency;
#endif
}

END_NATIVE_EXECUTIVE
