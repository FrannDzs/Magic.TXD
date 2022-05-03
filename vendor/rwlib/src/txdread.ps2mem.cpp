/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.ps2mem.cpp
*  PURPOSE:     PlayStation2 native texture implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2

#include "txdread.ps2.hxx"

#include "txdread.ps2gsman.hxx"

namespace rw
{

struct ps2GSMemoryLayoutManager
{
    static constexpr size_t NUM_BLOCKS_PER_PAGE = 32u;

    // Forward declaration.
    struct MemoryAllocation;
    struct VirtualMemoryPage;

    struct MemoryBlock
    {
        AINLINE MemoryBlock( void ) noexcept
        {
            this->residingAllocation = nullptr;
            this->inPageBlockIdx = 0;
            this->pageOfBlock = nullptr;
            this->memLayout_allocPageX = 0;
            this->memLayout_allocPageY = 0;
        }
        AINLINE MemoryBlock( const MemoryBlock& ) = delete;

    private:
        AINLINE void moveFrom( MemoryBlock&& right ) noexcept
        {
            MemoryAllocation *residingAllocation = right.residingAllocation;

            this->residingAllocation = residingAllocation;

            if ( residingAllocation )
            {
                this->allocationBlocksNode.moveFrom( std::move( right.allocationBlocksNode ) );
            }

            this->memLayout_allocPageX = right.memLayout_allocPageX;
            this->memLayout_allocPageY = right.memLayout_allocPageY;

            right.residingAllocation = nullptr;
            right.memLayout_allocPageX = 0;
            right.memLayout_allocPageY = 0;
        }

    public:
        AINLINE MemoryBlock( MemoryBlock&& right ) noexcept
        {
            moveFrom( std::move( right ) );
        }

        AINLINE ~MemoryBlock( void )
        {
            // We really should not be cleared while somebody is owning us.
            assert( this->residingAllocation == nullptr );
        }

        AINLINE MemoryBlock& operator = ( const MemoryBlock& ) = delete;
        AINLINE MemoryBlock& operator = ( MemoryBlock&& right )
        {
            // Cannot squash a block, seriously.
            if ( this->residingAllocation != nullptr )
            {
                throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
            }

            moveFrom( std::move( right ) );

            return *this;
        }

        inline bool IsOwned( void ) const
        {
            return ( this->residingAllocation != nullptr );
        }

        uint32 inPageBlockIdx;
        VirtualMemoryPage *pageOfBlock;

        MemoryAllocation *residingAllocation;

        // *** Allocation meta-data (only valid of residingAllocation != nullptr).
        uint32 memLayout_allocPageX;        // memory block x offset into page
        uint32 memLayout_allocPageY;        // memory block y offset into page

        RwListEntry <MemoryBlock> allocationBlocksNode;
    };

    struct MemoryAllocation
    {
        inline MemoryAllocation( void ) noexcept
        {
            return;
        }

    private:
        AINLINE void movefrom( MemoryAllocation&& right ) noexcept
        {
            // Take over all the allocated-on-blocks.
            LIST_FOREACH_BEGIN( MemoryBlock, right.allocated_on_blocks.root, allocationBlocksNode )

                item->residingAllocation = this;

            LIST_FOREACH_END

            this->allocated_on_blocks = std::move( right.allocated_on_blocks );
        }

    public:
        inline MemoryAllocation( const MemoryAllocation& ) = delete;
        inline MemoryAllocation( MemoryAllocation&& right ) noexcept
        {
            movefrom( std::move( right ) );
        }

    private:
        AINLINE void clear( void ) noexcept
        {
            // Clean up all allocations.
            LIST_FOREACH_BEGIN( MemoryBlock, this->allocated_on_blocks.root, allocationBlocksNode )

                item->residingAllocation = nullptr;
                item->memLayout_allocPageX = 0;
                item->memLayout_allocPageY = 0;

            LIST_FOREACH_END

            LIST_CLEAR( this->allocated_on_blocks.root );
        }

    public:
        inline ~MemoryAllocation( void )
        {
            clear();
        }

        inline MemoryAllocation& operator = ( const MemoryAllocation& ) = delete;
        inline MemoryAllocation& operator = ( MemoryAllocation&& right ) noexcept
        {
            clear();
            movefrom( std::move( right ) );

            return *this;
        }

        inline void AddMemoryBlockOwnership( MemoryBlock& theBlock, uint32 memLayout_allocPageX, uint32 memLayout_allocPageY )
        {
#ifdef _DEBUG
            assert( theBlock.residingAllocation == nullptr );
#endif //_DEBUG

            theBlock.memLayout_allocPageX = memLayout_allocPageX;
            theBlock.memLayout_allocPageY = memLayout_allocPageY;

            theBlock.residingAllocation = this;

            LIST_APPEND( this->allocated_on_blocks.root, theBlock.allocationBlocksNode );
        }

        // If resident then this list does contain all blocks that we are currently allocated on.
        RwList <MemoryBlock> allocated_on_blocks;
    };

    // Forward declaration.
    struct pageRegion;

    struct VirtualMemoryPage
    {
        inline VirtualMemoryPage( Interface *engineInterface, pageRegion *owner, uint32 page_idx )
        {
            this->engineInterface = engineInterface;
            this->page_idx = page_idx;

            uint32 idx = 0;

            for ( MemoryBlock& block : blocks )
            {
                block.inPageBlockIdx = idx++;
                block.pageOfBlock = this;
            }

            this->owner = owner;
        }
        inline VirtualMemoryPage( const VirtualMemoryPage& ) = delete;
        inline VirtualMemoryPage( VirtualMemoryPage&& ) = delete;

        inline ~VirtualMemoryPage( void )
        {
            //Interface *engineInterface = this->engineInterface;
        }

        inline VirtualMemoryPage& operator = ( const VirtualMemoryPage& ) = delete;
        inline VirtualMemoryPage& operator = ( VirtualMemoryPage&& ) = delete;

        inline uint32 GetBlockOffset( void ) const
        {
            return ( this->page_idx * 32u );
        }

        inline uint32 GetTextureBasePointer( void ) const
        {
            return ( GetBlockOffset() * 64u );
        }

        inline MemoryBlock& GetMemoryBlockByLayout( const ps2MemoryLayoutTypeProperties& layoutProps, uint32 blockidx )
        {
            if ( blockidx >= NUM_BLOCKS_PER_PAGE )
            {
                throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
            }

            uint32 layoutPageLocalBlockIndex
                = layoutProps.blockArrangementInPage[ blockidx ];

            MemoryBlock& theBlock = this->blocks[ layoutPageLocalBlockIndex ];

            return theBlock;
        }

        template <typename callbackType>
        AINLINE void IterateThroughBlocksUsingCoords( const ps2MemoryLayoutTypeProperties& layoutProps, callbackType&& cb )
        {
            uint32 widthBlocksPerPage = layoutProps.widthBlocksPerPage;
            uint32 heightBlocksPerPage = layoutProps.heightBlocksPerPage;

            for ( uint32 y = 0; y < heightBlocksPerPage; y++ )
            {
                for ( uint32 x = 0; x < widthBlocksPerPage; x++ )
                {
                    uint32 linear_blockidx = ( y * widthBlocksPerPage + x );

#ifdef _DEBUG
                    assert( linear_blockidx < NUM_BLOCKS_PER_PAGE );
#endif //_DEBUG

                    uint32 layout_blockidx = layoutProps.blockArrangementInPage[ linear_blockidx ];

#ifdef _DEBUG
                    assert( layout_blockidx < NUM_BLOCKS_PER_PAGE );
#endif //_DEBUG

                    MemoryBlock& curBlock = blocks[ layout_blockidx ];

                    cb( x, y, curBlock );
                }
            }
        }

        template <typename callbackType>
        AINLINE void IterateThroughBlocksUsingCoordsConst( const ps2MemoryLayoutTypeProperties& layoutProps, callbackType&& cb ) const
        {
            const_cast <VirtualMemoryPage*> ( this )->IterateThroughBlocksUsingCoords( layoutProps,
                [&]( uint32 x, uint32 y, MemoryBlock& block )
                {
                    cb( x, y, (const MemoryBlock&)block );
                }
            );
        }

        AINLINE bool GetLastUsedBlockRowIndex( const ps2MemoryLayoutTypeProperties& layoutProps, uint32& usedBlockRowOut ) const
        {
            bool hasUsedBlockRow = false;
            uint32 usedBlockRow = 0;

            IterateThroughBlocksUsingCoordsConst( layoutProps,
                [&]( uint32 x, uint32 y, const MemoryBlock& block )
                {
                    if ( block.IsOwned() && ( hasUsedBlockRow == false || usedBlockRow < y ) )
                    {
                        usedBlockRow = y;
                        hasUsedBlockRow = true;
                    }
                }
            );

            usedBlockRowOut = usedBlockRow;
            return hasUsedBlockRow;
        }

