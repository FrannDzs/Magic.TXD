/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.txdprocessing.cpp
*  PURPOSE:     Basic TXD tasks.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "qtrwutils.hxx"

bool MainWindow::openTxdFile(QString fileName, bool silent)
{
    rw::TexDictionary *txd = this->expensiveTasks.loadTxdFile( std::move( fileName ), silent );

    if ( txd == nullptr )
        return false;

    // Okay, we got a new TXD.
    // Set it as our current object in the editor.
    this->setCurrentTXD( txd );

    this->setCurrentFilePath(fileName);

    this->updateFriendlyIcons();

    return true;
}

MagicActionSystem::ActionToken MainWindow::saveCurrentTXDAt(
    QString txdFullPath,
    std::function <void ( void )> success_cb,
    std::function <void ( void )> fail_cb
)
{
    rw::ObjectPtr currentTXD = (rw::TexDictionary*)rw::AcquireObject( this->currentTXD );

    if ( currentTXD.is_good() )
    {
        return this->actionSystem.LaunchActionL(
            eMagicTaskType::SAVE_TXD, "General.TXDSave",
            [this, currentTXD = std::move( currentTXD ), txdFullPath = std::move( txdFullPath ), success_cb = std::move( success_cb )] ( void ) mutable
        {
            rw::rwStaticString <wchar_t> unicodeFullPath = qt_to_widerw( txdFullPath );

            // Try to save at an non-existing file position.
            FileSystem::filePtr tmpStream = fileSystem->GenerateRandomFile( this->tempFileRoot );

            if ( tmpStream.is_good() == false )
            {
                this->asyncLog( MAGIC_TEXT("General.TXDTempStreamFail"), LOGMSG_ERROR );
                throw std::exception();
            }

            // Prepare to copy/move the temporay file.
            filePath tmpFullPath = tmpStream->GetPath();

            try
            {
                // We serialize what we have at the location we loaded the TXD from.
                rw::Interface *rwEngine = this->rwEngine;

                rw::StreamPtr newTXDStream = RwStreamCreateTranslated( rwEngine, tmpStream );

                if ( newTXDStream.is_good() == false )
                {
                    this->asyncLog( MAGIC_TEXT("General.TXDStreamFail"), LOGMSG_ERROR );
                    throw std::exception();
                }

                // Write the TXD into it.
                try
                {
                    rwEngine->Serialize( currentTXD, newTXDStream );
                }
                catch( rw::RwException& except )
                {
                    this->asyncLog(MAGIC_TEXT("General.TXDSaveFail").arg(wide_to_qt(rw::DescribeException(rwEngine, except))), LOGMSG_ERROR);
                    throw;
                }
            }
            catch( ... )
            {
                // Cleanup the temporary file.
                tmpStream = nullptr;

                this->tempFileRoot->Delete( tmpFullPath, filesysPathOperatingMode::FILE_ONLY );

                throw;
            }

            // Close the temp file stream.
            tmpStream = nullptr;

            this->actionSystem.CurrentActionSetPostL(
                [this, tmpFullPath = tmpFullPath, unicodeFullPath = std::move(unicodeFullPath), txdFullPath = std::move( txdFullPath ), success_cb = std::move( success_cb )] ( void ) mutable
                {
                    // Write the TXD into the real location now per file move.
                    // Try with multiple approaches.
                    bool filePushSuccess = fileRoot->Rename( tmpFullPath, unicodeFullPath.GetConstString() );

                    if ( filePushSuccess == false )
                    {
                        filePushSuccess = fileRoot->Copy( tmpFullPath, unicodeFullPath.GetConstString() );
                    }

                    if ( filePushSuccess )
                    {
                        // Success, so lets update our target filename.
                        this->setCurrentFilePath( txdFullPath );

                        // We are no longer modified.
                        this->ClearModifiedState();

                        success_cb();
                    }
                    else
                    {
                        throw std::exception();
                    }
                }
            );

            this->actionSystem.CurrentActionSetCleanupL(
                [this, tmpFullName = std::move(tmpFullPath)] ( void ) mutable
            {
                // Remove the temporary file, if it exists.
                this->tempFileRoot->Delete( tmpFullName, filesysPathOperatingMode::FILE_ONLY );
            });
        }, std::move( fail_cb ));
    }
    else
    {
        fail_cb();
    }

    return MagicActionSystem::ActionToken();
}

void MainWindow::updateTextureView( void )
{
    TexInfoWidget *texItem = this->currentSelectedTexture;

    // We hide the image widget.
    this->clearViewImage();

    if ( texItem != nullptr )
    {
        // Get the actual texture we are associated with and present it on the output pane.
        rw::TextureBase *theTexture = texItem->GetTextureHandle();
        rw::RasterPtr rasterData = rw::AcquireRaster( theTexture->GetRaster() );
        if (rasterData.is_good())
        {
            bool drawMipmapLayers = this->drawMipmapLayers;

            this->tasktoken_updatePreview.Cancel( true );
            this->startMainPreviewLoadingAnimation();
            this->tasktoken_updatePreview = this->taskSystem.LaunchTaskL(
                "main-preview-update",
                [rasterData = std::move(rasterData), drawMipmapLayers, this] ( void ) mutable
                {
                    QImage imgData = this->expensiveTasks.makePreviewOfRaster( rasterData, drawMipmapLayers );

                    this->taskSystem.CurrentTaskSetPostL(
                        [imgData = std::move(imgData), this] ( void ) mutable
                        {
                            this->stopMainPreviewLoadingAnimation();
                            this->imageView->setTexturePixmap( QPixmap::fromImage( imgData ) );
                        }
                    );
                },
                [this]
                {
                    // When the task has been aborted we just stop the loading animation.
                    this->stopMainPreviewLoadingAnimation();
                }
            );
        }
    }
}