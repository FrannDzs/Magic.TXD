/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.evtwait.cpp
*  PURPOSE:     Per-thread wait event registration
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

#include "CExecutiveManager.evtwait.hxx"

BEGIN_NATIVE_EXECUTIVE

// We just use this file as storage-space and initializer for the plugin.
constinit optional_struct_space <eventWaiterEnvReg_t> eventWaiterEnvReg;

void registerEventWaiterEnvironment( void )
{
    eventWaiterEnvReg.Construct( executiveManagerFactory );
}

void unregisterEventWaiterEnvironment( void )
{
    eventWaiterEnvReg.Destroy();
}

END_NATIVE_EXECUTIVE