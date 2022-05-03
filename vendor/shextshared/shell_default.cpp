// Implement some default shell extension quirks.

//#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <atomic>

extern bool IsValidCLSID( REFCLSID rclsid );
extern IClassFactory* CreateClassFactory( REFCLSID rclsid );

extern void InitializeShellExtModule( void );
extern void ShutdownShellExtModule( void );

HMODULE module_instance = NULL;
std::atomic <unsigned long> module_refCount = 0;

__declspec( dllexport ) BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch( dwReason )
    {
    case DLL_PROCESS_ATTACH:
        module_instance = hModule;

        InitializeShellExtModule();

        break;
    case DLL_PROCESS_DETACH:
        ShutdownShellExtModule();

        module_instance = NULL;
        break;
    }

    return TRUE;
}

STDAPI DllCanUnloadNow( void )
{
    return ( module_refCount == 0 ? S_OK : S_FALSE );
}

STDAPI DllGetClassObject( REFCLSID rclsid, REFIID riid, void **ppOut )
{
    if ( IsValidCLSID( rclsid ) )
    {
        if ( riid == __uuidof(IClassFactory) )
        {
            IClassFactory *shellFact = CreateClassFactory( riid );

            if ( shellFact )
            {
                *ppOut = shellFact;
                return S_OK;
            }

            return E_OUTOFMEMORY;
        }
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}