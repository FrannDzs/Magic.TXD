/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.dbgheap.integchk.cpp
*  PURPOSE:     Heap management tools for error isolation & debugging
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "CExecutiveManager.dbgheap.hxx"
#include "CExecutiveManager.dbgheap.impl.hxx"

#include <sdk/MemoryRaw.h>
#include <sdk/DataUtil.h>

BEGIN_NATIVE_EXECUTIVE

#ifdef USE_HEAP_DEBUGGING
#ifdef USE_FULL_PAGE_HEAP
#ifdef PAGE_HEAP_INTEGRITY_CHECK

#ifndef PAGE_MEM_DEBUG_PATTERN
#define PAGE_MEM_DEBUG_PATTERN 0x6A
#endif //PAGE_MEM_DEBUG_PATTERN

#ifndef PAGE_MEM_ACTIVE_DEBUG_PATTERN
#define PAGE_MEM_ACTIVE_DEBUG_PATTERN 0x11
#endif //PAGE_MEM_ACTIVE_DEBUG_PATTERN

// Define the platform page mem intro checksum.
#ifdef __linux__
#define PAGE_MEM_INTRO_CHECKSUM 0xCAFEBABE13377331
#elif defined(_WIN32)
#if defined(_M_IX86) || defined(_M_ARM)
#define PAGE_MEM_INTRO_CHECKSUM 0xCAFEBABE
#elif defined(_M_AMD64) || defined(_M_ARM64)
#define PAGE_MEM_INTRO_CHECKSUM 0xCAFEBABE73311337
#endif
#endif

#ifndef PAGE_MEM_INTRO_CHECKSUM
#error missing platform page mem intro checksum
#endif

#define PAGE_MEM_OUTRO_CHECKSUM 0xBABECAFE

#define MEM_PAGE_MOD( bytes )   ( ( (bytes) + _PAGE_SIZE_ACTUAL - 1 ) / _PAGE_SIZE_ACTUAL )

struct _memIntro
{
    size_t checksum;                    // basic checksum that ought to stay untouched for under-run protection
    size_t offFromPageStart;            // we have to be closely aligned to the object for reverse lookup to work
    size_t pageAllocSize;               // for performance sake.
    size_t objSize;                     // requested size of the object in memory
    size_t objStartOff;                 // for proper alignment of the object
    RwListEntry <_memIntro> memList;    // list of all allocations is kept.
};

struct _memOutro
{
    unsigned int checksum;              // basic checksum for overrun protection.
};

static constinit optional_struct_space <RwList <_memIntro>> g_privateMemory;

void _native_initHeap( void )
{
    // Clear the list of all allocations.
    g_privateMemory.Construct();
}

inline static size_t _getRequiredAllocMetaSize( size_t objStartOff, size_t objSize )
{
    // Returns the size of the entire memory region taken by allocation of objSize and objStartOff
    // so that things can be safely placed.
    return objStartOff + objSize + sizeof( _memIntro ) + sizeof( _memOutro ) + alignof(_memOutro);
}

inline static void* _get_obj_from_intro( _memIntro *intro_ptr, size_t objStartOff )
{
    return ( (char*)( intro_ptr + 1 ) + objStartOff );
}

inline static _memIntro* _get_intro_from_obj( void *obj_ptr )
{
    return (_memIntro*)SCALE_DOWN( (size_t)obj_ptr - sizeof(_memIntro), alignof(_memIntro) );
}

inline static _memOutro* _get_outro_from_intro( _memIntro *intro_ptr, size_t objStartOff, size_t objSize )
{
    return (_memOutro*)ALIGN_SIZE( (size_t)( intro_ptr + 1 ) + objStartOff + objSize, alignof(_memOutro) );
}

inline static void* _get_page_alloc_from_intro( _memIntro *intro_ptr )
{
    return ( (char*)intro_ptr - intro_ptr->offFromPageStart );
}

inline static size_t _native_getRequiredAllocPageSize( size_t objSize, size_t alignment )
{
    // Returns the required memory size in pages that should be requested by the runtime.
    // We lie by giving the impossible value for objStartOff "alignment" because then we can always place it.
    return ( MEM_PAGE_MOD( _getRequiredAllocMetaSize( alignment, objSize ) ) * _PAGE_SIZE_ACTUAL );
}

