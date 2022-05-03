#include "StdInc.h"
#include <atomic>

#include <shlobj.h>                 // For SHChangeNotify

// {A16ACA16-0F66-480B-9F5C-B42E29761A9B}
const CLSID CLSID_RenderWareThumbnailProvider =
{ 0xa16aca16, 0xf66, 0x480b, { 0x9f, 0x5c, 0xb4, 0x2e, 0x29, 0x76, 0x1a, 0x9b } };

// {EE4F0E71-7A95-42B0-8064-C284EF1A0AC2}
const CLSID CLSID_RenderWareContextMenuProvider = 
{ 0xee4f0e71, 0x7a95, 0x42b0, { 0x80, 0x64, 0xc2, 0x84, 0xef, 0x1a, 0xa, 0xc2 } };


HMODULE module_instance = NULL;
std::atomic <unsigned long> module_refCount = 0;

rw::Interface *rwEngine = NULL;

// Sub modules.
extern void InitializeRwWin32StreamEnv( void );

extern void ShutdownRwWin32StreamEnv( void );

__declspec( dllexport ) BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch( dwReason )
    {
    case DLL_PROCESS_ATTACH:
        module_instance = hModule;

        //DisableThreadLibraryCalls( hModule );

        // Initialize the module-wide RenderWare environment.
        {
            rw::LibraryVersion libVer;
            libVer.rwLibMajor = 3;      // the newest RenderWare version ever released under RW3.
            libVer.rwLibMinor = 7;
            libVer.rwRevMajor = 0;
            libVer.rwRevMinor = 0;

            rwEngine = rw::CreateEngine( std::move( libVer ) );

            if ( rwEngine )
            {
                // Give information about this tool.
                rw::softwareMetaInfo metaInfo;
                metaInfo.applicationName = "RWtools_win32shell";
                metaInfo.applicationVersion = "shell";
                metaInfo.description = "Win32 shell utilities for RenderWare files (https://github.com/quiret/magic-txd)";

                rwEngine->SetApplicationInfo( metaInfo );

                // We need to properly initialize this engine for common-purpose operation.
                rwEngine->SetPaletteRuntime( rw::PALRUNTIME_PNGQUANT );
                rwEngine->SetDXTRuntime( rw::DXTRUNTIME_SQUISH );
                rwEngine->SetFixIncompatibleRasters( true );
                rwEngine->SetCompatTransformNativeImaging( true );
                rwEngine->SetPreferPackedSampleExport( true );
                rwEngine->SetIgnoreSerializationBlockRegions( true );
                rwEngine->SetMetaDataTagging( true );
                rwEngine->SetWarningLevel( 0 );     // the shell is not a warning provider; use Magic.TXD instead.
            }
        }

        InitializeRwWin32StreamEnv();
        break;
    case DLL_PROCESS_DETACH:
        ShutdownRwWin32StreamEnv();

        // Destroy the RenderWare environment.
        if ( rwEngine )
        {
            rw::DeleteEngine( rwEngine );
        }

        module_instance = NULL;
        break;
    }

    return TRUE;
}

