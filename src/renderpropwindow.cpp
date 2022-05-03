/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/renderpropwindow.cpp
*  PURPOSE:     Window for texture render properties.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include "renderpropwindow.h"

#include "qtinteroputils.hxx"

#include "qtutils.h"
#include "languages.h"

struct addrToNatural
{
    rw::eRasterStageAddressMode mode;
    std::string natural;

    inline bool operator == ( const rw::eRasterStageAddressMode& right ) const
    {
        return ( right == this->mode );
    }

    inline bool operator == ( const QString& right ) const
    {
        return ( qstring_native_compare( right, this->natural.c_str() ) );
    }
};

typedef naturalModeList <addrToNatural> addrToNaturalList_t;

static const addrToNaturalList_t addrToNaturalList =
{
    { rw::RWTEXADDRESS_WRAP, "wrap" },
    { rw::RWTEXADDRESS_CLAMP, "clamp" },
    { rw::RWTEXADDRESS_MIRROR, "mirror" }
};

struct filterToNatural
{
    rw::eRasterStageFilterMode mode;
    std::string natural;
    bool isMipmap;

    inline bool operator == ( const rw::eRasterStageFilterMode& right ) const
    {
        return ( right == this->mode );
    }

    inline bool operator == ( const QString& right ) const
    {
        return ( qstring_native_compare( right, this->natural.c_str() ) );
    }
};

typedef naturalModeList <filterToNatural> filterToNaturalList_t;

static const filterToNaturalList_t filterToNaturalList =
{
    { rw::RWFILTER_POINT, "point", false },
    { rw::RWFILTER_LINEAR, "linear", false },
    { rw::RWFILTER_POINT_POINT, "point_mip_point", true },
    { rw::RWFILTER_POINT_LINEAR, "point_mip_linear", true },
    { rw::RWFILTER_LINEAR_POINT, "linear_mip_point", true },
    { rw::RWFILTER_LINEAR_LINEAR, "linear_mip_linear", true }
};

inline QComboBox* createAddressingBox( void )
{
    QComboBox *addrSelect = new QComboBox();

    for ( const addrToNatural& item : addrToNaturalList )
    {
        addrSelect->addItem( QString::fromStdString( item.natural ) );
    }

    addrSelect->setMinimumWidth( 200 );

    return addrSelect;
}

QComboBox* RenderPropWindow::createFilterBox( void ) const
{
    // We assume that the texture we are editing does not get magically modified while
    // this dialog is open.

    bool hasMipmaps = false;
    {
        if ( rw::Raster *texRaster = toBeEdited->GetRaster() )
        {
            hasMipmaps = ( texRaster->getMipmapCount() > 1 );
        }
    }

    QComboBox *filterSelect = new QComboBox();

    for ( const filterToNatural& item : filterToNaturalList )
    {
        bool isMipmapProp = item.isMipmap;

        if ( isMipmapProp == hasMipmaps )
        {
            filterSelect->addItem( QString::fromStdString( item.natural ) );
        }
    }

    return filterSelect;
}

RenderPropWindow::RenderPropWindow( MainWindow *mainWnd, rw::ObjectPtr <rw::TextureBase> toBeEdited ) : QDialog( mainWnd ), toBeEdited( std::move( toBeEdited ) )
{
    this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    this->mainWnd = mainWnd;

    this->setAttribute( Qt::WA_DeleteOnClose );
    this->setWindowModality( Qt::WindowModal );

    // Decide what status to give the dialog first.
    rw::eRasterStageFilterMode begFilterMode = rw::RWFILTER_POINT;
    rw::eRasterStageAddressMode begUAddrMode = rw::RWTEXADDRESS_WRAP;
    rw::eRasterStageAddressMode begVAddrMode = rw::RWTEXADDRESS_WRAP;

    begFilterMode = this->toBeEdited->GetFilterMode();
    begUAddrMode = this->toBeEdited->GetUAddressing();
    begVAddrMode = this->toBeEdited->GetVAddressing();

    // We want to put rows of combo boxes.
    // Best put them into a form layout.
    MagicLayout<QFormLayout> layout(this);

    QComboBox *filteringSelectBox = createFilterBox();
    {
        QString natural;

        bool gotNatural = filterToNaturalList.getNaturalFromMode( begFilterMode, natural );

        if ( gotNatural )
        {
            filteringSelectBox->setCurrentText( natural );
        }
    }

    this->filterComboBox = filteringSelectBox;

    connect( filteringSelectBox, (void (QComboBox::*)( const QString& ))&QComboBox::activated, this, &RenderPropWindow::OnAnyPropertyChange );

    layout.top->addRow( CreateLabelL( "Main.SetupRP.Filter" ), filteringSelectBox );

    QComboBox *uaddrSelectBox = createAddressingBox();
    {
        QString natural;

        bool gotNatural = addrToNaturalList.getNaturalFromMode( begUAddrMode, natural );

        if ( gotNatural )
        {
            uaddrSelectBox->setCurrentText( natural );
        }
    }

    this->uaddrComboBox = uaddrSelectBox;

    connect( uaddrSelectBox, (void (QComboBox::*)( const QString& ))&QComboBox::activated, this, &RenderPropWindow::OnAnyPropertyChange );

    layout.top->addRow( CreateLabelL( "Main.SetupRP.UAddr" ), uaddrSelectBox );

    QComboBox *vaddrSelectBox = createAddressingBox();
    {
        QString natural;

        bool gotNatural = addrToNaturalList.getNaturalFromMode( begVAddrMode, natural );

        if ( gotNatural )
        {
            vaddrSelectBox->setCurrentText( natural );
        }
    }

    this->vaddrComboBox = vaddrSelectBox;

    connect( vaddrSelectBox, (void (QComboBox::*)( const QString& ))&QComboBox::activated, this, &RenderPropWindow::OnAnyPropertyChange );

    layout.top->addRow( CreateLabelL( "Main.SetupRP.VAddr" ), vaddrSelectBox );

    // And now add the usual buttons.
    QPushButton *buttonSet = CreateButtonL( "Main.SetupRP.Set" );
    layout.bottom->addWidget( buttonSet );

    this->buttonSet = buttonSet;

    connect( buttonSet, &QPushButton::clicked, this, &RenderPropWindow::OnRequestSet );

    QPushButton *buttonCancel = CreateButtonL( "Main.SetupRP.Cancel" );
    layout.bottom->addWidget( buttonCancel );

    connect( buttonCancel, &QPushButton::clicked, this, &RenderPropWindow::OnRequestCancel );

    mainWnd->renderPropDlg = this;

    RegisterTextLocalizationItem( this );

    // Initialize the dialog.
    this->UpdateAccessibility();
}

