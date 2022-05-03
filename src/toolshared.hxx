/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/toolshared.hxx
*  PURPOSE:     Shared utilities between editor tools.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include "tools/shared.h"

#include "qtinteroputils.hxx"

// Put more special shared things here, which are shared across Magic.TXD tools.
namespace toolshare
{

struct platformToNatural
{
    rwkind::eTargetPlatform mode;
    std::string natural;

    inline bool operator == ( const decltype( mode )& right ) const
    {
        return ( right == this->mode );
    }

    inline bool operator == ( const QString& right ) const
    {
        return ( qstring_native_compare( right, this->natural.c_str() ) );
    }
};

typedef naturalModeList <platformToNatural> platformToNaturalList_t;

static platformToNaturalList_t platformToNaturalList =
{
    { rwkind::PLATFORM_PC, "PC" },
    { rwkind::PLATFORM_PS2, "PS2" },
    { rwkind::PLATFORM_PSP, "PSP" },
    { rwkind::PLATFORM_XBOX, "XBOX" },
    { rwkind::PLATFORM_GC, "Gamecube" },
    { rwkind::PLATFORM_DXT_MOBILE, "S3TC mobile" },
    { rwkind::PLATFORM_PVR, "PowerVR" },
    { rwkind::PLATFORM_ATC, "AMD TC" },
    { rwkind::PLATFORM_UNC_MOBILE, "uncomp. mobile" }
};

struct gameToNatural
{
    rwkind::eTargetGame mode;
    std::string natural;

    inline bool operator == ( const decltype( mode )& right ) const
    {
        return ( right == this->mode );
    }

    inline bool operator == ( const QString& right ) const
    {
        return ( qstring_native_compare( right, this->natural.c_str() ) );
    }
};

typedef naturalModeList <gameToNatural> gameToNaturalList_t;

static gameToNaturalList_t gameToNaturalList =
{
    { rwkind::GAME_GTA3, "GTA III" },
    { rwkind::GAME_GTAVC, "GTA VC" },
    { rwkind::GAME_GTASA, "GTA SA" },
    { rwkind::GAME_MANHUNT, "Manhunt" },
    { rwkind::GAME_BULLY, "Bully" },
    { rwkind::GAME_LCS, "LCS" },
    { rwkind::GAME_SHEROES, "SHeroes" }
};

template <typename layoutClass>
inline void createTargetConfigurationComponents(
    layoutClass *rootLayout,
    rwkind::eTargetPlatform curPlatform, rwkind::eTargetGame curGame,
    QComboBox*& gameSelBoxOut,
    QComboBox*& platSelBoxOut
)
{
    // Now a target format selection group.
    QHBoxLayout *platformGroup = new QHBoxLayout();

    platformGroup->addWidget( CreateLabelL( "Tools.Plat" ) );

    QComboBox *platformSelBox = new QComboBox();

    // We have a fixed list of platforms here.
    platformToNaturalList.putDown( platformSelBox );

    platSelBoxOut = platformSelBox;

    // Select current.
    platformToNaturalList.selectCurrent( platformSelBox, curPlatform );

    platformGroup->addWidget( platformSelBox );

    rootLayout->addLayout( platformGroup );

    QHBoxLayout *gameGroup = new QHBoxLayout();

    gameGroup->addWidget( CreateLabelL( "Tools.Game" ) );

    QComboBox *gameSelBox = new QComboBox();

    // Add a fixed list of known games.
    gameToNaturalList.putDown( gameSelBox );

    gameSelBoxOut = gameSelBox;

    gameToNaturalList.selectCurrent( gameSelBox, curGame );

    gameGroup->addWidget( gameSelBox );

    rootLayout->addLayout( gameGroup );
}

};