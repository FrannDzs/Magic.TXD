/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Map.h
*  PURPOSE:     Optimized Map implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// Since we have the AVLTree class and the optimized native heap allocator semantics,
// we can get to implement a Map aswell. Sometimes we require Maps, so we do not want to
// depend on the STL things that often come with weird dependencies to third-party modules
// (specifically on the Linux implementation).

#ifndef _EIR_MAP_HEADER_
#define _EIR_MAP_HEADER_

#include "Exceptions.h"
#include "eirutils.h"
#include "MacroUtils.h"
#include "MetaHelpers.h"

#include "AVLTree.h"
#include "AVLTree.helpers.h"
#include "avlsetmaputil.h"

#include <type_traits>
#include <tuple>

namespace eir
{

typedef GenericDefaultComparator MapDefaultComparator;

#define MAP_TEMPLINST \
template <typename keyType, typename valueType, MemoryAllocator allocatorType, CompareCompatibleComparator <keyType, keyType> comparatorType = MapDefaultComparator, typename exceptMan = eir::DefaultExceptionManager>
#define MAP_TEMPLARGS \
template <typename TWC, typename, MemoryAllocator, CompareCompatibleComparator <TWC, TWC>, typename>

// Necessary forward declaration.
MAP_TEMPLARGS
struct Map;

namespace MapUtils
{

// Node inside the Map.
template <typename keyType, typename valueType>
struct Node
{
    MAP_TEMPLARGS friend struct Map;
    template <typename structType, typename, MemoryAllocator subAllocatorType, typename... Args> friend structType* eir::dyn_new_struct( subAllocatorType&, void*, Args&&... );
    template <typename structType, MemoryAllocator subAllocatorType> friend void eir::dyn_del_struct( subAllocatorType&, void*, structType* ) noexcept(std::is_nothrow_destructible <structType>::value);

    // Get rid of the default things.
    inline Node( const Node& right ) = delete;
    inline Node( Node&& right ) = delete;

    inline Node& operator = ( const Node& ) = delete;
    inline Node& operator = ( Node&& right ) = delete;

    // The public access stuff.
    inline const keyType& GetKey( void ) const noexcept
    {
        return this->key;
    }

    inline valueType& GetValue( void ) noexcept
    {
        return this->value;
    }

    inline const valueType& GetValue( void ) const noexcept
    {
        return this->value;
    }

    template <CompareCompatibleComparator <keyType, keyType> comparatorType>
    struct avlKeyNodeDispatcher
    {
        template <typename subFirstKeyType, typename subSecondKeyType> requires ( CompareCompatibleComparator <comparatorType, subFirstKeyType, subSecondKeyType> )
        static AINLINE eCompResult compare_keys( const subFirstKeyType& left, const subSecondKeyType& right ) noexcept(NothrowCompareCompatibleComparator <comparatorType, subFirstKeyType, subSecondKeyType>)
        {
            if constexpr ( CompareCompatibleRegularComparator <comparatorType, subFirstKeyType, subSecondKeyType> )
            {
                return ( comparatorType::compare_values( left, right ) );
            }
            else
            {
                return flip_comp_result( comparatorType::compare_values( right, left ) );
            }
        }

        static inline eCompResult CompareNodes( const AVLNode *left, const AVLNode *right ) noexcept(std::is_nothrow_invocable <decltype(compare_keys <keyType, keyType>), keyType, keyType>::value)
        {
            const Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );
            const Node *rightNode = AVL_GETITEM( Node, right, sortedByKeyNode );

            return compare_keys( leftNode->key, rightNode->key );
        }

        template <typename subKeyType>
        static inline eCompResult CompareNodeWithValue( const AVLNode *left, const subKeyType& right ) noexcept(std::is_nothrow_invocable <decltype(compare_keys <keyType, subKeyType>), keyType, subKeyType>::value)
        {
            const Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );

