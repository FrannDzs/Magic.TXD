/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.execgroup.internal.h
*  PURPOSE:     Groups of fibers that execute under constraints
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATIVE_EXECUTIVE_EXECGROUP_INTERNAL_
#define _NATIVE_EXECUTIVE_EXECGROUP_INTERNAL_

BEGIN_NATIVE_EXECUTIVE

struct CFiberImpl;

struct CExecutiveGroupImpl : public CExecutiveGroup
{
    CExecutiveGroupImpl( CExecutiveManagerNative *manager );
    ~CExecutiveGroupImpl( void );

    void AddFiberNative( CFiberImpl *fiber );
    void AddFiber( CFiberImpl *fiber );

    CExecutiveManagerNative *manager;

    RwList <CFiberImpl> fibers;                         // all fibers registered to this group.

    RwListEntry <CExecutiveGroupImpl> managerNode;      // node in list of all groups.

    // Per frame settings
    // These times are given in milliseconds.
    double totalFrameExecutionTime;
    double maximumExecutionTime;

    double perfMultiplier;
};

END_NATIVE_EXECUTIVE

#endif //_NATIVE_EXECUTIVE_EXECGROUP_INTERNAL_