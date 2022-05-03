/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.stats.cpp
*  PURPOSE:     Statistics API for the library
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

// This file serves the purpose to give good insight about memory usage of various
// NativeExecutive components. It is important because this library ought to be used
// in critical environments where used-memory should be held minimal.

#include "StdInc.h"

#include "CExecutiveManager.thread.hxx"
#include "CExecutiveManager.fiber.hxx"

BEGIN_NATIVE_EXECUTIVE

void _executive_manager_get_internal_mem_quota( CExecutiveManagerNative *nativeMan, size_t& usedBytesOut, size_t& metaBytesOut );

executiveStatistics CExecutiveManager::CollectStatistics( void )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)this;

    // Get necessary plugins.
    privateThreadEnvironment *threadEnv = privateThreadEnv.get().GetPluginStruct( nativeMan );

    assert( threadEnv != nullptr );

    privateFiberEnvironment *fiberEnv = privateFiberEnvironmentRegister.get().GetPluginStruct( nativeMan );

    assert( fiberEnv != nullptr );

    executiveStatistics stats;

    // Memory quotas.
    _executive_manager_get_internal_mem_quota( nativeMan, stats.realOverallMemoryUsage, stats.metaOverallMemoryUsage );

    // Object counts.
    stats.numThreadHandles = threadEnv->threadPlugins.GetNumberOfAliveClasses();
    stats.numFibers = fiberEnv->fiberFact.GetNumberOfAliveClasses();
    
    // Set plugin stuff.
    stats.structSizeManager = executiveManagerFactory.get().GetClassSize();
    stats.structSizeThread = threadEnv->threadPlugins.GetClassSize();
    stats.structSizeFiber = fiberEnv->fiberFact.GetClassSize();

    return stats;
}

END_NATIVE_EXECUTIVE