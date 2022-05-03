/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/rwlist.hpp
*  PURPOSE:     RenderWare list export for multiple usage
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _RENDERWARE_LIST_DEFINITIONS_
#define _RENDERWARE_LIST_DEFINITIONS_

#include "eirutils.h"

#include <cstddef>

#include <utility>

// Macros used by RW, optimized for usage by the engine (inspired by S2 Games' macros)
// The_GTA (20.8.2021): C preprocessor macros have been removed because C++ offers much stronger syntax using C++20 concepts.

template <typename listType>
concept RwIsDoubleLinkedNode = requires (listType *N) { N = N->prev; N = N->next; }; 

// Optimized versions.
// Not prone to bugs anymore.
template <RwIsDoubleLinkedNode listType>    inline bool LIST_ISVALID( listType& item ) noexcept                 { return item.next->prev == &item && item.prev->next == &item; }
template <RwIsDoubleLinkedNode listType>    inline void LIST_VALIDATE( listType& item )                         { FATAL_ASSERT( LIST_ISVALID( item ) ); }
template <RwIsDoubleLinkedNode listType>    inline void LIST_APPEND( listType& link, listType& item ) noexcept  { item.next = &link; item.prev = link.prev; item.prev->next = &item; item.next->prev = &item; }
template <RwIsDoubleLinkedNode listType>    inline void LIST_INSERT( listType& link, listType& item ) noexcept  { item.next = link.next; item.prev = &link; item.prev->next = &item; item.next->prev = &item; }
template <RwIsDoubleLinkedNode listType>    inline void LIST_REMOVE( listType& link ) noexcept                  { link.prev->next = link.next; link.next->prev = link.prev; }
template <RwIsDoubleLinkedNode listType>    inline void LIST_CLEAR( listType& link ) noexcept                   { link.prev = &link; link.next = &link; }
template <RwIsDoubleLinkedNode listType>    inline void LIST_INITNODE( listType& link ) noexcept                { link.prev = nullptr; link.next = nullptr; }
template <RwIsDoubleLinkedNode listType>    inline bool LIST_EMPTY( listType& link ) noexcept                   { return link.prev == &link && link.next == &link; }

// Need a special offsetof implementation because of a MSVC compiler error.
#ifdef _MSC_VER
#define rw_offsetof(t,m)    ( (size_t)( &( (t*)(0) )->m ) )
#else
#define rw_offsetof(t,m)    offsetof(t,m)
#endif

// These have to be macros unfortunately.
// Special macros used by RenderWare only.
#define LIST_GETITEM(type, item, node) ( (type*)( (char*)(item) - rw_offsetof(type, node) ) )
#define LIST_FOREACH_BEGIN(type, root, node) for ( RwListEntry <type> *iter = (root).next, *niter; iter != &(root); iter = niter ) { type *item = LIST_GETITEM(type, iter, node); niter = iter->next; (void)item;
#define LIST_FOREACH_END }

template < class type >
struct RwListEntry
{
    inline RwListEntry( void ) noexcept     {}

    // We do not support default move assignment ever.
    // If you want to do that you must mean it, using moveFrom method.
    inline RwListEntry( RwListEntry&& right ) = delete;
    inline RwListEntry( const RwListEntry& right ) = delete;

    // Since assignment operators could be expected we make this clear
    // that they are not.
    inline RwListEntry& operator = ( RwListEntry&& right ) = delete;
    inline RwListEntry& operator = ( const RwListEntry& right ) = delete;

    inline void moveFrom( RwListEntry&& right ) noexcept
    {
        // NOTE: members of this are assumed invalid.
        //       hence this is more like a moveFromConstructor.

        RwListEntry *realNext = right.next;
        RwListEntry *realPrev = right.prev;

        realNext->prev = this;
        realPrev->next = this;

        this->next = realNext;
        this->prev = realPrev;

        // The beautiful thing is that we implicitly "know" about
        // validity of list nodes. In other words there is no value
        // we set the node pointers to when moving away.
    }

    RwListEntry <type> *next, *prev;
};
template < class type >
struct RwList
{
    inline RwList( void ) noexcept
    {
        LIST_CLEAR( this->root );
    }

    inline RwList( const RwList& ) = delete;
    inline RwList( RwList&& right ) noexcept
    {
        if ( !LIST_EMPTY( right.root ) )
        {
            this->root.moveFrom( std::move( right.root ) );

            LIST_CLEAR( right.root );
        }
        else
        {
            LIST_CLEAR( this->root );
        }
    }

