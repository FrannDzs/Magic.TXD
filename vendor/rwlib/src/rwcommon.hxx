/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwcommon.hxx
*  PURPOSE:
*       Common definitions of RenderWare helpers that can be used throughout the engine.
*       Only solid and proven concepts can be posted here.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_COMMON_
#define _RENDERWARE_COMMON_

// Helper macros.
#if !defined(_DEBUG) && defined(_MSC_VER)
#define assume( x ) __analysis_assume( (x) )
#else
#define assume( x ) assert( (x) )
#endif

#include <sdk/MemoryUtils.h>

namespace rw
{

// Special version of the growable array.
struct _generic_growable_array_allocator
{
    inline void* Allocate( size_t memSize, unsigned int flags ) const
    {
        return malloc( memSize );
    }

    inline void* Realloc( void *ptr, size_t newMemSize, unsigned int flags ) const
    {
        return realloc( ptr, newMemSize );
    }

    inline void Free( void *ptr )
    {
        free( ptr );
    }
};

// Cached class allocator.
template <typename dataType>
struct CachedConstructedClassAllocator
{
protected:
    struct dataEntry : public dataType
    {
        template <typename ...Args>
        AINLINE dataEntry( CachedConstructedClassAllocator *storage, bool isSummoned, Args... theArgs ) : dataType( std::forward <Args> ( theArgs )... )
        {
            LIST_INSERT( storage->m_freeList.root, this->node );

            this->isSummoned = isSummoned;
        }

        RwListEntry <dataEntry> node;
        bool isSummoned;
    };

    struct summonBlockTail
    {
        unsigned int allocCount;
        RwListEntry <summonBlockTail> node;
    };

public:
    AINLINE CachedConstructedClassAllocator( void )
    {
        return;
    }

    AINLINE ~CachedConstructedClassAllocator( void )
    {
        assert( LIST_EMPTY( m_freeList.root ) );
        assert( LIST_EMPTY( m_usedList.root ) );

        assert( LIST_EMPTY( m_summonBlocks.root ) );
    }

    template <typename ...Args>
    AINLINE void SummonEntries( EngineInterface *intf, unsigned int startCount, Args... theArgs )
    {
        void *mem = intf->MemAllocateP( sizeof( dataEntry ) * startCount + sizeof( summonBlockTail ) );

        if ( mem )
        {
            for ( unsigned int n = 0; n < startCount; n++ )
            {
                new ( (dataEntry*)mem + n ) dataEntry( this, true, theArgs... );
            }

            // Get the tail of the summon block and add it to the list of summoned blocks.
            summonBlockTail *theTail = (summonBlockTail*)( (dataEntry*)mem + startCount );

            theTail->allocCount = startCount;

            LIST_APPEND( m_summonBlocks.root, theTail->node );
        }
    }

    AINLINE void Shutdown( EngineInterface *intf )
    {
        // Having actually used blocks by this point is deadly.
        // Make sure there are none.
        assert( LIST_EMPTY( m_usedList.root ) );

        // Deallocate all free blocks.
        LIST_FOREACH_BEGIN( dataEntry, m_freeList.root, node )
            if ( !item->isSummoned )
            {
                // Delete this object.
                item->~dataEntry();

                // Free this memory.
                intf->MemFree( item );
            }
        LIST_FOREACH_END

        LIST_CLEAR( m_freeList.root );

        // Deallocate all summoned blocks now.
        LIST_FOREACH_BEGIN( summonBlockTail, m_summonBlocks.root, node )
            unsigned int allocCount = item->allocCount;

            // Get the memory begin pointer.
            void *blockStart = (void*)( (dataEntry*)item - allocCount );

            // Destroy all items.
            for ( unsigned int n = 0; n < allocCount; n++ )
            {
                ((dataEntry*)blockStart + n )->~dataEntry();
            }

            // Free it.
            intf->MemFree( blockStart );
        LIST_FOREACH_END

        LIST_CLEAR( m_summonBlocks.root );
    }

private:
    template <typename ...Args>
    AINLINE dataEntry* AllocateNew( EngineInterface *intf, Args... theArgs )
    {
        void *mem = intf->MemAllocateP( sizeof( dataEntry ) );

        if ( mem )
        {
            return new (mem) dataEntry( this, false, theArgs... );
        }
        return nullptr;
    }

public:
    template <typename ...Args>
    AINLINE dataType* Allocate( EngineInterface *intf, Args... theArgs )
    {
        dataEntry *entry = nullptr;

        if ( !LIST_EMPTY( m_freeList.root ) )
        {
            entry = LIST_GETITEM( dataEntry, m_freeList.root.next, node );
        }

        if ( !entry )
        {
            entry = AllocateNew( intf, theArgs... );
        }

        if ( entry )
        {
            LIST_REMOVE( entry->node );
            LIST_INSERT( m_usedList.root, entry->node );
        }

        return entry;
    }

    AINLINE void Free( EngineInterface *intf, dataType *data )
    {
        dataEntry *entry = (dataEntry*)data;

        LIST_REMOVE( entry->node );
        LIST_INSERT( m_freeList.root, entry->node );
    }

protected:
    RwList <dataEntry> m_usedList;
    RwList <dataEntry> m_freeList;

    RwList <summonBlockTail> m_summonBlocks;
};

// Structure for fast stack based struct/class allocation.
template <class allocType, int max>
class StaticAllocator
{
    char mem[sizeof(allocType) * max];
    unsigned int count;

public:
    AINLINE StaticAllocator( void )
    {
        count = 0;
    }

    AINLINE void* Allocate( void )
    {
        assume( count < max );

        void *outMem = mem + (sizeof(allocType) * count++);

        assume( outMem != nullptr );

        return outMem;
    }

    // Hack function; only use if performance critical code is used.
    AINLINE void Pop( void )
    {
        count--;
    }
};

}

#endif //_RENDERWARE_COMMON_
