#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif //_MSC_VER

#include <CFileSystemInterface.h>

#include "include.h"

// Default allocator for this library.
struct ConfigHeapAllocator
{
    AINLINE static void* Allocate( void *refPtr, size_t memSize, size_t alignment ) noexcept
    {
#ifdef _MSC_VER
        return _aligned_malloc( memSize, alignment );
#else
        return aligned_alloc( alignment, memSize );
#endif
    }

    AINLINE static void Free( void *refPtr, void *memPtr ) noexcept
    {
#ifdef _MSC_VER
        _aligned_free( memPtr );
#else
        free( memPtr );
#endif
    }
};

// gtaconfig would like to have a global called "fileRoot".
extern CFileTranslator *fileRoot;
