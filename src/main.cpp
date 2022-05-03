/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/main.cpp
*  PURPOSE:     Initialization of the editor.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include <QtWidgets/QApplication>
#include "styles.h"
#include <QtCore/QTimer>
#include "resource.h"

#include <NativeExecutive/CExecutiveManager.h>

#include <QtCore/QtPlugin>

#include <exception>

#ifdef _WIN32
#include <Windows.h>
#endif //_WIN32

#ifdef QT_STATIC
#ifdef _WIN32
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#elif defined(__linux__)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif //CROSS PLATFORM CODE

// Import all required image formats.
Q_IMPORT_PLUGIN(QGifPlugin)
#endif //QT_STATIC

#include <QtGui/QImageWriter>

struct ScopedSystemEventFilter
{
    inline ScopedSystemEventFilter(QObject *receiver, QEvent *evt)
    {
        _currentFilter = this;

        QWidget *theWidget = nullptr;

        this->evt = nullptr;
        this->handlerWidget = nullptr;

        if ( receiver )
        {
            if (receiver->isWidgetType())
            {
                theWidget = (QWidget*)receiver;
            }
        }

        if (theWidget)
        {
            // We are not there yet.
            // Check whether this widget supports system event signalling.
            SystemEventHandlerWidget *hWidget = dynamic_cast <SystemEventHandlerWidget*> (theWidget);

            if (hWidget)
            {
                hWidget->beginSystemEvent(evt);

                this->evt = evt;
                this->handlerWidget = hWidget;
            }
        }
    }

    inline ~ScopedSystemEventFilter(void)
    {
        if (handlerWidget)
        {
            handlerWidget->endSystemEvent(evt);
        }

        _currentFilter = nullptr;
    }

    static ScopedSystemEventFilter *_currentFilter;

    QEvent *evt;
    SystemEventHandlerWidget *handlerWidget;
};

ScopedSystemEventFilter* ScopedSystemEventFilter::_currentFilter = nullptr;

SystemEventHandlerWidget::~SystemEventHandlerWidget(void)
{
    ScopedSystemEventFilter *sysEvtFilter = ScopedSystemEventFilter::_currentFilter;

    if (sysEvtFilter)
    {
        if (sysEvtFilter->handlerWidget == this)
        {
            sysEvtFilter->handlerWidget = nullptr;
            sysEvtFilter->evt = nullptr;
        }
    }
}

struct MagicTXDApplication : public QApplication
{
    inline MagicTXDApplication(int& argc, char *argv[]) : QApplication(argc, argv)
    {
        return;
    }

    bool notify(QObject *receiver, QEvent *evt) override
    {
        ScopedSystemEventFilter filter(receiver, evt);

        return QApplication::notify(receiver, evt);
    }
};

// Main window factory.
optional_struct_space <mainWindowFactory_t> mainWindowFactory;

struct mainWindowConstructor
{
    QString appPath;
    rw::Interface *rwEngine;
    CFileSystem *fsHandle;

    inline mainWindowConstructor(QString&& appPath, rw::Interface *rwEngine, CFileSystem *fsHandle)
        : appPath(appPath), rwEngine(rwEngine), fsHandle(fsHandle)
    {}

    inline MainWindow* Construct(void *mem) const
    {
        return new (mem) MainWindow(appPath, rwEngine, fsHandle);
    }
};

// Main window plugin entry points.
extern void InitializeRWFileSystemWrap( void );
extern void InitializeTaskCompletionWindowEnv( void );
extern void InitializeSerializationStorageEnv( void );
extern void InitializeMainWindowSerializationBlock( void );
extern void InitializeMainWindowThemeSupport( void );
extern void InitializeMagicLanguages( void );
extern void InitializeCustomFormatsEnvironment( void );
extern void InitializeMainWindowMenu( void );
extern void InitializeMainWindowRenderWarePlugins( void );
extern void InitializeHelperRuntime( void );
extern void InitializeMainWindowHelpEnv( void );
extern void InitializeTextureAddDialogEnv( void );
extern void InitializeExportAllWindowSerialization( void );
extern void InitializeMassconvToolEnvironment( void );
extern void InitializeMassExportToolEnvironment( void );
extern void InitializeMassBuildEnvironment( void );
extern void InitializeMainWindowStatusBarAnimations( void );
extern void InitializeGUISerialization( void );
extern void InitializeStreamCompressionEnvironment( void );

