/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/src/internal/CExecutiveManager.task.internal.h
*  PURPOSE:     Internal task implementation header file
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATIVE_EXECUTIVE_TASK_INTERNAL_
#define _NATIVE_EXECUTIVE_TASK_INTERNAL_

BEGIN_NATIVE_EXECUTIVE

struct CExecTaskImpl : public CExecTask
{
    CExecTaskImpl( CExecutiveManagerNative *manager, CFiber *runtime );
    ~CExecTaskImpl( void );

    CExecutiveManagerNative *manager;

    CFiber *runtimeFiber;

    taskexec_t callback;

    RwListEntry <CExecTask> node;

    void *finishEvent;

    bool isInitialized;
    bool isOnProcessedList;
    std::atomic <int> usageCount;
};

END_NATIVE_EXECUTIVE

#endif //_NATIVE_EXECUTIVE_TASK_INTERNAL_