void* _native_allocMem( size_t memSize, size_t alignment )
{
    const size_t pageRegionRequestSize = _native_getRequiredAllocPageSize( memSize, alignment );

    void *mem = _native_allocMemPage( pageRegionRequestSize );

#ifdef PAGE_HEAP_ERROR_ON_LOWMEM
    MEM_INTERRUPT( mem == nullptr );
#else
    if ( mem == nullptr )
    {
        return nullptr;
    }
#endif //PAGE_HEAP_ERROR_ON_LOWMEM

    // We make the proper assumption that _memIntro is always greatly aligned at page start.

    // Calculate the object alignment requirement.
    size_t earliest_obj_pos = (size_t)( (_memIntro*)mem + 1 );
    size_t earliest_possible_obj_pos = ALIGN_SIZE( earliest_obj_pos, alignment );

    // Now we have to place the intro header.
    size_t header_pos = SCALE_DOWN( earliest_possible_obj_pos - sizeof(_memIntro), alignof(_memIntro) );

    size_t objStartOff = ( earliest_possible_obj_pos - header_pos - sizeof(_memIntro) );

    _memIntro *intro = (_memIntro*)header_pos;
    _memOutro *outro = _get_outro_from_intro( intro, objStartOff, memSize );

    void *objPtr = _get_obj_from_intro( intro, objStartOff );

    // Fill memory with debug pattern
    {
        // *** PRE-INTRO MEMORY ***
        FSDataUtil::assign_impl <unsigned char> ( (unsigned char*)mem, (unsigned char*)intro, PAGE_MEM_DEBUG_PATTERN );
        // *** OBJECT MEMORY (contains the alignment artifacts) ***
        FSDataUtil::assign_impl <unsigned char> ( (unsigned char*)( intro + 1 ), (unsigned char*)outro, PAGE_MEM_ACTIVE_DEBUG_PATTERN );
        // *** AFTER-OUTRO MEMORY ***
        FSDataUtil::assign_impl <unsigned char> ( (unsigned char*)( outro + 1 ), (unsigned char*)mem + pageRegionRequestSize, PAGE_MEM_DEBUG_PATTERN );
    }

    // Fill in the headers themselves.
    intro->checksum = PAGE_MEM_INTRO_CHECKSUM;
    intro->offFromPageStart = (size_t)intro - (size_t)mem;
    intro->pageAllocSize = pageRegionRequestSize;
    intro->objSize = memSize;
    intro->objStartOff = objStartOff;
    LIST_APPEND( g_privateMemory.get().root, intro->memList );

    outro->checksum = PAGE_MEM_OUTRO_CHECKSUM;

    return objPtr;
}

void _native_checkBlockIntegrity( void *ptr )
{
    _memIntro *intro = _get_intro_from_obj( ptr );

    size_t objSize = intro->objSize;
    size_t objStartOff = intro->objStartOff;

    _memOutro *outro = _get_outro_from_intro( intro, objStartOff, objSize );

    MEM_INTERRUPT( intro->checksum == PAGE_MEM_INTRO_CHECKSUM && outro->checksum == PAGE_MEM_OUTRO_CHECKSUM );

    void *page_ptr = _get_page_alloc_from_intro( intro );

    size_t allocSize = intro->pageAllocSize;

    // Check memory integrity
    // *** PRE-INTRO MEMORY ***
    {
        unsigned char *endptr = (unsigned char*)intro;
        unsigned char *seek = (unsigned char*)page_ptr;

        while ( seek < endptr )
        {
            // If this check fails, memory corruption has happened before the intro header.
            MEM_INTERRUPT( *seek == PAGE_MEM_DEBUG_PATTERN );

            seek++;
        }
    }
    // *** OBJECT ALIGNMENT MEMORY ***
    {
        unsigned char *endptr = (unsigned char*)intro + objStartOff;
        unsigned char *seek = (unsigned char*)( intro + 1 );

        while ( seek < endptr )
        {
            // If this check fails, memory corruption has happened just before the object allocation.
            MEM_INTERRUPT( *seek == PAGE_MEM_ACTIVE_DEBUG_PATTERN );

            seek++;
        }
    }
    // *** OUTRO ALIGNMENT MEMORY ***
    {
        unsigned char *endptr = (unsigned char*)outro;
        unsigned char *seek = (unsigned char*)ptr + objSize;

        while ( seek < endptr )
        {
            // If this check fails, memory corruption has happened just after the memory allocation.
            MEM_INTERRUPT( *seek == PAGE_MEM_ACTIVE_DEBUG_PATTERN );

            seek++;
        }
    }
    // *** AFTER-OUTRO MEMORY ***
    {
        unsigned char *endptr = (unsigned char*)page_ptr + allocSize;
        unsigned char *seek = (unsigned char*)outro + sizeof(*outro);

        while ( seek < endptr )
        {
            // If this check fails, memory corruption has happened after the whole allocation.
            MEM_INTERRUPT( *seek == PAGE_MEM_DEBUG_PATTERN );

            seek++;
        }
    }

    LIST_VALIDATE( intro->memList );
}

