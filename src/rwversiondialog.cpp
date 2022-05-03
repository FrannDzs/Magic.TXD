/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwversiondialog.cpp
*  PURPOSE:     TXD version and platform setting dialog.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include "rwversiondialog.h"

RwVersionDialog::RwVersionDialog( MainWindow *mainWnd ) : QDialog( mainWnd ), versionGUI( mainWnd, this )
{
    setObjectName("background_1");
    setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );
    setAttribute( Qt::WA_DeleteOnClose );

    setWindowModality( Qt::WindowModal );

    this->mainWnd = mainWnd;

    MagicLayout<QVBoxLayout> layout(this);

    layout.top->addLayout( this->versionGUI.GetVersionRootLayout() );

    QPushButton *buttonAccept = CreateButtonL( "Main.SetupTV.Accept" );
    QPushButton *buttonCancel = CreateButtonL( "Main.SetupTV.Cancel" );

    this->applyButton = buttonAccept;

    connect( buttonAccept, &QPushButton::clicked, this, &RwVersionDialog::OnRequestAccept );
    connect( buttonCancel, &QPushButton::clicked, this, &RwVersionDialog::OnRequestCancel );

    layout.bottom->addWidget(buttonAccept);
    layout.bottom->addWidget(buttonCancel);

    // Initiate the ready dialog.
    this->versionGUI.InitializeVersionSelect();

    RegisterTextLocalizationItem( this );
}

RwVersionDialog::~RwVersionDialog( void )
{
    UnregisterTextLocalizationItem( this );

    // There can only be one version dialog.
    this->mainWnd->verDlg = nullptr;
}

void RwVersionDialog::updateContent( MainWindow *mainWnd )
{
    // Update localization items.
    this->setWindowTitle( MAGIC_TEXT( "Main.SetupTV.Desc" ) );
}

void RwVersionDialog::UpdateAccessibility( void )
{
    rw::LibraryVersion libVer;

    // Check whether we should allow setting this version.
    bool hasValidVersion = this->versionGUI.GetSelectedVersion( libVer );

    // Alright, set enabled-ness based on valid version.
    this->applyButton->setDisabled( !hasValidVersion );
}

void RwVersionDialog::OnRequestAccept( bool clicked )
{
    // Set the version and close.
    rw::LibraryVersion libVer;

    bool hasVersion = this->versionGUI.GetSelectedVersion( libVer );

    if ( !hasVersion )
        return;

    MainWindow *mainWnd = this->mainWnd;

    // Set the version of the entire TXD.
    // Also patch the platform if feasible.
    rw::ObjectPtr currentTXD = (rw::TexDictionary*)rw::AcquireObject( mainWnd->currentTXD );

    if ( currentTXD.is_good() )
    {
        QString previousPlatform = this->mainWnd->GetCurrentPlatform();
        QString currentPlatform = this->versionGUI.GetSelectedEnginePlatform();

        mainWnd->actionSystem.LaunchActionL(
            eMagicTaskType::SET_TXD_VERSION, "General.SetTXDVer",
            [mainWnd, previousPlatform = std::move(previousPlatform), currentPlatform = std::move(currentPlatform), currentTXD = std::move(currentTXD), libVer]
        {
            // todo: Maybe make SetEngineVersion set the version for all children objects?
            currentTXD->SetEngineVersion(libVer);

            bool hasChangedVersion = false;

            if (currentTXD->GetTextureCount() > 0)
            {
                for (rw::TexDictionary::texIter_t iter(currentTXD->GetTextureIterator()); !iter.IsEnd(); iter.Increment())
                {
                    rw::TextureBase *theTexture = iter.Resolve();

                    try
                    {
                        theTexture->SetEngineVersion(libVer);
                    }
                    catch( rw::RwException& except )
                    {
                        mainWnd->asyncLog(
                            MAGIC_TEXT("General.TexVersionFail")
                            .arg(ansi_to_qt( theTexture->GetName() ),
                                 wide_to_qt( rw::DescribeException( theTexture->GetEngine(), except ) ))
                        );
                    }

                    // Pretty naive, but in the context very okay.
                    hasChangedVersion = true;
                }
            }

            // If platform was changed
            bool hasChangedPlatform = false;

            if (previousPlatform != currentPlatform)
            {
                mainWnd->expensiveTasks.changeTXDPlatform(currentTXD, qt_to_ansirw( currentPlatform ));

                hasChangedPlatform = true;
            }

            if ( hasChangedVersion || hasChangedPlatform )
            {
                mainWnd->actionSystem.CurrentActionSetPostL(
                    [mainWnd, hasChangedPlatform, currentPlatform = std::move(currentPlatform), previousPlatform = std::move(previousPlatform)] ( void ) mutable
                {
                    if ( hasChangedPlatform )
                    {
                        mainWnd->SetRecommendedPlatform(currentPlatform);

                        // The user might want to be notified of the platform change.
                        mainWnd->asyncLog(
                            MAGIC_TEXT("General.TXDPlatChanged")
                            .arg(previousPlatform, currentPlatform)
                        );
                    }

                    // Update texture item info, because it may have changed.
                    mainWnd->updateAllTextureMetaInfo();

                    // The visuals of the texture _may_ have changed.
                    mainWnd->updateTextureView();

                    // Remember that we changed stuff.
                    mainWnd->NotifyChange();

                    // Update the MainWindow stuff.
                    mainWnd->updateWindowTitle();

                    // Since the version has changed, the friendly icons should have changed.
                    mainWnd->updateFriendlyIcons();
                });
            }

            // Done. :)
        });
    }

    this->close();
}

