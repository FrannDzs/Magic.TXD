/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.native.hxx
*  PURPOSE:     Includes native module definitions for NativeExecutive.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATIVE_EXECUTIVE_INTERNAL_
#define _NATIVE_EXECUTIVE_INTERNAL_

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>

#if defined(_M_IX86)

// Fiber routines.
extern "C" void __stdcall _fiber86_retHandler( NativeExecutive::FiberStatus *userdata );
extern "C" void __cdecl _fiber86_eswitch( NativeExecutive::Fiber *from, NativeExecutive::Fiber *to );
extern "C" void __cdecl _fiber86_qswitch( NativeExecutive::Fiber *from, NativeExecutive::Fiber *to );

// Thread routines.
extern "C" DWORD WINAPI _thread86_procNative( LPVOID lpThreadParameter );

#elif defined(_M_AMD64)

// Fiber routines.
extern "C" void __cdecl _fiber64_procStart( void );
extern "C" void __cdecl _fiber64_eswitch( NativeExecutive::Fiber *from, NativeExecutive::Fiber *to );
extern "C" void __cdecl _fiber64_qswitch( NativeExecutive::Fiber *from, NativeExecutive::Fiber *to );

// Fiber special routines not to be called directly.
extern "C" void __stdcall _fiber64_term( void );

// Thread routines.
extern "C" DWORD WINAPI _thread64_procNative( LPVOID lpThreadParameter );

#endif

#elif defined(__linux__)

// Native routines.
extern "C" void* _natexec_get_thread_pointer( void );    // only useful if threading library present.

#ifdef NATEXEC_LINUX_THREAD_SELF_CLEANUP
extern "C" [[noreturn]] void _natexec_linux_end_thread( void *stackaddr, size_t stacksize ) noexcept;
#endif //NATEXEC_LINUX_THREAD_SELF_CLEANUP

// SYSCALL routines.
extern "C" [[noreturn]] void _natexec_syscall_exit_group( int exit_code );
extern "C" long _natexec_syscall_futex( int *uaddr, int op, int val, struct timespec *utime, int *uaddr2, int val3 );
extern "C" void* _natexec_syscall_mmap( void *addr, size_t length, int prot, int flags, int fd, off_t offset );
extern "C" long _natexec_syscall_munmap( void *addr, size_t length );
extern "C" long _natexec_syscall_mprotect( void *addr, size_t length, int prot );
extern "C" long _natexec_syscall_arch_prctl( int code, void *ptr );
extern "C" pid_t _natexec_syscall_gettid( void );
extern "C" long _natexec_syscall_clone( int (*fn)(void*), void *child_stack, int flags, void *arg, pid_t *parent_tid, void *tls, pid_t *child_tid );
extern "C" long _natexec_syscall_tkill( pid_t tokill, int sig );

#endif //CROSS PLATFORM DEFINES.

#endif //_NATIVE_EXECUTIVE_INTERNAL_
