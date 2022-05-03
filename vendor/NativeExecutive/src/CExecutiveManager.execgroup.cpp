/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.execgroup.cpp
*  PURPOSE:     Groups of fibers execution under constraints.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "internal/CExecutiveManager.execgroup.internal.h"
#include "internal/CExecutiveManager.fiber.internal.h"

BEGIN_NATIVE_EXECUTIVE

CExecutiveGroupImpl::CExecutiveGroupImpl( CExecutiveManagerNative *manager )
{
    this->manager = manager;
    this->maximumExecutionTime = DEFAULT_GROUP_MAX_EXEC_TIME;
    this->totalFrameExecutionTime = 0;

    this->perfMultiplier = 1.0f;

    LIST_APPEND( manager->groups.root, managerNode );
}

CExecutiveGroupImpl::~CExecutiveGroupImpl( void )
{
    while ( !LIST_EMPTY( fibers.root ) )
    {
        CFiberImpl *fiber = LIST_GETITEM( CFiberImpl, fibers.root.next, groupNode );

        manager->CloseFiber( fiber );
    }

    LIST_REMOVE( managerNode );
}

void CExecutiveGroupImpl::AddFiberNative( CFiberImpl *fiber )
{
    LIST_APPEND( this->fibers.root, fiber->groupNode );

    fiber->group = this;
}

void CExecutiveGroupImpl::AddFiber( CFiberImpl *fiber )
{
    LIST_REMOVE( fiber->groupNode );
        
    AddFiberNative( fiber );
}

void CExecutiveGroup::AddFiber( CFiber *fiber )
{
    CExecutiveGroupImpl *nativeGroup = (CExecutiveGroupImpl*)this;
    CFiberImpl *nativeFiber = (CFiberImpl*)fiber;

    nativeGroup->AddFiber( nativeFiber );
}

void CExecutiveGroup::SetMaximumExecutionTime( double ms )
{
    CExecutiveGroupImpl *nativeGroup = (CExecutiveGroupImpl*)this;
    
    nativeGroup->maximumExecutionTime = ms;
}

double CExecutiveGroup::GetMaximumExecutionTime( void ) const
{
    CExecutiveGroupImpl *nativeGroup = (CExecutiveGroupImpl*)this;

    return nativeGroup->maximumExecutionTime;
}

void CExecutiveGroup::DoPulse( void )
{
    CExecutiveGroupImpl *nativeGroup = (CExecutiveGroupImpl*)this;

    nativeGroup->totalFrameExecutionTime = 0;
}

void CExecutiveGroup::SetPerfMultiplier( double mult )
{
    CExecutiveGroupImpl *nativeGroup = (CExecutiveGroupImpl*)this;

    nativeGroup->perfMultiplier = mult;
}

double CExecutiveGroup::GetPerfMultiplier( void ) const
{
    CExecutiveGroupImpl *nativeGroup = (CExecutiveGroupImpl*)this;

    return nativeGroup->perfMultiplier;
}

END_NATIVE_EXECUTIVE