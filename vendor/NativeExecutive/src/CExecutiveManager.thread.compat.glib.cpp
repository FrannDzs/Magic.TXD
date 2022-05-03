/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.compat.glib.cpp
*  PURPOSE:     Linux GCC libc compatibility layer for threads
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef __linux__

// We want to provide TLS (thread local storage) initialization functions for threads in this file.
// This is important because we let glibc handle all the TLS stuff, which entails module
// loading as well as employing the correct TLS model for the compilation settings.
// We hope that structures inside of the library do not change too much which we depend on.

#include "CExecutiveManager.thread.compat.glib.hxx"

#include "CExecutiveManager.thread.hxx"

#include "pthreads/internal/pthread.glibc.h"
#include "pthreads/CExecutiveManager.pthread.internal.h"

#include "PluginUtils.hxx"

#include <sdk/eirutils.h>

#include <sys/mman.h>
#include <linux/mman.h>
#include <unistd.h>
#include <link.h>
#include <elf.h>
#if defined(NATEXEC_LINUX_THREAD_SELF_CLEANUP) && defined(NATEXEC_LINUX_TSC_USE_DUMMY_TLD)
#include <asm/prctl.h>
#endif

// For compatibility.
#include <resolv.h>

#include <string.h>

#include <atomic>

#include <gnu/libc-version.h>
#include <sdk/NumericFormat.h>

// Version information.
static unsigned int _glibc_runtime_major = 0;
static unsigned int _glibc_runtime_minor = 0;

// Function dependencies on glibc.
extern "C" void __libc_thread_freeres( void );
extern "C" void __call_tls_dtors( void );
extern "C" void _IO_enable_locks( void );
extern "C" void __ctype_init( void );
extern "C" void _dl_get_tls_static_info( size_t *sizep, size_t *alignp );
extern "C" void* _dl_allocate_tls( void *tp );
extern "C" void _dl_deallocate_tls( void *tcb, bool dealloc_tcb );

// Data dependencies.
extern "C" __thread struct __res_state *__resp;

struct __128bits
{
    std::uint32_t data[4];
};

// The thread control block (TCB) data structure that should be located at the start of the TLS block.
// We list a limited amount of members that we are interested in. The more we detail things the more
// likely we are to lose support in the future!
struct _libc_tcbhead_t
{
    void *tcb;      // pointer to the thread-control-block (usually a self-pointer but not required)
    void *dtv;      // the DTV array used for implicit TLS support
    void *self;     // pointer to this thread descriptor
    int multiple_threads;
    std::atomic <int> gscope_flag;
    uintptr_t reserved3;
    uintptr_t stack_guard;
    uintptr_t pointer_guard;
    unsigned long int reserved6[2];
    unsigned int feature_1;
    int reserved_7;
    void *reserved_8[4];
    void *reserved_9;
    unsigned long long int reserved_10;
    alignas(32) __128bits reserved11[8][4];
    void *reserved12[8];

    // We must reach the thread id because it is actually important for compatibility between pthreads
    // and us. Seriously, glibc is infested by pthreads dependencies!
    void *reserved13[2];
    pid_t tid;
};
static_assert( offsetof(_libc_tcbhead_t, reserved11) == 0x80 );

#define THREAD_GSCOPE_FLAG_UNUSED   0
#define THREAD_GSCOPE_FLAG_USED     1
#define THREAD_GSCOPE_FLAG_WAIT     2

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#include <linux/futex.h>

struct _libc_auditstate
{
    uintptr_t pad1;
    unsigned int pad2;
};

struct _libc_r_scope_elem
{
    struct link_map **pad1;
    unsigned int pad2;
};

struct _libc_r_found_version
{
    const char *pad1;
    ElfW(Word) pad2;
    int pad3;
    const char *pad4;
};

struct _libc_r_search_path_struct
{
    void **pad1;
    int pad2;
};

