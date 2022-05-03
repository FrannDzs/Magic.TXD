/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.loadinganim.cpp
*  PURPOSE:     Loading animation helpers.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include <mainwindow.h>

#include <sdk/NumericFormat.h>
#include <sdk/Vector.h>

#include "mainwindow.loadinganim.hxx"

optional_struct_space <PluginDependantStructRegister <statusBarAnimationMembers, mainWindowFactory_t>> statusBarAnimationMembersRegister;

void MainWindow::startStatusLoadingAnimation( void )
{
    statusBarAnimationMembers *loadingInfo = statusBarAnimationMembersRegister.get().GetPluginStruct( this );
    
    loadingInfo->statusBar_loadingAnim.get().start();
}

void MainWindow::stopStatusLoadingAnimation( void )
{
    statusBarAnimationMembers *loadingInfo = statusBarAnimationMembersRegister.get().GetPluginStruct( this );
    
    loadingInfo->statusBar_loadingAnim.get().stop();
}

void MainWindow::progressStatusLoadingAnimation( void )
{
    statusBarAnimationMembers *loadingInfo = statusBarAnimationMembersRegister.get().GetPluginStruct( this );

    loadingInfo->statusBar_loadingAnim.get().progress();
}

void MainWindow::startMainPreviewLoadingAnimation( void )
{
    statusBarAnimationMembers *loadingInfo = statusBarAnimationMembersRegister.get().GetPluginStruct( this );

    this->imageView->setViewportMode( eViewportMode::SYSTEM );
    this->imageView->setContentSize( 400, 400 );
    loadingInfo->mainPreviewLoadingAnim.get().start();
}

void MainWindow::stopMainPreviewLoadingAnimation( void )
{
    statusBarAnimationMembers *loadingInfo = statusBarAnimationMembersRegister.get().GetPluginStruct( this );

    loadingInfo->mainPreviewLoadingAnim.get().stop();
    this->imageView->setViewportMode( eViewportMode::DISABLED );
}

// Module initialization and shutdown.
void InitializeMainWindowStatusBarAnimations( void )
{
    statusBarAnimationMembersRegister.Construct( mainWindowFactory );
}

void ShutdownMainWindowStatusBarAnimations( void )
{
    statusBarAnimationMembersRegister.Destroy();
}