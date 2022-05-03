/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.internal.h
*  PURPOSE:     Main internal include of the NativeExecutive framework
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATIVE_EXECUTIVE_MANAGER_INTERNAL_
#define _NATIVE_EXECUTIVE_MANAGER_INTERNAL_

// Main header.
#include "CExecutiveManager.h"

// Very important modules.
#include "CExecutiveManager.rwlock.internal.h"

// Need-to-know things.
#include "CExecutiveManager.thread.internal.h"
#include "CExecutiveManager.fiber.internal.h"

// For the allocator templates.
#include <sdk/MetaHelpers.h>

// For the optional_struct_space helper.
#include <sdk/eirutils.h>

BEGIN_NATIVE_EXECUTIVE

struct CExecutiveGroupImpl;
struct CFiberImpl;

class CExecutiveManagerNative : public CExecutiveManager
{
public:
    // We do not want the runtime to create us.
    // Instead we expose a factory that has to create us.

    CExecutiveManagerNative( void );
    ~CExecutiveManagerNative( void );

    void            PushFiber           ( CFiberImpl *fiber );
    void            PopFiber            ( void );

    static void     NativeFiberTerm     ( FiberStatus *status );

    void            CloseThreadNative   ( CExecThreadImpl *thread, bool isLocalThreadEnd = false );

    // This is the native representation of the executive manager.
    // Every alive type of CExecutiveManager should be of this type.

    // Memory management definitions.
    // Those either point outside the library or to an internal plugin that is used (usually associated with NativeHeapAllocator).
    MemoryInterface *memoryIntf;
    // There is a CReadWriteLock attached as plugin to this structure that acts as memory lock.

    bool isTerminating;         // if true then no new objects are allowed to spawn anymore.
    bool isRuntimeTerminating;  // if true allow no more new fibers and executive groups.
    bool isFullyInitialized;    // if true then all systems can run to full extent, like memory.

    RwList <CExecThreadImpl> threads;
    RwList <CFiberImpl> fibers;
    RwList <CExecTask> tasks;
    RwList <CExecutiveGroupImpl> groups;

    double frameTime;
    double frameDuration;

    double GetFrameDuration( void )
    {
        return frameDuration;
    }
};

// Allocator used for static things.
DEFINE_HEAP_ALLOC( nativeExecutiveAllocator );

typedef StaticPluginClassFactory <CExecutiveManagerNative, nativeExecutiveAllocator> executiveManagerFactory_t;

extern constinit optional_struct_space <executiveManagerFactory_t> executiveManagerFactory;

// Maximum size of implementation synchronization objects for storage in static contexts.
static constexpr size_t MAX_STATIC_SYNC_STRUCT_SIZE = ( sizeof(void*) * 4 );

END_NATIVE_EXECUTIVE

#endif //_NATIVE_EXECUTIVE_MANAGER_INTERNAL_
