/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/texnamewindow.cpp
*  PURPOSE:     Renaming support of textures.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include "texnamewindow.h"

#include "texnameutils.hxx"

TexNameWindow::TexNameWindow( MainWindow *mainWnd, rw::ObjectPtr <rw::TextureBase> toBeRenamed ) : QDialog( mainWnd ), toBeRenamed( std::move( toBeRenamed ) )
{
    this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );
    this->mainWnd = mainWnd;
    this->setWindowModality( Qt::WindowModal );
    this->setAttribute( Qt::WA_DeleteOnClose );

    QString curTexName = ansi_to_qt( this->toBeRenamed->GetName() );

    // Fill things.
    MagicLayout<QHBoxLayout> layout(this);
    layout.top->addWidget(CreateLabelL( "Main.Rename.Name" ));

    texture_name_validator *texNameValidator = new texture_name_validator( this );

    this->texNameEdit = new MagicLineEdit(curTexName);
    this->texNameEdit->setValidator( texNameValidator );
    layout.top->addWidget(this->texNameEdit);
    this->texNameEdit->setMaxLength(32);
    this->texNameEdit->setMinimumWidth(350);

    connect(this->texNameEdit, &QLineEdit::textChanged, this, &TexNameWindow::OnUpdateTexName);

    this->buttonSet = CreateButtonL("Main.Rename.Set");
    QPushButton *buttonCancel = CreateButtonL("Main.Rename.Cancel");

    connect(this->buttonSet, &QPushButton::clicked, this, &TexNameWindow::OnRequestSet);
    connect(buttonCancel, &QPushButton::clicked, this, &TexNameWindow::OnRequestCancel);

    layout.bottom->addWidget(this->buttonSet);
    layout.bottom->addWidget(buttonCancel);
    this->mainWnd->texNameDlg = this;
    // Initialize the window.
    this->UpdateAccessibility();

    // Make sure text inside edit box is selected, if present (Xinerki).
    this->texNameEdit->selectAll();

    RegisterTextLocalizationItem( this );
}

TexNameWindow::~TexNameWindow( void )
{
    UnregisterTextLocalizationItem( this );

    // There can be only one texture name dialog.
    this->mainWnd->texNameDlg = nullptr;
}

void TexNameWindow::updateContent( MainWindow *mainWnd )
{
    this->setWindowTitle( MAGIC_TEXT( "Main.Rename.Desc" ) );
}

void TexNameWindow::OnRequestSet( bool checked )
{
    // Attempt to change the name.
    QString texName = this->texNameEdit->text();

    if ( texName.isEmpty() )
        return;

    // TODO: verify if all ANSI.

    rw::rwStaticString <char> ansiTexName = qt_to_ansirw( texName );

    MainWindow *mainWnd = this->mainWnd;

    rw::ObjectPtr texHandle = this->toBeRenamed;

    mainWnd->actionSystem.LaunchPostOnlyActionL(
        eMagicTaskType::RENAME_TEXTURE, "General.TexRename",
        [mainWnd, ansiTexName = std::move(ansiTexName), texHandle = std::move(texHandle)] ( void ) mutable
    {
        // Set it.
        texHandle->SetName( ansiTexName.GetConstString() );

        // We have changed the TXD.
        mainWnd->NotifyChange();

        if ( TexInfoWidget *texInfo = mainWnd->getTextureTexInfoLink( texHandle ) )
        {
            // Update the info item.
            texInfo->updateInfo();

            // Update texture list width
            texInfo->listItem->setSizeHint(QSize(mainWnd->textureListWidget->sizeHintForColumn(0), 54));
        }
    });

    this->close();
}

void TexNameWindow::UpdateAccessibility( void )
{
    // Only allow to push "Set" if we actually have a valid texture name.
    QString curTexName = texNameEdit->text();

    bool shouldAllowSet = ( curTexName.isEmpty() == false );

    if ( shouldAllowSet )
    {
        // Setting an already set texture name makes no sense.
        auto ansiCurTexName = qt_to_ansirw( curTexName );

        if ( ansiCurTexName == this->toBeRenamed->GetName() )
        {
            shouldAllowSet = false;
        }
    }

    // TODO: validate the texture name aswell.

    this->buttonSet->setDisabled( !shouldAllowSet );
}