RenderPropWindow::~RenderPropWindow( void )
{
    this->mainWnd->renderPropDlg = nullptr;

    UnregisterTextLocalizationItem( this );
}

void RenderPropWindow::updateContent( MainWindow *mainWnd )
{
    this->setWindowTitle( MAGIC_TEXT("Main.SetupRP.Desc") );
}

void RenderPropWindow::OnRequestSet( bool checked )
{
    MainWindow *mainWnd = this->mainWnd;

    // Update the texture.
    if ( TexInfoWidget *texInfo = mainWnd->currentSelectedTexture )
    {
        rw::ObjectPtr texHandle = (rw::TextureBase*)rw::AcquireObject( texInfo->GetTextureHandle() );

        if ( texHandle.is_good() )
        {
            QString selectedFilterMode = this->filterComboBox->currentText();
            QString selectedUAddrMode = this->uaddrComboBox->currentText();
            QString selectedVAddrMode = this->vaddrComboBox->currentText();

            mainWnd->actionSystem.LaunchPostOnlyActionL(
                eMagicTaskType::SET_RENDER_PROPS, "Main.SetupRP.SetProps",
                [mainWnd, texHandle = std::move(texHandle), selectedFilterMode = std::move(selectedFilterMode), selectedUAddrMode = std::move(selectedUAddrMode), selectedVAddrMode = std::move(selectedVAddrMode)] ( void ) mutable
            {
                // Just set the properties that we recognize.
                bool hasChanged = false;
                {
                    rw::eRasterStageFilterMode filterMode;

                    bool hasProp = filterToNaturalList.getModeFromNatural( selectedFilterMode, filterMode );

                    if ( hasProp )
                    {
                        texHandle->SetFilterMode( filterMode );

                        hasChanged = true;
                    }
                }

                {
                    rw::eRasterStageAddressMode uaddrMode;

                    bool hasProp = addrToNaturalList.getModeFromNatural( selectedUAddrMode, uaddrMode );

                    if ( hasProp )
                    {
                        texHandle->SetUAddressing( uaddrMode );

                        hasChanged = true;
                    }
                }

                {
                    rw::eRasterStageAddressMode vaddrMode;

                    bool hasProp = addrToNaturalList.getModeFromNatural( selectedVAddrMode, vaddrMode );

                    if ( hasProp )
                    {
                        texHandle->SetVAddressing( vaddrMode );

                        hasChanged = true;
                    }
                }

                if ( hasChanged )
                {
                    // Fix any broken filtering.
                    texHandle->fixFiltering();

                    // Well, we changed something.
                    mainWnd->NotifyChange();

                    // We can close.
                    if ( RenderPropWindow *renderPropDlg = mainWnd->renderPropDlg )
                    {
                        renderPropDlg->close();
                    }
                }
            });
        }
    }
}

void RenderPropWindow::OnRequestCancel( bool checked )
{
    this->close();
}

void RenderPropWindow::UpdateAccessibility( void )
{
    // Only allow setting if we actually change from the original values.
    bool allowSet = true;

    rw::eRasterStageFilterMode curFilterMode = toBeEdited->GetFilterMode();
    rw::eRasterStageAddressMode curUAddrMode = toBeEdited->GetUAddressing();
    rw::eRasterStageAddressMode curVAddrMode = toBeEdited->GetVAddressing();

    bool havePropsChanged = false;

    if ( !havePropsChanged )
    {
        rw::eRasterStageFilterMode selFilterMode;

        QString stringFilterMode = this->filterComboBox->currentText();

        bool hasProp = filterToNaturalList.getModeFromNatural( stringFilterMode, selFilterMode );

        if ( hasProp && curFilterMode != selFilterMode )
        {
            havePropsChanged = true;
        }
    }

    if ( !havePropsChanged )
    {
        rw::eRasterStageAddressMode selUAddrMode;

        QString stringUAddrMode = this->uaddrComboBox->currentText();

        bool hasProp = addrToNaturalList.getModeFromNatural( stringUAddrMode, selUAddrMode );

        if ( hasProp && curUAddrMode != selUAddrMode )
        {
            havePropsChanged = true;
        }
    }

    if ( !havePropsChanged )
    {
        rw::eRasterStageAddressMode selVAddrMode;

        QString stringVAddrMode = this->vaddrComboBox->currentText();

        bool hasProp = addrToNaturalList.getModeFromNatural( stringVAddrMode, selVAddrMode );

        if ( hasProp && curVAddrMode != selVAddrMode )
        {
            havePropsChanged = true;
        }
    }

    if ( !havePropsChanged )
    {
        allowSet = false;
    }

    this->buttonSet->setDisabled( !allowSet );
}
