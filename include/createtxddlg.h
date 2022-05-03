/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/createtxddlg.h
*  PURPOSE:     Header of the New dialog.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QDialog>

#include "versionshared.h"

class MainWindow;

class CreateTxdDialog : public QDialog, public magicTextLocalizationItem
{
public:
    CreateTxdDialog(MainWindow *MainWnd);
    ~CreateTxdDialog(void);

    void updateContent( MainWindow *mainWnd ) override;

    void UpdateAccessibility(void);

private:
    struct CreateTxdVersionSet : public VersionSetSelection
    {
        inline CreateTxdVersionSet( MainWindow *mainWnd, CreateTxdDialog *dialog ) : VersionSetSelection( mainWnd )
        {
            this->dialog = dialog;
        }

        void NotifyUpdate( void ) override
        {
            // Update the button state of the main dialog.
            dialog->UpdateAccessibility();
        }

    private:
        CreateTxdDialog *dialog;
    };

    CreateTxdVersionSet versionGUI;

public slots:
    void OnRequestAccept(bool clicked);
    void OnRequestCancel(bool clicked);
    void OnUpdateTxdName(const QString& newText);

    MainWindow *mainWnd;

    MagicLineEdit *txdName;
    QPushButton *applyButton;
};
