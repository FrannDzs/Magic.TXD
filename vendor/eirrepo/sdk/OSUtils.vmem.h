/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/OSUtils.vmem.h
*  PURPOSE:     OS implementation of virtual memory mapping
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _NATIVE_OS_VIRTUAL_MEMORY_FUNCS_
#define _NATIVE_OS_VIRTUAL_MEMORY_FUNCS_

// For FATAL_ASSERT.
#include "eirutils.h"

// For size_t.
#include <stddef.h>

#if defined(__linux__)
#include <sys/mman.h>
#include <linux/mman.h>
#include <unistd.h>
#include "OSUtils.version.h"
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif //CROSS PLATFORM CODE

// Functions for reserving, freeing, committing and decomitting virtual memory regions.
// By implementing this interface for a certain platform you enable many features of
// the Eir development environment basing on very to-the-metal memory semantics!
#ifdef __linux__
template <typename syscallDispatcherType>
#endif
struct PlatformVirtualMemoryAccessor
{
private:
#ifdef _WIN32
    SYSTEM_INFO _systemInfo;
#elif defined(__linux__)
    // Some system metrics.
    size_t _sys_page_size;
    NativeOSVersionInfo _verInfo;
#endif //_WIN32

public:
    inline PlatformVirtualMemoryAccessor( void ) noexcept
    {
#ifdef _WIN32
        GetSystemInfo( &_systemInfo );
#elif defined(__linux__)
        long linux_page_size = sysconf( _SC_PAGESIZE );

        FATAL_ASSERT( linux_page_size > 0 );

        this->_sys_page_size = (size_t)linux_page_size;
#endif //_WIN32
    }

    // As long as we do not store complicated things we are fine with default methods.
    inline PlatformVirtualMemoryAccessor( PlatformVirtualMemoryAccessor&& right ) noexcept = default;
    inline PlatformVirtualMemoryAccessor( const PlatformVirtualMemoryAccessor& right ) = default;

    inline PlatformVirtualMemoryAccessor& operator = ( PlatformVirtualMemoryAccessor&& right ) noexcept = default;
    inline PlatformVirtualMemoryAccessor& operator = ( const PlatformVirtualMemoryAccessor& right ) = default;

    // Platform dependent query functions.
    inline size_t GetPlatformPageSize( void ) const noexcept
    {
        // The page size is the amount of bytes the smallest requestable memory unit on hardware consists of.

        size_t page_size = 0;

#ifdef _WIN32
        page_size = (size_t)this->_systemInfo.dwPageSize;
#elif defined(__linux__)
        page_size = this->_sys_page_size;
#else
#error No page size query function for this architecture.
#endif

        return page_size;
    }

    inline size_t GetPlatformAllocationGranularity( void ) const noexcept
    {
        // Allocation granularity defines the size of memory requestable by-minimum from the OS.
        // On Windows you usually cannot request memory by page-size but have to create arenas
        // that consist of multiple pages, managing memory inside manually.

        //TODO: for systems other than Windows it could make sense to specifically introduce
        // a recommended allocation granularity.

        size_t arena_size = 0;

#ifdef _WIN32
        arena_size = (size_t)this->_systemInfo.dwAllocationGranularity;
#elif defined(__linux__)
        // I am not aware of any limitation in the Linux kernel of this nature.
        arena_size = this->_sys_page_size;
#else
#error No memory arena query function for this architecture.
#endif

        return arena_size;
    }

