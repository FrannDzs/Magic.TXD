/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Set.h
*  PURPOSE:     Optimized Set implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// In the FileSystem presence data manager we need to determine an ordered set
// of locations that could be used for temporary data storage. Thus we need to
// offer a "Set" type, which is just a value-based Map.

#ifndef _EIR_SDK_SET_HEADER_
#define _EIR_SDK_SET_HEADER_

#include "eirutils.h"
#include "AVLTree.h"
#include "MacroUtils.h"
#include "MetaHelpers.h"

#include "AVLTree.helpers.h"
#include "avlsetmaputil.h"

#include <type_traits>
#include <array>

namespace eir
{

#define SET_TEMPLINST \
template <typename valueType, MemoryAllocator allocatorType, CompareCompatibleComparator <valueType, valueType> comparatorType = SetDefaultComparator, typename exceptMan = eir::DefaultExceptionManager>
#define SET_TEMPLARGS \
template <typename TWC, MemoryAllocator, CompareCompatibleComparator <TWC, TWC>, typename>

// Necessary forward declaration for the SetUtils.
SET_TEMPLARGS
struct Set;

namespace SetUtils
{

// Node inside the Set.
template <typename valueType>
struct ValueNode
{
    SET_TEMPLARGS friend struct Set;
    template <typename structType, typename, MemoryAllocator subAllocatorType, typename... Args> friend structType* eir::dyn_new_struct( subAllocatorType&, void*, Args&&... );
    template <typename structType, MemoryAllocator subAllocatorType> friend void eir::dyn_del_struct( subAllocatorType&, void*, structType* ) noexcept(std::is_nothrow_destructible <structType>::value);

    // Get rid of the default things.
    inline ValueNode( const ValueNode& right ) = delete;
    inline ValueNode( ValueNode&& right ) = delete;

    inline ValueNode& operator = ( const ValueNode& ) = delete;
    inline ValueNode& operator = ( ValueNode&& right ) = delete;

    // The public access stuff.
    inline const valueType& GetValue( void ) const noexcept
    {
        return this->value;
    }

    // Helper for AVLTree.
    template <CompareCompatibleComparator <valueType, valueType> comparatorType>
    struct avlKeyNodeDispatcher
    {
        template <typename subFirstValueType, typename subSecondValueType> requires CompareCompatibleComparator <comparatorType, subFirstValueType, subSecondValueType>
        static AINLINE eCompResult compare_values( const subFirstValueType& left, const subSecondValueType& right ) noexcept(NothrowCompareCompatibleComparator <comparatorType, subFirstValueType, subSecondValueType>)
        {
            if constexpr ( CompareCompatibleRegularComparator <comparatorType, subFirstValueType, subSecondValueType> )
            {
                return ( comparatorType::compare_values( left, right ) );
            }
            else
            {
                return flip_comp_result( comparatorType::compare_values( right, left ) );
            }
        }

        static inline eCompResult CompareNodes( const AVLNode *left, const AVLNode *right ) noexcept(std::is_nothrow_invocable <decltype(compare_values <valueType, valueType>), valueType, valueType>::value)
        {
            const ValueNode *leftNode = AVL_GETITEM( ValueNode, left, sortedByValueNode );
            const ValueNode *rightNode = AVL_GETITEM( ValueNode, right, sortedByValueNode );

            return compare_values( leftNode->value, rightNode->value );
        }

        template <typename subValueType> requires CompareCompatibleComparator <comparatorType, valueType, subValueType>
        static inline eCompResult CompareNodeWithValue( const AVLNode *left, const subValueType& right ) noexcept(std::is_nothrow_invocable <decltype(compare_values <valueType, subValueType>), valueType, subValueType>::value)
        {
            const ValueNode *leftNode = AVL_GETITEM( ValueNode, left, sortedByValueNode );

            return compare_values( leftNode->value, right );
        }
    };

private:
    // Not available to things that just use Set.
    // But the friend class does have access.
    template <typename... constrArgs> requires ( std::is_constructible <valueType, constrArgs&&...>::value )
    inline ValueNode( constrArgs&&... cargs )
        : value( std::forward <constrArgs> ( cargs )... )
    {
        return;
    }
    inline ~ValueNode( void ) noexcept = default;

    valueType value;