struct _libc_r_file_id
{
    dev_t pad1;
    ino64_t pad2;
};

// ARCHITECTURE DEPENDANT STRUCT.
struct _libc_link_map_machine
{
    ElfW(Addr) pad1;
    ElfW(Addr) pad2;
    void *pad3;
};

#define DT_THISPROCNUM 0

// Taken from glibc/include/link.h internal header file.
struct _libc_link_map
{
    link_map pad1;
    // TODO: add all the internal fields!
    void *pad2;
    Lmid_t pad3;
    void *pad4;
    ElfW(Dyn) *pad5[DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM+DT_VALNUM+DT_ADDRNUM];
    const ElfW(Phdr) *pad6;
    ElfW(Addr) pad7;
    ElfW(Half) pad8;
    ElfW(Half) pad9;
    struct _libc_r_scope_elem pad10;
    struct _libc_r_scope_elem pad11;
    struct _libc_link_map *pad12;
    struct _libc_r_found_version *pad13;
    unsigned int pad14;
    Elf_Symndx pad15;
    Elf32_Word pad16;
    Elf32_Word pad17;
    const ElfW(Addr) *pad18;
    void *pad19;
    void *pad20;
    unsigned int pad21;
    unsigned int pad22;
    // ARCHITECTURE DEPENDANT STRUCT BEGIN.
    int pad23;
    // ARCHITECTURE DEPENDANT STRUCT END.
    struct _libc_r_search_path_struct pad24;
    void *pad25;
    void *pad26;
    const char *pad27;
    ElfW(Addr) pad28, pad29, pad30;
    void *pad31[4];
    size_t pad32;
    void **pad33;
    void *pad34[2];
    struct _libc_r_file_id pad35;
    struct _libc_r_search_path_struct pad36;
    void **pad37;
    void *pad38;
    unsigned int pad39;
    unsigned int pad40;
    ElfW(Word) pad41, pad42, pad43;
    int pad44;
    struct _libc_link_map_machine pad45;
    void *pad46;
    int pad47;
    void *pad48;
    void *pad49;
    // We need to access module TLS information because it needs initialization during dlopen.
    void *l_tls_initimage;
    size_t l_tls_initimage_size, l_tls_blocksize, l_tls_align, l_tls_firstbyte_offset;
    ptrdiff_t l_tls_offset;
    size_t l_tls_modid;
    size_t l_tls_dtor_count;
    ElfW(Addr) pad58;
    size_t pad59;
    unsigned long long pad60;
};

// Need to keep this struct up-to-date.
// Taken from ldsodefs.h
struct _libc_rtld_global
{
    struct link_namespaces
    {
        void *pad1;
        unsigned int pad2;
        void *pad3;
        size_t pad4;
        // unique_sym_table.
        __natexec_glibc_pthread_mutex_t pad5;
        void *pad6;
        size_t pad7, pad8;
        void *pad9;
        struct r_debug pad10;
    } pad1[16];
    size_t pad2;
    __natexec_glibc_pthread_mutex_t _dl_load_lock;
    __natexec_glibc_pthread_mutex_t _dl_load_write_lock;
    unsigned long long pad3;
    void *pad4;
    void *pad5;
    unsigned long int pad6;
    unsigned long int pad7;
    void *pad8;
    struct _libc_link_map pad9;
    struct _libc_auditstate pad10[16];
    void (*_dl_rtld_lock_recursive) (void *);
    void (*_dl_rtld_unlock_recursive) (void *);
    // architecture specific stuff here.
    unsigned int pad11[2];
    unsigned long pad12[2];
    // END OF ARCH SPECIFIC STUFF.
    int (*_dl_make_stack_executable_hook) (void **);
    ElfW(Word) pad13;
    bool pad14;
    size_t pad15;
    void *pad16;
    size_t pad17, pad18, pad19, pad20;
    void *pad21;
    size_t pad22;
    void (*_dl_init_static_tls)( struct _libc_link_map * );
    void (*_dl_wait_lookup_done)( void );
    // For now we do not care about more members.
};
extern "C" _libc_rtld_global _rtld_global;

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

