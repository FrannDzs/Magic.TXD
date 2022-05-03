/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/SortedSliceSector.h
*  PURPOSE:     Sorted intrusion-interval region of memory
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// In a stream buffer you must remember regions that you wrote across a limited
// memory buffer. When intersecting non-convex shapes you do get a list of
// intervals. So you need a class that handles the building of intervals across
// dynamic operations.
// When merging source code conflicts you have regions/intervals of code/strings
// that have to merged based on ratings/user choice.

// The difference between eir::Set and eir::SortedSliceSector is that sets do
// check for equality only but SortedSliceSector does merge slices which
// overlap thus this class is more powerful.

#ifndef _EIR_SORTED_SLICE_SECTOR_HEADER_
#define _EIR_SORTED_SLICE_SECTOR_HEADER_

#include "MathSlice.h"
#include "MathSlice.algorithms.h"
#include "AVLTree.h"
#include "eirutils.h"
#include "MetaHelpers.h"
#include "avlsetmaputil.h"
#include "rwlist.hpp"

#ifdef _DEBUG
#include <assert.h>
#endif //_DEBUG

namespace eir
{

template <typename numberType>
struct SortedSliceSectorStdMetadata
{
    AINLINE SortedSliceSectorStdMetadata( mathSlice <numberType> slice ) : slice( std::move( slice ) )
    {
        return;
    }

    AINLINE bool IsMergeable( void ) const
    {
        return true;
    }

    AINLINE void Update( mathSlice <numberType> newSlice )
    {
        this->slice = std::move( newSlice );
    }

    AINLINE void Remove( void )
    {
        return;
    }

    AINLINE mathSlice <numberType> GetNodeSlice( void ) const
    {
        return slice;
    }

    // Optional helper.
    AINLINE operator const mathSlice <numberType>& ( void ) const
    {
        return slice;
    }

private:
    mathSlice <numberType> slice;
};

template <typename numberType, MemoryAllocator allocatorType, typename metaDataType = SortedSliceSectorStdMetadata <numberType>, typename exceptMan = eir::DefaultExceptionManager>
struct SortedSliceSector
{
    // REQUIREMENTS FOR metaDataType TYPE:
    // * constructor( eir::mathSlice <numberType> slice, ... );
    // * copy-constructor;
    // * move-constructor;
    // * sectorSlice_t GetNodeSlice( void ) const;
    // * void Remove( void );
    // * template <typename... constrArgs> bool IsMergeable( constrArgs... args ) const;
    // * void Update( const sectorSlice_t& newSlice );

    typedef mathSlice <numberType> sectorSlice_t;

private:
    struct sliceNode
    {
        template <typename, MemoryAllocator, typename, typename> friend struct SortedSliceSector;

        // DO NOT CONSTRUCT OR DESTROY THIS CLASS OUTSIDE OF SortedSliceSector !

        template <typename... constrArgs>
        AINLINE sliceNode( sectorSlice_t slice, constrArgs... args ) : metaData( std::move( slice ), std::forward <constrArgs> ( args )... )
        {
            return;
        }
        AINLINE sliceNode( metaDataType metaData ) : metaData( std::move( metaData ) )
        {
            return;
        }
        AINLINE sliceNode( const sliceNode& ) = delete;
        AINLINE sliceNode( sliceNode&& ) = delete;

        AINLINE ~sliceNode( void )
        {
            return;
        }

        AINLINE sliceNode& operator = ( const sliceNode& ) = delete;
        AINLINE sliceNode& operator = ( sliceNode&& ) = delete;

        AINLINE sectorSlice_t GetNodeSlice( void ) const
        {
            return metaData.GetNodeSlice();
        }

        AINLINE metaDataType& GetMetaData( void )
        {
            return this->metaData;
        }
        AINLINE const metaDataType& GetMetaData( void ) const
        {
            return this->metaData;
        }

    private:
        metaDataType metaData;
        mutable AVLNode node;
    };

    // Node creation and destruction helpers.
    template <typename... constrArgs>
    AINLINE sliceNode* new_node( constrArgs... args )
    {
        return eir::dyn_new_struct <sliceNode, exceptMan> ( this->data.opt(), this, std::forward <constrArgs> ( args )... );
    }

    AINLINE void del_node( sliceNode *node ) noexcept
    {
        eir::dyn_del_struct <sliceNode> ( this->data.opt(), this, node );
    }

public:
    AINLINE SortedSliceSector( void ) noexcept
    {
        return;
    }

