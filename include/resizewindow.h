/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/resizewindow.h
*  PURPOSE:     Header of the texture resizing window.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QFormLayout>
#include "qtutils.h"
#include "languages.h"

struct TexResizeWindow : public QDialog, public magicTextLocalizationItem
{
    inline TexResizeWindow( MainWindow *mainWnd, rw::ObjectPtr <rw::TextureBase> toBeResized ) : QDialog( mainWnd ), toBeResized( std::move( toBeResized ) )
    {
        this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

        this->mainWnd = mainWnd;

        this->setWindowModality( Qt::WindowModal );
        this->setAttribute( Qt::WA_DeleteOnClose );

        // Get the current texture dimensions.
        rw::uint32 curWidth = 0;
        rw::uint32 curHeight = 0;

        rw::rasterSizeRules rasterRules;
        {
            if ( rw::Raster *texRaster = this->toBeResized->GetRaster() )
            {
                try
                {
                    // Getting the size may fail if there is no native data.
                    texRaster->getSize( curWidth, curHeight );

                    // Also get the size rules.
                    texRaster->getSizeRules( rasterRules );
                }
                catch( rw::RwException& except )
                {
                    // Got no dimms.
                    curWidth = 0;
                    curHeight = 0;
                }
            }
        }

        // We want a basic dialog with two dimension line edits and our typical two buttons.
        MagicLayout<QFormLayout> layout(this);

        // We only want to accept unsigned integers.
        QIntValidator *dimensionValidator = new QIntValidator( 1, ( rasterRules.maximum ? rasterRules.maxVal : 4096 ), this );

        MagicLineEdit *widthEdit = new MagicLineEdit( ansi_to_qt( std::to_string( curWidth ) ) );
        widthEdit->setValidator( dimensionValidator );

        this->widthEdit = widthEdit;

        connect( widthEdit, &QLineEdit::textChanged, this, &TexResizeWindow::OnChangeDimensionProperty );

        MagicLineEdit *heightEdit = new MagicLineEdit( ansi_to_qt( std::to_string( curHeight ) ) );
        heightEdit->setValidator( dimensionValidator );

        this->heightEdit = heightEdit;

        connect( heightEdit, &QLineEdit::textChanged, this, &TexResizeWindow::OnChangeDimensionProperty );

        layout.top->addRow( CreateLabelL( "Main.Resize.Width" ), widthEdit );
        layout.top->addRow( CreateLabelL( "Main.Resize.Height" ), heightEdit );

        // Now the buttons, I guess.
        QPushButton *buttonSet = CreateButtonL( "Main.Resize.Set" );
        layout.bottom->addWidget( buttonSet );

        this->buttonSet = buttonSet;

        connect( buttonSet, &QPushButton::clicked, this, &TexResizeWindow::OnRequestSet );

        QPushButton *buttonCancel = CreateButtonL( "Main.Resize.Cancel" );
        layout.bottom->addWidget( buttonCancel );

        connect( buttonCancel, &QPushButton::clicked, this, &TexResizeWindow::OnRequestCancel );

        // Remember us as the only resize dialog.
        mainWnd->resizeDlg = this;

        // Initialize the dialog.
        this->UpdateAccessibility();

        RegisterTextLocalizationItem( this );
    }

    inline ~TexResizeWindow( void )
    {
        UnregisterTextLocalizationItem( this );

        // There can be only one.
        this->mainWnd->resizeDlg = nullptr;
    }

    void updateContent( MainWindow *mainWnd ) override
    {
        this->setWindowTitle( MAGIC_TEXT( "Main.Resize.Desc" ) );
    }

public slots:
    void OnChangeDimensionProperty( const QString& newText )
    {
        this->UpdateAccessibility();
    }

