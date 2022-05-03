/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/texadddialog.cpp
*  PURPOSE:     Texture add/modify dialog implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "texadddialog.h"

#include "qtrwutils.hxx"

#include "qtutils.h"
#include "languages.h"
#include "testmessage.h"

#include "texnameutils.hxx"

#include "mainwindow.loadinganim.hxx"

#ifdef _DEBUG
static constexpr bool _lockdownPlatform = false;        // SET THIS TO TRUE FOR RELEASE.
#else
static constexpr bool _lockdownPlatform = true;         // WE ARE RELEASING SOON.
#endif
static constexpr size_t _recommendedPlatformMaxName = 32;
static constexpr bool _enableMaskName = false;

void TexAddDialog::SetCurrentPlatform( QString name )
{
    if (MagicLineEdit *editBox = dynamic_cast <MagicLineEdit*> ( this->platformSelectWidget ) )
    {
        editBox->setText( std::move( name ) );
    }
    else if ( QComboBox *comboBox = dynamic_cast <QComboBox*> ( this->platformSelectWidget ) )
    {
        comboBox->setCurrentText( std::move( name ) );
    }
}

QString TexAddDialog::GetCurrentPlatform(void)
{
    QString currentPlatform;

    if (MagicLineEdit *editBox = dynamic_cast <MagicLineEdit*> (this->platformSelectWidget))
    {
        currentPlatform = editBox->text();
    }
    else if (QComboBox *comboBox = dynamic_cast <QComboBox*> (this->platformSelectWidget))
    {
        currentPlatform = comboBox->currentText();
    }

    return currentPlatform;
}

void TexAddDialog::RwTextureAssignNewRaster(
    rw::TextureBase *texHandle, rw::Raster *newRaster,
    const rw::rwStaticString <char>& texName, const rw::rwStaticString <char>& maskName
)
{
    // Update names.
    texHandle->SetName( texName.GetConstString() );
    texHandle->SetMaskName( maskName.GetConstString() );

    // Replace raster handle.
    texHandle->SetRaster( newRaster );

    // We have to set proper filtering flags.
    texHandle->fixFiltering();
}

void TexAddDialog::releaseConvRaster(void)
{
    this->convRaster = nullptr;
}

void TexAddDialog::clearTextureOriginal( void )
{
    // Remove any previous raster link.
    this->platformOrigRaster = nullptr;

    // Delete any texture link.
    this->texHandle = nullptr;
}

// Special variant of image importing for our dialog.
struct texAddImageImportMethods : public imageImportMethods
{
    inline texAddImageImportMethods( MainWindow *mainWindow, QString platformName )
    {
        this->mainWindow = mainWindow;
        this->platformName = std::move( platformName );
    }

    void setRWVersion( rw::LibraryVersion rwversion )
    {
        this->rwversion = std::move( rwversion );
        this->has_rwversion = true;
    }

    void OnWarning( rw::rwStaticString <wchar_t>&& msg ) const override
    {
        this->mainWindow->asyncLog( wide_to_qt( msg ), LOGMSG_WARNING );
    }

    void OnWarning( QString&& msg ) const override
    {
        this->mainWindow->asyncLog( std::move( msg ), LOGMSG_WARNING );
    }

    void OnError( rw::rwStaticString <wchar_t>&& msg ) const override
    {
        this->mainWindow->asyncLog( wide_to_qt( msg ), LOGMSG_ERROR );
    }

    void OnError( QString&& msg ) const override
    {
        this->mainWindow->asyncLog( std::move( msg ) );
    }

    rw::Raster* MakeRaster( void ) const override
    {
        rw::Interface *rwEngine = this->mainWindow->GetEngine();

        rw::Raster *platOrig = rw::CreateRaster( rwEngine );

        if ( platOrig )
        {
            try
            {
                // Set the platform of our raster.
                // If we have no platform, we fail.
                bool hasPlatform = false;

                QString currentPlatform = this->platformName;

                if (currentPlatform.isEmpty() == false)
                {
                    rw::rwStaticString <char> ansiNativeName = qt_to_ansirw( currentPlatform );

                    // Set the platform.
                    platOrig->newNativeData( ansiNativeName.GetConstString() );

                    // We also want to set the version of our raster.
                    if ( this->has_rwversion )
                    {
                        platOrig->SetEngineVersion( this->rwversion );
                    }

                    hasPlatform = true;
                }

                if ( !hasPlatform )
                {
                    rw::DeleteRaster( platOrig );

                    platOrig = nullptr;
                }
            }
            catch( ... )
            {
                rw::DeleteRaster( platOrig );

                throw;
            }
        }

        return platOrig;
    }

    MainWindow *mainWindow;
    QString platformName;
    rw::LibraryVersion rwversion;
    bool has_rwversion = false;
};

void TexAddDialog::startLoadingAnimation( void )
{
    if ( this->loadingAnimation.get().isRunning() == false )
    {
        this->previewView->setViewportMode( eViewportMode::SYSTEM );
        this->loadingAnimation.get().start();
        this->previewView->setContentSize( 300, 300 );
    }
}

void TexAddDialog::stopLoadingAnimation( void )
{
    this->loadingAnimation.get().stop();
    this->previewView->setViewportMode( eViewportMode::DISABLED );
}

