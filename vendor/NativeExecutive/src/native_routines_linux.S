.intel_syntax noprefix

.set _glibc_tcbhead_tcb, 0
.set _glibc_tcbhead_dvt, 8
.set _glibc_tcbhead_self, 16

#ifdef NATEXEC_FIRST_ENTRY_POINT

.global _start_natexec
.global _init_natexec

_start_natexec:
    # Please note that the program entry point IS NOT the first code that is executed inside a Linux ELF binary.
    # For example TLS and EH initialization is usually done prior to this.

    # Mark the outer-most frame as required by ABI (probably done by _start aswell but it's ok)
    xor rbp,rbp
    # First we initialize the NativeExecutive library to setup all subsystems (locks, memory management, etc).
    mov r12,rdx
    mov r13,rax
    call _init_natexec
    mov rdx,r12
    mov rax,r13
    # Proceed into the actual program start-up (whatever CRT the application was compiled with)
    jmp _start

#endif //NATEXEC_FIRST_ENTRY_POINT

#ifdef NATEXEC_LINUX_THREAD_SELF_CLEANUP

.global _natexec_linux_end_thread

_natexec_linux_end_thread:
    # rdi (first arg): stackaddr
    # rsi (second arg): stacksize

    # Free the stack memory.
    mov rax,0x0B
    syscall

    # Call the exit routine.
    mov rax,0x3C
    mov rdi,1
    syscall

    hlt

#endif //NATEXEC_LINUX_THREAD_SELF_CLEANUP

# For glibc compatibility.

.global _natexec_get_thread_pointer

_natexec_get_thread_pointer:
    mov rax,fs:[_glibc_tcbhead_self]
    ret

# SYSCALL implementations for clean interfacing with the Linux kernel.
# We do NOT want to use the GLIBC wrappers because they do dirty things like
# writing result codes to the TCB. This is unacceptable in certain to-the-metal
# scenarios.

.global _natexec_syscall_exit_group
.global _natexec_syscall_futex
.global _natexec_syscall_mmap
.global _natexec_syscall_munmap
.global _natexec_syscall_mprotect
.global _natexec_syscall_arch_prctl
.global _natexec_syscall_prctl
.global _natexec_syscall_gettid
.global _natexec_syscall_clone
.global _natexec_syscall_tkill

# rdi: int exit_code
_natexec_syscall_exit_group:
    mov rax,0xE7
    syscall
    hlt

# rdi: uint32_t* uaddr
# rsi: int op
# rdx: uint32_t val
# r10: struct timespec* utime
# r8: uint32_t* uaddr2
# r9: uint32_t val3
_natexec_syscall_futex:
    mov rax,0xCA
    mov r10,rcx
    syscall
    ret

# rdi: void* addr
# rsi: size_t length
# rdx: int prot
# r10: int flags
# r8: int fd
# r9: off_t offset
_natexec_syscall_mmap:
    mov rax,0x09
    mov r10,rcx
    syscall
    cmpq rax,-4095
    jae _natexec_syscall_mmap_label_failure
    ret
_natexec_syscall_mmap_label_failure:
    mov rax,-1
    ret

# rdi: void* addr
# rsi: size_t length
_natexec_syscall_munmap:
    mov rax,0x0B
    syscall
    ret

# rdi: void* addr
# rsi: size_t length
# rdx: int prot
_natexec_syscall_mprotect:
    mov rax,10
    syscall
    ret

# rdi: int code
# rsi: void* ptr
_natexec_syscall_arch_prctl:
    mov rax,0x9E
    syscall
    ret

# rdi: int option
# rsi: unsigned long arg2
# rdx: unsigned long arg3
# r10: unsigned long arg4
# r8: unsigned long arg5
_natexec_syscall_prctl:
    mov rax,0x9D
    mov r10,rcx
    syscall
    ret

_natexec_syscall_gettid:
    mov rax,0xBA
    syscall
    ret

_natexec_syscall_clone_beginroutine:
    # Execute the provided routine by the user.
    pop rax
    pop rdi
    call rax

    # Call the thread exit system call.
    mov rdi,rax
    mov rax,0x3C
    syscall

    hlt

# * raw system call description
# rdi: unsigned long flags
# rsi: void* child_stack
# rdx: pid_t* parent_tid
# r10: pid_t* child_tid
# r8: void* tls
# * we implement:
# rdi: int (*fn)(void*)
# rsi: void* child_stack
# rdx: int flags
# rcx: void* arg
# r8: pid_t* parent_tid
# r9: void* tls
# stack qword: pid_t* child_tid
_natexec_syscall_clone:
    mov r10,qword ptr[rsp+8]
    sub rsi,24
    lea rax,[rip+_natexec_syscall_clone_beginroutine]
    mov qword ptr[rsi],rax
    mov qword ptr[rsi+8],rdi
    mov qword ptr[rsi+16],rcx
    mov rax,56
    mov rdi,rdx
    mov rdx,r8
    mov r8,r9
    syscall
    # Here the child and the parent branch into different functions.
    ret

# rdi: pid_t tokill
# rsi: int sig
_natexec_syscall_tkill:
    mov rax,0xC8
    syscall
    ret
