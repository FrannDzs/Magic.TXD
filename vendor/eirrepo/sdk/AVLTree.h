/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/AVLTree.h
*  PURPOSE:     AVL-tree implementation (for the native heap allocator)
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _AVL_TREE_IMPLEMENTATION_
#define _AVL_TREE_IMPLEMENTATION_

#include "Exceptions.h"
#include "eirutils.h"
#include "MacroUtils.h"
#include "rwlist.hpp"
#include <algorithm>

// AVL tree is a sorted data structure that guarantees logarithmic-in-size add and
// remove operations while keeping item order intact. We realized that we needed
// this struct because the NativeHeapAllocator is being issued with random memory
// request sizes and we ought to decide fast in which free slot allocation might be
// possible.
// Note that this is NOT a naive implementation of AVL tree.
// Also used in the Map implementation.

// Useful to fetch an actual struct if AVLNode is a struct-member.
#define AVL_GETITEM( type, nodePtr, nodeMemb )      ( (type*)( (char*)nodePtr - offsetof(type, nodeMemb) ) )

// Has to be less-than comparable, in a total order.
struct AVLNode
{
    // TODO: find a way to segregate the AVL default fields from the nodestack field so that
    //  it is easier to maintain this thing.

    // We keep the default constructor while deleting the obvious culprits for safety.
    inline AVLNode( void ) = default;
    inline AVLNode( const AVLNode& ) = delete;
    inline AVLNode( AVLNode&& ) = delete;
    inline ~AVLNode( void ) = default;

    inline AVLNode& operator = ( const AVLNode& ) = delete;
    inline AVLNode& operator = ( AVLNode&& ) = delete;

    AVLNode *parent;            // if parent points to itself, then we are part of a node stack.
    union
    {
        struct
        {
            size_t height;
            AVLNode *owned_nodestack;   // if we have a nodestack, then this member points at it.
            AVLNode *left, *right;      // AVL structure default members.
        };
        struct
        {
            AVLNode *nodestack_owner;   // if nodestack member, then this points to the nodestack holder.
            AVLNode *prev, *next;       // double-linked list in case of node stack member.
        };
    };
};

template <typename dispatcherType, typename valueType>
concept AVLNodeDispatcherNodeLeftComparableWith =
    requires ( const AVLNode *L, const valueType& R ) { (eir::eCompResult)dispatcherType::CompareNodeWithValue( L, R ); };

template <typename dispatcherType, typename valueType>
concept AVLNodeDispatcherNodeRightComparableWith =
    requires ( const valueType& L, const AVLNode *R ) { (eir::eCompResult)dispatcherType::CompareNodeWithValue( L, R ); };

template <typename dispatcherType, typename valueType>
concept AVLNodeDispatcherNodeComparableWith =
    AVLNodeDispatcherNodeLeftComparableWith <dispatcherType, valueType> ||
    AVLNodeDispatcherNodeRightComparableWith <dispatcherType, valueType>;

template <typename dispatcherType>
concept AVLNodeDispatcher =
    std::is_invocable_r <eir::eCompResult, decltype(dispatcherType::CompareNodes), const AVLNode*, const AVLNode*>::value;

template <AVLNodeDispatcher dispatcherType, bool nodestackInsertOrAppend = true>
struct AVLTree
{
    // This class is NOT THREAD-SAFE.

    // other (optional) methods:
    // - template <typename valueType> static eir::eCompResult CompareNodeWithValue( const AVLNode *left, const valueType& right )

    inline AVLTree( void ) noexcept
    {
        this->root = nullptr;
    }

    inline AVLTree( const AVLTree& right ) = delete;
    inline AVLTree( AVLTree&& right ) noexcept
    {
        this->root = right.root;

        right.root = nullptr;
    }

    inline ~AVLTree( void )
    {
        // Cleanup is done by all the systems that created node memory themselves.
        // We do never allocate memory in this data structure anyway.
        return;
    }

    inline AVLTree& operator = ( const AVLTree& right ) = delete;

    // WARNING: a friendly reminder, that this operator trashes any previous tree nodes.
    // so make sure to empty the tree (dealloc nodes, etc) before moving to it!
    inline AVLTree& operator = ( AVLTree&& right ) noexcept
    {
        this->root = right.root;

        right.root = nullptr;

        return *this;
    }

private:
    static AINLINE size_t height_func( size_t leftHeight, size_t rightHeight ) noexcept
    {
        // So that we can choose a specific implementation of max function.
        return std::max( leftHeight, rightHeight );
    }

    static AINLINE size_t calc_height_of_node( AVLNode *firstNode, AVLNode *secondNode ) noexcept
    {
        // Calculate the maximum number of nodes that are on a path to the bottom in a binary tree.

        if ( firstNode == nullptr && secondNode == nullptr )
        {
            return 0;
        }

        size_t height_of_nodes_underneath;

        if ( firstNode == nullptr )
        {
            height_of_nodes_underneath = secondNode->height;
        }
        else if ( secondNode == nullptr )
        {
            height_of_nodes_underneath = firstNode->height;
        }
        else
        {
            height_of_nodes_underneath = height_func( firstNode->height, secondNode->height );
        }

        // We include the node that starts of our child.
        // THIS IS NOT THE ROOT NODE THAT IS INCLUDED.
        return ( 1 + height_of_nodes_underneath );
    }