        Interface *engineInterface;

        // The region that this page belongs to.
        pageRegion *owner;

        // Memory pages start at a certain index which is the virtual memory pointer
        // divided by 8192 bytes of 2048 words or 32 blocks.
        uint32 page_idx;

        // Memory pages can be viewn differently based on requested layout.
        // But every page is a fixed 32 blocks in side.
        MemoryBlock blocks[ NUM_BLOCKS_PER_PAGE ];

        // Pages reside in memory in their order in this list.
        AVLNode sortedNode;
    };

    Interface *engineInterface;

    // Forward declaration.
    struct pageTBPWSet;

    // Set of allocatable pages.
    struct pageRegion
    {
        inline pageRegion( Interface *rwEngine, pageTBPWSet *owner ) noexcept
        {
            this->rwEngine = rwEngine;
            this->owner = owner;
        }
        inline pageRegion( const pageRegion& ) = delete;
        inline pageRegion( pageRegion&& right ) noexcept : sortedPages( std::move( right.sortedPages ) )
        {
            this->rwEngine = right.rwEngine;
            this->owner = right.owner;
        }

    private:
        AINLINE void clear( void )
        {
            RwDynMemAllocator memAlloc( this->rwEngine );

            while ( AVLNode *nodePage = sortedPages.GetRootNode() )
            {
                VirtualMemoryPage *page = AVL_GETITEM( VirtualMemoryPage, nodePage, sortedNode );

                sortedPages.RemoveByNode( &page->sortedNode );

                eir::dyn_del_struct <VirtualMemoryPage> ( memAlloc, nullptr, page );
            }

            sortedPages.Clear();
        }

    public:
        inline ~pageRegion( void )
        {
            clear();
        }

        inline pageRegion& operator = ( const pageRegion& ) = delete;
        inline pageRegion& operator = ( pageRegion&& right ) noexcept
        {
            clear();

            this->rwEngine = right.rwEngine;
            this->owner = right.owner;
            this->sortedPages = std::move( right.sortedPages );

            return *this;
        }

        inline uint32 GetPageSpan( void ) const
        {
            const AVLNode *lastPage = this->sortedPages.GetBiggestNode();

            if ( lastPage == nullptr )
                return 0;

            const VirtualMemoryPage *memPage = AVL_GETITEM( VirtualMemoryPage, lastPage, sortedNode );

            return ( memPage->page_idx + 1 );
        }

        inline uint32 GetSizeInBlocks( void ) const
        {
            return ( GetPageSpan() * NUM_BLOCKS_PER_PAGE );
        }

        inline VirtualMemoryPage* GetPage( uint32 pageIndex )
        {
            // Try to fetch an existing page.
            if ( auto *avlExistingPage = sortedPages.FindNode( pageIndex ) )
            {
                return AVL_GETITEM( VirtualMemoryPage, avlExistingPage, sortedNode );
            }

            // Allocate a new page if it does not exist.
            Interface *engineInterface = this->rwEngine;

            RwDynMemAllocator memAlloc( engineInterface );

            VirtualMemoryPage *newPage = eir::dyn_new_struct <VirtualMemoryPage> ( memAlloc, nullptr, engineInterface, this, pageIndex );

            // Add it into our system.
            sortedPages.Insert( &newPage->sortedNode );

            return newPage;
        }

        AINLINE MemoryBlock& GetMemoryBlockByOffset( const ps2MemoryLayoutTypeProperties& layoutProps, uint32 blockOffset )
        {
            uint32 pageIndex = ( blockOffset / NUM_BLOCKS_PER_PAGE );
            uint32 pageLocalLinearBlockIndex = ( blockOffset % NUM_BLOCKS_PER_PAGE );

            VirtualMemoryPage *page = GetPage( pageIndex );

            return page->GetMemoryBlockByLayout( layoutProps, pageLocalLinearBlockIndex );
        }

        // Returns true if a block is already occupied on our virtual memory.
        AINLINE bool IsMemoryBlockOccupied( const ps2MemoryLayoutTypeProperties& layoutProps, uint32 blockOffset )
        {
            MemoryBlock& theBlock = GetMemoryBlockByOffset( layoutProps, blockOffset );

            return theBlock.IsOwned();
        }

        AINLINE void SetMemoryBlockOwnedBy( const ps2MemoryLayoutTypeProperties& layoutProps, uint32 blockOffset, uint32 pageBlockX, uint32 pageBlockY, MemoryAllocation *alloc )
        {
            MemoryBlock& theBlock = GetMemoryBlockByOffset( layoutProps, blockOffset );

            alloc->AddMemoryBlockOwnership( theBlock, pageBlockX, pageBlockY );
        }

        AINLINE VirtualMemoryPage* GetLastPage( void )
        {
            AVLNode *avlLastPage = sortedPages.GetBiggestNode();

            if ( avlLastPage == nullptr )
                return nullptr;

            return AVL_GETITEM( VirtualMemoryPage, avlLastPage, sortedNode );
        }

        Interface *rwEngine;

        pageTBPWSet *owner;

        struct avlSortedVirtualPageDispatcher
        {
            AINLINE static eir::eCompResult CompareNodes( const AVLNode *left, const AVLNode *right )
            {
                const VirtualMemoryPage *leftPage = AVL_GETITEM( VirtualMemoryPage, left, sortedNode );
                const VirtualMemoryPage *rightPage = AVL_GETITEM( VirtualMemoryPage, right, sortedNode );

                return eir::DefaultValueCompare( leftPage->page_idx, rightPage->page_idx );
            }

            AINLINE static eir::eCompResult CompareNodeWithValue( const AVLNode *left, uint32 page_idx )
            {
                const VirtualMemoryPage *leftPage = AVL_GETITEM( VirtualMemoryPage, left, sortedNode );

                return eir::DefaultValueCompare( leftPage->page_idx, page_idx );
            }
        };

        AVLTree <avlSortedVirtualPageDispatcher> sortedPages;
    };

    // Pages are grouped together into TBPW-sets for easier management.
    // The TBPW-sets are allocated with the minimum amount of page-rows that the
    // first allocation requires.
    // If the TBPW is constant for a set of pages then we can perform mipmap
    // allocation on it.
    struct pageTBPWSet
    {
        inline pageTBPWSet( Interface *rwEngine, uint32 tbpw ) noexcept : pages( rwEngine, this )
        {
            this->tbpw = tbpw;
            this->globalPageOffset = 0;
            this->isFinalized = false;
        }

        inline void Finalize( uint32 globalPageOffset ) noexcept
        {
#ifdef _DEBUG
            assert( this->isFinalized == false );
#endif //_DEBUG

            this->globalPageOffset = globalPageOffset;
            this->isFinalized = true;
        }

        inline uint32 GetTextureBasePointer( void ) const
        {
            if ( this->isFinalized == false )
            {
                throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
            }

            return ( globalPageOffset * NUM_BLOCKS_PER_PAGE );
        }

        inline uint32 GetFinalizedPageIndex( void ) const
        {
            if ( this->isFinalized == false )
            {
                throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
            }

            return globalPageOffset;
        }

        uint32 tbpw;
        uint32 globalPageOffset;
        bool isFinalized;

        pageRegion pages;

        AVLNode sortedNode;
    };

    // TODO: make it use pageTBPWSet for allocation-spot-finding instead of assuming an
    // infinite global memory.
    struct MemoryAllocationSpot
    {
        inline MemoryAllocationSpot( void ) noexcept
        {
            this->texBuffer = nullptr;
            this->global_block_x = 0;
            this->global_block_y = 0;
            this->layoutProps = getMemoryLayoutTypeProperties( PSMCT32 );
            this->widthInBlocks = 0;
            this->heightInBlocks = 0;
        }
        inline MemoryAllocationSpot(
            pageTBPWSet *texBuffer,
            uint32 global_block_x, uint32 global_block_y,
            eMemoryLayoutType memLayout,
            uint32 widthInBlocks, uint32 heightInBlocks
        )
        {
            this->texBuffer = texBuffer;
            this->global_block_x = global_block_x;
            this->global_block_y = global_block_y;
            this->layoutProps = getMemoryLayoutTypeProperties( memLayout );
            this->widthInBlocks = widthInBlocks;
            this->heightInBlocks = heightInBlocks;
        }

    private:
        AINLINE void moveFrom( MemoryAllocationSpot&& right ) noexcept
        {
            this->texBuffer = right.texBuffer;
            this->global_block_x = right.global_block_x;
            this->global_block_y = right.global_block_y;
            this->layoutProps = right.layoutProps;
            this->widthInBlocks = right.widthInBlocks;
            this->heightInBlocks = right.heightInBlocks;
        }

