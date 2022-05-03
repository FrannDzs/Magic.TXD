/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/versionshared.h
*  PURPOSE:     Include file shared between dialogs that want to display common version GUI components.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <regex>

#include <QtCore/QCoreApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/qlineedit.h>

#include <sdk/NumericFormat.h>

#include "qtutils.h"

static inline rw::rwStaticString <char> rwVersionToString( const rw::LibraryVersion& version )
{
    return
        eir::to_string <char, rw::RwStaticMemAllocator> (version.rwLibMajor) + "." +
        eir::to_string <char, rw::RwStaticMemAllocator> (version.rwLibMinor) + "." +
        eir::to_string <char, rw::RwStaticMemAllocator> (version.rwRevMajor) + "." +
        eir::to_string <char, rw::RwStaticMemAllocator> (version.rwRevMinor);
}

struct VersionSetSelection abstract : public QObject
{
    inline VersionSetSelection( MainWindow *mainWnd )
    {
        this->mainWnd = mainWnd;

        /************* Set ****************/
        QHBoxLayout *selectGameLayout = new QHBoxLayout;
        QLabel *gameLabel = CreateLabelL( "Main.SetupTV.Set" );
        gameLabel->setObjectName("label25px");
        QComboBox *gameComboBox = new QComboBox;
        gameComboBox->setFixedWidth(300);
        gameComboBox->addItem(getLanguageItemByKey("Main.SetupTV.Custom"));   /// HAXXXXXXX
        {
            int numVersionSets = mainWnd->versionSets.sets.size();
            for (int i = 0; i < numVersionSets; i++)
                gameComboBox->addItem(mainWnd->versionSets.sets[i].name);
        }
        this->gameSelectBox = gameComboBox;

        QObject::connect( gameComboBox, static_cast<void (QComboBox::*)(int index)>(&QComboBox::currentIndexChanged), this, &VersionSetSelection::OnChangeSelectedGame );

        selectGameLayout->addWidget(gameLabel);
        selectGameLayout->addWidget(gameComboBox);

        /************* Platform ****************/
        QHBoxLayout *selectPlatformLayout = new QHBoxLayout;
        QLabel *platLabel = CreateLabelL("Main.SetupTV.Plat");
        platLabel->setObjectName("label25px");
        QComboBox *platComboBox = new QComboBox;
        platComboBox->setFixedWidth(300);
        this->platSelectBox = platComboBox;

        QObject::connect(platComboBox, static_cast<void (QComboBox::*)(int index)>(&QComboBox::currentIndexChanged), this, &VersionSetSelection::OnChangeSelectedPlatform);

        selectPlatformLayout->addWidget(platLabel);
        selectPlatformLayout->addWidget(platComboBox);

        /************* Data type ****************/
        QHBoxLayout *selectDataTypeLayout = new QHBoxLayout;
        QLabel *dataTypeLabel = CreateLabelL( "Main.SetupTV.Data" );
        dataTypeLabel->setObjectName("label25px");
        QComboBox *dataTypeComboBox = new QComboBox;
        dataTypeComboBox->setFixedWidth(300);
        this->dataTypeSelectBox = dataTypeComboBox;

        selectDataTypeLayout->addWidget(dataTypeLabel);
        selectDataTypeLayout->addWidget(dataTypeComboBox);

        QHBoxLayout *versionLayout = new QHBoxLayout;
        QLabel *versionLabel = CreateFixedWidthLabelL( "Main.SetupTV.Version", 25 );
        versionLabel->setObjectName("label25px");

        QHBoxLayout *versionNumbersLayout = new QHBoxLayout;
        MagicLineEdit *versionLine1 = new MagicLineEdit;

        versionLine1->setInputMask("0.00.00.00");
        versionLine1->setFixedWidth(80);

        versionNumbersLayout->addWidget(versionLine1);

        versionNumbersLayout->setMargin(0);

        this->versionLineEdit = versionLine1;

        QObject::connect( versionLine1, &QLineEdit::textChanged, this, &VersionSetSelection::OnChangeVersion );

        QLabel *buildLabel = CreateFixedWidthLabelL( "Main.SetupTV.Build", 25 );
        buildLabel->setObjectName("label25px");
        MagicLineEdit *buildLine = new MagicLineEdit;
        buildLine->setInputMask(">HHHH");
        buildLine->clear();
        buildLine->setFixedWidth(60);

        this->buildLineEdit = buildLine;

        versionLayout->addWidget(versionLabel);
        versionLayout->addLayout(versionNumbersLayout);
        versionLayout->addWidget(buildLabel);
        versionLayout->addWidget(buildLine);

        QVBoxLayout *rootLayout = new QVBoxLayout();

        versionLayout->setAlignment(Qt::AlignRight);

        rootLayout->addLayout(selectGameLayout);
        rootLayout->addLayout(selectPlatformLayout);
        rootLayout->addLayout(selectDataTypeLayout);
        rootLayout->addSpacing(8);
        rootLayout->addLayout(versionLayout);

        this->rootLayout = rootLayout;
    }

