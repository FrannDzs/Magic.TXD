/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.memory.hxx
*  PURPOSE:     Memory management internal definitions header.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATEXEC_MM_INTERNAL_DEFS_
#define _NATEXEC_MM_INTERNAL_DEFS_

#include <sdk/OSUtils.memheap.h>

#ifdef __linux__

#include "CExecutiveManager.native.hxx"

// We want to use our own syscalls for every memory management we do in this library.
struct NatExecLinuxSyscallLayer
{
    static inline void* mmap( void *addr, size_t len, int prot, int flags, int fd, off_t offset )
    {
        return _natexec_syscall_mmap( addr, len, prot, flags, fd, offset );
    }

    static inline int munmap( void *addr, size_t len )
    {
        return (int)_natexec_syscall_munmap( addr, len );
    }

    static inline int mprotect( void *addr, size_t len, int prot )
    {
        return (int)_natexec_syscall_mprotect( addr, len, prot );
    }
};

#endif //__linux__

typedef PlatformHeapAllocator
#ifdef __linux__
    <NatExecLinuxSyscallLayer>
#endif //__linux__
    NatExecHeapAllocator;

#endif //_NATEXEC_MM_INTERNAL_DEFS_