    public:
        inline MemoryAllocationSpot( const MemoryAllocationSpot& ) = delete;
        inline MemoryAllocationSpot( MemoryAllocationSpot&& right ) noexcept
        {
            moveFrom( std::move( right ) );
        }

        inline MemoryAllocationSpot& operator = ( const MemoryAllocationSpot& ) = delete;
        inline MemoryAllocationSpot& operator = ( MemoryAllocationSpot&& right ) noexcept
        {
            moveFrom( std::move( right ) );

            return *this;
        }

        AINLINE bool IsInsideEstablishedSpace( void ) const
        {
            // Get index of the last block.
            uint32 block_end_x = ( this->global_block_x + this->widthInBlocks - 1 );
            uint32 block_end_y = ( this->global_block_y + this->heightInBlocks - 1 );

            uint32 page_x = ( block_end_x / this->layoutProps.widthBlocksPerPage );
            uint32 page_y = ( block_end_y / this->layoutProps.heightBlocksPerPage );

            uint32 page_idx = ( page_y * this->texBuffer->tbpw + page_x );

            uint32 requiredPageCount = ( page_idx + 1 );

            return ( requiredPageCount <= texBuffer->pages.GetPageSpan() );
        }

    private:
        AINLINE static void CalculatePageBlockBoundariesOfPoint(
            uint32 global_block_x, uint32 global_block_y, uint32 widthBlocksPerPage, uint32 heightBlocksPerPage,
            uint32& pagestart_x_out, uint32& pagestart_y_out, uint32& pageend_x_out, uint32& pageend_y_out
        )
        {
            uint32 cur_page_x = ( global_block_x / widthBlocksPerPage );
            uint32 cur_page_y = ( global_block_y / heightBlocksPerPage );

            uint32 pagestart_block_x = ( cur_page_x * widthBlocksPerPage );
            uint32 pagestart_block_y = ( cur_page_y * heightBlocksPerPage );

            uint32 pageend_block_x = ( pagestart_block_x + widthBlocksPerPage - 1 );
            uint32 pageend_block_y = ( pagestart_block_y + heightBlocksPerPage - 1 );

            pagestart_x_out = pagestart_block_x;
            pagestart_y_out = pagestart_block_y;

            pageend_x_out = pageend_block_x;
            pageend_y_out = pageend_block_y;
        }

    public:
        // Helper function to move the allocation onto the next best allocation spot.
        // Should not be used on allocations that are already placed.
        AINLINE void ChooseNextPossibleAllocationSpot( void )
        {
            // We have got TBPW * widthBlocksPerPage * MANY amount of blocks in the current layout.
            // Out of those we want to pick the top left
            // max(0, TBPW * widthBlocksPerPage - widthInBlocks) * max(0, MANY - heightInBlocks)
            // set of block positions for allocation.

            // We want to iterate on a page-by-page basis: try to fill page one to the brim, then move
            // to the next one. This way we put most data into the first pages, which leads
            // to improved performance.

            pageTBPWSet *texBuffer = this->texBuffer;

            uint32 tbpw = texBuffer->tbpw;

            uint32 widthBlocksPerPage = this->layoutProps.widthBlocksPerPage;
            uint32 heightBlocksPerPage = this->layoutProps.heightBlocksPerPage;

            uint32 global_block_x = this->global_block_x;
            uint32 global_block_y = this->global_block_y;

            uint32 widthInBlocks = this->widthInBlocks;
            uint32 heightInBlocks = this->heightInBlocks;

            // Get the page coordinate of the block right border top.
            uint32 global_block_rightborder_x = ( global_block_x + widthInBlocks - 1 );

            uint32 rightborder_pagestart_block_x, rightborder_pagestart_block_y, rightborder_pageend_block_x, rightborder_pageend_block_y;
            CalculatePageBlockBoundariesOfPoint(
                global_block_rightborder_x, global_block_y, widthBlocksPerPage, heightBlocksPerPage,
                rightborder_pagestart_block_x, rightborder_pagestart_block_y, rightborder_pageend_block_x, rightborder_pageend_block_y
            );

            // Get the page coordinates of the block bottom border.
            uint32 global_block_bottomborder_y = ( global_block_y + heightInBlocks - 1 );

            uint32 bottomborder_pagestart_block_x, bottomborder_pagestart_block_y, bottomborder_pageend_block_x, bottomborder_pageend_block_y;
            CalculatePageBlockBoundariesOfPoint(
                global_block_x, global_block_bottomborder_y, widthBlocksPerPage, heightBlocksPerPage,
                bottomborder_pagestart_block_x, bottomborder_pagestart_block_y, bottomborder_pageend_block_x, bottomborder_pageend_block_y
            );

            // Get the total allocation region block span.
            uint32 allocBlockSpan = ( this->layoutProps.widthBlocksPerPage * tbpw );

            if ( allocBlockSpan == 0 || allocBlockSpan < widthInBlocks )
            {
                throw NativeTextureInternalErrorException( "PlayStation2", nullptr );
            }

            if ( global_block_rightborder_x == rightborder_pageend_block_x )
            {
                if ( global_block_bottomborder_y != bottomborder_pageend_block_y )
                {
                    uint32 moveBackCount = ( widthInBlocks - 1 );

                    if ( rightborder_pagestart_block_x <= moveBackCount )
                    {
                        this->global_block_x = 0;
                    }
                    else
                    {
                        this->global_block_x = rightborder_pagestart_block_x - moveBackCount;
                    }

                    this->global_block_y = ( global_block_y + 1 );
                }
                else
                {
                    // Choose the next page starting coordinates.
                    if ( global_block_rightborder_x == allocBlockSpan )
                    {
                        this->global_block_x = 0;
                        this->global_block_y = ( global_block_y + 1 );
                    }
                    else
                    {
                        // Since we are not bordering to the right end of the TBPW texture buffer we can go to the right.
                        this->global_block_x = global_block_x + 1;

                        uint32 moveBackCount = ( heightInBlocks - 1 );

                        if ( bottomborder_pagestart_block_y <= moveBackCount )
                        {
                            this->global_block_y = 0;
                        }
                        else
                        {
                            this->global_block_y = bottomborder_pagestart_block_y - moveBackCount;
                        }
                    }
                }
            }
            else
            {
                this->global_block_x = ( global_block_x + 1 );
            }
        }

        template <typename callbackType>
        AINLINE void ForAllResidingBlocks( callbackType&& cb )
        {
            pageTBPWSet *texBuffer = this->texBuffer;

            uint32 tbpw = texBuffer->tbpw;

            uint32 global_block_start_x = this->global_block_x;
            uint32 global_block_start_y = this->global_block_y;

            uint32 widthInBlocks = this->widthInBlocks;
            uint32 heightInBlocks = this->heightInBlocks;

            uint32 widthBlocksPerPage = this->layoutProps.widthBlocksPerPage;
            uint32 heightBlocksPerPage = this->layoutProps.heightBlocksPerPage;

            for ( uint32 y = 0; y < heightInBlocks; y++ )
            {
                for ( uint32 x = 0; x < widthInBlocks; x++ )
                {
                    uint32 total_block_x = ( global_block_start_x + x );
                    uint32 total_block_y = ( global_block_start_y + y );

                    uint32 pageoff_x = ( total_block_x / widthBlocksPerPage );
                    uint32 in_page_block_x = ( total_block_x % widthBlocksPerPage );

                    uint32 pageoff_y = ( total_block_y / heightBlocksPerPage );
                    uint32 in_page_block_y = ( total_block_y % heightBlocksPerPage );

                    // Calculate the page index.
                    uint32 page_idx = ( pageoff_y * tbpw + pageoff_x );

                    VirtualMemoryPage *memPage = texBuffer->pages.GetPage( page_idx );

                    // Calculate the in-page block index.
                    uint32 in_page_blockidx = ( in_page_block_y * widthBlocksPerPage + in_page_block_x );

                    MemoryBlock& theBlock = memPage->GetMemoryBlockByLayout( this->layoutProps, in_page_blockidx );

                    if constexpr ( std::is_invocable <callbackType, MemoryBlock&, uint32, uint32>::value )
                    {
                        cb( theBlock, in_page_block_x, in_page_block_y );
                    }
                    else
                    {
                        cb( theBlock );
                    }
                }
            }
        }

        template <typename callbackType>
        AINLINE void ForAllResidingBlocksConst( callbackType&& cb ) const
        {
            const_cast <MemoryAllocationSpot*> ( this )->ForAllResidingBlocks(
                [&]( MemoryBlock& block )
                {
                    cb( (const MemoryBlock&)block );
                }
            );
        }