            return compare_keys( leftNode->key, right );
        }
    };

private:
    // Not available to things that just use Map.
    // But the friend class does have access.
    template <std::convertible_to <keyType> cKeyType, std::convertible_to <valueType> cValueType>
    inline Node( cKeyType&& key, cValueType&& value )
        : key( std::forward <cKeyType> ( key ) ), value( std::forward <cValueType> ( value ) )
    {
        return;
    }
    template <typename... valueArgs> requires ( eir::constructible_from <valueType, valueArgs...> )
    inline Node( keyType key, std::tuple <valueArgs...>&& vargs ) : key( std::move( key ) ), value( std::make_from_tuple <valueType> ( std::move( vargs ) ) )
    {
        return;
    }
    inline ~Node( void ) noexcept = default;

    keyType key;
    valueType value;

    AVLNode sortedByKeyNode;
};

} // namespace MapUtils

MAP_TEMPLINST
struct Map
{
    static_assert( std::is_nothrow_destructible <keyType>::value == true );
    static_assert( std::is_nothrow_destructible <valueType>::value == true );

    // Make templates friends of each-other.
    template <typename TWC, typename, MemoryAllocator, CompareCompatibleComparator <TWC, TWC>, typename> friend struct Map;

    typedef MapUtils::Node <keyType, valueType> Node;

    inline Map( void )
        noexcept(nothrow_constructible_from <allocatorType>)
        requires ( constructible_from <allocatorType> )
    {
        // Really nothing to do, I swear!
        return;
    }

private:
    struct key_value
    {
        keyType key;
        valueType value;
    };

public:
    inline Map( std::initializer_list <key_value> elems )
    {
        for ( const key_value& kv : elems )
        {
            this->Set( kv.key, kv.value );
        }
    }

private:
    typedef AVLTree <typename Node::template avlKeyNodeDispatcher <comparatorType>> MapAVLTree;

    static AINLINE void dismantle_node( Map *refMem, Node *theNode ) noexcept
    {
        eir::dyn_del_struct <Node> ( refMem->data.opt(), refMem, theNode );
    }

    static AINLINE void release_nodes( Map *refMem, MapAVLTree& workTree ) noexcept
    {
        // Clear all allocated Nodes.
        while ( AVLNode *avlSomeNode = workTree.GetRootNode() )
        {
            Node *someNode = AVL_GETITEM( Node, avlSomeNode, sortedByKeyNode );

            workTree.RemoveByNodeFast( avlSomeNode );

            dismantle_node( refMem, someNode );
        }
    }

    template <typename cKeyType, typename cValueType> //requires ( constructible_from <Node, cKeyType, cValueType> )
    static AINLINE Node* NewNode( Map *refMem, MapAVLTree& workTree, cKeyType&& key, cValueType&& value )
    {
        Node *newNode = eir::dyn_new_struct <Node, exceptMan> ( refMem->data.opt(), refMem, std::forward <cKeyType> ( key ), std::forward <cValueType> ( value ) );

        try
        {
            // Link this node into our system.
            workTree.Insert( &newNode->sortedByKeyNode );
        }
        catch( ... )
        {
            // Could receive an exception if the comparator of Node does throw.
            eir::dyn_del_struct <Node> ( refMem->data.opt(), refMem, newNode );

            throw;
        }

        return newNode;
    }

    template <typename otherNodeType, typename otherAVLTreeType>
    static AINLINE MapAVLTree clone_key_tree( Map *refMem, otherAVLTreeType& right )
    {
        // We know that there cannot be two-same-value nodes, so we skip checking nodestacks.
        MapAVLTree newTree;

        typename otherAVLTreeType::diff_node_iterator iter( right );

        try
        {
            while ( !iter.IsEnd() )
            {
                otherNodeType *rightNode = AVL_GETITEM( otherNodeType, iter.Resolve(), sortedByKeyNode );

                NewNode( refMem, newTree, rightNode->key, rightNode->value );

                iter.Increment();
            }
        }
        catch( ... )
        {
            // Have to release all items again because one of them did not cooperate.
            release_nodes( refMem, newTree );

            throw;
        }

        return newTree;
    }

public:
    template <typename... Args> requires ( constructible_from <allocatorType, Args...> )
    inline Map( constr_with_alloc _, Args&&... allocArgs )
        noexcept(nothrow_constructible_from <allocatorType, Args...>)
        : data( std::tuple( MapAVLTree() ), std::tuple( std::forward <Args> ( allocArgs )... ) )
    {
        return;
    }