    template <typename... allocArgs>
    AINLINE SortedSliceSector( eir::constr_with_alloc _, allocArgs... args )
        : data( std::tuple(), std::tuple( std::forward <allocArgs> ( args )... ) )
    {
        return;
    }

    AINLINE SortedSliceSector( const SortedSliceSector& ) = delete;
    AINLINE SortedSliceSector( SortedSliceSector&& right ) noexcept : data( std::move( right.data ) )
    {
        return;
    }
    AINLINE ~SortedSliceSector( void )
    {
        // Clean up memory.
        while ( AVLNode *avlNode = this->data.avlSliceTree.GetRootNode() )
        {
            sliceNode *node = AVL_GETITEM( sliceNode, avlNode, node );

            this->data.avlSliceTree.RemoveByNode( avlNode );

            del_node( node );
        }
    }

    AINLINE SortedSliceSector& operator = ( const SortedSliceSector& ) = delete;
    AINLINE SortedSliceSector& operator = ( SortedSliceSector&& right ) noexcept
    {
        this->avlSliceTree = std::move( right.avlSliceTree );

        this->data.opt() = std::move( right.data.opt() );

        return *this;
    }

private:
    template <typename... constrArgs>
    AINLINE void MergeSlice( sectorSlice_t slice, sliceNode *hostOfMerge, sectorSlice_t hostRegion, constrArgs... args )
    {
        // Special case: if we are told to merge in an empty slice, we discard it along with its node.
        if ( slice.IsEmpty() )
        {
            // Since we require that hostOfMerge is sharing region with slice variable,
            // then it mus tmean that hostOfMerge is empty too.
            // Thus it can be deleted.
            if ( hostOfMerge )
            {
                del_node( hostOfMerge );
            }

            return;
        }

        // Collect the region that our new slice should be at.
        lowerBound <numberType> regionMin = slice.GetSliceStartBound();
        upperBound <numberType> regionMax = slice.GetSliceEndBound();

        // Now process all nodes in the AVL tree that intersect the requested slice.
        // * if a node is found not mergeable, it is removed/shrunk.
        // * if a node is found mergeable, then it gives up its region to the new slice.
        // As a result, (at least) the requested slice is placed available in our region.

        sliceNode* nodesToSubtract[ 2 ];    // there is a max of two nodes that can be alive after subtraction.
        sectorSlice_t nodesToSubtractSlices[ 2 ];
        size_t numNodesToSubtract = 0;
        {
            while ( true )
            {
                AVLNode *avlIntersectNode = this->data.avlSliceTree.FindAnyNodeByCriteria(
                    [&]( AVLNode *leftNode )
                {
                    return _CompareNodeWithSlice( leftNode, slice, true );
                });

                if ( avlIntersectNode == nullptr )
                {
                    break;
                }

                // Remove it so we are done processing it.
                this->data.avlSliceTree.RemoveByNode( avlIntersectNode );

                // Can we merge with this node?
                sliceNode *intersectNode = AVL_GETITEM( sliceNode, avlIntersectNode, node );

                sectorSlice_t intersectSlice = intersectNode->GetNodeSlice();

                if ( intersectNode->metaData.IsMergeable( args... ) )
                {
                    // Update our merge region.
                    if ( intersectSlice.GetSliceStartBound() < regionMin )
                    {
                        regionMin = intersectSlice.GetSliceStartBound();
                    }

                    if ( regionMax < intersectSlice.GetSliceEndBound() )
                    {
                        regionMax = intersectSlice.GetSliceEndBound();
                    }

                    // Remember the first one so we can use it as memory.
                    if ( hostOfMerge == nullptr )
                    {
                        hostOfMerge = intersectNode;
                        hostRegion = intersectSlice;
                    }
                    // TODO: might make a selector here to pick a better one across all merge-candidates.
                    else
                    {
                        // Delete any non-candidates.
                        del_node( intersectNode );
                    }
                }
                else
                {
                    // If not then we need to subtract it later, if we would not disappear of course.
                    eIntersectionResult intResult = intersectSlice.intersectWith( slice );

                    bool hasRegistered = false;

                    // Graceful cleanup to handle the unknown case.

                    if ( isCoveredIntersect( intResult ) == false && intResult != INTERSECT_UNKNOWN )
                    {
#ifdef _DEBUG
                        assert( numNodesToSubtract < 2 );
#endif //_DEBUG

                        if ( numNodesToSubtract < 2 )
                        {
                            nodesToSubtract[ numNodesToSubtract ] = intersectNode;
                            nodesToSubtractSlices[ numNodesToSubtract ] = intersectSlice;

                            numNodesToSubtract++;

                            hasRegistered = true;
                        }
                    }

                    // Either if we are covered or there was an error (graceful handling).
                    if ( hasRegistered == false )
                    {
                        del_node( intersectNode );
                    }
                }
            }
        }

        // Construct the slice of the merge region we decided on.
        sectorSlice_t newSlice = sectorSlice_t::fromBounds( regionMin, regionMax );

        // Put in the other slices after subtraction.
        for ( size_t n = 0; n < numNodesToSubtract; n++ )
        {
            sliceNode *subtractNode = nodesToSubtract[ n ];
            sectorSlice_t subtractSlice = nodesToSubtractSlices[ n ];

            bool hasFirst = false;

            subtractSlice.subtractRegion( newSlice,
                [&]( const sectorSlice_t& remainder, bool isFirst )
            {
                sliceNode *insertToTreeNode = nullptr;

                if ( isFirst )
                {
                    // Did the validity region actually change?
                    if ( subtractSlice != remainder )
                    {
                        // Set the new validity region.
                        subtractNode->metaData.Update( remainder );
                    }

                    // Reinsert into the tree.
                    insertToTreeNode = subtractNode;

                    hasFirst = true;
                }
                else
                {
                    // Spawn a new node.
                    metaDataType metaData( subtractNode->metaData );

                    metaData.Update( remainder );

                    insertToTreeNode = new_node( std::move( metaData ) );
                }

                this->data.avlSliceTree.Insert( &insertToTreeNode->node );
            });

            if ( !hasFirst )
            {
                // Error handling, just in case (graceful).
                del_node( subtractNode );
            }
        }

        // Put in our new node with the arguments forwarded-in.
        if ( hostOfMerge )
        {
            // Did the region actually change?
            if ( hostRegion != newSlice )
            {
                hostOfMerge->metaData.Update( std::move( newSlice ) );
            }
        }
        else
        {
            // Allocate a new node.
            hostOfMerge = new_node( std::move( newSlice ), std::forward <constrArgs> ( args )... );
        }

        // Since this node has been removed we must reinsert it.
        this->data.avlSliceTree.Insert( &hostOfMerge->node );
    }

public:
    template <typename... constrArgs>
    inline void Insert( sectorSlice_t slice, constrArgs... args )
    {
        MergeSlice( std::move( slice ), nullptr, sectorSlice_t(), std::forward <constrArgs> ( args )... );
    }