        AINLINE bool IsObstructed( void ) const
        {
            bool isObstructed = false;

            ForAllResidingBlocksConst(
                [&]( const MemoryBlock& curBlock )
                {
                    if ( curBlock.residingAllocation != nullptr )
                    {
                        isObstructed = true;
                    }
                }
            );

            return isObstructed;
        }

        AINLINE void SetOwnershipOfBlocks( MemoryAllocation *alloc )
        {
            ForAllResidingBlocks(
                [&]( MemoryBlock& curBlock, uint32 pageBlockX, uint32 pageBlockY )
                {
                    alloc->AddMemoryBlockOwnership( curBlock, pageBlockX, pageBlockY );
                }
            );
        }

    private:
        pageTBPWSet *texBuffer;
        uint32 global_block_x;
        uint32 global_block_y;
        ps2MemoryLayoutTypeProperties layoutProps;
        uint32 widthInBlocks;
        uint32 heightInBlocks;
    };

    inline ps2GSMemoryLayoutManager( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
    }

    inline ~ps2GSMemoryLayoutManager( void )
    {
        RwDynMemAllocator memAlloc( this->engineInterface );

        // Delete all allocated texture buffers.
        while ( AVLNode *avlTexBuffer = this->sortedByTBPW_texBuffers.GetRootNode() )
        {
            pageTBPWSet *texBuffer = AVL_GETITEM( pageTBPWSet, avlTexBuffer, sortedNode );

            this->sortedByTBPW_texBuffers.RemoveByNodeFast( avlTexBuffer );

            eir::dyn_del_struct <pageTBPWSet> ( memAlloc, nullptr, texBuffer );
        }
    }

    struct avlTBPWSetDispatcher
    {
        AINLINE static eir::eCompResult CompareNodes( const AVLNode *left, const AVLNode *right ) noexcept
        {
            const pageTBPWSet *leftTexBuffer = AVL_GETITEM( pageTBPWSet, left, sortedNode );
            const pageTBPWSet *rightTexBuffer = AVL_GETITEM( pageTBPWSet, right, sortedNode );

            return eir::DefaultValueCompare( leftTexBuffer->tbpw, rightTexBuffer->tbpw );
        }

        AINLINE static eir::eCompResult CompareNodeWithValue( const AVLNode *left, uint32 tbpw )
        {
            const pageTBPWSet *leftTexBuffer = AVL_GETITEM( pageTBPWSet, left, sortedNode );

            return eir::DefaultValueCompare( leftTexBuffer->tbpw, tbpw );
        }
    };

    // We need to store a set of texture buffers.
    AVLTree <avlTBPWSetDispatcher> sortedByTBPW_texBuffers;

    // Memory management constants of the PS2 Graphics Synthesizer.
    static constexpr uint32 gsColumnSize = 16 * sizeof(uint32);
    static constexpr uint32 gsBlockSize = gsColumnSize * 4;
    static constexpr uint32 gsPageSize = gsBlockSize * 32;

    inline static uint32 GetPageIndexFromBlockOffset( uint32 globalBlockOffset )
    {
        return ( globalBlockOffset / NUM_BLOCKS_PER_PAGE );
    }

    // Returns the block index from an allocation spot.
    inline static uint32 getTextureBasePointer( const ps2MemoryLayoutTypeProperties& layoutProps, uint32 pageX, uint32 pageY, uint32 tbpw, uint32 blockOffsetX, uint32 blockOffsetY )
    {
#ifdef _DEBUG
        assert( blockOffsetX < layoutProps.widthBlocksPerPage && blockOffsetY < layoutProps.heightBlocksPerPage );
#endif //_DEBUG

        // Get block index from the dimensional coordinates.
        // This requires a dispatch according to the memory layout.
        uint32 blockIndex = 0;
        {
            const uint32 *blockArrangement = layoutProps.blockArrangementInPage;

            blockIndex = *( blockArrangement + (size_t)blockOffsetY * layoutProps.widthBlocksPerPage + blockOffsetX );
        }

        // Allocate the texture at the current position in the buffer.
        uint32 pageIndex = ( pageY * tbpw + pageX );

        return ( pageIndex * 32u + blockIndex );
    }

    static constexpr bool _allocateAwayFromBaseline = false;

    inline bool allocateOnExistingRegion_extend(
        eMemoryLayoutType memLayoutType,
        uint32 widthNumBlocks, uint32 heightNumBlocks,
        const ps2MemoryLayoutTypeProperties& layoutProps,
        MemoryAllocation& allocOut
    )
    {
        MemoryAllocationSpot spotToAllocateOn;

        uint32 min_tbpw = CEIL_DIV( widthNumBlocks, layoutProps.widthBlocksPerPage );

        // No existing space could satisfy our request.
        // Thus we try extending existing space if it fits our requirements exactly.
        AVLNode *avlFittingTBPWBuf = sortedByTBPW_texBuffers.FindNode( min_tbpw );

        if ( avlFittingTBPWBuf )
        {
            pageTBPWSet *texBuf = AVL_GETITEM( pageTBPWSet, avlFittingTBPWBuf, sortedNode );

            // Get the last occupied block row of this buffer.
            uint32 last_occupied_block_row_idx = 0;

            uint32 tbpw = texBuf->tbpw;

            for ( decltype(texBuf->pages.sortedPages)::diff_node_iterator pageiter( texBuf->pages.sortedPages ); !pageiter.IsEnd(); pageiter.Increment() )
            {
                VirtualMemoryPage *memPage = AVL_GETITEM( VirtualMemoryPage, pageiter.Resolve(), sortedNode );

                uint32 local_used_block_row;

                if ( memPage->GetLastUsedBlockRowIndex( layoutProps, local_used_block_row ) )
                {
                    uint32 page_row = ( memPage->page_idx / tbpw );

                    // Turn the block coordinate into a page coordinate.
                    uint32 page_used_block_row = ( local_used_block_row + page_row * layoutProps.heightBlocksPerPage );

                    if ( last_occupied_block_row_idx < page_used_block_row )
                    {
                        last_occupied_block_row_idx = page_used_block_row;
                    }
                }
            }

            // We put stuff at the next row index at x = 0.
            last_occupied_block_row_idx++;

            MemoryAllocationSpot newSpot( texBuf, 0, last_occupied_block_row_idx, memLayoutType, widthNumBlocks, heightNumBlocks );

#ifdef _DEBUG
            if ( newSpot.IsObstructed() )
            {
                assert( 0 );
            }
#endif //_DEBUG

            spotToAllocateOn = std::move( newSpot );
            goto spotAllocation;
        }

        return false;

    spotAllocation:
        MemoryAllocation newAlloc;
        spotToAllocateOn.SetOwnershipOfBlocks( &newAlloc );

        allocOut = std::move( newAlloc );
        return true;
    }

    inline bool allocateOnExistingRegion_findfirstorextend(
        eMemoryLayoutType memLayoutType,
        uint32 widthNumBlocks, uint32 heightNumBlocks,
        const ps2MemoryLayoutTypeProperties& layoutProps,
        MemoryAllocation& allocOut
    )
    {
        MemoryAllocationSpot spotToAllocateOn;

        // Check all texture buffers sorted by TBPW for available space.
        // If the texture buffer is found and it matches the TBPW exactly then it is extended to fit
        // the request.

        uint32 min_tbpw = CEIL_DIV( widthNumBlocks, layoutProps.widthBlocksPerPage );

        for ( decltype( sortedByTBPW_texBuffers )::diff_node_iterator texBufIter( sortedByTBPW_texBuffers.GetJustAboveOrEqualNode( min_tbpw ) ); !texBufIter.IsEnd(); texBufIter.Increment() )
        {
            for ( decltype( sortedByTBPW_texBuffers )::nodestack_iterator nsiter( texBufIter.Resolve() ); !nsiter.IsEnd(); nsiter.Increment() )
            {
                pageTBPWSet *texBuffer = AVL_GETITEM( pageTBPWSet, nsiter.Resolve(), sortedNode );

                // Try to find an existing spot.
                MemoryAllocationSpot searchExisting( texBuffer, 0, 0, memLayoutType, widthNumBlocks, heightNumBlocks );

                while ( searchExisting.IsInsideEstablishedSpace() )
                {
                    if ( searchExisting.IsObstructed() == false )
                    {
                        // Allocate on this spot.
                        spotToAllocateOn = std::move( searchExisting );
                        goto spotAllocation;
                    }

                    searchExisting.ChooseNextPossibleAllocationSpot();
                }
            }
        }

        // Try to extend instead.
        return allocateOnExistingRegion_extend( memLayoutType, widthNumBlocks, heightNumBlocks, layoutProps, allocOut );

    spotAllocation:
        MemoryAllocation newAlloc;
        spotToAllocateOn.SetOwnershipOfBlocks( &newAlloc );

        allocOut = std::move( newAlloc );
        return true;
    }

