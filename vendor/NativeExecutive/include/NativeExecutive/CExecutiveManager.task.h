/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.task.h
*  PURPOSE:     Runtime for quick parallel execution sheduling
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_TASKS_
#define _EXECUTIVE_MANAGER_TASKS_

BEGIN_NATIVE_EXECUTIVE

class CExecTask
{
public:
    typedef void (*taskexec_t)( CExecTask *task, void *userdata );

    void    Execute( void );
    void    WaitForFinish( void );

    bool    IsFinished( void ) const;
};

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_TASKS_
