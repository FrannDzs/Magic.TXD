/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.h
*  PURPOSE:     Thread abstraction layer for MTA
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_THREADS_
#define _EXECUTIVE_MANAGER_THREADS_

BEGIN_NATIVE_EXECUTIVE

enum eThreadStatus
{
    THREAD_SUSPENDED,       // either initial status or stopped by user-mode Suspend()
    THREAD_RUNNING,         // active on the OS sheduler
    THREAD_TERMINATING,     // active on the OS sheduler AND seeking closest path to termination
    THREAD_TERMINATED       // halted.
};

class CExecutiveManager;

class CExecThread abstract
{
protected:
    ~CExecThread( void ) = default;

public:
    typedef void (*threadEntryPoint_t)( CExecThread *thisThread, void *userdata );

    CExecutiveManager* GetManager( void );

    eThreadStatus GetStatus( void ) const;

    // Ask a thread to shut down, gracefully if possible.
    bool Terminate( bool waitOnRemote = true );

    // Ask a thread to cancel or remove it's cancel state.
    // Can also be applied to remote threads.
    void SetThreadCancelling( bool enableCancel );
    bool IsThreadCancelling( void ) const;

    bool Suspend( void );
    bool Resume( void );

    // Returns true if the running native OS thread is identified with this thread object.
    bool IsCurrent( void );

    // Returns the fiber that is currently running on this thread.
    // If there are multiple fibers nested then the top-most is returned.
    CFiber* GetCurrentFiber( void );
    bool IsFiberRunningHere( CFiber *fiber );

    // Plugin API.
    void* ResolvePluginMemory( threadPluginOffset offset );
    const void* ResolvePluginMemory( threadPluginOffset offset ) const;

    static bool IsPluginOffsetValid( threadPluginOffset offset ) noexcept;
    static threadPluginOffset GetInvalidPluginOffset( void ) noexcept;
};

// Cleanup-interface executed on thread destruction.
struct cleanupInterface
{
    // Called when the thread handle has been completely terminated.
    // This callback should make sure that all references to the thread are removed.
    // After cleanup phase no runtime component other than NativeExecutive may ever
    // reference this thread instance again; new thread instances have to be created
    // instead. It is very important to remove any dangling pointers. Removing them
    // in plugin destruction is illegal because then the thread has been partially
    // destroyed already (any NativeExecutive API call may result in undefined
    // behaviour/crashes).
    virtual void OnCleanup( CExecThread *thread ) noexcept = 0;
};

// Thread activity registration for creation and clearing of threads upon plugin registration.
struct threadActivityInterface
{
    virtual void InitializeActivity( CExecutiveManager *execMan ) = 0;
    virtual void ShutdownActivity( CExecutiveManager *execMan ) noexcept = 0;
};

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_THREADS_