    AINLINE AVLNode** get_node_src_ptr( AVLNode *parent, AVLNode *node ) noexcept
    {
        AVLNode **nodePtr;

        if ( parent )
        {
            if ( parent->left == node )
            {
                nodePtr = &parent->left;
            }
            else if ( parent->right == node )
            {
                nodePtr = &parent->right;
            }
            else
            {
                FATAL_ABORT();
            }
        }
        else
        {
            if ( this->root != node )
            {
                FATAL_ABORT();
            }

            nodePtr = &this->root;
        }

        return nodePtr;
    }

    AINLINE void rotate( AVLNode *node, AVLNode *moveToUsNeighbor, bool leftTrueRightFalse ) noexcept
    {
        // Both node and moveToUsNeighbor must not be nullptr.
        FATAL_ASSERT( node != nullptr );
        FATAL_ASSERT( moveToUsNeighbor != nullptr );

        // Set the new direct node which would be empty if the tree performed rotation.
        AVLNode *newDirectNodeChild;
        AVLNode *otherDirectChild;
        AVLNode *tuggedWithNode;

        if ( leftTrueRightFalse )
        {
            FATAL_ASSERT( node->right == moveToUsNeighbor );

            // If we move to the left, then the left subtree of our right child is bigger than us,
            // thus is becomes our right subtree.

            newDirectNodeChild = moveToUsNeighbor->left;

            node->right = newDirectNodeChild;

            // Do not forget to update the direct relationship!
            moveToUsNeighbor->left = node;

            otherDirectChild = node->left;

            tuggedWithNode = moveToUsNeighbor->right;
        }
        else
        {
            FATAL_ASSERT( node->left == moveToUsNeighbor );

            newDirectNodeChild = moveToUsNeighbor->right;

            node->left = newDirectNodeChild;

            moveToUsNeighbor->right = node;

            otherDirectChild = node->right;

            tuggedWithNode = moveToUsNeighbor->left;
        }

        // Update the parents.
        if ( newDirectNodeChild != nullptr )
        {
            newDirectNodeChild->parent = node;
        }

        AVLNode *prevParentOfMainNode = node->parent;

        moveToUsNeighbor->parent = prevParentOfMainNode;

        // Update the node pointer.
        {
            AVLNode **nodePtr = get_node_src_ptr( prevParentOfMainNode, node );

            *nodePtr = moveToUsNeighbor;
        }

        node->parent = moveToUsNeighbor;

        // Need to update node heights.
        size_t subHeight = calc_height_of_node( newDirectNodeChild, otherDirectChild );

        node->height = subHeight;

        // Height of the node on top.
        size_t fullHeight = 1;

        if ( tuggedWithNode == nullptr )
        {
            fullHeight += subHeight;
        }
        else
        {
            fullHeight += height_func( subHeight, tuggedWithNode->height );
        }

        moveToUsNeighbor->height = fullHeight;
    }

    static AINLINE bool detect_tree_two_imbalance( AVLNode *maybeBigger, AVLNode *maybeSmaller ) noexcept
    {
        // Calculates the first imbalance equation of the AVL balance method,

        if ( maybeBigger == nullptr )
        {
            return false;
        }

        size_t maybeBiggerHeight = maybeBigger->height;

        if ( maybeSmaller == nullptr )
        {
            return ( maybeBiggerHeight > 0 );
        }

        size_t maybeSmallerHeight = maybeSmaller->height;

        return ( maybeBiggerHeight > ( 1 + maybeSmallerHeight ) );
    }

    static AINLINE bool detect_tree_one_imbalance( AVLNode *maybeSmaller, AVLNode *maybeBigger ) noexcept
    {
        // Calculates the second imbalance equation of the AVL balance method,

        if ( maybeBigger == nullptr )
        {
            return false;
        }
        if ( maybeSmaller == nullptr )
        {
            return true;
        }

        return ( maybeSmaller->height < maybeBigger->height );
    }

    AINLINE void balance( AVLNode *node ) noexcept
    {
        AVLNode *leftNode = node->left;
        AVLNode *rightNode = node->right;

        if ( leftNode != nullptr && detect_tree_two_imbalance( leftNode, rightNode ) )
        {
            AVLNode *rightNodeOfLeftNode = leftNode->right;

            if ( rightNodeOfLeftNode != nullptr && detect_tree_one_imbalance( leftNode->left, rightNodeOfLeftNode ) )
            {
                rotate( leftNode, rightNodeOfLeftNode, true );

                leftNode = rightNodeOfLeftNode;
            }

            rotate( node, leftNode, false );
        }
        else if ( rightNode != nullptr && detect_tree_two_imbalance( rightNode, leftNode ) )
        {
            AVLNode *leftNodeOfRightNode = rightNode->left;

            if ( leftNodeOfRightNode != nullptr && detect_tree_one_imbalance( rightNode->right, leftNodeOfRightNode ) )
            {
                rotate( rightNode, leftNodeOfRightNode, false );

                rightNode = leftNodeOfRightNode;
            }

            rotate( node, rightNode, true );
        }
    }

