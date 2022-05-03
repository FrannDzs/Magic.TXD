/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/mainwindow.h
*  PURPOSE:     Header of the main window implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// DEBUG DEFINES.
#ifdef _DEBUG
#define _DEBUG_HELPER_TEXT
#endif

#include <QtCore/qconfig.h>

#include <QtCore/QEvent>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QListWidget>
#include <QtCore/QFileInfo>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QAction>
#include <QtWidgets/QMessageBox>

#include <renderware.h>

#include <sdk/MemoryUtils.h>
#include <sdk/PluginHelpers.h>

#include <CFileSystemInterface.h>
#include <CFileSystem.h>

#define NUMELMS(x)      ( sizeof(x) / sizeof(*x) )

#include "defs.h"

class MainWindow;

#include "qtfilesystem.h"
#include "embedded_resources.h"

#include "versionsets.h"
#include "textureViewport.h"

#include "mainwindow.events.h"
#include "mainwindow.actions.h"
#include "mainwindow.asynclog.h"
#include "mainwindow.parallel.h"

#include "expensivetasks.h"

struct SystemEventHandlerWidget abstract
{
    ~SystemEventHandlerWidget( void );

    virtual void beginSystemEvent( QEvent *evt ) = 0;
    virtual void endSystemEvent( QEvent *evt ) = 0;
};

// Global conversion from QString to c-str and other way round.
inline std::string qt_to_ansi( const QString& str )
{
    QByteArray charBuf = str.toLatin1();

    return std::string( charBuf.data(), charBuf.size() );
}

inline rw::rwStaticString <char> qt_to_ansirw( const QString& str )
{
    QByteArray charBuf = str.toLatin1();

    return rw::rwStaticString <char> ( charBuf.data(), charBuf.size() );
}

inline rw::rwStaticString <wchar_t> qt_to_widerw( const QString& str )
{
    QByteArray charBuf = str.toUtf8();

    return CharacterUtil::ConvertStringsLength <char8_t, wchar_t, rw::RwStaticMemAllocator> ( (const char8_t*)charBuf.data(), charBuf.size() );
}

inline filePath qt_to_filePath( const QString& str )
{
    QByteArray charBuf = str.toUtf8();

    return filePath( (const char8_t*)charBuf.data(), charBuf.size() );
}

inline QString ansi_to_qt( const std::string& str )
{
    return QString::fromLatin1( str.c_str(), str.size() );
}

template <typename... stringParams>
inline QString ansi_to_qt( const eir::String <char, stringParams...>& str )
{
    return QString::fromLatin1( str.GetConstString(), str.GetLength() );
}

template <typename allocatorType, typename exceptMan>
inline QString wide_to_qt( const eir::String <wchar_t, allocatorType, exceptMan>& str )
{
    eir::String <char8_t, allocatorType, exceptMan> utf8String = CharacterUtil::ConvertStrings <wchar_t, char8_t, allocatorType> ( str, str.GetAllocData() );

    return QString::fromUtf8( (const char*)utf8String.GetConstString(), utf8String.GetLength() );
}

inline QString filePath_to_qt( const filePath& path )
{
    auto widePath = path.convert_unicode <FileSysCommonAllocator> ();

    return wide_to_qt( widePath );
}

// The editor may have items that depend on a certain theme.
// We must be aware of theme changes!
struct magicThemeAwareItem abstract
{
    // Called when the theme has changed.
    virtual void updateTheme( MainWindow *mainWnd ) = 0;
};

#include "texinfoitem.h"
#include "txdlog.h"
#include "rwfswrap.h"
#include "guiserialization.h"
#include "aboutdialog.h"
#include "streamcompress.h"
#include "helperruntime.h"

#include "MagicExport.h"

// Global app-root system translator in jail-mode.
extern CFileTranslator *sysAppRoot;

#define _FEATURES_NOT_IN_CURRENT_RELEASE

class TexInfoWidget;

class MainWindow : public QMainWindow, public magicTextLocalizationItem, public magicThemeAwareItem
{
    friend class TexAddDialog;
    friend class RwVersionDialog;
    friend class TexNameWindow;
    friend class RenderPropWindow;
    friend class TexResizeWindow;
    //friend class PlatformSelWindow;
    friend class ExportAllWindow;
    friend class AboutDialog;
    friend class OptionsDialog;
    friend class mainWindowSerializationEnv;
    friend class CreateTxdDialog;

