/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/MathSlice.algorithms.h
*  PURPOSE:     Algorithm implementations that work with MathSlice.h
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_SDK_MATH_SLICE_ALGORITHMS_
#define _EIR_SDK_MATH_SLICE_ALGORITHMS_

#include "MathSlice.h"

namespace eir
{

namespace MathSliceHelpers
{

template <typename sliceNumberType>
static AINLINE eCompResult CompareSlicesByIntersection( const eir::mathSlice <sliceNumberType>& leftSlice, const eir::mathSlice <sliceNumberType>& rightSlice, bool doCountInNeighbors )
{
    // Here we can have an intersection, so treat any intersection as equality.
    eir::eIntersectionResult intResult = leftSlice.intersectWith( rightSlice );

    if ( intResult == eir::INTERSECT_UNKNOWN )
    {
        // Let's make ourselves more robust to hardware math errors and stuff.
        bool isLeftEmpty = leftSlice.IsEmpty();
        bool isRightEmpty = rightSlice.IsEmpty();

        // Those empty cases are going to treat well empty-slice requests.
        if ( isLeftEmpty && !isRightEmpty )
        {
            return eCompResult::LEFT_LESS;
        }
        if ( isRightEmpty && !isLeftEmpty )
        {
            return eCompResult::LEFT_GREATER;
        }

        // No idea. Just return something diverting.
        // As long as we do not return EQUAL it is fine.
        return eCompResult::LEFT_LESS;
    }
    if ( intResult == eir::INTERSECT_FLOATING_START )
    {
        if ( doCountInNeighbors )
        {
            // We are also the same if we are neighboring slices.
            if ( AreBoundsTight( leftSlice.GetSliceEndBound(), rightSlice.GetSliceStartBound() ) )
            {
                return eCompResult::EQUAL;
            }
        }

        return eCompResult::LEFT_LESS;
    }
    if ( intResult == eir::INTERSECT_FLOATING_END )
    {
        if ( doCountInNeighbors )
        {
            // Both slices have to have meeting points.
            if ( AreBoundsTight( rightSlice.GetSliceEndBound(), leftSlice.GetSliceStartBound() ) )
            {
                return eCompResult::EQUAL;
            }
        }

        return eCompResult::LEFT_GREATER;
    }

    // Have to treat this specially.
    return eCompResult::EQUAL;
}

// The_GTA: I at first wanted to use C++20 coroutines for this task but then I realized the weird but well argumented heap-memory-dependency of
//  the proposed "co_wait coroutines". Turns out that those are not meant for highly reliable code, NT kernel code anyway.

template <typename sortedSliceIteratorType, typename sliceNumberType>
struct intervalCutForwardIterator
{
    AINLINE intervalCutForwardIterator( sortedSliceIteratorType iter, eir::mathSlice <sliceNumberType> sliceShared, bool includeNotPresent = false, bool availableSliceWhole = false )
        : iter( std::move( iter ) ), sliceShared( std::move( sliceShared ) ), curStartBound( sliceShared.GetSliceStartBound() ),
          includeNotPresent( includeNotPresent ), availableSliceWhole( availableSliceWhole ),
          iter_state( eIterationState::CHECK_NOT_PRESENT )
    {
        return;
    }

    AINLINE intervalCutForwardIterator( const intervalCutForwardIterator& ) = default;
    AINLINE intervalCutForwardIterator( intervalCutForwardIterator&& ) = default;

    AINLINE ~intervalCutForwardIterator( void ) = default;

    AINLINE intervalCutForwardIterator& operator = ( const intervalCutForwardIterator& ) = default;
    AINLINE intervalCutForwardIterator& operator = ( intervalCutForwardIterator&& ) = default;

