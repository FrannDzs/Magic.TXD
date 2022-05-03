/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.fep.cpp
*  PURPOSE:     First-entry-point logic to pre-empt the runtime library
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#include "StdInc.h"

// Export "_native_executive_fep" function symbol that should be specified by
//  the linked project as real entry point; make it link into the CRT/STL/runtime
//  entry point (on MSVC this usually is mainCRTStartup).

#ifdef NATEXEC_FIRST_ENTRY_POINT

BEGIN_NATIVE_EXECUTIVE

// Module definitions.
extern void reference_library( void );
extern void dereference_library( void );

END_NATIVE_EXECUTIVE

#ifdef _WIN32

#include <Windows.h>

#ifdef _MSC_VER

#ifdef NATEXEC_FIRST_ENTRY_POINT_main

extern "C"
[[noreturn]]
void mainCRTStartup( void );

#define NAT_EXEC_FEP_RETURN_TYPE void
#define NAT_EXEC_FEP_ARGUMENTS ( void )

#elif defined(NATEXEC_FIRST_ENTRY_POINT_DllMain)

extern "C"
BOOL WINAPI _DllMainCRTStartup(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

#define NAT_EXEC_FEP_RETURN_TYPE BOOL WINAPI
#define NAT_EXEC_FEP_ARGUMENTS ( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved )

#elif defined(NATEXEC_FIRST_ENTRY_POINT_WinMain)

extern "C"
[[noreturn]]
void _WinMainCRTStartup( void );

#define NAT_EXEC_FEP_RETURN_TYPE void
#define NAT_EXEC_FEP_ARGUMENTS ( void )

#endif //ENTRY POINT SIGNATURES

#endif //_MSC_VER

static void _on_exit( void )
{
    // Clean stuff up that we set up in the entry point symbol.

    NativeExecutive::dereference_library();
}

// Executable starting point, for Windows.
extern "C" NAT_EXEC_FEP_RETURN_TYPE _native_executive_fep NAT_EXEC_FEP_ARGUMENTS
{
    NativeExecutive::reference_library();

#ifdef NATEXEC_FIRST_ENTRY_POINT_main
    mainCRTStartup();
#elif defined(NATEXEC_FIRST_ENTRY_POINT_DllMain)
    BOOL result = _DllMainCRTStartup( hinstDLL, fdwReason, lpReserved );

    NativeExecutive::dereference_library();

    return result;
#elif defined(NATEXEC_FIRST_ENTRY_POINT_WinMain)
    _WinMainCRTStartup();
#else
#error No implementation for CRT linkage.
#endif
}

#elif defined(__linux__)

extern "C" void _init_natexec( void )
{
    // This function is called by assembler code for initialization.

    NativeExecutive::reference_library();
}

#endif //CROSS PLATFORM CODE.

#endif //NATEXEC_FIRST_ENTRY_POINT