extern void ShutdownRWFileSystemWrap( void );
extern void ShutdownTaskCompletionWindowEnv( void );
extern void ShutdownSerializationStorageEnv( void );
extern void ShutdownMainWindowSerializationBlock( void );
extern void ShutdownMainWindowThemeSupport( void );
extern void ShutdownMagicLanguages( void );
extern void ShutdownCustomFormatsEnvironment( void );
extern void ShutdownMainWindowMenu( void );
extern void ShutdownMainWindowRenderWarePlugins( void );
extern void ShutdownHelperRuntime( void );
extern void ShutdownMainWindowHelpEnv( void );
extern void ShutdownTextureAddDialogEnv( void );
extern void ShutdownExportAllWindowSerialization( void );
extern void ShutdownMassconvToolEnvironment( void );
extern void ShutdownMassExportToolEnvironment( void );
extern void ShutdownMassBuildEnvironment( void );
extern void ShutdownMainWindowStatusBarAnimations( void );
extern void ShutdownGUISerialization( void );
extern void ShutdownStreamCompressionEnvironment( void );

struct _environment_register
{
    inline _environment_register( void )
    {
        mainWindowFactory.Construct();

        // Initialize all sub-modules.
        InitializeRWFileSystemWrap();
        InitializeTaskCompletionWindowEnv();
        InitializeSerializationStorageEnv();
        InitializeMainWindowSerializationBlock();
        InitializeMainWindowThemeSupport();
        InitializeMainWindowMenu();
        InitializeMainWindowRenderWarePlugins();
        InitializeMagicLanguages();
        InitializeCustomFormatsEnvironment();
        InitializeHelperRuntime();
        InitializeMainWindowHelpEnv();
        InitializeTextureAddDialogEnv();
        InitializeExportAllWindowSerialization();
        InitializeMassconvToolEnvironment();
        InitializeMassExportToolEnvironment();
        InitializeMassBuildEnvironment();
        InitializeMainWindowStatusBarAnimations();
        InitializeGUISerialization();
        InitializeStreamCompressionEnvironment();
    }

    inline ~_environment_register( void )
    {
        // Shutdown all sub-modules.
        ShutdownStreamCompressionEnvironment();
        ShutdownGUISerialization();
        ShutdownMainWindowStatusBarAnimations();
        ShutdownMassBuildEnvironment();
        ShutdownMassExportToolEnvironment();
        ShutdownMassconvToolEnvironment();
        ShutdownExportAllWindowSerialization();
        ShutdownTextureAddDialogEnv();
        ShutdownMainWindowHelpEnv();
        ShutdownHelperRuntime();
        ShutdownCustomFormatsEnvironment();
        ShutdownMagicLanguages();
        ShutdownMainWindowRenderWarePlugins();
        ShutdownMainWindowMenu();
        ShutdownMainWindowThemeSupport();
        ShutdownMainWindowSerializationBlock();
        ShutdownSerializationStorageEnv();
        ShutdownTaskCompletionWindowEnv();
        ShutdownRWFileSystemWrap();

        mainWindowFactory.Destroy();
    }
};

static void important_message( const char *msg, const char *title )
{
#ifdef _WIN32
    MessageBoxA( nullptr, msg, title, MB_OK );
#elif defined(__linux__)
    printf( "[%s]: %s\n", title, msg );
#endif //CROSS PLATFORM CODE
}

static void important_message_w( const wchar_t *msg, const wchar_t *title )
{
#ifdef _WIN32
    MessageBoxW( nullptr, msg, title, MB_OK );
#elif defined(__linux__)
    printf( "[%ls]: %ls\n", title, msg );
#endif
}

// Global app-root-only system file translator.
CFileTranslator *sysAppRoot = nullptr;

struct init_exception
{
    inline init_exception( const char *msg ) noexcept
    {
        this->msg = msg;
    }

    inline const char* get_msg( void ) const noexcept
    {
        return this->msg;
    }

private:
    const char *msg;
};

