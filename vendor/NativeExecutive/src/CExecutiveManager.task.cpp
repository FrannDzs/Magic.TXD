/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.task.cpp
*  PURPOSE:     Runtime for quick parallel execution sheduling
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "internal/CExecutiveManager.thread.internal.h"
#include "internal/CExecutiveManager.task.internal.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif //_WIN32

BEGIN_NATIVE_EXECUTIVE

struct shedulerThreadItem
{
    CExecTaskImpl *task;
};

struct taskSystemEnv
{
    inline taskSystemEnv( CExecutiveManagerNative *natExec ) : sheduledItems( natExec )
    {
        return;
    }

    inline void Initialize( CExecutiveManagerNative *natExec )
    {
        // Initialize synchronization objects.
        sheduledItems.Initialize( natExec, 256 );

        this->condHasItem = natExec->CreateConditionVariable();

        assert( this->condHasItem != nullptr );

        this->shedulerThread = nullptr;
    }

    inline void Shutdown( CExecutiveManagerNative *natExec )
    {
        if ( shedulerThread )
        {
            // Shutdown synchronization objects.
            natExec->TerminateThread( shedulerThread );

            natExec->CloseThread( shedulerThread );

            shedulerThread = nullptr;
        }

        natExec->CloseConditionVariable( this->condHasItem );

        sheduledItems.Shutdown( natExec );
    }

    void bootUpThread( CExecutiveManagerNative *natExec );

    // Thread to run tasks on.
    CExecThread *shedulerThread;

    // Event to wait for sheduler items.
    CCondVar *condHasItem;

    // Maintenance variables for hyper-sheduling.
    SynchronizedHyperQueue <shedulerThreadItem> sheduledItems;

    // Since no objects can be created during initialization we have to delay the
    // till later.
};

static constinit optional_struct_space <PluginDependantStructRegister <taskSystemEnv, executiveManagerFactory_t>> taskSystemEnvRegister;

// Potentially unsafe routine, but should be hyper-fast.
static AINLINE bool ProcessSheduleItem( taskSystemEnv *taskEnv )
{
    shedulerThreadItem currentSheduling;

    bool hasSheduledItem = taskEnv->sheduledItems.GetSheduledItemPreLock( currentSheduling );

    if ( hasSheduledItem )
    {
        // Run the task till it yields.
        CExecTaskImpl *theTask = currentSheduling.task;

        theTask->runtimeFiber->resume();

        // We finished one iteration of this task.
        theTask->usageCount--;
    }

    return hasSheduledItem;
}

static void TaskShedulerThread( CExecThread *threadInfo, void *param )
{
    CExecThreadImpl *nativeThread = (CExecThreadImpl*)threadInfo;

    CExecutiveManagerNative *natExec = nativeThread->manager;

    taskSystemEnv *taskEnv = taskSystemEnvRegister.get().GetPluginStruct( natExec );

    CReadWriteWriteContextSafe <> ctxProcessQueueItem( taskEnv->sheduledItems.GetLock() );

    try
    {
        while ( true )
        {
            taskEnv->condHasItem->Wait( ctxProcessQueueItem );

            while ( ProcessSheduleItem( taskEnv ) );
        }
    }
    catch( ... )
    {
        taskEnv->shedulerThread = nullptr;

        throw;
    }

    // Clean up.
    taskEnv->shedulerThread = nullptr;
}

// TODO: think about making the task system object-based, so you can have
// multiple task systems under one threading manager.

struct execWrapStruct
{
    CExecTaskImpl *theTask;
    void *userdata;
};

static void TaskFiberWrap( CFiber *theFiber, void *memPtr )
{
    // Initialize the runtime, as the pointer is only temporary.
    CExecTaskImpl *theTask = nullptr;
    void *userdata = nullptr;

    {
        execWrapStruct& theInfo = *(execWrapStruct*)memPtr;

        theTask = theInfo.theTask;
        userdata = theInfo.userdata;
    }

    // Wait for the next sheduling. We are set up by now.
    theFiber->yield();

    theTask->callback( theTask, userdata );
}