    inline ~VersionSetSelection( void )
    {
        // WARNING: we destroy, so the connections MUST NOT FIRE ANY EVENTS TO US ANYMORE.
    }

    inline QLayout* GetVersionRootLayout( void )
    {
        return this->rootLayout;
    }

    bool GetSelectedVersion( rw::LibraryVersion& verOut ) const
    {
        QString currentVersionString = this->versionLineEdit->text();

        std::string ansiCurrentVersionString = qt_to_ansi( currentVersionString );

        rw::LibraryVersion theVersion;

        bool hasValidVersion = false;

        // Verify whether our version is valid while creating our local version struct.
        unsigned int rwLibMajor, rwLibMinor, rwRevMajor, rwRevMinor;
        bool hasProperMatch = false;
        {
            std::regex ver_regex( "(\\d)\\.(\\d{1,2})\\.(\\d{1,2})\\.(\\d{1,2})" );

            std::smatch ver_match;

            std::regex_match( ansiCurrentVersionString, ver_match, ver_regex );

            if ( ver_match.size() == 5 )
            {
                try
                {
                    rwLibMajor = std::stoul( ver_match[ 1 ] );
                    rwLibMinor = std::stoul( ver_match[ 2 ] );
                    rwRevMajor = std::stoul( ver_match[ 3 ] );
                    rwRevMinor = std::stoul( ver_match[ 4 ] );

                    hasProperMatch = true;
                }
                catch( ... )
                {
                    // An error during conversion happened, so we dont have proper numbers.
                    hasProperMatch = false;

                    // Continue.
                }
            }
        }

        if ( hasProperMatch )
        {
            if ( ( rwLibMajor >= 3 && rwLibMajor <= 6 ) &&
                    ( rwLibMinor <= 15 ) &&
                    ( rwRevMajor <= 15 ) &&
                    ( rwRevMinor <= 63 ) )
            {
                theVersion.rwLibMajor = rwLibMajor;
                theVersion.rwLibMinor = rwLibMinor;
                theVersion.rwRevMajor = rwRevMajor;
                theVersion.rwRevMinor = rwRevMinor;

                hasValidVersion = true;
            }
        }

        if ( hasValidVersion )
        {
            // Also set the build number, if valid.
            QString buildNumber = this->buildLineEdit->text();

            std::string ansiBuildNumber = qt_to_ansi( buildNumber );

            unsigned int buildNum;

            int matchCount = sscanf( ansiBuildNumber.c_str(), "%x", &buildNum );

            if ( matchCount == 1 )
            {
                if ( buildNum <= 65535 )
                {
                    theVersion.buildNumber = buildNum;
                }
            }

            // Having an invalid build number does not mean that our version is invalid.
            // The build number is just candy anyway.
        }

        if ( hasValidVersion )
        {
            verOut = theVersion;
        }

        return hasValidVersion;
    }

    QString GetSelectedEnginePlatform( void ) const
    {
        return this->dataTypeSelectBox->currentText();
    }

    void InitializeVersionSelect( void )
    {
        this->OnChangeSelectedGame( 0 );
    }

    virtual void NotifyUpdate( void ) = 0;

private slots:
    void OnChangeVersion( const QString& newText )
    {
        this->UpdateAccessibility();

        this->NotifyUpdate();
    }