void TexAddDialog::OnUpdatePlatformOriginal( void )
{
    bool hasPreview = this->hasPlatformOriginal;

    // Hide or show the changeable properties.
    this->propGenerateMipmaps->setVisible( hasPreview );

    // If we have no preview, then we also cannot push the data to the texture container.
    // This is why we should disable that possibility.
    this->applyButton->setDisabled( !hasPreview );

    QString newText = this->GetCurrentPlatform();

    // We want to show the user properties based on what this platform supports.
    // So we fill the fields.

    rw::rwStaticString <char> ansiNativeName = qt_to_ansirw( newText );

    rw::nativeRasterFormatInfo formatInfo;

    // Any setting of values here should not be acknowledged for.
    this->adjustingProgrammaticConfig = true;

    // Decide what to do.
    bool enableOriginal = true;
    bool enableRawRaster = true;
    bool enableCompressSelect = false;
    bool enablePaletteSelect = false;
    bool enablePixelFormatSelect = true;

    bool supportsDXT1 = true;
    bool supportsDXT2 = true;
    bool supportsDXT3 = true;
    bool supportsDXT4 = true;
    bool supportsDXT5 = true;

    if ( hasPreview )
    {
        bool gotFormatInfo = rw::GetNativeTextureFormatInfo(this->mainWnd->rwEngine, ansiNativeName.GetConstString(), formatInfo);

        if (gotFormatInfo)
        {
            if (formatInfo.isCompressedFormat)
            {
                // We are a fixed compressed format, so we will pass pixels with high quality to the pipeline.
                enableRawRaster = false;
                enableCompressSelect = true;    // decide later.
                enablePaletteSelect = false;
                enablePixelFormatSelect = false;
            }
            else
            {
                // We are a dynamic raster, so whatever goes best.
                enableRawRaster = true;
                enableCompressSelect = true;    // we decide this later again.
                enablePaletteSelect = formatInfo.supportsPalette;
                enablePixelFormatSelect = true;
            }

            supportsDXT1 = formatInfo.supportsDXT1;
            supportsDXT2 = formatInfo.supportsDXT2;
            supportsDXT3 = formatInfo.supportsDXT3;
            supportsDXT4 = formatInfo.supportsDXT4;
            supportsDXT5 = formatInfo.supportsDXT5;
        }
    }
    else
    {
        // If there is no preview, we want nothing.
        enableOriginal = false;
        enableRawRaster = false;
        enableCompressSelect = false;
        enablePaletteSelect = false;
        enablePixelFormatSelect = false;
    }

    // Decide whether enabling the compression select even makes sense.
    // If we have no compression supported, then it makes no sense.
    if (enableCompressSelect)
    {
        enableCompressSelect =
            (supportsDXT1 || supportsDXT2 || supportsDXT3 || supportsDXT4 || supportsDXT5);
    }

    // Do stuff.
    this->platformOriginalToggle->setVisible(enableOriginal);

    if (QWidget *partnerWidget = this->platformPropForm->labelForField(this->platformOriginalToggle))
    {
        partnerWidget->setVisible(enableOriginal);
    }

    this->platformRawRasterProp->setVisible(enableRawRaster);

    if (QWidget *partnerWidget = this->platformPropForm->labelForField(this->platformRawRasterProp))
    {
        partnerWidget->setVisible(enableRawRaster);
    }

    this->platformCompressionSelectProp->setVisible(enableCompressSelect);

    if (QWidget *partnerWidget = this->platformPropForm->labelForField(this->platformCompressionSelectProp))
    {
        partnerWidget->setVisible(enableCompressSelect);
    }

    this->platformPaletteSelectProp->setVisible(enablePaletteSelect);

    if (QWidget *partnerWidget = this->platformPropForm->labelForField(this->platformPaletteSelectProp))
    {
        partnerWidget->setVisible(enablePaletteSelect);
    }

    this->platformPixelFormatSelectProp->setVisible(enablePixelFormatSelect);

    if (QWidget *partnerWidget = this->platformPropForm->labelForField(this->platformPixelFormatSelectProp))
    {
        partnerWidget->setVisible(enablePixelFormatSelect);
    }

    // If no option is visible, hide the label.
    bool shouldHideLabel = ( !enableOriginal && !enableRawRaster && !enableCompressSelect && !enablePaletteSelect && !enablePixelFormatSelect );

    this->platformHeaderLabel->setVisible( !shouldHideLabel );

    this->enableOriginal = enableOriginal;
    this->enableRawRaster = enableRawRaster;
    this->enableCompressSelect = enableCompressSelect;
    this->enablePaletteSelect = enablePaletteSelect;
    this->enablePixelFormatSelect = enablePixelFormatSelect;

    // Fill in fields depending on capabilities.
    if (enableCompressSelect)
    {
        QString currentText = this->platformCompressionSelectProp->currentText();

        this->platformCompressionSelectProp->clear();

        if (supportsDXT1)
        {
            this->platformCompressionSelectProp->addItem("DXT1");
        }

        if (supportsDXT2)
        {
            this->platformCompressionSelectProp->addItem("DXT2");
        }

        if (supportsDXT3)
        {
            this->platformCompressionSelectProp->addItem("DXT3");
        }

        if (supportsDXT4)
        {
            this->platformCompressionSelectProp->addItem("DXT4");
        }

        if (supportsDXT5)
        {
            this->platformCompressionSelectProp->addItem("DXT5");
        }

        this->platformCompressionSelectProp->setCurrentText( currentText );
    }

    // If none of the visible toggles are selected, select the first visible toggle.
    bool anyToggle = false;

    if (this->platformRawRasterToggle->isVisible() && this->platformRawRasterToggle->isChecked())
    {
        anyToggle = true;
    }

    if (!anyToggle)
    {
        if (this->platformCompressionToggle->isVisible() && this->platformCompressionToggle->isChecked())
        {
            anyToggle = true;
        }
    }

    if (!anyToggle)
    {
        if (this->platformPaletteToggle->isVisible() && this->platformPaletteToggle->isChecked())
        {
            anyToggle = true;
        }
    }

    if (!anyToggle)
    {
        if (this->platformOriginalToggle->isVisible() && this->platformOriginalToggle->isChecked())
        {
            anyToggle = true;
        }
    }

    // Now select the first one if we still have no selected toggle.
    if (!anyToggle)
    {
        bool selectedToggle = false;

        if (!selectedToggle)
        {
            if ( this->platformOriginalToggle->isVisible() )
            {
                this->platformOriginalToggle->setChecked( true );

                selectedToggle = true;
            }
        }

        if (!selectedToggle)
        {
            if (this->platformRawRasterToggle->isVisible())
            {
                this->platformRawRasterToggle->setChecked(true);

                selectedToggle = true;
            }
        }

        if (!selectedToggle)
        {
            if (this->platformCompressionToggle->isVisible())
            {
                this->platformCompressionToggle->setChecked(true);

                selectedToggle = true;
            }
        }

        if (!selectedToggle)
        {
            if (this->platformPaletteToggle->isVisible())
            {
                this->platformPaletteToggle->setChecked(true);

                selectedToggle = true;
            }
        }

        if (!selectedToggle)
        {
            if (this->platformOriginalToggle->isVisible())
            {
                this->platformOriginalToggle->setChecked(true);

                selectedToggle = true;
            }
        }

        // Well, we do not _have_ to select one.
    }

    // Raster settings update.
    {
        rw::Raster *origRaster = nullptr;

        // The user wants to know about the original raster format, so display an info string.
        if (hasPreview)
        {
            origRaster = this->platformOrigRaster;

            this->previewInfoLabel->setVisible( true );
            this->previewInfoLabel->setText( TexInfoWidget::getDefaultRasterInfoString( origRaster ) );
        }
        else
        {
            this->previewInfoLabel->setVisible( false );
        }

        // If we still want a good start setting, we can now determine it.
        if (hasPreview && this->wantsGoodPlatformSetting)
        {
            // Initially set the configuration that is best for the image.
            // This is what the user normally wants anyway.
            bool hasSet = false;

            if (origRaster->isCompressed())
            {
                // If the raster is DXT compression, we can select it.
                rw::eCompressionType comprType = origRaster->getCompressionFormat();

                bool hasFound = false;

                if (comprType == rw::RWCOMPRESS_DXT1)
                {
                    this->platformCompressionSelectProp->setCurrentText("DXT1");

                    hasFound = true;
                }
                else if (comprType == rw::RWCOMPRESS_DXT2)
                {
                    this->platformCompressionSelectProp->setCurrentText("DXT2");

                    hasFound = true;
                }
                else if (comprType == rw::RWCOMPRESS_DXT3)
                {
                    this->platformCompressionSelectProp->setCurrentText("DXT3");

                    hasFound = true;
                }
                else if (comprType == rw::RWCOMPRESS_DXT4)
                {
                    this->platformCompressionSelectProp->setCurrentText("DXT4");

                    hasFound = true;
                }
                else if (comprType == rw::RWCOMPRESS_DXT5)
                {
                    this->platformCompressionSelectProp->setCurrentText("DXT5");

                    hasFound = true;
                }

                if (hasFound)
                {
                    this->platformCompressionToggle->setChecked(true);

                    hasSet = true;
                }
            }
            else
            {
                // Set palette type and raster format, if available.
                {
                    rw::ePaletteType paletteType = origRaster->getPaletteType();

                    bool hasFound = false;

                    if (paletteType == rw::PALETTE_4BIT || paletteType == rw::PALETTE_4BIT_LSB)
                    {
                        this->platformPaletteSelectProp->setCurrentText("PAL4");

                        hasFound = true;
                    }
                    else if (paletteType == rw::PALETTE_8BIT)
                    {
                        this->platformPaletteSelectProp->setCurrentText("PAL8");

                        hasFound = true;
                    }

                    if (hasFound && !hasSet)
                    {
                        this->platformPaletteToggle->setChecked(true);

                        hasSet = true;
                    }
                }

                // Now raster format.
                {
                    rw::eRasterFormat rasterFormat = origRaster->getRasterFormat();

                    if (rasterFormat != rw::RASTER_DEFAULT)
                    {
                        this->platformPixelFormatSelectProp->setCurrentText(
                            rw::GetRasterFormatStandardName(rasterFormat)
                            );

                        if (!hasSet)
                        {
                            this->platformRawRasterToggle->setChecked(true);

                            hasSet = true;
                        }
                    }
                }
            }

            // If nothing was select, we are best off original.
            if (!hasSet)
            {
                this->platformOriginalToggle->setChecked(true);
            }

            // Done.
            this->wantsGoodPlatformSetting = false;
        }
    }

    // Now the user is given control again.
    this->adjustingProgrammaticConfig = false;

    // We want to create a raster special to the configuration.
    this->createRasterForConfiguration();
}

