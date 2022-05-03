/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/AVLTree.helpers.h
*  PURPOSE:     AVL-tree helper algorithms and structures
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _AVLTREE_HELPER_HEADER_
#define _AVLTREE_HELPER_HEADER_

#include "MacroUtils.h"
#include "AVLTree.h"

// Not placed in the main header because the difference of AVLTree types introduces great complexity.
template <typename nodeComparatorType, typename leftAVLTreeType, typename rightAVLTreeType>
AINLINE bool EqualsAVLTrees( leftAVLTreeType& left, rightAVLTreeType& right, const nodeComparatorType& cb )
{
    typename leftAVLTreeType::diff_node_iterator left_iter( left );
    typename rightAVLTreeType::diff_node_iterator right_iter( right );

    while ( true )
    {
        bool isLeftEnd = left_iter.IsEnd();
        bool isRightEnd = right_iter.IsEnd();

        if ( isLeftEnd && isRightEnd )
        {
            return true;
        }
        else if ( isLeftEnd || isRightEnd )
        {
            return false;
        }

        // If any value is different then we do not match.
        const AVLNode *leftNode = left_iter.Resolve();
        const AVLNode *rightNode = right_iter.Resolve();

        if ( cb( leftNode, rightNode ) == false )
        {
            return false;
        }

        left_iter.Increment();
        right_iter.Increment();
    }

    // Never reaches this point.
    return false;
}

#endif //_AVLTREE_HELPER_HEADER_