    inline void Remove( sectorSlice_t slice )
    {
        // While we find any intersection we remove areas from said nodes.
        while ( AVLNode *avlFoundNode = this->data.avlSliceTree.template FindNodeCustom <nodeWithoutNeighborComparator> ( slice ) )
        {
            sliceNode *foundNode = AVL_GETITEM( sliceNode, avlFoundNode, node );

            // Perform a subtract.
            sectorSlice_t nodeSlice = foundNode->GetNodeSlice();

            bool didHaveFirst = false;

            // Need to update our region.
            this->data.avlSliceTree.RemoveByNode( avlFoundNode );

            nodeSlice.subtractRegion( slice,
                [&]( const sectorSlice_t& newSlice, bool isFirst )
            {
                if ( isFirst )
                {
                    foundNode->metaData.Update( newSlice );

                    this->data.avlSliceTree.Insert( avlFoundNode );

                    didHaveFirst = true;
                }
                else
                {
                    // Must spawn a new node.
                    metaDataType metaData( foundNode->metaData );

                    metaData.Update( newSlice );

                    sliceNode *newNode = new_node( std::move( metaData ) );

                    try
                    {
                        this->data.avlSliceTree.Insert( &newNode->node );
                    }
                    catch( ... )
                    {
                        del_node( newNode );

                        throw;
                    }
                }
            });

            if ( !didHaveFirst )
            {
                // Remove us.
                foundNode->metaData.Remove();

                del_node( foundNode );
            }
        }
    }

    AINLINE bool HasIntersection( const sectorSlice_t& slice ) const
    {
        return ( this->data.avlSliceTree.template FindNodeCustom <nodeWithoutNeighborComparator> ( slice ) != nullptr );
    }

    AINLINE bool IsEmpty( void ) const
    {
        return ( this->data.avlSliceTree.GetRootNode() == nullptr );
    }

