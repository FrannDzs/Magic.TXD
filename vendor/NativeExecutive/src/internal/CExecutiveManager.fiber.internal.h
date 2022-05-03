/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.fiber.internal.h
*  PURPOSE:     Fiber environment internals
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_FIBER_INTERNAL_
#define _EXECUTIVE_MANAGER_FIBER_INTERNAL_

#include "CExecutiveManager.fiber.h"

#include "CExecutiveManager.execgroup.internal.h"

#define THREAD_PLUGIN_FIBER_STACK       0x00000001
#define THREAD_PLUGIN_FIBER_ROOT        0X00000002

#ifdef __linux__
#include <ucontext.h>
#endif //__linux__

BEGIN_NATIVE_EXECUTIVE

struct FiberStatus;

// size_t logically is the machine word size.
typedef size_t regType_t;
typedef char xmmReg_t[16];

struct Fiber
{
#ifdef _WIN32
#if defined(_M_IX86)
    // Preserve __cdecl
    // If changing any of this, please update the structs inside of native_routines_x86.asm
    regType_t ebx;   // 0
    regType_t edi;   // 4
    regType_t esi;   // 8
    regType_t esp;   // 12
    regType_t eip;   // 16
    regType_t ebp;   // 20

    // Stack manipulation routines.
    template <typename dataType>
    inline void pushdata( const dataType& data )
    {
        *--((dataType*&)esp) = data;
    }
#elif defined(_M_AMD64)
    // https://msdn.microsoft.com/en-us/library/9z1stfyw.aspx
    // If changing any of this, please update the structs inside of native_routines_x64.asm
    regType_t eip;
    regType_t esp;
    regType_t r12;
    regType_t r13;
    regType_t r14;
    regType_t r15;
    regType_t rdi;
    regType_t rsi;
    regType_t rbx;
    regType_t rbp;

    xmmReg_t xmm6;
    xmmReg_t xmm7;
    xmmReg_t xmm8;
    xmmReg_t xmm9;
    xmmReg_t xmm10;
    xmmReg_t xmm11;
    xmmReg_t xmm12;
    xmmReg_t xmm13;
    xmmReg_t xmm14;
    xmmReg_t xmm15;
#else
#error Unsupported Windows architecture for fibers
#endif //CROSS MACHINE DEFS
    void *stack_base, *stack_limit;
    void *except_info;
#elif defined(__linux__)
    // On Linux we have the ucontext_t family of functions
    ucontext_t linux_context;
    void *stack_ptr;
#else
#error Unsupported architecture for Fibers!
#endif

    size_t stackSize;
};

enum eFiberStatus : std::uint32_t
{
    FIBER_RUNNING,
    FIBER_SUSPENDED,
    FIBER_TERMINATED,
    FIBER_TERMINATING,
};

// Some calling convention quirks for platforms.
#ifdef _WIN32
#define FIBER_SAVED_CALLCONV __cdecl
#else
#define FIBER_SAVED_CALLCONV
#endif //CROSS PLATFORM CODE

struct FiberStatus
{
    Fiber *callee;      // yielding information

    typedef void (FIBER_SAVED_CALLCONV*termfunc_t)( FiberStatus *userdata );

    termfunc_t termcb;  // function called when fiber terminates

    eFiberStatus status;
};

// We need to decide on a calling convention because this is very important.
#if defined(_M_IX86)
// MSVC compiler.
#define FIBER_CALLCONV      __stdcall
#elif defined(__i686__)
// GNU GCC compiler.
// There is no STDCALL on Linux.
#define FIBER_CALLCONV      __attribute__((__cdecl__))
#else
// Anything else is undefined.
#define FIBER_CALLCONV
#endif

typedef void    (FIBER_CALLCONV*FiberProcedure) ( FiberStatus *status );

namespace ExecutiveFiber
{
    // Native assembler methods.
    Fiber*          newfiber        ( FiberStatus *userdata, void *fiber_ptr, void *stack_ptr, size_t stackSize, FiberProcedure proc, FiberStatus::termfunc_t termcb );
    void*           getstackspace   ( Fiber *env );

    void FIBER_SAVED_CALLCONV eswitch( Fiber *from, Fiber *to );
    void FIBER_SAVED_CALLCONV qswitch( Fiber *from, Fiber *to );

    static constexpr size_t STACK_ALIGNMENT = 16;
}

struct CFiberImpl : public CFiber, public FiberStatus
{
    CFiberImpl( CExecutiveManagerNative *manager, CExecutiveGroupImpl *group, Fiber *runtime )
    {
        // Using "this" is actually useful.
        this->runtime = runtime;
        this->callee = nullptr;

        this->has_term_unhandled_exception = false;

        this->manager = manager;
        this->group = group;

        this->status = FIBER_SUSPENDED;
    }

    void terminate_impl( void );

    CExecutiveManagerNative *manager;

    Fiber *runtime;                 // storing Fiber runtime context
    void *userdata;

    enum class eUnhandledExceptionType : unsigned int
    {
        UNKNOWN,
        FIBERTERM
    };

    bool has_term_unhandled_exception;
#ifdef NATEXEC_EXCEPTION_COPYPUSH
    std::exception_ptr term_unhandled_exception;
#else
    eUnhandledExceptionType unhandled_exception_type;
    union
    {
        CFiber *fiber;
    } unhandled_exception_args;
    // removed the STL thing because it has unspecific memory allocation.
#endif //NATEXEC_EXCEPTION_COPYPUSH

    fiberexec_t callback;               // routine set by the fiber request.

    RwListEntry <CFiberImpl> node;      // node in fiber manager
    RwListEntry <CFiberImpl> groupNode; // node in fiber group

    CExecutiveGroupImpl *group;         // fiber group this fiber is in.

    double resumeTimer;
};

// Thread fiber stack iterator.
struct threadFiberStackIterator
{
    threadFiberStackIterator( CExecThreadImpl *thread );
    ~threadFiberStackIterator( void );

    bool IsEnd( void ) const;
    void Increment( void );

    CFiberImpl* Resolve( void ) const;

private:
    CExecThreadImpl *thread;

    size_t iter;
};

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_FIBER_INTERNAL_