    inline Map( const Map& right )
        : data( std::tuple( clone_key_tree <Node> ( this, right.data.avlKeyTree ) ), std::tuple() )
    {
        return;
    }

    template <typename otherAllocatorType>
    inline Map( const Map <keyType, valueType, otherAllocatorType, comparatorType>& right )
        : data( std::tuple( clone_key_tree <typename Map <keyType, valueType, otherAllocatorType, comparatorType>::Node> ( this, right.data.avlKeyTree ) ), std::tuple() )
    {
        return;
    }

    template <MemoryAllocator otherAllocType, typename... otherMapArgs>
        requires ( nothrow_constructible_from <allocatorType, otherAllocType&&> )
    inline Map( Map <keyType, valueType, otherAllocType, comparatorType, otherMapArgs...>&& right ) noexcept
        : data( std::move( right.data.opt() ) )
    {
        this->data.avlKeyTree = std::move( right.data.avlKeyTree );
    }

    inline ~Map( void )
    {
        release_nodes( this, this->data.avlKeyTree );
    }

    inline Map& operator = ( const Map& right )
    {
        // Clone the thing.
        MapAVLTree newTree = clone_key_tree <Node> ( this, right.data.avlKeyTree );

        // Release the old.
        release_nodes( this, this->data.avlKeyTree );

        this->data.avlKeyTree = std::move( newTree );

        // TODO: think about copying the allocator aswell.

        return *this;
    }

    template <typename otherAllocatorType>
    inline Map& operator = ( const Map <keyType, valueType, otherAllocatorType, comparatorType>& right )
    {
        // Clone the thing.
        MapAVLTree newTree =
            clone_key_tree <typename Map <keyType, valueType, otherAllocatorType, comparatorType>::Node> ( this, right.data.avlKeyTree );

        // Release the old.
        release_nodes( this, this->data.avlKeyTree );

        this->data.avlKeyTree = std::move( newTree );

        // TODO: think about copying the allocator aswell.

        return *this;
    }

    template <MemoryAllocator otherAllocType, typename... otherMapArgs>
        requires ( std::is_nothrow_assignable <allocatorType&, otherAllocType&&>::value )
    inline Map& operator = ( Map <keyType, valueType, otherAllocType, comparatorType, otherMapArgs...>&& right ) noexcept
    {
        // Release the old.
        release_nodes( this, this->data.avlKeyTree );

        this->data.opt() = std::move( right.data.opt() );

        // We need to manually move over field values.
        this->data.avlKeyTree = std::move( right.data.avlKeyTree );

        return *this;
    }

    // *** Management methods.

    // Sets a new value to the tree.
    // Overrides any Node that previously existed.
    inline void Set( const keyType& key, valueType value )
    {
        // Check if such a node exists.
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindNode( key ) )
        {
            Node *existingNode = AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );

