/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.fiber.h
*  PURPOSE:     Executive manager fiber logic
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_FIBER_
#define _EXECUTIVE_MANAGER_FIBER_

BEGIN_NATIVE_EXECUTIVE

class CFiber
{
public:
    typedef void (*fiberexec_t)( CFiber *fiber, void *userdata );

    bool is_running( void ) const;
    bool is_terminated( void ) const;

    // Manager functions
    void push_on_stack( void );
    void pop_from_stack( void );
    bool is_current_on_stack( void ) const;

    // Native functions that skip manager activity.
    void resume( void );
    void yield( void );

    void check_termination( void );

    // Managed methods that use logic.
    void yield_proc( void );
};

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_FIBER_