// Since we have no access to the current pthread struct size we can only estimate how much
// size would give it justice to be. The real problem is of course that even if glibc is not
// linked with pthread support it does allocate this weird struct pthread as control-data
// for threads and expects it to be initialized in some way. It would greatly help me if they
// did consolidate this stuff as proper ABI and offer some glibc functions to retrieve more
// information (TCB struct size for one).
static constexpr size_t TCB_DATA_BUFFER_EXPAND = 2048;

#include "CExecutiveManager.native.hxx"

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

struct _libc_maintenance_preserve
{
    void (*_dl_init_static_tls)( struct _libc_link_map * );
    void (*_dl_wait_lookup_done)( void );
};
static _libc_maintenance_preserve _rtld_global_preserve = { 0 };

// pthread initialization routine as queried by libc on startup.
// If it is being queried then we are using a special static compilation of libc.
// Since pthread_initialize_minimal is called somewhere inside __libc_start_main or before it (through dl init)
// we can just null this function. This would hopefully kill any potential call. We can call the real deal inside
// our special entry point function instead.
extern "C" void __pthread_initialize_minimal( void )
{
    return;
}

// Special hooks.
static void _natexec_dl_rtld_lock_recursive( void *ptr )
{
    // If the mutex was not created yet then it points to a default mutex.
    //  but we want to have a recursive mutex here; this is actually handled through the non-POSIX
    //  special recursive mutex initializer.
    __natexec_glibc_pthread_mutex_t *mtx = (__natexec_glibc_pthread_mutex_t*)ptr;

    (void)_natexec_pthread_mutex_lock( mtx );
}

static void _natexec_dl_rtld_unlock_recursive( void *ptr )
{
    __natexec_glibc_pthread_mutex_t *mtx = (__natexec_glibc_pthread_mutex_t*)ptr;

    (void)_natexec_pthread_mutex_unlock( mtx );
}

static int _natexec_dl_make_stack_executable_hook( void **ptr )
{
    // We just do nothing, meow.
    return 0;
}

static void _natexec_dl_init_static_tls( struct _libc_link_map *map );
static void _natexec_dl_wait_lookup_done( void );

// Whatever that is.
static unsigned long _fork_gen_count = 0;

static void _natexec_threads_reclaim_event( void )
{
    // Since the user did execute a fork we can clear all thread resouces from us other than the main
    // thread, but fuck this shit. Using that function with our library is a terrible idea!
}

#include "CExecutiveManager.thread.compat.glib.ver2_31.hxx"
#include "CExecutiveManager.thread.compat.glib.ver2_32.hxx"

// Version-dependant symbol.
extern "C" void __libc_pthread_init( unsigned long *forkgenptr, void (*reclaim)(void), const void *pthread_functions );