void TexAddDialog::loadPlatformOriginal(void)
{
    // If we have a converted raster, release it.
    this->releaseConvRaster();

    this->tasktoken_loadPlatformOriginal.Cancel( true );

    // Determine some properties.
    bool wantsToAdjustRaster = true;
    bool adjustTextureChunksOnImport = this->mainWnd->adjustTextureChunksOnImport;

    MainWindow *mainWnd = this->mainWnd;
    texAddImageImportMethods impMeth( mainWnd, this->GetCurrentPlatform() );

    if ( rw::TexDictionary *texDict = mainWnd->getCurrentTXD() )
    {
        impMeth.setRWVersion( texDict->GetEngineVersion() );
    }

    eCreationType dialog_type = this->dialog_type;
    eImportExpectation img_exp = this->img_exp;
    QString imgPath = this->imgPath;

    this->startLoadingAnimation();

    this->tasktoken_loadPlatformOriginal = mainWnd->taskSystem.LaunchTaskL(
        "texadd-load-platform-original",
        [this, mainWnd, dialog_type, img_exp, impMeth = std::move(impMeth), imgPath = std::move(imgPath), wantsToAdjustRaster, adjustTextureChunksOnImport] ( void ) mutable
        {
            rw::Interface *rwEngine = mainWnd->GetEngine();

            try
            {
                // Depends on what we have.
                if (dialog_type == CREATE_IMGPATH)
                {
                    // Open a stream to the image data.
                    rw::rwStaticString <wchar_t> unicodePathToImage = qt_to_widerw( imgPath );

                    rw::streamConstructionFileParamW_t wparam( unicodePathToImage.GetConstString() );

                    rw::StreamPtr imgStream = rwEngine->CreateStream(rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_READONLY, &wparam);

                    if (imgStream.is_good())
                    {
                        // Load it.
                        imageImportMethods::loadActionResult load_result;

                        bool couldLoad = impMeth.LoadImage( imgStream, img_exp, load_result );

                        if ( couldLoad )
                        {
                            rw::RasterPtr texRaster = std::move( load_result.texRaster );
                            rw::ObjectPtr <rw::TextureBase> texHandle = std::move( load_result.texHandle );

                            // Proceed loading the stuff.
                            if ( texHandle.is_good() )
                            {
                                // Put the raster into the correct platform, if wanted.
                                // This is because textures could have come with their own configuration.
                                // It is unlikely to be a problem for casual rasters.
                                if ( wantsToAdjustRaster )
                                {
                                    rw::rwStaticString <char> ansiPlatformName = qt_to_ansirw( impMeth.platformName );

                                    rw::ConvertRasterTo( texRaster, ansiPlatformName.GetConstString() );
                                }

                                // Also adjust the texture version.
                                if ( adjustTextureChunksOnImport )
                                {
                                    if ( impMeth.has_rwversion )
                                    {
                                        texHandle->SetEngineVersion( impMeth.rwversion );
                                    }
                                }
                            }

                            mainWnd->taskSystem.CurrentTaskSetPostL(
                                [this, texRaster = std::move(texRaster), texHandle = std::move(texHandle), wantsToAdjustRaster] ( void ) mutable
                            {
                                if ( texHandle.is_good() && !wantsToAdjustRaster )
                                {
                                    // We can update the platform here, without problems.
                                    this->SetCurrentPlatform( texRaster->getNativeDataTypeName() );
                                }

                                // Store this raster.
                                // Since it comes with a special reference already, we do not have to cast one ourselves.
                                this->platformOrigRaster = std::move( texRaster );

                                // If there was a texture, we have to remember it too.
                                // It may contain unique properties.
                                this->texHandle = std::move( texHandle );

                                this->hasPlatformOriginal = true;

                                this->OnUpdatePlatformOriginal();
                            });
                        }
                    }
                }
                else if (dialog_type == CREATE_RASTER)
                {
                    // We always have a platform original.
                    mainWnd->taskSystem.CurrentTaskSetPostL(
                        [this]
                        {
                            this->hasPlatformOriginal = true;

                            this->OnUpdatePlatformOriginal();
                        }
                    );
                }
            }
            catch (rw::RwException& err)
            {
                // We do not care.
                // We simply failed to get a preview.

                // Probably should tell the user about this error, so we can fix it.
                mainWnd->asyncLog(MAGIC_TEXT("General.PreviewFail").arg(wide_to_qt(rw::DescribeException( rwEngine, err ))), LOGMSG_ERROR);
                throw;
            }
        },
        [this]
        {
            this->hasPlatformOriginal = false;
            this->stopLoadingAnimation();

            this->OnUpdatePlatformOriginal();
        }
    );
}

