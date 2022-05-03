/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/aboutdialog.cpp
*  PURPOSE:     Implementation of the About Us dialog.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include <QtGui/QMovie>

struct LabelDoNotUseCommercially : public QLabel, public magicTextLocalizationItem
{
    inline LabelDoNotUseCommercially( void )
    {
        RegisterTextLocalizationItem( this );
    }

    inline ~LabelDoNotUseCommercially( void )
    {
        UnregisterTextLocalizationItem( this );
    }

    void updateContent( MainWindow *mainWnd )
    {
        setText(
            MAGIC_TEXT( "Main.About.ComUse1" )
            + "\n\n" +
            MAGIC_TEXT( "Main.About.ComUse2")
        );
    }
};

AboutDialog::AboutDialog( MainWindow *mainWnd ) : QDialog( mainWnd )
{
    this->mainWnd = mainWnd;

    setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    setAttribute( Qt::WA_DeleteOnClose );

    // We should display all kinds of copyright information here.
    QVBoxLayout *rootLayout = new QVBoxLayout();

    this->setFixedWidth( 580 );

    QVBoxLayout *headerGroup = new QVBoxLayout();

    QLabel *mainLogoLabel = new QLabel();
    mainLogoLabel->setAlignment( Qt::AlignCenter );

    this->mainLogoLabel = mainLogoLabel;

    headerGroup->addWidget( mainLogoLabel );

    QLabel *labelCopyrightHolders = CreateLabelL( "Main.About.Credits" );

    labelCopyrightHolders->setObjectName( "labelCopyrightHolders" );

    labelCopyrightHolders->setAlignment( Qt::AlignCenter );

    headerGroup->addWidget( labelCopyrightHolders );

    //headerGroup->setContentsMargins( 0, 0, 0, 20 );

    rootLayout->addLayout( headerGroup );

    QLabel *labelDoNotUseCommercially = new LabelDoNotUseCommercially();
    labelDoNotUseCommercially->setWordWrap(true);

    labelDoNotUseCommercially->setAlignment(Qt::AlignCenter);

    rootLayout->addSpacing(10);

    rootLayout->addWidget( labelDoNotUseCommercially );

    rootLayout->addSpacing(15);

    // We need to thank our contributors :)
    QLabel *specialThanksHeader = CreateLabelL( "Main.About.SpecThx" );

    specialThanksHeader->setObjectName( "labelSpecThxHeader" );

    rootLayout->addWidget( specialThanksHeader, 0, Qt::AlignCenter );

    QHBoxLayout *specialThanksLayout = new QHBoxLayout();

    static const char *specialThanks1 =
        "Ash R. (GUI design help)\n" \
        "GodFather (translator)\n" \
        "Flame (support)\n";

    QLabel *labelSpecialThanks1 = new QLabel( specialThanks1 );

    labelSpecialThanks1->setObjectName( "labelSpecThx" );

    labelSpecialThanks1->setAlignment( Qt::AlignTop );

    specialThanksLayout->addWidget( labelSpecialThanks1, 0, Qt::AlignHCenter );

    static const char *specialThanks2 =
        "Vadim (tester)\n" \
        "Ss4gogeta0 (tester)\n" \
        "M4k3 (tester)";

    QLabel *labelSpecialThanks2 = new QLabel( specialThanks2 );

    labelSpecialThanks2->setObjectName( "labelSpecThx" );

    labelSpecialThanks2->setAlignment( Qt::AlignTop );

    specialThanksLayout->addWidget( labelSpecialThanks2, 0, Qt::AlignHCenter );

    rootLayout->addLayout( specialThanksLayout );

    rootLayout->addSpacing( 20 );

    // Add icons of important vendors.
    QHBoxLayout *vendorRowOne = new QHBoxLayout();

    vendorRowOne->setAlignment( Qt::AlignCenter );

    vendorRowOne->setSpacing(10);

    QLabel *rwLogo = new QLabel();
    rwLogo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/renderwarelogo.png")));
    rwLogo->setToolTip("RenderWare Graphics");

    vendorRowOne->addWidget(rwLogo);

    // Do some smart stuff: only include native texture images if rwlib actually supports the things.
    rw::Interface *rwEngine = mainWnd->GetEngine();

    if ( rw::IsNativeTexture( rwEngine, "AMDCompress" ) )
    {
        QLabel *amdLogo = new QLabel();
        amdLogo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/amdlogo.png")));
        amdLogo->setToolTip("AMD");

        vendorRowOne->addWidget(amdLogo);
    }

    if ( rw::IsNativeTexture( rwEngine, "PowerVR" ) )
    {
        QLabel *powervrLogo = new QLabel();
        powervrLogo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/powervrlogo.png")));
        powervrLogo->setToolTip("PowerVR");

        vendorRowOne->addWidget(powervrLogo);
    }

    rootLayout->addLayout(vendorRowOne);

    QHBoxLayout *vendorRowTwo = new QHBoxLayout();

    vendorRowTwo->setAlignment( Qt::AlignCenter );

    vendorRowTwo->setSpacing(10);

    if ( rw::IsNativeTexture( rwEngine, "XBOX" ) )
    {
        QLabel *xboxLogo = new QLabel();
        xboxLogo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/xboxlogo.png")));
        xboxLogo->setToolTip("XBOX");

        vendorRowTwo->addWidget(xboxLogo);
    }

    QLabel *pngquantLogo = new QLabel();
    pngquantLogo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/pngquantlogo.png")));
    pngquantLogo->setToolTip("pngquant");

    vendorRowTwo->addWidget(pngquantLogo);

    QLabel *libsquishLogo = new QLabel();
    libsquishLogo->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/squishlogo.png" ) ) );
    libsquishLogo->setToolTip("libsquish");

    vendorRowTwo->addWidget(libsquishLogo);

    if ( rw::IsNativeTexture( rwEngine, "PlayStation2" ) )
    {
        QLabel *ps2Logo = new QLabel();
        ps2Logo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/ps2logo.png")));
        ps2Logo->setToolTip("Playstation 2");

        vendorRowTwo->addWidget(ps2Logo);
    }

    QLabel *qtLogo = new QLabel();
    qtLogo->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/qtlogo.png" ) ) );
    qtLogo->setToolTip("Qt");

    vendorRowTwo->addWidget(qtLogo);

    rootLayout->addLayout( vendorRowTwo );

    rootLayout->addSpacing(10);

    this->setLayout( rootLayout );

    RegisterTextLocalizationItem( this );

    mainWnd->RegisterThemeItem( this );

    // There can be only one.
    mainWnd->aboutDlg = this;
}

AboutDialog::~AboutDialog( void )
{
    mainWnd->aboutDlg = NULL;

    mainWnd->UnregisterThemeItem( this );

    UnregisterTextLocalizationItem( this );
}

void AboutDialog::updateContent( MainWindow *mainWnd )
{
    setWindowTitle( MAGIC_TEXT( "Main.About.Desc" ) );
}

void AboutDialog::updateTheme( MainWindow *mainWnd )
{
    QMovie *stars = new QMovie();

    stars->setFileName(mainWnd->makeThemePath("stars2.gif"));
    mainLogoLabel->setMovie(stars);
    stars->start();
}
