/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.theme.cpp
*  PURPOSE:     Editor theme system.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include <mainwindow.h>

#include <QtGui/QMovie>

#include "styles.h"

// TODO: implement dynamic theme support by searching for all available
// .shell files in the resources folder and listing them as available themes
// in the GUI.

struct themeSupportEnv
{
    inline void Initialize( MainWindow *mainWnd )
    {
        // Default is dark theme.
        this->themeCurrentName = "dark";

        // We are the first one to be registered.
        mainWnd->RegisterThemeItem( mainWnd );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        mainWnd->UnregisterThemeItem( mainWnd );
    }

    // Editor theme awareness.
    std::vector <magicThemeAwareItem*> themeItems;
    filePath themeCurrentName;
};

static optional_struct_space <PluginDependantStructRegister <themeSupportEnv, mainWindowFactory_t>> themeSupportEnvRegister;

QString MainWindow::makeThemePath(QString subPath)
{
    themeSupportEnv *themeEnv = themeSupportEnvRegister.get().GetPluginStruct( this );

    return
        m_appPath + "/resources/"
        + wide_to_qt( themeEnv->themeCurrentName.convert_unicode <rw::RwStaticMemAllocator> () ) + "/"
        + subPath;
}

void MainWindow::setActiveTheme( const wchar_t *themeName )
{
    themeSupportEnv *themeEnv = themeSupportEnvRegister.get().GetPluginStruct( this );

    if ( themeEnv->themeCurrentName == themeName )
        return;

    this->setStyleSheet(styles::get(this->m_appPath, QString( "resources/" ) + QString::fromWCharArray( themeName ) + ".shell"));

    themeEnv->themeCurrentName = themeName;

    this->UpdateTheme();
}

filePath MainWindow::getActiveTheme( void ) const
{
    const themeSupportEnv *themeEnv = themeSupportEnvRegister.get().GetConstPluginStruct( this );

    return themeEnv->themeCurrentName;
}

void MainWindow::updateTheme( MainWindow *mainWnd )
{
    this->starsMovie->stop();
    this->starsMovie->setFileName(makeThemePath("stars.gif"));
    this->starsMovie->start();
}

// Theme management.
void MainWindow::RegisterThemeItem( magicThemeAwareItem *item )
{
    themeSupportEnv *themeEnv = themeSupportEnvRegister.get().GetPluginStruct( this );

    // Register the item.
    themeEnv->themeItems.push_back( item );

    // Initialize the theme.
    item->updateTheme( this );
}

void MainWindow::UnregisterThemeItem( magicThemeAwareItem *item ) noexcept
{
    themeSupportEnv *themeEnv = themeSupportEnvRegister.get().GetPluginStruct( this );

    // Remove the item, if found.
    auto findItem = std::find( themeEnv->themeItems.begin(), themeEnv->themeItems.end(), item );

    if ( findItem != themeEnv->themeItems.end() )
    {
        themeEnv->themeItems.erase( findItem );
    }
}

void MainWindow::UpdateTheme( void )
{
    themeSupportEnv *themeEnv = themeSupportEnvRegister.get().GetPluginStruct( this );

    // Notify all items.
    for ( magicThemeAwareItem *item : themeEnv->themeItems )
    {
        item->updateTheme( this );
    }
}

// Module initialization and shutdown.
void InitializeMainWindowThemeSupport( void )
{
    themeSupportEnvRegister.Construct( mainWindowFactory );
}

void ShutdownMainWindowThemeSupport( void )
{
    themeSupportEnvRegister.Destroy();
}