// Implementation of the factory object.
struct shellClassFactory : public IClassFactory
{
    // IUnknown
    IFACEMETHODIMP QueryInterface( REFIID riid, void **ppv )
    {
        if ( ppv == NULL )
            return E_POINTER;

        if ( riid == __uuidof(IClassFactory) )
        {
            this->refCount++;

            IClassFactory *comObj = this;

            *ppv = comObj;
            return S_OK;
        }
        else if ( riid == __uuidof(IUnknown) )
        {
            this->refCount++;

            IUnknown *comObj = this;

            *ppv = comObj;
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef( void )
    {
        return ++refCount;
    }

    IFACEMETHODIMP_(ULONG) Release( void )
    {
        unsigned long newRefCount = --refCount;

        if ( newRefCount == 0 )
        {
            // We can clean up after ourselves.
            delete this;
        }

        return newRefCount;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
    {
        if ( pUnkOuter != NULL )
        {
            return CLASS_E_NOAGGREGATION;
        }

        if ( riid == __uuidof(IThumbnailProvider) )
        {
            IThumbnailProvider *prov = new (std::nothrow) RenderWareThumbnailProvider();

            if ( !prov )
            {
                return E_OUTOFMEMORY;
            }

            *ppv = prov;
            return S_OK;
        }
        else if ( riid == __uuidof(IContextMenu) )
        {
            IContextMenu *prov = new (std::nothrow) RenderWareContextHandlerProvider();

            if ( !prov )
            {
                return E_OUTOFMEMORY;
            }

            *ppv = prov;
            return S_OK;
        }
        else if ( riid == __uuidof(IShellExtInit) )
        {
            IShellExtInit *prov = new (std::nothrow) RenderWareContextHandlerProvider();

            if ( !prov )
            {
                return E_OUTOFMEMORY;
            }

            *ppv = prov;
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    IFACEMETHODIMP LockServer(BOOL fLock)
    {
        if ( fLock )
        {
            module_refCount++;
        }
        else
        {
            module_refCount--;
        }

        return S_OK;
    }

    shellClassFactory( void )
    {
        module_refCount++;
    }

protected:
    ~shellClassFactory( void )
    {
        module_refCount--;
    }

private:
    std::atomic <unsigned long> refCount;
};

STDAPI DllCanUnloadNow( void )
{
    return ( module_refCount == 0 ? S_OK : S_FALSE );
}

STDAPI DllGetClassObject( REFCLSID rclsid, REFIID riid, void **ppOut )
{
    if ( IsEqualCLSID( rclsid, CLSID_RenderWareThumbnailProvider ) ||
         IsEqualCLSID( rclsid, CLSID_RenderWareContextMenuProvider ) )
    {
        if ( riid == __uuidof(IClassFactory) )
        {
            shellClassFactory *thumbFactory = new (std::nothrow) shellClassFactory();

            if ( thumbFactory )
            {
                IClassFactory *comObj = thumbFactory;

                *ppOut = comObj;
                return S_OK;
            }

            return E_OUTOFMEMORY;
        }
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

//
//   FUNCTION: DllRegisterServer
//
//   PURPOSE: Register the COM server and the thumbnail handler.
// 
STDAPI DllRegisterServer(void)
{
    HRESULT hr;

    wchar_t szModule[MAX_PATH];

    if (GetModuleFileNameW(module_instance, szModule, ARRAYSIZE(szModule)) == 0)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    // Register the thumbnail handler.
    hr = RegisterInprocServer(szModule, CLSID_RenderWareThumbnailProvider, 
        L"rwthumb.RenderWareThumbnailProvider Class", 
        L"Apartment");

    if ( SUCCEEDED(hr) )
    {
        hr = RegisterShellExtThumbnailHandler(L".txd", CLSID_RenderWareThumbnailProvider);

        if ( SUCCEEDED( hr ) )
        {
            hr = RegisterShellExtThumbnailHandler(L".rwtex", CLSID_RenderWareThumbnailProvider);

            if ( SUCCEEDED(hr) )
            {
                if ( auto SHChangeNotify = shell32min.SHChangeNotify )
                {
                    // This tells the shell to invalidate the thumbnail cache. It is 
                    // important because any files viewed before registering 
                    // this handler would otherwise show cached blank thumbnails.
                    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
                }
            }
        }
    }

    // Register the context menu handler.
    if ( SUCCEEDED(hr) )
    {
        hr = RegisterInprocServer(
            szModule, CLSID_RenderWareContextMenuProvider,
            L"rwthumb.RenderWareContextMenuProvider Class",
            L"Apartment"
        );

        if ( SUCCEEDED(hr) )
        {
            hr = RegisterShellExtContextMenuHandler(
                L".txd", CLSID_RenderWareContextMenuProvider,
                L"rwthumb.RenderWareContextMenuProvider"
            );

            if ( SUCCEEDED(hr) )
            {
                hr = RegisterShellExtContextMenuHandler(
                    L".rwtex", CLSID_RenderWareContextMenuProvider,
                    L"rwthumb.RenderWareContextMenuProvider"
                );
            }
        }
    }

    return hr;
}


//
//   FUNCTION: DllUnregisterServer
//
//   PURPOSE: Unregister the COM server and the thumbnail handler.
// 
STDAPI DllUnregisterServer(void)
{
    HRESULT hr = S_OK;

    wchar_t szModule[MAX_PATH];

    if (GetModuleFileNameW( module_instance, szModule, ARRAYSIZE(szModule) ) == 0)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    // Unregister the context menu handler.
    if ( SUCCEEDED( hr ) )
    {
        hr = UnregisterInprocServer( CLSID_RenderWareContextMenuProvider );

        if ( SUCCEEDED( hr ) )
        {
            hr = UnregisterShellExtContextMenuHandler( L".txd", CLSID_RenderWareContextMenuProvider );

            if ( SUCCEEDED( hr ) )
            {
                hr = UnregisterShellExtContextMenuHandler( L".rwtex", CLSID_RenderWareContextMenuProvider );
            }
        }
    }

    // Unregister the thumbnail handler
    if ( SUCCEEDED( hr ) )
    {
        hr = UnregisterInprocServer( CLSID_RenderWareThumbnailProvider );

        if ( SUCCEEDED(hr) )
        {
            // Unregister the thumbnail handler.
            hr = UnregisterShellExtThumbnailHandler(L".txd");

            if ( SUCCEEDED(hr) )
            {
                hr = UnregisterShellExtThumbnailHandler(L".rwtex");
            }
        }
    }

    return hr;
}