    void OnChangeSelectedGame( int newIndex )
    {
        if (newIndex >= 0)
        {
            rw::Interface *rwEngine = this->mainWnd->GetEngine();

            QComboBox *platSelectBox = this->platSelectBox;

            if (newIndex == 0) { // Custom
                platSelectBox->setCurrentIndex(-1);
                platSelectBox->setDisabled(true);

                QString lastDataTypeName = this->dataTypeSelectBox->currentText();

                QComboBox *dataTypeSelectBox = this->dataTypeSelectBox;

                dataTypeSelectBox->clear();

                int current_data_index = 0;

                for (int i = 1; i <= RwVersionSets::RWVS_DT_NUM_OF_TYPES; i++)
                {
                    const char *dataName = RwVersionSets::dataNameFromId( (RwVersionSets::eDataType)i );

                    // Only show the item if we actually support it.
                    if ( rw::IsNativeTexture( rwEngine, dataName ) )
                    {
                        dataTypeSelectBox->addItem(dataName);

                        if (lastDataTypeName == dataName)
                        {
                            dataTypeSelectBox->setCurrentIndex(current_data_index);
                        }

                        current_data_index++;
                    }
                }

                this->dataTypeSelectBox->setDisabled(false);
            }
            else
            {
                platSelectBox->clear();

                const RwVersionSets::Set& selectedSet = this->mainWnd->versionSets.sets[newIndex - 1];

                const size_t platformCount = selectedSet.availablePlatforms.size();

                for (size_t i = 0; i < platformCount; i++)
                {
                    const RwVersionSets::Set::Platform& curPlat = selectedSet.availablePlatforms[i];

                    this->platSelectBox->addItem(
                        RwVersionSets::platformNameFromId(
                            curPlat.platformType
                        )
                   );
                }

                this->platSelectBox->setDisabled( platformCount < 2 );

                //this->OnChangeSelecteedPlatform( 0 );
            }
        }

        this->UpdateAccessibility();

        this->NotifyUpdate();
    }

    void OnChangeSelectedPlatform( int newIndex )
    {
        if (newIndex >= 0)
        {
            rw::Interface *rwEngine = this->mainWnd->GetEngine();

            this->dataTypeSelectBox->clear();

            unsigned int set = this->gameSelectBox->currentIndex();

            const RwVersionSets::Set& versionSet = this->mainWnd->versionSets.sets[ set - 1 ];
            const RwVersionSets::Set::Platform& platformOfSet = versionSet.availablePlatforms[ newIndex ];

            const size_t dataTypeCount = platformOfSet.availableDataTypes.size();

            size_t nativeItemCount = 0;

            for (size_t i = 0; i < dataTypeCount; i++)
            {
                const char *nativeName =
                    RwVersionSets::dataNameFromId(platformOfSet.availableDataTypes[i]);

                // We must actually support the native texture to allow the user to target it.
                if ( rw::IsNativeTexture( rwEngine, nativeName ) )
                {
                    this->dataTypeSelectBox->addItem(
                        nativeName
                    );

                    nativeItemCount++;
                }
            }

            // If we have just a single item or none in the data type select, there is no point
            // in letting the user play around with it.
            this->dataTypeSelectBox->setDisabled( nativeItemCount < 2 );

            rw::rwStaticString <char> verString = rwVersionToString( platformOfSet.version );
            rw::rwStaticString <char> buildString;

            if (platformOfSet.version.buildNumber != 0xFFFF)
            {
                buildString = eir::to_string <char, rw::RwStaticMemAllocator> ( platformOfSet.version.buildNumber, 16 );
            }

            this->versionLineEdit->setText(ansi_to_qt(verString));
            this->buildLineEdit->setText(ansi_to_qt(buildString));
        }

        this->UpdateAccessibility();

        this->NotifyUpdate();
    }

    void UpdateAccessibility( void )
    {
        // Check whether we should even enable input.
        // This is only if the user selected "Custom".
        bool shouldAllowInput = ( this->gameSelectBox->currentIndex() == 0 );

        this->versionLineEdit->setDisabled( !shouldAllowInput );
        this->buildLineEdit->setDisabled( !shouldAllowInput );
    }

private:
    MainWindow *mainWnd;

public:
    QComboBox *gameSelectBox;
    QComboBox *platSelectBox;
    QComboBox *dataTypeSelectBox;

    MagicLineEdit *versionLineEdit;
    MagicLineEdit *buildLineEdit;

private:
    QLayout *rootLayout;
};