    AINLINE AVLNode* ScanForNodePosition( AVLNode *node, AVLNode**& iterPtrOut, AVLNode*& parentOut ) noexcept(noexcept(dispatcherType::CompareNodes( node, node )))
    {
        // Find the position to insert our new node at.
        AVLNode *parent = nullptr;
        AVLNode **iterPtr = &this->root;
        AVLNode *iter = *iterPtr;

        while ( true )
        {
            if ( iter == nullptr )
            {
                // Quit because we hit rock bottom.
                break;
            }

            AVLNode *newParent = iter;

            // Check key condition.
            eir::eCompResult cmpRes = dispatcherType::CompareNodes( node, iter );

            if ( cmpRes == eir::eCompResult::LEFT_LESS )
            {
                iterPtr = &iter->left;
                iter = iter->left;
            }
            else if ( cmpRes == eir::eCompResult::LEFT_GREATER )
            {
                iterPtr = &iter->right;
                iter = iter->right;
            }
            else
            {
                // Quit because we found the node.
                break;
            }

            // Update the parent.
            parent = newParent;
        }

        iterPtrOut = iterPtr;
        parentOut = parent;
        return iter;
    }

    // Note that we assume here that we have a "nearly-correct" AVL tree, which means
    // that there is at max an error of one, a difference of 2 in height compared to
    // the other child subtree.
    AINLINE void update_invalidated_tree_height( AVLNode *parent ) noexcept
    {
        while ( parent != nullptr )
        {
            parent->height = calc_height_of_node( parent->left, parent->right );

            balance( parent );

            parent = parent->parent;
        }
    }

public:
    // Note that we have no more failure case because our AVL tree implementation does support adding
    // multiple same values.
    // We do allow exceptions because the comparator could throw one.
    inline void Insert( AVLNode *node ) noexcept(noexcept(dispatcherType::CompareNodes( node, node )))
    {
        // Find the position to insert our new node at.
        AVLNode *parent;
        AVLNode **iterPtr;
        {
            AVLNode *iter = ScanForNodePosition( node, iterPtr, parent );

            if ( iter != nullptr )
            {
                // We already have found a node, so we have to make the new node part of the node stack!
                node->parent = node;
                node->nodestack_owner = iter;

                AVLNode *nodestack = iter->owned_nodestack;

                if ( nodestack == nullptr )
                {
                    LIST_CLEAR( *node );

                    iter->owned_nodestack = node;
                }
                else
                {
                    if constexpr ( nodestackInsertOrAppend )
                    {
                        LIST_INSERT( *nodestack, *node );
                    }
                    else
                    {
                        LIST_APPEND( *nodestack, *node );
                    }
                }

                return;
            }
        }

        // Just put the node here.
        *iterPtr = node;

        // Initialize the node properly.
        node->height = 0;
        node->left = nullptr;
        node->right = nullptr;
        node->parent = parent;
        node->owned_nodestack = nullptr;

        // Update to the very top all trees that have been modified.
        // This is much more efficient than doing a recursive method.
        update_invalidated_tree_height( parent );
    }

    // WARNING: only use this method if you know that a node (right) is part of a tree.
    // Otherwise you perform undefined behavior.
    // Calling this method does destroy the tree ownership of "right", so make sure you
    // remove the condition that casts it into a tree.
    // WARNING: make sure that you have taken over the same value to the new object from
    // the moved-from object or else the tree will be wrecked.
    inline void MoveNodeTo( AVLNode& left, AVLNode&& right ) noexcept
    {
        AVLNode *parent = right.parent;

        // Are we a nodestack member?
        if ( parent == &right )
        {
            left.parent = &left;
            left.nodestack_owner = right.nodestack_owner;

            AVLNode *next_listnode = right.next;

            // Because we have a linked-list that loops back to the first nodestack item
            // all pointers to members are not nullptr.
            // So fix up the links.
            next_listnode->prev = &left;

            left.next = next_listnode;

            AVLNode *prev_listnode = right.prev;

            prev_listnode->next = &left;

            left.prev = prev_listnode;
        }
        else
        {
            if ( parent != nullptr )
            {
                // Depends on if we are left or right of the node.
                if ( parent->left == &right )
                {
                    parent->left = &left;
                }
                else if ( parent->right == &right )
                {
                    parent->right = &left;
                }
                else
                {
                    FATAL_ABORT();
                }
            }
            else
            {
                // Update the root.
                this->root = &left;
            }

            left.parent = parent;

            // Fix up the children.
            AVLNode *left_node = right.left;

            if ( left_node != nullptr )
            {
                FATAL_ASSERT( left_node->parent == &right );

                left_node->parent = &left;
            }

            left.left = left_node;

            AVLNode *right_node = right.right;

            if ( right_node != nullptr )
            {
                FATAL_ASSERT( right_node->parent == &right );

                right_node->parent = &left;
            }

            left.right = right_node;

            // We have the same height, too.
            left.height = right.height;

            // Fix up all nodestack items, if we are having a nodestack.
            AVLNode *owned_nodestack = right.owned_nodestack;

            if ( owned_nodestack != nullptr )
            {
                AVLNode *iter = owned_nodestack;

                do
                {
                    FATAL_ASSERT( iter->nodestack_owner == &right );

                    iter->nodestack_owner = &left;

                    iter = iter->next;
                }
                while ( iter != owned_nodestack );
            }

            left.owned_nodestack = owned_nodestack;
        }
    }

private:
    // Simple method that is compatible with any binary tree. :)
    static AINLINE AVLNode* FindMaxNode( AVLNode *searchFrom, AVLNode**& searchFromPtrInOut ) noexcept
    {
        AVLNode *curMax = searchFrom;
        AVLNode **searchFromPtr = searchFromPtrInOut;

        while ( true )
        {
            AVLNode *nextBigger = curMax->right;

            if ( nextBigger == nullptr )
            {
                break;
            }

            searchFromPtr = &curMax->right;
            curMax = nextBigger;
        }

        searchFromPtrInOut = searchFromPtr;
        return curMax;
    }

