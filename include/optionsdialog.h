/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/optionsdialog.h
*  PURPOSE:     Header of the editor configuration dialog.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QComboBox>
#include <QtWidgets/qtabwidget.h>

struct OptionsDialog : public QDialog, public magicTextLocalizationItem
{
    OptionsDialog( MainWindow *mainWnd );
    ~OptionsDialog( void );

    void updateContent( MainWindow *mainWnd ) override;

public slots:
    void OnRequestApply( bool checked );
    void OnRequestCancel( bool checked );
    void OnChangeSelectedLanguage(int newIndex);

private:
    void serialize( void );

    MainWindow *mainWnd;

    QTabWidget *optTabs;
    int mainTabIndex;
    int rwTabIndex;

    // Main tab.
    QCheckBox *optionShowLogOnWarning;
    QCheckBox *optionShowGameIcon;

    QComboBox *languageBox;
    QLabel *languageAuthorLabel;

    // Advanced tab.
    QCheckBox *optionDeserWithoutBlocklengths;
    QComboBox *selectBlockAcquisitionMode;
    QComboBox *selectWarningLevel;
};