void TexAddDialog::updatePreviewWidget(void)
{
}

void TexAddDialog::createRasterForConfiguration(void)
{
    // Release information about old preview.
    this->ClearPreview();

    // If we have no preview then just bail.
    if (this->hasPlatformOriginal == false)
        return;

    // This function prepares the raster that will be given to the texture dictionary.

    rw::eCompressionType compressionType = rw::RWCOMPRESS_NONE;

    rw::eRasterFormat rasterFormat = rw::RASTER_DEFAULT;
    rw::ePaletteType paletteType = rw::PALETTE_NONE;

    try
    {
        bool keepOriginal = this->platformOriginalToggle->isChecked();

        if (!keepOriginal)
        {
            // Now for the properties.
            if (this->platformCompressionToggle->isChecked())
            {
                // We are a compressed format, so determine what we actually are.
                QString selectedCompression = this->platformCompressionSelectProp->currentText();

                if (selectedCompression == "DXT1")
                {
                    compressionType = rw::RWCOMPRESS_DXT1;
                }
                else if (selectedCompression == "DXT2")
                {
                    compressionType = rw::RWCOMPRESS_DXT2;
                }
                else if (selectedCompression == "DXT3")
                {
                    compressionType = rw::RWCOMPRESS_DXT3;
                }
                else if (selectedCompression == "DXT4")
                {
                    compressionType = rw::RWCOMPRESS_DXT4;
                }
                else if (selectedCompression == "DXT5")
                {
                    compressionType = rw::RWCOMPRESS_DXT5;
                }
                else
                {
                    throw std::exception(); //"invalid compression type selected"
                }

                rasterFormat = rw::RASTER_DEFAULT;
                paletteType = rw::PALETTE_NONE;
            }
            else
            {
                compressionType = rw::RWCOMPRESS_NONE;

                // Now we have a valid raster format selected in the pixel format combo box.
                // We kinda need one.
                if (this->enablePixelFormatSelect)
                {
                    QString formatName = this->platformPixelFormatSelectProp->currentText();

                    rw::rwStaticString <char> ansiFormatName = qt_to_ansirw( formatName );

                    rasterFormat = rw::FindRasterFormatByName(ansiFormatName.GetConstString());

                    if (rasterFormat == rw::RASTER_DEFAULT)
                    {
                        throw std::exception(); //"invalid pixel format selected"
                    }
                }

                // And then we need to know whether it should be a palette or not.
                if (this->platformPaletteToggle->isChecked())
                {
                    // Alright, then we have to fetch a valid palette type.
                    QString paletteName = this->platformPaletteSelectProp->currentText();

                    if (paletteName == "PAL4")
                    {
                        // TODO: some archictures might prefer the MSB version.
                        // we should detect that automatically!

                        paletteType = rw::PALETTE_4BIT;
                    }
                    else if (paletteName == "PAL8")
                    {
                        paletteType = rw::PALETTE_8BIT;
                    }
                    else
                    {
                        throw std::exception(); //"invalid palette type selected"
                    }
                }
                else
                {
                    paletteType = rw::PALETTE_NONE;
                }
            }
        }
    }
    catch ( ... )
    {
        // If we failed to push data to the output stage.
        this->mainWnd->txdLog->showError(MAGIC_TEXT("Modify.RasterConfigFail"));
    }

    rw::RasterPtr convRaster = rw::CloneRaster(this->platformOrigRaster);

    // Create the raster.
    this->tasktoken_generateConvRaster.Cancel( true );

    this->startLoadingAnimation();

    // Prepare variables for cancel safety.
    MainWindow *mainWnd = this->mainWnd;
    rw::rwStaticString <char> currentPlatform = qt_to_ansirw( this->GetCurrentPlatform() );

    this->tasktoken_generateConvRaster = mainWnd->taskSystem.LaunchTaskL(
        "texadd-gen-convraster",
        [this, mainWnd, currentPlatform = std::move( currentPlatform ), convRaster = std::move( convRaster ), compressionType, rasterFormat, paletteType] ( void ) mutable
        {
            try
            {
                // We must make sure that our raster is in the correct platform.
                rw::ConvertRasterTo(convRaster, currentPlatform.GetConstString());

                // Format the raster appropriately.
                {
                    if (compressionType != rw::RWCOMPRESS_NONE)
                    {
                        // If the raster is already compressed, we want to decompress it.
                        // Very, very bad practice, but we allow it.
                        {
                            rw::eCompressionType curCompressionType = convRaster->getCompressionFormat();

                            if ( curCompressionType != rw::RWCOMPRESS_NONE )
                            {
                                convRaster->convertToFormat( rw::RASTER_8888 );
                            }
                        }

                        // Just compress it.
                        convRaster->compressCustom(compressionType);
                    }
                    else if (rasterFormat != rw::RASTER_DEFAULT)
                    {
                        // We want a specialized format.
                        // Go ahead.
                        if (paletteType != rw::PALETTE_NONE)
                        {
                            // Palettize.
                            convRaster->convertToPalette(paletteType, rasterFormat);
                        }
                        else
                        {
                            // Let us convert to another format.
                            convRaster->convertToFormat(rasterFormat);
                        }
                    }
                }

                // Success!
                mainWnd->taskSystem.CurrentTaskSetPostL(
                    [this, convRaster = std::move(convRaster)] ( void ) mutable
                    {
                        // Set the new raster while releasing any old one.
                        this->releaseConvRaster();

                        this->convRaster = std::move( convRaster );

                        // Update the preview.
                        this->UpdatePreview();
                    }
                );
            }
            catch (rw::RwException& except)
            {
                mainWnd->asyncLog(MAGIC_TEXT("Modify.RasterCreateFail").arg(wide_to_qt(rw::DescribeException(convRaster->getEngine(), except))), LOGMSG_ERROR);
                throw;
            }
        },
        [this]
        {
            this->stopLoadingAnimation();
        }
    );
}

QComboBox* TexAddDialog::createPlatformSelectComboBox(MainWindow *mainWnd)
{
    QComboBox *platformComboBox = new QComboBox();

    // Fill out the combo box with available platforms.
    {
        rw::platformTypeNameList_t unsortedPlatforms = rw::GetAvailableNativeTextureTypes(mainWnd->rwEngine);

        // We want to sort the platforms by importance.
        rw::rwStaticVector <rw::rwStaticString <char>> platforms = PlatformImportanceSort( mainWnd, unsortedPlatforms );

        size_t numPlatforms = platforms.GetCount();

        for ( size_t n = 0; n < numPlatforms; n++ )
        {
            const rw::rwStaticString <char>& platName = platforms[ numPlatforms - 1 - n ];

            platformComboBox->addItem( ansi_to_qt( platName ) );
        }
    }

    return platformComboBox;
}

#define LEFTPANELADDDIALOGWIDTH 230