    // Some unknown but defined struct.
    // ODR is very important.
    friend struct mainWindowMenuEnv;
    friend struct statusBarAnimationMembers;

public:
    MainWindow(QString appPath, rw::Interface *rwEngine, CFileSystem *fsHandle, QWidget *parent = nullptr);
    ~MainWindow();

protected:
    unsigned int updateMenuContent( void );

public:
    void updateContent( MainWindow *mainWnd ) override;

private:
    void UpdateExportAccessibility(void);
    void UpdateAccessibility(void);

public:
    bool openTxdFile(QString fileName, bool silent = false);
    void setCurrentTXD(rw::TexDictionary *txdObj);
    rw::TexDictionary* getCurrentTXD(void)              { return this->currentTXD; }
    void updateTextureList(bool selectLastItemInList);

    void updateFriendlyIcons();

    void adjustDimensionsByViewport();

    void updateWindowTitle(void);
    void updateTextureMetaInfo( rw::TextureBase *theTexture );
    void updateAllTextureMetaInfo(void);

    void updateTextureView(void);

    MagicActionSystem::ActionToken saveCurrentTXDAt(
        QString location,
        std::function <void ( void )> success_cb,
        std::function <void ( void )> fail_cb
    );

    void clearViewImage(void);

    rw::Interface* GetEngine(void) { return this->rwEngine; }

    QString GetCurrentPlatform();

    void SetRecommendedPlatform(QString platform);

    const char* GetTXDPlatform(rw::TexDictionary *txd);

    void launchDetails( void );

    // Asynchronous logging.
    void asyncLog( QString logMsg, eLogMessageType msgType = LOGMSG_INFO );

    void startStatusLoadingAnimation( void );
    void stopStatusLoadingAnimation( void );
    void progressStatusLoadingAnimation( void );

    void startMainPreviewLoadingAnimation( void );
    void stopMainPreviewLoadingAnimation( void );

    // Theme registration API.
    void RegisterThemeItem( magicThemeAwareItem *item );
    void UnregisterThemeItem( magicThemeAwareItem *item ) noexcept;

    // Texture to TexInfoWidget link.
    void setTextureTexInfoLink( rw::TextureBase *texture, TexInfoWidget *widget );
    TexInfoWidget* getTextureTexInfoLink( rw::TextureBase *texture );

private:
    void DefaultTextureAddAndPrepare( rw::TextureBase *rwtex, const char *name, const char *maskName );

    inline void setCurrentFilePath(const QString& newPath)
    {
        this->openedTXDFileInfo = QFileInfo(newPath);
        this->hasOpenedTXDFileInfo = true;

        this->updateWindowTitle();
    }

    inline void clearCurrentFilePath(void)
    {
        this->hasOpenedTXDFileInfo = false;

        this->updateWindowTitle();
    }

    void UpdateTheme( void );

public:
    void NotifyChange( void );

private:
    void ClearModifiedState( void );

    typedef std::function <void (void)> modifiedEndCallback_t;

public:
    void ModifiedStateBarrier( bool blocking, modifiedEndCallback_t cb );

private:
    MagicActionSystem::ActionToken performSaveTXD(
        std::function <void ( void )> success_cb,
        std::function <void ( void )> fail_cb
    );
    MagicActionSystem::ActionToken performSaveAsTXD(
        std::function <void ( void )> success_cb,
        std::function <void ( void )> fail_cb
    );

    // *** EVENTS ***
    void closeEvent( QCloseEvent *evt ) override;

    // Drag and drop support.
    void dragEnterEvent( QDragEnterEvent *evt ) override;
    void dragLeaveEvent( QDragLeaveEvent *evt ) override;
    void dropEvent( QDropEvent *evt ) override;

    // Asynchronous logging.
    void asyncLogEvent( MGTXDAsyncLogEvent *evt );

    // Other events.
    void customEvent( QEvent *evt ) override;

private:
    QString requestValidImagePath( const QString *imageName = nullptr );

    void spawnTextureAddDialog( QString imgPath );

public slots:
    void onSetupTxdVersion(bool checked);

    void onTextureItemChanged(QListWidgetItem *texInfoItem, QListWidgetItem *prevTexInfoItem);

private:
    class rwPublicWarningDispatcher : public rw::WarningManagerInterface
    {
    public:
        inline rwPublicWarningDispatcher(MainWindow *theWindow)
        {
            this->mainWnd = theWindow;
        }

        void OnWarning(rw::rwStaticString <wchar_t>&& msg) override
        {
            this->mainWnd->asyncLog(wide_to_qt(msg), LOGMSG_WARNING);
        }

    private:
        MainWindow *mainWnd;
    };

    rwPublicWarningDispatcher rwWarnMan;

    rw::Interface *rwEngine;
    rw::TexDictionary *currentTXD;

    TexInfoWidget *currentSelectedTexture;

    QFileInfo openedTXDFileInfo;
    bool hasOpenedTXDFileInfo;