    inline bool allocateOnRegion_new(
        eMemoryLayoutType memLayoutType,
        uint32 widthNumBlocks, uint32 heightNumBlocks,
        const ps2MemoryLayoutTypeProperties& layoutProps,
        MemoryAllocation& allocOut
    )
    {
        // This code can be shared with the CLUT.

        uint32 min_tbpw = CEIL_DIV( widthNumBlocks, layoutProps.widthBlocksPerPage );

        Interface *rwEngine = this->engineInterface;

        // Since there was no existing texture buffer that could fulfill our request we try allocation on an entirely new one.
        RwDynMemAllocator memAlloc( rwEngine );

        pageTBPWSet *newSet = eir::dyn_new_struct <pageTBPWSet, rwEirExceptionManager> ( memAlloc, nullptr, rwEngine, min_tbpw );

        // Add the set to the resident texture buffers.
        this->sortedByTBPW_texBuffers.Insert( &newSet->sortedNode );

        MemoryAllocationSpot spotToAllocateOn = MemoryAllocationSpot( newSet, 0, 0, memLayoutType, widthNumBlocks, heightNumBlocks );

        MemoryAllocation newAlloc;
        spotToAllocateOn.SetOwnershipOfBlocks( &newAlloc );

        allocOut = std::move( newAlloc );
        return true;
    }

    inline static uint32 calculateTextureMemSize(
        const ps2MemoryLayoutTypeProperties& layoutProps,
        uint32 texBasePointer,
        uint32 pageX, uint32 pageY, uint32 bufferPageWidth,
        uint32 blockOffsetX, uint32 blockOffsetY, uint32 blockWidth, uint32 blockHeight
    )
    {
        uint32 texelBlockWidthOffset = ( blockWidth - 1 ) + blockOffsetX;
        uint32 texelBlockHeightOffset = ( blockHeight - 1 ) + blockOffsetY;

        uint32 finalPageX = pageX + texelBlockWidthOffset / layoutProps.widthBlocksPerPage;
        uint32 finalPageY = pageY + texelBlockHeightOffset / layoutProps.heightBlocksPerPage;

        uint32 finalBlockOffsetX = texelBlockWidthOffset % layoutProps.widthBlocksPerPage;
        uint32 finalBlockOffsetY = texelBlockHeightOffset % layoutProps.heightBlocksPerPage;

        uint32 texEndOffset =
            getTextureBasePointer(layoutProps, finalPageX, finalPageY, bufferPageWidth, finalBlockOffsetX, finalBlockOffsetY);

        return ( texEndOffset - texBasePointer ) + 1; //+1 because its a size
    }

    static AINLINE uint32 get_block_pixel_width( const ps2MemoryLayoutTypeProperties& props )
    {
        return props.pixelWidthPerColumn;
    }

    static AINLINE uint32 get_block_pixel_height( const ps2MemoryLayoutTypeProperties& props )
    {
        return props.pixelHeightPerColumn * 4;
    }

    inline static uint32 calculateTextureBufferPageWidth(
        const ps2MemoryLayoutTypeProperties& layoutProps,
        uint32 texelWidth, uint32 texelHeight
    )
    {
        // Get block dimensions.
        uint32 texelBlockWidth = CEIL_DIV( texelWidth, layoutProps.pixelWidthPerColumn );

        // Return the width in amount of pages.
        uint32 texBufferPageWidth = CEIL_DIV( texelBlockWidth, layoutProps.widthBlocksPerPage );

        return texBufferPageWidth;
    }

    inline bool allocateTexture(
        eMemoryLayoutType memLayoutType, const ps2MemoryLayoutTypeProperties& layoutProps,
        uint32 texelWidth, uint32 texelHeight,
        MemoryAllocation& allocOut
    )
    {
        uint32 blockPixelWidth = get_block_pixel_width( layoutProps );
        uint32 blockPixelHeight = get_block_pixel_height( layoutProps );

        // Get block dimensions.
        uint32 texelBlockWidth = CEIL_DIV( texelWidth, blockPixelWidth );
        uint32 texelBlockHeight = CEIL_DIV( texelHeight, blockPixelHeight );

        // Loop through all pages and try to find the correct placement for the new texture.
        MemoryAllocation possibleAlloc;
        {
            bool validAllocation =
                allocateOnExistingRegion_findfirstorextend(
                    memLayoutType, texelBlockWidth, texelBlockHeight,
                    layoutProps,
                    possibleAlloc
                );

            // This may trigger if we overshot memory capacity.
            if ( validAllocation == false )
            {
                validAllocation = allocateOnRegion_new(
                    memLayoutType, texelBlockWidth, texelBlockHeight,
                    layoutProps,
                    possibleAlloc
                );

                if ( validAllocation == false )
                {
                    return false;
                }
            }
        }

        allocOut = std::move( possibleAlloc );
        return true;
    }

    // Special CLUT allocation spot finder because the CLUT cannot be placed across pages.
    inline bool allocateOnExistingRegion_CLUT(
        eMemoryLayoutType memLayoutType,
        uint32 widthNumBlocks, uint32 heightNumBlocks,
        const ps2MemoryLayoutTypeProperties& layoutProps,
        MemoryAllocation& allocOut,
        size_t mipCount
    )
    {
        // The CLUT is always one page wide.
        // If not then we do have a real problem.
        //uint32 min_tbpw = 1;

        // The CLUT is first tried to be allocated on 0,0 of any existing page.
        // If that fails then it is allocated on the bottom right corner.
        // For the allocation on the bottom right corner there are multiple approaches:
        // * if the texture has no mipmaps then try the last block of the page minus one in
        //   both dimensions (consider that this behaviour is same for PSMT4/PSMT8 even with
        //   differing CLUT block dimensions)
        // * if the texture has mipmaps then try allocating on the bottom right of the page
        //   minus the CLUT block dimensions.
        // To extend this algorithm we specify the CLUT allocation spot as wish. If the CLUT would
        // overshoot the page then it is moved backward ín the page, of course.

        uint32 widthBlocksPerPage = layoutProps.widthBlocksPerPage;
        uint32 heightBlocksPerPage = layoutProps.heightBlocksPerPage;

        uint32 corneralloc_blocklocal_x, corneralloc_blocklocal_y;

        if ( mipCount == 1 )
        {
            corneralloc_blocklocal_x = std::min(widthBlocksPerPage - 2, widthBlocksPerPage - widthNumBlocks);
            corneralloc_blocklocal_y = std::min(heightBlocksPerPage - 2, heightBlocksPerPage - heightNumBlocks);
        }
        else
        {
            corneralloc_blocklocal_x = ( widthBlocksPerPage - widthNumBlocks );
            corneralloc_blocklocal_y = ( heightBlocksPerPage - heightNumBlocks );
        }

        // Go through all pages of all TBPW sets in descending order of TBPW value.
        for ( decltype( sortedByTBPW_texBuffers )::diff_node_iterator bufiter( sortedByTBPW_texBuffers.GetBiggestNode() ); !bufiter.IsEnd(); bufiter.Decrement() )
        {
            for ( decltype( sortedByTBPW_texBuffers )::nodestack_iterator nsiter( bufiter.Resolve() ); !nsiter.IsEnd(); nsiter.Increment() )
            {
                pageTBPWSet *texBuf = AVL_GETITEM( pageTBPWSet, nsiter.Resolve(), sortedNode );

                // We just allocate into the bottom-right corner of the TBPW buffer.
                // This is automatically the last page.
                VirtualMemoryPage *memPage = texBuf->pages.GetLastPage();

                if ( memPage )
                {
                    uint32 texBuf_tbpw = texBuf->tbpw;

                    uint32 page_x = ( memPage->page_idx % texBuf_tbpw );
                    uint32 page_y = ( memPage->page_idx / texBuf_tbpw );

                    // Try the to allocate in the corner of each page.
                    uint32 corneralloc_pagelocal_x = ( page_x * widthBlocksPerPage + corneralloc_blocklocal_x );
                    uint32 corneralloc_pagelocal_y = ( page_y * heightBlocksPerPage + corneralloc_blocklocal_y );

                    MemoryAllocationSpot cornerAlloc( texBuf, corneralloc_pagelocal_x, corneralloc_pagelocal_y, memLayoutType, widthNumBlocks, heightNumBlocks );

                    if ( cornerAlloc.IsObstructed() == false )
                    {
                        MemoryAllocation newAlloc;
                        cornerAlloc.SetOwnershipOfBlocks( &newAlloc );

                        allocOut = std::move( newAlloc );
                        return true;
                    }
                }
            }
        }

        // Try to extend an existing TBPW set and put ourselves there.
        return allocateOnExistingRegion_extend( memLayoutType, widthNumBlocks, heightNumBlocks, layoutProps, allocOut );
    }