// We need an environment to handle helper stuff.
struct texAddDialogEnv
{
    inline void Initialize( MainWindow *mainWnd )
    {
        RegisterHelperWidget( mainWnd, "dxt_warning", eHelperTextType::DIALOG_WITH_TICK, "Modify.Help.DXTNotice" );
        RegisterHelperWidget( mainWnd, "pal_warning", eHelperTextType::DIALOG_WITH_TICK, "Modify.Help.PALNotice" );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        UnregisterHelperWidget( mainWnd, "pal_warning" );
        UnregisterHelperWidget( mainWnd, "dxt_warning" );
    }
};

static optional_struct_space <PluginDependantStructRegister <texAddDialogEnv, mainWindowFactory_t>> texAddDialogEnvRegister;

void InitializeTextureAddDialogEnv( void )
{
    texAddDialogEnvRegister.Construct( mainWindowFactory );
}

void ShutdownTextureAddDialogEnv( void )
{
    texAddDialogEnvRegister.Destroy();
}

inline QString calculateImageBaseName(QString fileName)
{
    // Determine the texture name.
    QFileInfo fileInfo(fileName);

    return fileInfo.baseName();
}

TexAddDialog::TexAddDialog(MainWindow *mainWnd, const dialogCreateParams& create_params, TexAddDialog::operationCallback_t cb)
    : QDialog(mainWnd)
{
    this->mainWnd = mainWnd;

    this->adjustingProgrammaticConfig = false;

    // Remember for language updates.
    this->titleBarToken = create_params.actionDesc;

    this->dialog_type = create_params.type;

    this->cb = std::move( cb );

    this->setAttribute(Qt::WA_DeleteOnClose);
    // does not have to be modal anymore because we properly texture backlinks and stuff.

    this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    this->has_applied_item = false;

    // Initialize the loading animation.
    {
        NativeExecutive::CExecutiveManager *execMan = (NativeExecutive::CExecutiveManager*)rw::GetThreadingNativeManager( mainWnd->GetEngine() );

        statusBarAnimationMembers *animEnv = statusBarAnimationMembersRegister.get().GetPluginStruct( mainWnd );

        this->loadingAnimation.Construct( execMan, &animEnv->previewLoadingFrames );
    }

    try
    {
        // Create a raster handle that will hold platform original data.
        this->hasPlatformOriginal = false;
        this->texHandle = nullptr;
        this->convRaster = nullptr;

        if (this->dialog_type == CREATE_IMGPATH)
        {
            QString imgPath = create_params.img_path.imgPath;

            // Determine what kind of path we have and deduce what the user expects it to be.
            // This way we can determine what messages the user should receive and when.
            {
                std::wstring wImgPath = imgPath.toStdWString();

                filePath extension;

                FileSystem::GetFileNameItem <FileSysCommonAllocator> ( wImgPath.c_str(), true, nullptr, &extension );

                this->img_exp = getRecommendedImageImportExpectation( extension );
            }

            // We want to load the raster on demand.
            this->platformOrigRaster = nullptr;

            this->imgPath = std::move( imgPath );
        }
        else if (this->dialog_type == CREATE_RASTER)
        {
            this->platformOrigRaster = rw::AcquireRaster(create_params.orig_raster.tex->GetRaster());
        }

        // Set proper defaults.
        this->enableOriginal = true;
        this->enableRawRaster = true;
        this->enableCompressSelect = true;
        this->enablePaletteSelect = true;
        this->enablePixelFormatSelect = true;

        this->hasConfidentPlatform = false;

        this->wantsGoodPlatformSetting = true;

        // Calculate an appropriate texture name.
        QString textureBaseName;
        QString textureMaskName;

        if (this->dialog_type == CREATE_IMGPATH)
        {
            textureBaseName = calculateImageBaseName(this->imgPath);

            // screw mask name.
        }
        else if (this->dialog_type == CREATE_RASTER)
        {
            textureBaseName = ansi_to_qt(create_params.orig_raster.tex->GetName());
            textureMaskName = ansi_to_qt(create_params.orig_raster.tex->GetMaskName());
        }

        if (const QString *overwriteTexName = create_params.overwriteTexName)
        {
            textureBaseName = *overwriteTexName;
        }

        QString curPlatformText;

        // Create our GUI interface.
        MagicLayout<QHBoxLayout> layout;
        layout.root->setAlignment(Qt::AlignTop);

        QVBoxLayout *leftPanelLayout = new QVBoxLayout();
        leftPanelLayout->setAlignment(Qt::AlignTop);
        { // Top Left (platform options)
          //QWidget *leftTopWidget = new QWidget();
            { // Names and Platform
                texture_name_validator *texNameValid = new texture_name_validator( this );

                QFormLayout *leftTopLayout = new QFormLayout();
                MagicLineEdit *texNameEdit = new MagicLineEdit(textureBaseName);
                texNameEdit->setMaxLength(_recommendedPlatformMaxName);
                //texNameEdit->setFixedWidth(LEFTPANELADDDIALOGWIDTH);
                texNameEdit->setFixedHeight(texNameEdit->sizeHint().height());
                texNameEdit->setValidator( texNameValid );
                this->textureNameEdit = texNameEdit;
                leftTopLayout->addRow(CreateLabelL("Modify.TexName"), texNameEdit);
                if (_enableMaskName)
                {
                    MagicLineEdit *texMaskNameEdit = new MagicLineEdit(textureMaskName);
                    //texMaskNameEdit->setFixedWidth(LEFTPANELADDDIALOGWIDTH);
                    texMaskNameEdit->setFixedHeight(texMaskNameEdit->sizeHint().height());
                    texMaskNameEdit->setMaxLength(_recommendedPlatformMaxName);
                    texMaskNameEdit->setValidator( texNameValid );
                    leftTopLayout->addRow(CreateLabelL("Modify.MskName"), texMaskNameEdit);
                    this->textureMaskNameEdit = texMaskNameEdit;
                }
                else
                {
                    this->textureMaskNameEdit = nullptr;
                }
                // If the current TXD already has a platform, we disable editing this platform and simply use it.
                bool lockdownPlatform = ( _lockdownPlatform && mainWnd->lockDownTXDPlatform );

                QString currentForcedPlatform = mainWnd->GetCurrentPlatform();

                this->hasConfidentPlatform = ( !currentForcedPlatform.isEmpty() );

                QWidget *platformDisplayWidget;
                if (lockdownPlatform == false || currentForcedPlatform.isEmpty())
                {
                    QComboBox *platformComboBox = createPlatformSelectComboBox(mainWnd);
                    //platformComboBox->setFixedWidth(LEFTPANELADDDIALOGWIDTH);

                    connect(platformComboBox, (void (QComboBox::*)(const QString&))&QComboBox::activated, this, &TexAddDialog::OnPlatformSelect);

                    platformDisplayWidget = platformComboBox;
                    if (!currentForcedPlatform.isEmpty())
                    {
                        platformComboBox->setCurrentText(currentForcedPlatform);
                    }
                    curPlatformText = platformComboBox->currentText();
                }
                else
                {
                    // We do not want to allow editing.
                    MagicLineEdit *platformDisplayEdit = new MagicLineEdit();
                    //platformDisplayEdit->setFixedWidth(LEFTPANELADDDIALOGWIDTH);
                    platformDisplayEdit->setDisabled(true);

                    if ( currentForcedPlatform != nullptr )
                    {
                        platformDisplayEdit->setText( currentForcedPlatform );
                    }

                    platformDisplayWidget = platformDisplayEdit;
                    curPlatformText = platformDisplayEdit->text();
                }
                this->platformSelectWidget = platformDisplayWidget;
                leftTopLayout->addRow(CreateLabelL("Modify.Plat"), platformDisplayWidget);

                this->platformHeaderLabel = CreateLabelL("Modify.RasFmt");

                leftTopLayout->addRow(platformHeaderLabel);

                //leftTopWidget->setLayout(leftTopLayout);
                //leftTopWidget->setObjectName("background_1");
                leftPanelLayout->addLayout(leftTopLayout);

            }
            //leftPanelLayout->addWidget(leftTopWidget);

            QFormLayout *groupContentFormLayout = new QFormLayout();
            { // Platform properties

                this->platformPropForm = groupContentFormLayout;
                QRadioButton *origRasterToggle = CreateRadioButtonL("Modify.Origin");

                connect(origRasterToggle, &QRadioButton::toggled, this, &TexAddDialog::OnPlatformFormatTypeToggle);

                this->platformOriginalToggle = origRasterToggle;
                groupContentFormLayout->addRow(origRasterToggle);
                QRadioButton *rawRasterToggle = CreateRadioButtonL("Modify.RawRas");
                this->platformRawRasterToggle = rawRasterToggle;
                rawRasterToggle->setChecked(true);

                connect(rawRasterToggle, &QRadioButton::toggled, this, &TexAddDialog::OnPlatformFormatTypeToggle);

                groupContentFormLayout->addRow(rawRasterToggle);
                this->platformRawRasterProp = rawRasterToggle;
                QRadioButton *compressionFormatToggle = CreateRadioButtonL("Modify.Comp");
                this->platformCompressionToggle = compressionFormatToggle;

                connect(compressionFormatToggle, &QRadioButton::toggled, this, &TexAddDialog::OnPlatformFormatTypeToggle);

                QComboBox *compressionFormatSelect = new QComboBox();
                //compressionFormatSelect->setFixedWidth(LEFTPANELADDDIALOGWIDTH - 18);

                connect(compressionFormatSelect, (void (QComboBox::*)(const QString&))&QComboBox::activated, this, &TexAddDialog::OnTextureCompressionSeelct);

                groupContentFormLayout->addRow(compressionFormatToggle, compressionFormatSelect);
                this->platformCompressionSelectProp = compressionFormatSelect;
                QRadioButton *paletteFormatToggle = CreateRadioButtonL("Modify.Pal");
                this->platformPaletteToggle = paletteFormatToggle;

                connect(paletteFormatToggle, &QRadioButton::toggled, this, &TexAddDialog::OnPlatformFormatTypeToggle);

                QComboBox *paletteFormatSelect = new QComboBox();
                //paletteFormatSelect->setFixedWidth(LEFTPANELADDDIALOGWIDTH - 18);
                paletteFormatSelect->addItem("PAL4");
                paletteFormatSelect->addItem("PAL8");

                connect(paletteFormatSelect, (void (QComboBox::*)(const QString&))&QComboBox::activated, this, &TexAddDialog::OnTexturePaletteTypeSelect);

                groupContentFormLayout->addRow(paletteFormatToggle, paletteFormatSelect);
                this->platformPaletteSelectProp = paletteFormatSelect;
                QComboBox *pixelFormatSelect = new QComboBox();
                //pixelFormatSelect->setFixedWidth(LEFTPANELADDDIALOGWIDTH - 18);

                // TODO: add API to fetch actually supported raster formats for a native texture.
                // even though RenderWare may have added a bunch of raster formats, the native textures
                // are completely liberal in implementing any or not.
                pixelFormatSelect->addItem(rw::GetRasterFormatStandardName(rw::RASTER_1555));
                pixelFormatSelect->addItem(rw::GetRasterFormatStandardName(rw::RASTER_565));
                pixelFormatSelect->addItem(rw::GetRasterFormatStandardName(rw::RASTER_4444));
                pixelFormatSelect->addItem(rw::GetRasterFormatStandardName(rw::RASTER_LUM));
                pixelFormatSelect->addItem(rw::GetRasterFormatStandardName(rw::RASTER_8888));
                pixelFormatSelect->addItem(rw::GetRasterFormatStandardName(rw::RASTER_888));
                pixelFormatSelect->addItem(rw::GetRasterFormatStandardName(rw::RASTER_555));
                pixelFormatSelect->addItem(rw::GetRasterFormatStandardName(rw::RASTER_LUM_ALPHA));

                connect(pixelFormatSelect, (void (QComboBox::*)(const QString&))&QComboBox::activated, this, &TexAddDialog::OnTexturePixelFormatSelect);

                groupContentFormLayout->addRow(CreateLabelL("Modify.PixFmt"), pixelFormatSelect);
                this->platformPixelFormatSelectProp = pixelFormatSelect;
            }

            leftPanelLayout->addLayout(groupContentFormLayout);

            leftPanelLayout->addSpacing(12);

            { // Add some basic properties that exist no matter what platform.
                QCheckBox *generateMipmapsToggle = CreateCheckBoxL("Modify.GenML");
                generateMipmapsToggle->setChecked( mainWnd->addImageGenMipmaps );

                this->propGenerateMipmaps = generateMipmapsToggle;

                leftPanelLayout->addWidget(generateMipmapsToggle);
            }
        }
        layout.top->addLayout(leftPanelLayout);

        QVBoxLayout *rightPanelLayout = new QVBoxLayout();
        rightPanelLayout->setAlignment(Qt::AlignHCenter);
        { // Top right (preview options, preview image)
            QHBoxLayout *rightTopPanelLayout = new QHBoxLayout();
            scaledPreviewCheckBox = CreateCheckBoxL("Modify.Scaled");
            scaledPreviewCheckBox->setChecked(true);

            connect(scaledPreviewCheckBox, &QCheckBox::stateChanged, this, &TexAddDialog::OnScalePreviewStateChanged);

            fillPreviewCheckBox = CreateCheckBoxL("Modify.Fill");

            connect(fillPreviewCheckBox, &QCheckBox::stateChanged, this, &TexAddDialog::OnFillPreviewStateChanged);

            backgroundForPreviewCheckBox = CreateCheckBoxL("Modify.Bckgr");

            connect(backgroundForPreviewCheckBox, &QCheckBox::stateChanged, this, &TexAddDialog::OnPreviewBackgroundStateChanged);

            rightTopPanelLayout->addWidget(scaledPreviewCheckBox);
            rightTopPanelLayout->addWidget(fillPreviewCheckBox);
            rightTopPanelLayout->addWidget(backgroundForPreviewCheckBox);
            rightPanelLayout->addLayout(rightTopPanelLayout);

            this->previewView = new TexViewportWidget( mainWnd );
            this->previewView->setFrameShape(QFrame::NoFrame);
            this->previewView->setObjectName("background_2");

            this->loadingAnimation.get().setTargetWidget( this->previewView );

            rightPanelLayout->addWidget(this->previewView);

            this->previewInfoLabel = new QLabel();
            rightPanelLayout->addWidget(this->previewInfoLabel);

            rightPanelLayout->setAlignment(rightTopPanelLayout, Qt::AlignHCenter);
            rightPanelLayout->setAlignment(this->previewView, Qt::AlignHCenter);
            rightPanelLayout->setAlignment(this->previewInfoLabel, Qt::AlignHCenter);
        }
        layout.top->addLayout(rightPanelLayout);

        // Add control buttons at the bottom.
        QPushButton *cancelButton = CreateButtonL("Modify.Cancel");
        this->cancelButton = cancelButton;

        connect(cancelButton, &QPushButton::clicked, this, &TexAddDialog::OnCloseRequest);

        layout.bottom->addWidget(cancelButton);
        QPushButton *addButton = CreateButtonL(create_params.actionName);
        this->applyButton = addButton;

        connect(addButton, &QPushButton::clicked, this, &TexAddDialog::OnTextureAddRequest);

        layout.bottom->addWidget(addButton);

        this->setLayout(layout.root);

        // Do initial stuff.
        {
            this->OnUpdatePlatformOriginal();

            if (curPlatformText.isEmpty() == false)
            {
                this->OnPlatformSelect(curPlatformText);
            }

            this->UpdateAccessability();

            // Set focus on the apply button, so users can quickly add textures.
            this->applyButton->setDefault( true );

            // Setup the preview.
            this->scaledPreviewCheckBox->setChecked( mainWnd->texaddViewportScaled );
            this->fillPreviewCheckBox->setChecked( mainWnd->texaddViewportFill );
            this->backgroundForPreviewCheckBox->setChecked( mainWnd->texaddViewportBackground );
        }

        mainWnd->RegisterThemeItem( this );

        RegisterTextLocalizationItem( this );
    }
    catch( ... )
    {
        this->loadingAnimation.Destroy();
        throw;
    }
}

