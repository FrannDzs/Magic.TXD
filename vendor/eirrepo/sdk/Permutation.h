/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Permutation.h
*  PURPOSE:     Buffer permutation utilities.
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIR_PERMUTATION_HEADER_
#define _EIR_PERMUTATION_HEADER_

// For CEIL_DIV.
#include "MemoryRaw.h"

namespace eir
{

template <typename numType>
struct nullPermuter
{
    static_assert( std::is_integral <numType>::value );

    AINLINE static numType getInputWidth( void )
    {
        return 1;
    }

    AINLINE static numType getInputHeight( void )
    {
        return 1;
    }

    AINLINE static numType getOutputWidth( void )
    {
        return 1;
    }

    AINLINE static numType getOutputHeight( void )
    {
        return 1;
    }

    AINLINE static void permute( numType x, numType y, numType& tx, numType& ty, numType wcolidx, numType hcolidx )
    {
        tx = 0;
        ty = 0;
    }
};

template <typename numType, typename permuter>
AINLINE void performLocal2DPermute( numType& x, numType& y, numType sw, numType sh, numType wcolidx, numType hcolidx, const permuter& perm )
{
    numType slx = ( x % sw );
    numType sly = ( y % sh );

    numType sox = ( x - slx );
    numType soy = ( y - sly );

    numType tx, ty;
    perm.permute( slx, sly, tx, ty, wcolidx, hcolidx );

    x = ( sox + tx );
    y = ( soy + ty );
}

template <typename... permuters>
struct stackedPermuter
{
    typedef decltype(((typename std::tuple_element <0, std::tuple <permuters...>>::type*)0)->getSwizzleWidth()) wnumType;
    typedef decltype(((typename std::tuple_element <0, std::tuple <permuters...>>::type*)0)->getSwizzleHeight()) hnumType;

    AINLINE stackedPermuter( permuters&&... perms ) : perms( std::forward <permuters> ( perms )... )
    {
        return;
    }

    AINLINE wnumType getInputWidth( void ) const
    {
        return std::max( std::get <permuters> ( perms ).getInputWidth()... );
    }

    AINLINE hnumType getInputHeight( void ) const
    {
        return std::max( std::get <permuters> ( perms ).getInputHeight()... );
    }

    AINLINE wnumType getOutputWidth( void ) const
    {
        return std::max( std::get <permuters> ( perms ).getOutputWidth()... );
    }

    AINLINE hnumType getOutputHeight( void ) const
    {
        return std::max( std::get <permuters> ( perms ).getOutputHeight()... );
    }

    AINLINE void permute( wnumType x, hnumType y, wnumType& tx, hnumType& ty, wnumType wcolidx, hnumType hcolidx ) const
    {
        ( performLocal2DPermute( x, y, std::get <permuters> ( perms ).getInputWidth(), std::get <permuters> ( perms ).getInputHeight(), wcolidx, hcolidx, std::get <permuters> ( perms ) ), ... );

        tx = x;
        ty = y;
    }

    std::tuple <permuters...> perms;
};

template <typename coordPermuter, typename wdimmNumType, typename hdimmNumType>
AINLINE void permute2DCoord(
    wdimmNumType x, hdimmNumType y,
    const coordPermuter& perm,
    wdimmNumType& xOut, hdimmNumType& yOut
)
{
    wdimmNumType inputDimmW = perm.getInputWidth();
    hdimmNumType inputDimmH = perm.getInputHeight();

    wdimmNumType outputDimmW = perm.getOutputWidth();
    hdimmNumType outputDimmH = perm.getOutputHeight();

    // Preparation for y coord.
    hdimmNumType ycol = ( y / inputDimmH );
    hdimmNumType permy = ( y % inputDimmH );

    hdimmNumType output_ycol_pixoff = ( ycol * outputDimmH );

    // Preparation for x coord.
    wdimmNumType xcol = ( x / inputDimmW );
    wdimmNumType permx = ( x % inputDimmW );

    wdimmNumType output_xcol_pixoff = ( xcol * outputDimmW );

    // Perform the permute.
    wdimmNumType tx; hdimmNumType ty;
    perm.permute( permx, permy, tx, ty, xcol, ycol );

    // Return the result.
    xOut = ( output_xcol_pixoff + tx );
    yOut = ( output_ycol_pixoff + ty );
}

template <typename callbackType, typename srcCoordPermuter, typename dstCoordPermuter, typename wdimmNumType, typename hdimmNumType>
AINLINE void permute2DBuffer(
    wdimmNumType width, hdimmNumType height,
    const srcCoordPermuter& src_perm, const dstCoordPermuter& dst_perm,
    callbackType&& item_cb
)
{
    static_assert( std::is_integral <wdimmNumType>::value && std::is_integral <hdimmNumType>::value );

    for ( hdimmNumType y = 0; y < height; y++ )
    {
        for ( wdimmNumType x = 0; x < width; x++ )
        {
            wdimmNumType src_x; hdimmNumType src_y;
            permute2DCoord( x, y, src_perm, src_x, src_y );

            wdimmNumType dst_x; hdimmNumType dst_y;
            permute2DCoord( x, y, dst_perm, dst_x, dst_y );

            item_cb( src_x, src_y, dst_x, dst_y );
        }
    }
}

} // namespace eir

#endif //_EIR_PERMUTATION_HEADER_
