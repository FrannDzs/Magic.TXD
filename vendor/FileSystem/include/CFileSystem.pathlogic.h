/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/include/CFileSystem.pathlogic.h
*  PURPOSE:     Path logic helpers
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_PATHLOGIC_HEADER_
#define _FILESYSTEM_PATHLOGIC_HEADER_

#include <CFileSystem.common.h>

#include <tuple>
#include <algorithm>

// Here for now for fast prototyping reasons.
namespace PathSegmentation
{

template <typename... pathViews>
struct PathSegmenter
{
    static constexpr size_t segment_count = sizeof... ( pathViews );

private:
    template <typename viewType>
    AINLINE void parse_segment( const viewType& segment_item, size_t seg_idx )
    {
        segment_info_t& seg_info = this->segment_info[ seg_idx ];

        size_t backCount = segment_item.BackCount();

        for ( size_t n = 1; n < seg_idx; n++ )
        {
            size_t backward_idx = ( seg_idx - n );

            segment_info_t& backplay_info = this->segment_info[ backward_idx ];

            size_t backplay_travelCount = backplay_info.realTravelCount;

            if ( backplay_travelCount > backCount )
            {
                backplay_info.realTravelCount = ( backplay_travelCount - backCount );

                backCount = 0;
            }
            else
            {
                backplay_info.realTravelCount = 0;

                backCount -= backplay_travelCount;
            }

            if ( backCount == 0 )
            {
                break;
            }
        }

        if ( backCount > 0 && seg_idx > 0 )
        {
            segment_info_t& first_seg = this->segment_info[ 0 ];

            size_t first_seg_travelCount = first_seg.realTravelCount;
                
            if ( first_seg_travelCount > backCount )
            {
                first_seg.realTravelCount = ( first_seg_travelCount - backCount );
            }
            else
            {
                first_seg.realTravelCount = 0;

                backCount -= first_seg_travelCount;

                first_seg.realBackCount += backCount;
            }

            backCount = 0;
        }
            
        seg_info.tokens = segment_item.GetItems();
        seg_info.realTravelCount = segment_item.ItemCount();
        seg_info.realBackCount = backCount;
    }

public:
    AINLINE PathSegmenter( pathViews&&... views )
    {
        // Build the travelCounts.
        {
            size_t seg_idx = 0;

            ((parse_segment( std::forward <pathViews> ( views ), seg_idx++ )), ...);
        }

        // Build the first segment info.
        {
            segment_info_t& first_seg_info = this->segment_info[ 0 ];

            first_seg_info.segment_start = 0;
            first_seg_info.segment_end = first_seg_info.realTravelCount;
        }

        for ( size_t n = 1; n < segment_count; n++ )
        {
            segment_info_t& seg_info = this->segment_info[ n ];

            const segment_info_t& prev_seg_info = this->segment_info[ n - 1 ];

            size_t seg_start = prev_seg_info.segment_end;

            seg_info.segment_start = seg_start;
            seg_info.segment_end = ( seg_start + seg_info.realTravelCount );
        }
    }

    AINLINE const filePath& GetItem( size_t idx ) const
    {
        for ( size_t n = 0; n < segment_count; n++ )
        {
            const segment_info_t& info = this->segment_info[ n ];

            size_t segment_start = info.segment_start;

            if ( idx >= segment_start && idx < info.segment_end )
            {
                return info.tokens[ idx - segment_start ];
            }
        }

        throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
    }

    AINLINE size_t GetSegmentTravelCount( size_t seg_idx ) const
    {
        assert( seg_idx < segment_count );

        return segment_info[ seg_idx ].realTravelCount;
    }

    AINLINE size_t GetSegmentBackCount( size_t seg_idx ) const
    {
        assert( seg_idx < segment_count );

        return segment_info[ seg_idx ].realBackCount;
    }

    AINLINE size_t GetSegmentStartIndex( size_t seg_idx ) const
    {
        assert( seg_idx < segment_count );

        return segment_info[ seg_idx ].segment_start;
    }
    
    AINLINE size_t GetSegmentEndOffset( size_t seg_idx ) const
    {
        assert( seg_idx < segment_count );

        return segment_info[ seg_idx ].segment_end;
    }

    AINLINE size_t GetTravelCount( void ) const
    {
        return this->segment_info[ segment_count - 1 ].segment_end;
    }

    AINLINE size_t GetBackCount( void ) const
    {
        return this->segment_info[ 0 ].realBackCount;
    }

    AINLINE size_t ChopToSegment( size_t seg_idx, size_t travelIndex ) const
    {
        assert( seg_idx < segment_count );

        const segment_info_t& seg_info = this->segment_info[ seg_idx ];

        size_t seg_start = seg_info.segment_start;
        size_t seg_end = seg_info.segment_end;

        if ( travelIndex <= seg_start )
        {
            return 0;
        }

        if ( travelIndex >= seg_end )
        {
            return seg_info.realTravelCount;
        }

        return ( travelIndex - seg_start );
    }

private:
    struct segment_info_t
    {
        const filePath *tokens;
        size_t realBackCount;
        size_t realTravelCount;
        size_t segment_start;
        size_t segment_end;
    };

    segment_info_t segment_info[ segment_count ];
};

namespace helpers
{

// Wraps a normalNodePath object for use in PathSegmenter.
struct normalPathView
{
    AINLINE normalPathView( const normalNodePath& thePath ) : path( thePath )
    {
        return;
    };

    AINLINE size_t BackCount( void ) const
    {
        return this->path.backCount;
    }

    AINLINE size_t ItemCount( void ) const
    {
        if ( path.isFilePath )
        {
            return this->path.travelNodes.GetCount() - 1;
        }

        return this->path.travelNodes.GetCount();
    }

    AINLINE const filePath* GetItems( void ) const
    {
        return this->path.travelNodes.GetData();
    }

    const normalNodePath& path;
};

// Represents full-path nodes.
struct fullDirNodesView
{
    AINLINE fullDirNodesView( const dirNames& nodes ) : nodes( nodes )
    {
        return;
    }

    AINLINE size_t BackCount( void ) const
    {
        return 0;
    }

    AINLINE size_t ItemCount( void ) const
    {
        return nodes.GetCount();
    }

    AINLINE const filePath* GetItems( void ) const
    {
        return nodes.GetData();
    }

    const dirNames& nodes;
};

// Travelling algorithm for filePath.
template <typename firstSegmenterType, typename secondSegmenterType>
AINLINE void TravelPaths( size_t& travelIndex, const firstSegmenterType& segFirst, const secondSegmenterType& segSecond, bool caseSensitive )
{
    size_t maxTravel = std::min( segFirst.GetTravelCount(), segSecond.GetTravelCount() );

    while ( travelIndex < maxTravel )
    {
        const filePath& leftItem = segFirst.GetItem( travelIndex );
        const filePath& rightItem = segSecond.GetItem( travelIndex );

        if ( leftItem.equals( rightItem, caseSensitive ) == false )
        {
            break;
        }

        travelIndex++;
    }
}

} // namespace helpers

} // namespace PathSegmentation

#endif //_FILESYSTEM_PATHLOGIC_HEADER_