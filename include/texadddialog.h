/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/texadddialog.h
*  PURPOSE:     Header of the texture add/modify dialog.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>

#include <functional>

#include "rwimageimporter.h"

#include "qtutils.h"

class MainWindow;

class TexAddDialog : public QDialog, public magicThemeAwareItem, public magicTextLocalizationItem
{
public:
    enum eCreationType
    {
        CREATE_IMGPATH,
        CREATE_RASTER
    };

    struct dialogCreateParams
    {
        inline dialogCreateParams(void)
        {
            this->overwriteTexName = nullptr;
        }

        QString actionDesc;
        QString actionName;
        eCreationType type;
        struct
        {
            QString imgPath;
        } img_path;
        struct
        {
            rw::TextureBase *tex;
        } orig_raster;

        // Optional properties.
        const QString *overwriteTexName;
    };

    struct texAddOperation
    {
        enum eAdditionType
        {
            ADD_RASTER,
            ADD_TEXCHUNK
        };
        eAdditionType add_type;

        struct
        {
            // Selected texture properties.
            rw::rwStaticString <char> texName;
            rw::rwStaticString <char> maskName;

            rw::Raster *raster;
        } add_raster;
        struct
        {
            rw::TextureBase *texHandle;
        } add_texture;
    };

    typedef std::function <void(const texAddOperation&)> operationCallback_t;

    TexAddDialog(MainWindow *mainWnd, const dialogCreateParams& create_params, operationCallback_t func);
    ~TexAddDialog(void);

private:
    void updateContent( MainWindow *mainWnd ) override;

    void startLoadingAnimation( void );
    void stopLoadingAnimation( void );

    void OnUpdatePlatformOriginal( void );
    
public:
    void loadPlatformOriginal(void);
    void updatePreviewWidget(void);

    void createRasterForConfiguration(void);

    // Helpers.
    static QComboBox* createPlatformSelectComboBox(MainWindow *mainWnd);

    static void RwTextureAssignNewRaster(
        rw::TextureBase *texHandle, rw::Raster *newRaster,
        const rw::rwStaticString <char>& texName, const rw::rwStaticString <char>& maskName
    );

private:
    void UpdatePreview();
    void ClearPreview();

    void releaseConvRaster(void);

    void UpdateAccessability(void);

    void SetCurrentPlatform( QString name );
    QString GetCurrentPlatform(void);

public slots:
    void OnTextureAddRequest(bool checked);
    void OnCloseRequest(bool checked);

    void OnPlatformSelect(const QString& newText);

    void OnPlatformFormatTypeToggle(bool checked);

    void OnTextureCompressionSeelct(const QString& newCompression);
    void OnTexturePaletteTypeSelect(const QString& newPaletteType);
    void OnTexturePixelFormatSelect(const QString& newPixelFormat);

    void OnPreviewBackgroundStateChanged(int state);
    void OnScalePreviewStateChanged(int state);
    void OnFillPreviewStateChanged(int state);

protected:
    void updateTheme( MainWindow *mainWnd ) override;

private:
    MainWindow *mainWnd;

    bool adjustingProgrammaticConfig;

    QString titleBarToken;

    eCreationType dialog_type;
    eImportExpectation img_exp;

    rw::RasterPtr platformOrigRaster;
    rw::ObjectPtr <rw::TextureBase> texHandle;  // if not NULL, then this texture will be used for import.
    rw::RasterPtr convRaster;
    bool hasPlatformOriginal;
    QPixmap pixelsToAdd;

    bool hasConfidentPlatform;
    bool wantsGoodPlatformSetting;

    MagicLineEdit *textureNameEdit;
    MagicLineEdit *textureMaskNameEdit;
    QWidget *platformSelectWidget;

    QFormLayout *platformPropForm;
    QLabel *platformHeaderLabel;
    QWidget *platformRawRasterProp;
    QComboBox *platformCompressionSelectProp;
    QComboBox *platformPaletteSelectProp;
    QComboBox *platformPixelFormatSelectProp;

    bool enableOriginal;
    bool enableRawRaster;
    bool enableCompressSelect;
    bool enablePaletteSelect;
    bool enablePixelFormatSelect;

    QRadioButton *platformOriginalToggle;
    QRadioButton *platformRawRasterToggle;
    QRadioButton *platformCompressionToggle;
    QRadioButton *platformPaletteToggle;

    // General properties.
    QCheckBox *propGenerateMipmaps;

    // Preview widget stuff.
    TexViewportWidget *previewView;
    QCheckBox *scaledPreviewCheckBox;
    QCheckBox *fillPreviewCheckBox;
    QCheckBox *backgroundForPreviewCheckBox;
    QLabel *previewInfoLabel;

    MagicParallelTasks::TaskToken tasktoken_loadPlatformOriginal;
    MagicParallelTasks::TaskToken tasktoken_generatePreview;
    MagicParallelTasks::TaskToken tasktoken_generateConvRaster;
    MagicParallelTasks::TaskToken tasktoken_prepareConvRaster;

    // For the loading animation.
    optional_struct_space <cyclingAnimation> loadingAnimation;

    // The buttons.
    QPushButton *cancelButton;
    QPushButton *applyButton;

    bool has_applied_item;

    operationCallback_t cb;

    QString imgPath;

    void clearTextureOriginal( void );

    rw::Raster* MakeRaster( void );
};
