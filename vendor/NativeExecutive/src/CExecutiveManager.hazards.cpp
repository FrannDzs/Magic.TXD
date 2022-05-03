/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.hazards.cpp
*  PURPOSE:     Thread hazard management internals, to prevent deadlocks
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "CExecutiveManager.hazards.hxx"

BEGIN_NATIVE_EXECUTIVE

constinit optional_struct_space <executiveHazardManagerEnvRegister_t> executiveHazardManagerEnvRegister;

template <typename callbackType>
inline void _hazardreg_safe_modify_gateway( executiveHazardManagerEnv *hazardEnv, CExecThreadImpl *currentThread, const callbackType& cb )
{
    // First we try the fiber stack.
    // There are different rules for it.
    if ( CFiberImpl *currentFiber = (CFiberImpl*)currentThread->GetCurrentFiber() )
    {
        // We can only modify the fiber if it is running.
        // Since fibers can only self-terminate (not remote-terminate) we are safe to check that.
        currentFiber->check_termination();

        stackObjectHazardRegistry *fiberReg = hazardEnv->GetFiberHazardRegistry( currentFiber );

#ifdef _DEBUG
        assert( currentFiber->is_running() == true );
#endif //_DEBUG

        if ( fiberReg )
        {
            cb( fiberReg );
        }
    }
    else
    {
        CUnfairMutexContext ctx_threadStatus( currentThread->mtxThreadStatus );

        currentThread->CheckTerminationRequest();

#ifdef _DEBUG
        assert( currentThread->GetStatus() == THREAD_RUNNING );
#endif //_DEBUG

        stackObjectHazardRegistry *threadReg = hazardEnv->GetThreadHazardRegistry( currentThread );

        if ( threadReg )
        {
            cb( threadReg );
        }
    }
}

// Hazard API implementation.
void PushHazard( CExecutiveManager *manager, hazardPreventionInterface *intf, bool delete_on_removal )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)manager;

    executiveHazardManagerEnv *hazardEnv = executiveHazardManagerEnvRegister.get().GetPluginStruct( nativeMan );

    if ( hazardEnv )
    {
        CExecThreadImpl *currentThread = (CExecThreadImpl*)nativeMan->GetCurrentThread( false );

        assert( currentThread != nullptr );

        try
        {
            _hazardreg_safe_modify_gateway( hazardEnv, currentThread,
                [&]( stackObjectHazardRegistry *reg )
                {
                    reg->PushHazard( intf, delete_on_removal );
                }
            );
        }
        catch( ... )
        {
            // If the hazard could not be registered then we clean it up straight away.
            intf->TerminateHazard();

            // Also release it if requested.
            if ( delete_on_removal )
            {
                eir::static_del_struct <hazardPreventionInterface, hazardPreventionInterface::dyn_alloc> ( nullptr, intf );
            }

            throw;
        }
    }
}

void PopHazard( CExecutiveManager *manager, bool execute_handler )
{
    CExecutiveManagerNative *nativeMan = (CExecutiveManagerNative*)manager;

    executiveHazardManagerEnv *hazardEnv = executiveHazardManagerEnvRegister.get().GetPluginStruct( nativeMan );

    if ( hazardEnv )
    {
        CExecThreadImpl *currentThread = (CExecThreadImpl*)nativeMan->GetCurrentThread( true );

#ifdef _DEBUG
        assert( currentThread != nullptr );
#endif //_DEBUG

        if ( currentThread != nullptr )
        {
            _hazardreg_safe_modify_gateway( hazardEnv, currentThread,
                [&]( stackObjectHazardRegistry *reg )
                {
                    reg->PopHazard( execute_handler );
                }
            );
        }
    }
}

void registerStackHazardManagement( void )
{
    executiveHazardManagerEnvRegister.Construct( executiveManagerFactory );
}

void unregisterStackHazardManagement( void )
{
    executiveHazardManagerEnvRegister.Destroy();
}

END_NATIVE_EXECUTIVE