    inline bool allocateCLUT(
        eMemoryLayoutType memLayoutType, const ps2MemoryLayoutTypeProperties& layoutProps,
        uint32 clutWidth, uint32 clutHeight,
        MemoryAllocation& allocOut,
        size_t mipmapCount
    )
    {
        uint32 blockPixelWidth = get_block_pixel_width( layoutProps );
        uint32 blockPixelHeight = get_block_pixel_height( layoutProps );

        // Get block dimensions.
        uint32 texelBlockWidth = CEIL_DIV( clutWidth, blockPixelWidth );
        uint32 texelBlockHeight = CEIL_DIV( clutHeight, blockPixelHeight );

        // Try to allocate the CLUT.
        MemoryAllocation clutAlloc;
        {
            bool validAllocation = allocateOnExistingRegion_CLUT(
                memLayoutType, texelBlockWidth, texelBlockHeight, layoutProps,
                clutAlloc, mipmapCount
            );

            if ( validAllocation == false )
            {
                validAllocation = allocateOnRegion_new(
                    memLayoutType, texelBlockWidth, texelBlockHeight, layoutProps,
                    clutAlloc
                );

                if ( validAllocation == false )
                {
                    return false;
                }
            }
        }

        // Return the allocation.
        // We can decide about real TBP after finalization of pageTBPWSets.
        allocOut = std::move( clutAlloc );
        return true;
    }

    inline void FinalizeMemory( void )
    {
        // Turn all existing memory buffers final and give them real memory offsets.
        // To do that we assign a sequentially-running page index to every TBPW set.

        uint32 cur_real_page_idx = 0;

        for ( decltype( sortedByTBPW_texBuffers )::diff_node_iterator bufiter( sortedByTBPW_texBuffers.GetBiggestNode() ); !bufiter.IsEnd(); bufiter.Decrement() )
        {
            for ( decltype( sortedByTBPW_texBuffers )::nodestack_iterator nsiter( bufiter.Resolve() ); !nsiter.IsEnd(); nsiter.Increment() )
            {
                pageTBPWSet *texBuf = AVL_GETITEM( pageTBPWSet, nsiter.Resolve(), sortedNode );

                texBuf->Finalize( cur_real_page_idx );

                cur_real_page_idx += texBuf->pages.GetPageSpan();
            }
        }
    }

    inline uint32 GetTextureMemoryPageSize( void )
    {
        uint32 requiredPageSize = 0;

        for ( decltype( sortedByTBPW_texBuffers )::diff_node_iterator bufiter( sortedByTBPW_texBuffers ); !bufiter.IsEnd(); bufiter.Increment() )
        {
            for ( decltype( sortedByTBPW_texBuffers )::nodestack_iterator nsiter( bufiter.Resolve() ); !nsiter.IsEnd(); nsiter.Increment() )
            {
                pageTBPWSet *texBuf = AVL_GETITEM( pageTBPWSet, nsiter.Resolve(), sortedNode );

                if ( texBuf->isFinalized )
                {
                    uint32 bufRequiredPageSize = ( texBuf->globalPageOffset + texBuf->pages.GetPageSpan() );

                    if ( bufRequiredPageSize > requiredPageSize )
                    {
                        requiredPageSize = bufRequiredPageSize;
                    }
                }
            }
        }

        // Return the page offset in PS2 words.
        return requiredPageSize * 2048u;
    }
};

AINLINE uint32 GetMemoryLayoutPagePixelWidth( const ps2MemoryLayoutTypeProperties& layoutProps )
{
    return ( layoutProps.widthBlocksPerPage * layoutProps.pixelWidthPerColumn );
}

AINLINE bool doesCLUTRequireNewTransmission(
    ps2GSMemoryLayoutManager::MemoryAllocation& allocInfo,
    eMemoryLayoutType mipmapBITBLTLayout, eMemoryLayoutType clutBITBLTLayout
)
{
    ps2GSMemoryLayoutManager::MemoryBlock *firstMemBlock = LIST_GETITEM( ps2GSMemoryLayoutManager::MemoryBlock, allocInfo.allocated_on_blocks.root.next, allocationBlocksNode );

    ps2GSMemoryLayoutManager::VirtualMemoryPage *pageOfBlock = firstMemBlock->pageOfBlock;

    ps2GSMemoryLayoutManager::pageRegion *pageRegionOfBlock = pageOfBlock->owner;

    ps2GSMemoryLayoutManager::pageTBPWSet *texBuf = pageRegionOfBlock->owner;

    uint32 tbpw = texBuf->tbpw;

    // HACK: we do not support CLUT swizzling yet. Thus we force a new transmission if the BITBLT
    // for CLUT does not match the mipmap transmission layout.
    // This is the same thing for PSMT4 coming as PSMCT32 but the mipmap being swizzled as
    // PSMCT16.
    if ( mipmapBITBLTLayout != clutBITBLTLayout )
    {
        return true;
    }

    // CSM1 CLUT has to be placed using a page-wide TBW.
    return ( tbpw != 1 );
}

AINLINE void prepareGSParametersForTexture(
    ps2GSMemoryLayoutManager::MemoryAllocation& allocInfo, uint32 decodedPagePixelWidth, uint32 blockPixelWidth, uint32 blockPixelHeight,
    const ps2MemoryLayoutTypeProperties& allocMemProps,
    uint32& tbpOut, uint32& tbwOut, ps2MipmapTransmissionData& transOut,
    bool requiresNewTransmission
)
{
    ps2GSMemoryLayoutManager::MemoryBlock *firstMemBlock = LIST_GETITEM( ps2GSMemoryLayoutManager::MemoryBlock, allocInfo.allocated_on_blocks.root.next, allocationBlocksNode );

    ps2GSMemoryLayoutManager::VirtualMemoryPage *pageOfBlock = firstMemBlock->pageOfBlock;

    ps2GSMemoryLayoutManager::pageRegion *pageRegionOfBlock = pageOfBlock->owner;

    ps2GSMemoryLayoutManager::pageTBPWSet *texBuf = pageRegionOfBlock->owner;

    uint32 tbpwbuf_pageidx = pageOfBlock->page_idx;

    uint32 pageOffsetIntoMemory_blocks = ( ( texBuf->GetFinalizedPageIndex() + tbpwbuf_pageidx ) * ps2GSMemoryLayoutManager::NUM_BLOCKS_PER_PAGE );

    uint32 inPageBlockIdx = firstMemBlock->inPageBlockIdx;

    tbpOut = ( pageOffsetIntoMemory_blocks + inPageBlockIdx );

    uint32 tbpw = texBuf->tbpw;

    // If the page is 128 pixels wide, we have to double the TBPW to get the TBW.
    tbwOut = decodedPagePixelWidth * tbpw / 64u;

    if ( requiresNewTransmission )
    {
        transOut.destX = 0;
        transOut.destY = 0;
    }
    else
    {
        // Calculate TBPW texture buffer block coordinates.
        uint32 page_x = ( tbpwbuf_pageidx % tbpw );
        uint32 page_y = ( tbpwbuf_pageidx / tbpw );

        transOut.destX = ( ( allocMemProps.widthBlocksPerPage * page_x + firstMemBlock->memLayout_allocPageX ) * blockPixelWidth );
        transOut.destY = ( ( allocMemProps.heightBlocksPerPage * page_y + firstMemBlock->memLayout_allocPageY ) * blockPixelHeight );
    }
}

bool NativeTexturePS2::allocateTextureMemoryNative(
    uint32 mipmapBasePointer[], uint32 mipmapBufferWidth[], ps2MipmapTransmissionData mipmapTransData[], uint32 maxMipmaps,
    uint32& clutBasePointerOut, ps2MipmapTransmissionData& clutTransDataOut,
    uint32& textureBufPageSizeOut
) const
{
    static constexpr uint32 MAX_PS2_MIPMAPS = 7u;

    eMemoryLayoutType clutPixelStorageMode = GetCommonMemoryLayoutTypeFromCLUTLayoutType( this->clutStorageFmt );

    eMemoryLayoutType decodedPixelStorageMode = this->pixelStorageMode;
    eMemoryLayoutType bitblt_targetFmt = this->bitblt_targetFmt;

    ps2MemoryLayoutTypeProperties bitblt_layoutProps = getMemoryLayoutTypeProperties( bitblt_targetFmt );
    ps2MemoryLayoutTypeProperties clutLayoutProps = getMemoryLayoutTypeProperties( clutPixelStorageMode );
    ps2MemoryLayoutTypeProperties decodedPixelLayoutProps = getMemoryLayoutTypeProperties( decodedPixelStorageMode );

    size_t mipmapCount = this->mipmaps.GetCount();

    // Perform the allocation.
    {
        Interface *engineInterface = this->engineInterface;

        ps2GSMemoryLayoutManager memManager( engineInterface );

        ps2GSMemoryLayoutManager::MemoryAllocation mipAllocs[ MAX_PS2_MIPMAPS ];

        for ( uint32 n = 0; n < mipmapCount; n++ )
        {
            const NativeTexturePS2::GSTexture& gsTex = this->mipmaps[n];

            // Get the texel dimensions of this texture.
            uint32 encodedWidth = gsTex.transmissionAreaWidth;
            uint32 encodedHeight = gsTex.transmissionAreaHeight;

            if ( encodedWidth == 0 || encodedHeight == 0 )
            {
                return false;
            }

            ps2GSMemoryLayoutManager::MemoryAllocation allocInfo;

            bool hasAllocated = memManager.allocateTexture( bitblt_targetFmt, bitblt_layoutProps, encodedWidth, encodedHeight, allocInfo );

            if ( !hasAllocated )
            {
                return false;
            }

            if ( n >= maxMipmaps )
            {
                // We do not know how to handle more mipmaps than the hardware allows.
                // For safety reasons terminate.
                return false;
            }

            // Store the allocation for future reference.
            mipAllocs[ n ] = std::move( allocInfo );
        }

        // Allocate the palette data if required.
        bool hasCLUT = ( this->paletteType != ePaletteTypePS2::NONE );

        ps2GSMemoryLayoutManager::MemoryAllocation clutAllocInfo;

        if ( hasCLUT )
        {
            const NativeTexturePS2::GSTexture& palTex = this->paletteTex;

            uint32 clut_width = palTex.transmissionAreaWidth;
            uint32 clut_height = palTex.transmissionAreaHeight;

            if ( clut_width == 0 || clut_height == 0 )
            {
                return false;
            }

            // Allocate it.
            bool hasAllocatedCLUT = memManager.allocateCLUT( clutPixelStorageMode, clutLayoutProps, clut_width, clut_height, clutAllocInfo, mipmapCount );

            if ( hasAllocatedCLUT == false )
            {
                return false;
            }
        }

        // Finalize the allocation manager so that we can retrieve real values.
        memManager.FinalizeMemory();

        // Get the properties of the encoded mem layout.
        uint32 bitblt_blockPixelWidth = ps2GSMemoryLayoutManager::get_block_pixel_width( bitblt_layoutProps );
        uint32 bitblt_blockPixelHeight = ps2GSMemoryLayoutManager::get_block_pixel_height( bitblt_layoutProps );

        uint32 decodedPagePixelWidth = GetMemoryLayoutPagePixelWidth( decodedPixelLayoutProps );

        // Fetch results.
        // First the mipmaps.
        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            ps2GSMemoryLayoutManager::MemoryAllocation& mipAlloc = mipAllocs[ n ];

            prepareGSParametersForTexture(
                mipAlloc, decodedPagePixelWidth,
                bitblt_blockPixelWidth, bitblt_blockPixelHeight,
                bitblt_layoutProps,
                mipmapBasePointer[ n ], mipmapBufferWidth[ n ], mipmapTransData[ n ],
                false
            );
        }

        // Then the CLUT, if required.
        if ( hasCLUT )
        {
            // Get the properties of the CLUT memory layout.
            uint32 clut_blockPixelWidth = ps2GSMemoryLayoutManager::get_block_pixel_width( clutLayoutProps );
            uint32 clut_blockPixelHeight = ps2GSMemoryLayoutManager::get_block_pixel_height( clutLayoutProps );

            uint32 _clut_tbw;

            prepareGSParametersForTexture(
                clutAllocInfo, decodedPagePixelWidth,
                clut_blockPixelWidth, clut_blockPixelHeight,
                clutLayoutProps,
                clutBasePointerOut, _clut_tbw, clutTransDataOut,
                doesCLUTRequireNewTransmission( clutAllocInfo, bitblt_targetFmt, clutPixelStorageMode )
            );
        }
        else
        {
            // Reset to something.
            clutBasePointerOut = 0;
            clutTransDataOut.destX = 0;
            clutTransDataOut.destY = 0;
        }

        // Calculate the total size of the texture memory buffer in page size (page span).
        textureBufPageSizeOut = memManager.GetTextureMemoryPageSize();

        // Normalize all the remaining fields.
        for ( size_t n = mipmapCount; n < maxMipmaps; n++ )
        {
            mipmapBasePointer[ n ] = 0;
            mipmapBufferWidth[ n ] = 1;

            ps2MipmapTransmissionData& transData = mipmapTransData[ n ];

            transData.destX = 0;
            transData.destY = 0;
        }
    }