TexAddDialog::~TexAddDialog( void )
{
    MainWindow *mainWnd = this->mainWnd;

    UnregisterTextLocalizationItem( this );

    mainWnd->UnregisterThemeItem( this );

    // Make sure to cancel any async activity based on this dialog.
    this->tasktoken_loadPlatformOriginal.Cancel( true );
    this->tasktoken_generatePreview.Cancel( true );
    this->tasktoken_generateConvRaster.Cancel( true );
    this->tasktoken_prepareConvRaster.Cancel( true );

    this->loadingAnimation.Destroy();

    // We want to save some persistence related configurations.
    mainWnd->texaddViewportScaled = this->scaledPreviewCheckBox->isChecked();
    mainWnd->texaddViewportFill = this->fillPreviewCheckBox->isChecked();
    mainWnd->texaddViewportBackground = this->backgroundForPreviewCheckBox->isChecked();

    // Remember properties that count for any raster format.
    mainWnd->addImageGenMipmaps = this->propGenerateMipmaps->isChecked();
}

void TexAddDialog::updateContent( MainWindow *mainWnd )
{
    this->setWindowTitle(MAGIC_TEXT(this->titleBarToken));
}

void TexAddDialog::updateTheme( MainWindow *mainWnd )
{
    if ( this->loadingAnimation.get().isRunning() )
    {
        this->loadingAnimation.get().refreshPixmap();
    }
}