    // Cross-platform memory request API.
    // Allocates memory randomly or at the exactly specified position using the provided size.
    // The allocated memory does not have to be accessible after allocation; it could have to
    // be committed first.
    // MUST BE AN atomic operation.
    inline void* RequestVirtualMemory( void *memPtr, size_t memSize ) const noexcept
    {
        void *actualMemPtr = nullptr;

#ifdef _WIN32
        actualMemPtr = VirtualAlloc( memPtr, (SIZE_T)memSize, MEM_RESERVE, PAGE_READWRITE );
#elif defined(__linux__)
        // We actually removed MAP_UNINITIALIZED here because MSFT dropped support for it inside
        // of their own WSL implementation. It can have good reasons imo, like consolidating the
        // syscall layer. But it is against the (shaky) specification.
        int mmap_flags = MAP_PRIVATE|MAP_ANONYMOUS;

        if ( this->_verInfo.IsThirdPartyKernel() == false )
        {
            // If we are not the MSFT kernel then we can just try to allocate dirty bytes.
            // The functionality of this flag is just optional anyway.
            mmap_flags |= MAP_UNINITIALIZED;
        }

        bool do_fixed = false;

        if ( memPtr != nullptr )
        {
            // We must not use MAP_FIXED because it is broken shit.
            // Instead we must do verifications.
            do_fixed = true;

            unsigned char ver_major = this->_verInfo.GetKernelMajor();
            unsigned char ver_minor = this->_verInfo.GetKernelMinor();

            if ( ver_major > 4 || ( ver_major == 4 && ver_minor >= 17 ) )
            {
                // Under Linux 4.17 there is this new flag called MAP_FIXED_NOREPLACE.
                // I am happy that the Linux developers actually improve by comparison :-)
                mmap_flags |= MAP_FIXED_NOREPLACE;
            }
        }

        actualMemPtr = syscallDispatcherType::mmap( memPtr, memSize, PROT_NONE, mmap_flags, -1, 0 );

        if ( actualMemPtr == MAP_FAILED )
        {
            return nullptr;
        }

        if ( do_fixed && actualMemPtr != memPtr )
        {
            syscallDispatcherType::munmap( actualMemPtr, memSize );
            return nullptr;
        }
#endif //CROSS PLATFORM CODE.

        return actualMemPtr;
    }

    // Release memory that was previously allocated using RequestVirtualMemory.
    inline bool ReleaseVirtualMemory( void *memPtr, size_t memSize ) const noexcept
    {
        bool success = false;

#ifdef _WIN32
        // The documentation says that if we MEM_RELEASE things, the size
        // parameter must be 0.
        success = ( VirtualFree( memPtr, 0, MEM_RELEASE ) != FALSE );
#elif defined(__linux__)
        success = ( syscallDispatcherType::munmap( memPtr, memSize ) == 0 );
#endif //_WIN32

        FATAL_ASSERT( success == true );

        return success;
    }

    // Committing and decommitting memory.
    // This function makes the reserved memory actually usable in the program.
    static inline bool CommitVirtualMemory( void *mem_ptr, size_t mem_size ) noexcept
    {
        bool success = false;

#ifdef _WIN32
        LPVOID commitHandle = VirtualAlloc( mem_ptr, mem_size, MEM_COMMIT, PAGE_READWRITE );

        success = ( commitHandle == mem_ptr );
#elif defined(__linux__)
        // We protect the page into accessibility.
        int error_out = syscallDispatcherType::mprotect( mem_ptr, mem_size, PROT_READ|PROT_WRITE );

        success = ( error_out == 0 );
#endif //_WIN32

        return success;
    }

    // Decommits the memory, making it unusable by the program.
    static inline bool DecommitVirtualMemory( void *mem_ptr, size_t mem_size ) noexcept
    {
        bool success = false;

#ifdef _WIN32
        BOOL decommitSuccess = VirtualFree( mem_ptr, mem_size, MEM_DECOMMIT );

        success = ( decommitSuccess == TRUE );
#elif defined(__linux__)
        int error_out = syscallDispatcherType::mprotect( mem_ptr, mem_size, PROT_NONE );

        success = ( error_out == 0 );
#endif //_WIN32

        return success;
    }
};

#ifdef __linux__

// By default pass syscalls through the CRT layer.
struct LinuxVirtualMemoryAccessorDefaultSyscalls
{
    static inline void* mmap( void *memPtr, size_t size, int flags, int prot, int fd, off_t offset )
    {
        void *memptr = ::mmap( memPtr, size, flags, prot, fd, offset );

        if ( memptr == MAP_FAILED )
        {
#ifdef _DEBUG
            int errcode = errno;

            (void)errcode;
#endif //_DEBUG

            return MAP_FAILED;
        }

        return memptr;
    }

    static inline int munmap( void *memPtr, size_t size )
    {
        return ::munmap( memPtr, size );
    }

    static inline int mprotect( void *memPtr, size_t size, int prot )
    {
        return ::mprotect( memPtr, size, prot );
    }
};

typedef PlatformVirtualMemoryAccessor <LinuxVirtualMemoryAccessorDefaultSyscalls> NativeVirtualMemoryAccessor;
#else
typedef PlatformVirtualMemoryAccessor NativeVirtualMemoryAccessor;
#endif //CROSS PLATFORM CODE.

#endif //_NATIVE_OS_VIRTUAL_MEMORY_FUNCS_