    return true;
}

bool NativeTexturePS2::allocateTextureMemory(
    uint32 mipmapBasePointer[], uint32 mipmapBufferWidth[], ps2MipmapTransmissionData mipmapTransData[], uint32 maxMipmaps,
    uint32& clutBasePointerOut, ps2MipmapTransmissionData& clutTransDataOut,
    uint32& textureBufPageSizeOut
) const
{
    // Allocate the memory.
    bool allocSuccess = allocateTextureMemoryNative(
        mipmapBasePointer, mipmapBufferWidth, mipmapTransData, maxMipmaps,
        clutBasePointerOut, clutTransDataOut,
        textureBufPageSizeOut
    );

    if ( allocSuccess == false )
    {
        return false;
    }

    // Convert the transition data to pixel offsets in the encoded format.
    size_t mipmapCount = this->mipmaps.GetCount();

    // Make sure buffer sizes are in their limits.
    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        uint32 thisBase = mipmapBasePointer[n];

        if ( thisBase >= 0x4000 )
        {
            return false;
        }

        uint32 thisBufferWidth = mipmapBufferWidth[n];

        if ( thisBufferWidth >= 64 )
        {
            return false;
        }
    }

    return true;
}

bool ps2NativeTextureTypeProvider::CalculateTextureMemoryAllocation(
    // Input values:
    ps2public::eMemoryLayoutType pixelStorageFmt, ps2public::eMemoryLayoutType bitbltTransferFmt, ps2public::eCLUTMemoryLayoutType clutStorageFmt,
    const ps2public::ps2TextureMetrics *mipmaps, size_t numMipmaps, const ps2public::ps2TextureMetrics& clut,
    // Output values:
    uint32 mipmapBasePointer[], uint32 mipmapBufferWidth[], ps2public::ps2MipmapTransmissionData mipmapTransData[],
    uint32& clutBasePointerOut, ps2public::ps2MipmapTransmissionData& clutTransDataOut,
    uint32& textureBufPageSizeOut
)
{
    static constexpr uint32 MAX_PS2_MIPMAPS = 7u;

    eMemoryLayoutType clutPixelStorageMode = GetCommonMemoryLayoutTypeFromCLUTLayoutType( GetInternalCLUTMemoryLayoutTypeFromPublic( clutStorageFmt ) );

    eMemoryLayoutType decodedPixelStorageMode = GetInternalMemoryLayoutTypeFromPublic( pixelStorageFmt );
    eMemoryLayoutType bitblt_targetFmt = GetInternalMemoryLayoutTypeFromPublic( bitbltTransferFmt );

    ps2MemoryLayoutTypeProperties bitblt_layoutProps = getMemoryLayoutTypeProperties( bitblt_targetFmt );
    ps2MemoryLayoutTypeProperties clutLayoutProps = getMemoryLayoutTypeProperties( clutPixelStorageMode );
    ps2MemoryLayoutTypeProperties decodedPixelLayoutProps = getMemoryLayoutTypeProperties( decodedPixelStorageMode );

    // Perform the allocation.
    {
        Interface *engineInterface = this->rwEngine;

        ps2GSMemoryLayoutManager memManager( engineInterface );

        ps2GSMemoryLayoutManager::MemoryAllocation mipAllocs[ MAX_PS2_MIPMAPS ];

        for ( uint32 n = 0; n < numMipmaps; n++ )
        {
            const ps2public::ps2TextureMetrics& gsTex = mipmaps[n];

            // Get the texel dimensions of this texture.
            uint32 encodedWidth = gsTex.transmissionAreaWidth;
            uint32 encodedHeight = gsTex.transmissionAreaHeight;

            if ( encodedWidth == 0 || encodedHeight == 0 )
            {
                return false;
            }

            ps2GSMemoryLayoutManager::MemoryAllocation allocInfo;

            bool hasAllocated = memManager.allocateTexture( bitblt_targetFmt, bitblt_layoutProps, encodedWidth, encodedHeight, allocInfo );

            if ( !hasAllocated )
            {
                return false;
            }

            if ( n >= MAX_PS2_MIPMAPS )
            {
                // We do not know how to handle more mipmaps than the hardware allows.
                // For safety reasons terminate.
                return false;
            }

            // Store the allocation for future reference.
            mipAllocs[ n ] = std::move( allocInfo );
        }

        // Allocate the palette data if required.
        bool hasCLUT = DoesMemoryLayoutTypeRequireCLUT( decodedPixelStorageMode );

        ps2GSMemoryLayoutManager::MemoryAllocation clutAllocInfo;

        if ( hasCLUT )
        {
            uint32 clut_width = clut.transmissionAreaWidth;
            uint32 clut_height = clut.transmissionAreaHeight;

            if ( clut_width == 0 || clut_height == 0 )
            {
                return false;
            }

            // Allocate it.
            bool hasAllocatedCLUT = memManager.allocateCLUT( clutPixelStorageMode, clutLayoutProps, clut_width, clut_height, clutAllocInfo, numMipmaps );

            if ( hasAllocatedCLUT == false )
            {
                return false;
            }
        }

        // Finalize the allocation manager so that we can retrieve real values.
        memManager.FinalizeMemory();

        // Get the properties of the encoded mem layout.
        uint32 bitblt_blockPixelWidth = ps2GSMemoryLayoutManager::get_block_pixel_width( bitblt_layoutProps );
        uint32 bitblt_blockPixelHeight = ps2GSMemoryLayoutManager::get_block_pixel_height( bitblt_layoutProps );

        uint32 decodedPagePixelWidth = GetMemoryLayoutPagePixelWidth( decodedPixelLayoutProps );

        // Fetch results.
        // First the mipmaps.
        for ( size_t n = 0; n < numMipmaps; n++ )
        {
            ps2GSMemoryLayoutManager::MemoryAllocation& mipAlloc = mipAllocs[ n ];

            ps2MipmapTransmissionData transData;

            prepareGSParametersForTexture(
                mipAlloc, decodedPagePixelWidth,
                bitblt_blockPixelWidth, bitblt_blockPixelHeight,
                bitblt_layoutProps,
                mipmapBasePointer[ n ], mipmapBufferWidth[ n ], transData,
                false
            );

            ps2public::ps2MipmapTransmissionData& publicTransData = mipmapTransData[ n ];
            publicTransData.destX = transData.destX;
            publicTransData.destY = transData.destY;
        }

        // Then the CLUT, if required.
        if ( hasCLUT )
        {
            // Get the properties of the CLUT memory layout.
            uint32 clut_blockPixelWidth = ps2GSMemoryLayoutManager::get_block_pixel_width( clutLayoutProps );
            uint32 clut_blockPixelHeight = ps2GSMemoryLayoutManager::get_block_pixel_height( clutLayoutProps );

            uint32 _clut_tbw;

            ps2MipmapTransmissionData transData;

            prepareGSParametersForTexture(
                clutAllocInfo, decodedPagePixelWidth,
                clut_blockPixelWidth, clut_blockPixelHeight,
                clutLayoutProps,
                clutBasePointerOut, _clut_tbw, transData,
                doesCLUTRequireNewTransmission( clutAllocInfo, bitblt_targetFmt, clutPixelStorageMode )
            );

            clutTransDataOut.destX = transData.destX;
            clutTransDataOut.destY = transData.destY;
        }
        else
        {
            // Reset to something.
            clutBasePointerOut = 0;
            clutTransDataOut.destX = 0;
            clutTransDataOut.destY = 0;
        }

        // Calculate the total size of the texture memory buffer in page size (page span).
        textureBufPageSizeOut = memManager.GetTextureMemoryPageSize();
    }

    return true;
}

