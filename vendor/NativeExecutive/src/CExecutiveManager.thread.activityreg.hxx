/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.activityreg.hxx
*  PURPOSE:     Thread activity registration and handling.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_THREAD_ACTIVITY_ENV_
#define _EXECUTIVE_MANAGER_THREAD_ACTIVITY_ENV_

BEGIN_NATIVE_EXECUTIVE

void suspend_thread_activities( CExecutiveManagerNative *execMan );
void resume_thread_activities( CExecutiveManagerNative *execMan );

struct stacked_thread_activity_suspend
{
    AINLINE stacked_thread_activity_suspend( CExecutiveManagerNative *execMan )
    {
        this->nativeMan = execMan;

        suspend_thread_activities( execMan );
    }

    AINLINE ~stacked_thread_activity_suspend( void )
    {
        resume_thread_activities( this->nativeMan );
    }

private:
    CExecutiveManagerNative *nativeMan;
};

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_THREAD_ACTIVITY_ENV_