    AINLINE bool FetchNext( eir::mathSlice <sliceNumberType>& sliceOut, bool& actualOut, sortedSliceIteratorType& iterOut )
    {
    try_again:
        {
            eIterationState iter_state = this->iter_state;

            if ( iter_state == eIterationState::CHECK_NOT_PRESENT )
            {
                // Have we reached the end of actual regions?
                if ( iter.IsEnd() )
                {
                    goto handle_last_item;
                }

                // Fetch the current region.
                curSlice = iter.GetCurrentRegion();

                // Is that node still in the region?
                eCompResult cmpRes = CompareSlicesByIntersection( curSlice, sliceShared, false );

                if ( cmpRes == eCompResult::LEFT_LESS )
                {
                    iter.Increment();

                    goto try_again;
                }

                if ( cmpRes == eCompResult::LEFT_GREATER )
                {
                    goto handle_last_item;
                }

                // Now process the actual status.
                bool hasRegion = false;

                if ( this->includeNotPresent )
                {
                    // If we have a region prior to our region, then we must report it.
                    if ( curStartBound < curSlice.GetSliceStartBound() )
                    {
                        slice_t unavailSlice = slice_t::fromBounds( curStartBound, GetTightBound( curSlice.GetSliceStartBound() ) );

                        sliceOut = std::move( unavailSlice );
                        actualOut = false;
                        iterOut = this->iter;

                        hasRegion = true;
                    }

                    // Update the current start point.
                    curStartBound = GetTightBound( curSlice.GetSliceEndBound() );
                }

                this->iter_state = eIterationState::CHECK_PRESENT;

                if ( hasRegion )
                {
                    return true;
                }

                goto try_again;
            }
            else if ( iter_state == eIterationState::CHECK_PRESENT )
            {
                // Report the regular stuff.
                if ( availableSliceWhole )
                {
                    sliceOut = std::move( curSlice );
                }
                else
                {
                    slice_t objSharedSlice;

#ifdef _DEBUG
                    bool gotShared =
#endif //_DEBUG
                    curSlice.getSharedRegion( sliceShared, objSharedSlice );

#ifdef _DEBUG
                    FATAL_ASSERT( gotShared == true );
#endif //_DEBUG

                    sliceOut = std::move( objSharedSlice );
                }

                actualOut = true;
                iterOut = this->iter;

                this->iter.Increment();
                this->iter_state = eIterationState::CHECK_NOT_PRESENT;

                return true;
            }
            else
            {
                FATAL_ASSERT( 0 );
            }
        }

        return false;

    handle_last_item:
        {
            if ( includeNotPresent )
            {
                // Check if we have any unavailable slice at the end.
                if ( curStartBound <= sliceShared.GetSliceEndBound() )
                {
                    slice_t unavailSlice = slice_t::fromBounds( curStartBound, sliceShared.GetSliceEndBound() );

                    curStartBound = GetTightBound( sliceShared.GetSliceEndBound() );

                    sliceOut = std::move( unavailSlice );
                    actualOut = false;
                    iterOut = this->iter;

                    return true;
                }
            }

            return false;
        }
    }

private:
    enum class eIterationState
    {
        CHECK_NOT_PRESENT,
        CHECK_PRESENT
    };

    typedef eir::mathSlice <sliceNumberType> slice_t;

    sortedSliceIteratorType iter;
    slice_t sliceShared;
    lowerBound <sliceNumberType> curStartBound;
    slice_t curSlice;

    bool includeNotPresent;
    bool availableSliceWhole;

    eIterationState iter_state;
};

template <typename sortedSliceBackwardIteratorType, typename sliceNumberType>
struct intervalCutBackwardIterator
{
    AINLINE intervalCutBackwardIterator( sortedSliceBackwardIteratorType iter, eir::mathSlice <sliceNumberType> sliceShared, bool includeNotPresent = false, bool availableSliceWhole = false )
        : iter( std::move( iter ) ), sliceShared( std::move( sliceShared ) ), curEndBound( sliceShared.GetSliceEndBound() ),
          includeNotPresent( includeNotPresent ), availableSliceWhole( availableSliceWhole ),
          iter_state( eIterationState::CHECK_NOT_PRESENT )
    {
        return;
    }

    AINLINE intervalCutBackwardIterator( const intervalCutBackwardIterator& ) = default;
    AINLINE intervalCutBackwardIterator( intervalCutBackwardIterator&& ) = default;

    AINLINE ~intervalCutBackwardIterator( void ) = default;

    AINLINE intervalCutBackwardIterator& operator = ( const intervalCutBackwardIterator& ) = default;
    AINLINE intervalCutBackwardIterator& operator = ( intervalCutBackwardIterator&& ) = default;

