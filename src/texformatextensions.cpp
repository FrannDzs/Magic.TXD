/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/texformatextensions.cpp
*  PURPOSE:     magf plugin implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#ifdef _WIN32
#include <d3d9.h>
#include <cwchar>
#include <locale>
#endif //_WIN32

#include "texformathelper.hxx"

#ifdef _WIN32
#include <Windows.h>
#endif //_WIN32

inline const wchar_t* GetMAGFDir( void )
{
    return
        L"formats"
#ifdef _DEBUG
        L"_d"
#endif
#ifdef _M_AMD64
        L"_x64"
#endif
#ifdef _BUILD_LEGACY
        L"_legacy"
#endif
        ;
}

#ifdef _WIN32
typedef void (__cdecl* LPFNSETINTERFACE)( const MagicFormatPluginInterface *intf );
typedef MagicFormat* (__cdecl* LPFNDLLFUNC1)(unsigned int&);

struct MagicFormat_Ver1handler : public rw::d3dpublic::nativeTextureFormatHandler
{
    inline MagicFormat_Ver1handler( MagicFormat *handler )
    {
        this->libHandler = handler;
    }

    const char*     GetFormatName( void ) const override
    {
        return libHandler->GetFormatName();
    }

    size_t GetFormatTextureDataSize( unsigned int width, unsigned int height ) const override
    {
        return libHandler->GetFormatTextureDataSize( width, height );
    }

    void GetTextureRWFormat( rw::eRasterFormat& rasterFormatOut, unsigned int& depthOut, rw::eColorOrdering& colorOrderOut ) const
    {
        MAGIC_RASTER_FORMAT mrasterformat;
        unsigned int mdepth;
        MAGIC_COLOR_ORDERING mcolororder;

        libHandler->GetTextureRWFormat( mrasterformat, mdepth, mcolororder );

        MagicMapToInternalRasterFormat( mrasterformat, rasterFormatOut );

        depthOut = mdepth;

        MagicMapToInternalColorOrdering( mcolororder, colorOrderOut );
    }

    virtual void ConvertToRW(
        const void *texData, unsigned int texMipWidth, unsigned int texMipHeight, size_t dstRowStride, size_t texDataSize,
        void *texOut
    ) const override
    {
        libHandler->ConvertToRW(
            texData, texMipWidth, texMipHeight, dstRowStride, texDataSize,
            texOut
        );
    }

    virtual void ConvertFromRW(
        unsigned int texMipWidth, unsigned int texMipHeight, unsigned int texRowAlignment,
        const void *texelSource, rw::eRasterFormat rasterFormat, unsigned int depth, rw::eColorOrdering colorOrder, rw::ePaletteType paletteType, const void *paletteData, unsigned int paletteSize,
        void *texOut
    ) const override
    {
        MAGIC_RASTER_FORMAT mrasterformat;
        MAGIC_COLOR_ORDERING mcolororder;
        MAGIC_PALETTE_TYPE mpalettetype;

        MagicMapToVirtualRasterFormat( rasterFormat, mrasterformat );
        MagicMapToVirtualColorOrdering( colorOrder, mcolororder );
        MagicMapToVirtualPaletteType( paletteType, mpalettetype );

        libHandler->ConvertFromRW(
            texMipWidth, texMipHeight, texRowAlignment,
            texelSource, mrasterformat, depth, mcolororder, mpalettetype, paletteData, paletteSize,
            texOut
        );
    }

private:
    MagicFormat *libHandler;
};

static optional_struct_space <MagicFormatPluginExports> _funcExportIntf;
#endif //_WIN32