void _native_freeMem( void *ptr )
{
    if ( !ptr )
        return;

    _native_checkBlockIntegrity( ptr );

    _memIntro *intro = _get_intro_from_obj( ptr );
    LIST_REMOVE( intro->memList );

    void *page_ptr = _get_page_alloc_from_intro( intro );

    _native_freeMemPage( page_ptr );
}

size_t _native_getAllocSize( void *ptr )
{
    if ( !ptr )
        return 0;

    _native_checkBlockIntegrity( ptr );

    _memIntro *intro = _get_intro_from_obj( ptr );

    return intro->objSize;
}

inline static void _update_debug_pattern_after_object( _memIntro *intro, void *ptr, size_t newSize, void *page_ptr, size_t pageAllocSize )
{
    // Get new pointers to meta-data.
    _memOutro *outro = _get_outro_from_intro( intro, intro->objStartOff, newSize );

    // Rewrite block integrity
    intro->objSize = newSize;
    outro->checksum = PAGE_MEM_OUTRO_CHECKSUM;

    // Update the debug patterns starting from the end of object memory.
    // *** OUTRO ALIGNMENT MEMORY ***
    FSDataUtil::assign_impl <unsigned char> ( (unsigned char*)ptr + newSize, (unsigned char*)outro, PAGE_MEM_ACTIVE_DEBUG_PATTERN );
    // *** AFTER-OUTRO MEMORY ***
    FSDataUtil::assign_impl <unsigned char> ( (unsigned char*)( outro + 1 ), (unsigned char*)page_ptr + pageAllocSize, PAGE_MEM_DEBUG_PATTERN );
}

bool _native_resizeMem( void *ptr, size_t newSize )
{
    // Cannot free memory using this function.
    if ( !ptr || !newSize )
        return false;

    // Verify block contents.
    _native_checkBlockIntegrity( ptr );

    // Get the meta-data of our block.
    _memIntro *intro = _get_intro_from_obj( ptr );

    size_t oldObjSize = intro->objSize;

    // Verify that our object size has changed at all
    if ( newSize == oldObjSize )
    {
        // Nothing to do.
        return true;
    }

    size_t objStartOff = intro->objStartOff;

    void *page_ptr = _get_page_alloc_from_intro( intro );

    // Reallocate to actually required page memory.
    const size_t constructNewSize = _native_getRequiredAllocPageSize( newSize, objStartOff );

    bool reallocSuccess = _native_resizeMemPage( page_ptr, constructNewSize );

    if ( reallocSuccess == false )
    {
        // Could not resize thus bail.
        return false;
    }

    // Update fields.
    intro->pageAllocSize = constructNewSize;

    // Success in resizing the underlying page allocation.
    // We just update the debug information and proceed.
    _update_debug_pattern_after_object( intro, ptr, newSize, page_ptr, constructNewSize );
    return true;
}

