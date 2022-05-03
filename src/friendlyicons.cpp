/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/friendlyicons.cpp
*  PURPOSE:     Icons at the bottom right of the editor.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

// We know that certain data types are most likely to be combined with a certain platform.
static inline bool getRecommendedPlatformForDataType( RwVersionSets::eDataType dataType, RwVersionSets::ePlatformType& platformOut )
{
    switch( dataType )
    {
    case RwVersionSets::RWVS_DT_NOT_DEFINED:
        break;
    case RwVersionSets::RWVS_DT_D3D8:
    case RwVersionSets::RWVS_DT_D3D9:
        platformOut = RwVersionSets::RWVS_PL_PC;
        return true;
    case RwVersionSets::RWVS_DT_XBOX:
        platformOut = RwVersionSets::RWVS_PL_XBOX;
        return true;
    case RwVersionSets::RWVS_DT_PS2:
        platformOut = RwVersionSets::RWVS_PL_PS2;
        return true;
    case RwVersionSets::RWVS_DT_GAMECUBE:
        platformOut = RwVersionSets::RWVS_PL_GAMECUBE;
        return true;
    case RwVersionSets::RWVS_DT_AMD_COMPRESS:
    case RwVersionSets::RWVS_DT_UNCOMPRESSED_MOBILE:
    case RwVersionSets::RWVS_DT_POWERVR:
    case RwVersionSets::RWVS_DT_S3TC_MOBILE:
        platformOut = RwVersionSets::RWVS_PL_MOBILE;
        return true;
    case RwVersionSets::RWVS_DT_PSP:
        platformOut = RwVersionSets::RWVS_PL_PSP;
        return true;
    }

    // No idea :(
    return false;
}

void MainWindow::updateFriendlyIcons()
{
    // Decide, based on the currently selected texture, what icons to show.

    this->bShowFriendlyIcons = false;

    // We can display just the platform icon if we do not know about the game.
    bool doesHaveGameIcon = false;
    bool doesHavePlatformIcon = false;

    if (this->currentTXD)
    {
        // The idea is that games were released with distinctive RenderWare versions and raster configurations.
        // We should decide very smartly what icons we want to show, out of a limited set.

        QString currentNativeName = this->GetCurrentPlatform();
        RwVersionSets::eDataType dataTypeId = RwVersionSets::dataIdFromEnginePlatformName(currentNativeName);

        if (dataTypeId != RwVersionSets::RWVS_DT_NOT_DEFINED)
        {
            rw::LibraryVersion libVersion = this->currentTXD->GetEngineVersion();

            // Get properties to display icons.
            RwVersionSets::ePlatformType platformType = RwVersionSets::RWVS_PL_NOT_DEFINED;

            int set, platform, data;

            if (this->versionSets.matchSet(libVersion, dataTypeId, set, platform, data))
            {
                const RwVersionSets::Set& currentSet = this->versionSets.sets[ set ];
                const RwVersionSets::Set::Platform& currentPlatform = currentSet.availablePlatforms[ platform ];

                platformType = currentPlatform.platformType;

                if (this->showGameIcon)
                {
                    // Prepare game icon.
                    if (!currentSet.iconName.isEmpty())
                    {
                        QString gameIconPath = makeAppPath("resources/icons/") + currentSet.iconName + ".png";

                        this->friendlyIconGame->setPixmap( QPixmap( gameIconPath ) );

                        doesHaveGameIcon = true;
                    }
                    else if ( currentSet.displayName.isEmpty() == false )
                    {
                        this->friendlyIconGame->setText( currentSet.displayName );

                        doesHaveGameIcon = true;
                    }
                }
                else if (!currentSet.displayName.isEmpty())
                {
                    this->friendlyIconGame->setText(currentSet.displayName);

                    doesHaveGameIcon = true;
                }
            }

            // If we have not gotten a platform icon from the game configuration, we just guess it.
            if ( platformType == RwVersionSets::RWVS_PL_NOT_DEFINED )
            {
                getRecommendedPlatformForDataType( dataTypeId, platformType );
            }

            // Show the platform icon if we could determine it.
            if ( platformType != RwVersionSets::RWVS_PL_NOT_DEFINED )
            {
                // Prepare platform icon.
                QString platIconPath;

                switch (platformType)
                {
                case RwVersionSets::RWVS_PL_NOT_DEFINED:
                    break;
                case RwVersionSets::RWVS_PL_PC:
                    platIconPath = makeAppPath("resources/icons/pc.png");
                    break;
                case RwVersionSets::RWVS_PL_PS2:
                    platIconPath = makeAppPath("resources/icons/ps2.png");
                    break;
                case RwVersionSets::RWVS_PL_PSP:
                    platIconPath = makeAppPath("resources/icons/psp.png");
                    break;
                case RwVersionSets::RWVS_PL_XBOX:
                    platIconPath = makeAppPath("resources/icons/xbox.png");
                    break;
                case RwVersionSets::RWVS_PL_GAMECUBE:
                    platIconPath = makeAppPath("resources/icons/gamecube.png");
                    break;
                case RwVersionSets::RWVS_PL_MOBILE:
                    platIconPath = makeAppPath("resources/icons/mobile.png");
                    break;
                }

                bool doShowPlatIcon = ( platIconPath.isEmpty() == false );

                if ( doShowPlatIcon )
                {
                    this->friendlyIconPlatform->setPixmap( QPixmap( platIconPath ) );

                    doesHavePlatformIcon = true;
                }
            }
        }
    }

    this->friendlyIconGame->setVisible( doesHaveGameIcon );
    this->friendlyIconSeparator->setVisible( doesHaveGameIcon && doesHavePlatformIcon );
    this->friendlyIconPlatform->setVisible( doesHavePlatformIcon );

    this->bShowFriendlyIcons = ( doesHaveGameIcon || doesHavePlatformIcon );
}