int main(int argc, char *argv[])
{
    // Boot up all sub-modules.
    _environment_register _register;

    int iRet = -1;

    try
    {
        // Initialize the RenderWare engine.
        rw::LibraryVersion engineVersion;

        // This engine version is the default version we create resources in.
        // Resources can change their version at any time, so we do not have to change this.
        engineVersion.rwLibMajor = 3;
        engineVersion.rwLibMinor = 6;
        engineVersion.rwRevMajor = 0;
        engineVersion.rwRevMinor = 3;

        rw::InterfacePtr rwEngine = rw::CreateEngine( engineVersion );

        if ( rwEngine.is_good() == false )
        {
            throw std::exception(); // "failed to initialize the RenderWare engine"
        }

        // Set some typical engine properties.
        // The MainWindow will override these.
        rwEngine->SetIgnoreSerializationBlockRegions( true );
        rwEngine->SetBlockAcquisitionMode( rw::eBlockAcquisitionMode::FIND );
        rwEngine->SetIgnoreSecureWarnings( false );

        rwEngine->SetWarningLevel( 3 );

        rwEngine->SetCompatTransformNativeImaging( true );
        rwEngine->SetPreferPackedSampleExport( true );

        rwEngine->SetDXTRuntime( rw::DXTRUNTIME_SQUISH );
        rwEngine->SetPaletteRuntime( rw::PALRUNTIME_PNGQUANT );

        // Give RenderWare some info about us!
        rw::softwareMetaInfo metaInfo;
        metaInfo.applicationName = "Magic.TXD";
        metaInfo.applicationVersion = MTXD_VERSION_STRING;
        metaInfo.description = "by DK22Pac and The_GTA (https://osdn.net/projects/magic-txd/)";

        rwEngine->SetApplicationInfo( metaInfo );

        // removed library path stuff, because we statically link.

        MagicTXDApplication a(argc, argv);
        {
            QString styleSheet = styles::get(a.applicationDirPath(), "resources/dark.shell");

            if ( styleSheet.isEmpty() )
            {
                important_message(
                    "Failed to load stylesheet resource \"resources/dark.shell\".\n" \
                    "Please verify whether you have installed Magic.TXD correctly!",
                    "Error"
                );

                // Even without stylesheet, we allow launching Magic.TXD.
                // The user gets what he asked for.
            }
            else
            {
                a.setStyleSheet(styleSheet);
            }
        }
        mainWindowConstructor wnd_constr(a.applicationDirPath(), rwEngine, fileSystem);

        rw::RwStaticMemAllocator memAlloc;

        MainWindow *w = mainWindowFactory.get().ConstructTemplate(memAlloc, wnd_constr);

        if ( w == nullptr )
        {
            throw init_exception( "Failed to construct the Qt MainWindow" );
        }

        try
        {
            w->setWindowIcon(QIcon(w->makeAppPath("resources/icons/stars.png")));
            w->show();

            w->launchDetails();

            QApplication::processEvents();

            QStringList appargs = a.arguments();

            if (appargs.size() >= 2) {
                QString txdFileToBeOpened = appargs.at(1);
                if (!txdFileToBeOpened.isEmpty()) {
                    w->openTxdFile(txdFileToBeOpened);

                    w->adjustDimensionsByViewport();
                }
            }

            // Try to catch some known C++ exceptions and display things for them.
            try
            {
                iRet = a.exec();
            }
            catch( rw::RwException& except )
            {
                auto errMsg = L"uncaught RenderWare exception: " + rw::DescribeException( rwEngine, except );

                important_message_w(
                    errMsg.GetConstString(),
                    L"Uncaught C++ Exception"
                );

                // Continue execution.
                iRet = -1;
            }
            catch( std::exception& except )
            {
                std::string errMsg = std::string( "uncaught C++ STL exception: " ) + except.what();

                important_message(
                    errMsg.c_str(),
                    "Uncaught C++ Exception"
                );

                // Continue execution.
                iRet = -2;
            }
        }
        catch( ... )
        {
            mainWindowFactory.get().Destroy( memAlloc, w );

            throw;
        }

        mainWindowFactory.get().Destroy(memAlloc, w);
    }
    catch( init_exception& except )
    {
        important_message(
            except.get_msg(),
            "Initialization Exception"
        );

        // Just continue.
    }
    catch( std::exception& except )
    {
        std::string errMsg = std::string( "uncaught C++ STL error during init: " ) + except.what();

        important_message(
            errMsg.c_str(),
            "Uncaught C++ Exception"
        );

        // Just continue.
    }
    catch( ... )
    {
#ifdef _DEBUG
        auto cur_except = std::current_exception();
#endif //_DEBUG

#ifdef _DEBUG
        // We want to be notified about any uncaught exceptions in DEBUG mode.
        throw;
#else
        important_message(
            "Magic.TXD has encountered an unknown C++ exception and was forced to close. Please report this to the developers with appropriate steps to reproduce.",
            "Uncaught C++ Exception"
        );

        // Continue for safe quit.

#endif //_DEBUG
    }

    return iRet;
}