    AINLINE bool FetchNext( eir::mathSlice <sliceNumberType>& sliceOut, bool& actualOut, sortedSliceBackwardIteratorType& iterOut )
    {
    try_again:
        {
            eIterationState iter_state = this->iter_state;

            if ( iter_state == eIterationState::CHECK_NOT_PRESENT )
            {
                // Have we reached the end of actual regions?
                if ( iter.IsEnd() )
                {
                    goto handle_last_item;
                }

                // Fetch the current region.
                curSlice = iter.GetCurrentRegion();

                // Is that node still in the region?
                eCompResult cmpRes = CompareSlicesByIntersection( curSlice, sliceShared, false );

                if ( cmpRes == eCompResult::LEFT_GREATER )
                {
                    iter.Decrement();

                    goto try_again;
                }

                if ( cmpRes == eCompResult::LEFT_LESS )
                {
                    goto handle_last_item;
                }

                // Now process the actual status.
                bool hasRegion = false;

                if ( this->includeNotPresent )
                {
                    // If we have a region prior to our region, then we must report it.
                    if ( curEndBound > curSlice.GetSliceEndBound() )
                    {
                        slice_t unavailSlice = slice_t::fromBounds( GetTightBound( curSlice.GetSliceEndBound() ), curEndBound );

                        sliceOut = std::move( unavailSlice );
                        actualOut = false;
                        iterOut = this->iter;

                        hasRegion = true;
                    }

                    // Update the current start point.
                    curEndBound = GetTightBound( curSlice.GetSliceStartBound() );
                }

                this->iter_state = eIterationState::CHECK_PRESENT;

                if ( hasRegion )
                {
                    return true;
                }

                goto try_again;
            }
            else if ( iter_state == eIterationState::CHECK_PRESENT )
            {
                // Report the regular stuff.
                if ( availableSliceWhole )
                {
                    sliceOut = std::move( curSlice );
                }
                else
                {
                    slice_t objSharedSlice;

#ifdef _DEBUG
                    bool gotShared =
#endif //_DEBUG
                    curSlice.getSharedRegion( sliceShared, objSharedSlice );

#ifdef _DEBUG
                    FATAL_ASSERT( gotShared == true );
#endif //_DEBUG

                    sliceOut = std::move( objSharedSlice );
                }

                actualOut = true;
                iterOut = this->iter;

                this->iter.Decrement();
                this->iter_state = eIterationState::CHECK_NOT_PRESENT;

                return true;
            }
            else
            {
                FATAL_ASSERT( 0 );
            }
        }

        return false;

    handle_last_item:
        {
            if ( includeNotPresent )
            {
                // Check if we have any unavailable slice at the end.
                if ( curEndBound >= sliceShared.GetSliceStartBound() )
                {
                    slice_t unavailSlice = slice_t::fromBounds( sliceShared.GetSliceStartBound(), curEndBound );

                    curEndBound = GetTightBound( sliceShared.GetSliceStartBound() );

                    sliceOut = std::move( unavailSlice );
                    actualOut = false;
                    iterOut = this->iter;

                    return true;
                }
            }

            return false;
        }
    }

private:
    enum class eIterationState
    {
        CHECK_NOT_PRESENT,
        CHECK_PRESENT
    };

    typedef eir::mathSlice <sliceNumberType> slice_t;

    sortedSliceBackwardIteratorType iter;
    slice_t sliceShared;
    upperBound <sliceNumberType> curEndBound;
    slice_t curSlice;

    bool includeNotPresent;
    bool availableSliceWhole;

    eIterationState iter_state;
};

// TODO: turn this stack-based function into an iterator, so we can abort iteration at any moment (when for instance one block fails).
template <bool trueForwardFalseBackward = true, typename sortedSliceIteratorType, typename sliceNumberType, typename sliceCallbackType>
static AINLINE void ScanSortedSharedSlicesGeneric(
    sortedSliceIteratorType internal_iter, eir::mathSlice <sliceNumberType> sliceShared, const sliceCallbackType& cb, bool includeNotPresent = false, bool availableSliceWhole = false
)
{
    /* REQUIREMENTS FOR sortedSliceIteratorType IF FORWARD:
        - default-constructor;
        - eir::mathSlice <sliceNumberType> GetCurrentRegion( void ) const;
        - bool IsEnd( void ) const;
        - void Increment( void );
       REQUIREMENTS FOR sortedSliceIteratorType IF BACKWARD:
        - default-constructor;
        - eir::mathSlice <sliceNumberType> GetCurrentRegion( void ) const;
        - bool IsEnd( void ) const;
        - void Decrement( void );
       REQUIREMENTS FOR sliceCallbackType:
        - void sliceCallbackType( const eir::mathSlice <sliceNumberType>& regionSlice, sortedSliceIteratorType *iterPtr );
    */

    typedef eir::mathSlice <sliceNumberType> slice_t;

    if constexpr ( trueForwardFalseBackward )
    {
        intervalCutForwardIterator <sortedSliceIteratorType, sliceNumberType> iter( std::move( internal_iter ), std::move( sliceShared ), includeNotPresent, availableSliceWhole );

        slice_t regionSlice;
        bool isPresentRegion;
        sortedSliceIteratorType regionIter;

        while ( iter.FetchNext( regionSlice, isPresentRegion, regionIter ) )
        {
            cb( (const slice_t&)regionSlice, ( isPresentRegion ? &regionIter : nullptr ) );
        }
    }
    else
    {
        intervalCutBackwardIterator <sortedSliceIteratorType, sliceNumberType> iter( std::move( internal_iter ), std::move( sliceShared ), includeNotPresent, availableSliceWhole );

        slice_t regionSlice;
        bool isPresentRegion;
        sortedSliceIteratorType regionIter;

        while ( iter.FetchNext( regionSlice, isPresentRegion, regionIter ) )
        {
            cb( (const slice_t&)regionSlice, ( isPresentRegion ? &regionIter : nullptr ) );
        }
    }
}

}

}

#endif //_EIR_SDK_MATH_SLICE_ALGORITHMS_
