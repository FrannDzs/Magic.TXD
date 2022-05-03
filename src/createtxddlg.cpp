/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/createtxddlg.cpp
*  PURPOSE:     Implementation of the New dialog.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include "qtutils.h"
#include <regex>
#include "testmessage.h"

#include "createtxddlg.h"

static inline const QRegularExpression& get_forb_path_chars( void )
{
    // IMPORTANT: always put statics into functions.
    static const QRegularExpression forbPathChars("[/:?\"<>|\\[\\]\\\\]");

    return forbPathChars;
}

CreateTxdDialog::CreateTxdDialog(MainWindow *mainWnd) : QDialog(mainWnd), versionGUI( mainWnd, this )
{
    this->mainWnd = mainWnd;

    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setWindowModality(Qt::WindowModality::WindowModal);
    this->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // Create our GUI interface
    MagicLayout<QVBoxLayout> layout(this);

    QHBoxLayout *nameLayout = new QHBoxLayout;
    QLabel *nameLabel = CreateLabelL("New.Name");
    nameLabel->setObjectName("label25px");
    MagicLineEdit *nameEdit = new MagicLineEdit();

    connect(nameEdit, &QLineEdit::textChanged, this, &CreateTxdDialog::OnUpdateTxdName);

    nameEdit->setFixedWidth(300);
    nameLayout->addWidget(nameLabel);
    nameLayout->addWidget(nameEdit);
    this->txdName = nameEdit;

    layout.top->addLayout(nameLayout);
    layout.top->addSpacing(8);

    layout.top->addLayout( this->versionGUI.GetVersionRootLayout() );

    QPushButton *buttonAccept = CreateButtonL("New.Create");
    QPushButton *buttonCancel = CreateButtonL("New.Cancel");

    this->applyButton = buttonAccept;

    connect(buttonAccept, &QPushButton::clicked, this, &CreateTxdDialog::OnRequestAccept);
    connect(buttonCancel, &QPushButton::clicked, this, &CreateTxdDialog::OnRequestCancel);

    layout.bottom->addWidget(buttonAccept);
    layout.bottom->addWidget(buttonCancel);

    // Initiate the ready dialog.
    this->versionGUI.InitializeVersionSelect();

    this->UpdateAccessibility();

    RegisterTextLocalizationItem( this );
}

CreateTxdDialog::~CreateTxdDialog( void )
{
    UnregisterTextLocalizationItem( this );
}

void CreateTxdDialog::updateContent( MainWindow *mainWnd )
{
    this->setWindowTitle(MAGIC_TEXT("New.Desc"));
}

void CreateTxdDialog::UpdateAccessibility(void)
{
    rw::LibraryVersion libVer;

    bool hasValidVersion = this->versionGUI.GetSelectedVersion(libVer);

    // Alright, set enabled-ness based on valid version.
    if(!hasValidVersion || this->txdName->text().isEmpty() || this->txdName->text().contains(get_forb_path_chars()))
        this->applyButton->setDisabled(true);
    else
        this->applyButton->setDisabled(false);
}

void CreateTxdDialog::OnRequestAccept(bool clicked)
{
    // Just create an empty TXD.
    rw::ObjectPtr <rw::TexDictionary> newTXD;

    MainWindow *mainWnd = this->mainWnd;

    rw::Interface *rwEngine = mainWnd->rwEngine;

    try
    {
        newTXD = rw::CreateTexDictionary(rwEngine);
    }
    catch (rw::RwException& except)
    {
        mainWnd->txdLog->showError(MAGIC_TEXT("General.TXDFail").arg(wide_to_qt(rw::DescribeException( rwEngine, except))));

        // We failed.
        return;
    }

    if (newTXD.is_good() == false)
    {
        mainWnd->txdLog->showError(MAGIC_TEXT("General.TXDFailUnk"));

        return;
    }

    QString txdName = this->txdName->text();

    rw::LibraryVersion libVer;
    this->versionGUI.GetSelectedVersion(libVer);

    QString currentPlatform = this->versionGUI.GetSelectedEnginePlatform();

    mainWnd->actionSystem.LaunchPostOnlyActionL( eMagicTaskType::NEW_TXD, "New.ActionTitle",
        [mainWnd, newTXD = std::move(newTXD), currentPlatform = std::move(currentPlatform), libVer = std::move(libVer), txdName = std::move(txdName)]( void ) mutable
    {
        mainWnd->setCurrentTXD( newTXD.detach() );

        mainWnd->clearCurrentFilePath();

        // Set the version of the entire TXD.
        mainWnd->newTxdName = std::move( txdName );

        newTXD->SetEngineVersion( libVer );

        mainWnd->SetRecommendedPlatform( std::move( currentPlatform ) );

        // Update the MainWindow stuff.
        mainWnd->updateWindowTitle();

        // Since the version has changed, the friendly icons should have changed.
        mainWnd->updateFriendlyIcons();
    });

    this->close();
}

void CreateTxdDialog::OnRequestCancel(bool clicked)
{
    this->close();
}

void CreateTxdDialog::OnUpdateTxdName(const QString& newText)
{
    this->UpdateAccessibility();
}
