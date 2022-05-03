/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.hazard.h
*  PURPOSE:     Deadlock prevention by signaling code paths to continue execution
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATIVE_EXECUTIVE_HAZARD_PREVENTION_
#define _NATIVE_EXECUTIVE_HAZARD_PREVENTION_

#include <exception>

BEGIN_NATIVE_EXECUTIVE

struct hazardPreventionInterface abstract
{
    // Useful if you want the library to clean-up your interface.
    virtual ~hazardPreventionInterface( void )  {}

    // Called by the thread executive manager runtime when it has detected a dangerous
    // situation and wants threads associated with this resource to terminate properly.
    // The method implementation must atomatically destroy all resources.
    // It might not run on the same thread that owns the resources so be careful.
    // If the thread missed to pop the hazard during execution then it is executed during
    // cleanup instead; you should always pop your handlers in regular flow!
    // Never pop hazards inside catch handlers.
    virtual void TerminateHazard( void ) noexcept = 0;

    // The allocator to be used for dynamic delete_on_removal.
    typedef NatExecGlobalStaticAlloc dyn_alloc;
};

// Global API for managing hazards.
void PushHazard(
    CExecutiveManager *manager, hazardPreventionInterface *intf,
    bool delete_on_removal = false
);
void PopHazard( CExecutiveManager *manager, bool execute_handler = false );

// Helpers to make things easier for you.
struct hazardousSituation
{
    inline hazardousSituation( CExecutiveManager *manager, hazardPreventionInterface *intf )
    {
        this->manager = manager;
        this->intf = intf;

        PushHazard( manager, intf );
    }

    inline ~hazardousSituation( void ) noexcept(false)
    {
        try
        {
            PopHazard( manager );
        }
        catch( ... )
        {
            if ( std::uncaught_exceptions() == 0 )
            {
                throw;
            }

            // Just eat-away the exception throw.
            // We are handling an exception anyway.
        }
    }

private:
    CExecutiveManager *manager;
    hazardPreventionInterface *intf;
};

END_NATIVE_EXECUTIVE

#endif //_NATIVE_EXECUTIVE_HAZARD_PREVENTION_