// In the SHARED configuration of libc we cannot prevent any pthread_initialize_minimal from being called
// from any other pthreads implementation DLL. Thus we must write this routine to override changes made by
// any other possible pthreads shared object.
static void __natexec_pthread_initialize_minimal_internal( void )
{
    // If pthreads implementation is enabled then override the threading variables in the GLIBC struct.
    // otherwise disable all kinds of critical components we have put here related to pthreads inclusion.

    _rtld_global._dl_rtld_lock_recursive = _natexec_dl_rtld_lock_recursive;
    _rtld_global._dl_rtld_unlock_recursive = _natexec_dl_rtld_unlock_recursive;

    _rtld_global._dl_make_stack_executable_hook = _natexec_dl_make_stack_executable_hook;

    pthread_mutex_t *crtmtx = (pthread_mutex_t*)&_rtld_global._dl_load_lock;

    unsigned int rtld_lock_count = crtmtx->__data.__count;
    crtmtx->__data.__count = 0;

    while ( rtld_lock_count-- )
    {
        _natexec_pthread_mutex_lock( &_rtld_global._dl_load_lock );
    }

    // Preserve the existing function registrations so that we establish compatibility with the case
    // that internally a pthread of another library was created anyway (for whatever reason!).
    _rtld_global_preserve._dl_init_static_tls = _rtld_global._dl_init_static_tls;
    _rtld_global_preserve._dl_wait_lookup_done = _rtld_global._dl_wait_lookup_done;

    _rtld_global._dl_init_static_tls = _natexec_dl_init_static_tls;
    _rtld_global._dl_wait_lookup_done = _natexec_dl_wait_lookup_done;

    // Perform the call to __libc_pthread_init.
    bool has_valid_glibc_version = false;

    if ( _glibc_runtime_major == 2 )
    {
        if ( _glibc_runtime_minor == 31 )
        {
            __libc_pthread_init( &_fork_gen_count, _natexec_threads_reclaim_event, &_libc_pthread_functions_2_31_data );

            has_valid_glibc_version = true;
        }
        else if ( _glibc_runtime_minor == 32 )
        {
            __libc_pthread_init( &_fork_gen_count, _natexec_threads_reclaim_event, &_libc_pthread_functions_2_32_data );

            has_valid_glibc_version = true;
        }
    }

    if ( has_valid_glibc_version == false )
    {
        printf( "unsupported glibc version: %u.%u\n", _glibc_runtime_major, _glibc_runtime_minor );
        _natexec_syscall_exit_group( -1 );
    }
}

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

BEGIN_NATIVE_EXECUTIVE

static size_t _glibc_tcb_off = 0;
static size_t _glibc_tls_size = 0;
static size_t _glibc_tls_align = 0;

// We also need to have some private definitions.
struct threadPlugin_maintenance
{
    struct __res_state res = { 0 };

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
    RwListEntry <threadPlugin_maintenance> tls_node;
#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
};

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

END_NATIVE_EXECUTIVE

#include "CExecutiveManager.mtxevtstatic.hxx"

BEGIN_NATIVE_EXECUTIVE

// Need to keep track of a global TLS data list because modules can be loaded dynamically
// and still reserve TLS space that has to be initialized on-load time.
static constinit optional_struct_space <RwList <threadPlugin_maintenance>> _threads_tls_list;
static constinit mutexWithEventStatic _threads_tls_list_lock;

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

struct threadPlugin_glibc_tlsdata
{
    static inline size_t GetStructSize( CExecutiveManagerNative *execMan )
    {
        return _glibc_tls_size + sizeof(threadPlugin_maintenance);
    }

    static inline size_t GetStructAlignment( CExecutiveManagerNative *execMan )
    {
        return _glibc_tls_align;
    }

    inline _libc_tcbhead_t* get_header( void )
    {
        // Need to determine the correct placement of the TCB struct.
        // Pretty risky stuff here if things change in the future.
        return (_libc_tcbhead_t*)( (size_t)this + _glibc_tcb_off );
    }

    inline threadPlugin_maintenance* get_maintenance( void )
    {
        return (threadPlugin_maintenance*)( (size_t)this + _glibc_tls_size );
    }

    static threadPlugin_glibc_tlsdata* get_struct_from_maintenance( threadPlugin_maintenance *data )
    {
        return (threadPlugin_glibc_tlsdata*)( (size_t)data - _glibc_tls_size );
    }