    AINLINE void Clear( void )
    {
        while ( AVLNode *avlCurNode = this->data.avlSliceTree.GetRootNode() )
        {
            sliceNode *curNode = AVL_GETITEM( sliceNode, avlCurNode, node );

            curNode->metaData.Remove();

            this->data.avlSliceTree.RemoveByNode( avlCurNode );

            del_node( curNode );
        }
    }

    AINLINE size_t GetSliceCount( void ) const
    {
        typename avlSliceTree_t::diff_node_iterator iter( this->data.avlSliceTree );

        size_t count = 0;

        while ( !iter.IsEnd() )
        {
            count++;

            iter.Increment();
        }

        return count;
    }

    AINLINE sectorSlice_t GetSpan( void ) const
    {
        AVLNode *avlBegNode = this->data.avlSliceTree.GetSmallestNode();

        if ( avlBegNode == nullptr )
        {
            return sectorSlice_t();
        }

        sliceNode *begNode = AVL_GETITEM( sliceNode, avlBegNode, node );

        numberType rangeMin = begNode->GetNodeSlice().GetSliceStartPoint();

        AVLNode *avlEndNode = this->data.avlSliceTree.GetBiggestNode();

#ifdef _DEBUG
        assert( avlEndNode != nullptr );
#endif //_DEBUG

        sliceNode *endNode = AVL_GETITEM( sliceNode, avlEndNode, node );

        numberType rangeMax = endNode->GetNodeSlice().GetSliceEndPoint();

        return sectorSlice_t::fromOffsets( rangeMin, rangeMax );
    }

    // Runs through all slices that have a shared region with the input and passes
    // them to the callback. Also passes all slices inbetween that are not in the
    // list, if desired.
    template <typename callbackType>
    AINLINE void ScanSharedSlices( sectorSlice_t sliceShared, const callbackType& cb, bool includeNotPresent = false, bool availableSliceWhole = false )
    {
        AVLNode *avlBeginNode = this->data.avlSliceTree.FindMinimumNodeByCriteria(
            [&]( AVLNode *leftNode )
        {
            return _CompareNodeWithSlice( leftNode, sliceShared, false );
        });

        typename avlSliceTree_t::diff_node_iterator iter( avlBeginNode );

        struct sortedSliceIteratorType
        {
            AINLINE sortedSliceIteratorType( void ) = default;

            AINLINE sortedSliceIteratorType( typename avlSliceTree_t::diff_node_iterator iter ) : iter( std::move( iter ) ), curNode( nullptr )
            {
                this->hasEnded = this->iter.IsEnd();

                if ( this->hasEnded == false )
                {
                    this->curNode = AVL_GETITEM( sliceNode, this->iter.Resolve(), node );
                }
            }

            AINLINE sectorSlice_t GetCurrentRegion( void ) const
            {
                return this->curNode->GetNodeSlice();
            }

            AINLINE bool IsEnd( void ) const
            {
                return this->hasEnded;
            }

            AINLINE void Increment( void )
            {
                this->iter.Increment();

                this->hasEnded = this->iter.IsEnd();

                if ( this->hasEnded == false )
                {
                    this->curNode = AVL_GETITEM( sliceNode, this->iter.Resolve(), node );
                }
            }

            typename avlSliceTree_t::diff_node_iterator iter;

            bool hasEnded;
            sliceNode *curNode;
        };

        sortedSliceIteratorType sorted_iter( std::move( iter ) );

        MathSliceHelpers::ScanSortedSharedSlicesGeneric(
            std::move( sorted_iter ), std::move( sliceShared ),
            [&]( const sectorSlice_t& regionSlice, sortedSliceIteratorType *iter )
            {
                cb( regionSlice, ( iter ? &iter->curNode->metaData : nullptr ) );
            },
            includeNotPresent, availableSliceWhole
        );
    }

    // TODO: create node Detach and node Recommit API that by principle avoids additional memory allocations
    // by removing and reinserting already-allocated nodes, just exposing the meta-data handle to the user.
    // * Of course additional memory allocation is only prevented if removal and reinsertion of nodes is atomic
    // but if the slice structure does change new memory allocation is handled properly.

    AINLINE void DetachData( metaDataType *data )
    {
        sliceNode *node = LIST_GETITEM( sliceNode, data, metaData );

        this->data.avlSliceTree.RemoveByNode( &node->node );

        // You can not treat this structure as if the node never existed
        // inside it, like inserting new slices or removing some, etc.
    }