    AVLNode sortedByValueNode;
};

} // namespace SetUtils

typedef GenericDefaultComparator SetDefaultComparator;

SET_TEMPLINST
struct Set
{
    static_assert( std::is_nothrow_destructible <valueType>::value == true );

    typedef SetUtils::ValueNode <valueType> Node;

    // Make templates friends of each-other.
    template <typename TWC, MemoryAllocator, CompareCompatibleComparator <TWC, TWC>, typename> friend struct Set;

    inline Set( void )
        noexcept(nothrow_constructible_from <allocatorType>)
        requires ( constructible_from <allocatorType> )
    {
        // Really nothing to do, I swear!
        return;
    }

private:
    typedef AVLTree <typename Node::template avlKeyNodeDispatcher <comparatorType>> SetAVLTree;

    static AINLINE void dismantle_node( Set *refMem, Node *theNode ) noexcept
    {
        eir::dyn_del_struct <Node> ( refMem->data.opt(), refMem, theNode );
    }

    static AINLINE void release_nodes( Set *refMem, SetAVLTree& workTree ) noexcept
    {
        // Clear all allocated Nodes.
        while ( AVLNode *avlSomeNode = workTree.GetRootNode() )
        {
            Node *someNode = AVL_GETITEM( Node, avlSomeNode, sortedByValueNode );

            workTree.RemoveByNodeFast( avlSomeNode );

            dismantle_node( refMem, someNode );
        }
    }

