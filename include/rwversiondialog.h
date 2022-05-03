/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/rwversiondialog.h
*  PURPOSE:     Header of the version and platform dialog for TXDs.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>

#include <regex>
#include <string>
#include <sstream>

#include <vector>

#include "qtutils.h"
#include "languages.h"

#include "versionshared.h"

class RwVersionDialog : public QDialog, public magicTextLocalizationItem
{
    MainWindow *mainWnd;

    QPushButton *applyButton;

private:
    struct RwVersionDialogSetSelection : public VersionSetSelection
    {
        inline RwVersionDialogSetSelection( MainWindow *mainWnd, RwVersionDialog *dialog ) : VersionSetSelection( mainWnd )
        {
            this->verDialog = dialog;
        }

        void NotifyUpdate( void ) override
        {
            // When changes are made to the configuration, we have to update the button in the main dialog.
            this->verDialog->UpdateAccessibility();
        }

    private:
        RwVersionDialog *verDialog;
    };

    RwVersionDialogSetSelection versionGUI;

private:
    void UpdateAccessibility( void );

public slots:
    void OnRequestAccept( bool clicked );
    void OnRequestCancel( bool clicked );

public:
    RwVersionDialog( MainWindow *mainWnd );
    ~RwVersionDialog( void );

    void updateContent( MainWindow *mainWnd ) override;

    void SelectGame(unsigned int gameId)
    {
        this->versionGUI.gameSelectBox->setCurrentIndex(gameId);
    }

    void updateVersionConfig();
};