void TexAddDialog::UpdatePreview()
{
    rw::RasterPtr previewRaster = this->convRaster;
    if (previewRaster.is_good())
    {
        this->tasktoken_generatePreview.Cancel( true );

        // Prepare variables for cancel safety.
        MainWindow *mainWnd = this->mainWnd;

        this->tasktoken_generatePreview = mainWnd->taskSystem.LaunchTaskL(
            "texadd-preview-update",
            [this, mainWnd, previewRaster = std::move(previewRaster)] ( void ) mutable
            {
                int w, h;
                QPixmap pixmap;

                rw::Interface *rwEngine = previewRaster->getEngine();

                try
                {
                    NativeExecutive::CExecutiveManager *execMan = (NativeExecutive::CExecutiveManager*)rw::GetThreadingNativeManager( rwEngine );

                    // Put the contents of the platform original into the preview widget.
                    // We want to transform the raster into a bitmap, basically.
                    pixmap = convertRWBitmapToQPixmap( execMan, previewRaster->getBitmap() );

                    w = pixmap.width(), h = pixmap.height();
                }
                catch (rw::RwException& except) {
                    mainWnd->asyncLog(MAGIC_TEXT("General.PreviewFail").arg(wide_to_qt(rw::DescribeException(rwEngine, except))), LOGMSG_ERROR);
                    throw;
                }

                mainWnd->taskSystem.CurrentTaskSetPostL(
                    [this, pixmap = std::move(pixmap), w, h] ( void ) mutable
                {
                    // Clear any loading animation.
                    this->stopLoadingAnimation();

                    // Update the content image.
                    this->previewView->setTexturePixmap( std::move(pixmap) );
                });
            },
            [this]
            {
                // If there was any animation then stop it.
                this->stopLoadingAnimation();

                this->previewView->clearContent();
                this->previewView->setContentSize( 300, 300 );
            }
        );
    }
    else
    {
        ClearPreview();
    }
}

void TexAddDialog::ClearPreview() {
    // If there was any animation then stop it.
    this->stopLoadingAnimation();

    this->tasktoken_generatePreview.Cancel( true );
    this->previewView->clearContent();
    this->previewView->setContentSize( 300, 300 );
}