    void OnRequestSet( bool checked )
    {
        // Do the resize.
        MainWindow *mainWnd = this->mainWnd;

        rw::ObjectPtr toBeResized = this->toBeResized;

        rw::RasterPtr texRaster = rw::AcquireRaster( toBeResized->GetRaster() );

        if ( texRaster.is_good() )
        {
            // Fetch the sizes (with minimal validation).
            QString widthDimmString = this->widthEdit->text();
            QString heightDimmString = this->heightEdit->text();

            bool validWidth, validHeight;

            int widthDimm = widthDimmString.toInt( &validWidth );
            int heightDimm = heightDimmString.toInt( &validHeight );

            if ( validWidth && validHeight )
            {
                mainWnd->actionSystem.LaunchActionL(
                    eMagicTaskType::RESIZE_TEXTURE, "General.TexResize",
                    [mainWnd, widthDimm, heightDimm, texRaster = std::move( texRaster ), toBeResized = std::move(toBeResized)] ( void ) mutable
                {
                    mainWnd->actionSystem.CurrentActionSetCleanupL(
                        [mainWnd, toBeResized = std::move(toBeResized)]
                        {
                            // We have changed the TXD.
                            mainWnd->NotifyChange();

                            // Since we succeeded, we should update the view and things.
                            mainWnd->updateTextureView();

                            // Also update the texture if the info widget is available.
                            if ( TexInfoWidget *texInfo = mainWnd->getTextureTexInfoLink( toBeResized ) )
                            {
                                texInfo->updateInfo();
                            }
                        }
                    );

                    // Resize!
                    rw::uint32 rwWidth = (rw::uint32)widthDimm;
                    rw::uint32 rwHeight = (rw::uint32)heightDimm;

                    try
                    {
                        // Use default filters.
                        texRaster->resize( rwWidth, rwHeight );
                    }
                    catch( rw::RwException& except )
                    {
                        mainWnd->asyncLog(
                            MAGIC_TEXT("General.TexResizeFail").arg(wide_to_qt( rw::DescribeException( texRaster->getEngine(), except ))),
                            LOGMSG_ERROR
                        );
                        throw;
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
    void UpdateAccessibility( void )
    {
        // Only allow setting if we have a width and height, whose values are different from the original.
        bool allowSet = true;

        if ( rw::Raster *texRaster = toBeResized->GetRaster() )
        {
            rw::uint32 curWidth, curHeight;

            try
            {
                texRaster->getSize( curWidth, curHeight );
            }
            catch( rw::RwException& )
            {
                curWidth = 0;
                curHeight = 0;
            }

            // Check we have dimensions currenctly.
            bool gotValidDimms = false;

            QString widthDimmString = this->widthEdit->text();
            QString heightDimmString = this->heightEdit->text();

            if ( widthDimmString.isEmpty() == false && heightDimmString.isEmpty() == false )
            {
                bool validWidth, validHeight;

                int widthDimm = widthDimmString.toInt( &validWidth );
                int heightDimm = heightDimmString.toInt( &validHeight );

                if ( validWidth && validHeight )
                {
                    if ( widthDimm > 0 && heightDimm > 0 )
                    {
                        // Also verify whether the native texture can even accept the dimensions.
                        // Otherwise we get a nice red present.
                        rw::rasterSizeRules rasterRules;

                        texRaster->getSizeRules( rasterRules );

                        if ( rasterRules.verifyDimensions( widthDimm, heightDimm ) )
                        {
                            // Now lets verify that those are different.
                            rw::uint32 selWidth = (rw::uint32)widthDimm;
                            rw::uint32 selHeight = (rw::uint32)heightDimm;

                            if ( selWidth != curWidth || selHeight != curHeight )
                            {
                                gotValidDimms = true;
                            }
                        }
                    }
                }
            }

            if ( !gotValidDimms )
            {
                allowSet = false;
            }
        }

        this->buttonSet->setDisabled( !allowSet );
    }

    MainWindow *mainWnd;
    rw::ObjectPtr <rw::TextureBase> toBeResized;

    QPushButton *buttonSet;
    MagicLineEdit *widthEdit;
    MagicLineEdit *heightEdit;
};