            existingNode->value = std::move( value );
            return;
        }

        // We must create a new node.
        NewNode( this, this->data.avlKeyTree, key, std::move( value ) );
    }

    inline void Set( keyType&& key, valueType value )
    {
        // Check if such a node exists.
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindNode( key ) )
        {
            Node *existingNode = AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );

            existingNode->value = std::move( value );
            return;
        }

        // We must create a new node.
        NewNode( this, this->data.avlKeyTree, std::move( key ), std::move( value ) );
    }

    // Removes a specific node that was previously found.
    // The code must make sure that the node really belongs to this tree.
    inline void RemoveNode( Node *theNode ) noexcept
    {
        this->data.avlKeyTree.RemoveByNodeFast( &theNode->sortedByKeyNode );

        dismantle_node( this, theNode );
    }

    // Erases any Node by key.
    template <typename queryType> requires ( CompareCompatibleComparator <comparatorType, keyType, queryType> )
    inline void RemoveByKey( const queryType& key ) noexcept(NothrowCompareCompatibleComparator <comparatorType, keyType, queryType>)
    {
        AVLNode *avlExistingNode = this->data.avlKeyTree.FindNode( key );

        if ( avlExistingNode == nullptr )
            return;

        // Remove it.
        Node *existingNode = AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );

        RemoveNode( existingNode );
    }

    // Clears all keys and values from this Map.
    inline void Clear( void ) noexcept
    {
        release_nodes( this, this->data.avlKeyTree );
    }

    // Returns the amount of keys/values inside this Map.
    inline size_t GetKeyValueCount( void ) const noexcept
    {
        size_t count = 0;

        typename MapAVLTree::diff_node_iterator iter( this->data.avlKeyTree );

        while ( !iter.IsEnd() )
        {
            count++;

            iter.Increment();
        }

        return count;
    }

    // Returns the minimum key of this map.
    inline Node* GetMinimumByKey( void ) noexcept
    {
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.GetSmallestNode() )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    inline const Node* GetMinimumByKey( void ) const noexcept
    {
        if ( const AVLNode *avlExistingNode = this->data.avlKeyTree.GetSmallestNode() )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    // Returns the maximum key of this map.
    inline Node* GetMaximumByKey( void ) noexcept
    {
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.GetBiggestNode() )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    inline const Node* GetMaximumByKey( void ) const noexcept
    {
        if ( const AVLNode *avlExistingNode = this->data.avlKeyTree.GetBiggestNode() )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename queryType> requires ( CompareCompatibleComparator <comparatorType, keyType, queryType> )
    inline Node* Find( const queryType& key ) noexcept(NothrowCompareCompatibleComparator <comparatorType, keyType, queryType> )
    {
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindNode( key ) )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename queryType> requires ( CompareCompatibleComparator <comparatorType, keyType, queryType> )
    inline const Node* Find( const queryType& key ) const noexcept(NothrowCompareCompatibleComparator <comparatorType, keyType, queryType> )
    {
        if ( const AVLNode *avlExistingNode = this->data.avlKeyTree.FindNode( key ) )
        {
            return AVL_GETITEM( constNode, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    // Special finding function that uses a comparison function instead of mere "is_less_than".
    template <typename comparisonCallbackType> requires ( std::is_invocable_r <eCompResult, comparisonCallbackType, Node*>::value )
    inline Node* FindByCriteria( const comparisonCallbackType& cb ) noexcept(std::is_nothrow_invocable_r <eCompResult, comparisonCallbackType, Node*>::value)
    {
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindAnyNodeByCriteria(
            [&]( AVLNode *left ) -> eCompResult
        {
            Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename comparisonCallbackType> requires ( std::is_invocable_r <eCompResult, comparisonCallbackType, const Node*>::value )
    inline const Node* FindByCriteria( const comparisonCallbackType& cb ) const noexcept(std::is_nothrow_invocable_r <eCompResult, comparisonCallbackType, const Node*>::value)
    {
        if ( const AVLNode *avlExistingNode = this->data.avlKeyTree.FindAnyNodeByCriteria(
            [&]( const AVLNode *left ) -> eCompResult
        {
            const Node *leftNode = AVL_GETITEM( constNode, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( constNode, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename comparisonCallbackType> requires ( std::is_invocable_r <eCompResult, comparisonCallbackType, Node*>::value )
    inline Node* FindMinimumByCriteria( const comparisonCallbackType& cb ) noexcept(std::is_nothrow_invocable_r <eCompResult, comparisonCallbackType, Node*>::value)
    {
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindMinimumNodeByCriteria(
            [&]( AVLNode *left ) -> eCompResult
        {
            Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename comparisonCallbackType> requires ( std::is_invocable_r <eCompResult, comparisonCallbackType, const Node*>::value )
    inline const Node* FindMinimumByCriteria( const comparisonCallbackType& cb ) const noexcept(std::is_nothrow_invocable_r <eCompResult, comparisonCallbackType, const Node*>::value)
    {
        if ( const AVLNode *avlExistingNode = this->data.avlKeyTree.FindMinimumNodeByCriteria(
            [&]( const AVLNode *left ) -> eCompResult
        {
            const Node *leftNode = AVL_GETITEM( constNode, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( constNode, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename comparisonCallbackType> requires ( std::is_invocable_r <eCompResult, comparisonCallbackType, Node*>::value )
    inline Node* FindMaximumByCriteria( const comparisonCallbackType& cb ) noexcept(std::is_nothrow_invocable_r <eCompResult, comparisonCallbackType, Node*>::value)
    {
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindMaximumNodeByCriteria(
            [&]( AVLNode *left ) -> eCompResult
        {
            Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename comparisonCallbackType> requires ( std::is_invocable_r <eCompResult, comparisonCallbackType, const Node*>::value )
    inline const Node* FindMaximumByCriteria( const comparisonCallbackType& cb ) const noexcept(std::is_nothrow_invocable_r <eCompResult, comparisonCallbackType, const Node*>::value)
    {
        if ( const AVLNode *avlExistingNode = this->data.avlKeyTree.FindMaximumNodeByCriteria(
            [&]( const AVLNode *left ) -> eCompResult
        {
            const Node *leftNode = AVL_GETITEM( constNode, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( constNode, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename queryType> requires ( CompareCompatibleComparator <comparatorType, keyType, queryType> )
    inline Node* FindJustAbove( const queryType& key ) noexcept(NothrowCompareCompatibleComparator <comparatorType, keyType, queryType>)
    {
        if ( AVLNode *avlJustAboveNode = this->data.avlKeyTree.GetJustAboveNode( key ) )
        {
            return AVL_GETITEM( Node, avlJustAboveNode, sortedByKeyNode );
        }
        return nullptr;
    }

    template <typename queryType> requires ( CompareCompatibleComparator <comparatorType, keyType, queryType> )
    inline const Node* FindJustAbove( const queryType& key ) const noexcept(NothrowCompareCompatibleComparator <comparatorType, keyType, queryType>)
    {
        if ( const AVLNode *avlJustAboveNode = this->data.avlKeyTree.GetJustAboveNode( key ) )
        {
            return AVL_GETITEM( constNode, avlJustAboveNode, sortedByKeyNode );
        }
        return nullptr;
    }

    template <typename queryType>
        requires ( CompareCompatibleComparator <comparatorType, keyType, queryType> && constructible_from <valueType> )
    inline valueType FindOrDefault( const queryType& key ) noexcept(nothrow_constructible_from <valueType> && NothrowCompareCompatibleComparator <comparatorType, keyType, valueType>)
    {
        if ( auto *findNode = this->Find( key ) )
        {
            return findNode->GetValue();
        }

        return valueType();
    }

    // Returns true if there is nothing inside this Map/the AVL tree.
    inline bool IsEmpty( void ) const noexcept
    {
        return ( this->data.avlKeyTree.GetRootNode() == nullptr );
    }

    MAKE_SETMAP_ITERATOR( iterator, Map, Node, sortedByKeyNode, data.avlKeyTree, MapAVLTree );
    typedef const Node constNode;
    typedef const Map constMap;
    MAKE_SETMAP_ITERATOR( const_iterator, constMap, constNode, sortedByKeyNode, data.avlKeyTree, MapAVLTree );

    // Walks through all nodes of this tree.
    template <typename callbackType>
    inline void WalkNodes( const callbackType& cb )
    {
        iterator iter( this );

        while ( !iter.IsEnd() )
        {
            Node *curNode = iter.Resolve();

            cb( curNode );

            iter.Increment();
        }
    }

    // Support for the standard C++ for-each walking.
    struct end_std_iterator {};
    struct std_iterator
    {
        AINLINE std_iterator( iterator&& right ) : iter( std::move( right ) )
        {
            return;
        }

        AINLINE bool operator != ( const end_std_iterator& right ) const    { return iter.IsEnd() == false; }

        AINLINE std_iterator& operator ++ ( void )
        {
            iter.Increment();
            return *this;
        }
        AINLINE Node* operator * ( void )
        {
            return iter.Resolve();
        }

    private:
        iterator iter;
    };
    AINLINE std_iterator begin( void )              { return std_iterator( iterator( *this ) ); }
    AINLINE end_std_iterator end( void ) const      { return end_std_iterator(); }

    struct const_std_iterator
    {
        AINLINE const_std_iterator( const_iterator&& right ) : iter( std::move( right ) )
        {
            return;
        }

        AINLINE bool operator != ( const end_std_iterator& right ) const    { return iter.IsEnd() == false; }

        AINLINE const_std_iterator& operator ++ ( void )
        {
            iter.Increment();
            return *this;
        }
        AINLINE const Node* operator * ( void )
        {
            return iter.Resolve();
        }

    private:
        const_iterator iter;
    };
    AINLINE const_std_iterator begin( void ) const  { return const_std_iterator( const_iterator( *this ) ); }

    // Nice helpers using operators.
    template <std::convertible_to <keyType> queryType>
        requires ( std::is_default_constructible <valueType>::value && CompareCompatibleComparator <comparatorType, keyType, queryType> )
    inline valueType& operator [] ( queryType&& key )
    {
        if ( Node *findNode = this->Find( key ) )
        {
            return findNode->value;
        }

        return NewNode( this, this->data.avlKeyTree, std::forward <queryType> ( key ), std::tuple() )->value;
    }

    // Similar to the operator [] but it allows for non-default-construction of
    // a value under a not-present key.
    template <std::convertible_to <keyType> queryType, typename... constrArgs>
        requires ( std::is_constructible <valueType, constrArgs...>::value && CompareCompatibleComparator <comparatorType, keyType, queryType> )
    inline valueType& get( queryType&& key, constrArgs&&... cargs )
    {
        if ( Node *findNode = this->Find( key ) )
        {
            return findNode->value;
        }
        
        return NewNode( this, this->data.avlKeyTree, std::forward <queryType> ( key ), std::tuple <constrArgs&&...> ( std::forward <constrArgs> ( cargs )... ) )->value;
    }

    // Returns the node from a reference-to-value, in constant time.
    inline Node* from_value_ref( valueType& valueRef )
    {
#ifdef _DEBUG
        // Check whether the value-ref does reside within our system.
        bool has_found_node = false;

        for ( Node *node : *this )
        {
            if ( &node->value == &valueRef )
            {
                has_found_node = true;
                break;
            }
        }
        FATAL_ASSERT( has_found_node == true );
#endif //_DEBUG

        Node *backlink_node = LIST_GETITEM( Node, &valueRef, value );

        return backlink_node;
    }

    template <typename otherAllocatorType>
    inline bool operator == ( const Map <keyType, valueType, otherAllocatorType, comparatorType>& right ) const
    {
        typedef typename Map <keyType, valueType, otherAllocatorType, comparatorType>::Node otherNodeType;

        return EqualsAVLTrees( this->data.avlKeyTree, right.data.avlKeyTree,
            [&]( const AVLNode *left, const AVLNode *right )
        {
            // If any key is different then we do not match.
            const Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );
            const otherNodeType *rightNode = AVL_GETITEM( otherNodeType, right, sortedByKeyNode );

            if ( leftNode->GetKey() != rightNode->GetKey() )
            {
                return false;
            }

            // If any value is different then we do not match.
            if ( leftNode->GetValue() != rightNode->GetValue() )
            {
                return false;
            }

            return true;
        });
    }

    template <typename otherAllocatorType>
    inline bool operator != ( const Map <keyType, valueType, otherAllocatorType, comparatorType>& right ) const
    {
        return !( operator == ( right ) );
    }

private:
    struct mutable_data
    {
        AINLINE mutable_data( void ) = default;
        AINLINE mutable_data( MapAVLTree tree ) : avlKeyTree( std::move( tree ) )
        {
            return;
        }
        AINLINE mutable_data( mutable_data&& right ) = default;

        AINLINE mutable_data& operator = ( mutable_data&& right ) = default;

        mutable MapAVLTree avlKeyTree;
    };

    empty_opt <allocatorType, mutable_data> data;
};

// Traits.
template <typename>
struct is_map : std::false_type
{};
template <typename... mapArgs>
struct is_map <Map <mapArgs...>> : std::true_type
{};
template <typename... mapArgs>
struct is_map <const Map <mapArgs...>> : std::true_type
{};
template <typename... mapArgs>
struct is_map <Map <mapArgs...>&> : std::true_type
{};
template <typename... mapArgs>
struct is_map <Map <mapArgs...>&&> : std::true_type
{};
template <typename... mapArgs>
struct is_map <const Map <mapArgs...>&> : std::true_type
{};
template <typename... mapArgs>
struct is_map <const Map <mapArgs...>&&> : std::true_type
{};

} // namespace eir

#endif //_EIR_MAP_HEADER_