CExecTaskImpl::CExecTaskImpl( CExecutiveManagerNative *manager, CFiber *runtime )
{
    this->runtimeFiber = runtime;

    this->manager = manager;

    // Event that is signaled when the task finished execution.
#ifdef _WIN32
    this->finishEvent = CreateEventW( nullptr, true, true, nullptr );
#endif //_WIN32
    this->isInitialized = false;
    this->isOnProcessedList = false;
    this->usageCount = 0;
}

CExecTaskImpl::~CExecTaskImpl( void )
{
#ifdef _WIN32
    // Clean up resources.
    CloseHandle( finishEvent );
#endif //_WIN32
}

CExecTask* CExecutiveManager::CreateTask( CExecTask::taskexec_t callback, void *userdata, size_t stackSize )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    // Create the underlying fiber.
    execWrapStruct info;
    info.theTask = nullptr;
    info.userdata = userdata;

    CFiber *theFiber = CreateFiber( TaskFiberWrap, &info, stackSize );

    if ( !theFiber )
        return nullptr;

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    CExecTaskImpl *task = eir::dyn_new_struct <CExecTaskImpl> ( memAlloc, nullptr, nativeMan, theFiber );

    // Set up runtime task members.
    task->callback = callback;

    // Finalize the information to pass to the fiber.
    info.theTask = task;

    // Make a first pulse into the task so it can set itself up.
    theFiber->resume();

    // The task is ready to be used.
    return task;
}

void CExecutiveManager::CloseTask( CExecTask *task )
{
    CExecTaskImpl *nativeTask = (CExecTaskImpl*)task;

    // We must wait for the task to finish.
    nativeTask->WaitForFinish();

    CloseFiber( nativeTask->runtimeFiber );

    NatExecStandardObjectAllocator memAlloc( this );

    eir::dyn_del_struct <CExecTaskImpl> ( memAlloc, nullptr, nativeTask );
}

void taskSystemEnv::bootUpThread( CExecutiveManagerNative *natExec )
{
    if ( shedulerThread == nullptr )
    {
        shedulerThread = natExec->CreateThread( TaskShedulerThread, nullptr );

        if ( shedulerThread )
        {
            // Start the thread.
            shedulerThread->Resume();
        }
    }
}

void CExecTask::Execute( void )
{
    CExecTaskImpl *nativeTask = (CExecTaskImpl*)this;

    CExecutiveManagerNative *natExec = nativeTask->manager;

    taskSystemEnv *taskEnv = taskSystemEnvRegister.get().GetPluginStruct( natExec );

    if ( !taskEnv )
        return;

    // Make sure the thread is kicked up.
    taskEnv->bootUpThread( natExec );

    // Make sure the task is initialized.
    if ( !nativeTask->isInitialized )
    {
        // Ping the task for initialization on the main thread.
        nativeTask->runtimeFiber->resume();

        nativeTask->isInitialized = true;
    }

    // Make sure the thread is not marked as finished.
    nativeTask->usageCount++;

    // Use a circling queue for this as found in the R* streaming runtime.
    shedulerThreadItem theItem;
    theItem.task = nativeTask;

    taskEnv->sheduledItems.AddItem( theItem );

    // Notify the sheduler that there are items to process.
    taskEnv->condHasItem->Signal();
}

bool CExecTask::IsFinished( void ) const
{
    CExecTaskImpl *nativeTask = (CExecTaskImpl*)this;

    return ( nativeTask->usageCount == 0 );
}

void CExecTask::WaitForFinish( void )
{
    CExecTaskImpl *nativeTask = (CExecTaskImpl*)this;

    CFiber *theFiber = nativeTask->runtimeFiber;

    if ( !theFiber->is_terminated() )
    {
        // Spin lock until the task has finished.
        while ( !IsFinished() );
#if 0
        {
            ProcessSheduleItem();
        }
#endif
    }
}

void registerTaskPlugin( void )
{
    taskSystemEnvRegister.Construct( executiveManagerFactory );
}

void unregisterTaskPlugin( void )
{
    taskSystemEnvRegister.Destroy();
}

END_NATIVE_EXECUTIVE
