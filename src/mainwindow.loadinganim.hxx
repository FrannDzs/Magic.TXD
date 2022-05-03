/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.loadinganim.hxx
*  PURPOSE:     Internal header of the loading animations.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QStatusBar>

#include <sdk/PluginHelpers.h>

struct statusBarAnimationMembers : public QObject, public magicThemeAwareItem
{
    inline void Initialize( MainWindow *mainWnd )
    {
        NativeExecutive::CExecutiveManager *execMan = (NativeExecutive::CExecutiveManager*)rw::GetThreadingNativeManager( mainWnd->GetEngine() );

        this->execMan = execMan;

        QStatusBar *statusBar = mainWnd->statusBar();

        assert( statusBar != nullptr );

        AnimationSimpleUpdateLabel *statusBarLoadingIcon = new AnimationSimpleUpdateLabel;
        statusBarLoadingIcon->setObjectName( "statusLoadingIcon" );
        statusBarLoadingIcon->setScaledContents( true );

        this->statusBar_loadingAnim.Construct( execMan, &this->statusBar_frameStore );
        this->statusBar_loadingAnim.get().setTargetWidget( statusBarLoadingIcon );
        this->statusBarLoadingIcon = statusBarLoadingIcon;

        this->mainPreviewLoadingAnim.Construct( execMan, &this->previewLoadingFrames );
        this->mainPreviewLoadingAnim.get().setTargetWidget( mainWnd->imageView );

        statusBar->addPermanentWidget( statusBarLoadingIcon );

        mainWnd->RegisterThemeItem( this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        mainWnd->UnregisterThemeItem( this );

        this->mainPreviewLoadingAnim.Destroy();
        this->statusBar_loadingAnim.Destroy();
    }

    void updateTheme( MainWindow *mainWnd ) override
    {
        // Load the images.
        this->statusBar_loadingAnim.get().initializeData( mainWnd, "loading_full_circle.png" );
        this->statusBar_frameStore.initializeData( mainWnd, "loading_frame", ".png" );
        this->previewLoadingFrames.initializeData( mainWnd, "loading_preview_frame", ".png" );

        // Update the images.
        this->statusBar_loadingAnim.get().refreshPixmap();
    }

    NativeExecutive::CExecutiveManager *execMan;

    // Loading animation members.
    cyclingAnimationStore statusBar_frameStore;
    optional_struct_space <cyclingAnimation> statusBar_loadingAnim;
    AnimationSimpleUpdateLabel *statusBarLoadingIcon;

    // Animation for main preview loading.
    cyclingAnimationStore previewLoadingFrames;
    optional_struct_space <cyclingAnimation> mainPreviewLoadingAnim;
};

extern optional_struct_space <PluginDependantStructRegister <statusBarAnimationMembers, mainWindowFactory_t>> statusBarAnimationMembersRegister;