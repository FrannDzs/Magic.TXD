/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/OptimizedVector.h
*  PURPOSE:     Vector optimization helpers
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EIRREPO_VECTOR_OPTIMIZATION_HEADER_
#define _EIRREPO_VECTOR_OPTIMIZATION_HEADER_

#include "Vector.h"
#include "EmptyVector.h"

namespace eir
{

namespace hidden_detail
{

template <bool is_empty, typename structType, MemoryAllocator allocatorType, typename exceptMan>
struct SelectOptimizedVector
{
};

template <typename structType, MemoryAllocator allocatorType, typename exceptMan>
struct SelectOptimizedVector <true, structType, allocatorType, exceptMan>
{
    typedef EmptyVector <structType, exceptMan> type;
};
template <typename structType, MemoryAllocator allocatorType, typename exceptMan>
struct SelectOptimizedVector <false, structType, allocatorType, exceptMan>
{
    typedef Vector <structType, allocatorType, exceptMan> type;
};

} // namespace hidden_detail

template <typename structType, MemoryAllocator allocatorType, typename exceptMan = eir::DefaultExceptionManager>
using OptimizedVector = hidden_detail::SelectOptimizedVector <std::is_empty <structType>::value, structType, allocatorType, exceptMan>::type;

} // namespace eir

#endif //_EIRREPO_VECTOR_OPTIMIZATION_HEADER_