    static AINLINE void remove_nodestack_member( AVLNode *node, AVLNode *nodestack_owner ) noexcept
    {
        // Remove us from the nodestack.
        bool is_empty = LIST_EMPTY( *node );

        if ( is_empty )
        {
            // There is no more nodestack on the owner.
            FATAL_ASSERT( nodestack_owner != nullptr );
            FATAL_ASSERT( nodestack_owner == node->nodestack_owner );

            nodestack_owner->owned_nodestack = nullptr;
        }
        else
        {
            // Just remove us from the nodestack.
            if ( nodestack_owner->owned_nodestack == node )
            {
                nodestack_owner->owned_nodestack = node->next;
            }

            LIST_REMOVE( *node );
        }
    }

    static AINLINE void inplace_node_replace(
        size_t nodeHeight, AVLNode **nodePtr, AVLNode *leftChild, AVLNode *rightChild, AVLNode *parent, AVLNode *owned_nodestack,
        AVLNode *replaceBy
    ) noexcept
    {
        // Replace nodes.
        replaceBy->parent = parent;
        replaceBy->left = leftChild;
        replaceBy->right = rightChild;
        replaceBy->height = nodeHeight;
        replaceBy->owned_nodestack = owned_nodestack;

        // If we took over a live nodestack, then we have to update the nodestack owners on the members.
        if ( owned_nodestack != nullptr )
        {
            AVLNode *curnode = owned_nodestack;

            do
            {
                curnode->nodestack_owner = replaceBy;

                curnode = curnode->next;
            }
            while ( curnode != owned_nodestack );
        }

        *nodePtr = replaceBy;

        // Update parents.
        if ( leftChild != nullptr )
        {
            leftChild->parent = replaceBy;
        }

        if ( rightChild != nullptr )
        {
            rightChild->parent = replaceBy;
        }
    }

    AINLINE void remove_node( AVLNode *node, AVLNode **nodePtr, AVLNode *leftChild, AVLNode *rightChild, AVLNode *parent ) noexcept
    {
        // We assume that node is NOT part of a nodestack!
        FATAL_ASSERT( parent != node );

        // If we have a nodestack, then we just shedule the first member of the nodestack to be new member of the AVL tree.
        if ( AVLNode *new_node = node->owned_nodestack )
        {
            // Remove it from the nodestack.
            remove_nodestack_member( new_node, node );

            // Set it in-place of node.
            inplace_node_replace(
                node->height, nodePtr, leftChild, rightChild, parent, node->owned_nodestack,
                new_node
            );
        }
        else
        {
            if ( leftChild == nullptr )
            {
                *nodePtr = rightChild;

                if ( rightChild != nullptr )
                {
                    rightChild->parent = parent;
                }
            }
            else if ( rightChild == nullptr )
            {
                *nodePtr = leftChild;

                if ( leftChild != nullptr )
                {
                    leftChild->parent = parent;
                }
            }
            else
            {
                // Pretty complicated situation.
                AVLNode **closestReplacementPtr = &node->left;
                AVLNode *closestReplacement = FindMaxNode( leftChild, closestReplacementPtr );

                // Now we perform what someone usually has to do when updating the value of a node.
                // But this is optimized to keep the pointers valid.

                // Remove the closest node.
                // We know that the node has no right child, because it is the maximum in its subtree.
                // But since remove_node has become pretty complicated we rather stick to a call of itself :/
                remove_node( closestReplacement, closestReplacementPtr, closestReplacement->left, nullptr, closestReplacement->parent );

                // We need to update members because they could be out-of-date.
                parent = node->parent;
                leftChild = node->left;
                rightChild = node->right;

                nodePtr = get_node_src_ptr( parent, node );

                inplace_node_replace(
                    node->height, nodePtr, leftChild, rightChild, parent, nullptr,
                    closestReplacement
                );

                // Now closestReplacement takes the role of node.
            }

            // Update height of the changed tree.
            update_invalidated_tree_height( parent );
        }
    }

public:
    // Returns true if the provided node-pointer is inside of this AVL tree.
    inline bool IsNodeInsideTree( AVLNode *node ) noexcept(noexcept(ScanForNodePosition( nullptr, *(AVLNode***)nullptr, *(AVLNode**)nullptr )))
    {
        AVLNode *parent;
        AVLNode **iterPtr;

        AVLNode *iter = ScanForNodePosition( node, iterPtr, parent );

        if ( iter == nullptr )
        {
            return false;
        }

        // Check if it is either this node or part of the node stack.
        if ( iter != node )
        {
            if ( AVLNode *nodestack = iter->owned_nodestack )
            {
                AVLNode *curnode = nodestack;

                do
                {
                    if ( curnode == node )
                    {
                        return true;
                    }

                    curnode = curnode->next;
                }
                while ( curnode != nodestack );
            }

            return false;
        }

        return true;
    }