struct magictxdCustomFormatEnv
{
    inline void Initialize( MainWindow *mainWnd )
    {
#ifdef _WIN32
        _funcExportIntf.Construct();

        // Register a basic format that we want to test things on.
        // We only can do that if the engine has the Direct3D9 native texture loaded.
        rw::d3dpublic::d3dNativeTextureDriverInterface *driverIntf = (rw::d3dpublic::d3dNativeTextureDriverInterface*)rw::GetNativeTextureDriverInterface( mainWnd->GetEngine(), "Direct3D9" );

        if ( driverIntf )
        {
            WIN32_FIND_DATAW FindFileData;
            memset(&FindFileData, 0, sizeof(FindFileData));
            std::wstring magfpath = mainWnd->m_appPath.toStdWString();
            std::wstring path = magfpath + L'/' + GetMAGFDir() + L'/' + L"*.magf";
            HANDLE hFind = FindFirstFileW(path.c_str(), &FindFileData);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        std::wstring wPluginName = FindFileData.cFileName;
                        std::wstring filename = magfpath + L'/' + GetMAGFDir() + L'/' + FindFileData.cFileName;
                        HMODULE hDLL = LoadLibraryW(filename.c_str());
                        if (hDLL != nullptr)
                        {
                            bool success = false;

                            LPFNDLLFUNC1 func = (LPFNDLLFUNC1)GetProcAddress(hDLL, "GetFormatInstance");
                            LPFNSETINTERFACE intfFunc = (LPFNSETINTERFACE)GetProcAddress( hDLL, "SetInterface" );
                            if (func && intfFunc)
                            {
                                unsigned int magf_version = 0;

                                MagicFormat *handler = func( magf_version );

                                // We must have correct ABI version to load.
                                if ( magf_version == MagicFormatAPIVersion() )
                                {
                                    // Give it our module interface.
                                    intfFunc( &_funcExportIntf.get() );

                                    MagicFormat_Ver1handler *vhandler = new MagicFormat_Ver1handler( handler );

                                    bool hasRegistered = driverIntf->RegisterFormatHandler(handler->GetD3DFormat(), vhandler);

                                    if ( hasRegistered )
                                    {
                                        magf_extension reg_entry;
                                        reg_entry.d3dformat = handler->GetD3DFormat();
                                        reg_entry.loadedLibrary = hDLL;
                                        reg_entry.handler = vhandler;

                                        this->magf_formats.push_back( reg_entry );

                                        success = true;

                                        QString message =
                                            MAGIC_TEXT("MagicFmt.PluginLoad").arg(QString::fromStdWString( wPluginName ), handler->GetFormatName());

                                        mainWnd->txdLog->addLogMessage(std::move(message), LOGMSG_INFO);
                                    }
                                    else
                                    {
                                        delete vhandler;
                                    }
                                }
                                else
                                {
                                    QString message =
                                        MAGIC_TEXT("MagicFmt.IncorrectVer").arg(QString::fromStdWString( wPluginName ));

                                    mainWnd->txdLog->showError(std::move(message));
                                }
                            }
                            else
                            {
                                QString message =
                                    MAGIC_TEXT("MagicFmt.ErrCorrupted").arg(QString::fromStdWString( wPluginName ));

                                mainWnd->txdLog->showError(std::move(message));
                            }

                            if ( success == false )
                            {
                                FreeLibrary( hDLL );
                            }
                        }
                        else
                        {
                            DWORD lastError = GetLastError();

                            QString message =
                                MAGIC_TEXT("MagicFmt.LoadError").arg(QString::fromStdWString( wPluginName ), ansi_to_qt( std::to_string( lastError ) ));

                            mainWnd->txdLog->showError(std::move(message));
                        }
                    }
                } while (FindNextFileW(hFind, &FindFileData));
                FindClose(hFind);
            }
        }
#endif //_WIN32
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
#ifdef _WIN32
        rw::d3dpublic::d3dNativeTextureDriverInterface *driverIntf = (rw::d3dpublic::d3dNativeTextureDriverInterface*)rw::GetNativeTextureDriverInterface( mainWnd->GetEngine(), "Direct3D9" );

        if ( driverIntf )
        {
            // Unload all formats.
            for ( magf_formats_t::const_iterator iter = this->magf_formats.begin(); iter != this->magf_formats.end(); iter++ )
            {
                const magf_extension& ext = *iter;

                // Unregister the plugin from the engine.
                driverIntf->UnregisterFormatHandler( ext.d3dformat );

                // Delete the virtual interface.
                {
                    MagicFormat_Ver1handler *vhandler = (MagicFormat_Ver1handler*)ext.handler;

                    delete vhandler;
                }

                // Unload the library.
                FreeLibrary( (HMODULE)ext.loadedLibrary );
            }

            // Clear the list of resident formats.
            this->magf_formats.clear();
        }

        _funcExportIntf.Destroy();
#endif //_WIN32
    }

    struct magf_extension
    {
        D3DFORMAT_SDK d3dformat;
        void *loadedLibrary;
        void *handler;
    };

    typedef std::list <magf_extension> magf_formats_t;

    magf_formats_t magf_formats;
};

static optional_struct_space <PluginDependantStructRegister <magictxdCustomFormatEnv, mainWindowFactory_t>> magictxdCustomFormatEnvRegister;

void InitializeCustomFormatsEnvironment( void )
{
    magictxdCustomFormatEnvRegister.Construct( mainWindowFactory );
}

void ShutdownCustomFormatsEnvironment( void )
{
    magictxdCustomFormatEnvRegister.Destroy();
}