    template <typename... constrArgs>
    static AINLINE Node* NewNode( Set *refMem, SetAVLTree& workTree, constrArgs&&... cargs )
    {
        Node *newNode = eir::dyn_new_struct <Node, exceptMan> ( refMem->data.opt(), refMem, std::forward <constrArgs> ( cargs )... );

        try
        {
            // Link this node into our system.
            workTree.Insert( &newNode->sortedByValueNode );
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
    static AINLINE SetAVLTree clone_value_tree( Set *refMem, otherAVLTreeType& right )
    {
        // We know that there cannot be two-same-value nodes, so we skip checking nodestacks.
        SetAVLTree newTree;

        typename otherAVLTreeType::diff_node_iterator iter( right );

        try
        {
            while ( !iter.IsEnd() )
            {
                otherNodeType *rightNode = AVL_GETITEM( otherNodeType, iter.Resolve(), sortedByValueNode );

                NewNode( refMem, newTree, rightNode->value );

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
    inline Set( constr_with_alloc _, Args&&... allocArgs )
        noexcept(nothrow_constructible_from <allocatorType, Args...>)
        : data( std::tuple( SetAVLTree() ), std::tuple( std::forward <Args> ( allocArgs )... ) )
    {
        return;
    }
    
    template <typename subValueType, size_t N> requires std::convertible_to <subValueType, valueType>
    inline Set( std::array <subValueType, N> values )
    {
        for ( subValueType& value : values )
        {
            this->Insert( std::move( value ) );
        }
    }

    inline Set( const Set& right )
        : data( std::tuple( clone_value_tree <Node> ( this, right.data.avlValueTree ) ), std::tuple() )
    {
        return;
    }

    template <typename otherAllocatorType>
    inline Set( const Set <valueType, otherAllocatorType, comparatorType>& right )
        : data( std::tuple( clone_value_tree <typename Set <valueType, otherAllocatorType, comparatorType>::Node> ( this, right.data.avlValueTree ) ), std::tuple() )
    {
        return;
    }

    template <MemoryAllocator otherAllocType, typename... otherSetArgs>
        requires ( eir::nothrow_constructible_from <allocatorType, otherAllocType&&> )
    inline Set( Set <valueType, otherAllocType, comparatorType, otherSetArgs...>&& right ) noexcept
        : data( std::move( right.data.opt() ) )
    {
        this->data.avlValueTree = std::move( right.data.avlValueTree );
    }

    inline ~Set( void )
    {
        release_nodes( this, this->data.avlValueTree );
    }

    inline Set& operator = ( const Set& right )
    {
        // Clone the thing.
        SetAVLTree newTree = clone_value_tree <Node> ( this, right.data.avlValueTree );

        // Release the old.
        release_nodes( this, this->data.avlValueTree );

        this->data.avlValueTree = std::move( newTree );

        // TODO: think about copying the allocator aswell.

        return *this;
    }

    template <typename otherAllocatorType>
    inline Set& operator = ( const Set <valueType, otherAllocatorType, comparatorType>& right )
    {
        // Clone the thing.
        SetAVLTree newTree =
            clone_value_tree <typename Set <valueType, otherAllocatorType, comparatorType>::Node> ( this, right.data.avlValueTree );

        // Release the old.
        release_nodes( this, this->data.avlValueTree );

        this->data.avlValueTree = std::move( newTree );

        // TODO: think about copying the allocator aswell.

        return *this;
    }

    template <MemoryAllocator otherAllocType, typename... otherSetArgs>
        requires ( std::is_nothrow_assignable <allocatorType&, otherAllocType&&>::value )
    inline Set& operator = ( Set <valueType, otherAllocType, comparatorType, otherSetArgs...>&& right ) noexcept
    {
        // Release the old.
        release_nodes( this, this->data.avlValueTree );

        this->data.opt() = std::move( right.data.opt() );

        // Need to manually move over fields.
        this->data.avlValueTree = std::move( right.data.avlValueTree );

        return *this;
    }

    template <std::convertible_to <valueType> subValueType, size_t N>
    AINLINE Set& operator = ( std::array <subValueType, N> arr )
    {
        release_nodes( this, this->data.avlValueTree );

        for ( subValueType& val : arr )
        {
            this->Insert( std::move( val ) );
        }

        return *this;
    }

    // *** Management methods.

    // Sets a new value to the tree.
    // Overrides any Node that previously existed.
    inline void Insert( const valueType& value )
    {
        // Check if such a node exists.
        if ( AVLNode *avlExistingNode = this->data.avlValueTree.FindNode( value ) )
        {
            return;
        }

        // We must create a new node.
        NewNode( this, this->data.avlValueTree, value );
    }
    
    template <typename insertValueType> 
        requires ( CompareCompatibleComparator <comparatorType, valueType, insertValueType> && std::is_constructible <valueType, insertValueType&&>::value && ( std::is_same <valueType, insertValueType>::value == false ) )
    inline void Insert( insertValueType&& insval )
    {
        // Check if such a node exists.
        if ( AVLNode *avlExistingNode = this->data.avlValueTree.FindNode( insval ) )
        {
            return;
        }

        // We must create a new node.
        NewNode( this, this->data.avlValueTree, std::move( insval ) );
    }

    inline void Insert( valueType&& value )
    {
        // Check if such a node exists.
        if ( AVLNode *avlExistingNode = this->data.avlValueTree.FindNode( value ) )
        {
            return;
        }

        // We must create a new node.
        NewNode( this, this->data.avlValueTree, std::move( value ) );
    }

    // Removes a node from visibility, moves the value out of internal structures and returns it to
    // the caller. Should be used for final removal, NOT for updates of the key!
    inline valueType ExtractNode( const Node *theNode )
        noexcept(nothrow_constructible_from <valueType, valueType&&>)
        requires ( constructible_from <valueType, valueType&&> )
    {
        Node *mutableNode = const_cast <Node*> ( theNode );

        this->data.avlValueTree.RemoveByNodeFast( &mutableNode->sortedByValueNode );

        valueType val( std::move( mutableNode->value ) );

        dismantle_node( this, mutableNode );

        return val;
    }

    // Enters update-mode for a node by pointer.
    inline valueType& BeginNodeUpdate( const Node *theNode ) noexcept
    {
        Node *mutableNode = const_cast <Node*> ( theNode );

        this->data.avlValueTree.RemoveByNodeFast( &mutableNode->sortedByValueNode );

        return mutableNode->value;
    }

    // Commits an update to a node.
    inline void EndNodeUpdate( const Node *theNode ) noexcept(NothrowCompareCompatibleComparator <comparatorType, valueType, valueType>)
    {
#ifdef _DEBUG
        // Here we do not check by value but by the node pointer.
        FATAL_ASSERT( this->data.avlValueTree.IsNodeInsideTree( (AVLNode*)&theNode->sortedByValueNode ) == false );
#endif //_DEBUG

        Node *mutableNode = const_cast <Node*> ( theNode );

        // Check that we do not have a node of that value previously inside of our tree.
        // If we do then we can remove our node.
        if ( this->data.avlValueTree.FindNode( mutableNode->value ) == nullptr )
        {
            this->data.avlValueTree.Insert( &mutableNode->sortedByValueNode );
        }
        else
        {
            dismantle_node( this, mutableNode );
        }
    }

    // Removes a specific node that was previously found.
    // The code must make sure that the node really belongs to this tree.
    inline void RemoveNode( Node *theNode ) noexcept
    {
        this->data.avlValueTree.RemoveByNodeFast( &theNode->sortedByValueNode );

        dismantle_node( this, theNode );
    }

    // Erases any Node by value.
    template <typename queryType> requires ( CompareCompatibleComparator <comparatorType, valueType, queryType> )
    inline bool Remove( const queryType& value ) noexcept(NothrowCompareCompatibleComparator <comparatorType, valueType, queryType>)
    {
        AVLNode *avlExistingNode = this->data.avlValueTree.FindNode( value );

        if ( avlExistingNode == nullptr )
            return false;

        // Remove it.
        Node *existingNode = AVL_GETITEM( Node, avlExistingNode, sortedByValueNode );

        RemoveNode( existingNode );

        return true;
    }

    // Clears all values from this Set.
    inline void Clear( void ) noexcept
    {
        release_nodes( this, this->data.avlValueTree );
    }

    // Returns the amount of values inside this Set.
    inline size_t GetValueCount( void ) const noexcept
    {
        size_t count = 0;

        typename SetAVLTree::diff_node_iterator iter( this->data.avlValueTree );

        while ( !iter.IsEnd() )
        {
            count++;

            iter.Increment();
        }

        return count;
    }

    inline Node* GetAny( void ) noexcept
    {
        if ( AVLNode *avlAnyNode = this->data.avlValueTree.GetRootNode() )
        {
            return AVL_GETITEM( Node, avlAnyNode, sortedByValueNode );
        }

        return nullptr;
    }

    inline const Node* GetAny( void ) const noexcept
    {
        return const_cast <Set*> ( this )->GetAny();
    }

    inline Node* GetMinimum( void ) noexcept
    {
        if ( AVLNode *avlMinimumNode = this->data.avlValueTree.GetSmallestNode() )
        {
            return AVL_GETITEM( Node, avlMinimumNode, sortedByValueNode );
        }

        return nullptr;
    }

    inline const Node* GetMinimum( void ) const noexcept
    {
        return const_cast <Set*> ( this )->GetMinimum();
    }

    inline Node* GetMaximum( void ) noexcept
    {
        if ( AVLNode *avlMaximumNode = this->data.avlValueTree.GetBiggestNode() )
        {
            return AVL_GETITEM( Node, avlMaximumNode, sortedByValueNode );
        }

        return nullptr;
    }

    inline const Node* GetMaximum( void ) const noexcept
    {
        return const_cast <Set*> ( this )->GetMaximum();
    }

    template <typename queryType> requires CompareCompatibleComparator <comparatorType, valueType, queryType>
    inline Node* Find( const queryType& value ) noexcept(NothrowCompareCompatibleComparator <comparatorType, valueType, queryType>)
    {
        if ( AVLNode *avlExistingNode = this->data.avlValueTree.FindNode( value ) )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByValueNode );
        }

        return nullptr;
    }

    template <typename queryType> requires CompareCompatibleComparator <comparatorType, valueType, queryType>
    inline const Node* Find( const queryType& value ) const noexcept(NothrowCompareCompatibleComparator <comparatorType, valueType, queryType>)
    {
        return const_cast <Set*> ( this )->Find( value );
    }

    template <typename queryType> requires CompareCompatibleComparator <comparatorType, valueType, queryType>
    inline Node* FindJustAboveOrEqual( const queryType& value ) noexcept(NothrowCompareCompatibleComparator <comparatorType, valueType, queryType>)
    {
        if ( AVLNode *avlAboveOrEqual = this->data.avlValueTree.GetJustAboveOrEqualNode( value ) )
        {
            return AVL_GETITEM( Node, avlAboveOrEqual, sortedByValueNode );
        }

        return nullptr;
    }

    template <typename queryType> requires CompareCompatibleComparator <comparatorType, valueType, queryType>
    inline const Node* FindJustAboveOrEqual( const queryType& value ) const noexcept(NothrowCompareCompatibleComparator <comparatorType, valueType, queryType>)
    {
        return const_cast <Set*> ( this )->FindJustAboveOrEqual( value );
    }

    template <typename queryType> requires CompareCompatibleComparator <comparatorType, valueType, queryType>
    inline Node* FindJustAbove( const queryType& value ) noexcept(NothrowCompareCompatibleComparator <comparatorType, valueType, queryType>)
    {
        if ( AVLNode *avlJustAbove = this->data.avlValueTree.GetJustAboveNode( value ) )
        {
            return AVL_GETITEM( Node, avlJustAbove, sortedByValueNode );
        }

        return nullptr;
    }

    template <typename queryType> requires CompareCompatibleComparator <comparatorType, valueType, queryType>
    inline const Node* FindJustAbove( const queryType& value ) const noexcept(NothrowCompareCompatibleComparator <comparatorType, valueType, queryType>)
    {
        return const_cast <Set*> ( this )->FindJustAbove( value );
    }

    // Returns true if there is nothing inside this Set/the AVL tree.
    inline bool IsEmpty( void ) const noexcept
    {
        return ( this->data.avlValueTree.GetRootNode() == nullptr );
    }

    // Itemwise comparison function.
    template <typename itemwiseCallbackType, typename otherAllocatorType, typename... otherArgs>
    inline void ItemwiseComparison( const Set <valueType, otherAllocatorType, comparatorType, otherArgs...>& otherSet, const itemwiseCallbackType&& cb ) const
    {
        // true is in this set, false if in other.

        bool is_left_end = this->IsEmpty();
        bool is_right_end = otherSet.IsEmpty();

        optional_struct_space <valueType> left_value;
        optional_struct_space_init <valueType> left_value_init;
        optional_struct_space <valueType> right_value;
        optional_struct_space_init <valueType> right_value_init;

        if ( is_left_end == false )
        {
            left_value_init = optional_struct_space_init <valueType> ( left_value, this->GetMinimum()->GetValue() );
        }
        if ( is_right_end == false )
        {
            right_value_init = optional_struct_space_init <valueType> ( right_value, otherSet.GetMinimum()->GetValue() );
        }

        while ( is_left_end == false && is_right_end == false )
        {
            eir::eCompResult cmpRes = comparatorType::compare_values( left_value.get(), right_value.get() );

            bool increment_left = false;
            bool increment_right = false;

            if ( cmpRes == eir::eCompResult::LEFT_LESS )
            {
                // There is an unique item in the old set.
                cb( left_value.get(), true );

                increment_left = true;
            }
            else if ( cmpRes == eir::eCompResult::LEFT_GREATER )
            {
                // There is an unique item in the new set.
                cb( right_value.get(), false );

                increment_right = true;
            }
            else
            {
                increment_left = true;
                increment_right = true;
            }

            if ( increment_left )
            {
                const Node *left_next_node = this->FindJustAbove( left_value.get() );

                if ( left_next_node == nullptr )
                {
                    is_left_end = true;
                }
                else
                {
                    left_value.get() = left_next_node->GetValue();
                }
            }

            if ( increment_right )
            {
                const auto *right_next_node = otherSet.FindJustAbove( right_value.get() );

                if ( right_next_node == nullptr )
                {
                    is_right_end = true;
                }
                else
                {
                    right_value.get() = right_next_node->GetValue();
                }
            }
        }

        while ( is_left_end == false )
        {
            cb( left_value.get(), true );

            const Node *left_next_node = this->FindJustAbove( left_value.get() );

            if ( left_next_node == nullptr )
            {
                is_left_end = true;
                break;
            }

            left_value.get() = left_next_node->GetValue();
        }

        while ( is_right_end == false )
        {
            cb( right_value.get(), false );

            const auto *right_next_node = otherSet.FindJustAbove( right_value.get() );

            if ( right_next_node == nullptr )
            {
                is_right_end = true;
                break;
            }

            right_value.get() = right_next_node->GetValue();
        }
    }

    // Public iterator.
    MAKE_SETMAP_ITERATOR( iterator, Set, Node, sortedByValueNode, data.avlValueTree, SetAVLTree );
    typedef const Node constNode;
    typedef const Set constSet;
    MAKE_SETMAP_ITERATOR( const_iterator, constSet, constNode, sortedByValueNode, data.avlValueTree, SetAVLTree );

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

    // Support for standard C++ container for-each loop.
    struct end_std_iterator {};
    struct std_iterator
    {
        AINLINE std_iterator( const_iterator&& right ) : iter( std::move( right ) )
        {
            return;
        }

        AINLINE bool operator != ( const end_std_iterator& ) const      { return iter.IsEnd() == false; }
        AINLINE bool operator != ( const std_iterator& right ) const    { return ( iter.Resolve() != right.iter.Resolve() ); }

        AINLINE std_iterator& operator ++ ( void )
        {
            iter.Increment();
            return *this;
        }
        AINLINE const valueType& operator * ( void ) const      { return iter.Resolve()->GetValue(); }

    private:
        const_iterator iter;
    };
    AINLINE std_iterator begin( void ) const    { return std_iterator( const_iterator( *this ) ); }
    AINLINE end_std_iterator end( void ) const  { return end_std_iterator(); }

    // Nice helpers using operators.
    template <std::convertible_to <valueType> queryType>
        requires ( constructible_from <valueType, queryType> && CompareCompatibleComparator <comparatorType, valueType, queryType> )
    inline valueType& operator [] ( queryType&& key )
    {
        if ( Node *findNode = this->Find( key ) )
        {
            return findNode->value;
        }

        return NewNode( this, this->data.avlValueTree, std::forward <queryType> ( key ) )->value;
    }

    // Similar to the operator [] but it allows for non-default-construction of
    // a value under a not-present key.
    template <std::convertible_to <valueType> queryType, typename... constrArgs>
        requires ( constructible_from <valueType, constrArgs...> && CompareCompatibleComparator <comparatorType, valueType, queryType> )
    inline valueType& get( queryType&& key, constrArgs&&... cargs )
    {
        if ( Node *findNode = this->Find( key ) )
        {
            return findNode->value;
        }
        
        return NewNode( this, this->data.avlValueTree, std::tuple <constrArgs&&...> ( std::forward <constrArgs> ( cargs )... ) )->value;
    }

    // TODO: create a common method between Map and Set so they reuse maximum amount of code.
    template <typename otherAllocatorType>
    inline bool operator == ( const Set <valueType, otherAllocatorType, comparatorType>& right ) const
    {
        typedef typename Set <valueType, otherAllocatorType, comparatorType>::Node otherNodeType;

        return EqualsAVLTrees( this->data.avlValueTree, right.data.avlValueTree,
            [&]( const AVLNode *left, const AVLNode *right )
        {
            // If any value is different then we do not match.
            const Node *leftNode = AVL_GETITEM( Node, left, sortedByValueNode );
            const otherNodeType *rightNode = AVL_GETITEM( otherNodeType, right, sortedByValueNode );

            return ( leftNode->GetValue() == rightNode->GetValue() );
        });
    }

    template <typename otherAllocatorType>
    inline bool operator != ( const Set <valueType, otherAllocatorType, comparatorType>& right ) const
    {
        return !( operator == ( right ) );
    }

private:
    struct mutable_data
    {
        AINLINE mutable_data( void ) = default;
        AINLINE mutable_data( SetAVLTree tree ) : avlValueTree( std::move( tree ) )
        {
            return;
        }
        AINLINE mutable_data( mutable_data&& right ) = default;

        AINLINE mutable_data& operator = ( mutable_data&& right ) = default;

        mutable SetAVLTree avlValueTree;
    };

    empty_opt <allocatorType, mutable_data> data;
};

// Traits.
template <typename>
struct is_set : std::false_type
{};
template <typename... setArgs>
struct is_set <Set <setArgs...>> : std::true_type
{};
template <typename... setArgs>
struct is_set <const Set <setArgs...>> : std::true_type
{};
template <typename... setArgs>
struct is_set <Set <setArgs...>&> : std::true_type
{};
template <typename... setArgs>
struct is_set <Set <setArgs...>&&> : std::true_type
{};
template <typename... setArgs>
struct is_set <const Set <setArgs...>&> : std::true_type
{};
template <typename... setArgs>
struct is_set <const Set <setArgs...>&&> : std::true_type
{};

} // namespace eir

#endif //_EIR_SDK_SET_HEADER_