    inline void RemoveByNodeFast( AVLNode *node ) noexcept
    {
        // A faster removal method which assumes that node does really reside inside this AVL tree.
        AVLNode *parent = node->parent;

        if ( parent == node )
        {
            remove_nodestack_member( node, node->nodestack_owner );
        }
        else
        {
            AVLNode **iterPtr = get_node_src_ptr( parent, node );

            remove_node( node, iterPtr, node->left, node->right, parent );
        }
    }

    inline bool RemoveByNode( AVLNode *node ) noexcept(noexcept(ScanForNodePosition( nullptr, *(AVLNode***)nullptr, *(AVLNode**)nullptr )))
    {
        // Find the node we are talking about.
        // This is basically just verification that the node is inside this tree, meow.
        // Use RemoveByNodeFast if you know for sure that the node is inside this tree.
        AVLNode *parent;
        AVLNode **iterPtr;
        {
            AVLNode *iter = ScanForNodePosition( node, iterPtr, parent );

            if ( iter == nullptr )
            {
                // We could not find the node, so quit.
                return false;
            }

            if ( node != iter )
            {
                // It is not the same node!
                // It could still be part of its nodestack.
                if ( AVLNode *nodestack = iter->owned_nodestack )
                {
                    AVLNode *curnode = nodestack;

                    do
                    {
                        if ( curnode == node )
                        {
                            // Found it.
                            remove_nodestack_member( node, iter );

                            return true;
                        }

                        curnode = curnode->next;
                    }
                    while ( curnode != nodestack );
                }

                return false;
            }
        }

        remove_node( node, iterPtr, node->left, node->right, parent );

        return true;
    }

    // Clears the tree, removing all nodes from it.
    inline void Clear( void ) noexcept
    {
        this->root = nullptr;
    }

    // Iterates over every same-value node of a node-stack. The walkNode must be a node
    // that is linked to the AVL tree (not a node-stack member itself).
    struct nodestack_iterator
    {
        AINLINE nodestack_iterator( AVLNode *walkNode ) noexcept
        {
            // Must not be part of node-stack.
            FATAL_ASSERT( walkNode->parent != walkNode );

            this->curNode = walkNode;
            this->nodestack = nullptr;
            this->doesIterateNodestack = false;
            this->isEnd = ( walkNode == nullptr );
        }
        AINLINE nodestack_iterator( const nodestack_iterator& ) = default;
        AINLINE ~nodestack_iterator( void ) = default;

        AINLINE nodestack_iterator& operator = ( const nodestack_iterator& ) = default;

        AINLINE bool IsEnd( void ) const noexcept
        {
            return this->isEnd;
        }

        AINLINE void Increment( void ) noexcept
        {
            AVLNode *curNode = this->curNode;

            FATAL_ASSERT( curNode != nullptr );

            if ( this->doesIterateNodestack )
            {
                curNode = curNode->next;

                if ( curNode == this->nodestack )
                {
                    this->isEnd = true;
                }
            }
            else
            {
                AVLNode *nodestack = curNode->owned_nodestack;

                if ( nodestack != nullptr )
                {
                    curNode = nodestack;
                    this->nodestack = nodestack;
                    this->doesIterateNodestack = true;
                }
                else
                {
                    curNode = nullptr;
                    this->isEnd = true;
                }
            }

            // It has to change through iteration.
            // Very friendly to do so at the end aswell.
            this->curNode = curNode;
        }

        AINLINE AVLNode* Resolve( void ) noexcept
        {
            return this->curNode;
        }

    private:
        AVLNode *curNode;
        AVLNode *nodestack;
        bool doesIterateNodestack;
        bool isEnd;
    };