    inline void Initialize( CExecThreadImpl *td )
    {
        // If we are a remote thread then we will never end up using this plugin.
        // Thus we just bail.
        if ( td->isLocallyRemoteThread )
        {
            return;
        }

        // Zero out our memory for good measure.
        memset( this, 0, _glibc_tls_size );

        // Proceed with basic initialization.
        {
            _libc_tcbhead_t *tcbptr = this->get_header();

            // Allocate the DTV.
            if ( _dl_allocate_tls(tcbptr) == nullptr )
            {
                // By throwing an exception we prevent construction of the thread.
                throw eir::eir_exception();
            }

#ifdef _DEBUG
            // Check the alignment.
            assert( (size_t)this % _glibc_tls_align == 0 );
#endif //_DEBUG

            // Initialize the data.
            tcbptr->tcb = tcbptr;
            tcbptr->self = tcbptr;

            _libc_tcbhead_t *current_thread_tcb = (_libc_tcbhead_t*)_natexec_get_thread_pointer();

            if ( current_thread_tcb != nullptr )
            {
                // Need to take over some data from the current thread aswell.
                tcbptr->stack_guard = current_thread_tcb->stack_guard;
                tcbptr->pointer_guard = current_thread_tcb->pointer_guard;
                tcbptr->multiple_threads = 1;       // enable locks in the CRT.

                // tls_setup_tcbhead:
                tcbptr->feature_1 = current_thread_tcb->feature_1;
            }

            // We can now proceed to bring havoc amongst the meows of the Linux world.
        }

        // Now initialize maintenance structs awell.
        {
            threadPlugin_maintenance *maintain = new (get_maintenance()) threadPlugin_maintenance();
            (void)maintain;

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
            CUnfairMutexContext ctx_append_tls( _threads_tls_list_lock.GetMutex() );

            LIST_APPEND( _threads_tls_list.get().root, maintain->tls_node );
#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
        }
    }

    inline void Shutdown( CExecThreadImpl *td )
    {
        // Remote threads do not have any TLS data set by us.
        if ( td->isLocallyRemoteThread )
        {
            return;
        }

        // De-Initialize the maintenance struct now.
        {
            threadPlugin_maintenance *maintain = get_maintenance();

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
            CUnfairMutexContext ctx_remove_tls( _threads_tls_list_lock.GetMutex() );

            LIST_REMOVE( maintain->tls_node );
#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

            maintain->~threadPlugin_maintenance();
        }

        void *tcbptr = this->get_header();

        // We tell the runtime to release all resources from the TCB that it created.
        _dl_deallocate_tls( tcbptr, false );

        // Memory will be automatically released along with the thread descriptor.
    }
};

static constinit optional_struct_space <PluginDependantStructRegister <PerThreadPluginRegister <threadPlugin_glibc_tlsdata, true, true, true>, executiveManagerFactory_t>> _threadplugin_glibc_support_reg;

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

END_NATIVE_EXECUTIVE

void _natexec_dl_init_static_tls( struct _libc_link_map *map )
{
    using namespace NativeExecutive;

    // Under linux we can load .so files and then reserve static TLS space inside
    // our thread TLS regions. At load time of the .so we need to initialize the TLS
    // data into each thread aswell.
    {
        CUnfairMutexContext ctx_init_new_tls_data( _threads_tls_list_lock.GetMutex() );

        LIST_FOREACH_BEGIN( threadPlugin_maintenance, _threads_tls_list.get().root, tls_node )

            void *tlsdata = threadPlugin_glibc_tlsdata::get_struct_from_maintenance( item );

            // TCB at TP
            void *dest = ( (char*)tlsdata + _glibc_tls_size ) - map->l_tls_offset;

            memcpy( dest, map->l_tls_initimage, map->l_tls_initimage_size );
            memset( (char*)dest + map->l_tls_initimage_size, 0, map->l_tls_blocksize );

        LIST_FOREACH_END
    }

    // Call the previous function at the end.
    _rtld_global_preserve._dl_init_static_tls( map );
}

