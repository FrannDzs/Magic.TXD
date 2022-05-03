/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/OSUtils.vmemconf.h
*  PURPOSE:     Virtual-memory-based vector list of structs
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _NATIVE_VIRTUAL_MEMORY_CONFIG_UTILS_
#define _NATIVE_VIRTUAL_MEMORY_CONFIG_UTILS_

#include <algorithm>

#include "MemoryRaw.h"
#include "OSUtils.vmem.h"

// Shared helpers for rational memory management.
struct NativeVirtualMemoryRationalConfig
{
    static constexpr size_t RECOMMENDED_MIN_NUM_ALLOC_PAGES = 16;   // We mimic the Windows operating system.
    static constexpr size_t RECOMMENDED_MAX_NUM_ALLOC_PAGES = 32;

    static constexpr size_t RECOMMENDED_STORE_ATLEAST = 3;

    // Returns the virtual memory size that should be reserved to support valid container commit semantics.
    // The function is growing monotonely on both arguments.
#ifdef __linux__
    template <typename syscallDispatcherType>
#endif //__linux__
    static inline size_t GetRecommendedArenaSize( PlatformVirtualMemoryAccessor
#ifdef __linux__
        <syscallDispatcherType>
#endif //__linux__
        & vmemAccess, size_t maintainSize, size_t structSize ) noexcept
    {
        size_t pageSize = vmemAccess.GetPlatformPageSize();

        // Need to be able to at least store one entry.
        size_t minSizeByDef = ALIGN( maintainSize + structSize * RECOMMENDED_STORE_ATLEAST, pageSize, pageSize );

        size_t minSizeBySpec = std::max( minSizeByDef, ( pageSize * RECOMMENDED_MIN_NUM_ALLOC_PAGES ) );

        size_t minSizeBySystem = vmemAccess.GetPlatformAllocationGranularity();

        return std::max( minSizeBySpec, minSizeBySystem );
    }
};

// Helper.
template <typename numType>
inline numType UINT_CEIL_DIV( numType value, numType divBy ) noexcept
{
    if ( value == 0 )
    {
        return 0;
    }

    // It helps to think of when the division returns exactly 1 and what
    // it then means to increment it by one.
    return ( ( value - 1 ) / divBy ) + 1;
}

#endif //_NATIVE_VIRTUAL_MEMORY_CONFIG_UTILS_