    // Calls the function for each node-stack member of the node.
    // Should be used if you want to iterate over absolutely every node of a tree.
    template <typename callbackType> requires ( std::is_invocable <callbackType, AVLNode*>::value )
    static AINLINE void call_for_each_node_for_nodestack( AVLNode *walkNode, callbackType&& cb ) noexcept(noexcept(cb( nullptr )))
    {
        nodestack_iterator iter( walkNode );

        while ( !iter.IsEnd() )
        {
            cb( iter.Resolve() );

            iter.Increment();
        }
    }

    // Iterates over each value-different node without counting the same-value nodestack members.
    struct diff_node_iterator
    {
        AINLINE diff_node_iterator( void ) noexcept
        {
            this->curNode = nullptr;
        }
        AINLINE diff_node_iterator( AVLNode *walkNode ) noexcept
        {
            this->curNode = walkNode;
        }
        AINLINE diff_node_iterator( AVLTree& tree ) noexcept : diff_node_iterator( tree.GetSmallestNode() )
        {
            return;
        }
        AINLINE diff_node_iterator( const diff_node_iterator& right ) = default;
        AINLINE ~diff_node_iterator( void ) = default;

        AINLINE diff_node_iterator& operator = ( const diff_node_iterator& right ) = default;

        AINLINE bool IsEnd( void ) const noexcept
        {
            return ( this->curNode == nullptr );
        }

        AINLINE void Increment( void ) noexcept
        {
            AVLNode *curNode = this->curNode;

            // Iterate the right.
            if ( AVLNode *rightNode = curNode->right )
            {
                curNode = rightNode;

                // Go to the first item of the newly-found subtree.
                while ( AVLNode *leftChild = curNode->left )
                {
                    curNode = leftChild;
                }
            }
            else
            {
                // Go up until we find a branch to the right again.
                // On this way up call for each node when we come from a left branch.
                AVLNode *parent = curNode->parent;

                while ( parent != nullptr && curNode != parent->left )
                {
                    // Go up one more.
                    curNode = parent;
                    parent = parent->parent;
                }

                curNode = parent;
            }

            this->curNode = curNode;
        }

        AINLINE void Decrement( void ) noexcept
        {
            AVLNode *curNode = this->curNode;

            // Iterate the left.
            if ( AVLNode *leftNode = curNode->left )
            {
                curNode = leftNode;

                // Go to the first item of the newly-found subtree.
                while ( AVLNode *rightChild = curNode->right )
                {
                    curNode = rightChild;
                }
            }
            else
            {
                // Go up until we find a branch to the left again.
                // On this way up call for each node when we come from a right branch.
                AVLNode *parent = curNode->parent;

                while ( parent != nullptr && curNode != parent->right )
                {
                    // Go up one more.
                    curNode = parent;
                    parent = parent->parent;
                }

                curNode = parent;
            }

            this->curNode = curNode;
        }

        AINLINE AVLNode* Resolve( void ) const noexcept
        {
            return this->curNode;
        }

    private:
        AVLNode *curNode;
    };

    // Goes through each node in sorted order.
    // This is just a helper for the entire tree.
    template <typename callbackType> requires ( std::is_invocable <callbackType, AVLNode*>::value )
    inline void Walk( callbackType&& cb ) noexcept(noexcept(call_for_each_node_for_nodestack( nullptr, cb )))
    {
        diff_node_iterator iter( this->GetSmallestNode() );

        while ( !iter.IsEnd() )
        {
            // First iterate the left child, then current node and then the right node,
            // just like in a regular binary tree.

            // Now we can iterate this node.
            call_for_each_node_for_nodestack( iter.Resolve(), cb );

            iter.Increment();
        }
    }

    // Goes through each node in reverse sorted ordner.
    template <typename callbackType> requires ( std::is_invocable <callbackType, AVLNode*>::value )
    inline void WalkReverse( const callbackType& cb ) noexcept(noexcept(call_for_each_node_for_nodestack( nullptr, cb )))
    {
        diff_node_iterator iter( this->GetBiggestNode() );

        while ( !iter.IsEnd() )
        {
            call_for_each_node_for_nodestack( iter.Resolve(), cb );

            iter.Decrement();
        }
    }

    // Returns the maximum path length to the botton of the tree.
    inline size_t GetTreeHeight( void ) const noexcept
    {
        if ( AVLNode *rootNode = this->root )
        {
            return ( 1 + rootNode->height );
        }

        return 0;
    }

    // Returns the smallest node that is part of the tree.
    inline AVLNode* GetSmallestNode( void ) noexcept
    {
        AVLNode *curNode = this->root;

        if ( !curNode )
        {
            return nullptr;
        }

        while ( AVLNode *smallerNode = curNode->left )
        {
            curNode = smallerNode;
        }

        return curNode;
    }

    inline const AVLNode* GetSmallestNode( void ) const noexcept
    {
        return const_cast <AVLTree*> ( this )->GetSmallestNode();
    }

