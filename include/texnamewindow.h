/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/texnamewindow.h
*  PURPOSE:     Header of the texture naming window.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QDialog>
#include "qtutils.h"
#include "languages.h"

struct TexNameWindow : public QDialog, public magicTextLocalizationItem
{
    TexNameWindow( MainWindow *mainWnd, rw::ObjectPtr <rw::TextureBase> toBeRenamed );
    ~TexNameWindow( void );

    void updateContent( MainWindow *mainWnd ) override;

public slots:
    void OnUpdateTexName( const QString& newText )
    {
        this->UpdateAccessibility();
    }

    void OnRequestSet( bool clicked );
    void OnRequestCancel( bool clicked )
    {
        this->close();
    }

private:
    void UpdateAccessibility( void );

    MainWindow *mainWnd;
    rw::ObjectPtr <rw::TextureBase> toBeRenamed;

    MagicLineEdit *texNameEdit;

    QPushButton *buttonSet;
};