    // We currently have a very primitive change-tracking system.
    // If we made any action that could have modified the TXD, we remember that.
    // Then if the user wants to discard the TXD, we ask if he wants to save it first.
    bool wasTXDModified;

    QString newTxdName;

    QString recommendedTxdPlatform;

    QListWidget *textureListWidget;

    TexViewportWidget *imageView; // we handle full 2d-viewport as a scroll-area
    MagicParallelTasks::TaskToken tasktoken_updatePreview;

    QLabel *txdNameLabel;

    QPushButton *rwVersionButton;

    QMovie *starsMovie;

    QSplitter *mainSplitter;

    bool drawMipmapLayers;

public:
    QHBoxLayout *contentVerticalLayout;

private:
    QHBoxLayout *friendlyIconRow;
    QLabel *friendlyIconGame;
    QWidget *friendlyIconSeparator;
    QLabel *friendlyIconPlatform;

    bool bShowFriendlyIcons;

public:
    // Action system for multi-threading.
    struct EditorActionSystem : public MagicActionSystem
    {
        EditorActionSystem( MainWindow *mainWnd );
        ~EditorActionSystem( void );

        // Synced with the execution thread.
        void OnStartAction( void ) override;
        void OnStopAction( void ) override;
        void OnUpdateStatusMessage( const rw::rwStaticString <wchar_t>& statusString ) override;

        // Called on the main event dispatcher thread.
        void OnAsyncStartAction( const MagicActionDesc& desc ) override;
        void OnAsyncStopAction( const MagicActionDesc& desc ) override;
        void OnAsyncTaskProcessingBegin( void ) override;
        void OnAsyncTaskProcessingEnd( void ) override;

        struct rwAsyncWarningManager : public rw::WarningManagerInterface
        {
            void OnWarning( rw::rwStaticString <wchar_t>&& msg ) override;
        };
        rwAsyncWarningManager asyncWarn;

        MainWindow *wnd;
    };
    EditorActionSystem actionSystem;
    // TODO: actually use EditorActionSystem to spawn parallel tasks to GUI activity.

    struct EditorExpensiveTasks : public ExpensiveTasks
    {
    protected:
        void pushLogMessage( QString msg, eLogMessageType msgType ) override;
        MainWindow* getMainWindow( void ) override;
    };
    EditorExpensiveTasks expensiveTasks;

    // Sheduler for parallel activity.
    MagicParallelTasks taskSystem;


    // REMEMBER TO DELETE EVERY WIDGET THAT DEPENDS ON MAINWINDOW INSIDE OF MAINWINDOW DESTRUCTOR.
    // OTHERWISE THE EDITOR COULD CRASH.

    TxdLog *txdLog; // log management class
    class RwVersionDialog *verDlg; // txd version setup class
    class TexNameWindow *texNameDlg; // dialog to change texture name
    class RenderPropWindow *renderPropDlg; // change a texture's wrapping or filtering
    class TexResizeWindow *resizeDlg; // change raster dimensions
    //class PlatformSelWindow *platformDlg; // set TXD platform
    class AboutDialog *aboutDlg;  // about us. :-)
    QDialog *optionsDlg;    // many options.

public:
    QString m_appPath;
    QString m_appPathForStyleSheet;

    // Use this if you need to get a path relatively to app directory
    QString makeAppPath(QString subPath);
    QString makeThemePath(QString subPath);
    void setActiveTheme( const wchar_t *themeName );
    filePath getActiveTheme( void ) const;

protected:
    void updateTheme( MainWindow *mainWnd ) override;

public:
    RwVersionSets versionSets;  // we need access to this in utilities.

    // NOTE: there are multiple ways to get absolute path to app directory coded in this editor!

public:
    CFileSystem *fileSystem;

    // Serialization properties.
    QString lastTXDOpenDir;     // maybe.
    QString lastTXDSaveDir;
    QString lastImageFileOpenDir;

    bool addImageGenMipmaps;
    bool lockDownTXDPlatform;
    bool adjustTextureChunksOnImport;
    bool texaddViewportFill;
    bool texaddViewportScaled;
    bool texaddViewportBackground;

    bool isLaunchedForTheFirstTime;

    // Options.
    bool showLogOnWarning;
    bool showGameIcon;

    QString lastLanguageFileName;

    // ExportAllWindow
    rw::rwStaticString <char> lastUsedAllExportFormat;
    rw::rwStaticString <wchar_t> lastAllExportTarget;

    // Temporary folder for critical tasks.
    CFileTranslator *tempFileRoot;
};

#include "mainwindow.loadinganim.h"

typedef StaticPluginClassFactory <MainWindow, rw::RwStaticMemAllocator> mainWindowFactory_t;

extern optional_struct_space <mainWindowFactory_t> mainWindowFactory;

#endif // MAINWINDOW_H