    // Returns the biggest node that is linked into the tree.
    inline AVLNode* GetBiggestNode( void ) noexcept
    {
        AVLNode *curNode = this->root;

        if ( !curNode )
        {
            return nullptr;
        }

        while ( AVLNode *biggerNode = curNode->right )
        {
            curNode = biggerNode;
        }

        return curNode;
    }

    inline const AVLNode* GetBiggestNode( void ) const noexcept
    {
        return const_cast <AVLTree*> ( this )->GetBiggestNode();
    }

    // Returns the node at the top of the tree.
    inline AVLNode* GetRootNode( void ) noexcept
    {
        return this->root;
    }
    inline const AVLNode* GetRootNode( void ) const noexcept
    {
        return this->root;
    }

    // Returns the node whose value is just above or equal to the given value.
    template <typename valueType> requires AVLNodeDispatcherNodeComparableWith <dispatcherType, valueType>
    inline AVLNode* GetJustAboveOrEqualNode( const valueType& theValue ) noexcept(noexcept(dispatcherType::CompareNodeWithValue( nullptr, theValue )))
    {
        AVLNode *closestAboveNode = nullptr;

        // Search for the equal node.
        // On the way of search check all that all are bigger or equal to theValue.
        AVLNode *walkNode = this->root;

        while ( walkNode )
        {
            // Walk to the next node.
            eir::eCompResult cmpRes = dispatcherType::CompareNodeWithValue( walkNode, theValue );

            if ( cmpRes == eir::eCompResult::LEFT_LESS )
            {
                walkNode = walkNode->right;
            }
            else if ( cmpRes == eir::eCompResult::EQUAL )
            {
                // We found the exact node. There is no beating that.
                return walkNode;
            }
            else
            {
                // We have found a closer node to our accomplishment.
                closestAboveNode = walkNode;

                // Any further node we find must be closer to theValue because it is left from the closest.
                walkNode = walkNode->left;
            }
        }

        return closestAboveNode;
    }

    // Returns the node whose value is just above to the given value.
    template <typename valueType> requires AVLNodeDispatcherNodeComparableWith <dispatcherType, valueType>
    inline AVLNode* GetJustAboveNode( const valueType& theValue ) noexcept(noexcept(dispatcherType::CompareNodeWithValue( nullptr, theValue )))
    {
        AVLNode *closestAboveNode = nullptr;

        // Search for the equal node.
        // On the way of search check all that all are bigger or equal to theValue.
        AVLNode *walkNode = this->root;

        while ( walkNode )
        {
            // Walk to the next node.
            eir::eCompResult cmpRes;

            if constexpr ( AVLNodeDispatcherNodeLeftComparableWith <dispatcherType, valueType> )
            {
                cmpRes = dispatcherType::CompareNodeWithValue( walkNode, theValue );
            }
            else
            {
                cmpRes = eir::flip_comp_result( dispatcherType::CompareNodeWithValue( theValue, walkNode ) );
            }

            if ( cmpRes != eir::eCompResult::LEFT_GREATER )
            {
                walkNode = walkNode->right;
            }
            else
            {
                // We have found a closer node to our accomplishment.
                closestAboveNode = walkNode;

                // Any further node we find must be closer to theValue because it is left from the closest.
                walkNode = walkNode->left;
            }
        }

        return closestAboveNode;
    }

    // Returns the node that matches the given value.
    template <typename customDispatcherType, typename valueType> requires AVLNodeDispatcherNodeComparableWith <customDispatcherType, valueType>
    inline AVLNode* FindNodeCustom( const valueType& value ) noexcept(noexcept(customDispatcherType::CompareNodeWithValue( nullptr, value )))
    {
        AVLNode *curNode = this->root;

        while ( curNode )
        {
            // Check which one to go for next.
            eir::eCompResult cmpRes;
            
            if constexpr ( AVLNodeDispatcherNodeLeftComparableWith <customDispatcherType, valueType> )
            {
                cmpRes = customDispatcherType::CompareNodeWithValue( curNode, value );
            }
            else
            {
                cmpRes = eir::flip_comp_result( customDispatcherType::CompareNodeWithValue( value, curNode ) );
            }

            if ( cmpRes == eir::eCompResult::LEFT_LESS )
            {
                curNode = curNode->right;
            }
            else if ( cmpRes == eir::eCompResult::LEFT_GREATER )
            {
                curNode = curNode->left;
            }
            else
            {
                // We found it.
                break;
            }
        }

        return curNode;
    }
    template <typename valueType> requires AVLNodeDispatcherNodeComparableWith <dispatcherType, valueType>
    inline AVLNode* FindNode( const valueType& value ) noexcept(noexcept(FindNodeCustom <dispatcherType> ( value )))
    {
        return FindNodeCustom <dispatcherType> ( value );
    }