void* _native_reallocMem( void *ptr, size_t newSize, size_t alignment )
{
    // Is this a request to allocate new memory?
    if ( !ptr )
    {
        return _native_allocMem( newSize, alignment );
    }

    // Actually fulfill request to release memory.
    if ( newSize == 0 )
    {
        _native_freeMem( ptr );
        return nullptr;
    }

    // Verify block contents.
    _native_checkBlockIntegrity( ptr );

    // Get the meta-data of the old data.
    _memIntro *old_intro = _get_intro_from_obj( ptr );

    size_t oldObjSize = old_intro->objSize;

    // Verify that our object size has changed at all
    if ( newSize == oldObjSize )
    {
        // Nothing to do.
        return ptr;
    }

    void *page_ptr = _get_page_alloc_from_intro( old_intro );

    // Reallocate to actually required page memory.
    const size_t constructNewSize = _native_getRequiredAllocPageSize( newSize, alignment );

    bool reallocSuccess = _native_resizeMemPage( page_ptr, constructNewSize );

    void *out_ptr = nullptr;

    // The reallocation may fail if the page nesting is too complicated.
    // For this we must move to a completely new block of memory that is size'd appropriately.
    if ( !reallocSuccess )
    {
        // Allocate a new page region of memory.
        void *newMem = _native_allocMem( newSize, alignment );

        // Only process this request if the OS kernel could fetch a new page for us.
        if ( newMem != nullptr )
        {
            // Copy the data contents to the new memory region.
            out_ptr = newMem;

            size_t validDataSize = std::min( newSize, oldObjSize );

            memcpy( newMem, ptr, validDataSize );

            // Deallocate the old memory.
            // Only can be done if memory was successfully served.
            _native_freeMem( ptr );
        }
    }
    else
    {
        out_ptr = ptr;

        // Update fields.
        old_intro->pageAllocSize = constructNewSize;

        _update_debug_pattern_after_object( old_intro, ptr, newSize, page_ptr, constructNewSize );
    }

    return out_ptr;
}

void _native_validateMemory( void )
{
    // Make sure the DebugHeap manager is not damaged.
    LIST_VALIDATE( g_privateMemory.get().root );

    // Check all blocks in order
    LIST_FOREACH_BEGIN( _memIntro, g_privateMemory.get().root, memList )
        void *objMem = _get_obj_from_intro( item, item->objStartOff );
        _native_checkBlockIntegrity( objMem );
    LIST_FOREACH_END
}

#ifdef PAGE_HEAP_MEMORY_STATS
inline static void OutputDebugStringFormat( const char *fmt, ... )
{
    char buf[0x10000];
    va_list argv;

    va_start( argv, fmt );
    vsnprintf( buf, sizeof( buf ), fmt, argv );
    va_end( argv );

#ifdef _WIN32
    OutputDebugStringA( buf );
#elif defined(__linux__)
    printf( "%s\n", buf );
#endif //CROSS PLATFORM CODE
}
#endif //PAGE_HEAP_MEMORY_STATS

void _native_shutdownHeap( void )
{
    // Make sure the DebugHeap manager is not damaged.
    LIST_VALIDATE( g_privateMemory.get().root );

#ifdef PAGE_HEAP_MEMORY_STATS
    // Memory debugging statistics.
    size_t blockCount = 0;
    size_t pageCount = 0;
    size_t memLeaked = 0;
#endif //PAGE_HEAP_MEMORY_STATS

    // Check all blocks in order and free them
    while ( !LIST_EMPTY( g_privateMemory.get().root ) )
    {
        _memIntro *item = LIST_GETITEM( _memIntro, g_privateMemory.get().root.next, memList );

#ifdef PAGE_HEAP_MEMORY_STATS
        // Keep track of stats.
        blockCount++;
        pageCount += MEM_PAGE_MOD( item->objSize + sizeof(_memIntro) + sizeof(_memOutro) );
        memLeaked += item->objSize;
#endif //PAGE_HEAP_MEMORY_STATS

        void *objMem = _get_obj_from_intro( item, item->objStartOff );
        _native_freeMem( objMem );
    }

#ifdef PAGE_HEAP_MEMORY_STATS
    if ( blockCount != 0 )
    {
        OutputDebugStringFormat( "Heap Memory Leak Protocol:\n" );
        OutputDebugStringFormat(
            "* leaked memory: %u\n" \
            "* blocks/pages allocated: %u/%u [%u]\n",
            memLeaked,
            blockCount, pageCount, blockCount * _PAGE_SIZE_ACTUAL
        );
    }
    else
        OutputDebugStringFormat( "No memory leaks detected." );
#endif //PAGE_HEAP_MEMORY_STATS

    // Release static variables.
    g_privateMemory.Destroy();
}

#endif //PAGE_HEAP_INTEGRITY_CHECK
#endif //USE_FULL_PAGE_HEAP
#endif //USE_HEAP_DEBUGGING

END_NATIVE_EXECUTIVE
