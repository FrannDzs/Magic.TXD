/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderpropwindow.h
*  PURPOSE:     Header of the texture render properties window.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <list>

#include <QtWidgets/QDialog>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>

struct RenderPropWindow : public QDialog, public magicTextLocalizationItem
{
private:
    QComboBox* createFilterBox( void ) const;

public:
    RenderPropWindow( MainWindow *mainWnd, rw::ObjectPtr <rw::TextureBase> toBeEdited );
    ~RenderPropWindow( void );

    void updateContent( MainWindow *mainWnd ) override;

public slots:
    void OnRequestSet( bool checked );
    void OnRequestCancel( bool checked );

    void OnAnyPropertyChange( const QString& newValue )
    {
        this->UpdateAccessibility();
    }

private:
    void UpdateAccessibility( void );

    MainWindow *mainWnd;
    rw::ObjectPtr <rw::TextureBase> toBeEdited;

    QPushButton *buttonSet;
    QComboBox *filterComboBox;
    QComboBox *uaddrComboBox;
    QComboBox *vaddrComboBox;
};