void _natexec_dl_wait_lookup_done( void )
{
    using namespace NativeExecutive;

    // Do the specific gscope waiting for all current threads.
    {
        CUnfairMutexContext ctx_wait_lookup( _threads_tls_list_lock.GetMutex() );

        _libc_tcbhead_t *curthreadtcb = (_libc_tcbhead_t*)_natexec_get_thread_pointer();

        LIST_FOREACH_BEGIN( threadPlugin_maintenance, _threads_tls_list.get().root, tls_node )

            threadPlugin_glibc_tlsdata *tlsdata = threadPlugin_glibc_tlsdata::get_struct_from_maintenance( item );

            _libc_tcbhead_t *tcbptr = tlsdata->get_header();

            if ( tcbptr != curthreadtcb && tcbptr->gscope_flag != THREAD_GSCOPE_FLAG_UNUSED )
            {
                std::atomic <int> *const gscope_flagp = &tcbptr->gscope_flag;

                int expected_gscope_value = THREAD_GSCOPE_FLAG_USED;

                if ( gscope_flagp->compare_exchange_strong(expected_gscope_value, THREAD_GSCOPE_FLAG_WAIT) )
                {
                    do
                    {
                        _natexec_syscall_futex( (int*)gscope_flagp, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, THREAD_GSCOPE_FLAG_WAIT, 0, 0, 0 );
                    }
                    while ( *gscope_flagp == THREAD_GSCOPE_FLAG_WAIT );
                }
            }

        LIST_FOREACH_END
    }

    // Call the previous function at the end.
    _rtld_global_preserve._dl_wait_lookup_done();
}

BEGIN_NATIVE_EXECUTIVE

// The event subsystem has its own refcount so this is valid.
extern void registerEventManagement( void );
extern void unregisterEventManagement( void );

#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

static inline threadPlugin_glibc_tlsdata* get_thread_local_data( CExecThreadImpl *thread )
{
    CExecutiveManagerNative *execMan = thread->manager;

    auto *env = _threadplugin_glibc_support_reg.get().GetPluginStruct( execMan );

    if ( env == nullptr )
    {
        return nullptr;
    }

    return env->GetThreadPlugin( thread );
}

void _threadsupp_glibc_enter_thread( CExecThreadImpl *current_thread )
{
    FATAL_ASSERT( current_thread->isLocallyRemoteThread == false );

    threadPlugin_glibc_tlsdata *data = get_thread_local_data( current_thread );

    // Since glibc is infested by dependencies to pthreads we want to also fill in the tid field
    // to make sure that glibc can properly differentiate between threads. Might look into this issue
    // more and see if we can cut away pthreads stuff and completely replace it with our own.
    _libc_tcbhead_t *tcb = data->get_header();

    tcb->tid = _natexec_syscall_gettid();

    threadPlugin_maintenance *maintain = data->get_maintenance();

    // Do the regular initialization.
    __resp = &maintain->res;

    __ctype_init();

    // Now the user code can run.
}

#if defined(NATEXEC_LINUX_THREAD_SELF_CLEANUP) && defined(NATEXEC_LINUX_TSC_USE_DUMMY_TLD)
static void *_dummy_tld_memptr = nullptr;
#endif //DUMMY TLD SAFETY

void _threadsupp_glibc_leave_thread( CExecThreadImpl *current_thread )
{
    FATAL_ASSERT( current_thread->isLocallyRemoteThread == false );

    // Clean up the bullshit ;)
    __call_tls_dtors();

    __libc_thread_freeres();

#if defined(NATEXEC_LINUX_THREAD_SELF_CLEANUP) && defined(NATEXEC_LINUX_TSC_USE_DUMMY_TLD)
    // The framework has asked us to create a dummy TLD to prevent crashing on FS register memory access.
    // We can grant this wish.
    void *tcbptr = (char*)_dummy_tld_memptr + _glibc_tcb_off;

    _natexec_syscall_arch_prctl( ARCH_SET_FS, tcbptr );
#endif //DUMMY TLD SAFETY

    // Now we are ready to kick the bucket from glibc perspective.
}