void RwVersionDialog::OnRequestCancel( bool clicked )
{
    this->close();
}

void RwVersionDialog::updateVersionConfig()
{
    MainWindow *mainWnd = this->mainWnd;

    // Try to find a set for current txd version
    bool setFound = false;

    if (rw::TexDictionary *currentTXD = mainWnd->getCurrentTXD())
    {
        rw::LibraryVersion version = currentTXD->GetEngineVersion();

        QString platformName = mainWnd->GetCurrentPlatform();

        if (!platformName.isEmpty())
        {
            RwVersionSets::eDataType platformDataTypeId = RwVersionSets::dataIdFromEnginePlatformName(platformName);

            if (platformDataTypeId != RwVersionSets::RWVS_DT_NOT_DEFINED)
            {
                int setIndex, platformIndex, dataTypeIndex;

                if (mainWnd->versionSets.matchSet(version, platformDataTypeId, setIndex, platformIndex, dataTypeIndex))
                {
                    this->versionGUI.gameSelectBox->setCurrentIndex(setIndex + 1);
                    this->versionGUI.platSelectBox->setCurrentIndex(platformIndex);
                    this->versionGUI.dataTypeSelectBox->setCurrentIndex(dataTypeIndex);

                    setFound = true;
                }
            }
        }
    }

    // If we could not find a correct set, we still try to display good settings.
    if (!setFound)
    {
        if (this->versionGUI.gameSelectBox->currentIndex() != 0)
        {
            this->versionGUI.gameSelectBox->setCurrentIndex(0);
        }
        else
        {
            this->versionGUI.InitializeVersionSelect();
        }

        if (rw::TexDictionary *currentTXD = mainWnd->getCurrentTXD())
        {
            // Deduce the best data type from the current platform of the TXD.
            {
                QString platformName = mainWnd->GetCurrentPlatform();

                if ( platformName.isEmpty() == false )
                {
                    this->versionGUI.dataTypeSelectBox->setCurrentText( platformName );
                }
            }

            // Fill out the custom version string
            {
                const rw::LibraryVersion& txdVersion = currentTXD->GetEngineVersion();

                rw::rwStaticString <char> verString = rwVersionToString( txdVersion );
                rw::rwStaticString <char> buildString;

                if (txdVersion.buildNumber != 0xFFFF)
                {
                    buildString = eir::to_string <char, rw::RwStaticMemAllocator> ( txdVersion.buildNumber, 16 );
                }

                this->versionGUI.versionLineEdit->setText(ansi_to_qt(verString));
                this->versionGUI.buildLineEdit->setText(ansi_to_qt(buildString));
            }
        }
    }
}
