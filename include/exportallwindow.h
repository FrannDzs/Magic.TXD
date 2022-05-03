/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/exportallwindow.h
*  PURPOSE:     Header of the export all textures window.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <renderware.h>

#include <QtWidgets/QDialog>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>

#include "qtutils.h"
#include "languages.h"

#include <sdk/UniChar.h>

// We want to have a simple window which can be used by the user to export all textures of a TXD.

struct ExportAllWindow : public QDialog, public magicTextLocalizationItem
{
private:
    inline static rw::rwStaticVector <rw::rwStaticString <char>> GetAllSupportedImageFormats( rw::TexDictionary *texDict )
    {
        rw::rwStaticVector <rw::rwStaticString <char>> formatsOut;

        rw::Interface *engineInterface = texDict->GetEngine();

        // The only formats that we question support for are native formats.
        // Currently, each native texture can export one native format.
        // Hence the only supported formats can be formats that are supported from the first texture on.
        bool isFirstRaster = true;

        for ( rw::TexDictionary::texIter_t iter( texDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
        {
            rw::TextureBase *texture = iter.Resolve();

            rw::Raster *texRaster = texture->GetRaster();

            if ( texRaster )
            {
                // If we are the first raster we encounter, we add all of its formats to our list.
                // Otherwise we check for support on each raster until no more support is guarranteed.
                rw::nativeImageRasterResults_t reg_natTex;

                const char *nativeName = texRaster->getNativeDataTypeName();

                rw::GetNativeImageTypesForNativeTexture( engineInterface, nativeName, reg_natTex );

                if ( isFirstRaster )
                {
                    size_t numFormats = reg_natTex.GetCount();

                    for ( size_t n = 0; n < numFormats; n++ )
                    {
                        formatsOut.AddToBack( reg_natTex[n] );
                    }

                    isFirstRaster = false;
                }
                else
                {
                    // If we have no more formats, we quit.
                    if ( formatsOut.GetCount() == 0 )
                    {
                        break;
                    }

                    // Remove anything thats not part of the supported.
                    rw::rwStaticVector <rw::rwStaticString <char>> newList;

                    for ( const auto& nativeFormat : reg_natTex )
                    {
                        for ( const auto& formstr : formatsOut )
                        {
                            if ( formstr == nativeFormat )
                            {
                                newList.AddToBack( nativeFormat );
                                break;
                            }
                        }
                    }

                    formatsOut = std::move( newList );
                }
            }
        }

        // After that come the formats that every raster supports anyway.
        rw::registered_image_formats_t availImageFormats;

        rw::GetRegisteredImageFormats( engineInterface, availImageFormats );

        for ( const rw::registered_image_format& format : availImageFormats )
        {
            const char *defaultExt = nullptr;

            bool gotDefaultExt = rw::GetDefaultImagingFormatExtension( format.num_ext, format.ext_array, defaultExt );

            if ( gotDefaultExt )
            {
                formatsOut.AddToBack( defaultExt );
            }
        }

        // And of course, the texture chunk, that is always supported.
        formatsOut.AddToBack( "RWTEX" );

        return formatsOut;
    }

public:
    ExportAllWindow( MainWindow *mainWnd, rw::ObjectPtr <rw::TexDictionary> texDict )
        : QDialog( mainWnd ), texDict( std::move( texDict ) )
    {
        this->mainWnd = mainWnd;
        setWindowFlags( windowFlags() & ~Qt::WindowContextHelpButtonHint );
        setWindowModality( Qt::WindowModal );
        this->setAttribute( Qt::WA_DeleteOnClose );
        // Basic window with a combo box and a button to accept.
        MagicLayout<QFormLayout> layout(this);

        QComboBox *formatSelBox = new QComboBox();
        // List formats that are supported by every raster in a TXD.
        {
            auto formatsSupported = GetAllSupportedImageFormats( texDict );
            for ( const auto& format : formatsSupported )
            {
                formatSelBox->addItem( ansi_to_qt( format ) );
            }
        }

        // Select the last used format, if it exists.
        {
            int foundLastUsed = formatSelBox->findText( ansi_to_qt( mainWnd->lastUsedAllExportFormat ), Qt::MatchExactly );
            if ( foundLastUsed != -1 )
                formatSelBox->setCurrentIndex( foundLastUsed );
        }
        this->formatSelBox = formatSelBox;
        layout.top->addRow(CreateLabelL( "Main.ExpAll.Format" ), formatSelBox);

        // Add the button row last.
        QPushButton *buttonExport = CreateButtonL( "Main.ExpAll.Export" );

        connect( buttonExport, &QPushButton::clicked, this, &ExportAllWindow::OnRequestExport );

        layout.bottom->addWidget( buttonExport );
        QPushButton *buttonCancel = CreateButtonL( "Main.ExpAll.Cancel" );

        connect( buttonCancel, &QPushButton::clicked, this, &ExportAllWindow::OnRequestCancel );

        layout.bottom->addWidget( buttonCancel );
        this->setMinimumWidth( 250 );

        RegisterTextLocalizationItem( this );
    }

    ~ExportAllWindow( void )
    {
        UnregisterTextLocalizationItem( this );
    }

    void updateContent( MainWindow *mainWnd ) override
    {
        this->setWindowTitle( MAGIC_TEXT( "Main.ExpAll.Desc" ) );
    }

public slots:
    void OnRequestExport( bool checked )
    {
        MainWindow *mainWnd = this->mainWnd;

        rw::ObjectPtr <rw::TexDictionary> texDict = this->texDict;

        rw::Interface *engineInterface = texDict->GetEngine();

        // Get the format to export as.
        QString formatTarget = this->formatSelBox->currentText();

        if ( formatTarget.isEmpty() == false )
        {
            rw::rwStaticString <char> ansiFormatTarget = qt_to_ansirw( formatTarget );

            // We need a directory to export to, so ask the user.
            QString folderExportTarget = QFileDialog::getExistingDirectory(
                this, MAGIC_TEXT("Main.ExpAll.ExpTarg"),
                wide_to_qt( mainWnd->lastAllExportTarget )
            );

            if ( folderExportTarget.isEmpty() == false )
            {
                rw::rwStaticString <wchar_t> wFolderExportTarget = qt_to_widerw( folderExportTarget );

                // Remember this path.
                mainWnd->lastAllExportTarget = wFolderExportTarget;

                wFolderExportTarget += L'/';

                mainWnd->actionSystem.LaunchActionL(
                    eMagicTaskType::EXPORT_ALL, "General.ExportTexAll",
                    [mainWnd, texDict = std::move(texDict), formatTarget = std::move(formatTarget), engineInterface, ansiFormatTarget = std::move(ansiFormatTarget)] ( void ) mutable
                {
                    // If we successfully did any export, we can close.
                    bool hasExportedAnything = false;

                    for ( rw::TexDictionary::texIter_t iter( texDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
                    {
                        rw::TextureBase *texture = iter.Resolve();

                        // TODO: we might want to have the ability to lock textures, raster, tc so we can query
                        // abilities safely without them modifying under our fingertips.

                        // We can only serialize if we have a raster.
                        rw::RasterPtr texRaster = rw::AcquireRaster( texture->GetRaster() );

                        if ( texRaster.is_good() )
                        {
                            // Create a path to put the image at.
                            auto imgExportPath = texture->GetName() + '.' + qt_to_ansirw( formatTarget.toLower() );

                            FileSystem::filePtr targetStream = fileRoot->Open( imgExportPath.GetConstString(), "wb" );

                            if ( targetStream.is_good() )
                            {
                                rw::StreamPtr rwStream = RwStreamCreateTranslated( engineInterface, targetStream );

                                if ( rwStream.is_good() )
                                {
                                    // Now attempt the write.
                                    try
                                    {
                                        if ( StringEqualToZero( ansiFormatTarget.GetConstString(), "RWTEX", false ) )
                                        {
                                            engineInterface->Serialize( texture, rwStream );
                                        }
                                        else
                                        {
                                            texRaster->writeImage( rwStream, ansiFormatTarget.GetConstString() );
                                        }

                                        hasExportedAnything = true;
                                    }
                                    catch( rw::RwException& except )
                                    {
                                        mainWnd->asyncLog(
                                            MAGIC_TEXT("General.ExportTexFailNamed")
                                            .arg(ansi_to_qt(texture->GetName()), wide_to_qt( rw::DescribeException( engineInterface, except ) )),
                                            LOGMSG_WARNING
                                        );
                                    }
                                }
                                else
                                {
                                    mainWnd->asyncLog(
                                        MAGIC_TEXT("General.StreamFailTex").arg(ansi_to_qt(texture->GetName())),
                                        LOGMSG_WARNING
                                    );
                                }
                            }
                            else
                            {
                                mainWnd->asyncLog(
                                    MAGIC_TEXT("General.StreamFailTex").arg(ansi_to_qt(texture->GetName())),
                                    LOGMSG_WARNING
                                );
                            }
                        }
                    }

                    if ( hasExportedAnything )
                    {
                        mainWnd->actionSystem.CurrentActionSetPostL(
                            [mainWnd, ansiFormatTarget = std::move(ansiFormatTarget)] ( void ) mutable
                            {
                                // Remember the format that we used.
                                mainWnd->lastUsedAllExportFormat = std::move( ansiFormatTarget );

                                // We can close now.
                                //TODO: actually close only if we exported anything; would require more efficient wiring.
                            }
                        );
                    }
                });
            }
        }

        this->close();
    }

    void OnRequestCancel( bool checked )
    {
        this->close();
    }

private:
    MainWindow *mainWnd;
    rw::ObjectPtr <rw::TexDictionary> texDict;

    QComboBox *formatSelBox;
};