void TexAddDialog::UpdateAccessability(void)
{
    // We have to disable or enable certain platform property selectable fields if you toggle a certain format type.
    // This is to guide the user into the right direction, that a native texture cannot have multiple format types at once.

    bool wantsPixelFormatAccess = false;
    bool wantsCompressionAccess = false;
    bool wantsPaletteAccess = false;

    bool has_applied_already = this->has_applied_item;

    if ( !has_applied_already )
    {
        if (this->platformOriginalToggle->isChecked())
        {
            // We want nothing.
        }
        else if (this->platformRawRasterToggle->isChecked())
        {
            wantsPixelFormatAccess = true;
        }
        else if (this->platformCompressionToggle->isChecked())
        {
            wantsCompressionAccess = true;
        }
        else if (this->platformPaletteToggle->isChecked())
        {
            wantsPixelFormatAccess = true;
            wantsPaletteAccess = true;
        }
    }

    // Now disable or enable stuff.
    this->platformPixelFormatSelectProp->setDisabled(!wantsPixelFormatAccess);
    this->platformCompressionSelectProp->setDisabled(!wantsCompressionAccess);
    this->platformPaletteSelectProp->setDisabled(!wantsPaletteAccess);

    // If we have already applied then we can just remove access to the main components.
    this->textureNameEdit->setDisabled( has_applied_already );

    if ( MagicLineEdit *textureMaskNameEdit = this->textureMaskNameEdit )
    {
        textureMaskNameEdit->setDisabled( has_applied_already );
    }

    this->propGenerateMipmaps->setDisabled( has_applied_already );

    // Also the toggles.
    this->platformOriginalToggle->setDisabled( has_applied_already );
    this->platformRawRasterToggle->setDisabled( has_applied_already );
    this->platformCompressionToggle->setDisabled( has_applied_already );
    this->platformPaletteToggle->setDisabled( has_applied_already );

    // TODO: maybe clear combo boxes aswell?
}

void TexAddDialog::OnPlatformFormatTypeToggle(bool checked)
{
    // If we have already applied an item then we do not care.
    if ( this->has_applied_item )
        return;

    if (checked != true)
        return;

    // Depending on the thing we clicked, we want to send some help text.
    if ( !this->adjustingProgrammaticConfig )
    {
        QObject *clickedOn = sender();

        if ( clickedOn == this->platformCompressionToggle )
        {
            TriggerHelperWidget( this->mainWnd, "dxt_warning", this );
        }
        else if ( clickedOn == this->platformPaletteToggle )
        {
            TriggerHelperWidget( this->mainWnd, "pal_warning", this );
        }
    }

    // Since we switched the platform format type, we have to adjust the accessability.
    // The accessability change must not swap items around on the GUI. Rather it should
    // disable items that make no sense.
    this->UpdateAccessability();

    // Since we switched the format type, the texture encoding has changed.
    // Update the preview.
    this->createRasterForConfiguration();
}

void TexAddDialog::OnTextureCompressionSeelct(const QString& newCompression)
{
    // If we have already applied an item then we do not care.
    if ( this->has_applied_item )
        return;

    this->createRasterForConfiguration();
}

void TexAddDialog::OnTexturePaletteTypeSelect(const QString& newPaletteType)
{
    // If we have already applied an item then we do not care.
    if ( this->has_applied_item )
        return;

    this->createRasterForConfiguration();
}

void TexAddDialog::OnTexturePixelFormatSelect(const QString& newPixelFormat)
{
    // If we have already applied an item then we do not care.
    if ( this->has_applied_item )
        return;

    this->createRasterForConfiguration();
}

void TexAddDialog::OnPlatformSelect(const QString& _)
{
    // If we have already applied an item then we do not care.
    if ( this->has_applied_item )
        return;

    // Update what options make sense to the user.
    this->UpdateAccessability();

    // Reload the preview image with what the platform wants us to see.
    this->loadPlatformOriginal();   // ALLOWED TO CHANGE THE PLATFORM.
}

void TexAddDialog::OnTextureAddRequest(bool checked)
{
    // This is where we want to go.
    // Decide the format that the runtime has requested.

    if ( this->has_applied_item )
        return;

    rw::RasterPtr convRaster = std::move( this->convRaster );

    if (convRaster.is_good() == false)
    {
        // Just ignore this.
        return;
    }

    rw::ObjectPtr texHandle = std::move( this->texHandle );

    // Disable any further add requests.
    this->applyButton->setDisabled( true );

    this->has_applied_item = true;

    // Disable further GUI.
    this->UpdateAccessability();

    this->platformSelectWidget->setDisabled( true );

    // Prepare and add the raster/texture.
    bool generateMipmaps = this->propGenerateMipmaps->isChecked();

    this->tasktoken_prepareConvRaster.Cancel( true );

    // Prepare variables for cancel safety.
    MainWindow *mainWnd = this->mainWnd;

    this->tasktoken_prepareConvRaster = mainWnd->taskSystem.LaunchTaskL(
        "texadd-prepare-convraster",
        [this, mainWnd, generateMipmaps, convRaster = std::move(convRaster), texHandle = std::move(texHandle)] ( void ) mutable
        {
            // Maybe generate mipmaps.
            if ( generateMipmaps )
            {
                try
                {
                    convRaster->generateMipmaps( 0xFFFFFFFF, rw::MIPMAPGEN_DEFAULT );
                }
                catch( rw::RwException& )
                {
                    // We do not have to be able to generate mipmaps.
                }
            }

            mainWnd->taskSystem.CurrentTaskSetPostL(
                [this, convRaster = std::move(convRaster), texHandle = std::move(texHandle)] ( void ) mutable
            {
                texAddOperation desc;

                // We can either add a raster or a texture chunk.
                rw::rwStaticString <char> texName = qt_to_ansirw( this->textureNameEdit->text() );
                rw::rwStaticString <char> maskName;

                if ( this->textureMaskNameEdit )
                {
                    maskName = qt_to_ansirw( this->textureMaskNameEdit->text() );
                }

                if ( texHandle.is_good() )
                {
                    // Initialize the texture handle.
                    RwTextureAssignNewRaster( texHandle, convRaster, texName, maskName );

                    desc.add_texture.texHandle = texHandle;

                    desc.add_type = texAddOperation::ADD_TEXCHUNK;
                }
                else
                {
                    desc.add_raster.texName = std::move( texName );
                    desc.add_raster.maskName = std::move( maskName );

                    desc.add_raster.raster = convRaster;

                    desc.add_type = texAddOperation::ADD_RASTER;
                }

                this->cb(desc);

                // Close ourselves, because we fulfilled the request successfully.
                this->close();
            });
        }
    );
}

void TexAddDialog::OnCloseRequest(bool checked)
{
    // The user doesnt want to do it anymore.
    this->close();
}

void TexAddDialog::OnPreviewBackgroundStateChanged(int state)
{
    this->previewView->setShowBackground( state != Qt::Unchecked );
}

void TexAddDialog::OnScalePreviewStateChanged(int state) {
    if (state == Qt::Unchecked && this->fillPreviewCheckBox->isChecked()) {
        this->fillPreviewCheckBox->setChecked(false);
    }
    else {
        this->previewView->setShowFullImage( state != Qt::Unchecked || this->fillPreviewCheckBox->isChecked() );
    }
}

void TexAddDialog::OnFillPreviewStateChanged(int state) {
    if (state == Qt::Checked && !this->scaledPreviewCheckBox->isChecked()) {
        this->scaledPreviewCheckBox->setChecked(true);
    }
    else {
        this->previewView->setShowFullImage( state != Qt::Unchecked || this->scaledPreviewCheckBox->isChecked() );
    }
}
