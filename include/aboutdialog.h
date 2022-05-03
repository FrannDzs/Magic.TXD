/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/aboutdialog.h
*  PURPOSE:     Header of the About Us dialog.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QDialog>

class MainWindow;

struct AboutDialog : public QDialog, public magicTextLocalizationItem, public magicThemeAwareItem
{
    AboutDialog( MainWindow *mainWnd );
    ~AboutDialog( void );

    void updateContent( MainWindow *mainWnd ) override;

    void updateTheme( MainWindow *mainWnd ) override;

public slots:
    void OnRequestClose( bool checked );

private:
    MainWindow *mainWnd;

    QLabel *mainLogoLabel;
};
