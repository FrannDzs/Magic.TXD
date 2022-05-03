/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.events.cpp
*  PURPOSE:     Qt event handling.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include <QtCore/QMimeData>
#include <QtGui/QCloseEvent>

#include "tools/imagepipe.hxx"

#include "qtrwutils.hxx"

void MainWindow::closeEvent( QCloseEvent *evt )
{
    // Here we prompt the user with a modifications-pending dialog on close.
    // If the user decided to save the TXD or decided to ignore the changes then we can just close.
    // But the user does always have the option to cancel actions that are critical to changes
    // pending. In that case the closing of the editor is not performed.
    bool doClose = false;

    ModifiedStateBarrier( true,
        [&]( void )
    {
        doClose = true;
    });

    if ( doClose )
    {
        evt->accept();
    }
    else
    {
        evt->ignore();
    }
}

// Drag and drop stuff.
void MainWindow::dragEnterEvent( QDragEnterEvent *evt )
{
    // Check if we are a TXD or image file, based on the extention.
    const QMimeData *mimeStuff = evt->mimeData();

    // Basically, we receive a number of files in a drag operation.
    // We only support a single TXD file in the

    if ( mimeStuff )
    {
        rw::Interface *rwEngine = this->rwEngine;

        QList <QUrl> urls = mimeStuff->urls();

        bool areLocationsLookingGood = false;
        bool hasValidFile = false;
        bool hasTXDFile = false;

        for ( QUrl location : urls )
        {
            QString qtPath = location.toLocalFile();

            if ( qtPath.isEmpty() == false )
            {
                std::wstring widePath = qtPath.toStdWString();

                filePath extention;

                FileSystem::GetFileNameItem <FileSysCommonAllocator> ( widePath.c_str(), false, nullptr, &extention );

                if ( extention.size() != 0 )
                {
                    bool recognizedData = false;

                    bool hasNewTXDFile = false;

                    // We do support opening TXD files.
                    if ( extention.equals( L"TXD", false ) )
                    {
                        // If we had a valid file already, we quit.
                        if ( hasValidFile )
                        {
                            areLocationsLookingGood = false;
                            break;
                        }

                        recognizedData = true;

                        hasNewTXDFile = true;
                    }

                    // Recognize image data if we have an open TXD file.
                    if ( this->currentTXD )
                    {
                        eImportExpectation imp_exp = getActualImageImportExpectation( rwEngine, extention );

                        if ( imp_exp != IMPORTE_NONE )
                        {
                            recognizedData = true;
                        }
                    }

                    // If we support it, then we accept it.
                    if ( recognizedData )
                    {
                        // If we had a TXD file, there is no point.
                        if ( hasTXDFile )
                        {
                            areLocationsLookingGood = false;
                            break;
                        }

                        areLocationsLookingGood = true;

                        hasValidFile = true;
                    }

                    if ( hasNewTXDFile )
                    {
                        hasTXDFile = true;
                    }
                }
            }
        }

        if ( areLocationsLookingGood )
        {
            evt->acceptProposedAction();
        }
        else
        {
            evt->ignore();
        }
    }
}

void MainWindow::dragLeaveEvent( QDragLeaveEvent *evt )
{
    // Nothin to do here.
    return;
}

void MainWindow::dropEvent( QDropEvent *evt )
{
    // Check what kind of data we got.
    if ( const QMimeData *mimeStuff = evt->mimeData() )
    {
        rw::Interface *rwEngine = this->rwEngine;

        QList <QUrl> urls = mimeStuff->urls();

        // We want to display the image config dialog if we add just one image.
        bool isSingleFile = ( urls.size() == 1 );

        for ( QUrl location : urls )
        {
            QString qtPath = location.toLocalFile();

            if ( qtPath.isEmpty() == false )
            {
                std::wstring widePath = qtPath.toStdWString();

                filePath extention;

                filePath nameItem = FileSystem::GetFileNameItem <FileSysCommonAllocator> ( widePath.c_str(), false, nullptr, &extention );

                bool hasHandledFile = false;

                // * TXD file?
                if ( extention.equals( L"TXD", false ) )
                {
                    bool loadedTXD = this->openTxdFile( qtPath, false );

                    if ( loadedTXD )
                    {
                        hasHandledFile = true;
                    }
                }

                if ( !hasHandledFile )
                {
                    // * image file?
                    rw::ObjectPtr txd = (rw::TexDictionary*)rw::AcquireObject( this->currentTXD );

                    if ( txd.is_good() )
                    {
                        if ( isSingleFile )
                        {
                            // Should verify if this is even an image file candidate.
                            eImportExpectation imp_exp = getActualImageImportExpectation( rwEngine, extention );

                            if ( imp_exp != IMPORTE_NONE )
                            {
                                // We want to allow configuration.
                                spawnTextureAddDialog( qtPath );
                            }
                        }
                        else
                        {
                            this->actionSystem.LaunchActionL(
                                eMagicTaskType::ADD_TEXTURE, "Main.TexDragNDrop",
                                [this, txd, nameItem, widePath, extention] ( void ) mutable
                                {
                                    rw::rwStaticString <char> platformTexName = qt_to_ansirw( this->GetCurrentPlatform() );

                                    rw::ObjectPtr rwtex = this->expensiveTasks.loadTexture( widePath.c_str(), platformTexName.GetConstString(), extention );

                                    if ( rwtex.is_good() )
                                    {
                                        // Set the texture version to the TXD version.
                                        rwtex->SetEngineVersion( txd->GetEngineVersion() );

                                        // Give the texture an ANSI name.
                                        // NOTE that we overwrite any original name that the texture chunk might have come with.
                                        auto ansiTexName = nameItem.convert_ansi <rw::RwStaticMemAllocator> ();

                                        this->actionSystem.CurrentActionSetPostL(
                                            [this, rwtex = std::move(rwtex), ansiTexName = std::move(ansiTexName)] ( void ) mutable
                                            {
                                                this->DefaultTextureAddAndPrepare( rwtex.detach(), ansiTexName.GetConstString(), "" );
                                            }
                                        );
                                    }
                                }
                            );
                        }
                    }
                }
            }
        }
    }
}

// Need to catch some custom events.
void MainWindow::customEvent( QEvent *evt )
{
    MGTXDCustomEvent evtType = (MGTXDCustomEvent)evt->type();

    if ( evtType == MGTXDCustomEvent::AsyncLogEvent )
    {
        MGTXDAsyncLogEvent *cevt = (MGTXDAsyncLogEvent*)evt;

        this->asyncLogEvent( cevt );
        return;
    }
}