bool NativeTexturePS2::getDebugBitmap( Bitmap& bmpOut ) const
{
    // Setup colors to use for the rectangles.
    struct singleColorSourcePipeline : public Bitmap::sourceColorPipeline
    {
        double red, green, blue, alpha;

        inline singleColorSourcePipeline( void )
        {
            this->red = 0;
            this->green = 0;
            this->blue = 0;
            this->alpha = 1.0;
        }

        uint32 getWidth( void ) const
        {
            return 1;
        }

        uint32 getHeight( void ) const
        {
            return 1;
        }

        void fetchcolor( uint32 x, uint32 y, double& red, double& green, double& blue, double& alpha )
        {
            red = this->red;
            green = this->green;
            blue = this->blue;
            alpha = this->alpha;
        }
    };

    singleColorSourcePipeline colorSrcPipe;

    size_t mipmapCount = this->mipmaps.GetCount();

    // Perform the allocation.
    const uint32 maxMipmaps = 7;

    uint32 mipmapBasePointer[ maxMipmaps ];
    uint32 mipmapBufferWidth[ maxMipmaps ];

    ps2MipmapTransmissionData mipmapTransData[ maxMipmaps ];

    uint32 clutBasePointer;
    uint32 texMemSize;

    ps2MipmapTransmissionData clutTransData;

    bool hasAllocated = this->allocateTextureMemoryNative(
        mipmapBasePointer, mipmapBufferWidth, mipmapTransData, maxMipmaps,
        clutBasePointer, clutTransData,
        texMemSize
    );

    if ( !hasAllocated )
        return false;

    for ( size_t n = 0; n < mipmapCount; n++ )
    {
        const NativeTexturePS2::GSTexture& gsTex = this->mipmaps[n];

        // Get the texel dimensions of this texture.
        uint32 encodedWidth = gsTex.transmissionAreaWidth;
        uint32 encodedHeight = gsTex.transmissionAreaHeight;

        const ps2MipmapTransmissionData& mipTransData = mipmapTransData[ n ];

        uint32 pixelOffX = mipTransData.destX;
        uint32 pixelOffY = mipTransData.destY;

        // Get the real width and height.
        uint32 texelWidth = encodedWidth;
        uint32 texelHeight = encodedHeight;

        // Make sure the bitmap is large enough for our drawing.
        uint32 reqWidth = pixelOffX + texelWidth;
        uint32 reqHeight = pixelOffY + texelHeight;

        bmpOut.enlargePlane( reqWidth, reqHeight );

        // Set special color depending on mipmap count.
        if ( n == 0 )
        {
            colorSrcPipe.red = 0.5666;
            colorSrcPipe.green = 0;
            colorSrcPipe.blue = 0;
        }
        else if ( n == 1 )
        {
            colorSrcPipe.red = 0;
            colorSrcPipe.green = 0.5666;
            colorSrcPipe.blue = 0;
        }
        else if ( n == 2 )
        {
            colorSrcPipe.red = 0;
            colorSrcPipe.green = 0;
            colorSrcPipe.blue = 1.0;
        }
        else if ( n == 3 )
        {
            colorSrcPipe.red = 1.0;
            colorSrcPipe.green = 1.0;
            colorSrcPipe.blue = 0;
        }
        else if ( n == 4 )
        {
            colorSrcPipe.red = 0;
            colorSrcPipe.green = 1.0;
            colorSrcPipe.blue = 1.0;
        }
        else if ( n == 5 )
        {
            colorSrcPipe.red = 1.0;
            colorSrcPipe.green = 1.0;
            colorSrcPipe.blue = 1.0;
        }
        else if ( n == 6 )
        {
            colorSrcPipe.red = 0.5;
            colorSrcPipe.green = 0.5;
            colorSrcPipe.blue = 0.5;
        }

        // Draw the rectangle.
        bmpOut.draw(
            colorSrcPipe, pixelOffX, pixelOffY, texelWidth, texelHeight,
            Bitmap::SHADE_SRCALPHA, Bitmap::SHADE_ONE, Bitmap::BLEND_ADDITIVE
        );
    }

    // Also render the palette if its there.
    if (this->paletteType != ePaletteTypePS2::NONE)
    {
        uint32 clutOffX = clutTransData.destX;
        uint32 clutOffY = clutTransData.destY;

        colorSrcPipe.red = 1;
        colorSrcPipe.green = 0.75;
        colorSrcPipe.blue = 0;

        uint32 palWidth = 0;
        uint32 palHeight = 0;

        if (this->paletteType == ePaletteTypePS2::PAL4)
        {
            palWidth = 8;
            palHeight = 3;
        }
        else if (this->paletteType == ePaletteTypePS2::PAL8)
        {
            palWidth = 16;
            palHeight = 16;
        }

        bmpOut.enlargePlane( palWidth + clutOffX, palHeight + clutOffY );

        bmpOut.draw(
            colorSrcPipe, clutOffX, clutOffY,
            palWidth, palHeight,
            Bitmap::SHADE_SRCALPHA, Bitmap::SHADE_ONE, Bitmap::BLEND_ADDITIVE
        );
    }

    return true;
}

}

#endif //RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2