    template <typename... constrArgs>
    inline void RecommitData( metaDataType *data, constrArgs... args )
    {
        // Remember to not use the data anymore after calling this function.
        // This includes removing the data out of any management structures.

        sliceNode *node = LIST_GETITEM( sliceNode, data, metaData );

        sectorSlice_t hostRegion = node->GetNodeSlice();

        MergeSlice( hostRegion, node, hostRegion, std::forward <constrArgs> ( args )... );
    }

    // If you want to prepare a node to be added to the slices.
    template <typename... constrArgs>
    AINLINE metaDataType* PrepareDecommittedNode( sectorSlice_t slice, constrArgs... args )
    {
        sliceNode *newNode = new_node( std::move( slice ), std::forward <constrArgs> ( args )... );

        return &newNode->metaData;
    }

    // For garbage-canning any nodes. Can also be used on detached nodes.
    template <typename... constrArgs>
    AINLINE void DeleteDecommittedNode( metaDataType *data )
    {
        sliceNode *node = LIST_GETITEM( sliceNode, data, metaData );

#ifdef _DEBUG
        assert( this->data.avlSliceTree.IsNodeInsideTree( &node->node ) == false );
#endif //_DEBUG

        del_node( node );
    }

private:
    static AINLINE eCompResult _CompareNodeWithSlice( const AVLNode *left, const sectorSlice_t& rightSlice, bool doCountInNeighbors )
    {
        const sliceNode *leftNode = AVL_GETITEM( sliceNode, left, node );

        sectorSlice_t leftSlice = leftNode->GetNodeSlice();

        return eir::MathSliceHelpers::CompareSlicesByIntersection( leftSlice, rightSlice, doCountInNeighbors );
    }

    struct metaAVLNodeDispatcher
    {
        static AINLINE eCompResult CompareNodes( const AVLNode *left, const AVLNode *right )
        {
            const sliceNode *leftNode = AVL_GETITEM( sliceNode, left, node );
            const sliceNode *rightNode = AVL_GETITEM( sliceNode, right, node );

            sectorSlice_t leftSlice = leftNode->GetNodeSlice();
            sectorSlice_t rightSlice = rightNode->GetNodeSlice();

            // We assume that nodes never intersect, thus this operation fits perfectly.
            return eir::DefaultValueCompare( leftSlice.GetSliceStartPoint(), rightSlice.GetSliceStartPoint() );
        }
    };

    struct nodeWithoutNeighborComparator
    {
        static AINLINE eCompResult CompareNodeWithValue( const AVLNode *left, const sectorSlice_t& rightSlice )
        {
            return _CompareNodeWithSlice( left, rightSlice, false );
        }
    };

    typedef AVLTree <metaAVLNodeDispatcher> avlSliceTree_t;

    struct fields
    {
        mutable avlSliceTree_t avlSliceTree;
    };

    empty_opt <allocatorType, fields> data;

public:
    // We want to offer standard-iterator looping over slices.
    typedef const sliceNode constSliceNode;
    typedef const SortedSliceSector constSortedSliceSector;
    MAKE_SETMAP_ITERATOR( iterator, constSortedSliceSector, constSliceNode, node, data.avlSliceTree, avlSliceTree_t );

    struct end_std_iterator {};
    struct std_iterator
    {
        AINLINE std_iterator( iterator&& iter ) : iter( std::move( iter ) )
        {
            return;
        }

        AINLINE bool operator != ( const end_std_iterator& ) const
        {
            return ( iter.IsEnd() == false );
        }

        AINLINE std_iterator& operator ++ ( void )
        {
            iter.Increment();
            return *this;
        }

        AINLINE const metaDataType& operator * ( void )
        {
            return iter.Resolve()->metaData;
        }

    private:
        iterator iter;
    };
    AINLINE std_iterator begin( void ) const    { return std_iterator( iterator( *this ) ); }
    AINLINE end_std_iterator end( void ) const  { return end_std_iterator(); }

    AINLINE iterator GetFirstIter( void )
    {
        if ( AVLNode *avlFirstNode = this->data.avlSliceTree.GetSmallestNode() )
        {
            return AVL_GETITEM( sliceNode, avlFirstNode, node );
        }

        return iterator();
    }

    AINLINE iterator GetLastIter( void )
    {
        if ( AVLNode *avlLastNode = this->data.avlSliceTree.GetBiggestNode() )
        {
            return AVL_GETITEM( sliceNode, avlLastNode, node );
        }

        return iterator();
    }
};

}

#endif //_EIR_SORTED_SLICE_SECTOR_HEADER_