    inline void moveIntoBegin( RwList&& right ) noexcept
    {
        // Inserts all items from right at the beginning of our list.

        if ( LIST_EMPTY( right.root ) == false )
        {
            RwListEntry <type> *insFirst = right.root.next;
            RwListEntry <type> *insLast = right.root.prev;

            // We keep our prev node.
            RwListEntry <type> *ourFirst = this->root.next;

            insFirst->prev = &this->root;
            this->root.next = insFirst;

            insLast->next = ourFirst;
            ourFirst->prev = insLast;

            LIST_CLEAR( right.root );
        }
    }

    inline void moveIntoEnd( RwList&& right ) noexcept
    {
        // Appends all items from right at the end of our list.

        if ( LIST_EMPTY( right.root ) == false )
        {            
            RwListEntry <type> *insNext = right.root.next;
            RwListEntry <type> *insPrev = right.root.prev;

            // We keep our next node.
            RwListEntry <type> *ourPrev = root.prev;

            insNext->prev = ourPrev;
            ourPrev->next = insNext;

            insPrev->next = &this->root;
            this->root.prev = insPrev;

            LIST_CLEAR( right.root );
        }
    }

    inline RwList& operator = ( const RwList& ) = delete;
    inline RwList& operator = ( RwList&& right ) noexcept
    {
        // Inserts the items from the list "right" at the end of list "this".
        moveIntoEnd( std::move( right ) );

        return *this;
    }

    RwListEntry <type> root;
};

// List helpers.
template <typename structType, size_t nodeOffset, bool isReverse = false, bool isConst = false>
struct rwListIterator
{
    inline rwListIterator( void ) noexcept
    {
        // Actually works really fine!
        this->rootNode = nullptr;
        this->list_iterator = nullptr;
    }

    inline rwListIterator( RwList <structType>& rootNode ) noexcept
    {
        this->rootNode = &rootNode.root;

        if ( isReverse == false )
        {
            this->list_iterator = this->rootNode->next;
        }
        else
        {
            this->list_iterator = this->rootNode->prev;
        }
    }

    inline rwListIterator( RwList <structType>& rootNode, RwListEntry <structType> *node ) noexcept
    {
        this->rootNode = &rootNode.root;

        this->list_iterator = node;
    }

    inline rwListIterator( const rwListIterator& right ) noexcept
    {
        this->rootNode = right.rootNode;

        this->list_iterator = right.list_iterator;
    }

    inline rwListIterator( rwListIterator&& right ) noexcept
    {
        this->rootNode = right.rootNode;

        this->list_iterator = right.list_iterator;

        right.rootNode = nullptr;
        right.list_iterator = nullptr;
    }

    inline void operator = ( const rwListIterator& right ) noexcept
    {
        this->rootNode = right.rootNode;

        this->list_iterator = right.list_iterator;
    }

    inline bool IsEnd( void ) const noexcept
    {
        return ( this->rootNode == this->list_iterator );
    }

    inline void Increment( void ) noexcept
    {
        if constexpr ( isReverse == false )
        {
            this->list_iterator = this->list_iterator->next;
        }
        else
        {
            this->list_iterator = this->list_iterator->prev;
        }
    }

    inline void Decrement( void ) noexcept
    {
        if constexpr ( isReverse == false )
        {
            this->list_iterator = this->list_iterator->prev;
        }
        else
        {
            this->list_iterator = this->list_iterator->next;
        }
    }

    inline typename std::conditional <isConst, const structType*, structType*>::type Resolve( void ) const noexcept
    {
        return (structType*)( (char*)this->list_iterator - nodeOffset );
    }

    inline RwListEntry <structType>* GetNode( void ) const noexcept
    {
        return this->list_iterator;
    }

private:
    RwListEntry <structType> *rootNode;
    RwListEntry <structType> *list_iterator;
};

#define DEF_LIST_ITER( newName, structType, nodeName ) \
    typedef rwListIterator <structType, offsetof(structType, nodeName)> newName;
#define DEF_LIST_CONST_ITER( newName, structType, nodeName ) \
    typedef rwListIterator <structType, offsetof(structType, nodeName), false, true> newName;

#define DEF_LIST_REVITER( newName, structType, nodeName ) \
    typedef rwListIterator <structType, offsetof(structType, nodeName), true> newName;
#define DEF_LIST_CONST_REVITER( newName, structType, nodeName ) \
    typedef rwListIterator <structType, offsetof(structType, nodeName), true, true> newName;

#endif //_RENDERWARE_LIST_DEFINITIONS_