void _threadsupp_glibc_register( void )
{
    // First obtain the version information.
    {
        const char *ver_check_ptr = gnu_get_libc_version();

        const char *ver_check_major_beg = ver_check_ptr;

        bool has_ended = false;

        while ( true )
        {
            char c = *ver_check_ptr;

            if ( c == 0 || c == '.' )
            {
                if ( c == 0 )
                {
                    has_ended = true;
                }
                break;
            }

            ver_check_ptr++;
        }

        unsigned int runtime_major = eir::to_number_len <unsigned int> ( ver_check_major_beg, ver_check_ptr - ver_check_major_beg );
        unsigned int runtime_minor = 0;

        if ( !has_ended )
        {
            ver_check_ptr++;

            const char *ver_check_minor_beg = ver_check_ptr;

            while ( true )
            {
                char c = *ver_check_ptr;

                if ( c == 0 || c == '.' )
                {
                    if ( c == 0 )
                    {
                        has_ended = true;
                    }
                    break;
                }

                ver_check_ptr++;
            }

            runtime_minor = eir::to_number_len <unsigned int> ( ver_check_minor_beg, ver_check_ptr - ver_check_minor_beg );
        }

        // Store the version information globally.
        _glibc_runtime_major = runtime_major;
        _glibc_runtime_minor = runtime_minor;
    }

    // Setup basic stuff.
    // We do not screw around with threading because in linux there can be threads anyway starting with
    // sudden asynchronous signals.
    _IO_enable_locks();

    // Initialize the TCB.
    {
        _libc_tcbhead_t *tcb = (_libc_tcbhead_t*)_natexec_get_thread_pointer();

        tcb->multiple_threads = 1;      // enable locks in the CRT.
    }

    size_t tls_size, tls_align;
    _dl_get_tls_static_info( &tls_size, &tls_align );

    size_t alloc_size = ( tls_size + TCB_DATA_BUFFER_EXPAND + tls_align );

    _glibc_tls_size = alloc_size;
    _glibc_tls_align = tls_align;
    _glibc_tcb_off = ALIGN_SIZE( tls_size, tls_align );

    _threadplugin_glibc_support_reg.Construct( executiveManagerFactory );

#if defined(NATEXEC_LINUX_THREAD_SELF_CLEANUP) && defined(NATEXEC_LINUX_TSC_USE_DUMMY_TLD)
    _dummy_tld_memptr = malloc( alloc_size );
    memset( _dummy_tld_memptr, 0, alloc_size );
#endif //DUMMY TLD SAFETY

#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
    // Need to have the event subsystem because we use locks in very internal spaces.
    registerEventManagement();
    _threads_tls_list.Construct();
    _threads_tls_list_lock.Initialize();

    // Initialize the pthreads specific data.
    // This is mainly compatibility of glibc because it expects an available pthreads API layer.
    __natexec_pthread_initialize_minimal_internal();
#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
}

void _threadsupp_glibc_unregister( void )
{
#ifdef NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION
    _threads_tls_list_lock.Shutdown();
    _threads_tls_list.Destroy();
    unregisterEventManagement();
#endif //NATEXEC_ENABLE_PTHREADS_IMPLEMENTATION

#if defined(NATEXEC_LINUX_THREAD_SELF_CLEANUP) && defined(NATEXEC_LINUX_TSC_USE_DUMMY_TLD)
    free( _dummy_tld_memptr );
#endif //DUMMY TLD SAFETY

    _threadplugin_glibc_support_reg.Destroy();
}

void* _threadsupp_glibc_get_tcb( CExecThreadImpl *threadDescriptor )
{
    threadPlugin_glibc_tlsdata *tlsptr = get_thread_local_data( threadDescriptor );

    if ( tlsptr == nullptr )
    {
        return nullptr;
    }

    return tlsptr->get_header();
}

END_NATIVE_EXECUTIVE

#endif //__linux__