    // Returns the node that matches the given value, const-equivalent.
    template <typename customDispatcherType, typename valueType> requires AVLNodeDispatcherNodeComparableWith <customDispatcherType, valueType>
    inline const AVLNode* FindNodeCustom( const valueType& value ) const noexcept(noexcept(const_cast <AVLTree*> ( this )->FindNodeCustom( value )))
    {
        return const_cast <AVLTree*> ( this )->FindNodeCustom( value );
    }
    template <typename valueType> requires AVLNodeDispatcherNodeComparableWith <dispatcherType, valueType>
    inline const AVLNode* FindNode( const valueType& value ) const noexcept(noexcept(FindNodeCustom <dispatcherType> ( value )))
    {
        return FindNodeCustom <dispatcherType> ( value );
    }

private:
    template <typename callbackType> requires ( std::is_invocable_r <eir::eCompResult, callbackType, AVLNode*>::value )
    AINLINE AVLNode* _FindAnyNodeByCriteriaFromNode( AVLNode *curNode, const callbackType& cb ) noexcept(std::is_nothrow_invocable_r <eir::eCompResult, callbackType, AVLNode>::value)
    {
        while ( curNode )
        {
            eir::eCompResult cmpRes = cb( curNode );

            if ( cmpRes == eir::eCompResult::LEFT_LESS )
            {
                curNode = curNode->right;
            }
            else if ( cmpRes == eir::eCompResult::LEFT_GREATER )
            {
                curNode = curNode->left;
            }
            else
            {
                // Found any node.
                break;
            }
        }

        return curNode;
    }

public:
    // Returns any node that matches our criteria.
    template <typename callbackType> requires ( std::is_invocable_r <eir::eCompResult, callbackType, AVLNode*>::value )
    AINLINE AVLNode* FindAnyNodeByCriteria( const callbackType& cb ) noexcept(noexcept(_FindAnyNodeByCriteriaFromNode( nullptr, cb )))
    {
        return _FindAnyNodeByCriteriaFromNode( this->root, cb );
    }

    // Finds the minimum node that matches certain criteria.
    // Returning eCompResult::EQUAL indicates a match but in this function multiple nodes are allowed to be equal, forming
    // classes of values.
    // IMPORTANT: node classes are supposed to be contiguous! meaning there cannot be a node not in the class between two
    // nodes that instead are.
    template <typename callbackType> requires ( std::is_invocable_r <eir::eCompResult, callbackType, AVLNode*>::value )
    inline AVLNode* FindMinimumNodeByCriteria( const callbackType& cb ) noexcept(noexcept(_FindAnyNodeByCriteriaFromNode( nullptr, cb )))
    {
        // NOTE: cb is a virtual "right value-span". it is pretty complicated but just roll with it.

        // We first try to find any node in the class.
        AVLNode *curNode = FindAnyNodeByCriteria( cb );

        if ( curNode != nullptr )
        {
            // Is there any smaller node that matches us?
            // If yes then we pick that one instead.
            while ( AVLNode *smaller = _FindAnyNodeByCriteriaFromNode( curNode->left, cb ) )
            {
                curNode = smaller;
            }
        }

        return curNode;
    }

    template <typename callbackType> requires ( std::is_invocable_r <eir::eCompResult, callbackType, AVLNode*>::value )
    inline AVLNode* FindMaximumNodeByCriteria( const callbackType& cb ) noexcept(noexcept(_FindAnyNodeByCriteriaFromNode( nullptr, cb )))
    {
        AVLNode *curNode = FindAnyNodeByCriteria( cb );

        if ( curNode != nullptr )
        {
            // Is there any bigger node that matches us?
            // If yes then we pick that one instead.
            while ( AVLNode *bigger = _FindAnyNodeByCriteriaFromNode( curNode->right, cb ) )
            {
                curNode = bigger;
            }
        }

        return curNode;
    }

    // Should always succeed.
    inline void Validate( void )
    {
        Walk(
            []( AVLNode *node )
        {
            // Ignore any nodestack member.
            AVLNode *parent = node->parent;

            if ( parent == node )
            {
                return;
            }

            if ( parent )
            {
                if ( parent->left == node )
                {
                    FATAL_ASSERT( dispatcherType::CompareNodes( node, parent ) == eir::eCompResult::LEFT_LESS );
                }
                else if ( parent->right == node )
                {
                    FATAL_ASSERT( dispatcherType::CompareNodes( parent, node ) == eir::eCompResult::LEFT_LESS );
                }
                else
                {
                    FATAL_ABORT();
                }
            }

            if ( AVLNode *leftNode = node->left )
            {
                FATAL_ASSERT( leftNode->parent == node );
                FATAL_ASSERT( dispatcherType::CompareNodes( leftNode, node ) == eir::eCompResult::LEFT_LESS );
            }

            if ( AVLNode *rightNode = node->right )
            {
                FATAL_ASSERT( rightNode->parent == node );
                FATAL_ASSERT( dispatcherType::CompareNodes( node, rightNode ) == eir::eCompResult::LEFT_LESS );
            }

            // TODO: verify the height.
        });
    }

private:
    AVLNode *root;
};

#endif //_AVL_TREE_IMPLEMENTATION_
